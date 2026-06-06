# Wall bounce — the side-wall "stuck ball" regression (fixed 2026-06-06)

## Symptom (user-reported)

Balls juggle/pin against the left and right borders — they reach the wall
and bounce in place ("not moving much") instead of reflecting back across
the playfield. Long-standing; survived because nothing gated wall bounces
(the byte-exact L3 ball gate is brick-collision-dominated and never reaches
a bare side wall; `make test` only checks static screens).

## Root cause

`reflect_obj_dir` (the shared ball wall-reflect) had the X/Y reflect masks
**swapped and off by one**:

```c
if (flip_x) dir = (0x3F - dir) & 0x3F;   /* WRONG for a side wall */
if (flip_y) dir = (0x1F - dir) & 0x3F;   /* WRONG for the top wall */
```

The direction is a 6-bit angle (0=E, 16=S, 32=W, 48=N, clockwise; +y down).
A correct horizontal reflect (side wall, negate dx) is `((dir ^ 0x1F)+1) &
0x3F`; a correct vertical reflect (top, negate dy) is `((dir ^ 0x3F)+1) &
0x3F`. The buggy `0x3F-dir` is the *vertical* mask minus one, and was used
for the *side* wall — so hitting a side wall negated **dy** and left **dx**
pointing into the wall:

```
ball dir 0x20 = (-255, 0)  (pure left)  hits left wall
  buggy: 0x3F - 0x20 = 0x1F = (-253, +24)  -> still moving LEFT
  next frame: clamped to x=8 again, 0x3F-0x1F=0x20 -> still LEFT
  => oscillates 0x1F<->0x20, pinned at x=8, dy juggling. STUCK.
```

## The original (ground truth)

`bounce_wall` ($AC75) calls `change_direction` ($AC40) with explicit masks:

```
bounce_wall:
  LD B,$3F ; check_top_margin   -> change_direction(dir, $3F)   ; TOP  (negate dy)
  LD B,$1F ; check_left_margin  -> change_direction(dir, $1F)   ; LEFT (negate dx)
           ; check_right_margin -> change_direction(dir, $1F)   ; RIGHT(negate dx)

change_direction:  A = ((dir XOR B) + 1) AND $3F
```

So: **side walls use mask $1F, the top wall uses $3F** — the *same*
`change_direction` LAFFC uses for brick faces (`laffc_change_dir`, which the
byte-exact brick gate already validates with $1F=horizontal, $3F=vertical).
The port's brick path was correct; only the wall path had them backwards.

## Fix

`reflect_obj_dir` now mirrors `change_direction`:

```c
if (flip_x) dir = ((dir ^ 0x1F) + 1) & 0x3F;   /* L/R wall: negate dx */
if (flip_y) dir = ((dir ^ 0x3F) + 1) & 0x3F;   /* top wall: negate dy */
```

Verified (`dir_to_dxdy` over all 64 dirs): flip_x negates dx & preserves dy
for 64/64; flip_y negates dy & preserves dx for 64/64. Both `flip_x` and
`flip_y` callers (primary ball `step_ball`, secondary balls) go through this
one function, so all wall reflects are fixed at once.

## Regression test

`make test-wall-bounce` (`scripts/test_wall_bounce.py`): seeds a ball in the
open band (y=0x90, below bricks / above bat) aimed straight at a wall at
speed 6, runs 8 frames via the `$BA83` probe harness, and asserts it
bounced clear:

```
fixed:  left  f8 x=37 (dir 0x00, rightward)   right f8 x=210 (dir 0x20, leftward)
buggy:  left  f8 x~=8  (pinned)                right f8 x~=240 (pinned)
```

Thresholds: left `x>=24`, right `x<=220`. Wired into `parity-check-full`.
Validated to PASS on the fix and FAIL on the reverted (buggy) formula.
