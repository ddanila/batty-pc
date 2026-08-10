# LAFFC — brick-collision decode (the next parity port)

The frame-step gate (`make capture-timeline-both`) isolates the
port-vs-original residual to **brick collision**: the ball starts inside
the L3 brick band, hits a brick on frame 1, and the port's
`brick_collision` (`src/main.cpp`) reflects/positions it differently from
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

  **On an exact TIE it bounces HORIZONTALLY** (decoded 2026-08-10, after
  a mutation showed nothing tested it):

        LAFFC_25:
          LD E,D / CP C          ; A = y_pen, C = x_pen
          RES 2,D / RES 3,D      ; clear the vertical bits
          JP NC,LAFFC_17         ; y_pen >= x_pen: keep them cleared
          LD A,E / AND $0C / JP LAFFC_18

  `CP C` sets carry only when `y_pen < x_pen`, so `JP NC` takes the
  verticals-cleared branch on equality. The port's `y_pen >= x_pen`
  matches; `>` would flip every tie to a vertical bounce, and 236
  positions in a single-brick neighbourhood do so. Pinned by
  `laffc_corner_tie_goes_horizontal` in `tests/test_physics.cpp`.

`change_direction` (`$ACEE`, decoded): `dir = ((dir XOR B) + 1) AND
$3F`, B=$1F flips the HORIZONTAL component (side walls), B=$3F flips
VERTICAL (top) — per `bounce_wall` ($AC75): `B=$3F` before
`check_top_margin`, `B=$1F` before `check_left/right_margin`. (An
earlier revision of this note had the two swapped and the address as
$AD5C; wall-bounce.md + lessons.md + the port agree on this corrected
reading.)

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

`laffc_collision` (`src/main.cpp`) is in, gated behind **`BATTY_LAFFC=1`**
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

### Update 2 (2026-06-04): change_direction + snap landed

Fixed the inverted mask (set bit = neighbour **empty/open**, the face the
ball reflects off — was set-when-solid), made the hit cell the ball's own
cell `(row,col)` (with a right-straddle fallback), and gave
`laffc_collision` its own reflect+snap: `change_direction`
(`((dir^mask)+1)&0x3F`, $1F horizontal / $3F vertical) and the LAFFC_26-29
cell-edge snaps (`Lx-w` / `Lx+$10` / `Hy-h` / `Hy+8`), returning code `3`
so `step_ball` adopts the snapped position/dir without re-reflecting.

**Gate now (`BATTY_LAFFC=1`, frames 0/1/3/5):** 0 / 220 / **276** /
**426** px vs `brick_collision`'s 0 / 212 / 333 / 494. The LAFFC path now
**beats** the approximation on the accumulating frames 3/5 (frame 1 ~tied,
+8 px). Still not 0 — remaining: the penetration-depth corner case
(LAFFC_21-25, two open faces → pick the shallower axis) and the exact
two-cell straddle/`IY` adjustment (phase 4 head). Default still
`brick_collision` (static 5/5); flip once the gate reaches ~0.

### Update 3 (2026-06-04): corner case ported; frame-1 residual is motion, not collision

Ported LAFFC_21-25 (phase 5b): when the direction gate leaves both an
open horizontal and vertical face, compute X-pen / Y-pen from the open
sides and bounce off the shallower (Y-pen >= X-pen → horizontal). Gate
unchanged (0/220/276/426) — this trajectory never hits a two-open-face
corner in frames 1-5, so it is a faithful, no-regression addition that
will matter for corner hits / other levels.

**Frame-1 residual is NOT brick collision.** Pixel classification of the
frame-1 diff (both LAFFC and `brick_collision` paths) is dominated by
**white↔magenta swaps (123 px)** = the *ball at a different position*,
not a destroyed/colour-changed brick. The ball seeds at y=$4E (78),
already inside brick row 5 (y 72-80); on frame 1 it moves up ~3 px and a
sub-pixel first-step difference flips whether it registers a collision
that frame, snapping it to a cell edge on one side only. So the next
lever is the **frame-1 ball move / collide-vs-not edge**, shared by both
paths — not more LAFFC bounce logic. Investigate: does the original
collide on frame 1 at this seed, and at what exact post-move y? Compare
the original's ball `IX+$02/$04` at frame 1 (ZRCP probe) against the
port's.

### Update 4 (2026-06-04): dir_to_dxdy X/Y were crossed (LAD69)

Probed the original ball through the reliable tool
(`capture_frame_timeline_original.py --probe-ball 0x9AD0`): from the seed
`(x=108,y=78,dir=$1F)` the Spectrum's frame-1 ball is **(x=105,y=65,
dir=$21)** — it moved **dx=-3** and bounced, up-snapping y to `Hy-h=65`.
The port had `dx=+0.28`. Root cause: **`LAD69` crosses the components** —
it `PUSH HL`, runs `LAD13` (multiply-by-speed) on **BC** and adds that to
**X**, then `POP HL` and adds `HL*speed` to **Y**. `dir_to_dxdy` assigned
them straight (`out_dx = hl`), so X got the wrong magnitude. Fixed to
`out_dx = bc*speed`, `out_dy = hl*speed`; for dir $1F that yields
`dx = -L*speed ≈ -3`, matching the probe.

