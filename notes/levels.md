# Level data — pointer table, layout, cell vocabulary

Decoding reference for the 15 levels at
`original/blocks/03_DATA_headless.dat.bin` (= our
`assets/levels.bin`, 2700 B = 15 levels × 180 B). The runtime
brick collision + rendering logic lives in `src/main.cpp`
(`brick_collision`, `print_one_brik_buf_c`) and is documented in
[`blitter-port.md`](blitter-port.md).

## Pointer table at `0x6CBD`

Array of 15 little-endian 16-bit pointers. Indexed by `(0xB7EA)`
(the level number, 0-based), accessed by `sub_9779h`:

```
sub_9779h:                 ; A = (0xB7EA) on entry
    ld hl, 0x6CBD
    add a, a               ; index *= 2
    ld e, a / ld d, 0
    add hl, de             ; HL = &table[A]
    ld e, (hl) / inc hl / ld d, (hl)   ; DE = *HL
    ex de, hl
    ld (l9789h), hl        ; install as live brick-list pointer
    ret
```

Result installed at `(0x9789)`, which is what `ld iy, (0x9789)` in
`sub_ad8fh` reads as the **brick descriptor list IY** for the
per-frame brick blit.

## The pointers

| Level | Pointer  |
|-------|----------|
|  1    | `0x6CDB` |
|  2    | `0x6D8F` |
|  3    | `0x6E43` |
|  4    | `0x6EF7` |
|  5    | `0x6FAB` |
|  6    | `0x705F` |
|  7    | `0x7113` |
|  8    | `0x71C7` |
|  9    | `0x727B` |
| 10    | `0x732F` |
| 11    | `0x73E3` |
| 12    | `0x7497` |
| 13    | `0x754B` |
| 14    | `0x75FF` |
| 15    | `0x76B3` |

Delta between consecutive levels = exactly `0xB4` = 180 bytes. Total
level-data span: `0x6CDB..0x7766` = **2700 B = 15 × 180**.

## Cell format

Each level is **180 raw bytes**, one byte per cell in a **12-row ×
15-col** grid (row-major). Per cell:

- `bit 7` (`0x80`) — no brick / destroyed at runtime (skip rendering)
- `bit 6` (`0x40`) — present only as `0xC0` (= bit 7 + 6) = "empty
  sentinel" (never had a brick; level_attrs still applies a strip
  attr there).
- `bit 5` (`0x20`) — undestructible (renders, bounces, no destruction
  — e.g. `$2B`, `$2C`, `$2D`, `$2E`).
- `bit 4` (`0x10`) — "this hit destroys": 1-hit destructible OR a
  multi-hit brick already damaged once and primed for the next hit
  to take it out (e.g. `$11`..`$15`).
- low nibble (`0x0F`) — brick TYPE / COLOUR index into
  `briks_colors[]` (`src/main.cpp`).

Cells with bit 7 set are skipped during rendering. All others get
blitted as a 16×8-px sprite at `(1 + 2*col byte_col, 4 + row char_row)`
via `print_one_brik_buf_c`.

## Cell-value vocabulary (across all 15 levels, 2700 cells)

| Value | Count | Pct   | Kind                                   |
|-------|-------|-------|----------------------------------------|
| `$C0` | 1494  | 55.3% | empty sentinel (bit 7+6)               |
| `$15` |  178  |  6.6% | brick `$15` → briks_colors[5] (= `$70`) |
| `$13` |   90  |  3.3% | brick `$13` → briks_colors[3] (= `$5F`) |
| `$12` |   86  |  3.2% | brick `$12` → briks_colors[2] (= `$4F`) |
| `$14` |   67  |  2.5% | brick `$14` → briks_colors[4] (= `$20`) |
| `$11` |   54  |  2.0% | brick `$11` → briks_colors[1] (= `$57`) |
| `$07` |  216  |  8.0% | hard `$07` → briks_colors[7] (= `$57`) |
| `$06` |  109  |  4.0% | hard `$06` → briks_colors[6] (= `$47`) |
| `$09` |  109  |  4.0% | hard `$09` → briks_colors[9] (= `$4F`) |
| `$2E` |   92  |  3.4% | undestructible `$2E` → briks_colors[14] (= `$5F`) |
| `$2B` |   93  |  3.4% | undestructible `$2B` → briks_colors[11] (= `$47`) |
| `$08` |   50  |  1.9% | hard `$08` → briks_colors[8] (= `$5F`) |
| `$2C` |   54  |  2.0% | undestructible `$2C` → briks_colors[12] (= `$57`) |
| `$2D` |    8  |  0.3% | undestructible `$2D` → briks_colors[13] (= `$4F`) |

