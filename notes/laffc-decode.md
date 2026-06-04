# LAFFC — brick-collision decode (the next parity port)

The frame-step gate (`make capture-timeline-both`) isolates the
port-vs-original residual to **brick collision**: the ball starts inside
the L3 brick band, hits a brick on frame 1, and the port's
`brick_collision` (`src/main.c`) reflects/positions it differently from
the original `LAFFC` (`original/disasm/batty.asm:4470`). Frame-1 residual
is ~212 px, dominated by white↔magenta swaps = the **ball at a different
position** because the two collide differently. First divergence is
around brick grid **col 6–7, row 4–7** (ball seeds at x=$6C col 6,
y=$4E row 5). This note decodes `LAFFC` so it can be ported faithfully.

## IX object fields used

- `IX+$02/$03` = X hi/lo (q8.8 pixel/frac). `IX+$04/$05` = Y hi/lo.
- `IX+$06` = 6-bit direction. `IX+$0C` = body width px, `IX+$0D` = body
  height px. `IX+$00` = sprite/type (`AND $3F` == $05 → spark; `AND $7F`
  == $02 ball / $05 / $08-09 enemy).

## Grid

`current_level_addr` points at a brick grid with **row stride $0F (15)**.
Cell byte bit 7 = destroyed (1 = gone), bit 5 = undestructible, bit 4 =
multi-hit "half hit" flag, low nibble = brick type (>=6 metal). Brick
band starts at screen Y=$20 (32), rows are 8 px tall; columns start at
X=$08, 16 px wide (so the X loop steps $10 and the cell-in-row steps by
1 with an X step of $08… see LAFFC_4).

## Phase 1 — early exits (LAFFC..4478)

- `flag_2 = 0` (will be set to 1 if a collision is handled).
- if `Y (IX+$04) >= $80` → ret (ball below playfield).
- if `Y + height (IX+$0D) < $20` → ret (entirely above the brick band).

## Phase 2 — find the row (LAFFC_0..LAFFC_2, 4479-4501)

