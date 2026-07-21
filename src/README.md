# ORBIT — C++/Qt rewrite (`src/`)

Native C++ multi-store launcher, officially named **ORBIT** (formerly SteamSwitch;
the CMake target / exe is `Orbit`, the QML module URI is `Orbit`, and the core
keeps its `ss::` namespaces). Replaced the Python `core/` + Tauri shell +
`server.py` sidecar + `web/` UI with a single Qt 6 application; the legacy stack
was deleted after v1.0.0 (browse it at the `v1.0.0` tag). The Steam logic is
a **behaviour-faithful port** of that Python `core/` (same constraints — see the
repo `CLAUDE.md`); the other stores (Epic/GOG/Xbox) are added on top.

## Layout

| Path | Role |
|------|------|
| `core/` | Pure C++ engine, **no Qt** — headless + unit-testable. VDF/JSON parsers, SHA-256 (`sha256.*`), OS platform layer, Steam paths/games/accounts/switcher/launcher/covers, Epic (`epic_games.*`), GOG registry (`gog_games.*`) and Xbox/Game Pass (`xbox_games.*`) enumeration, the `IStore` interface + `SteamStore`/`EpicStore`/`GogStore`/`XboxStore`. |
| `core/platform_{win,posix}.cpp` | OS specifics: registry, process control, `EnumWindows`, `ShellExecute`/`xdg-open`. Windows is the real one; POSIX mirrors the Python stubs. |
| `core/http.{h,cpp}` | Injectable HTTP — the host installs a fetcher so `core/` stays Qt-free (the Qt UI installs a `QNetwork` one; tests inject a stub). |
| `ui/` | Qt 6 + QML. `Backend` (the in-process replacement for `server.py`+`bridge.js`), `GameModel`, `QtFetcher`, and `qml/` views. `ui/icons/` holds the generated app icon (PNGs + `orbit.ico`, embedded via `ui/orbit.rc`); regenerate with `tools/make_orbit_icon.py` (PIL). |
| `tools/ssdiag.cpp` | Headless diagnostic CLI (`ssdiag accounts|games|epic|gog|xbox|covers|storecovers`) — was the Phase 1 parity gate against the Python app. |
| `tests/` | Header-only test harness + cases against a synthetic Steam tree (no real Steam). |

## Build

### Full app (Windows — the priority platform)

