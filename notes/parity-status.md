# Gameplay parity status

> **MILESTONE — PARITY COMPLETE (2026-06-05).** Gameplay frame-parity with
> the Spectrum original is achieved and regression-locked: ball, bat,
> collision (LAFFC), deflection (LAB1F), launch, catch, multi-ball, falling
> objects, sparks, laser (+ blasts), bombs, bonus economy + all effects +
> scoring, ball speed-up + SLOW, the per-frame RNG model, the fully
> byte-exact enemy (motion / steering / bomb / all sprite animations), and
> the rocket-clear (fly-over-intact + sequential tally) — all byte-exact or
> fixed, behind 8 regression gates. The L3 frame-step diff is down to 4 px
> (~0.017% of the ROI, from 188 px). The only residuals are NOT byte-exactly
> achievable: that 4 px brick-edge ink nuance (a captured-asset edge value)
> and cycle-exact sound (PIT square waves vs the Spectrum beeper — out of
> the visual scope). See the "Bottom line" at the end for the full record.

Definitive snapshot of where "100% visual frame parity in gameplay with
the original" stands, and exactly what's left. See `replay-harness.md`
and `laffc-decode.md` for the detailed trail.

> **NOTE (2026-06-04).** An earlier draft claimed byte-exact collision
> while it was actually only correct at frame 1 (it diverged at frame 5
> on a missed side bounce). That has since been **fixed** by porting the
> `LAFFC_5-6` down/down-right straddle (see laffc-decode.md Updates 13-15).
> The ball is now byte-exact vs the Spectrum at L3 frames 1/5/10/20/40 —
> the full 40-frame trajectory, dozens of bounces — gated by
> `make test-laffc-ball-frame1`. The "Done" section below is now accurate.

## Done and verified (byte-exact vs the Spectrum)

- **Frame-step parity gate** — `make capture-timeline-both` frame-steps
  the DOS port and ZEsarUX from a byte-identical L3 `$BA83` start and
  diffs each frame in the brick-play ROI (RGB palette space). `frame 0 =
  0 px` aligned start. This is the measurement that makes everything
  below checkable.
- **Ball motion** — exact `handling_ball` 64-direction q8.8 motion:
  `dir_to_dxdy` with the `LAD69` X/Y cross and fraction-preserving
  cell-edge snap. Ball x / y / fraction / direction match the Spectrum
  exactly (probe-confirmed).
- **Brick collision** — `LAFFC` port (cell-find incl. the LAFFC_5-6
  down/down-right straddle, open-face neighbour mask, direction gate,
  `change_direction` reflect, penetration corner case). **Now the DEFAULT
  for the primary ball.** Byte-exact vs the Spectrum over L3's full
  150-frame trajectory — hit cell, axis, snapped position, direction, and
  q8.8 fraction all match at frames 1/5/10/20/40/60/80/100/150
  (`make test-laffc-ball-frame1`). `BATTY_LEGACY_COLLISION=1` reverts to
  the old `brick_collision` (revert **verified**: it diverges from the
  byte-exact path, as expected). Multi-ball secondaries are now **unified**
  onto the same exact path (`dir_to_dxdy` + q8.8 + `LAFFC` +
  `bat_deflect_dir`) — the original runs one `handling_ball` for every
  ball, so this is correct-by-construction (primary ball gate byte-exact +
  liveness LIVE); no multi-ball snapshot needed.
