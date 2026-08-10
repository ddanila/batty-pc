# Level data — pointer table, cell format, attribute derivation

Decoding reference for the 15 levels in
`original/blocks/03_DATA_headless.dat.bin` (= `assets/levels.bin`, 2700 B =
15 x 180 B). The runtime collision and rendering live in `src/bricks.cpp`
and `src/physics.cpp`; see `notes/blitter-port.md` and
`notes/laffc-decode.md`.

## Pointer table at `0x6CBD`

Fifteen little-endian 16-bit pointers, indexed by `(0xB7EA)` (the 0-based
level number):

```
sub_9779h:                 ; A = (0xB7EA) on entry
    ld hl, 0x6CBD
    add a, a               ; index *= 2
    ld e, a / ld d, 0
    add hl, de             ; HL = &table[A]
    ld e, (hl) / inc hl / ld d, (hl)   ; DE = *HL
    ex de, hl
    ld (l9789h), hl        ; install as the live brick-list pointer
    ret
```

L1 starts at `0x6CDB` and each level is exactly `0xB4` = 180 bytes further
on, through L15 at `0x76B3`; the span `0x6CDB..0x7766` is the whole 2700 B.
Poking `$B7EA` and jumping to `$BA24` (`briks_calc`) is what lets the
capture and oracle tooling reach any level — see `notes/modded-batty.md`.

## Cell format

180 bytes per level, one byte per cell in a **12-row x 15-col** grid,
row-major. Row stride is therefore $0F, which is what `LAFFC` walks.

| bit | meaning |
|-----|---------|
| 7 (`$80`) | no brick / destroyed at runtime — skip rendering |
| 6 | only ever seen as `$C0` (bits 7+6) = "never had a brick" sentinel |
| 5 (`$20`) | undestructible metal |
| 4 (`$10`) | "this hit destroys" — 1-hit, or a multi-hit already damaged |
| 0-3 | brick TYPE / colour index into `briks_colors[]` |

Three categories, and the whole vocabulary across all 2700 cells:

| kind | values | cells |
|---|---|---|
| empty sentinel | `$C0` | 1494 (55.3%) |
| 1-hit destructible | `$11`..`$15` | 475 |
| multi-hit (2 hits) | `$06`..`$09` | 484 |
| undestructible metal | `$2B`..`$2E` | 247 |

A multi-hit brick's first hit SETs bit 4, which turns `$06`..`$09` into
`$16`..`$19`, and the next hit takes the bit-4 branch and destroys it.

## Deriving `level_attrs.bin` — all 768 cells per level

`assets/level_attrs.bin` is 15 x 768 bytes of ZX attribute cells, originally
extracted from emulator captures. `build_level_attrs_from_data` replaces the
load entirely, and all 15 levels come out pixel-identical. Every region is
accounted for:

| cells | region | source | gate |
|---|---|---|---|
| 630 | rows 3..23, cols 1..30 | `paint_bricks` run in order | `attrs_generate` (host) |
| 32 | row 0, all cols | the eight top border sprites' attrs | `test-frame-derivable` |
| 42 | rows 3..23, cols 0/31 | the six side placements' attrs | `test-frame-derivable` |
| 4 | rows 1..2, cols 0/31 | the seventh side placement | `test-frame-derivable` |
| 60 | rows 1..2, cols 1..30 | `bg_attr` + `print_border_shadow` | `test-level-attrs-derivable` |