One-time toolchain: **CMake ≥ 3.21**, a C++17 compiler (MSVC), and **Qt 6.5+**
(Core/Gui/Quick/Network). Install Qt via the Qt online installer or
[`aqtinstall`](https://github.com/miurahr/aqtinstall); point CMake at it with
`-DCMAKE_PREFIX_PATH=<qt>/<ver>/msvc2022_64`.

```
cmake -S src -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.7.2/msvc2022_64
cmake --build build --config Release
```

If Qt 6 isn't found, CMake still builds **`sscore` + `ssdiag` + `sstests`** (no GUI)
— useful for CI / headless verification.

### Release zip

After building + `windeployqt --release --qmldir src\ui\qml build\Release\Orbit.exe`:

```
powershell -ExecutionPolicy Bypass -File src\tools\make-release-zip.ps1
```

Produces `dist\Orbit-portable.zip` with only what a release needs — it strips
`data\` (settings, launch history, cover cache), `startup.log` and the
ssdiag/sstests dev binaries, so personal data can't end up in a published zip.
Never zip `build\Release` by hand.

### Headless core only (Linux/macOS, no Qt)

```
cmake -S src -B build && cmake --build build
ctest --test-dir build              # runs sstests
./build/ssdiag accounts             # diagnostic
```

Or straight with the compiler (what the dev sandbox uses):

```
g++ -std=c++17 -Isrc src/tests/*.cpp \
    src/core/*.cpp src/core/stores/*.cpp -o /tmp/sstests && /tmp/sstests
```

## Verifying the Steam port (parity gate — historical)

The port was validated by diffing `ssdiag accounts` against the Python app's
`python -m core.accounts` on the same `$STEAM_ROOT` — byte-identical on a synthetic
tree AND on the user's real PC ("FC: no differences"), which is when the C++ core
was trusted over the Python one. The Python app has since been deleted; to re-run
the comparison, check out the `v1.0.0` tag, which contains both stacks.

## Status

- **Done + tested headless:** VDF, JSON, Steam paths/games/accounts (with the
  family-share playtime tiebreak), switcher (loginusers/config writes, offline
  flag, backups, `can_autologin`), launcher (online + offline flag method), cover
  resolver, `IStore`/`SteamStore`, `ssdiag`. 29 core tests pass; ssdiag output
  matches the Python diagnostic.
- **Epic store — done + tested headless:** `core/epic_games.*` enumerates installed
  games from the launcher's `*.item` manifests
  (`%PROGRAMDATA%\Epic\EpicGamesLauncher\Data\Manifests`, override with
  `$SS_EPIC_MANIFESTS`), filters DLC/addons, and `stores/epic_store.cpp` launches via
  `com.epicgames.launcher://apps/<AppName>?action=launch&silent=true` (no account
  switching). `Backend` aggregates it alongside Steam; non-Steam games get a stable
  synthetic id so the appid-keyed UI can address them. Verify on real hardware with
  `ssdiag epic`.
- **GOG store — done + tested headless:** `core/gog_games.*` enumerates installed
  games from the Windows registry (`HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\<gameID>`,
  32-bit fallback under `SOFTWARE\GOG.com\Games`) — no Galaxy SQLite dependency.
  `stores/gog_store.cpp` launches via GOG Galaxy's
  `GalaxyClient.exe /command=runGame /gameId=<id> /path=<dir>` when Galaxy is
  installed, else runs the DRM-free exe directly in its install dir. Needed two new
  platform primitives: `regSubKeys()` (registry enumeration) and a
  working-directory `spawnDetached()` overload. Registry-only ⇒ empty off-Windows;
  the pure Game/argv builders are unit-tested. Verify with `ssdiag gog`.
- **Xbox / Game Pass store — done + tested headless:** `core/xbox_games.*`
  enumerates installed titles by scanning the Xbox install roots
  (`<drive>:\XboxGames`, override `$SS_XBOX_ROOTS`) for each game's
  `Content\MicrosoftGame.config`, then computes the launch **AUMID**
  (`PackageFamilyName!AppId`) — the PackageFamilyName's publisher hash comes from
  `core/sha256.*` + a base32, so no WinRT/PackageManager dependency. Launch is
  `explorer shell:AppsFolder\<AUMID>` (UWP titles are license-gated, so — unlike GOG
  — the exe can't be run directly). The publisher-hash pipeline is unit-tested
  against Microsoft's well-known `8wekyb3d8bbwe`. Verify with `ssdiag xbox` and
  cross-check AUMIDs against PowerShell `get-StartApps`.
- **Multi-store UI redesign — "ORBIT" (imported from the Claude Design project):**
  the Qt/QML shell was rebuilt around a store-agnostic, dynamic-accent glass design.
  Frameless window with a custom title bar; a glass sidebar (Library / Accounts /
  Settings + a live STORES panel); a **Library** screen (search, refresh, offline
  toggle, store + account filter chips, cover grid); a **Detail** screen (big cover,
  Play now/offline, owning account + truthful stat cards — no fabricated
  ratings/descriptions); an **Accounts** screen of per-store cards; **Settings**
  (offline-default + language/RTL); and a slide-in **Manage** panel (per-store sync
  status + Steam multi-account list + add-account). Each game gets a stable derived
  accent (from its id) driving the ambient backdrop + placeholder gradient; real
  cover art layers on top. Typography is **Space Grotesk (display) + Manrope (body)**,
  instanced to static TTFs and bundled (IBM Plex Sans Arabic covers Arabic). Backend gained a
  `stores` model + a store filter on the proxy. Also a **first-run onboarding
  overlay** (`OnboardingOverlay.qml`: Welcome → Connect → Done) shown once, tracked
  by a new `onboarded` flag in `settings.json` (`core/settings.*` +
  `Backend.completeOnboarding()`); re-openable from Accounts → "Re-run setup". Its
  "Connect" step is adapted to this app's reality — stores are auto-detected (no
  OAuth), so it shows real per-store detection status and a Steam "Add account"
  action + Rescan, not the mock's simulated connect toggles. All QML passes `qmllint`
  (Qt 6.11) clean; not yet run on a display here (no Qt/GUI in the sandbox) — verify
  the frameless window (translucency + `startSystemMove`/`startSystemResize`) and the
  `Canvas`-drawn icons on the real Windows build.
- **All four stores done + the multi-store UI is in**, and the app was validated on
  the user's real Windows PC (all stores detected, per-store covers, launch, Arabic).
- **ORBIT rename + app icon (CINEMA redesign Phase 0):** app renamed to **ORBIT**
  (target `Orbit`, QML URI `Orbit`, org/app name), with a real icon — the orbit mark
  (dark disc, amber ring + orbiting dot) generated by `tools/make_orbit_icon.py` into
  `ui/icons/` and wired three ways: `ui/orbit.rc` (Explorer/taskbar), `setWindowIcon`
  (Alt-Tab/titlebar), and a `/icons` Qt resource used by the QML brand marks. The
  full CINEMA redesign plan is at `design/ORBIT-REDESIGN-PLAN.md`.
- **CINEMA redesign implemented (Phases 1–4, 2026-07-02):** per
  `design/ORBIT-REDESIGN-PLAN.md` + the `design/m1-*.html` previews.
  Core (headless, 59 tests): per-account bulk usage map, ORBIT launch history in
  settings.json, hero/wide art resolvers for all four stores, run-at-startup
  (HKCU Run + `platform::regDeleteValue`), cover-cache size/clear, persisted
  heroMode/offlineDefault, `steam::switchTo` (switch without launching).
  Backend: playtime/lastPlayed roles, `requestHero`/`heroReady`, QML-instantiable
  `GameFilter` (per-shelf live filters + `gameAt`), switchTo/setRunAtStartup/
  setHeroMode/setOfflineDefault/coverCacheSize/clearCoverCache/lastScanTime,
  launch-history recording on successful play. QML: top-bar shell (tabs + Ctrl-K
  search + status pill + offline pill + avatar), library hero + shelves, detail
  as hero takeover with pin-to-account, Ctrl-K palette (↵ play / Shift+↵ offline
  / Tab details), CINEMA accounts + settings, restyled onboarding. Arabic
  catalog regenerated: 140 strings, all translated, `ar.qm` prebuilt.
- **Windows verification checklist (CINEMA):** icon in taskbar/Alt-Tab/Explorer;
  hero art per store (Steam `library_hero`, Xbox TitledHeroArt, Epic
  DieselGameBox, GOG background); Continue-playing shelf ordering (Steam
  playtime + ORBIT history after launching a non-Steam game); Ctrl-K palette
  keys; Accounts "Switch now" (switches + restarts Steam, no launch);
  Settings run-at-startup writes/removes `HKCU\...\CurrentVersion\Run\ORBIT`;
  cover-cache size/clear; Arabic/RTL across the new screens; frameless
  drag/resize still works with the new top bar.
- **Shipped:** v1.0.0 released as `Orbit-portable.zip` (windeployqt); the legacy
  Python/Tauri stack is deleted from the tree (last present at the `v1.0.0` tag).
- **Custom games — done + tested headless (post-v1.0.0):** a fifth store,
  `Store::Custom`, for anything the four real stores don't detect. Unlike them it's
  **user-authored**, not enumerated: `core/custom_games.*` persists a hand-maintained
  list to **`custom_games.json`** (sibling of settings.json) and `core/stores/
  custom_store.cpp` wraps it as an `IStore` that launches the game's exe directly (GOG's
  direct-launch pattern; `platform::spawnDetached(argv, workingDir)`). Works on every
  platform (no registry/store), so the builders + CRUD are unit-tested (`test_custom.cpp`,
  64 tests total). Backend gained `addCustomGame`/`updateCustomGame`/`removeCustomGame`/
  `customGame`/`urlToLocalFile` invokables + a `launchId` model role (so the detail view
  can address a Custom row). UI: an **"＋ Add game"** pill in the Library chip row and
  **Edit/Remove** on the Detail view (Custom only), both driving a shared
  **`AddGameDialog.qml`** (exe picker via `QtQuick.Dialogs` FileDialog, optional
  name/args/cover-image), routed through `AppState.openAddGame()/openEditGame()`. Custom
  games get the neutral slate brand (`Backend::storeBrand` + `Theme.storeMeta`, keep in
  sync) and a "Custom" filter chip when populated. **Cover art** resolves in `store_covers`
  from anchors the game itself carries — no API key — in this order (`coverHint` is
  `"<name>|<local image path>|<exe path>"`): **user-picked image → cached → `steam_appid.txt`
  breadcrumb in the install folder → real Steam art; `goggame-<id>.info` breadcrumb → real GOG
  art; name match on Steam's storesearch; the exe's own embedded icon** (`platform::exeIcon`,
  Windows-only, returned as a 32-bpp BMP so a tile is never a blank gradient). Only real hits
  are disk-cached (the icon isn't, so a network source can take over later). Reuses
  `covers::coverBytes`/`heroBytes` for the Steam path. A "Choose cover" pick (or Edit → Clear
  to drop back to auto) always wins. Excluded from the Accounts "auto-detected stores" list
  (it's manual). Verify with `ssdiag custom`. **HW-verify the exe-icon fallback** — it's
  untested Win32 (PrivateExtractIcons → GetDIBits → BMP; BMP alpha via BITMAPV4HEADER, degrades
  to black-on-dark if a reader drops alpha), and it links **gdi32** (added to `sscore`).
  **NOTE:** the
  new qsTr strings aren't in `ar.qm` yet — they show English under RTL until the catalog
  is regenerated (Qt LinguistTools; watch the pyside6-lupdate ar.ts corruption trap).
- **Update check — done + tested headless (post-v1.0.0):** notify-only (a portable app
  shouldn't swap a running exe): `core/updates.*` GETs `api.github.com/repos/tv7/Orbit/
  releases/latest` via the injected fetcher, parses `tag_name`/`html_url`/`body`, and
  `compareVersions` (dotted-numeric, tolerates a leading `v`, ignores pre-release suffixes)
  decides if it's newer than **`core/version.h` `ss::kVersion`** (the single source of truth).
  Backend exposes `appVersion` + `updateState`/`updateAvailable`/`latestVersion`/`updateUrl`/
  `updateNotes` + `checkForUpdates(manual)`/`openDownloadPage()`/`skipThisUpdate()`. Main.qml
  runs a quiet check on launch (honours a skipped version persisted in settings.json's
  `skip_update`) and shows a dismissible bottom banner (Download / Skip / ✕); SettingsView has
  an ABOUT card (current version + "Check for updates" → Download). No API key (60 req/hr
  unauth is plenty for once-per-launch). Tests in `test_updates.cpp` (68 total).
  **⚠ RELEASE WORKFLOW:** before building a release, **bump `ss::kVersion` in
  `core/version.h`** and publish the GitHub release with a **matching tag** (`v1.2.0` or
  `1.2.0`). The running build compares its own `kVersion` to the latest tag — if they don't
  advance together, users are never notified. This build is set to **1.2.0** (bundles custom
  games + the updater).
