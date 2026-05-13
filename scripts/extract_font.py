#!/usr/bin/env python3
"""Extract the in-game font from the tape blob.

Font layout (deduced via VRAM matching — see notes/encoding.md):
  - base address:  0x6A15  (blob offset 0x021D = 533)
  - glyph size:    6 bytes (= 8 px wide × 6 px tall)
  - indexing:      glyph[i] starts at base + i × 6, indexed by markup char-code:
                     0..9    → digits 0..9
                     0x0A..0x23  → letters A..Z

Emits:
  assets/font.bin           raw 216 B (36 × 6) for use by the C renderer
  assets/font_sheet.png     PNG mosaic — visual sanity check
"""
import sys
from pathlib import Path
from PIL import Image

ORIGIN     = 0x6800
FONT_BASE  = 0x6A15
GLYPH_W    = 8       # bits per row → pixel width
GLYPH_H    = 6       # 6 rows per glyph
N_GLYPHS   = 43      # 10 digits + 26 letters + 7 special: . , <sp> - _ II =
ZOOM       = 4
COLS       = 12


def render_sheet(font: bytes) -> Image.Image:
    rows = (N_GLYPHS + COLS - 1) // COLS
    gap = 2
    cell_w, cell_h = GLYPH_W + gap, GLYPH_H + gap
    img = Image.new("L", (cell_w * COLS, cell_h * rows), 64)
    px = img.load()
    for i in range(N_GLYPHS):
        cr, cc = divmod(i, COLS)
        base = i * GLYPH_H
        gx = cc * cell_w
        gy = cr * cell_h
        for r in range(GLYPH_H):
            byte = font[base + r]
            for b in range(8):
                if (byte >> (7 - b)) & 1:
                    px[gx + b, gy + r] = 255
    return img.resize((img.width * ZOOM, img.height * ZOOM), Image.NEAREST)


def main():
    blob = Path('original/blocks/03_DATA_headless.dat.bin').read_bytes()
    o = FONT_BASE - ORIGIN
    font = blob[o : o + N_GLYPHS * GLYPH_H]
    out_bin = Path('assets/font.bin')
    out_bin.write_bytes(font)
    print(f"wrote {out_bin}  ({len(font)} B = {N_GLYPHS} × {GLYPH_H})")

    img = render_sheet(font)
    out_png = Path('assets/font_sheet.png')
    img.save(out_png)
    print(f"wrote {out_png}  ({COLS} cols × {(N_GLYPHS + COLS - 1) // COLS} rows, {ZOOM}× zoom)")


if __name__ == "__main__":
    main()