Rows 1 and 2 are the HUD's and are plain `bg_attr` plus the border shadow —
900 of 900. The 1UP/HI/2UP labels and the score digits are PIXELS and touch
no attribute cell, the same colour-clash rule as the enemy's
(known-bugs #7).

### The live-brick cells are two rules

    attr = briks_colors[cell & 0x0F]      both halves of the cell
    attr &= 0xBF  at char column 1        the border's drop shadow

`briks_colors` is the 16-byte table at `$AEEC`. The second rule is
`print_border_shadow` ($BFCF), which `game_screen_draw_to_buffer` calls
AFTER `print_briks`:

    print_border_shadow:
      LD HL,attr_buff+$21 / LD B,$17 / LD DE,$0020
      LBFCF_0: RES 6,(HL) / ADD HL,DE / DJNZ LBFCF_0   ; col 1, rows 1..23
      LD HL,attr_buff+$22 / LD B,$1D
      LBFCF_1: RES 6,(HL) / INC L / DJNZ LBFCF_1       ; row 1, cols 2..30

Running AFTER the bricks is the whole point: a brick in field column 0 gets
the bright bit taken back off its LEFT char. Without that rule the match is
2374/2412 and every one of the 38 misses differs by exactly `$40`. With it,
2412/2412.

### The EMPTY cells are settled by ORDER, not by a predicate

The empty cells of the brick zone take exactly two values per colour cycle —
`bg_attr_per_cycle[]` and the same value with bit 6 cleared:

| cycle | levels | bright | dimmed |
|---|---|---|---|
| 0 | 1, 5, 9, 13  | `$46` | `$06` |
| 1 | 2, 6, 10, 14 | `$44` | `$04` |
| 2 | 3, 7, 11, 15 | `$45` | `$05` |
| 3 | 4, 8, 12     | `$47` | `$07` |

so the only question is WHICH are dimmed. Neighbour predicates get close
enough to mislead — "dimmed if char col 1" is 89.4%, adding "or the cell to
the left is live" 91.8%, "or above-left is live" 94.4%. **That is
curve-fitting, not deriving.**

The real answer is the ORDER of the passes. `print_briks` walks the band row
by row calling `brik_shadow`, which dims both chars of every LIVE brick's
column at the row below; the NEXT row's own print re-brightens its live cells
over the top; and `print_border_shadow` runs last. An empty cell's value is
whatever survives that sequence.

`attrs_generate` in `tests/test_bricks.cpp` simulates it: fill the band with
`bg_attr_per_cycle[]`, run `paint_bricks`, apply `print_border_shadow`'s left
arm, compare char rows 3..23, cols 1..30 against the blob. **All 5400 cells
match, for all 15 levels.**

The instrument has to be `paint_bricks`, NOT `paint_brick_band`: the latter
opens with a `memcpy` from `level_attrs.bin`, so comparing its output against
the blob compares the blob with itself. Row 16 has to be in range too — it is
the shadow row for field row 11, and a bg-plus-border-shadow rule misses 152
of its cells across the 15 levels.

### Which passes are redundant, measured

Three builds of the generator, all 15 levels each:

| generator | result |
|---|---|
| bg + frame attrs only | **FAIL** — L01 off by 1696 px, L03 by 1608 |
| bg + frame attrs + border shadow | all 15 pixel-identical |
| the above + `paint_bricks` | all 15 pixel-identical |

So the brick COLOURS in the generated base band are dead weight and the
border shadow is not. The base band is the EMPTY playfield's attributes; the
port repaints live bricks at every level entry through `paint_brick_band`,
and `render_brick_band_rows` re-applies `dim_border_shadow_column` for the
rows it rebuilds. The runtime passes redo the work.

Two mutants survive for exactly that reason — dropping the brick-attr pass,
and shortening `print_border_shadow`'s column by a row — and they are
EQUIVALENT mutants, not missed coverage.

It also makes `reset_destroyed_cell_attrs`' name half-wrong. Its reset half,
clearing brick colour out of cells whose brick is gone, is a no-op against a
band that never had brick colour. Its SHADOW half still earns its keep: a
destroyed cell's left char goes non-bright when its left neighbour is live,
and nothing else writes that.

The array has not collapsed — `paint_frame_to_buff` reads char rows 0..2
across all 32 columns plus rows 3..23's columns 0 and 31, and
`paint_brick_band` re-bases rows 3..16 so the `$C0` sentinel cells keep their
background. What has gone is any dependence on a capture.

`test-level-attrs-derivable` locks the derivation in, and not only as WS7
evidence: the blob's extraction is manual and off the build graph, so nothing
else would notice a re-extraction that no longer matches
`assets/levels.bin`.
