#!/usr/bin/env python3
"""Extract the L1 perimeter frame: top strip + left strip + right
strip (NO bottom - confirmed against the GT capture, the bottom edge
of the playfield has only bat + score numbers, no frame ornament).

Layout of the output blob:
    [0..511]     top pixels    (32 cols x 16 rows)
    [512..575]   top attrs     (32 cols x  2 char-rows)
    [576..751]   left pixels   ( 2 cols x 176 rows; playfield y=16..191)
    [752..795]   left attrs    ( 2 cols x 22 char-rows)
    [796..971]   right pixels  ( 2 cols x 176 rows)
    [972..1015]  right attrs   ( 2 cols x 22 char-rows)
Total: 1016 bytes.

The three strips together form the visible cyan playfield frame
including the HUD at top. The brick area, bat, and bg-tile region
are *not* covered by this asset; they're painted by our renderers.
Frame shadow on the game field (a darker band just inside the frame
edge) is a follow-up.
"""
import sys
from pathlib import Path

SRC = Path('build/level_gt/level_01.scr')
ATTR_BASE = 6144

TOP_ROWS_PX   = 16
SIDE_ROWS_PX  = 176    # playfield y=16..191
SIDE_BYTES_W  = 2


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

    # Left strip: y=16..191, byte_x=0..1
    for py in range(16, 16 + SIDE_ROWS_PX):
        for bx in range(0, SIDE_BYTES_W):
            buf.append(scr[zx_byte_off(py, bx)])
    # Left attrs (char-rows 2..23, cols 0..1)
    for cr in range(2, 24):
        buf += scr[ATTR_BASE + cr * 32 : ATTR_BASE + cr * 32 + 2]

    # Right strip: y=16..191, byte_x=30..31
    for py in range(16, 16 + SIDE_ROWS_PX):
        for bx in range(30, 32):
            buf.append(scr[zx_byte_off(py, bx)])
    # Right attrs (char-rows 2..23, cols 30..31)
    for cr in range(2, 24):
        buf += scr[ATTR_BASE + cr * 32 + 30 : ATTR_BASE + cr * 32 + 32]

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(bytes(buf))
    print(f'wrote {out_path} ({len(buf)} B)')


if __name__ == '__main__':
    main()
