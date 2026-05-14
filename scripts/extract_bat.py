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

SRC = Path('build/level_gt/level_01.scr')
BAT_Y0 = 167
BAT_Y1 = 183           # exclusive (16 rows)
BAT_BYTE_X0 = 14       # = pixel x 112
BAT_BYTE_X1 = 18       # exclusive (4 bytes = 32 px)


def zx_byte_off(py, byte_x):
    return ((py & 0xC0) * 32) + ((py & 7) * 256) + ((py & 0x38) * 4) + byte_x


def main():
    out_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('assets/bat_l1.bin')
    scr = SRC.read_bytes()
    h = BAT_Y1 - BAT_Y0
    w = BAT_BYTE_X1 - BAT_BYTE_X0
    buf = bytearray(w * h)
    for r in range(h):
        for c in range(w):
            buf[r * w + c] = scr[zx_byte_off(BAT_Y0 + r, BAT_BYTE_X0 + c)]
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(bytes(buf))
    print(f'wrote {out_path} ({len(buf)} B = {w*8} x {h} px)')


if __name__ == '__main__':
    main()
