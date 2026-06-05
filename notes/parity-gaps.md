# Original parity gaps

Known places where the current recreation is intentionally approximate
or still depends on captured original data. This is separate from
`known-bugs.md`: these are not necessarily user-reported defects, but
they are good next targets when tightening original fidelity.

## Visible / behavioral

### Some motion is approximate, not descriptor-exact

Several paths use gameplay-equivalent but not byte-exact motion:

- enemy movement: the `LAA7D` steering is now decoded and partly ported
  (turn 1 dir-step toward target every 4 frames; repick a random target
  *on arrival*, matching the original — the old 64-frame timer repick is
  gone). Still approximate: the port advances the RNG on demand rather
  than the original's per-frame tick (so enemy targets aren't byte-exact),
  and the bird doesn't yet run `LAFFC` brick collision or the exact
  `check_margins`. Ground truth + decode: `notes/enemy-movement.md`,
  capture via `scripts/capture_enemy_flight.py`. Enemy capture is
  unblocked (the L3 snapshot has a live enemy), unlike multi-ball.
- **brick/ball collision** — DONE. The `LAFFC` port (cell-find incl. the
  LAFFC_5-6 down/down-right straddle + neighbour-open-face mask +
  direction gate + `change_direction` reflect + fraction-preserving snap +
  penetration corner case) is now the **default** for the primary ball
  and is byte-exact vs the Spectrum over L3's 150-frame trajectory
  (`make test-laffc-ball-frame1`). `BATTY_LEGACY_COLLISION=1` reverts.
  Full decode + status in `notes/laffc-decode.md`.
- **bat-ball deflection (primary ball)** — DONE. The `LAB1F` ($AB1F) port
  (`bat_deflect_dir` in `src/main.c`) replaces the 5-zone approximation
  for the primary ball: snap ball y to the bat top, `offset =
  next_x+3-BAT_X`, walk the `LABEE`/`LABFC` threshold→zone table,
  optionally double-reflect `dir=((dir^$1F)+1)&$3F`, look up
  `LAC0A[(zone&3)*6 + dir_index]`. Validated vs the Spectrum at five bat
  positions (`make test-bat-deflection`, in `parity-check`); the
  byte-exact L3 gate is unchanged. The "needs a ball-onto-bat scenario"
  blocker was solved by *repositioning the coherent `l3-brick-flash`
  ball* (no new snapshot) — `scripts/capture_bat_deflection.py`.
  The MAGNET catch branch (bonus $03, `LAB1F_1..3`) is also ported: the
  caught offset is quantized (`& 0xFC`, clamp 0x18) and gated on a normal
  bat, so the rest position (x=132 for a centre drop) and the derived
  launch match the Spectrum (validated in `make test-bat-deflection`).
  The held-ball rest y is correct ($A7=167 for a MAGNET-held ball vs
  $A6=166 for the launch rest — the maintainers add 1px when the bat
  carries bonus $03). The multi-ball secondaries (`step_extra_ball`) are
  now UNIFIED with the primary: they use the exact `dir_to_dxdy` + q8.8 +
  `LAFFC` + `bat_deflect_dir` path (not the old integer + 5-zone model), so
  they move/bounce/deflect exactly like the original (one `handling_ball`
  for all balls) — correct-by-construction, primary ball gate byte-exact +
  liveness LIVE. Remaining (minor): the catch->release launch isn't
  separately gate-verified (it's correct-by-construction — the quantized
  offset feeds the already-validated launch formula), and a MAGNET bat
  catches the primary but not yet the unified secondaries. The latter is a
  deliberate project, not a quick fix: the primary stuck system spans ~32
  sites (init/respawn/level-entry/catch/play-loop maintainer/release/
  dirty-redraw), so catching secondaries means mirroring that as a parallel
  per-ball stuck system — substantial, NEW (not correct-by-construction)
  code for the niche MAGNET+TRIPLE_BALL-simultaneous case. Deferred. Full
  decode + validation in `notes/bat-deflection.md`.
- **metal-brick shimmer** is the remaining L3 frame-step residual, now
  fully decoded (see `notes/metal-shimmer.md`; the earlier "sliding
  window" description was wrong). It is a *ball-hit-triggered, then
  permanent* per-brick shimmer: hitting an undestructible brick registers
  it in one of 5 `briks_data` slots (`LAFFC_34`), and `fill_briks_data`/
  `metal_brik_anim` ($B6A9) then cycle it through `anim_brik`'s 8 brick
  sprites ({2,6,3,7,4,5,5,1}, 2 ticks each) forever. The port stubs this
  (`render_brick_flash_to_buff` is a no-op). Cosmetic (no gameplay
  effect), so it's a self-contained render feature; implementation path in
  the note.
- big-bat resize timing is matched visually but not a literal port of
  the original bit-gated state machine.
- **alien-explosion cadence** (cosmetic). `handling_blast_obj` plays the 5
  blast sprites once at 3 ticks/frame; the original's `LAAD2` advances on a
  `$12=$50` timer and `handling_blast` deactivates at frame `$09`, which
  (with only 5 sprites) likely cycles them ~twice. Same 5 sprites, same
  gameplay (alien dies, slot frees); only the explosion's duration/repeat
  may differ. Needs a blast-anim ground-truth capture to pin down.
- **progressive ball speed-up + SLOW semantics** — DONE (2026-06-05; see
  parity-status.md "Ball speed-up + SLOW"). The port now models the
  original's accelerating ball (`$02 → $06` via the `ball+$13`/`$94` ramp
  counter on the 8-frame `counter_misc & 7` cadence) and SLOW as a
  ball-speed reset to `$02` (wears off as the speed climbs back), replacing
  the old fixed-speed + permanent-frame-skip approximation. All gates stay
  green (byte-exact ball gate, 5 visual states, 14/14 bat deflection).