Verified: death-spark motion test still PASS (also uses `dir_to_dxdy`),
static 5/5. Gate impact is real but modest — LAFFC frame 1 220→**188**,
later frames within noise; the swap is one factor, the collision
cell/straddle still dominate the residual. Next: read the port's frame-1
ball the same way to confirm a full (x,y,dir) match after the swap.

### Update 5 (2026-06-04): ball fully matches; residual is now the brick cell

Read the port's frame-1 ball via `PROBE.TXT` (`object_ball_1`): after the
swap it is **(x=105,y=65,dir=$21)** — the *pixel* position and direction
match the original exactly. The only gap was the q8.8 **fraction** (port
xf=0/yf=0 vs original xf=9/yf=72): the snap zeroed it, but LAFFC_26-29
write only the pixel byte and leave the fraction from the move. Fixed
`step_ball`'s `hit==3` branch to keep the moved low byte
(`next_x_q8 = (BALL_X<<8) | (next_x_q8 & 0xFF)`); port now reads xf=9,
yf=72 — a byte-exact ball match.

**Gate (LAFFC, frames 0/1/3/5): 0 / 188 / 188 / 339** vs `brick_collision`
0 / 212 / 333 / 494 — the LAFFC path now beats the approximation on
*every* frame, and the ball is exact.

### Update 6 (2026-06-04): MILESTONE — collision byte-exact; residual is the hit shimmer

Per-cell analysis of the frame-1 residual: **exactly one cell differs**,
col6 row5 — the cell the ball hits — and it differs with *mixed* pixels
(both port-brick/orig-empty and port-empty/orig-brick). If the two sides
had destroyed *different* cells there would be two differing cells; one
cell with mixed pixels means they hit the **same** cell and render its
post-hit state differently. Dumping the cell's 16×8 pixels confirms it:
**both sides draw a cyan metal-brick shimmer there, but at different
animation frames.**

So the LAFFC collision is now **byte-exact** — ball position (x,y,
fraction), direction, hit cell, and bounce axis all match the Spectrum
(verified by `--probe-ball` + `PROBE.TXT`). The "two-cell straddle" port
is NOT needed for this trajectory (same cell hit). The only residual is
the **brick-hit shimmer animation**, and it is **shared by both
collision paths** (both go through `brick_hit_anim_spawn` via
`brick_hit_resolve`), so it is orthogonal to the LAFFC work.

**Root of the shimmer mismatch.** The original per-hit shimmer
`metal_brik_anim` (`$B6A9`) reads a **sliding 16-byte window into the
`anim_brik` buffer**, advanced **2 bytes per frame** by a per-slot
counter at `briks_data IY+$00` that increments (`INC A; AND $0F`) every
frame (`fill_briks_data` calls it per active slot). The port's
the brick-hit anim instead picks **discrete frames** from
`brik_anim_sprites[]` via `brik_anim_order = {1,5,2,6,3,4,4,0}` (that
order is the *level-entry reveal* `play_brik_anim` sequence, not the
per-hit one) indexed by `(tick-1)>>1`. Different sprite mechanism →
different shimmer pattern.

**Next:** port `metal_brik_anim`'s sliding-window-into-`anim_brik`,
per-slot frame counter (increment each frame, slot freed when the cell's
bit7 sets) to `brick_hit_anim`'s render, replacing the
`brik_anim_order`/discrete-frame approximation. That should drive the
LAFFC gate residual toward 0 (frames 3/5 too — they are the same shimmer
on later-hit bricks), after which the default can flip from
`brick_collision` to the byte-exact LAFFC path. The shimmer fix benefits
the default path as well.

**Gate command.** `make capture-timeline-both LAFFC_FLAG=1` measures the
byte-exact LAFFC path (frame 0/1/3/5 ROI ≈ 0/188/188/~350, residual =
the brick-hit shimmer); omit `LAFFC_FLAG` to measure the shipping
`brick_collision` (≈ 0/212/333/494). frame 0 is 0 on both — the aligned
byte-identical start.

**Shimmer investigation status.** Timing analysis says the port's
per-hit sequence already matches `metal_brik_anim` (same sprites
brik_2,6,3,7,4,5,5,1; counter starts at the hit frame, two frames per
sprite, render-then-increment). Yet the frame-1 cell pixels still differ
(port shows a more-intact cyan brick, original shows the shimmer
erase/redraw — e.g. row y79 port `85555555DDDDDDDD` vs orig
`0000000088888888`). So the gap is most likely in the shimmer's **sprite
data, screen position, or attr handling**, not the frame timing — the
next thing to pin down (compare `brik_anim_sprites[1]` / the render
address `0x401+row*0x100+col*2` against `metal_brik_anim`'s
`screen_addr_calc`/`scr_buff_addr_calc` slot addresses).

### Update 7 (2026-06-04): LAFFC path validated over 40 frames

