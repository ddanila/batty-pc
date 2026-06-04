# Original parity gaps

Known places where the current recreation is intentionally approximate
or still depends on captured original data. This is separate from
`known-bugs.md`: these are not necessarily user-reported defects, but
they are good next targets when tightening original fidelity.

## Visible / behavioral

### Some motion is approximate, not descriptor-exact

Several paths use gameplay-equivalent but not byte-exact motion:

- enemy movement now uses the original 6-bit direction-table shape and
  q8.8 subpixel coordinates, but target selection/collision steering is
  still simplified versus `LAA7B`, brick/ball collision, and
  `check_margins`,
- **brick/ball collision** — DONE. The `LAFFC` port (cell-find incl. the
  LAFFC_5-6 down/down-right straddle + neighbour-open-face mask +
  direction gate + `change_direction` reflect + fraction-preserving snap +
  penetration corner case) is now the **default** for the primary ball
  and is byte-exact vs the Spectrum over L3's 150-frame trajectory
  (`make test-laffc-ball-frame1`). `BATTY_LEGACY_COLLISION=1` reverts.
  Full decode + status in `notes/laffc-decode.md`.
- **bat-ball deflection** — the next gameplay-parity port, now **decoded
  and ground-truthed** (see `notes/bat-deflection.md`). The port uses a
  5-zone approximation (`step_ball`/`step_extra_ball` set dir to one of
  ~5 fixed values by hit zone); the original derives it in `LAB1F`
  ($AB1F): snap ball y to $A6, `offset = ball_x+3-bat_x`, walk the
  `LABEE`/`LABFC` threshold→zone table, optionally reflect
  `dir=((dir^$1F)+1)&$3F`, then look up `LAC0A[(zone&3)*6 + dir_index]`
  (plus a MAGNET catch branch for bonus $03). The "needs a ball-onto-bat
  scenario" blocker is **solved**: repositioning the coherent
  `l3-brick-flash` ball just above the bat (not poking a placeholder)
  drops it onto the bat without a new snapshot —
  `scripts/capture_bat_deflection.py` captures the deflection table.
  Remaining: port `LAB1F` literally (hand-derivation diverges — the bit2
  zones double-reflect) and gate it against the captured table.
- **brick-hit shimmer** is now the remaining frame-step residual (shared
  by both collision paths via `brick_hit_anim`). The original
  `metal_brik_anim` (`$B6A9`) slides a 16-byte window into `anim_brik`
  (2 bytes/frame, per-slot counter); the port uses discrete reordered
  frames. Porting the sliding-window shimmer is the next gate-closing
  step (see `notes/laffc-decode.md` Update 6).
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
