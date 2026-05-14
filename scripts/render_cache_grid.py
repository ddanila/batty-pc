#!/usr/bin/env python3
"""Render the 0xE400..0xF1FF sprite cache as a grid of 16x8 px chunks.

The blitter sub_adbch consumes 16 bytes per chunk (2 bytes wide x 8
rows). 3584 B / 16 = 224 chunks. Arrange them N per row so we can
eyeball the cache structure (= which chunks are pre-shifted copies of
the same logical sprite, where the chunk groups start, etc.).
"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw

RAM_BASE  = 0x4000
CACHE_LO  = 0xE400
CACHE_HI  = 0xF200
CHUNK_W_PX = 16
CHUNK_H_PX = 8
CHUNK_BYTES = CHUNK_W_PX // 8 * CHUNK_H_PX   # 16
N_CHUNKS_PER_ROW = 16
GUTTER_PX = 2
ZOOM = 3


def main():
    snap_path = Path(sys.argv[1])
    out_path  = Path(sys.argv[2]) if len(sys.argv) > 2 else Path('assets/sprite_cache/grid.png')
    out_path.parent.mkdir(parents=True, exist_ok=True)
    ram = snap_path.read_bytes()
    cache = ram[CACHE_LO - RAM_BASE : CACHE_HI - RAM_BASE]
    n = len(cache) // CHUNK_BYTES
    rows = (n + N_CHUNKS_PER_ROW - 1) // N_CHUNKS_PER_ROW
    img_w = N_CHUNKS_PER_ROW * (CHUNK_W_PX + GUTTER_PX) + GUTTER_PX
    img_h = rows * (CHUNK_H_PX + GUTTER_PX) + GUTTER_PX
    img = Image.new('RGB', (img_w, img_h), (40, 40, 60))
    for idx in range(n):
        cx = idx % N_CHUNKS_PER_ROW
        cy = idx // N_CHUNKS_PER_ROW
        x0 = GUTTER_PX + cx * (CHUNK_W_PX + GUTTER_PX)
        y0 = GUTTER_PX + cy * (CHUNK_H_PX + GUTTER_PX)
        for r in range(CHUNK_H_PX):
            for c in range(2):
                b = cache[idx * CHUNK_BYTES + r * 2 + c]
                for bit in range(8):
                    if (b >> (7 - bit)) & 1:
                        img.putpixel((x0 + c * 8 + bit, y0 + r), (255, 255, 255))
    img = img.resize((img_w * ZOOM, img_h * ZOOM), Image.NEAREST)
    # Overlay chunk indices in tiny digits — skip, just save
    img.save(out_path)
    print(f'wrote {out_path}: {n} chunks @ 16x8 px, {N_CHUNKS_PER_ROW}/row, {rows} rows')


if __name__ == '__main__':
    main()
