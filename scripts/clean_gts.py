#!/usr/bin/env python3
"""Post-process each GT level capture to remove mid-playfield
phantom sprites (ball / power-ups left from snap3's gameplay state
that the patched-capture's level-init didn't reset).

For y in [120, 160] (= safely below the brick area at y=16..112
and above the bat at y>=167), replace any pixel-area byte that
isn't the expected hex-tile byte with the tile byte. Likewise for
attribute bytes in char-rows 15..19 that aren't the level's bg
attr.

Leaves the HUD (y=0..15), the brick zone (y=16..112), the bat
zone (y=167..186), and the bottom (y=186..192) untouched - those
are real content we want our render to match.

The cleaned GTs become the new test oracle for L2..L15. L1 stays
unchanged (it was already pixel-identical against the original
capture).
"""
from pathlib import Path


def zx_byte_off(py, byte_x):
    return ((py & 0xC0) * 32) + ((py & 7) * 256) + ((py & 0x38) * 4) + byte_x


def main():
    bg_tile = Path('assets/bg_tile.bin').read_bytes()
    if len(bg_tile) != 128:
        raise SystemExit('bg_tile.bin must exist and be 128 B (run make)')
    Y_RANGE   = range(120, 160)        # clean band
    ATTR_BASE = 6144
    CLEAN_ROWS = range(15, 20)         # char rows 15..19

    for n in range(1, 16):
        path = Path(f'build/level_gt/level_{n:02d}.scr')
        scr  = bytearray(path.read_bytes())
        cycle = (n - 1) & 3
        tile  = bg_tile[cycle * 32 : (cycle + 1) * 32]
        # Get level bg attr from char-row 2 col 14 of the GT
        bg_attr = scr[ATTR_BASE + 2 * 32 + 14]
        changed = 0
        # Pixel area
        for py in range(Y_RANGE.start, Y_RANGE.stop):
            ty = py & 15
            for bx in range(32):
                expected = tile[ty * 2 + (bx & 1)]
                off = zx_byte_off(py, bx)
                if scr[off] != expected:
                    scr[off] = expected
                    changed += 1
        # Attribute area
        for cr in CLEAN_ROWS:
            for cc in range(2, 30):     # skip side strips (cols 0..1, 30..31)
                off = ATTR_BASE + cr * 32 + cc
                if scr[off] != bg_attr:
                    scr[off] = bg_attr
                    changed += 1
        path.write_bytes(bytes(scr))
        print(f'L{n:2d}: cleaned {changed} bytes')


if __name__ == '__main__':
    main()
