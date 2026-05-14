#!/usr/bin/env python3
"""Extract the second life indicator at the bottom-left of L1's GT.

The first life indicator (left of the pair) falls inside the 3-col
left frame strip we already ship, so it renders for free. This asset
captures just the second indicator at cols 3..4 (= pixel x 24..39),
y=183..190. 2 bytes wide x 8 rows = 16 B.
"""
import sys
from pathlib import Path

Y0, Y1 = 183, 191
BYTE_X0, BYTE_X1 = 3, 5
CYCLES = ['build/level_gt/level_01.scr',
          'build/level_gt/level_02.scr',
          'build/level_gt/level_03.scr',
          'build/level_gt/level_04.scr']


def zx_byte_off(py, byte_x):
    return ((py & 0xC0) * 32) + ((py & 7) * 256) + ((py & 0x38) * 4) + byte_x


def extract_one(scr):
    h = Y1 - Y0
    w = BYTE_X1 - BYTE_X0
    buf = bytearray(w * h)
    for r in range(h):
        for c in range(w):
            buf[r * w + c] = scr[zx_byte_off(Y0 + r, BYTE_X0 + c)]
    return bytes(buf)


def main():
    out_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('assets/lives_l1.bin')
    blob = bytearray()
    for src in CYCLES:
        scr = Path(src).read_bytes()
        blob.extend(extract_one(scr))
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(bytes(blob))
    print(f'wrote {out_path} ({len(blob)} B = 4 cycles x {len(blob)//4} B)')


if __name__ == '__main__':
    main()
