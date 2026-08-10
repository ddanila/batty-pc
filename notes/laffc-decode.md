# LAFFC — brick collision, decoded

`LAFFC` (`original/disasm/batty.asm:4470`) is the original's collision
sweep, shared by the ball, the laser bullet, the sparks and the enemy.
Ported as `laffc_collision` + `laffc_sweep` + `laffc_bounce`; it is the
DEFAULT for every ball and is byte-exact against the Spectrum — hit cell,
bounce axis, snapped position, direction and q8.8 fraction — over L3's
150-frame trajectory and L5's edge-metal case
(`test-laffc-ball-frame1`, `test-laffc-ball-l5-metal`).
`BATTY_LEGACY_COLLISION=1` reverts to the older approximate
`brick_collision`, which also stays in as a fallback when LAFFC reports
no hit, so no path can pass a brick through.

## Object fields and the grid

`IX+$02/$03` = X hi/lo (q8.8 pixel/fraction), `IX+$04/$05` = Y hi/lo,
`IX+$06` = 6-bit direction, `IX+$0C/$0D` = body width/height,
`IX+$00` = sprite set (`AND $7F`: $02 ball, $05 bullet/spark, $08-09
bird/UFO).

`current_level_addr` points at a 12-row x 15-col grid, **row stride $0F**.
Per cell: bit 7 destroyed, bit 5 undestructible, bit 4 "this hit
destroys", low nibble = colour index. The band starts at screen Y=$20,
rows are 8 px; columns start at X=$08 and are 16 px wide.

## Phase 1 — early exits

`flag_2 = 0`; return if `Y >= $80` (below the playfield) or
`Y + height < $20` (entirely above the band).

## Phase 2 — find the row (LAFFC_0..2)

Walk 12 rows with `C` = the row's top Y (from $20, +8 each) and the
borrow of `C - Y` / `(C - Y) - height` deciding whether the ball's Y span
falls inside this band. Leaves the row index, its top Y, and `IY` at the
row start.

## Phase 3 — find the column (LAFFC_3..5)

`A = X - $08`, then subtract $10 while no borrow, stepping `IY` and
tracking the cell's left X. Leaves `IY` at the cell under the ball's
(X, Y), plus both penetration remainders.

## Phase 4 — land on the cell the BODY overlaps (LAFFC_5-6)

When the reference cell is empty the routine straddles: horizontally when
`remainder + width >= 16` (and not at `$E8`), and **vertically down a row
when `Y + height - band_y >= 8`** (and not at `$78`). The port tries own
-> right -> down -> down-right.

The vertical straddle is not optional. Omitting it made the port miss a
side bounce the original makes around L3 frame 4-5: the row-finder
correctly assigns a ball at y=64 to row 3 (spanning 56-64), while the
brick it is entering is in row 4, the row its BODY penetrates.

## Phase 4b — the open-face mask (LAFFC_7..13)

`D = $0F` (all four faces OPEN); a bit is RESet — that face closed — only
when the neighbour is SOLID *and* the cell is not against that playfield
boundary:

    left  (bit 0): Lx == $08  || EMPTY(row, col-1)
    right (bit 1): Lx == $E8  || EMPTY(row, col+1)
    up    (bit 2): Hy <  $21  || EMPTY(row-1, col)
    down  (bit 3): Hy >= $78  || EMPTY(row+1, col)

**A brick against a boundary keeps that boundary face OPEN** — the ball
bounces off it. Writing the conditions the other way round (`Lx != $08 &&
EMPTY`) closes it, and for INTERIOR cells both forms reduce to
`EMPTY(neighbour)`, so the inverted version was byte-exact on L3's
interior trajectory for the whole campaign.

At a boundary they diverge, and that was known-bugs #6: a ball moving
straight down (dir $10) into a row-0 metal brick with an empty right
neighbour got a HORIZONTAL bounce — and flipping the dx component of a
pure-vertical direction is a no-op, so the ball was nudged sideways,
snapped, and kept falling THROUGH the brick. Destructible bricks broke
regardless of the axis, which hid it; metal ones cannot.
`test-ball-no-tunnel` covers the class (L5/L7 have the row-0 metal).

