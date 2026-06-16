"""Resolve real Steam cover art for an appid, through a layered fallback.

Why this exists: Steam art is discrete pre-rendered files (header.jpg,
library_600x900.jpg, capsule_*.jpg), and Valve has *migrated* them to a new CDN
path that embeds a content hash:

    https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/{appid}/{hash}/header.jpg?t=...

The old flat path (cdn.cloudflare.steamstatic.com/steam/apps/{appid}/header.jpg)
still serves *older* games but 404s for newer apps/demos — which is why some tiles
were blank. The hash isn't guessable; the authoritative source for the current URL
is Steam's store `appdetails` API. So we resolve in this order:

    1. disk cache (data/covers/{appid}.jpg)         — instant, offline
    2. Steam's local librarycache (installed games) — offline, no hash needed
    3. legacy flat CDN                              — fast for older games
    4. store appdetails API -> the live hashed URL  — recovers newer apps/demos

Any local/network hit is cached to disk, so step 4 (rate-limited) runs at most once
per game. Returns raw image bytes; the GUI crops/letterboxes to the portrait card.

Pure standard library (urllib + json) — no Pillow here, so it stays importable and
unit-testable anywhere.
"""

from __future__ import annotations

import json
import urllib.request
from pathlib import Path

from . import steam_paths

# Downloaded covers live here (gitignored). Shared with the GUI.
COVER_DIR = Path(__file__).resolve().parent.parent / "data" / "covers"

_TIMEOUT = 10
_UA = "Mozilla/5.0"

# Legacy flat CDN — still serves older games. Tried in order (portrait first).
_LEGACY_HOST = "https://cdn.cloudflare.steamstatic.com/steam/apps/{appid}/{asset}"
_LEGACY_ASSETS = ("library_600x900.jpg", "header.jpg", "capsule_616x353.jpg")

# Local librarycache filenames Steam writes, in preference order (portrait first).
_LOCAL_PREF = ("library_600x900.jpg", "header.jpg", "library_hero.jpg", "logo.png")


# ---------------------------------------------------------------------------
# Disk cache
# ---------------------------------------------------------------------------

def _disk_path(appid: int) -> Path:
    return COVER_DIR / f"{appid}.jpg"


def _read_disk(appid: int) -> bytes | None:
    p = _disk_path(appid)
    if not p.exists():
        return None
    try:
        data = p.read_bytes()
        return data or None
    except OSError:
        return None


def _save_disk(appid: int, data: bytes) -> None:
    try:
        COVER_DIR.mkdir(parents=True, exist_ok=True)
        _disk_path(appid).write_bytes(data)
    except OSError:
        pass


# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------

def _read_file(path: Path) -> bytes | None:
    try:
        data = path.read_bytes()
        return data or None
    except OSError:
        return None


def _http_get(url: str) -> bytes | None:
    try:
        req = urllib.request.Request(url, headers={"User-Agent": _UA})
        with urllib.request.urlopen(req, timeout=_TIMEOUT) as r:
            if getattr(r, "status", 200) != 200:
                return None
            data = r.read()
            return data or None
    except Exception:
        return None


def _local_steam(appid: int, root: Path | None = None) -> bytes | None:
    """Whatever art Steam already cached for this installed app. Handles the newer
    per-app folder (librarycache/<appid>/<asset>) and the older flat layout
    (librarycache/<appid>_<asset>)."""
    root = root or steam_paths.steam_root()
    if not root:
        return None
    cache = root / "appcache" / "librarycache"

    appdir = cache / str(appid)
    if appdir.is_dir():
        for name in _LOCAL_PREF:                       # preferred exact names
            f = appdir / name
            if f.exists():
                data = _read_file(f)
                if data:
                    return data
        for f in sorted(appdir.glob("*.jpg")) + sorted(appdir.glob("*.png")):
            data = _read_file(f)                       # else any image in the folder
            if data:
                return data

    for name in _LOCAL_PREF:                           # old flat layout
        f = cache / f"{appid}_{name}"
        if f.exists():
            data = _read_file(f)
            if data:
                return data
    return None


def _legacy_cdn(appid: int) -> bytes | None:
    for asset in _LEGACY_ASSETS:
        data = _http_get(_LEGACY_HOST.format(appid=appid, asset=asset))
        if data:
            return data
    return None


def _appdetails(appid: int) -> bytes | None:
    """Ask Steam's store API for the live (hashed) art URL and fetch it. This is
    what recovers newer apps/demos whose art moved off the legacy flat path."""
    raw = _http_get(
        f"https://store.steampowered.com/api/appdetails?appids={appid}&l=en"
    )
    if not raw:
        return None
    try:
        entry = json.loads(raw).get(str(appid), {})
        if not entry.get("success"):
            return None
        data = entry["data"]
    except (ValueError, KeyError, AttributeError):
        return None
    for field in ("header_image", "capsule_image", "capsule_imagev5"):
        url = data.get(field)
        if url:
            img = _http_get(url)
            if img:
                return img
    return None


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def cover_bytes(appid: int, *, allow_network: bool = True) -> bytes | None:
    """Raw image bytes of real Steam art for `appid`, or None if nothing exists.

    Layered: disk cache -> Steam local librarycache -> legacy CDN -> appdetails API.
    Any local/network hit is saved to the disk cache. Set allow_network=False to
    use only on-disk sources (offline / tests).
    """
    cached = _read_disk(appid)
    if cached:
        return cached

    local = _local_steam(appid)
    if local:
        _save_disk(appid, local)
        return local

    if not allow_network:
        return None

    for source in (_legacy_cdn, _appdetails):
        data = source(appid)
        if data:
            _save_disk(appid, data)
            return data
    return None
