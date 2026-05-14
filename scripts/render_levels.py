#!/usr/bin/env python3
"""Render each of Batty's 15 levels as a 240x96 PNG.

For every non-skip cell (bit 7 OR bit 4 clear), grab the 16-byte
sprite chunk at cache[ cell * 16 .. cell * 16 + 16 ] and blit it
to (col * 16, row * 8). Quick first-pass mapping — if visuals
match the original game, the mapping is right; otherwise we
refine.

Cache source: snap3 RAM 0xE400..0xF200.
"""
from pathlib import Path
from PIL import Image

BLOB_BASE = 0x6800
TABLE     = 0x6CBD
N_LEVELS  = 15
COLS, ROWS = 15, 12
CELL_W_PX, CELL_H_PX = 16, 8
RAM_BASE  = 0x4000
CACHE_LO  = 0xE400

ZOOM = 3

def render_level(cells: bytes, cache: bytes, palette_index_func) -> Image.Image:
    img = Image.new('RGB', (COLS * CELL_W_PX, ROWS * CELL_H_PX), (0, 0, 0))
    for r in range(ROWS):
        for c in range(COLS):
            v = cells[r * COLS + c]
            if (v & 0x90) != 0:
                continue
            chunk = cache[v * 16 : (v + 1) * 16]
            for row in range(CELL_H_PX):
                for byte_col in range(2):
                    b = chunk[row * 2 + byte_col]
                    for bit in range(8):
                        if (b >> (7 - bit)) & 1:
                            x = c * CELL_W_PX + byte_col * 8 + bit
                            y = r * CELL_H_PX + row
                            img.putpixel((x, y), (255, 255, 255))
    return img


def main():
    blob   = Path('original/blocks/03_DATA_headless.dat.bin').read_bytes()
    ram3   = Path('build/snapshots/20260513T202101Z/ram_4000_FFFF.bin').read_bytes()
    cache  = ram3[CACHE_LO - RAM_BASE : CACHE_LO - RAM_BASE + 0xE00]
    out    = Path('assets/levels')
    out.mkdir(parents=True, exist_ok=True)

    ptrs = []
    for i in range(N_LEVELS):
        o = TABLE - BLOB_BASE + i * 2
        ptrs.append(blob[o] | (blob[o+1] << 8))

    # Composite grid: 15 levels arranged 3 wide x 5 tall with labels
    grid_w = 3 * (COLS * CELL_W_PX + 4)
    grid_h = 5 * (ROWS * CELL_H_PX + 12)
    grid = Image.new('RGB', (grid_w, grid_h), (32, 32, 48))

    for i, p in enumerate(ptrs):
        cells = blob[p - BLOB_BASE : p - BLOB_BASE + COLS * ROWS]
        img = render_level(cells, cache, None)
        img.save(out / f'level{i+1:02d}.png')
        # Paste scaled-up into the grid
        gx = (i % 3) * (COLS * CELL_W_PX + 4) + 2
        gy = (i // 3) * (ROWS * CELL_H_PX + 12) + 10
        grid.paste(img, (gx, gy))

    grid_scaled = grid.resize((grid.width * ZOOM, grid.height * ZOOM), Image.NEAREST)
    grid_scaled.save(out / 'all_levels.png')
    print(f'wrote {N_LEVELS} per-level PNGs + assets/levels/all_levels.png')


if __name__ == '__main__':
    main()
