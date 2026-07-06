#!/usr/bin/env python3
"""Generate the ORBIT app icon.

Draws the orbit mark — dark disc, amber orbit ring, orbiting dot — with PIL at
8x supersampling, renders each size independently (crisp small sizes instead of
downscaled 256px art), and packs a PNG-entry .ico.

Outputs (committed; the Windows build box doesn't need PIL):
    src/ui/icons/orbit-{16,24,32,48,64,128,256}.png
    src/ui/icons/orbit.ico

Colors come from the CINEMA design (design/m1base.css): bg #0a0c11, amber
accent #f59e0b.

Usage: python3 tools/make_orbit_icon.py   (from src/; PIL required)
"""

import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw

SIZES = [16, 24, 32, 48, 64, 128, 256]
SS = 8  # supersampling factor

BG_TOP = (26, 30, 41)      # disc gradient top (lifted #0a0c11)
BG_BOT = (10, 12, 17)      # disc gradient bottom = design --bg
RING = (245, 158, 11)      # --acc1 amber
DOT_CORE = (255, 224, 138) # hot center of the orbiting dot
EDGE = (255, 255, 255, 24) # subtle disc rim light


def draw_mark(size: int) -> Image.Image:
    s = size * SS
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = s / 2

    # Dark disc with a vertical gradient, clipped to a circle.
    grad = Image.new("RGBA", (s, s))
    gd = ImageDraw.Draw(grad)
    for y in range(s):
        t = y / (s - 1)
        gd.line([(0, y), (s, y)], fill=tuple(
            round(a + (b - a) * t) for a, b in zip(BG_TOP, BG_BOT)) + (255,))
    mask = Image.new("L", (s, s), 0)
    ImageDraw.Draw(mask).ellipse([0, 0, s - 1, s - 1], fill=255)
    img.paste(grad, (0, 0), mask)

    # Subtle rim light so the disc reads on dark taskbars.
    rim_w = max(1, round(s * 0.015))
    d.ellipse([rim_w / 2, rim_w / 2, s - 1 - rim_w / 2, s - 1 - rim_w / 2],
              outline=EDGE, width=rim_w)

    # Amber orbit ring. Proportions tuned to stay readable at 16px.
    ring_r = s * 0.315
    ring_w = max(SS, round(s * 0.075))
    d.ellipse([c - ring_r, c - ring_r, c + ring_r, c + ring_r],
              outline=RING + (255,), width=ring_w)

    # Orbiting dot at upper-right (45°), sitting on the ring with a dark gap
    # punched around it so it reads as a separate body.
    ang = math.radians(-45)
    dx, dy = c + ring_r * math.cos(ang), c + ring_r * math.sin(ang)
    dot_r = s * 0.105
    gap_r = dot_r * 1.55
    # gap: repaint the disc gradient over the ring around the dot
    gap = Image.new("L", (s, s), 0)
    ImageDraw.Draw(gap).ellipse([dx - gap_r, dy - gap_r, dx + gap_r, dy + gap_r], fill=255)
    gap = Image.composite(mask, Image.new("L", (s, s), 0), gap)  # keep inside disc
    img.paste(grad, (0, 0), gap)
    # dot: amber with a hot core
    d.ellipse([dx - dot_r, dy - dot_r, dx + dot_r, dy + dot_r], fill=RING + (255,))
    core_r = dot_r * 0.55
    cx, cy = dx - dot_r * 0.25, dy - dot_r * 0.25
    d.ellipse([cx - core_r, cy - core_r, cx + core_r, cy + core_r], fill=DOT_CORE + (255,))

    return img.resize((size, size), Image.LANCZOS)


def pack_ico(pngs: dict[int, bytes], out: Path) -> None:
    """PNG-entry ICO: ICONDIR + ICONDIRENTRYs + raw PNG blobs."""
    sizes = sorted(pngs)
    header = struct.pack("<HHH", 0, 1, len(sizes))
    entries, blobs = b"", b""
    offset = len(header) + 16 * len(sizes)
    for sz in sizes:
        data = pngs[sz]
        entries += struct.pack("<BBBBHHII", sz % 256, sz % 256, 0, 0, 1, 32,
                               len(data), offset)
        blobs += data
        offset += len(data)
    out.write_bytes(header + entries + blobs)


def main() -> None:
    out_dir = Path(__file__).resolve().parent.parent / "ui" / "icons"
    out_dir.mkdir(parents=True, exist_ok=True)
    pngs = {}
    for size in SIZES:
        img = draw_mark(size)
        path = out_dir / f"orbit-{size}.png"
        img.save(path)
        import io
        buf = io.BytesIO()
        img.save(buf, "PNG")
        pngs[size] = buf.getvalue()
        print(f"wrote {path}")
    ico = out_dir / "orbit.ico"
    pack_ico(pngs, ico)
    print(f"wrote {ico} ({ico.stat().st_size} bytes, {len(SIZES)} PNG entries)")


if __name__ == "__main__":
    main()