`IY = current_level_addr`, `DE=$000F` (row stride), `H=0` (row index),
`B=$0C` (12 rows), `C=$20` (current row's top Y). Loop:

- `A = C - Y`. If borrow (C<Y) → LAFFC_1: `A += 8`; if still borrow the
  ball top is within this row's 8px band → LAFFC_3 (found). Else fall.
- else `A = (C - Y) - height`. If borrow → LAFFC_3 (found). else LAFFC_2:
  advance `IY += $0F`, `C += 8`, `H++`, `DJNZ`. ret if no row matched.

So it finds the **row band the ball's Y span falls in**, leaving `H` =
row index, `C` = that row's top Y, `IY` = start of that row.

## Phase 3 — find the column (LAFFC_3..LAFFC_5, 4503-4526)

- `brik_value+1 = H` (row index, stashed). `LB087+1 = Y + height - C`
  (penetration depth into the row from the top — used later at LAFFC_6).
- `H = C` (reuse H as the cell top-Y). `A = X (IX+$02)`, `BC=$1008`
  (B=$10 step, C=$08 start). `A -= C` then loop LAFFC_4: `A -= B` while
  no borrow, `IY++`, `C += B`. Leaves `L = C` (cell left-X), `IY` →
  the cell at the ball's X, `LB069+1 = A` (X penetration remainder).

So after phase 3: `IY` points at the **cell under the ball's (X,Y)**,
`H` = cell top Y, `L` = cell left X.

## Phase 4 — neighbour-bit mask D (LAFFC_5..LAFFC_13, 4527-4611)

`D=$0F` (all 4 sides candidate). The routine adjusts which cell `IY`
finally lands on (handling the case where the ball overlaps two cells
horizontally — the `$E8`/`IX+$0C` width tests, LAFFC_5/6 and the
`+$10`/`INC IY` steps), marks `brik_value+1` (the chosen row), then
clears D bits for any neighbour that is **not solid** (bit7 set =
destroyed), per side:

- bit 1 (right): if `L==$E8` edge or `(IY+$01)` destroyed → `RES 1,D`.
- bit 0 (left):  if `L==$08` edge or `(IY-$01)` destroyed → `RES 0,D`.
- bit 2 (up):    if `H<$21` or `(IY-$0F)` destroyed → `RES 2,D`.
- bit 3 (down):  if `H>=$78` or `(IY+$0F)` destroyed → `RES 3,D`.

So **D = which of the 4 neighbours are solid** (would block a bounce on
that side). `$E8`/`$08` are the right/left playfield X edges; `$21`/`$78`
the top/bottom brick-band Y edges.

## Phase 5 — gate D by ball direction (LAFFC_13..LAFFC_17, 4612-4636)

Spark type (`IX+$00 AND $3F == $05`) skips to LAFFC_29 (always bounce
down). Otherwise, using direction `IX+$06`:

- if `dir < $20` → moving up-ish → `RES 3,D` (can't bounce off "down").
  else → `RES 2,D` (can't bounce off "up").
- `(dir + $10) AND $3F`: if `>= $20` → `RES 0,D` else `RES 1,D`
  (left/right gate by the rotated direction).

So D is reduced to **only the sides the ball is actually travelling
toward** that are also solid.

## Phase 6 — choose axis from D + penetration (LAFFC_18..LAFFC_25)

`A = D`, shift bits out (bit0 left, bit1 right, bit2 up, bit3 down):

- if exactly one of {left,right} set and no vertical → horizontal bounce
  (LAFFC_26/27: snap X to cell edge, `change_direction` with B=$1F).
- if exactly one of {up,down} → vertical bounce (LAFFC_28/29, B=$3F).
- if BOTH an X and a Y side are solid (corner), LAFFC_21-25 compares the
  **X penetration vs Y penetration** (using `IX+$0C/$0D` body size, `L`,
  `H`) and bounces off the **shallower-penetrated axis**, clearing the
  other axis bits and re-deciding (`JP LAFFC_17/18`).

`change_direction` (`$AD5C…`, decoded): `dir = ((dir XOR B) + 1) AND
$3F`, B=$1F flips the vertical component, B=$3F flips horizontal.

## Phase 7 — destroy / half-hit / shimmer (LAFFC_30..LAFFC_38)

After the bounce + position snap: `flag_2=1`. Then per cell bits:
undestructible (bit5) → just shimmer slot (LAFFC_34, allocate a
`briks_data` slot, max 5); multi-hit first contact → `SET 4` + shimmer;
SMASH (bat+$14==$07) or bit4 already set → destroy (LAFFC_38: dec
`briks_quantity`, `points_calc_and_add`, …). The port already mirrors
this half-state/destroy/shimmer logic in `brick_collision`; the GAP is
phases 2–6 (cell-finding + neighbour-gated axis), which the port
approximates with "first non-destroyed cell in scan order + overlap
axis".

## Port plan (next iterations, gate-verified)

1. Port phases 2–3 (exact cell at ball X/Y, row stride 15, the $08/$10
   steps) as a helper returning `(row,col, pen_x, pen_y)`.
2. Port phase 4 (neighbour-bit mask D) reading `live_level` with bit7 =
   destroyed and the $08/$E8/$21/$78 edge constants.
3. Port phases 5–6 (direction gate + axis from D + penetration compare)
   and `change_direction`, then snap position like LAFFC_26-29.
4. Keep the existing destroy/half-hit/shimmer tail (phase 7) — it matches.
5. After each phase, run `make capture-timeline-both` and watch the
   frame-1/3/5 ROI residual fall toward 0. Keep each commit a working
   game (the gate frames + manual `make run` sanity).

## Implementation status (2026-06-04)

`laffc_collision` (`src/main.c`) is in, gated behind **`BATTY_LAFFC=1`**
(default off → the proven `brick_collision` still runs, static
regression 5/5). The shared destroy/half-hit/shimmer tail was extracted
into `brick_hit_resolve(col,row,axis)` (behavior-preserving) and is used
by both paths. Done so far: phases 1–3 (byte-faithful row/column cell
finder), phase 4 (neighbour-solidity mask), phase 5 (direction gate),
and a first-cut phase 6 (bounce off the gated solid neighbour, destroy
that cell, map to the port's axis reflect).

**Current gate numbers** (`BATTY_LAFFC=1`, frames 0/1/3/5, brick ROI):
0 / **415** / 604 / 486 px — *worse* than `brick_collision`'s 0 / 212 /
333 / 494. The first cut is not yet correct because:

- it returns an axis for `step_ball` to reflect via
  `ball_reflect_descriptor` (`0x3F-dir` / `0x1F-dir`), but `LAFFC`
  bounces with `change_direction` (`((dir^B)+1)&0x3F`, B=$1F horizontal,
  B=$3F vertical) and snaps the ball to the cell edge itself — different
  resulting direction and position;
- the corner case (LAFFC_21–25 penetration-depth axis pick) and the
  two-cell-overlap `IY` adjustment (phase 4 head) are not ported, so the
  chosen neighbour/axis is sometimes wrong;
- it may destroy a neighbour the original does not (mask/own-cell edge).

Next: give `laffc_collision` its own `change_direction` reflect + cell-
edge position snap (return "handled, don't let step_ball re-reflect"),
then add the penetration-depth corner case, re-measuring the gate each
step until it beats `brick_collision`, then flip the default.
