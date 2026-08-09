#!/usr/bin/env python3
"""Extract the 16x16 hex-pattern bg tile, one per colour cycle.

FROM THE TAPE, since 2026-08-09. This used to read the pattern back out
of an emulator screen capture (`build/level_gt/level_01.scr`), with a
comment about choosing a y-window free of "snap3-state debris". It never
needed to: the four textures are tape data, at

    spr_level_texture_1   $C015
    spr_level_texture_2   $8EE8
    spr_level_texture_3   $8F10
    spr_level_texture_4   $8F38

each a standard sprite block — `(w=2, h=$10)` header, 32 pattern bytes,
then `(2, 2)` and four attribute bytes. `game_screen_draw_to_buffer`
picks one with `LD A,(current_level_number_1up) / AND $03` and tiles the
screen with it.

### The rows come out reversed, and that is not a quirk of the tile

`print_sprite_pix` draws UPWARD: the first data row lands at the given y
and later rows stack above it. The texture loop starts at `LD HL,$0F00`
— y=15 — and steps down a band at a time, so within each 16-row band the
texture's row 0 is at the BOTTOM.

The port's `paint_bg_to_buff` indexes with `ty = y & 15` from the top, so
this writes the rows reversed: `tile[j] = texture[15 - j]`. Doing it here
keeps `paint_bg_to_buff` untouched and the output byte-identical to the
capture-derived file it replaces (verified before the switch).

The attribute byte of each block is that cycle's background attr — $46,
$44, $45, $47 — which is `bg_attr_per_cycle[]` in the port.

Output blob layout (4 * 32 = 128 B): tile[cycle] for cycle 0..3.
"""
import sys
from pathlib import Path

# Z80 address -> offset in the headless data block.
BLOCK = Path('original/blocks/03_DATA_headless.dat.bin')
BASE = 0x6800
TEXTURES = (0xC015, 0x8EE8, 0x8F10, 0x8F38)

SPRITE_W = 2
SPRITE_H = 16


def main():
    out_path = Path(sys.argv[1]) if len(sys.argv) > 1 \
        else Path('assets/bg_tile.bin')
    data = BLOCK.read_bytes()

    buf = bytearray()
    for cycle, addr in enumerate(TEXTURES):
        off = addr - BASE
        w, h = data[off], data[off + 1]
        if (w, h) != (SPRITE_W, SPRITE_H):
            sys.exit(f'texture {cycle} at ${addr:04X} has header ({w}, {h}), '
                     f'expected ({SPRITE_W}, {SPRITE_H}) — the block moved')
        px = data[off + 2:off + 2 + w * h]
        # print_sprite_pix stacks rows upward; paint_bg_to_buff indexes
        # from the top. Reverse.
        for j in range(h):
            src = (h - 1 - j) * w
            buf += px[src:src + w]

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(bytes(buf))
    print(f'wrote {out_path} ({len(buf)} B = 4 cycles x {SPRITE_W * SPRITE_H} B)')


if __name__ == '__main__':
    main()
