# Gameplay parity status

Core gameplay is byte-exact against the Spectrum and regression-locked:
ball motion, `LAFFC` brick collision, `LAB1F` bat deflection, launch,
catch, multi-ball, falling objects, sparks, laser and blasts, bombs, the
bonus economy and every effect, scoring, ball speed-up and SLOW, the
per-frame RNG model, the enemy (motion, steering, bombs, every sprite
animation) and the rocket-clear.

*Read "byte-exact" as scoped to what the oracles cover.* Written once as
unqualified "PARITY COMPLETE", it was followed by manual play finding two real
bugs the suite had missed: the oracle was L3-only, so non-L3 dynamics and
sustained play were blind spots.

The byte-exact frame-step oracle covers **L3 and L5**; the
poke-`$B7EA`+`$BA24` recipe generalises it to any level. Beyond it,
oracle-free invariants cover every level: ball-never-tunnels-a-brick (all
approaches and speeds, including the extra-ball and magnet paths),
ball-never-escapes-the-field, moving-sprite attr non-corruption, and a
multi-level soak.

Two residuals are accepted as not byte-exactly achievable: the 4 px
brick-edge nuance in the L3 frame-step diff (a capture-phase offset — see
`notes/metal-shimmer.md`) and cycle-exact sound. Open items are in
`notes/parity-gaps.md`.

## The measurement everything rests on

`make capture-timeline-both` frame-steps the DOS port and ZEsarUX from a
byte-identical L3 `$BA83` start and diffs each frame in the brick-play ROI
in RGB palette space. **Frame 0 is 0 px** — that aligned start is what
makes everything below checkable. `make test-frame-step` gates the floor.

## Verified byte-exact

**Ball motion.** `handling_ball`'s 64-direction q8.8 motion:
`dir_to_dxdy` with the `LAD69` X/Y cross, and a fraction-preserving
cell-edge snap. `LAD69` `PUSH HL`, runs the multiply-by-speed on **BC** and
adds that to **X**, then `POP HL` and adds `HL * speed` to **Y** — so
assigning the components straight through gives X the wrong magnitude.

**Brick collision.** The `LAFFC` port, default for every ball, byte-exact
over L3's 150-frame trajectory at frames 1/5/10/20/40/60/80/100/150 — hit
cell, axis, snapped position, direction and q8.8 fraction.
`test-laffc-ball-frame1`, and `test-laffc-ball-l5-metal` for the L5
edge-metal case. `BATTY_LEGACY_COLLISION=1` reverts to `brick_collision`,
which also remains a fallback when LAFFC reports no hit, so nothing can
pass a brick through. `notes/laffc-decode.md`.

**Falling objects** (bonus drop, enemy bomb, +400 popup). `LA55A_0`'s
accelerating fall (`LD DE,$0008 / LD B,$02`: acc += 8/frame, velocity
capped at $0200, position += acc) is `motion_accel_step(&m, 0x0008, 0x02)`;
the bonus and bomb share it as the original shares `object_bonus`. The
+400 uses `handling_400pts`'s `$0028`/`$80`. Its vertical init is
`-((counter_misc & 1) + 1)`, and its horizontal drift is
`±((random & 1) + 1)` — magnitude from bit 0, sign from bit 7, read-current.

**Laser.** Bullet speed 6 px/frame (`SUB $06`). Fire cadence: the original
gates on `SUB $02 / JR C` and resets the counter to a parity-adjusted
`$16`/`$17`, both of which underflow exactly 12 frames after a shot. The
port checks `cooldown == 0` BEFORE its end-of-frame `-= 2`, so the
equivalent reset is `$18`. A countdown gating on the CARRY of a `SUB`
trips one tick later than a `== 0` gate in C — match the PERIOD, not the
literal.

**Bullet vs brick.** The original calls the same `LAFFC`, but at
`LAFFC_13` a bullet (`sprite_set & $3F == $05`) jumps to
`LAFFC_29`/`LAFFC_31`, which CONVERTS the bullet object into the impact
blast (sprite 2, height 6, x snapped `AND $F8`) and falls into the
destruction path — so a bullet stops at the first brick. The port mirrors
that, including the 8 px x snap. Residual: the cell find is a point lookup
rather than LAFFC's full straddle, which for a narrow bullet moving
straight up differs only at sub-cell boundaries.

**Death sparks.** Spawn (`LBC10_3`): 10 sparks, `x = bat.x + width/2 - $0C`
then +3 each, `y = $AE`, `dir = $1B` then `+5 & $3F` each, speed $02, body
width $08, duration_base and frame_ticks `$18`. Per frame
(`handling_spark`): `LAD69` motion, deactivate at `y >= $C0`,
`bounce_wall`, and the decay timer `DEC (IX+$15)`; on zero advance the
frame, `$14 >>= 1`, `$15 = $14 + 1`, die at frame 4. The Double Play split
is in `notes/double-play.md`.

