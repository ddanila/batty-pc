#!/usr/bin/env python3
"""Extract perimeter frames, one per colour cycle (4 variants).

The frame includes the cyan ornament + per-level hex bg pixels
between ornament pixels. Those hex bg pixel bits vary per cycle
(yellow / green / cyan / white) because each cycle has a slightly
different hex-tile bit pattern. Shipping a single L1-derived frame
embedded L1's bg bits into all 15 levels; we now ship four frames
keyed by (level_idx & 3).

Per-cycle layout (1764 B each):
    [0..511]    top pixels    (32 cols x 16 rows)
    [512..575]  top attrs     (32 cols x  2 char-rows)
    [576..1103] left pixels   ( 3 cols x 176 rows = 528 B)
    [1104..1169] left attrs   ( 3 cols x  22 char-rows = 66 B)
    [1170..1697] right pixels ( 3 cols x 176 rows = 528 B)
    [1698..1763] right attrs  ( 3 cols x  22 char-rows = 66 B)
Total blob: 4 cycles x 1764 = 7056 B.
"""
import sys
from pathlib import Path

ATTR_BASE = 6144
TOP_ROWS_PX  = 24       # HUD: y=0..7 frame ornament + y=8..15 labels +
                        # y=16..23 score digits.
SIDE_ROWS_PX = 168      # y=24..191 (side frame below the HUD)
SIDE_BYTES_W = 1   # Only the actual ornament column; see main.c FRAME_SIDE_W.
# One representative level per cycle (yellow / green / cyan / white).
CYCLES = ['build/level_gt/level_01.scr',
          'build/level_gt/level_02.scr',
          'build/level_gt/level_03.scr',
          'build/level_gt/level_04.scr']


def zx_byte_off(py, byte_x):
    return ((py & 0xC0) * 32) + ((py & 7) * 256) + ((py & 0x38) * 4) + byte_x


def extract_one(scr):
    buf = bytearray()
    # Top strip: y=0..(TOP_ROWS_PX-1), all 32 cols
    for py in range(0, TOP_ROWS_PX):
        for bx in range(32):
            buf.append(scr[zx_byte_off(py, bx)])
    # Top attrs: char-rows 0..(TOP_ROWS_PX/8 - 1)
    top_char_rows = TOP_ROWS_PX // 8
    for cr in range(top_char_rows):
        buf += scr[ATTR_BASE + cr * 32 : ATTR_BASE + cr * 32 + 32]
    # Left strip: y=TOP_ROWS_PX..(TOP_ROWS_PX + SIDE_ROWS_PX - 1)
    for py in range(TOP_ROWS_PX, TOP_ROWS_PX + SIDE_ROWS_PX):
        for bx in range(0, SIDE_BYTES_W):
            buf.append(scr[zx_byte_off(py, bx)])
    side_char_rows = SIDE_ROWS_PX // 8
    for cr in range(top_char_rows, top_char_rows + side_char_rows):
        buf += scr[ATTR_BASE + cr * 32 : ATTR_BASE + cr * 32 + SIDE_BYTES_W]
    # Right strip
    right_start = 32 - SIDE_BYTES_W
    for py in range(TOP_ROWS_PX, TOP_ROWS_PX + SIDE_ROWS_PX):
        for bx in range(right_start, 32):
            buf.append(scr[zx_byte_off(py, bx)])
    for cr in range(top_char_rows, top_char_rows + side_char_rows):
        buf += scr[ATTR_BASE + cr * 32 + right_start : ATTR_BASE + cr * 32 + 32]
    return bytes(buf)


def main():
    out_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('assets/frame_l1.bin')
    blob = bytearray()
    for src in CYCLES:
        scr = Path(src).read_bytes()
        blob.extend(extract_one(scr))
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(bytes(blob))
    print(f'wrote {out_path} ({len(blob)} B = 4 cycles x {len(blob)//4} B)')


if __name__ == '__main__':
    main()
