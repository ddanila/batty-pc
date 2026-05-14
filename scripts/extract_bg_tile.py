#!/usr/bin/env python3
"""Extract the 16x16 hex-pattern bg tile from a GT level capture.

The pattern is colour-invariant (1bpp bitmap; per-level paper/ink
applies the level palette at render time). H=V=16-px period — verified
in the L1 capture by comparing adjacent tile candidates.

Output: 32 raw bytes (16 rows x 2 bytes = 16 px x 16 px).
"""
import sys
from pathlib import Path

SRC = Path('build/level_gt/level_01.scr')
# Pure-bg region: lower playfield, y >= 128, x in [64, 80).
ORIGIN_Y = 128
ORIGIN_BYTE_X = 8     # = pixel x 64; 2 bytes wide.


def zx_byte_off(py, byte_x):
    return ((py & 0xC0) * 32) + ((py & 7) * 256) + ((py & 0x38) * 4) + byte_x


def main():
    out_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('assets/bg_tile.bin')
    scr = SRC.read_bytes()
    buf = bytearray(32)
    for r in range(16):
        buf[r*2 + 0] = scr[zx_byte_off(ORIGIN_Y + r, ORIGIN_BYTE_X + 0)]
        buf[r*2 + 1] = scr[zx_byte_off(ORIGIN_Y + r, ORIGIN_BYTE_X + 1)]
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(bytes(buf))
    print(f'wrote {out_path} ({len(buf)} B)')


if __name__ == '__main__':
    main()
