#!/usr/bin/env python3
"""Extract the top-strip HUD (1UP / HI / 2UP labels + scores) from
L1's GT capture as a paint-it-verbatim asset.

Format:
    [512 B] pixel data: 32 cols x 16 rows, row-major top-to-bottom
                        (i.e. row r pixel bytes at offset r*32..r*32+32)
    [ 64 B] attr data : 32 cols x 2 char-rows (rows 0..1 of the attr area)
Total: 576 bytes.

The strip covers playfield y=0..15 across the full 256-px width, so
it carries the side-edge cyan stripes (cols 0, 1, 30, 31) for free.
For a static L1 render the scores read "000000" / "100000" / "000000"
which is what the player sees at the start; per-frame score updates
come with the gameplay port.
"""
import sys
from pathlib import Path

SRC = Path('build/level_gt/level_01.scr')
ROWS_PX  = 16     # playfield y=0..15
COLS     = 32
ATTR_BASE = 6144


def zx_byte_off(py, byte_x):
    return ((py & 0xC0) * 32) + ((py & 7) * 256) + ((py & 0x38) * 4) + byte_x


def main():
    out_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('assets/hud_l1.bin')
    scr = SRC.read_bytes()
    pixels = bytearray(ROWS_PX * COLS)
    for r in range(ROWS_PX):
        for c in range(COLS):
            pixels[r * COLS + c] = scr[zx_byte_off(r, c)]
    # Char rows 0..1 of the attr area.
    attrs = scr[ATTR_BASE : ATTR_BASE + 2 * COLS]
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(bytes(pixels) + bytes(attrs))
    print(f'wrote {out_path} ({len(pixels) + len(attrs)} B = '
          f'{len(pixels)} px + {len(attrs)} attr)')


if __name__ == '__main__':
    main()
