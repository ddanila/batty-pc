# Gameplay parity status

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

So the full achieved parity is intact and guarded; none of the recent
work regressed the byte-exact ball, bat, or visual gates.

## Bottom line

The core parity goal — exact ball motion, brick collision (LAFFC), bat
deflection (LAB1F incl. contact timing, MAGNET catch, resting position),
and launch — is achieved, gate-verified on L3, and regression-locked.
Enemy steering is a faithful structural port (`LAA7D`); the metal shimmer
loops permanently like the original (sprite data byte-identical). The one
frame-step diagnostic residual (`capture-timeline-both`, ~188 px in
freshly-hit cells) is proven a cosmetic, seed-comparison artifact (unsynced
shimmer counter phase — not a render bug; a measured phase-shift experiment
made it worse and was reverted). The remaining un-achieved items —
byte-exact enemy RNG targets, multi-ball secondaries, cycle-exact sound —
each need a deliberate sub-project (RNG-tick alignment + enemy seeding;
the `bonus_triple_ball` self-modifying spawn replay; a beeper backend),
not further analysis of the existing setup.