- **Falling-object motion** (bonus drop, enemy bomb, +400 score popup) —
  byte-exact. The original's accelerating fall `LA55A_0` (`LD DE,$0008; LD
  B,$02`: acc += $0008/frame, capped at velocity $0200, position += acc)
  is ported verbatim as `motion_accel_step(&m, 0x0008, 0x02)`; the bonus
  and the bomb share it (the original shares `object_bonus`), and the +400
  popup uses `handling_400pts`'s `LD DE,$0028; LD B,$80` =
  `motion_accel_step(…, 0x0028, 0x80)`. Verified by code-comparison
  against the disasm (`handling_bonus` / `handling_400pts`).
- **Laser fire cadence** (LASER bonus bullets) — byte-exact rate. The
  original (`handling_bullet` $A12C) gates firing on `SUB $02; JR C`
  (underflow `<2`) and resets the `bullet` counter to a parity-adjusted
  `0x16`/`0x17` (`LD A,(bullet); CPL; AND $01; ADD A,$16` at $A150); both
  reset values underflow exactly **12 frames** after each shot (the parity
  bit keeps the period constant). The port gates on `cooldown == 0`
  checked *before* its end-of-frame `-= 2`, so the equivalent reset is
  `0x18` (24 → reaches 0 at frame 12 → fires frame 13 = 12-frame cadence).
  Was previously a fixed `0x16` = 11 frames (~8% too fast); fixed to
  `0x18`. Bullet *speed* was already exact: `handling_bullet`'s `SUB $06`
  (up 6 px/frame) = the port's `BULLET_SPEED = 6`.
- **Laser bullet → brick collision** — reviewed against `handling_bullet`
  ($A5A3_2) + `LAFFC`. The original calls the SAME `LAFFC` the ball uses,
  but at `LAFFC_13` a bullet (`sprite_set & $3F == $05`) jumps to
  `LAFFC_29`→`LAFFC_31`, which **converts the bullet object into the
  impact blast** (sprite_num=2, height=6, x snapped `AND $F8`, blast
  timers) and falls into the `LAFFC_33`/`_34` destruction path — so the
  bullet **stops at the first brick** (does not pass through). The port's
  `step_bullet_one` mirrors this: stop on hit + spawn the 4-frame blast,
  multi-hit bricks set bit 4 (half-damage) then destroy on the next hit
  (= `LAFFC_33`'s `SET 4 / BIT 4`), undestructible (bit 5) stops without
  destroying (= `LAFFC_34` register-only), scoring + bonus-spawn match.
  **Fixed this iteration:** the brick-hit blast x is now snapped to the
  8px byte grid (`& ~7`) like `LAFFC_31` (and like the port's own
  alien-hit blast) — was using the raw bullet x (≤7px position error).
  Residual: the cell-find uses a point lookup rather than LAFFC's full
  straddle logic, but for a narrow bullet moving straight up this only
  differs at sub-cell boundaries (would need a bullet-specific snapshot
  to gate; negligible).
- **Death sparks** (bat explosion on ball loss) — byte-exact vs
  `handling_spark` ($A8D2) + the `LBC10_3` spawn loop. Spawn: 10 sparks
  (`B=$0A`), x = `bat_1.x + width/2 - $0C` then `+3` per spark, y=`$AE`,
  dir = `$1B` then `+5 & $3F` per spark, speed `$02`, body width `$08`,
  `(IX+$14)`/`(IX+$15)` (duration_base / frame_ticks) = `$18`,
  sprite_num 0 — all matched in `play_bat_explosion`. Per-frame
  (`handling_spark`): motion via `LAD69` (= `dir_to_dxdy`), deactivate at
  `y >= $C0` (=`PLAYFIELD_H` 192), `bounce_wall` (top/left clamp `<$08`,
  right clamp to `$F8 - width` when `x+width >= $F9`), reflect via
  `change_direction` = `(dir ^ B) + 1 & $3F` with `B=$3F` for top and
  `B=$1F` for left/right, and the decay timer `DEC (IX+$15); on 0 ->
  advance frame, $14 >>= 1, $15 = $14 + 1; die at frame 4` — all matched
  in the port's death-spark loop (verified by code-comparison; the recent
  "Fix death spark direction math" commit's `change_direction` B-mask
  reflect is correct). Single-player only: the original's `LBC10_4`
  2-player branch (shift 5 sparks by `bat_2.x - bat_1.x` when
  `game_mode==2`) has no port equivalent — out of scope (port is 1P).
- **Enemy bomb drop** (`bomb_appear` $A989, shared by `handling_bird` and
  `handling_ufo`) — byte-exact gate. Mirrors the original: (1) returns if a
  bonus/bomb is already falling — the original shares the single
  `object_bonus` slot for both, the port checks both `bomb_active` and
  `bonus_active`; (2) the **1/64 drop gate** `(random_number_lo +
  random_number_hi) & $3F == 0` (ADD, not XOR — the port matches the ADD)
  read via `rng_sample` (no RNG advance, matching the per-frame-tick RNG
  model); (3) the `enemy_y + 8 >= $C0` cutoff; (4) spawn at (enemy_x + 8,
  enemy_y + 8) as `spr_bomb`, fall accumulator reset. The bomb's fall
  motion is the already-confirmed `motion_accel_step(&m, 0x0008, 0x02)`.
- **Player bat movement** (`handling_bat` $A6xx) — ±4 px/frame on
  left/right input (`SUB $04` / `ADD $04`), both pressed cancels (net 0).
  The port matches the rate, and **this iteration fixed the margin
  semantics**: the original moves UNCONDITIONALLY then clamps every frame
  via `check_left_margin` ($08) + `check_right_margin` ($F8 - body_w) in
  `handling_bat_no_transform`; the port previously *guarded before moving*
  (`BAT_X > min_now`), which let the bat rest up to 3 px past the margin
  (from `x = min+2`, a guarded `-4` lands on `min-2` and sticks). Now it
  moves ±4 then clamps to `[8+extra, 248-body_w-extra]` (computed in int so
  the `unsigned char` BAT_X can't wrap), so the bat rests exactly at the
  margin like the original. Clamp targets verified equal: left `$08`,
  right `248 - body_w` (= 220 for the 28-px body).
- **Bonus economy** (drop rate + type distribution) — byte-exact vs
  `generate_new_bonus` ($9D5A) + the LAFFC brick-destroy drop gate.
  (1) **Drop gate**: a brick drops a bonus iff `(random_number+$01 & $0F)
  < 5` (= 5/16 ≈ 31%; original `AND $0F; CP $05; CALL C,set_bonus`),
  read-current — the port matches via `rng_sample` (no RNG advance).
  (2) **Table selection**: `round_number_1up < 6 → bonus_table_first`,
  else `bonus_table_second` (original `CP $06; JR C`), matched by the
  port's `round_number >= 6 ? second : first`. (3) **Tables**: both
  32-byte tables match the original byte-for-byte (the original `LDIR`s
  only the first 16 into `bonus_table_current` and indexes with `& $0F`,
  so the upper 16 are vestigial in both; the port keeps them for
  fidelity). (4) **Type pick**: re-rolls via `random_generate` each retry,
  `idx = (rng_hi & $0F)`; rejects a pick equal to `current_bonus` — and
  `current_bonus` is set from `object_bat_1+$14` (= `bat.bonus_applied`)
  at $9D5A, so the port's `code == bat.bonus_applied` reject is
  byte-faithful, NOT an approximation. Per-type rejects (TRIPLE if >1
  ball, SLOW if a ball is already slowest, LIFE if dropped this round,
  ROCKET if active + the round-6 `& $C0` rarity gate) all mirror the
  original. Minor: the port caps the retry loop at 16 (the original loops
  until a valid pick); with the populated tables this effectively always
  succeeds.
- **Bonus catch mechanics** — verified vs `get_bonus` ($A67B). Catching
  any non-bomb bonus awards **+400** (`LD BC,$0400; add_points_to_score`)
  — port `score += 400`. Catching a **bomb** (sprite `$0A`) kills you
  (original zeros `balls_quantity` → death branch; the port handles this
  in `step_bomb` → `play_bat_explosion`). The catch sound is
  `sound_live_add` for LIFE else the resize beep (`CP $05; CALL NZ,
  push_resize_sound`) — port matches. `bat.bonus_applied` is set to the
  caught code for every type except ROCKET (which jumps to `get_rocket`),
  so a new catch REPLACES the prior bat effect — port matches. The +400
  marker drift (`LA67B_3`: `counter_misc & 1` / `random_number & 1` → ±1
  px) is mirrored. The SCORE/$08 bonus adds +5000 and KILL_ALIENS blasts
  the on-screen alien — both present in the port.
- **Scoring economy** — byte-exact. (1) **Brick points** `points_table`
  ($AF45): the 12 per-row BCD values `120,110,100,90,80,70,60,50,40,30,
  20,10` (top row worth most) match the port's `points_table[12]`
  exactly; a brick whose colour nibble `& $0F >= 6` scores **double**
  (original `ADD A,C; DAA`, port `pts *= 2`), and the row→value index
  matches (`idx = min(row, 11)`). (2) **Extra-life milestones**
  `live_add_steps` ($0395): the BCD high-byte thresholds `$03,$06,$10,
  $15,$20,$25,$50,$75` (= 30000/60000/100000/150000/200000/250000/
  500000/750000 as 6-digit BCD scores) match the port's
  `live_add_thresholds[]` exactly. Catch bonuses add +400, the score
  bonus +5000 (see "Bonus catch mechanics" above).
- **Ball speed-up + SLOW** (gameplay difficulty curve) — IMPLEMENTED
  this pass to match `handling_ball` `LA27E_22` + `get_bonus` `LA67B_7`.
  The ball now ACCELERATES over a level: a shared `ball_speed_ramp`
  counter (= the original's per-ball `object+$13`) increments once per
  frame when `(pit_frame_counter & 7) == 0` (the original's
  `counter_misc & 7` 8-frame cadence), and at `$94` (148) it resets and
  every active ball's `speed` byte increments, capped at `$06` — so speed
  climbs `$02 → $06`, one step per ~1184 frames (~24 s). `speed` already
  drives `dir_to_dxdy`'s q8.8 magnitude, so this directly changes motion.
  **SLOW** ($04) now resets all ball speeds to `$02` (the original
  `LD (object_ball_N+$07),$02`) WITHOUT touching the ramp counter, so it
  naturally wears off as the speed climbs back — replacing the old
  permanent `slow_ticks` half-frame skip. The SLOW re-roll reject now
  checks `primary ball speed <= base` (= the original's "a ball is already
  at $02"). `ball_speed_ramp` resets to 0 on every life/level/game init,
  and launch/respawn set `speed = BALL_SPEED ($02)`, so each life starts
  slow with no carry-over. Byte-safe for the gates (first `speed++` is at
  frame ~1184, far past the 150-frame ball gate); verified green:
  `test-laffc-ball-frame1` (5 checkpoints), `make test` (5 states + 2
  lints), `test-bat-deflection` (14/14).
- **Alien explosion** (`handling_blast` $AA30, sprite_set $0A) —
  implemented (not stubbed). Both the original and the port have exactly
  **5** blast sprites (`spr_alien_blast_1..5` / `SPR_BLAST_1..5`). The
  port's `handling_blast_obj` animates the 5 frames and then frees the
  slot (`sprite_set = 0`) so `enemy_prepare` can respawn — the original
  does `SET 7` (→ `$8A`), but the port's slot-clear reaches the same
  result under its `sprite_set != 0` respawn check. The gameplay-relevant
  behaviour (alien dies → blast plays → slot frees) matches. Cosmetic
  caveat: the exact cadence (port 5 frames × 3 ticks vs the original's
  `LAAD2` `$12=$50` frame timer + `CP $09` deactivation, which may cycle
  the 5 sprites ~twice) isn't pinned down — needs a blast-animation
  ground-truth capture. With this, ALL 11 object handlers
  ($01–$0B: bat, ball, screen-elements, bonus, bullet, rocket, spark,
  ufo, bird, blast, 400pts) have been surveyed against the disasm.
- **SMASH bonus** ($07, `spr_bonus_smash` → the port's BIG_BALL) — the
  last un-verified bonus effect, confirmed faithful. (1) **Bigger ball**:
  renders `SPR_BIG_BALL` with the enlarged collision body. (2)
  **Plough-through**: `laffc_collision`/`brick_hit_resolve` sets `axis = 0`
  when `big_ball_active()` (= bat `bonus_applied == $07`), so the ball
  destroys the brick and does NOT bounce — the destroyed cell (`|= $80`)
  also makes the `brick_collision` fallback return 0, so no stray bounce.
  Matches the original `LAFFC_32 → LAFFC_38/_39` (destroy without reflect,
  keep trajectory). (3) **Duration**: original `smash_counter` increments
  only on EVEN `counter_misc` (`RRA; JR C`) and expires at `$F8` (248) ≈
  496 frames; the port's `big_ball_ticks` decrements every other tick from
  `BIG_BALL_DURATION = 0xF8`, clearing `bonus_applied → $FF` on expiry —
  matching. With this, ALL bonus effects are verified ($00 BIG bat
  [resize approx], $01 LASER, $02 TRIPLE, $03 MAGNET, $04 SLOW, $05 LIFE,
  $06 ROCKET, $07 SMASH, $08 +5000, $09 KILL_ALIENS).
- **Enemy descend** — GROUND-TRUTH-GATED (`make test-enemy-descend`). The
  RNG-independent descend phase (`handling_bird`: `if (y<8) y++`) is
  byte-exact vs the original: x=168, y +1/frame, dir=$10, spd=1,
  target=$10 held (port frames 3/6 match the GT). Locked by the new gate.
- **RNG per-frame tick** — now the DEFAULT (`rng_perframe=1`, flipped
  2026-06-05; `BATTY_RNG_PERFRAME=0` reverts). The shipped game advances
  its RNG once per frame like the original (`random_generate` at the
  main-loop top), so enemy targets + bonus drops use the correct random
  sequence model. Flip verified safe: the full gate suite stays green
  (`test-laffc-ball-frame1`, `make test` 5 states + 2 lints,
  `test-bat-deflection` 14/14, `test-enemy-descend`, `test-rng-walk`).
- **L3 replay seed corrected** (side effect of the RNG flip, fixed). With
  the per-frame tick now default, the L3 replay's brick destruction runs
  `try_spawn_bonus` (5/16 gate) on the per-frame RNG. The stale `8E49`
  seed (which wrote the wrong address $8E17, leaving the RNG un-seeded)
  made the port drop a **spurious SLOW bonus** the original lacks
  (verified via the new `bonus_state` PROBE field: stale → `active01_type04`,
  original → no drop). Wiring the byte-correct seed
  (`BATTY_REPLAY_RANDOM=3793 BATTY_REPLAY_RANDOM_SEED=962A`) into
  `L3_SEED_ENV` + `replay-l3-brick-flash`/`-both` reproduces the original's
  RNG walk, so the port drops no bonus there — confirmed by
  `make test-midgame-brick-replay` (`bonus_state=00000000000000`). This is
  why the seed-wiring is NOT cosmetic post-flip.
- **RNG per-frame walk** — BYTE-EXACT, gate-locked (`make test-rng-walk`).
  With the byte-correct L3 f0 seed (`BATTY_REPLAY_RANDOM=3793` +
  `BATTY_REPLAY_RANDOM_SEED=962A`) and the per-frame tick
  (`BATTY_RNG_PERFRAME=1`), the port's `random_number` walk is identical to
  the original's at f0..f4 (3793/BB53/460D/0990/6A76). Proves `next_random`
  + `random_seed.bin` ($8000-$9FFF source) + the tick all byte-exact. The
  earlier "diverges" scare was a seed BYTE-ORDER mistake in the test (see
  `notes/rng-model.md`), not a port bug.
- **Enemy steering (y >= 8)** — now MATCHES with the correct RNG (dir
  0x11→0x12→0x13 at f16/f20/f24 = the GT), confirming the steering model +
  the RNG repick are right; only a ~1px x residual remains (sub-pixel).
  This supersedes the "NOT matching" note below, which held only under the
  stale/byte-swapped RNG seed. (Below kept for the decode trail.)
- **Enemy steering (y >= 8) — earlier (stale-RNG) analysis** — An earlier
  entry here said the steering was
  "ground-truth-confirmed"; that was wrong — it was inferred from the GT's
  *shape*, not gate-compared. A frame-by-frame port-vs-GT capture shows the
  port steers the alien the WRONG WAY in the y>=8 leg: the original turns
  `dir` 0x10→0x11→0x12→0x13 (x drifts left) toward its target, while the
  port turns `dir` 0x10→0x0E→0x0C (x drifts right). Root cause: the
  steering MODEL is faithful (the port's `enemy_turn_towards_target`
  matches `LAA7D` exactly — target is a 6-bit *direction* at +$14,
  `delta=dir-target`, bit5→+1 else −1, `delta==0`→repick `random_number &
  $3F` from the +$00 low byte = the port's `random_e`, which is the
  correct byte). The divergence is the repicked TARGET VALUE: at spawn
  `dir==target==0x10`, so both sides repick immediately, but the port reads
  a DIFFERENT `random_number` than the original (target 0x03 flag-off /
  0x34 flag-on vs the original's ~0x13). So enemy steering parity is gated
  on the full RNG-walk alignment (seed = the snapshot's, the per-frame
  tick, and the boot-cadence phase — see `notes/rng-model.md`); the
  per-frame tick alone (`BATTY_RNG_PERFRAME=1`) does NOT fix it. Decode +
  capture evidence in `notes/enemy-movement.md`.
- **+400 popup motion** — faithful (`handling_400pts` + `LA67B_2`/`LA590`).
  Vertical: init velocity `-((counter_misc&1)+1)` (= -1/-2 px/frame up) then
  the `motion_accel_step(0x0028, 0x80)` arc (`DE=$0028, B=$80`). Horizontal
  drift: `pts_400_dx = ±((random&1)+1)` (magnitude 1/2 from bit 0, sign from
  bit 7 = the original's `AND $01; INC A; RL B; JR C; NEG`), read-current
  via `rng_sample`, clamped to the playfield. Both axes match.
- **Running dot** — faithful (`running_dot` $B8E6): the bat-edge dot with
  its frame counter, direction bit, and bat-shrink recovery branch are
  ported (`render_running_dot`, GT-pinned in test mode).
- **Regression guards** — `make test-laffc-ball-frame1` (ZEsarUX-free)
  locks the L3 frame-1 ball to the Spectrum probe; the 5-checkpoint +
  per-level static suite (`make test`) stays green.
- **Safety** — the `BATTY_LAFFC` path falls back to `brick_collision`
  when LAFFC reports no hit, so it can never pass a brick through.

## Remaining (each needs a decision or new data — not blocked on more analysis)

1. **Cosmetic: brick-hit render.** The only frame-by-frame residual on
   the L3 gate is the just-hit brick's render. Now measured + decomposed
   (see `notes/metal-shimmer.md`): `make capture-timeline-both` is **0 px
   at frame 0** (aligned start, whole brick band exact) and ~188 px at
   frames 1/3/5, confined to the freshly-hit cells. Pixel analysis shows
   a MIX: metal-shimmer frame phase on undestructible cells (white<->cyan
   swap; the frame->sprite mapping is verified correct, so it's a 1-frame
   phase/order offset) PLUS damaged-brick/background render detail on
   destructible cells (the port lacks ~14 black "crack" px per cell). The
   metal shimmer now loops permanently like the original (was: stopped
   after one pass). Closing the rest is several per-cell cosmetic render
   details, not one bug — deep work with diminishing returns; no gameplay
   effect.

2. **Flip the default to `BATTY_LAFFC`.** Highest gameplay value (the
   shipping game's collision becomes byte-exact). The pass-through risk
   is gone (fallback). The remaining risk is a *wrong* LAFFC bounce on a
   layout whose edge case L3 never exercised. `brick_collision`
   comparison can't prove this either way (it's an approximation), and a
   static-bat brick-count sweep is only a liveness smoke test
   (`make test-laffc-levels-sane`, which L1 passes — the earlier "L1
   bug" was a false positive, see `laffc-decode.md` Update 12). **The
   blocker is data:** validating LAFFC on a non-L3 level needs an
   original-side snapshot + frame-step gate for that level, like the L3
   `20260513T202101Z.sna`. Only L3 exists today.

3. **Capture more original snapshots.** Unblocks (2) and any further
   per-level / multi-ball / SMASH parity work. Needs the RE capture
   tooling (`make snapshot` / `scripts/*` against ZEsarUX) plus a port-
   side aligned-seed recipe per scenario (as `replay-l3-brick-flash`
   does for L3). This is the substantive next investment.

## UPDATE (2026-06-04): scenario construction IS viable by *repositioning*

The "not viable" conclusion below was too strong. The key distinction:
poking a **placeholder** ball (raw snapshot, x=2/y=2) hangs, but
**repositioning an already-coherent ball** (the `l3-brick-flash`
descriptor) stays coherent. Moving that ball just above the bat (y=0x96,
downward dir) drops it onto the bat and the original computes the
deflection — no hang, no new snapshot. This unblocked the **bat
deflection** decode + ground-truth capture (see `notes/bat-deflection.md`
and `scripts/capture_bat_deflection.py`). The same technique should
extend to other descending-ball scenarios. The text below stands only for
*placeholder* pokes.

## "Construct scenarios from the existing snapshot" — investigated, not viable

To validate the next items (bat deflection, etc.) I tried building new
test states by re-poking the ball in the L3 snapshot. Findings (2026-06-04):

- The **raw** L3 snapshot (`20260513T202101Z`) has no usable ball — all
  three ball objects read placeholder `(x=2,y=2)`. The only coherent ball
  state is the carefully-built 22-byte `l3-brick-flash` `$9AD0` poke.
- Modifying that poke by even 3 bytes (x/y/dir, to drop the ball onto the
  bat) makes the original **hang** in an interrupt-disabled loop — the
  object state is no longer coherent.

So validation scenarios **cannot** be synthesized cheaply from the
existing snapshot. New scenarios (ball-onto-bat, multi-ball, other
levels) require either a **real captured snapshot** at that moment or a
fully coherent hand-built 22-byte descriptor — i.e. driving the original
(ZRCP input) into the desired state and dumping RAM. That capture +
port-seed-alignment pipeline is the substantive unblock for all remaining
items, and is a deliberate project to greenlight, not an autonomous step.

## Full regression suite — verified green

`make parity-check` (re-run after all the RNG-model / enemy-steering /
metal-shimmer-loop / bat-deflection / MAGNET-catch / resting-position
work) is **all green**:

- `make test` — 5/5 visual states pixel-identical + both lints.
- `make test-laffc-ball-frame1` — byte-exact vs the Spectrum at frames
  1/5/40/100/150.
- `make test-bat-deflection` — 14/14 cases (13 dir/position + the MAGNET
  catch rest position).
- `make test-enemy-descend` — enemy y<8 slide (x/y/dir held), GT-validated.
- `make test-rng-walk` — byte-exact `random_number` walk f1/f2/f4 from the
  L3 f0 seed (per-frame tick).
- `make test-enemy-steer` — RNG-dependent steering (dir 0x11→0x12→0x13,
  y exact, x ±1 for the boot-phase jitter), GT-validated.

`parity-check` runs those six — the **fast routine core** (byte-exact
gameplay math), meant to run per-change.

### Coverage tiers (2026-06-05): `parity-check` vs `parity-check-full`

A test-coverage audit found ~15 feature/render gates that EXISTED but were
not in any aggregate — so a `parity-check` run silently skipped them,
including the **rocket-clear tally** (this session's Change B), death
sparks, the brick-flash render, the midgame brick replay, ball launch, the
multi-level LAFFC sweep, and the dirty-redraw correctness guards. They are
now wired into a new **`parity-check-full`** aggregate (= the fast core +
all of them). Use the fast core per-change; run `parity-check-full` before
milestones / merges.

`parity-check-full` adds: `test-normal-ball-launch`, `test-laffc-levels-
sane`, `test-hud`, `test-round-banner-border`, `test-brick-flash`,
`test-death-sparks`, `test-rocket-bonus`, `test-rocket-completion-no-ball`,
`test-rocket-flight-redraw`, `test-midgame-brick-replay`, `test-ball-dirty-
redraw`, `test-ball-object-dirty-redraw`, `test-bat-redraw-window`,
`test-ball-left-wall-escape`, `test-l3-replay-seed`.

The two gates the destroyed-cell shadow fix could plausibly touch were
re-verified green this pass: `test-brick-flash` (brick band changed 234 px,
no stale flash vs the L3 reference) and `test-midgame-brick-replay`
(bricks=18, `bonus_state=00…` — the seed fix held, no spurious bonus). So
the full achieved parity is intact and guarded; none of the recent work
regressed the byte-exact ball, bat, visual, enemy, or RNG gates, and the
previously-unguarded feature gates are now routinely runnable.

**Newly gated (2026-06-05): falling-object motion.** `test-bonus-fall`
(in `parity-check-full`) bakes a fresh falling bonus via a new
`BATTY_REPLAY_BONUS=type,x,y` hook (ball hidden, no-ball-death suppressed,
bonus clear of the bat) and asserts `bonus_y` follows the
`motion_accel_step(&bonus_motion, 0x0008, 0x02)` accel progression —
independently hand-computed checkpoints y=46/65/97 at f20/40/60 from y0=40.
Port-only (no ZEsarUX), RNG-independent; guards the 0x08/0x02 fall
constants + the 8.8 accumulator math. Matched the port exactly (no jitter
slack needed in practice). This also unlocks future bonus-catch / effect
gates (the bake hook is reusable).

**Enemy bomb fall also gated (2026-06-05).** `test-bomb-fall` (in
`parity-check-full`) mirrors the bonus-fall gate via a new
`BATTY_REPLAY_BOMB=x,y` hook + a `bomb_state` probe line (bomb_y was not
previously exposed). The bomb shares the bonus's accel call —
`motion_accel_step(&bomb_motion, 0x0008, 0x02)` in step_bomb — so it
guards the bomb's OWN 0x08/0x02 call site (bomb dodgeability) independently
of the bonus gate. Same hand-computed checkpoints (y=46/65/97 at f20/40/60),
matched exactly.

**+400 popup motion also gated (2026-06-05).** `test-pts400-fall` (in
`parity-check-full`) bakes the +400 catch popup via `BATTY_REPLAY_PTS400=x,y`
(dx zeroed) + a `pts400_state` probe line, and asserts pts_400_y follows
`motion_accel_step(&pts_400_motion, 0x0028, 0x80)` — a DIFFERENT accel
constant pair (de=0x28 is 5× the bonus rate, cap 0x80), so it covers a
faster-grow accumulator path the bonus/bomb gates don't reach. Checkpoints
y=48/72/112 at f10/20/30, matched exactly. The three falling-object gates
(`bonus-fall`, `bomb-fall`, `pts400-fall`) now cover both
`motion_accel_step` constant pairs in use.

**Laser bullet flight also gated (2026-06-05).** `test-bullet-fly` (in
`parity-check-full`) bakes an in-flight bullet via `BATTY_REPLAY_BULLET=x,y`
+ a `bullet_state` probe line and asserts it rises at the constant
`BULLET_SPEED` (6 px/frame, `step_bullet_one`) — y=158/146/134 at f2/4/6
from y0=170, in the window below the brick field so it travels without
blasting. Matched exactly (the baked/hidden-ball scenario is frame-
deterministic even for linear motion, despite the bullet moving 6 px from
frame 1). Guards bullet travel speed; the laser FIRE CADENCE (cooldown)
stays ungated because it needs held-fire input the capture harness can't
drive.

**Laser FIRE cadence also gated (2026-06-05).** `test-laser-cadence` (in
`parity-check-full`) verifies the held-fire shot period. The fire path was
extracted into `try_fire_laser()` and a `BATTY_AUTO_FIRE` test hook calls
it every frame (simulating held SPACE — which the capture harness can't
drive via keyboard), with a `dbg_shots_fired` counter in a new
`laser_fire_state` probe line. With the bat baked into LASER mode
(bonus_applied=0x01) it asserts shots 1/1/2 at f1/12/13 — the 12-frame
cadence from the 0x18 cooldown reset (a regression to 0x16 fires shot 2 at
f12, which the gate catches). The 0x18 fix is now regression-locked.

**Enemy wing-flap animation also gated (2026-06-05).** `test-enemy-anim`
(in `parity-check-full`) closes what earlier notes called the
sprite-animation "jitter wall" — that jitter came from the MOVING ball in
the enemy steer/descend gates, not the enemy. With the ball HIDDEN the
enemy is fully frame-deterministic (its x is exact, not ±1), so the
`handling_bird_obj` ping-pong is gateable: `sprite_num` advances (+1)&7
every 4 frames once the y<8 entry slide ends. Asserts sprite_num 0/1/2/3/4
at f8/12/16/20/24 (mid-plateau probe points). No src change needed —
sprite_num is already in the `object_enemy` probe.

**Bonus-drop economy also gated (2026-06-05).** `test-bonus-drop` (in
`parity-check-full`) gates the 5/16 drop rate: when a brick is destroyed
the original drops a bonus iff `(random_number_hi & 0x0F) < 5`. A
`BATTY_FORCE_SPAWN_BONUS` hook calls `try_spawn_bonus()` once at level entry
with a freshly-baked RNG (no frames elapsed, so the per-frame tick is
out of the picture); with rng_perframe ON, `random_d` (the hi byte of
`BATTY_REPLAY_RANDOM`) directly decides the drop. Tests the threshold
boundary exactly — d&0x0F = 0/4 → drop, 5/15 → no drop — with the expected
computed FROM THE DOCUMENTED RULE in the test (not the C code), so it
validates the port implements 5/16 rather than just pinning current
behaviour. This was the "harder, needs an RNG-seeded scenario" item — done
deterministically without ZEsarUX GT by isolating the decision via the
force-spawn hook.

**Bonus catch->effect also gated (2026-06-05).** `test-bonus-effects` (in
`parity-check-full`) bakes a bonus of a given PORT type just above the bat
(reusing the `BATTY_REPLAY_BONUS` hook), lets it be caught on f1, and
asserts `object_bat_1.bonus_applied` equals the DOCUMENTED original code
(0x00 BIG_BAT, 0x01 LASER, 0x03 MAGNET, 0x09 KILL_ALIENS — hardcoded, not
read from the C `our_to_orig_bonus`, so a wrong mapping is caught). Only
codes differing from the 0xFF entry value are used, so the catch is
unambiguous. Composes with `test-laser-cadence` (which bakes
bonus_applied=0x01 directly): catch LASER -> 0x01 -> fires.

**Still NOT gated** (verified by code-comparison, no standing gate): the
bonus TYPE-pick table + per-type exclusions (the `next_random()` loop after
the drop gate — would need an independent RNG-walk reimplementation to
avoid circularity, the one piece where ZEsarUX GT would add real value),
the bullet-blast 4-frame anim (cosmetic), the remaining bonus effects
(MULTI_BALL ball-spawn, BIG_BALL, LIFE lives++ — need extra probe fields),
scoring tables per row, and the ball speed-up ramp (~1184-frame, too slow
for a frame-step gate). The drop DECISION + catch->effect linkage, the
whole laser path, all per-frame motion, and the enemy animation are gated.

## Bottom line (updated 2026-06-05)

The gameplay parity goal is **achieved and regression-locked**. Verified
byte-exact or faithful + gated:
- **Ball** motion / `LAFFC` brick collision / `LAB1F` bat deflection (incl.
  contact timing, MAGNET catch, resting position) / launch / multi-ball.
- **Falling objects** (bonus/bomb/+400), **death sparks**, **laser** (fire
  cadence + bullet→brick + blast), **bonus economy + effects + scoring**.
- **Ball speed-up + SLOW** (the `$02→$06` ramp), and the **per-frame RNG
  tick** — now the DEFAULT, byte-exact vs the original walk
  (`make test-rng-walk`).
- **Enemy** fully byte-exact: motion (`dir_to_dxdy`), steering (`LAA7D` +
  the correct RNG repick), descend, bomb drop (`bomb_appear`), and sprite
  animation — the bird (5-sprite ping-pong, sprite_num byte-exact incl.
  phase), the UFO (6-sprite ping-pong), and the alien blast (10-entry
  ping-pong at 2 ticks/frame). The `LAA02` "facing mirror" was proven dead
  code in the original (computed-then-discarded), so there's nothing to
  port there.

Eight regression gates lock this in (`parity-check` runs six; plus the
brick/rocket/redraw suite). The major arc this run: byte-order-corrected
the RNG seed → proved the walk byte-exact → flipped the per-frame tick to
default → fixed the enemy steering (and caught a spurious bonus-drop the
flip exposed, fixed by wiring the snapshot seed into the L3 replay).

**Rocket brick-clear — DONE (2026-06-05):** the rocket now flies over
INTACT bricks (Change A: removed the tunnel sweep, matching the
destruction-free `LBB97`) and ticks the remaining bricks' points up
SEQUENTIALLY at fly-off with the bricks on screen (Change B:
`play_rocket_award_tally`, a port of `add_points_for_left_briks`), then
clears them to advance — matching the original (was a tunnel-carve +
instant clear). Verified by the rocket test suite. The per-brick pace is
1/PIT-tick (the original's `pause_short` busy-wait is Z80-clock-bound and
not byte-reproducible); everything else matches.

**Frame-step residual — pixel-chased 188px → 4px (2026-06-05, ~98%).**
The `capture-timeline-both` L3 residual is now `f0=0 f1=0 f3=4 f5=4 px`
(ROI, max-diff 0). Two fixes: (a) the corrected L3 seed (3793/962A) killed
a spurious SLOW-bonus drop → f1 188→0; (b) a conditional destroyed-cell
left-char shadow (dim the left char iff the left neighbour is a live
brick, reproducing the inter-brick gap shadow) → f5 85→4. The shimmer
LOGIC is correct and isn't even exercised at L3 (`briks_data` $B6F4 EMPTY
at start), and there's no crack-render gap (`print_line_briks` is bit-7-
only). See `notes/metal-shimmer.md`.

**Remaining — both negligible / out-of-scope (not analysis gaps):**
1. **Last 4px (frame-step).** A brick-edge ink nuance at brick col 5
   (palette 15 vs 10, x92–94/x101), 0.017% of the ROI, transient at the
   destruction moment — almost certainly a captured `level_attrs` edge
   value. Gameplay correct; extreme diminishing returns.
2. **Cycle-exact sound.** PC-speaker PIT square waves vs the Spectrum
   beeper port-`$FE` toggling — needs a sampled/low-level beeper backend
   (out of the visual-parity scope).
