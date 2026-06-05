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
- **alien-explosion cadence** — FIXED (2026-06-05). GT capture (poke the
  enemy to the blast state `set=$0A, misc_12=$50, misc_13=$90` and step)
  showed `anim_alien_blast` is a 10-entry PING-PONG `{1,2,3,4,5,4,3,2,1,1}`
  with sprite_num advancing **+1 every 2 frames** (misc_12 toggles
  `$50`↔`$10`) and `handling_blast` deactivating at frame 9 (~20 frames).
  The port played only the 5-frame expand once at 3 ticks/frame. Fixed:
  `spr_blast_frames` is now the 10-entry ping-pong, `BLAST_FRAMES=10`,
  `BLAST_TICKS_PER_FRAME=2`. Port sprite_num now matches the GT (f0=0,
  f2=1, f4=2, f6=3 — +1/2 frames). (The earlier "only 5 sprites would
  overflow" worry was from mis-reading anim_bird's tail; the real
  `anim_alien_blast` table ping-pongs the 5 sprites over 10 entries.)
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
  **PARTIALLY FIXED (2026-06-05):** ported the 8-step ping-pong. Added
  `SPR_BIRD_4` ($8722) + `SPR_BIRD_5` ($8778) and made `spr_bird_frames`
  the 8-entry ping-pong `{1,2,3,4,3,2,1,5}` indexed by `(misc_12>>2) & 7`
  (the GT cycle). The bird now does the correct 5-sprite wing-flap (was a
  3-sprite cycle); the cadence (+1/4 frames) already matched. The UFO
  ($08) render keeps `% 3`. **DONE — there is NO mirror to port
  (2026-06-05).** The `LAA02` "direction mirror" is **dead code** in the
  original: it computes a remapped sprite_num (`$0E - sprite_num` or
  `(sprite_num^7)+7`) but **never stores it** — `LA9BC_5` immediately does
  `LD A,(flag_2)`, overwriting `A`, with no `LD (IX+$01),A`. So the bird
  never visually flips by facing. The GT confirms it: sprite_num
  increments smoothly 4→5→6 across the f28 hemisphere flip (dir→0x2C
  repick) with no remap jump. So the 8-entry ping-pong IS the complete
  bird animation; `LAA02`'s only live effect is its tail (`flag_2`-gated
  `LAA7D_1` target re-pick, already handled). **Phase also fixed
  (2026-06-05):** `sprite_num` is now an INDEPENDENT counter (init 0, +1
  every 4 frames on `misc_12 & 3`) rather than `(misc_12>>2)&7`, so it
  starts at 0 like the original instead of mid-loop at 4. Now **byte-exact
  vs the GT**: port sprite_num f8=0, f12=1, f16=2, f24=4 = the original's
  walk. So the bird animation is fully byte-exact (cycle + cadence +
  phase). (Not added to the auto-gate: it shares the ±1 boot-phase jitter
  that the x-coord does, so it'd be flaky; verified manually.) Decode + GT
  in `notes/enemy-movement.md`.
  **UFO ($08) also done (2026-06-05):** `anim_ufo` ($789E) is the same
  8-step ping-pong over 6 sprites — `{1,2,3,4,5,6,5,4}`. Added
  `SPR_UFO_4/5/6` ($84C4/$852C/$859A — growing-height UFO frames) and made
  `spr_ufo_frames` the 8-entry table indexed by the shared `sprite_num &
  7`. Table-derived (no runtime GT — the L3 snapshot has a bird, not a
  UFO — but the `anim_ufo` table IS the authoritative frame mapping), and
  the UFO's phase is actually correct (its `prop_even[1]=$60` → sprite_num
  0 at spawn). No mirror needed (the `LAA02` remap is dead code — see the
  bird entry above). So both enemy animations are now complete.
- **rocket bonus flight** — now fully decoded (`notes/rocket-flight.md`).
  The **motion** is FAITHFUL (`handling_rocket` $A89A accel model + bat
  attach; the port's per-rocket counter is byte-equivalent because
  `counter_misc` is reset to 0 at launch in `LBAED_6`).
  **(1) Flight — FIXED (2026-06-05):** removed the in-flight bbox tunnel
  sweep in `step_rocket`, so the rocket now flies over INTACT bricks like
  the original's destruction-free `LBB97` loop (the dirty redraw restores
  the bricks behind the rocket from `scr_buff`). Verified by
  `make test-rocket-flight-redraw` (redraw == full flush),
  `test-rocket-completion-no-ball`, and `test-rocket-bonus` — all green
  (level still advances at fly-off via `award_left_bricks`).
  **(2) End-award — DONE (2026-06-05, user-greenlit).** Replaced the
  instant `award_left_bricks` with `play_rocket_award_tally` (port of
  `add_points_for_left_briks` $AF0D): it sweeps the grid row-major and ticks
  each remaining brick's points up one PIT tick at a time with the scene +
  counting score on screen and the **bricks still visible**, then clears
  them so `live_bricks_remaining()==0` advances the level. The original's
  per-brick `pause_short` is a CPU busy-wait (Z80-clock-bound,
  unreproducible), so the port paces one brick per PIT tick — the same
  visible count-up, timing not byte-exact. Verified: `test-rocket-
  completion-no-ball`, `test-rocket-flight-redraw`, `test-rocket-bonus`,
  `test-midgame-brick-replay` all green (level advances, no hang/stale
  pixels). With Change A (intact flight) + Change B (sequential tally) the
  rocket-clear now fully matches the original's behaviour.

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
