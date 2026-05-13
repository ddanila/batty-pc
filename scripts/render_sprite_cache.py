#!/usr/bin/env python3
"""Render the 0xE400..0xF1FF sprite cache from a snapshot at multiple widths.

These bytes don't exist in the on-tape blob — some init routine builds
them at startup. The fact that runtime VRAM bytes mostly come from this
range identifies it as the real pre-shifted sprite cache.

Renders at byte-widths 1..8 (= 8..64 px wide) so we can eyeball the
sprite layout.
"""
import sys
from pathlib import Path
from PIL import Image

RAM_BASE  = 0x4000
CACHE_LO  = 0xE400
CACHE_HI  = 0xF200    # exclusive
ZOOM      = 3


def render(data: bytes, width_bytes: int) -> Image.Image:
    w_px = width_bytes * 8
    rows = (len(data) + width_bytes - 1) // width_bytes
    img = Image.new("L", (w_px, rows), 0)
    px = img.load()
    for i, b in enumerate(data):
        r = i // width_bytes
        c0 = (i % width_bytes) * 8
        for bit in range(8):
            if (b >> (7 - bit)) & 1:
                px[c0 + bit, r] = 255
    return img


def main():
    snap_path = Path(sys.argv[1])
    out_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else Path('assets/sprite_cache')
    out_dir.mkdir(parents=True, exist_ok=True)
    ram = snap_path.read_bytes()
    cache = ram[CACHE_LO - RAM_BASE : CACHE_HI - RAM_BASE]
    print(f"cache {CACHE_LO:#06x}..{CACHE_HI:#06x}  ({len(cache)} B)")
    for w in range(1, 9):
        img = render(cache, w)
        scaled = img.resize((img.width * ZOOM, img.height * ZOOM), Image.NEAREST)
        scaled.save(out_dir / f"cache_w{w}.png")
        print(f"  -> cache_w{w}.png   {img.width}px wide x {img.height} rows")


if __name__ == "__main__":
    main()
