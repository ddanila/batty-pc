#!/usr/bin/env python3
"""Extract per-cell 16x8 brick bitmaps from all 15 GT captures.

The original game's brick renderer is a neighbour-aware multi-pass
compositor (sub_b765h walks an IX list at 0xAF6F, painting all 180
cells per pass with a different 16-byte sprite, with two sentinel
passes that trigger sub_c101h's IX=0xC0B8 side-pipeline). Rather
than port the full pipeline now, we extract the final composited
bitmap per (level, row, col) directly from each level's GT .scr.

Output layout (43200 B):
    [level=0..14][row=0..11][col=0..14] -> 16 bytes (16px wide, 8 rows)
    offset = (level * 180 + row * 15 + col) * 16

For each cell, the bitmap is decoded by comparing each pixel to the
cell's attr-paper colour: pixel == paper -> bit 0, else -> bit 1.
That gives us the ink-pixel mask the original renderer ultimately
ended up writing to VRAM, which is exactly what we want our C
blitter to OR'd against ink/paper per the current attribute.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from extract_scr import decode

N_LEVELS = 15
ROWS, COLS = 12, 15
CELL_W_PX, CELL_H_PX = 16, 8
BYTES_PER_CELL = (CELL_W_PX // 8) * CELL_H_PX     # 2 * 8 = 16
BRICK_X0, BRICK_Y0 = 8, 24                        # playfield-relative
ATTR_BASE = 6144


def half_attr(scr_bytes, r, c, half):
    # Brick at grid (r, c) spans char-cols (1+c*2) and (1+c*2+1).
    # half=0 -> left, half=1 -> right.
    # With BRICK_Y0=24, brick row r occupies char-row 3+r.
    return scr_bytes[ATTR_BASE + (3 + r) * 32 + (1 + c * 2 + half)]


def paper_idx(attr):
    return ((attr >> 3) & 7) | ((attr & 0x40) >> 3)


def main():
    out_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('assets/brick_bitmaps.bin')
    out = bytearray()
    for n in range(1, N_LEVELS + 1):
        scr = Path(f'build/level_gt/level_{n:02d}.scr').read_bytes()
        idx = decode(scr)
        for r in range(ROWS):
            for c in range(COLS):
                # Each 8-pixel byte_col of the brick has its own attribute
                # (and thus its own paper colour). Decode each byte using
                # its own half-attr's paper so the resulting bitmap is the
                # pure "ink-or-not" mask the original would composite.
                for dy in range(CELL_H_PX):
                    for byte_col in range(CELL_W_PX // 8):
                        paper = paper_idx(half_attr(scr, r, c, byte_col))
                        b = 0
                        for bit in range(8):
                            px = idx[(BRICK_Y0 + r * CELL_H_PX + dy) * 256 +
                                     (BRICK_X0 + c * CELL_W_PX + byte_col * 8 + bit)]
                            if px != paper:
                                b |= (1 << (7 - bit))
                        out.append(b)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(bytes(out))
    print(f'wrote {out_path} ({len(out)} B = {N_LEVELS} x {ROWS*COLS} cells '
          f'x {BYTES_PER_CELL} B/cell)')


if __name__ == '__main__':
    main()
