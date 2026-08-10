# Enemy (bird / UFO) — motion, steering, animation

All ported and gated: `test-enemy-descend`, `test-enemy-steer`,
`test-enemy-anim`, `test-enemy-brick-walk`, `test-enemy-margin-clamp`,
`test-enemy-attr-parity`, `test-enemy-flyover-redraw`,
`test-enemy-brick-residue`.

Capture is easy here, unlike the multi-ball secondaries: the `l3-brick-flash`
snapshot already carries a LIVE, coherently-moving enemy at `object_enemy`
($9B96), so `scripts/capture_enemy_flight.py` frame-steps the original and
probes it directly.

## `handling_bird`'s call sequence

    handling_bird:
      LD A,(IX+$04) / CP $08 / JR NC / INC (IX+$04) / RET   ; entry slide
      CALL bomb_appear
      ...                                                   ; LAA02 patch
      LD B,$01
      LD A,(counter_misc) / AND $03 / CALL Z,LAA7D          ; steer every 4
      CALL LAD69                                            ; q8.8 move
      CALL LAFFC                                            ; BRICK collision
      CALL check_margins                                    ; clamp only
      LD A,(IX+$04) / CP $C0 / ... / SET 7,(IX+$00)          ; die below 192

Note the ORDER: brick collision first, margins after, because LAFFC can move
the alien into a margin.

`LAD69` is the same routine the ball uses, called with the RAW `(IX+$06)`. The
port must therefore use `dir_to_dxdy`; a separate `enemy_dir_delta_q8` with its
per-quadrant switch had X and Y swapped, so `dir $10` came out `dx=$FF, dy=0` —
the alien flew right instead of descending.

The steer gates on the GLOBAL `counter_misc`, so every enemy turns on the same
4-frame phase; the port gates on `pit_frame_counter & 3`. Gating on the
per-object `misc_12` instead gives a spawn-relative phase — and `+$12` is the
SPRITE ADDRESS in the original, not a turn counter.

## Steering — `LAA7D`

    LAA7D:
      LD A,(IX+$06) / LD L,A / SUB (IX+$14) / JR Z,LAA7D_1
      BIT 5,A / LD A,B / JR NZ,LAA7D_0 / NEG
    LAA7D_0:
      ADD A,L / AND $3F / LD (IX+$06),A / RET
    LAA7D_1:
      LD A,(random_number) / AND $3F / LD (IX+$14),A / RET

Pick a random target heading; turn one 6-bit step toward it every 4 frames,
choosing the shorter arc by bit 5 of the delta; on ARRIVAL pick a new random
target, reading the current `random_number` without advancing.

**Nothing in it looks at the alien's position** — the original's alien has no
wall avoidance at all. It presses against the clamp, its dir eventually reaches
its target, the arrival re-pick sends it somewhere random, and it leaves. The
L3 curve toward the bat is the random target happening to point that way; the
enemy does not pursue anything.

`enemy_prepare` ($9EAA) sets both `dir` and target to **$10 unconditionally** —
always straight down, then steer. Spawn x is `prop_x_coord[random_number & 3]`
= {64, 168, 64, 168}, read-current.

To hold an alien on a heading in an experiment, pin the target to something it
is steering TOWARD but has not reached — never to the dir itself, which is the
one value that guarantees the arrival re-pick fires.

## `check_margins` is three clamps

    check_top_margin    y < $08              -> y = $08
    check_left_margin   x < $08              -> x = $08
    check_right_margin  (u8)(w + x) >= $F9   -> x = $F8 - w

That is the whole routine: no direction change, no re-aim. The port had given
the alien ball-style reflection plus a target re-pick, and both were inventions
— `bounce_wall` is the one that reflects, and it belongs to the ball and the
sparks.

**No gate saw the swap**, because nothing in the suite had ever watched an
alien reach a margin. `test-enemy-margin-clamp` now does, and asserts the
phase-robust form — each non-arrival turn moves dir by exactly one 6-bit step,
so `shortest_arc(dir, start) <= turns - arrivals` is exact whatever the steer
phase, and a reflection is an arc of 32.

The right clamp's `ADD A,(IX+$02)` is 8-BIT and unguarded, so for the bird's
`w = $18` the sum wraps and the clamp stops firing: x in `[$E1,$E7]` clamps
back to `$E0`, and x >= `$E8` escapes entirely, reappearing at the other side.
**Reproduced rather than fixed** — faithful, and startling to watch. Reaching
it needs a jump of more than 2 px past `$E7`, which ordinary flight cannot do;
a LAFFC snap is the only way in. See known-bugs #16.

## What LAFFC does for the BIRD

`LAFFC_30`'s tail branches on the caller, and only `sprite_set $02` (the ball)
reaches the destroy-and-score path. For `$08`/`$09` it latches the alien's own
position into `LAA7B` — the word `handling_bird` tests at its top — and then
**falls through**:

    LD L,(IX+$02) / LD H,(IX+$04) / LD (LAA7B),HL
    LB1C3:
      LD HL,$0000        ; SELF-MODIFIED at LAFFC_7 with the entry position
      LD (IX+$02),L / LD (IX+$04),H / RET

