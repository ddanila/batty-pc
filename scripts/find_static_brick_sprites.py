#!/usr/bin/env python3
"""For each cell type 0x11..0x15 (the static-brick range that sub_adbch
skips), extract the 16x8 sprite bitmap from L1's GT capture and compare
against the naive cache[type * 16] guess.

Approach: a cell of type V at level-grid (r, c) occupies playfield
pixels (8 + c*16, 16 + r*8) and is painted with attr-paper as the
"off" pixels and attr-ink as the "on" pixels. We decode bits by
comparing each pixel to the cell's paper colour: if it's the paper,
bit=0; if it's the ink, bit=1.

Output: per-type bitmap dump + a side-by-side PNG comparing all
instances of that type in L1 vs the cache[type*16] guess.
"""
from pathlib import Path
from PIL import Image
import sys

sys.path.insert(0, str(Path(__file__).parent))
from extract_scr import decode

BLOB_BASE = 0x6800
TABLE     = 0x6CBD
RAM_BASE  = 0x4000
CACHE_LO  = 0xE400
ROWS, COLS  = 12, 15
BRICK_W, BRICK_H = 16, 8
BRICK_X0, BRICK_Y0 = 8, 16
ATTR_BAND_BASE  = 6144 + 2 * 32   # char rows 2..13 of L1.scr's attrs


def main():
    blob   = Path('original/blocks/03_DATA_headless.dat.bin').read_bytes()
    ram    = Path('build/snapshots/20260513T202101Z/ram_4000_FFFF.bin').read_bytes()
    cache  = ram[CACHE_LO - RAM_BASE : CACHE_LO - RAM_BASE + 0xE00]
    scr1   = Path('build/level_gt/level_01.scr').read_bytes()
    idx1   = decode(scr1)

    # Level 1 cells.
    p0 = blob[TABLE - BLOB_BASE] | (blob[TABLE - BLOB_BASE + 1] << 8)
    cells = blob[p0 - BLOB_BASE : p0 - BLOB_BASE + ROWS * COLS]

    # Group all cells in L1 by value.
    by_val = {}
    for r in range(ROWS):
        for c in range(COLS):
            v = cells[r * COLS + c]
            by_val.setdefault(v, []).append((r, c))

    # ZX palette indices for the decoded SCR (extract_scr maps to ZX 16-pal).
    # We can probe paper/ink for each cell from the .scr attr band.
    def attr_at(r, c):
        # Brick at grid (r, c) -> char row 2+r, char col 1+c*2 (left half).
        return scr1[6144 + (2 + r) * 32 + (1 + c * 2)]
    def attr_to_idx(attr, bit):
        # bit=1 -> ink, bit=0 -> paper
        ink_lo  = attr & 7
        paper_lo = (attr >> 3) & 7
        bright  = (attr & 0x40) >> 3
        return (ink_lo | bright) if bit else (paper_lo | bright)

    out_dir = Path('build/static_brick_diag')
    out_dir.mkdir(parents=True, exist_ok=True)

    for v in [0x11, 0x12, 0x13, 0x14, 0x15, 0x07]:
        positions = by_val.get(v, [])
        if not positions:
            print(f'  type 0x{v:02X}: not present in L1')
            continue

        # Decode each cell of type v into a 16x8 1bpp bitmap by comparing
        # to its paper colour.
        bitmaps = []
        for r, c in positions:
            attr  = attr_at(r, c)
            paper = attr_to_idx(attr, 0)
            ink   = attr_to_idx(attr, 1)
            bm = []
            for dy in range(BRICK_H):
                rowbits = 0
                for dx in range(BRICK_W):
                    px = idx1[(BRICK_Y0 + r * BRICK_H + dy) * 256 +
                              (BRICK_X0 + c * BRICK_W + dx)]
                    rowbits = (rowbits << 1) | (1 if px == ink else 0)
                bm.append(rowbits)
            bitmaps.append((r, c, attr, bm))

        # Are all bitmaps identical?
        ref = bitmaps[0][3]
        all_same = all(bm == ref for _, _, _, bm in bitmaps)
        flag7 = bool(v & 0x80)
        flag4 = bool(v & 0x10)
        print(f'\ntype 0x{v:02X} (skip7={flag7}, skip4={flag4}): '
              f'{len(positions)} cells in L1, all same bitmap: {all_same}')
        # Print the reference bitmap.
        for row in ref:
            print(f'  {row:016b}')

        # Compare against cache[v*16..v*16+16] interpreted as the same
        # 16-px-wide x 8-row sprite.
        cache_bm = []
        for dy in range(BRICK_H):
            hi = cache[v * 16 + dy * 2]
            lo = cache[v * 16 + dy * 2 + 1]
            cache_bm.append((hi << 8) | lo)
        print(f'  cache[0x{v*16:03X}..] bitmap (for comparison):')
        for row in cache_bm:
            print(f'    {row:016b}')

        # Search the entire cache for a chunk that matches the reference bitmap.
        def chunk_to_bits(off):
            return [(cache[off + 2*r] << 8) | cache[off + 2*r + 1] for r in range(8)]
        found = []
        for off in range(0, len(cache) - 15, 1):
            if chunk_to_bits(off) == ref:
                found.append(off)
        print(f'  cache offsets matching this bitmap (byte-aligned search): {[hex(x) for x in found[:8]]}')


if __name__ == '__main__':
    main()
