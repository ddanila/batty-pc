#!/usr/bin/env python3
"""Find the in-game font table by matching displayed glyphs back to RAM.

Method:
  1. From snap1's VRAM, extract the 8-byte glyph at each (char_row, char_col).
     Each glyph = the 8 pixel-row bytes at the interleaved screen address
     for that cell, ordered top-to-bottom.
  2. Search the whole RAM (excluding VRAM itself) and the on-tape blob
     for that 8-byte pattern.
  3. When several consecutive cells map to consecutive 8-byte chunks in
     RAM/blob, that span is the font table.
"""
import sys
from pathlib import Path

RAM_BASE = 0x4000
SCREEN_END = 0x5800        # pixel area only; skip attrs
VRAM_LEN = 0x1B00          # 6912


def cell_glyph(vram: bytes, char_row: int, char_col: int) -> bytes:
    """8 bytes top-to-bottom for an 8x8 cell at (char_row, char_col)."""
    out = bytearray(8)
    for r in range(8):
        y = char_row * 8 + r
        third             = (y >> 6) & 3
        char_row_in_third = (y >> 3) & 7
        pixel_row_in_char = y & 7
        addr = (third << 11) | (pixel_row_in_char << 8) | (char_row_in_third << 5) | char_col
        out[r] = vram[addr]
    return bytes(out)


def find_all(hay: bytes, needle: bytes, skip_range=None):
    hits, off = [], 0
    while True:
        i = hay.find(needle, off)
        if i < 0: break
        if not skip_range or not (skip_range[0] <= i < skip_range[1]):
            hits.append(i)
        off = i + 1
    return hits


def main():
    if len(sys.argv) != 3:
        print("usage: find_font.py <snap_ram> <tape_blob>", file=sys.stderr)
        sys.exit(2)
    ram  = Path(sys.argv[1]).read_bytes()
    blob = Path(sys.argv[2]).read_bytes()
    vram = ram[:SCREEN_END - 0x4000]   # 6144 bytes pixel area

    # Probe the title row (char_row = 1, cols 0..31 to be safe).
    # Also probe a few entry rows for digits.
    rows_to_probe = [1, 4, 6, 8, 10, 12]
    print("--- title + entry-row glyph candidates (col, hex bytes, hits) ---")
    blob_hits_by_col = {}
    ram_hits_by_col  = {}
    for char_row in rows_to_probe:
        print(f"\nchar_row {char_row}  (pixel y={char_row*8}..{char_row*8+7})")
        for col in range(32):
            glyph = cell_glyph(vram, char_row, col)
            if glyph == b"\x00" * 8:
                continue   # blank cell
            hits_blob = find_all(blob, glyph)
            hits_ram  = find_all(ram,  glyph, skip_range=(0, VRAM_LEN))
            if hits_blob or hits_ram:
                print(f"  col {col:2d}  {glyph.hex()}  blob={[hex(0x6800+h) for h in hits_blob[:3]]}  ram={[hex(RAM_BASE+h) for h in hits_ram[:3]]}")
                blob_hits_by_col.setdefault(char_row, {})[col] = hits_blob
                ram_hits_by_col.setdefault(char_row, {})[col]  = hits_ram

    # Look for adjacent cells whose hits in blob/ram are 8 bytes apart
    # — that's the font table.
    print("\n--- consecutive-cell stride analysis (looking for stride=8) ---")
    for char_row, cells in blob_hits_by_col.items():
        cols = sorted(cells.keys())
        for i in range(len(cols) - 1):
            c1, c2 = cols[i], cols[i + 1]
            if c2 != c1 + 1: continue
            for h1 in cells[c1]:
                for h2 in cells[c2]:
                    if h2 - h1 == 8:
                        print(f"  row={char_row} col {c1}->{c2}: blob 0x{0x6800+h1:04X} -> 0x{0x6800+h2:04X}  (stride=8)")
    for char_row, cells in ram_hits_by_col.items():
        cols = sorted(cells.keys())
        for i in range(len(cols) - 1):
            c1, c2 = cols[i], cols[i + 1]
            if c2 != c1 + 1: continue
            for h1 in cells[c1]:
                for h2 in cells[c2]:
                    if h2 - h1 == 8:
                        print(f"  row={char_row} col {c1}->{c2}: ram  0x{RAM_BASE+h1:04X} -> 0x{RAM_BASE+h2:04X}  (stride=8)")


if __name__ == "__main__":
    main()