`make gate-laffc-long` frame-steps both runners for 40 frames on the L3
seed. The ROI residual stays **bounded** — frames 0/1/5/10/20/40 ≈
0/188/339/386/530/444 px (~2% of the ROI), never exploding into the
thousands that a lost or diverged ball would produce. So the byte-exact
LAFFC collision keeps the ball in lockstep with the Spectrum for 40
frames; the only accumulating diff is the brick-hit shimmer on each cell
the ball strikes (and it even shrinks as earlier shimmers finish).

**Flip readiness.** The LAFFC path is byte-exact and stable on the L3
single-ball trajectory and beats `brick_collision` on every measured
frame. Before flipping the default it should be validated on the cases
this gate does not cover: other level layouts (different brick
neighbourhoods exercise the two-cell straddle / fully-enclosed fallback),
multi-ball, undestructible (bit5) and metal/multi-hit bricks, and SMASH
(big-ball plough-through). Those need original-side snapshots per
scenario (only L3 `20260513T202101Z.sna` exists today) or a second
seeded trajectory. Until then the default stays `brick_collision`; the
byte-exact path ships behind `BATTY_LAFFC=1`.

### Update 8 (2026-06-04): byte-exact ball locked in by a headless regression

`make test-laffc-ball-frame1` (`scripts/test_laffc_ball_frame1.py`) is a
ZEsarUX-free regression: it steps the L3-seeded LAFFC port one frame from
the aligned `$BA83` entry and asserts `object_ball_1` (from `PROBE.TXT`)
equals the Spectrum's probed frame-1 ball **x=0x69 xf=0x09 y=0x41
yf=0x48 dir=0x21**. That single assertion covers the whole exact-motion
chain — `dir_to_dxdy` (LAD69 X/Y cross), the q8.8 fraction, the LAFFC
up-bounce cell/axis, `change_direction`, and the fraction-preserving
cell-edge snap — so any regression flips a byte and fails. PASS as of
this commit. Fast guard; the full frame-step gate (`gate-laffc-long`)
remains the visual/long-horizon check.

### Update 9 (2026-06-04): frame-1 residual is the damaged-brick render, not the shimmer

Probed the original `briks_data` ($B6F4, the per-hit shimmer slots) across
frames via the reliable tool: slot 0 is **empty at the frame-1 capture
and only populates at frame 2**, even though the ball is already bounced
(dir 0x21, y=65) at frame 1. The frame-step captures sit at the `$BA83`
loop top, so the `briks_data` shimmer animation shows up one capture after
the bounce. Therefore the cyan difference at the frame-1 hit cell is **not
the `briks_data`/`metal_brik_anim` shimmer** — it is the **damaged
multi-hit brick render** (the bit-4-set "this brick has been hit"
appearance, the README's "damage dim"), a separate sub-system from the
shimmer animation. So the earlier shimmer-sprite hunt was aimed one layer
off: the remaining residual is the **bit-4 damaged-brick pixels/attr**
diverging between `print_one_brik_buf` (port) and `print_briks` (original)
for a just-hit brick, plus, one frame later, the `briks_data` shimmer.

Ball motion + collision stay byte-exact (locked by
`test-laffc-ball-frame1`). The remaining gate residual is purely these
brick *render* details (damaged-brick frame, then shimmer) — cosmetic,
shared by both collision paths, and the precise next target if/when the
shimmer/damage rendering is taken on.

### Update 10 (2026-06-04): brick_collision fallback de-risks the flip

`step_ball`'s LAFFC branch now falls back to `brick_collision` when
`laffc_collision` reports **no hit** (returns 0): LAFFC-exact bounce
where it fires, the proven `brick_collision` as a floor otherwise. So the
byte-exact path can **never pass a brick through** that LAFFC failed to
resolve (e.g. an unported two-cell straddle on a non-L3 layout) — the
worst case degrades to today's shipping behaviour, not a regression. On
L3 LAFFC handles the hit, so the fallback never triggers and parity is
unchanged (`test-laffc-ball-frame1` still PASS, static 5/5).

**Updated flip risk.** With the fallback, flipping the default to LAFFC
removes the *pass-through* risk entirely; the only residual risk is a
*wrong* LAFFC bounce (returns 3 with a slightly-off dir/snap) on a layout
that exercises an unported edge case. That is bounded and still wants
per-level original snapshots or a port-side multi-level sanity sweep
(ball stays in play, bricks decrease) before flipping. Until then the
default stays `brick_collision`; the fallback is in place for the flip.

### Update 11 (2026-06-04): sanity sweep flagged L1 — but it was a false positive

First pass: `make test-laffc-levels-sane` compared bricks destroyed
under LAFFC vs `brick_collision` and flagged L1 (LAFFC 0 vs
brick_collision 10) as a bug.

### Update 12 (2026-06-04): CORRECTION — L1 LAFFC trajectory is physically sane

Traced the L1 LAFFC ball frame-by-frame (per-frame `PROBE.TXT`):
`f195 (134,160,dir34)` launch → `f210 (145,133)` → `f225 (157,106)` →
`f240 (168,80,dir0C)` → `f255 (179,107,dir0C)`. The ball rises into the
bricks, **bounces correctly off a brick at y=80** (dir `0x34`→`0x0C` is
exactly `change_direction(0x34,$3F)`, a vertical flip for a hit-from-
below), then falls. With a **static bat** (the sweep gives no input) the
ball simply isn't aimed, so it falls past and loses a life — *trajectory
luck, not a bug*. `brick_collision`'s ball happens to survive longer, but
`brick_collision` is itself only an approximation, **not** ground truth;
LAFFC is the byte-exact path, so its (different) trajectory is if
anything *more* likely correct.

So the Update-11 "L1 bug" was a **flawed heuristic**: brick-count
divergence from `brick_collision` under a static bat is expected, not a
defect. The sweep was rewritten as a **liveness smoke test** (FAIL only
on crash/hang; brick counts are INFO) and L1 now reports LIVE/PASS.

**Where this actually leaves the flip:** no L1 bug is demonstrated, but
no *correctness* is demonstrated either — a `brick_collision` comparison
can't prove parity (it's approximate). Validating LAFFC on a non-L3
level still requires an **original-side snapshot + frame-step gate** for
that level (as L3 has). Until such a reference exists, the default stays
`brick_collision`; LAFFC remains byte-exact-on-L3 behind `BATTY_LAFFC=1`
with the pass-through fallback. `test-laffc-ball-frame1` (L3) still PASS.