## Phase 5 — gate the mask by direction (LAFFC_13..17)

A spark (`AND $3F == $05`) skips to LAFFC_29 and always bounces down.
Otherwise:

- `dir < $20` (moving up) -> `RES 3` (cannot bounce off "down");
  else `RES 2`.
- `(dir + $10) & $3F >= $20` -> `RES 0`, else `RES 1`.

So the mask keeps only faces the ball is actually travelling toward.

## Phase 6 — pick the axis (LAFFC_18..29)

One horizontal face and no vertical -> horizontal bounce (snap X to the
cell edge, `change_direction` with `B=$1F`). One vertical -> vertical
bounce (`B=$3F`). Both — a corner — compares X against Y penetration and
bounces off the SHALLOWER axis.

**An exact tie bounces HORIZONTALLY:**

    LAFFC_25:
      LD E,D / CP C            ; A = y_pen, C = x_pen
      RES 2,D / RES 3,D        ; clear the vertical bits
      JP NC,LAFFC_17           ; y_pen >= x_pen: keep them cleared
      LD A,E / AND $0C / JP LAFFC_18

`CP C` carries only when `y_pen < x_pen`, so `JP NC` takes the
verticals-cleared branch on equality. The port's `y_pen >= x_pen`
matches.

`change_direction` ($ACEE) is `dir = ((dir XOR B) + 1) & $3F`, with
`B=$1F` flipping the horizontal component and `B=$3F` the vertical — the
same masks `bounce_wall` uses (`notes/wall-bounce.md`). The snap writes
only the pixel byte; the port must KEEP the moved q8.8 fraction, or the
ball is byte-off from the Spectrum even with the right pixel and
direction.

## Phase 7 — destroy / half-hit / shimmer (LAFFC_30..38)

`flag_2 = 1`, then per cell: undestructible (bit 5) registers a shimmer
slot only; a fresh multi-hit brick `SET`s bit 4 and shimmers; bit 4
already set (or SMASH, bat `+$14 == $07`) destroys — decrement
`briks_quantity`, `points_calc_and_add`. `notes/metal-shimmer.md`.

**The tail branches on WHO called.** Only `sprite_set $02` reaches the
destroy path, which is why the alien can hit a brick without eating the
level. `notes/enemy-movement.md` traces the enemy's branch, including the
`LB1C3` fallthrough that UNDOES the position snap.

## Three untested edges, found by mutation

A sampling pass over `laffc_sweep` turned up three surviving mutants, all
boundary conditions. Each was checked against the disassembly and the
port turned out RIGHT every time — what was missing was any test that
could tell.

| mutation | differs when | consequence |
|---|---|---|
| `a + BRICK_H_PX > 0xFF` -> `> 0xFE` | `new_y - band_y == 9` | invents a hit one px past the brick |
| `(dir+$10)&$3F >= $20` -> `> $20` | `dir == $10` | swaps LEFT/RIGHT face at pure vertical |
| `y_pen >= x_pen` -> `>` | an exact corner tie | flips ties to a vertical bounce |

Pinned by `laffc_row_scan_edge`, `laffc_dir_gate_ge_at_vertical` and
`laffc_corner_tie_goes_horizontal` in `tests/test_physics.cpp`. All three
came from differential dumps — run both variants over a neighbourhood and
diff — rather than from derivation.

The `dir == $10` case is the sharpest. The BALL never carries $10:
`bat_dir_index` skips it and a synthetic $10 ball does not return cleanly
from `LAB1F` (`notes/bat-deflection.md`), so it reads as unreachable. The
ENEMY carries it — straight down — and runs the same sweep through
`enemy_brick_reaction`. **A value impossible for one caller is ordinary
for another.**
