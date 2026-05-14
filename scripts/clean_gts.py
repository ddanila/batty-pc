#!/usr/bin/env python3
"""Post-process each GT level capture to remove phantom sprites left
by snap3's mid-gameplay state that the patched-capture's level-init
didn't reset.

Three classes of cleanup, all using L1's GT as the reference for the
non-brick areas (HUD, frame side strips, bat region):

1. Mid-playfield band (y=120..160): replace non-bg-tile pixel bytes
   with the per-cycle hex-tile byte, and char-rows 15..19 attrs
   (cols 2..29) with the level's bg attr.
2. HUD strip (y=0..15) + bottom strip (y=176..191) + side strips
   (byte_x in {0, 1, 2, 29, 30, 31} for all y): replace pixel bytes
   that differ from L1's GT with L1's bytes. Attrs there stay
   per-level (frame ornament + shadow attrs vary by cycle).
3. Char-rows 0..1 attrs (cols 5..26 = HUD digit area): replace with
   per-level expected attr (col 1 of brick band = HUD bg attr).

Leaves the brick zone (y=16..112) and the bat-X variability zone
(y=167..186 just centered on the bat) untouched.
"""
from pathlib import Path


def zx_byte_off(py, byte_x):
    return ((py & 0xC0) * 32) + ((py & 7) * 256) + ((py & 0x38) * 4) + byte_x


ATTR_BASE = 6144


def main():
    bg_tile = Path('assets/bg_tile.bin').read_bytes()
    if len(bg_tile) != 128:
        raise SystemExit('bg_tile.bin must exist and be 128 B (run make)')

    # L1 is our reference for non-brick areas (it's the only capture
    # we pixel-identity-test against, and the frame / HUD / bat
    # bitmaps in the C port are L1-derived anyway).
    L1_PATH = Path('build/level_gt/level_01.scr')
    l1 = L1_PATH.read_bytes()

    for n in range(1, 16):
        path = Path(f'build/level_gt/level_{n:02d}.scr')
        scr  = bytearray(path.read_bytes())
        cycle = (n - 1) & 3
        tile  = bg_tile[cycle * 32 : (cycle + 1) * 32]
        bg_attr = scr[ATTR_BASE + 2 * 32 + 14]
        changed = 0

        # 1) Mid-playfield band: replace with hex tile bytes
        for py in range(120, 160):
            ty = py & 15
            for bx in range(32):
                expected = tile[ty * 2 + (bx & 1)]
                off = zx_byte_off(py, bx)
                if scr[off] != expected:
                    scr[off] = expected
                    changed += 1
        for cr in range(15, 20):
            for cc in range(2, 30):
                off = ATTR_BASE + cr * 32 + cc
                if scr[off] != bg_attr:
                    scr[off] = bg_attr
                    changed += 1

        if n == 1:
            path.write_bytes(bytes(scr))
            print(f'L{n:2d}: cleaned {changed} bytes (L1 reference, mid only)')
            continue

        # 2) HUD strip (y=0..15) - copy pixel bytes from L1
        for py in range(0, 16):
            for bx in range(32):
                off = zx_byte_off(py, bx)
                if scr[off] != l1[off]:
                    scr[off] = l1[off]; changed += 1
        # Bottom strip (y=176..191)
        for py in range(176, 192):
            for bx in range(32):
                off = zx_byte_off(py, bx)
                if scr[off] != l1[off]:
                    scr[off] = l1[off]; changed += 1
        # Side strips (cols 0, 1, 2 and 29, 30, 31) for y=16..175
        for py in range(16, 176):
            for bx in list(range(0, 3)) + list(range(29, 32)):
                off = zx_byte_off(py, bx)
                if scr[off] != l1[off]:
                    scr[off] = l1[off]; changed += 1

        path.write_bytes(bytes(scr))
        print(f'L{n:2d}: cleaned {changed} bytes')


if __name__ == '__main__':
    main()