Three categories:
- `$11..$15` (low nibble 1..5, bit 4 set) = **1-hit destructible**.
- `$06..$09` (low nibble 6..9, bit 4 clear) = **multi-hit** (= 2 hits
  to destroy; first hit sets bit 4 → matches `$16..$19` → next hit
  destroys).
- `$2B..$2E` (low nibble 11..14, bit 5 set) = **undestructible metal**.
- `$C0` = empty (no brick here in this level layout).


## How much of `level_attrs.bin` is actually data (2026-08-09)

`assets/level_attrs.bin` is 15 x 768 = 11520 bytes of ZX attribute
cells, extracted from emulator captures. PLAN.md WS7 wants it gone.
Measured, before porting any of the writer:

| region | bytes | share | status |
|---|---|---|---|
| live-brick cells | 2412 | 20.9% | **derivable exactly** |
| rest of the brick zone (empty cells) | 2988 | 26.0% | side-strip / bg colours, not yet derived |
| HUD rows, side strips, bottom | 6120 | 53.1% | not yet derived |

The derivation for the first row of that table is two rules:

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

Running after the bricks is the whole point: a brick in field column 0
gets the bright bit taken back off its LEFT char. Without that rule the
match is 2374/2412, and every one of the 38 misses differs by exactly
$40. With it, 2412/2412.

I went looking for that discrepancy expecting a runtime bug — that
`repaint_row_attrs` re-brightens char column 1 and loses the shadow.
It does write there, but `render_brick_band_rows` calls
`dim_border_shadow_column(cr0, cr1)` afterwards, and `cr1` is exactly
row `r1+1`'s cell row, so the repaint is always covered. No bug; the
port had `print_border_shadow_c` all along.

