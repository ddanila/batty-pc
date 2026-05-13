#!/usr/bin/env python3
"""Convert a 6912-byte ZX Spectrum .SCR to a flat 256x192 8bpp bitmap.

The ZX screen layout interleaves pixel rows in a way that's a pain for
random access on a Z80 but trivial to undo here. Each output byte is
a palette index in 0..15 (bright<<3 | colour); the matching palette
lives in src/main.c.

Flash bit is ignored — we render the steady frame.
"""
import sys
from pathlib import Path

W, H = 256, 192
PIXEL_BYTES = (W // 8) * H        # 6144
ATTR_BYTES  = (W // 8) * (H // 8) # 768
SCR_BYTES   = PIXEL_BYTES + ATTR_BYTES  # 6912


def pixel_addr(y, x_byte):
    # bit layout: (third << 11) | (pixel_row << 8) | (char_row << 5) | x_byte
    third     = (y >> 6) & 0x03
    pixel_row =  y       & 0x07
    char_row  = (y >> 3) & 0x07
    return (third << 11) | (pixel_row << 8) | (char_row << 5) | x_byte


def decode(scr: bytes) -> bytes:
    if len(scr) != SCR_BYTES:
        raise SystemExit(f"expected {SCR_BYTES} bytes, got {len(scr)}")
    pixels = scr[:PIXEL_BYTES]
    attrs  = scr[PIXEL_BYTES:]
    out = bytearray(W * H)
    for y in range(H):
        attr_row = (y >> 3) * (W // 8)
        for xb in range(W // 8):
            byte = pixels[pixel_addr(y, xb)]
            attr = attrs[attr_row + xb]
            ink    = attr        & 0x07
            paper  = (attr >> 3) & 0x07
            bright = (attr >> 6) & 0x01
            ink_idx   = (bright << 3) | ink
            paper_idx = (bright << 3) | paper
            row_off = y * W + xb * 8
            for bit in range(8):
                set_ = (byte >> (7 - bit)) & 1
                out[row_off + bit] = ink_idx if set_ else paper_idx
    return bytes(out)


def main():
    if len(sys.argv) != 3:
        print("usage: extract_scr.py <in.scr> <out.bin>", file=sys.stderr)
        sys.exit(2)
    src = Path(sys.argv[1]).read_bytes()
    out = decode(src)
    Path(sys.argv[2]).write_bytes(out)
    print(f"wrote {sys.argv[2]} ({len(out)} bytes)")


if __name__ == "__main__":
    main()
