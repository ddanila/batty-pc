#!/usr/bin/env python3
"""Per-cell brick diff between our C render and the GT capture.

For each brick cell in level 1 (12 rows x 15 cols):
  - Extract the 16x8 pixel block from our state4_level1.ppm
  - Extract the same block from GT's level_01.scr
  - If they differ, log (row, col, cell_value, diff_px)
Aggregates by cell value so we can spot which cell values use a
different sprite than our naive `cache[cell*16]` lookup.
"""
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from extract_scr import decode
from test_visual import (PALETTE_RGB, ppm_inner_to_indices, PLAYFIELD_W)

BLOB_BASE = 0x6800
TABLE     = 0x6CBD
ROWS, COLS = 12, 15
BRICK_W_PX = 16
BRICK_H_PX = 8
BRICK_X0   = 8     # playfield-relative
BRICK_Y0   = 16


def main():
    blob = Path('original/blocks/03_DATA_headless.dat.bin').read_bytes()
    ours = ppm_inner_to_indices(Path('build/test_visual/state4_level1.ppm'))
    gt   = decode(Path('build/level_gt/level_01.scr').read_bytes())

    # Level 1 cells.
    p0   = blob[TABLE - BLOB_BASE] | (blob[TABLE - BLOB_BASE + 1] << 8)
    cells = blob[p0 - BLOB_BASE : p0 - BLOB_BASE + ROWS * COLS]

    bucket = defaultdict(lambda: {'cells': 0, 'diff_cells': 0, 'diff_px': 0,
                                  'total_px': 0})
    for r in range(ROWS):
        for c in range(COLS):
            v = cells[r * COLS + c]
            x0 = BRICK_X0 + c * BRICK_W_PX
            y0 = BRICK_Y0 + r * BRICK_H_PX
            diff = 0
            for dy in range(BRICK_H_PX):
                for dx in range(BRICK_W_PX):
                    a = ours[(y0 + dy) * PLAYFIELD_W + (x0 + dx)]
                    e = gt  [(y0 + dy) * PLAYFIELD_W + (x0 + dx)]
                    if PALETTE_RGB[a] != PALETTE_RGB[e]:
                        diff += 1
            bk = bucket[v]
            bk['cells']     += 1
            bk['total_px']  += BRICK_W_PX * BRICK_H_PX
            if diff > 0:
                bk['diff_cells'] += 1
                bk['diff_px']    += diff

    print(f'{"cell":>4} {"flag":>8} {"cells":>6} {"diff_cells":>11} {"diff_px":>9} {"of":>5} {"pct":>6}')
    for v in sorted(bucket):
        b = bucket[v]
        flag = []
        if v & 0x80: flag.append('skip7')
        if v & 0x10: flag.append('skip4')
        flag = ','.join(flag) or '-'
        pct = 100.0 * b['diff_px'] / b['total_px'] if b['total_px'] else 0
        print(f'  {v:02X} {flag:>8} {b["cells"]:6d} {b["diff_cells"]:11d} '
              f'{b["diff_px"]:9d} {b["total_px"]:5d} {pct:5.1f}%')


if __name__ == '__main__':
    main()
