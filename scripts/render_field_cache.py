#!/usr/bin/env python3
"""Render 0xE400..0xF1FF as a 256x112 px ZX-interleaved screen region.

3584 bytes = 14 char rows × 32 bytes/row × 8 pixel rows = exactly two
thirds-worth of a ZX screen (one full 64-px third plus 48 px of the
second third). Stored in the same interleaved layout the ULA expects so
that LDIR straight into 0x4000 (or wherever) just works.
"""
import sys
from pathlib import Path
from PIL import Image

W_PX  = 256
H_PX  = 112      # 14 char rows × 8


def cache_addr(y: int, x_byte: int) -> int:
    third             = (y >> 6) & 3
    char_row_in_third = (y >> 3) & 7
    pixel_row_in_char = y & 7
    # Layout per third: 8 sub-thirds of 256 bytes each (one per pixel_row),
    # each sub-third has 8 × 32 = 256 bytes of char-row × x_byte.
    return (third << 11) | (pixel_row_in_char << 8) | (char_row_in_third << 5) | x_byte


def main():
    snap_path = Path(sys.argv[1])
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else Path('assets/field.png')
    ram = snap_path.read_bytes()
    cache = ram[0xE400 - 0x4000 : 0xF200 - 0x4000]
    img = Image.new("L", (W_PX, H_PX), 0)
    px = img.load()
    for y in range(H_PX):
        for xb in range(W_PX // 8):
            a = cache_addr(y, xb)
            if a >= len(cache): continue
            byte = cache[a]
            for b in range(8):
                if (byte >> (7 - b)) & 1:
                    px[xb * 8 + b, y] = 255
    img.resize((W_PX * 2, H_PX * 2), Image.NEAREST).save(out)
    print(f"wrote {out}  ({W_PX}x{H_PX}, 2× zoom)")


if __name__ == "__main__":
    main()
