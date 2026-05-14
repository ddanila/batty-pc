#!/usr/bin/env python3
"""Extract the 16x16 hex-pattern bg tile, one per color cycle.

The bitmap pattern is NOT level-invariant - tile bits differ between
the 4 colour cycles (yellow=L1/5/9/13, green=L2/6/10/14, cyan=L3/7/
11/15, white=L4/8/12). Within each cycle the tile is byte-identical.

Output blob layout (4 * 32 = 128 B): tile[cycle] for cycle 0..3.
Lookup at render time: cycle = (level_idx - 1) mod 4, tile starts
at offset cycle * 32.
"""
import sys
from pathlib import Path

# Pure-bg region: y=160..175 byte_x 8..9. y=128 had snap3-state
# debris contaminating tile row 0 on the cyan / white cycle GTs
# (verified empirically: y=128 row 0 = `00 00` for L4 but the real
# tile row 0 is `EF EF` per y=160). The y=160..175 window is clear.
ORIGIN_Y = 160
ORIGIN_BYTE_X = 8

# One representative level per cycle. Tiles are byte-identical within
# a cycle (verified) so any level in the cycle works.
CYCLES = [
    ('yellow', 'build/level_gt/level_01.scr'),
    ('green',  'build/level_gt/level_02.scr'),
    ('cyan',   'build/level_gt/level_03.scr'),
    ('white',  'build/level_gt/level_04.scr'),
]


def zx_byte_off(py, byte_x):
    return ((py & 0xC0) * 32) + ((py & 7) * 256) + ((py & 0x38) * 4) + byte_x


def main():
    out_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('assets/bg_tile.bin')
    blob = bytearray()
    for name, path in CYCLES:
        scr = Path(path).read_bytes()
        for r in range(16):
            blob.append(scr[zx_byte_off(ORIGIN_Y + r, ORIGIN_BYTE_X + 0)])
            blob.append(scr[zx_byte_off(ORIGIN_Y + r, ORIGIN_BYTE_X + 1)])
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(bytes(blob))
    print(f'wrote {out_path} ({len(blob)} B = 4 cycles x 32 B)')


if __name__ == '__main__':
    main()
