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
collision paths** (both go through `brick_hit_anim` via
`brick_hit_resolve`), so it is orthogonal to the LAFFC work.

**Root of the shimmer mismatch.** The original per-hit shimmer
`metal_brik_anim` (`$B6A9`) reads a **sliding 16-byte window into the
`anim_brik` buffer**, advanced **2 bytes per frame** by a per-slot
counter at `briks_data IY+$00` that increments (`INC A; AND $0F`) every
frame (`fill_briks_data` calls it per active slot). The port's
`brick_hit_anim` instead picks **discrete frames** from
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