`LB1C3+$01` is written at `LAFFC_7` with the object's position at LAFFC ENTRY,
so the fallthrough means "put the object back where it started this frame".
And `LAFFC_30` is only reachable from `LAFFC_26..29`, each of which has ALREADY
snapped one axis to the cell edge and called `change_direction`.

So a bird that hits a brick gets:

1. `dir` reflected by `change_direction` — kept
2. the snapped position recorded in `LAA7B`, then **undone**
3. `flag_2` set, so the handler tail re-targets at random

and then `LAA44` (ported as `enemy_home_step` in `src/enemies.cpp`) walks it
from where it was to where the snap would have put it, one pixel per axis per
frame, with steering, movement and collision all skipped. On arrival `LAA7B`
clears and normal flight resumes. **It is the same bounce a ball gets, animated
over a few frames instead of applied at once** — the snap is at most 16 px in x
and 8 in y.

Three details of `LAA44` worth keeping: the "too far right" case is
`DEC / DEC` falling into `LAA44_1`'s `INC`, a net -1, so both directions move
one pixel; the y branches both `RET`, so the arrival test is only reached on a
frame where y was already equal; and the only clamp is a LOW one on x
(`CP $10`), written back into `LAA7B` — nothing clamps the right or y.

### The alien does NOT damage bricks — measured

Settled by capture rather than argument, because the port's `laffc_collision`
also calls `brick_hit_resolve`, and wiring that into the bird unchanged would
have aliens eating the level. `--poke-at-frame` put the alien on a solid `$06`
cell and `--probe-grid` dumped the 180-byte grid at each checkpoint: identical
over frames 10..26 while the alien drifted up through two brick rows. The
alien's POSITION was verified separately (an unchanged grid proves nothing if
the poke silently failed), and `LAA7B` read `$0000`, confirming `LAFFC` really
was called rather than the homing branch that cannot damage bricks anyway.

The port runs the detection between the move and the margin check, reuses the
ball's `laffc_sweep` and `laffc_bounce`, and then does the alien's half of
`LAFFC_30`. `brick_hit_resolve` is deliberately not called.

The whole reaction is invisible on screen — the bricks are untouched by design
and the alien ends up roughly where a bounce would have left it — so it needed
a probe word to be checkable at all. `PROBE.TXT` carries `enemy_home=<x><y>`,
and `test-enemy-brick-walk` reads it:

    frame  x    y   dir   enemy_home
      2   165   61  $10   A83D      <- latched at the snap point
      3   166   61  $10   A83D      <- walking, 1 px/frame
      4   167   61  $10   A83D
      6   168   62  $30   A834      <- arrived, cleared, flew, re-latched

with `bricks_quantity` unchanged at 25 throughout. That gate does not assert
`target`: the alien's first frame is ordinary flight and a steer there is
phase-gated, while from frame 1 on it is homing, and homing skips the steer.

## Ground truth (L3, `capture_enemy_flight.py --frames 40 --step 4`)

    f0 : x=168 y=1  dir=0x10 spd=1   <- fresh spawn, heading down
    f4 : x=168 y=5  dir=0x10         <- descend phase (y<8): y += 1/frame
    f8 : x=168 y=8  dir=0x10         <- crossed y>=8: motion via dir now
    f12: x=168 y=12 dir=0x10
    f16: x=167 y=16 dir=0x11         <- steering turns dir +1 ...
    f20: x=167 y=20 dir=0x12
    f24: x=165 y=24 dir=0x13         <- ... every 4 frames toward target
    f28: x=164 y=28 dir=0x2C         <- ARRIVAL: dir jumps to the new target
    f32: x=163 y=25 dir=0x2C         <- target is up-left; y now decreasing
    f36: x=162 y=21 dir=0x2D

Speed is **1**, not the ball's 2.

`test-enemy-descend` gates the RNG-independent half (y +1/frame, x / dir / spd
/ target held). `test-enemy-steer` gates the RNG-dependent leg at f16/f20/f24
with `dir` and `y` asserted EXACTLY and `x` within +/-1: the integer x jitters
165<->166 run to run because the boot / WAIT_KEY release phase shifts a 4-frame
turn by one frame. `dir` and `y` are phase-stable and carry the regression
signal — a wrong RNG walk flips `dir` the wrong way to 0x0E/0x0C
(`notes/rng-model.md`).

## Sprite animation, draw order, sprite encoding

All render-side, and all in `notes/bird-render-parity.md`: the `LAAD2` cadence
stepper and the bird/UFO frame tables, the `$9AD0` slot paint order, the
`gfx_inverse` boot XOR, and the "moving objects never recolour a cell"
invariant.

Two facts from there are worth knowing before reading the motion code.
`LAA02`'s facing mirror is DEAD CODE, so the bird never flips by direction.
And the `$10` rotation in that block is not motion: `handling_bird` patches
`(dir - $10) & $3F` into `LAA02+$01` before the move, recomputes it after,
`XOR`s and tests bit 5 — it is the facing TEST feeding that dead remap.
`LAD69` still gets the raw dir.