- **enemy sprite animation / facing** (cosmetic). The enemy *motion*,
  *steering* (`LAA7D`), *brick collision* (`LAFFC`), *margins*, and *bomb
  drop* (`bomb_appear`, see parity-status.md) are all ported faithfully,
  but the **sprite-frame selection** is approximate. The port uses a flat
  3-frame timer cycle (`sprite_num = (misc_12 >> 2) % 3`) for both the bird
  and the UFO, and `handling_ufo_obj` simply delegates to
  `handling_bird_obj`. The original is richer: `handling_bird` ($A9BC) runs
  `LAAD2` (timer-based frame advance gated by the `IX+$12` anim state) plus
  `LAA02`, which **mirrors/flips the sprite by flight direction** (the bird
  faces the way it flies — `dir`-derived `XOR $07` / `$0E - frame`), while
  `handling_ufo` ($A902) has a *distinct* tail (no direction-sprite; a
  `flag_2`-gated `LAA7D_1` target re-pick on brick hit). So in the port the
  enemy doesn't visibly face its travel direction and the UFO is not
  byte-distinct from the bird. Cosmetic only (no motion/gameplay effect).
  **GT-measured spec (2026-06-05):** the original's enemy `sprite_num`
  increments **+1 every 4 frames, cycling 0→7** (probed `object_enemy+$01`:
  f8=0,f12=1,f16=2,f20=3,f24=4,f28=5,f32=6 — the `IX+$13=$70` max-nibble 7
  gives an 8-step counter), and `LAA02` maps that 0..7 + flight direction
  onto the **5** sprite frames (`spr_bird_1..5`) with a horizontal mirror.
  The port has only **3** bird frames (`SPR_BIRD_1..3`) and does a flat
  `%3` cycle. Note the port's CADENCE already matches (`misc_12>>2` = +1
  per 4 frames); the gaps are (a) the 2 missing sprites `spr_bird_4/5` (not
  yet extracted into `assets/sprites.bin` refs), and (b) the `LAAD2` 8-step
  range + the `LAA02` direction→frame mapping/mirror. So this is a scoped
  cosmetic sub-project: extract `spr_bird_4/5`, widen the frame table, and
  port the `LAAD2`/`LAA02` index logic. Decode in `notes/enemy-movement.md`.
- **rocket bonus flight** — now fully decoded (`notes/rocket-flight.md`).
  The **motion** is FAITHFUL (`handling_rocket` $A89A accel model + bat
  attach; the port's per-rocket counter is byte-equivalent because
  `counter_misc` is reset to 0 at launch in `LBAED_6`). Two **divergences**
  remain, both long-standing port choices: (1) the port carves a tunnel by
  destroying bricks in a bbox sweep during flight, but the original's
  flight loop (`LBB97`) has **no** brick destruction — the rocket flies
  over intact bricks; (2) the port's `award_left_bricks` clears all
  remaining bricks instantly, but the original's `add_points_for_left_briks`
  ($AF0D) ticks points up **sequentially** (pause + sound per brick) and
  **never clears** the bricks (the level transition does). Fixing both is a
  deliberate sub-project: no rocket-flight ground-truth capture exists yet,
  and `scripts/test_rocket_completion_no_ball.py` encodes the current
  behaviour. Decode + exact plan in `notes/rocket-flight.md`.

These should be compared against ZEsarUX captures if they become
visibly wrong.

### Sound envelopes are approximate

Most queued sound effects follow the original sound IDs and rough
periods, but the PC speaker layer is not a cycle-exact Spectrum beeper
port. The original toggles port `$FE` in tight loops; the DOS port uses
PIT channel 2 square waves and frame-paced queue updates. Envelope
shape can be matched, but exact timbre/duty/timing would need a
sampled audio backend or a much lower-level beeper emulator.

## Render implementation shortcuts

### Frame ornament is captured

`assets/frame_l1.bin` is a captured 4-cycle frame blob. It currently
matches the GT for the covered cycles, but the original builds the
frame from border sprite primitives at runtime. See `notes/shortcuts.md`.

### Level attribute bands are captured

`assets/level_attrs.bin` ships captured per-level attr bands. Brick
body attrs are now written dynamically by the port, but non-brick
cells in the brick band, frame-strip attrs, and pre-dimmed shadow attrs
still come from captured data.

## Test coverage gaps

The current visual regression covers static title/menu/high-score and
early level-entry frames. It does not yet assert mid-game parity for:

- hard-brick first-hit shimmer,
- brick destruction / bonus drop timing,
- all bonus effects,
- enemy/bomb behavior,
- level-clear/rocket flow.

Adding short deterministic ZEsarUX-vs-QEMU gameplay traces would catch
most remaining visible mismatches.
