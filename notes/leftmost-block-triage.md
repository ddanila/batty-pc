# Per-level leftmost brick — useful for collision-edge debugging

Historical context: a user-reported "can't hit the leftmost block"
symptom was eventually traced to undestructible metal at col 0 in
several levels (L5 / L7 / L8 / L9 / L12). The collision math is correct;
the cell at col 0 simply bounces in those levels (= `$2x` with bit 5
set, undestructible by design).

This table is useful when reproducing a "ball/bullet doesn't kill the
leftmost brick" report — different levels have the leftmost brick at
different col/row positions, and some are correctly undestructible.

## What "col 0" actually is

`LVL_COLS = 15`, so the level grid is cols 0..14. Col 0 occupies
playfield `x = 8..23`. The frame side strip (left ornament) paints at
`x = 0..7` (`FRAME_SIDE_W = 1` byte after iter-18). At y = 57..62 the
side-strip byte is `$9F` (= bit 7 set, frame ornament column 0). The
brick at col 0 sits at byte_col 1..2 (= `x = 8..23`), just to the right
of the side strip.

## Per-level leftmost visible brick

| Level | Leftmost VISIBLE brick (= bit4=0/1, bit7=0) | Destructible? |
|-------|---------------------------------------------|---------------|
| L1    | row 0 col 3 (`$07`)                         | yes (2-hit)   |
| L2    | row 0 col 2 (`$06`)                         | yes (2-hit)   |
| L3    | row 1 col 0 (`$06`) ← col 0!                | yes (2-hit)   |
| L4    | row 1 col 5 (`$06`)                         | yes (2-hit)   |
| L5    | row 0 col 0 (`$2E`) ← col 0!                | **no (metal)** |
| L6    | row 0 col 2 (`$07`)                         | yes (2-hit)   |
| L7    | row 0 col 1 (`$2B`)                         | **no (metal)** |
| L8    | row 0 col 1 (`$2B`)                         | **no (metal)** |
| L9    | row 0 col 2 (`$2C`)                         | **no (metal)** |
| L10   | row 2 col 3 (`$09`)                         | yes (2-hit)   |
| L11   | row 0 col 1 (`$07`)                         | yes (2-hit)   |
| L12   | row 0 col 2 (`$2B`)                         | **no (metal)** |
| L13   | row 5 col 1 (`$06`)                         | yes (2-hit)   |
| L14   | row 3 col 3 (`$07`)                         | yes (2-hit)   |
| L15   | row 0 col 0 (`$09`) ← col 0!                | yes (2-hit)   |

For cell vocabulary see [`levels.md`](levels.md).