### Update 13 (2026-06-04): collision diverges by L3 frame 5 — side bounce missed

Probed the port (LAFFC) vs original ball object across L3 frames:

| frame | original (x,y,dir) | port LAFFC (x,y,dir) |
|------:|--------------------|----------------------|
| 1     | 105,65,**0x21**    | 105,65,**0x21** (byte-exact) |
| 5     | 112,64,**0x21**    | 112,64,**0x3F** |
| 10    | 107,62,0x3F        | 124,63,0x00 |
| 20    | 136,59,0x3F        | 171,63,0x1F |
| 40    | 113,54,0x3F        | 172,63,0x1F |

They are byte-exact at frame 1 but **diverge at frame 5**: same position
(112,64) but the original has done a **horizontal bounce** (dir 0x3F →
0x21 is `change_direction(0x3F,$1F)`, a B=$1F left/right flip) while the
port stayed 0x3F. So the port **missed a side (left/right) brick
collision** the original made around f4-5; from there the trajectories
diverge completely.

This corrects two overstatements: (a) the collision is **not** byte-exact
past frame 1 even on L3, and (b) the "40-frame lockstep" of
`gate-laffc-long` was a *pixel* residual staying bounded — it did **not**
mean the ball matched (at f5 the position matched so pixels matched even
though the direction had diverged; by f10+ the ball is visibly apart but
the bounded ROI/shimmer pixel count understated it).

LAFFC's **vertical** bounce (hit from below/above) is exact — that's the
frame-1 case. The **side** bounce (ball moving left/right into a brick
face) is missed or mis-resolved. Likely causes in `laffc_collision`: the
cell-finder picks the ball's own/right-straddle cell only (no left
straddle), and/or the open-face mask + direction gate don't select the
horizontal face when the ball enters a brick from the side. **Next fix:**
reproduce the f4-5 state (ball ~(110,64) moving right, dir 0x3F), trace
which cell/mask `laffc_collision` computes, and make it bounce horizontal
like LAFFC_27 (`change_direction(_,$1F)`, snap X to the cell edge). Then
extend `test-laffc-ball-frame1` to also assert frames 5/10 against the
probe table above so the side-bounce case is gated, not just frame 1.

### Update 14 (2026-06-04): pinpointed — missing VERTICAL straddle (LAFFC_5-6)

Instrumented `laffc_collision` (debug dumped to `PROBE.TXT` as
`laffc_dbg=`). At the L3 f4/f5 divergence the port reports:
`newx=110 row=3 col=6 ... exit=2` (exit 2 = straddle found no brick).

Root cause, with grids confirmed byte-identical port-vs-original through
f2 (so NOT a destruction cascade):

- The byte-faithful row-finder assigns the ball at **y=64** to **row 3**
  (`Hy=56`, spanning y56-64) — correct per LAFFC_0-2, since y64 is row
  3's bottom edge.
- The `0x13` brick the ball is entering is at **(4,7)** — row 4, the row
  the ball *body* (y64-70) penetrates.
- The port's cell-finder, when `(3,6)` is empty, only straddles **right**
  to `(3,7)` (also empty) → returns no-hit (exit 2) → falls back to
  `brick_collision`, which also misses → no bounce. The ball sails on
  (dir stays 0x3F) while the original bounces (dir 0x21).

The original's phase-4 head (**LAFFC_5-6**) does more than a right
straddle: when the ball's body penetrates the next row by >= 8 px
(`Y + height - C >= 8`, true here: 64+7-56 = 15) and `H < $78`, it steps
**down** a row (`H += 8`, `IY += $0F`) to find the solid cell the body
overlaps. That vertical straddle is what the port omits.