`test-level-attrs-derivable` locks this in. It is not only WS7 evidence:
the extraction is manual and off the build graph (see the Makefile note
on `frame_l1.bin`'s twin problem), so nothing else would notice a
re-extraction that no longer matches `assets/levels.bin`.


## The empty brick-zone cells resist a predicate (2026-08-09)

The live-brick fifth of `level_attrs.bin` is derived exactly
(`test-level-attrs-derivable`). The next 26% is the EMPTY cells of the
brick zone — destroyed markers and the `$C0` sentinel. Measured, they
take exactly two values per colour cycle:

| cycle | levels | bright | dimmed |
|---|---|---|---|
| 0 | 1, 5, 9, 13  | `$46` | `$06` |
| 1 | 2, 6, 10, 14 | `$44` | `$04` |
| 2 | 3, 7, 11, 15 | `$45` | `$05` |
| 3 | 4, 8, 12     | `$47` | `$07` |

which is `bg_attr_per_cycle[]` and the same value with bit 6 cleared.
So the only question is WHICH cells are dimmed.

I tried to answer it with a neighbour predicate and got close enough to
be misleading:

    dimmed if char col 1 (border shadow)                    89.4%
      ... or the cell to the left is live                   91.8%
      ... or the cell above-left is live                    94.4%

**That is curve-fitting, not deriving,** and 94.4% is exactly the range
where one more term looks tempting. Stopped there deliberately.

The real answer is not a predicate over neighbours at all: it is the
ORDER of the passes. `print_briks` walks the band row by row calling
`brik_shadow`, which dims both chars of every LIVE brick's column at
`brik_attr_buf`'s row; the NEXT row's own print then re-brightens its
live cells over the top; and `print_border_shadow` runs last over
column 1 and row 1. An empty cell's value is whatever survives that
sequence, which depends on the order, not on a local rule.

### Settled by simulation, not by a rule (2026-08-09)

Done, and it works. `tests/test_bricks.cpp`'s `attrs_generate` fills the
band with `bg_attr_per_cycle[]`, runs `paint_bricks`, applies
`print_border_shadow`'s left arm, and compares char rows 4..15, cols
1..30 against `level_attrs.bin`:

    attrs_generate               5400 cells, 15 levels

**All 5400 match, for all 15 levels.** That is the whole brick zone —
46.9% of the blob — generated from `assets/levels.bin` and the tape's
`briks_colors`, with no reference to the capture at all.

The instrument had to be `paint_bricks`, NOT `paint_brick_band`. The
latter starts with `memcpy(&attr_buff[3*32], &lattr[3*32], 14*32)` — it
re-bases from `level_attrs.bin` — so comparing its output against the
blob would have compared the blob with itself. `paint_bricks` is the
generator: it walks the rows calling `paint_shadow_row` (the
`brik_shadow` port) exactly where `print_briks` does.

Mutations confirm the test bites: removing the shadow pass, and moving
`brik_attr_buf` one char row, are both caught.

What is left of the blob is the HUD rows, the side-frame strips and the
bottom bat/lives rows — 53.1%, and none of it is brick work.


## All of level_attrs.bin is accounted for (2026-08-09)

768 cells per level, every one derived and gated:

| cells | region | source | gate |
|---|---|---|---|
| 630 | rows 3..23, cols 1..30 | `paint_bricks` run in order | `attrs_generate` (host) |
|  32 | row 0, all cols | the eight top border sprites' attrs | `test-frame-derivable` |
|  42 | rows 3..23, cols 0/31 | the six side placements' attrs | `test-frame-derivable` |
|   4 | rows 1..2, cols 0/31 | the SEVENTH side placement | `test-frame-derivable` |
|  60 | rows 1..2, cols 1..30 | `bg_attr` + `print_border_shadow` | `test-level-attrs-derivable` |

The host test's range grew from rows 4..15 to 3..23 to get there. Row 16
is why: it is the shadow row for field row 11, written by
`paint_shadow_row` like every other, and a bg-plus-border-shadow rule
misses 152 of its cells across the 15 levels with nothing local
explaining them. Rows 17..23 and row 3 are plain background and cost
nothing to include.

Rows 1 and 2 are the HUD's, and they are plain `bg_attr` with the border
shadow — 900 of 900. The 1UP/HI/2UP labels and the score digits are
PIXELS; they do not touch attribute cells. Same colour-clash rule as
known-bugs #7's enemy.

**The blob is now redundant, and removing it is plumbing rather than
analysis.** `paint_brick_band` opens by re-basing the band from it at
every level entry, and `paint_frame_to_buff` takes its attrs from it.
Both have generators sitting next to them.


## level_attrs is generated now, and may not be needed at all

`build_level_attrs_from_data` replaces the `LVLATTR.BIN` load, running
the attribute passes in `game_screen_draw_to_buffer`'s order: `bg_attr`
everywhere, the frame sprites' attr blocks, `paint_bricks` (which calls
`paint_shadow_row` per row), then `print_border_shadow`. All 15 levels
come out pixel-identical.

Mutating that generator taught me something. Dropping the brick-attr
pass entirely, and shortening `print_border_shadow`'s column by a row,
BOTH survive the whole suite — and they are equivalent mutants, not
missed coverage. `paint_brick_band` calls `paint_bricks` again at every
level entry, and `render_brick_band_rows` re-applies
`dim_border_shadow_column` for the rows it rebuilds. The runtime passes
redo the work.

So the base band's brick colours and its border shadow are both dead
weight in the generated array. What is NOT redundant is the frame's
columns 0/31 and row 0, which `paint_frame_to_buff` reads directly.

Worth measuring next: fill `level_attrs[]` with nothing but `bg_attr`
plus the frame cells and see whether all 15 levels still match. If they
do, the whole 11 KB array collapses to a handful of per-cycle values and
the frame attrs — and `paint_brick_band`'s opening `memcpy` goes with
it.
