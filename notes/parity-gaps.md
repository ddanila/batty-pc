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
  The multi-ball secondaries (`step_extra_ball`) are now UNIFIED with the
  primary: they use the exact `dir_to_dxdy` + q8.8 + `LAFFC` +
  `bat_deflect_dir` path (not the old integer + 5-zone model), so they
  move/bounce/deflect exactly like the original (one `handling_ball` for
  all balls) — correct-by-construction, primary ball gate byte-exact +
  liveness LIVE. Remaining: the catch held-ball rest y is 1px off
  ($A6 vs $A7, cosmetic) and the catch->release launch isn't yet
  gate-verified. Full decode + validation in `notes/bat-deflection.md`.
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
