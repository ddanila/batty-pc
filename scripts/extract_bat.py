#!/usr/bin/env python3
"""Extract the bat+ball sprite from L1's GT capture.

At level 1 start the ball sits on the bat - both render at the bottom
of the playfield. We grab a 4-byte-wide x 16-row composite (= 32 x 16 px)
spanning y=167..182, byte_x=14..17. The block is the "bat with ball"
state ready for launch. Output: 64 raw bytes, row-major top-to-bottom,
2 bytes per row.

This is L1-specific (= our snapshot of choice); other levels may
position the bat differently. Phase E will port the bat-motion logic
and replace this static asset.
"""
import sys
from pathlib import Path

BAT_Y0, BAT_Y1 = 167, 186
BAT_BYTE_X0, BAT_BYTE_X1 = 14, 19
CYCLES = ['build/level_gt/level_01.scr',
          'build/level_gt/level_02.scr',
          'build/level_gt/level_03.scr',
          'build/level_gt/level_04.scr']


def zx_byte_off(py, byte_x):
    return ((py & 0xC0) * 32) + ((py & 7) * 256) + ((py & 0x38) * 4) + byte_x


def extract_one(scr):
    h = BAT_Y1 - BAT_Y0
    w = BAT_BYTE_X1 - BAT_BYTE_X0
    buf = bytearray(w * h)
    for r in range(h):
        for c in range(w):
            buf[r * w + c] = scr[zx_byte_off(BAT_Y0 + r, BAT_BYTE_X0 + c)]
    return bytes(buf)


def main():
    out_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('assets/bat_l1.bin')
    blob = bytearray()
    for src in CYCLES:
        scr = Path(src).read_bytes()
        blob.extend(extract_one(scr))
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(bytes(blob))
    print(f'wrote {out_path} ({len(blob)} B = 4 cycles x {len(blob)//4} B)')


if __name__ == '__main__':
    main()
