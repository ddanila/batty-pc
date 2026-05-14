#!/usr/bin/env python3
"""Extract the L1 perimeter frame: top strip + left strip + right
strip (NO bottom).

Left and right strips are 3 cols wide each, not 2 - the third column
(col 2 on the left, col 29 on the right) is the SHADOW band the
original game paints just inside the cyan frame edge. Including it
in the frame asset captures that detail for free.

Layout:
    [0..511]    top pixels    (32 cols x 16 rows)
    [512..575]  top attrs     (32 cols x  2 char-rows)
    left pixels  ( 3 cols x 176 rows = 528 B)
    left attrs   ( 3 cols x  22 char-rows = 66 B)
    right pixels ( 3 cols x 176 rows = 528 B)
    right attrs  ( 3 cols x  22 char-rows = 66 B)
Total: 1764 bytes.
"""
import sys
from pathlib import Path

SRC = Path('build/level_gt/level_01.scr')
ATTR_BASE = 6144

TOP_ROWS_PX   = 16
SIDE_ROWS_PX  = 176    # playfield y=16..191
SIDE_BYTES_W  = 3      # cols 0..2 on left, 29..31 on right


def zx_byte_off(py, byte_x):
    return ((py & 0xC0) * 32) + ((py & 7) * 256) + ((py & 0x38) * 4) + byte_x


def main():
    out_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('assets/frame_l1.bin')
    scr = SRC.read_bytes()
    buf = bytearray()

    # Top strip: y=0..15, all 32 cols
    for py in range(0, TOP_ROWS_PX):
        for bx in range(32):
            buf.append(scr[zx_byte_off(py, bx)])
    # Top attrs (char-rows 0..1)
    buf += scr[ATTR_BASE : ATTR_BASE + 2 * 32]

    # Left strip: y=16..191, byte_x=0..(SIDE_BYTES_W-1)
    for py in range(16, 16 + SIDE_ROWS_PX):
        for bx in range(0, SIDE_BYTES_W):
            buf.append(scr[zx_byte_off(py, bx)])
    # Left attrs (char-rows 2..23)
    for cr in range(2, 24):
        buf += scr[ATTR_BASE + cr * 32 : ATTR_BASE + cr * 32 + SIDE_BYTES_W]

    # Right strip: y=16..191, byte_x=(32-SIDE_BYTES_W)..31
    right_start = 32 - SIDE_BYTES_W
    for py in range(16, 16 + SIDE_ROWS_PX):
        for bx in range(right_start, 32):
            buf.append(scr[zx_byte_off(py, bx)])
    # Right attrs (char-rows 2..23)
    for cr in range(2, 24):
        buf += scr[ATTR_BASE + cr * 32 + right_start : ATTR_BASE + cr * 32 + 32]

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(bytes(buf))
    print(f'wrote {out_path} ({len(buf)} B)')


if __name__ == '__main__':
    main()