**Fix (next):** port LAFFC_5-6 faithfully — when the ball's reference
cell is empty, straddle horizontally (by body width vs `$E8`) AND
vertically (down a row when `new_y + h - Hy >= 8`), landing `IY`/(row,
col) on the solid cell the body actually overlaps, before building the
mask. Then re-run the f1/f5/f10 ball probes against the original table in
Update 13 until they match, and extend `test-laffc-ball-frame1` to gate
frames 5/10. The `laffc_dbg=` PROBE line stays as the debugging aid.

### Update 15 (2026-06-04): FIXED — ported LAFFC_5-6 straddle; byte-exact f1-f40

Ported the full phase-4 head (`LAFFC_5-6`) into `laffc_collision`: when
the ball's reference cell is empty, land on the solid cell the body
overlaps by trying own → right → down → down-right, with the straddle
conditions `rem + width >= 16` (crosses into the next column, not at
`$E8`) and `new_y + h - Hy >= 8` (penetrates the next row, not at `$78`).
The earlier right-only straddle missed the down/down-right brick at a row
boundary (Update 14).

**Result:** the port ball is now **byte-exact vs the Spectrum at frames
1, 5, 10, 20, and 40** (x / x-frac / y / y-frac / dir all match) — the
full 40-frame L3 trajectory, dozens of bounces, including the
horizontal/side bounce that diverged before. The frame-step gate's
remaining residual (~160-220 px) is purely the cosmetic brick-hit
shimmer render (the ball object itself matches exactly; verified by
`object_ball_1` probes, not just pixels). Locked in by the extended
`make test-laffc-ball-frame1` (now checks frames 1/5/10/40) and static
5/5. So **the brick collision is byte-exact on L3's full trajectory**,
correcting Update 13's "diverges at frame 5".

### Update 16 (2026-06-04): LAFFC is now the DEFAULT (primary ball)

With the ball byte-exact vs the Spectrum over L3's **150-frame**
trajectory (frames 1/5/10/20/40/60/80/100/150 all match, dozens of
bounces and cell configs) and the brick_collision pass-through fallback
in place, `use_laffc` now **defaults to 1** for the primary ball.
`BATTY_LEGACY_COLLISION=1` reverts to the old `brick_collision` path.
Multi-ball secondaries still use `brick_collision` (not yet validated on
the LAFFC path). Static regression 5/5 + lints (entry frames don't
exercise collision); `test-laffc-ball-frame1` extended to gate frames
1/5/40/100/150. The shipping game's primary-ball brick collision is now
byte-exact with the Spectrum on the validated trajectory; the only
frame-by-frame residual left is the cosmetic brick-hit shimmer render.

### Update 17 (2026-06-04): the residual is the periodic metal shimmer, definitively

Pinned down the sole remaining frame-by-frame residual (~185 px at the
central brick cluster, e.g. cells col6/7 row5/6):

- It is **not the collision** — the ball object is byte-exact f1..f150
  and the port/original brick grids are byte-identical (same cells
  destroyed at the same frames).
- It is **not a destruction flash** — `render_brick_flash_to_buff` is a
  no-op in the port (it draws nothing), so destroyed bricks are removed
  cleanly on both sides.
- The differing pixels are **cyan (palette 5/D)** while those bricks are
  white/magenta at rest — i.e. a shimmer overlay. Since 0x13 bricks
  destroy on first hit (no hit-shimmer) and the grids agree, this is the
  **periodic metal-brick shimmer** (`all_metal_briks` / `metal_brik_anim`,
  the cyan animation the original plays on certain bricks every few
  frames during gameplay), running with an RNG/phase the port does not
  replicate. This is the same cosmetic shimmer flagged at the start of
  the parity work, now isolated from the (solved) collision.

Conclusion: **gameplay parity (ball motion + brick collision) is
byte-exact and shipping by default.** The only frame-by-frame visual
residual is this cosmetic periodic metal shimmer — orthogonal to
gameplay, RNG/phase-driven, and the same on both collision paths. The
authoritative parity gate is the object-level `make
test-laffc-ball-frame1` (green, byte-exact); the pixel gate carries the
cosmetic shimmer. Closing the shimmer would need porting `all_metal_briks`
with the original's exact per-frame RNG/phase — a self-contained cosmetic
task, not gameplay parity.

### Update 18 (2026-06-04): residual is cosmetic shimmer BOTH sides render — out of scope

Side-by-side pixels at cell (6,5), frame 5:

```
       GT (static)        PORT f5            ORIG f5
 y72:  FFFFFFFFFFFFFFFB | 5555888888888888 | 555555558DDDDDD8
 ...   (white/magenta)  | (cyan shimmer A) | (cyan shimmer B)
```