**Enemy bomb drop** (`bomb_appear` $A989, shared by bird and UFO): returns
if a bonus or bomb is already falling (the original shares one
`object_bonus` slot); the 1/64 gate is `(random_lo + random_hi) & $3F == 0`
— an ADD, not an XOR — read via `rng_sample`; cutoff at `enemy_y + 8 >=
$C0`; spawns at (enemy_x + 8, enemy_y + 8).

**Bat movement.** ±4 px/frame, both directions pressed cancels. The
original moves UNCONDITIONALLY then clamps every frame; the port used to
guard BEFORE moving, which let the bat rest up to 3 px past the margin
(from `x = min + 2` a guarded -4 lands on `min - 2` and sticks). It now
moves then clamps to `[8 + extra, 248 - body_w - extra]`, computed in `int`
so the `unsigned char` cannot wrap.

**Bonus economy.** Drop gate `(random_number + $01 & $0F) < 5` (5/16),
read-current. Table selection `round_number < 6 ? first : second`. Both
32-byte tables match byte-for-byte (the original `LDIR`s only the first 16
and indexes `& $0F`, so the upper 16 are vestigial in both). Type pick
re-rolls via `random_generate` each retry, `idx = rng_hi & $0F`, rejecting
a pick equal to `current_bonus` — which the original sets from
`object_bat_1+$14`, so the port's `code == bat.bonus_applied` reject is
faithful rather than approximate. Per-type rejects (TRIPLE if >1 ball, SLOW
if a ball is already slowest, LIFE if dropped this round, ROCKET if active
plus the round-6 `& $C0` rarity gate) all mirror it. The port caps the
retry loop at 16 where the original loops until valid; with populated
tables that always succeeds.

**Bonus catch.** Any non-bomb bonus awards +400; a bomb kills you; the
sound is `sound_live_add` for LIFE else the resize beep; `bonus_applied`
takes the caught code for every type except ROCKET, which jumps to
`get_rocket` — so a new catch REPLACES the prior effect. SCORE adds +5000
and KILL_ALIENS blasts the on-screen alien.

**Scoring.** `points_table` ($AF45), 12 per-row BCD values
`120,110,100,90,80,70,60,50,40,30,20,10` (top row worth most), doubled when
the colour nibble `>= 6`, indexed `min(row, 11)`. Extra-life milestones
`live_add_steps` ($0395): BCD high bytes `$03,$06,$10,$15,$20,$25,$50,$75`
= 30k/60k/100k/150k/200k/250k/500k/750k.

**Ball speed-up and SLOW.** A shared ramp counter (the original's per-ball
`+$13`) increments when `(pit_frame_counter & 7) == 0`, and at `$94` resets
and increments every active ball's speed byte, capped at `$06` — so speed
climbs `$02 -> $06`, one step per ~1184 frames. `speed` drives
`dir_to_dxdy`'s q8.8 magnitude directly. SLOW resets all ball speeds to
`$02` WITHOUT touching the ramp, so it wears off as the speed climbs back.
Resets to 0 on every life / level / game init.

**Alien explosion.** `anim_alien_blast` is a 10-entry PING-PONG
`{1,2,3,4,5,4,3,2,1,1}` over the five blast sprites, advancing +1 every 2
frames (`misc_12` toggles `$50`<->`$10`), with `handling_blast`
deactivating at frame 9. GT-captured by poking the enemy to the blast state
(`set=$0A, misc_12=$50, misc_13=$90`) and stepping. The port's
`spr_blast_frames` is that table, `BLAST_FRAMES=10`,
`BLAST_TICKS_PER_FRAME=2`, matching the GT's sprite walk.

**SMASH** ($07, the port's BIG_BALL). Renders `SPR_BIG_BALL` with the
enlarged body; sets `axis = 0` so the ball destroys without bouncing
(`LAFFC_32 -> LAFFC_38/39`), and the destroyed cell makes the
`brick_collision` fallback return 0 so there is no stray bounce. Duration:
the original's `smash_counter` increments only on EVEN `counter_misc` and
expires at `$F8` (~496 frames); the port decrements `big_ticks` every other
tick from `BIG_BALL_DURATION = 0xF8`.

All 11 object handlers ($01-$0B: bat, ball, screen elements, bonus,
bullet, rocket, spark, UFO, bird, blast, 400pts) and all 10 bonus effects
have been surveyed against the disassembly.

**Running dot** (`running_dot` $B8E6) — the bat-edge dot with its frame
counter, direction bit and bat-shrink recovery branch, GT-pinned in test
mode.

## Remaining

**More original snapshots.** LAFFC is oracle-verified on L3 and L5.
Extending that to further levels, or to multi-ball and SMASH scenarios,
needs an original-side snapshot plus a port-side aligned-seed recipe per
scenario, the way `replay-l3-brick-flash` does for L3. That is the
substantive next investment, and it is a data problem rather than an
analysis one.