The static GT renders the brick **white/magenta**; **both** the port and
the original render a **cyan metal shimmer** there during gameplay, at
different animation phases. So: (a) the shimmer is a periodic cosmetic
effect both sides play; (b) the static GT does **not** capture it (it was
grabbed with the metal-brick animation NOP'd), so there is no "ground
truth still frame" for the in-motion shimmer to match; (c) the port and
original only differ in the shimmer's per-frame phase.

**Conclusion / scope decision:** the gameplay-parity goal (ball motion +
brick collision) is **met and byte-exact** — authoritatively gated by
`make test-laffc-ball-frame1` (object-level, green). The remaining
frame-step *pixel* residual is exclusively this periodic metal-brick
shimmer, which is cosmetic, RNG/phase-driven, rendered by both sides, and
not represented in the GT. Matching its exact phase would mean porting
`all_metal_briks`'s per-frame RNG/phase bit-for-bit — a self-contained
cosmetic task with no gameplay effect. **Treating it as out of scope for
the gameplay frame-parity goal**; the pixel gate's central-cluster
residual is expected and is not a collision regression (the object gate
catches those).

### Update 19 (2026-06-04): precise shimmer spec — port lacks the per-frame metal animation

Pixel comparison of cell (6,5) over frames:

- **Original animates** it: f1 `...8888DDD8 / 88888DD8 ...` != f5
  `...8DDDDDD8 / D8DDDDD8 ...` — the metal shimmer steps each frame.
- **Port is static**: f1 == f5 (byte-identical) — it shows one fixed
  cyan frame and never advances.

`briks_colors[3] = 0x5F` (bright white-ink / magenta-paper) is the
brick's at-rest colour (what the static GT shows). During gameplay the
original runs `all_metal_briks_frame` **every frame** to animate the
metal shimmer on these bricks; the port only plays the entry reveal
(`play_brik_anim`) and the per-hit shimmer (`brick_hit_anim`), so a
shimmered brick is left on a **static reveal-leftover frame** with no
per-frame driver.

**Exact spec to close the cosmetic residual (optional, no gameplay
effect):** add a per-frame `all_metal_briks_frame` equivalent — advance
the `anim_brik` shimmer on the shimmering bricks (the original animates
low-nibble-3 cells like (6,5), so the set is broader than nibble>=6) by
a global frame counter, matching the original's `$AD8F` phase. To reach
0 px it must match the original's exact per-frame phase/RNG; otherwise it
trades a static residual for a phase-offset one. This remains out of
scope for the gameplay frame-parity goal (byte-exact motion + collision,
already shipping); it is purely the metal-brick light animation.

### Update 20 (2026-06-04): correction — gameplay shimmer driver is NOT all_metal_briks

U19's spec ("add a per-frame `all_metal_briks_frame`") is **wrong**:
`all_metal_briks_animation_snd` is called once in the round-intro
sequence (disasm line 6148: after `show_window_round_number` /
`pause_long`), i.e. it is the **entry reveal** (`$BA6C`, which the
`l3-brick-flash` setup NOPs) — NOT a per-frame gameplay driver. And
`metal_brik_anim`/`briks_data` frees its slot the moment a brick's bit7
sets, so it cannot animate the destroy-on-first-hit 0x13 cell at (6,5)
during gameplay either.

So the original's per-frame cyan animation at (6,5) is explained by
**neither** known shimmer routine. Its driver is unidentified after
extensive RE (14+ passes). Closing this last cosmetic pixel residual
would require deeper runtime single-stepping of the original's gameplay
brick-render path to find what re-animates these cells — beyond what the
static disasm + frame probes have revealed. **Definitively out of scope**
for the gameplay frame-parity goal (byte-exact motion + collision is
done and shipping); logged here so the next person doesn't chase the
wrong (entry-only) routine.

### Update 21 (2026-06-04): bat-deflection validation — scenario built, tool obstacle found

Good news: validation scenarios can be **constructed from the existing
L3 snapshot** by re-poking the ball — no new snapshot needed. Built
`replays/l3-bat-bounce.json` (the l3-brick-flash setup with the $9AD0
ball poked to x=0x80, y=0xA0, dir=0x0F, dropping it onto the bat at
x=0x74/y=0xAD).

Obstacle: the frame-step harness pins the frame boundary at `$BA83`
(main-loop top), but with a ball-onto-bat seed the original leaves that
loop early — `frame_step` lands at `$ABE1` after one step instead of
`$BA83`. The brick-bounce scenario frame-steps cleanly; the bat path
takes a different route (the "ball near/!on bat" handling around
handling_bat / the running_dot / catch logic), so `$BA83` is not hit
every frame. So validating the bat deflection needs the original-side
tool taught an alternate/robust per-frame boundary for the bat path
(e.g. break on the frame-interrupt vector `$0038` and count IM1s, which
fires once per frame regardless of the game-state branch), then probe
the post-bounce ball and compare to the port's 5-zone result.

So: the bat deflection IS reachable for validation from the existing
snapshot (scenario built), but the frame-step tool needs that boundary
fix first. Scoped here; `replays/l3-bat-bounce.json` is the reusable
scenario.

### Update 22 (2026-06-04): bat-bounce scenario hangs the original — needs full object state

Tried frame-stepping `l3-bat-bounce.json` with both `$BA83` and the IM1
vector `$0038` as the boundary. Both fail: the run executes ~5M opcodes
ending around `$ABDx` *without* `$0038` firing — i.e. the original is hung
in a tight interrupt-disabled loop, not even reaching a frame HALT. So
poking only the ball's x/y/dir ($9AD0+2/+4/+6) and leaving the other 19
bytes from the brick-flash seed produces an **inconsistent ball-object
state that hangs the game** (the L3-brick-flash seed worked because its
full 22 bytes were a coherent captured state).

So validating the bat deflection by constructing a ball-onto-bat state is
**not** just a 3-byte poke: it needs a coherent full ball descriptor (buf
addresses, prev_x/y, the +08..+13 fields) consistent with the new
position, or — more reliably — capturing an actual original snapshot at a
ball-near-bat moment. That puts bat-deflection validation back in the
"needs a real captured reference" bucket, same as multi-ball and non-L3.

Net: the gameplay frame-parity goal (byte-exact ball motion + brick
collision) remains delivered and shipping; every *further* parity item
(bat deflection, secondaries, non-L3) is blocked on capturing real
original reference states — constructed-from-poke scenarios hang unless
the full object state is coherent.

### Update 23 (2026-06-04): robust frame-step boundary ($0038) verified — pipeline foundation

Toward validating the bat deflection (and other states), the frame-step
harness needs a per-frame boundary that survives the bat-interaction
control flow (the `$BA83` main-loop-top boundary is skipped when the
game branches into the bat/ball-lost paths). Verified that the IM1
maskable-interrupt vector **`$0038`** frame-steps the *coherent*
l3-brick-flash state cleanly (frames 1/5/10, ball progresses, no hang) —
it fires once per 50 Hz frame regardless of which game-state branch is
running. (The earlier `$0038` failure was on the incoherent 3-byte poke,
which hangs the game outright — not a boundary problem.)

So `capture_frame_timeline_original.py --frame-pc 0x0038` is the **robust
boundary** for capturing through control-flow branches. Caveats / next
steps for the capture pipeline:

- **Phase alignment.** `$0038` samples at interrupt time, a different
  point in the frame than `$BA83` (loop top) — the port's visual-probe
  halt is at the main-loop boundary, so port-vs-original byte comparison
  needs both sides sampled at the same phase. Either capture the port at
  the matching phase, or compare at `$BA83` for the brick path and only
  switch to `$0038` for the bat path (then align the port accordingly).
- **Reaching a bat contact.** With a static bat the seeded ball never
  contacts it (it falls past → ball-lost). A bat contact requires driving
  the bat under the descending ball via ZRCP `send_key_event` during
  coherent play (timed to the probed ball trajectory). That input
  orchestration is the next pipeline piece, now unblocked by the robust
  `$0038` boundary.

### Update 24 (2026-06-04): no bat-contact trajectory from the existing seed

With the robust `$0038` boundary working, frame-stepped the coherent
l3-brick-flash ball over 240 frames and tracked it: y stays in the
**upper field** (y ≈ 10–56, trending up; x sweeps 14–206) — it bounces
among the top/metal bricks and **never descends to the bat** (y≈173).
So this seed provides no bat-contact to validate the bat deflection
against, and (Update 22) a poked descending-ball state hangs the game.

Therefore validating/porting the exact bat deflection requires a **real
captured snapshot taken during actual gameplay at a ball-descending-onto-
bat moment** — which means driving the original through menus + play
(tape boot or the L3 snapshot + scripted bat/launch input) to that state
and dumping RAM, then aligning a port seed. That gameplay-capture
pipeline is large and fragile (menu/play automation via ZRCP input), and
is the real remaining cost for the bat deflection — the robust `$0038`
boundary (Update 23) is necessary but not sufficient without a usable
descending trajectory to apply it to.

Net: the bat-deflection avenue is, concretely and after multiple
attempts, blocked behind a real gameplay-capture effort — same wall as
multi-ball and non-L3. The shipped byte-exact-on-L3 gameplay parity
(motion + brick collision) stands as the delivered milestone.

### Update 25 (2026-06-04): launch deflection VALIDATED via the round-init state

The `l3-entry` setup (round-init, no ball poke) leaves a **coherent**
stuck ball on the bat — usable, unlike the hung bat-bounce poke (U22).
Used it to validate a bat-related deflection: the level-entry
**auto-launch direction**.

- Original (l3-entry, frame ~195, past the 192-frame auto-launch): stuck
  ball at x=128 (offset 12 on the bat at x=116), launches **dir 0x34**.
- Port (L3 entry, default stuck ball, auto-launch): launches **dir 0x34**.

So the launch deflection **matches the Spectrum** — another bat-related
behaviour confirmed byte-correct, joining motion + brick collision.

Open sub-finding: the port's stuck-ball offset is 16 (x=132) vs the
original's 12 (x=128) — a 4 px difference, with the original settling
132→128 in frame 1 while the port stays at 132. The auto-launch dir is
the same (0x34) for both offsets, and the static state4 test passes (the
port matches the GT's rendered stuck position), so this is likely a
frame-phase sampling difference ($BA83 loop-top vs the port's visual-
probe phase) rather than a real position gap — confirm with a same-phase
capture before treating it as a bug. Method note: usable validation
states CAN be reached from the round-init (`l3-entry`) without a new
snapshot; the hang (U22) was specific to incoherent hand-pokes.

### Update 26 (2026-06-04): stuck-offset thread closed (tangled with GT, gameplay OK)

Followed up the U25 stuck-offset sub-finding (port offset 16 vs original
12). The port uses offset 16 + a `-4` in its launch formula, which lands
on the same launch dir (0x34) as the original's offset 12 — so the
gameplay-relevant launch is correct. A "clean" fix (offset 12 + drop the
-4, porting LA27E_15's exact mapping) is possible in principle, BUT the
L3 GT has **no white ball at the stuck position** (scanned y150-172) —
so the port/GT/live-original offset relationship is tangled (the GT
appears captured with the ball absent/elsewhere), and `state4`'s ~49 px
residual can't be cleanly attributed. Not worth re-baselining the GT and
risking the static gate for a 4 px pre-launch visual whose launch outcome
already matches. Thread closed.

Validated-parity surface to date: ball **motion** (q8.8, byte-exact 150
frames), **brick collision** (LAFFC, byte-exact, default), **launch**
direction (matches). Remaining (bat-in-flight bounce, multi-ball
secondaries, non-L3) all need the original driven into a descending-ball
state — the input-orchestration capture pipeline — which the available
coherent seeds don't reach (the ball stays in the upper field).

### Update 27 (2026-06-04): SMASH plough-through is reachable + coherent (validatable)

Poking the bat's applied-bonus flag `object_bat_1+$14 = $07` (SMASH /
BIG_BALL plough) is coherent — the original does NOT hang (unlike the
ball-position poke, U22) and the ball **ploughs straight through the
bricks**: dir stays `0x1F`, x marches 105→99→93→87→81 with no bounce
(vs the non-SMASH ball, which bounced to dir 0x21 on frame 1). Scenario
saved as `replays/l3-smash.json`.

So bat-flag pokes ARE a usable way to reach more validation states. To
validate the port's SMASH collision against this, force the port into
plough mode and compare the ball trajectory + destroyed cells. Nuance to
resolve first: the port gates plough on `big_ball_active()`, which also
**enlarges** the ball (`eff_ball_size`), whereas the original's `$07`
plough flag is set without necessarily changing the ball size — so the
port may couple size+plough where the original separates them. A clean
SMASH validation needs a port "force plough, normal size" hook (or
confirming the original's `$07` also enlarges). Logged as a reachable,
niche next validation; the core gameplay parity (motion + collision +
launch) stands verified.

### Update 28 (2026-06-17): boundary-face mask was INVERTED (known-bugs #6)

The phase-4 open-face mask had its four edge conditions negated vs the
disasm — the root cause of "ball teleported through red bricks". The
original (LAFFC, ~$B019) starts `D=$0F` (all four faces OPEN) and RESes a
bit (closes that face) only when the neighbour is SOLID *and* the cell is
not against that playfield boundary:

```
right (bit1): CP $E8; JR Z   -> skip RES if Lx==$E8 (right wall)
left  (bit0): CP $08; JR Z   -> skip RES if Lx==$08 (left wall)
up    (bit2): CP $21; JR C   -> skip RES if Hy <$21 (top row, Hy=$20)
down  (bit3): CP $78; JR NC  -> skip RES if Hy>=$78 (bottom row)
```

So a brick AGAINST a boundary keeps that boundary face OPEN — a ball can
bounce off it. The port had written the SET form as `Lx!=$08 && EMPTY`,
`Hy>=$21 && EMPTY`, `Hy<$78 && EMPTY`, `Lx!=$E8 && EMPTY` — which makes the
boundary face CLOSED. For INTERIOR cells the edge term is false, so both
forms reduce to `EMPTY(neighbour)` and behaviour is identical (this is why
L3's interior-ball gate stayed byte-exact and the bug hid for the whole
campaign). At a boundary they diverge.

Manifestation: a ball moving straight down (dir $10) into a **row-0**
(`Hy=$20`) metal brick whose right neighbour is empty. The up-face bit
(bit2) never set (`Hy>=$21` false), the dir-gate clears the down bit, and
the empty right neighbour sets the right bit — which survives. Phase 6
then does a *horizontal* bounce: `dir = ($10 ^ $1F)+1 & $3F = $10`,
unchanged (flipping the dx component of a pure-vertical dir is a no-op),
ball nudged sideways and snapped to keep falling → it passes through. Metal
bricks expose it (can't be destroyed); destructible bricks broke regardless
of the (wrong) bounce axis, masking it. `laffc_collision` returns 3
(handled) so the `brick_collision` fallback never runs.

Fix: the four mask conditions now match the disasm:
```
if (Lx == 0x08 || EMPTY(row, col-1)) mask |= 1;   /* left  */
if (Lx == 0xE8 || EMPTY(row, col+1)) mask |= 2;   /* right */
if (Hy <  0x21 || EMPTY(row-1, col)) mask |= 4;   /* up    */
if (Hy >= 0x78 || EMPTY(row+1, col)) mask |= 8;   /* down  */
```
Now the row-0 brick's top face is open, the ball reflects up
(`dir ($10^$3F)+1 = $30`). Gate: `make test-ball-no-tunnel` (found it on
L5/L7 row-0 metal; the default subset pins those). L3 byte-exact ball
parity unaffected (test-laffc-ball-frame1 green at f1/5/40/100/150).
