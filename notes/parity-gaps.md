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
- **brick/ball collision** is the active parity target: the frame-step
  gate (`make capture-timeline-both`) isolates a ~212 px frame-1 residual
  to the ball-vs-brick bounce. The port's `brick_collision` picks the
  first non-destroyed cell in scan order + an overlap-axis rule, while
  the original `LAFFC` finds the cell at the ball's position, builds a
  neighbour-bit solidity mask, gates it by ball direction, and bounces
  off the shallower-penetrated solid side. Full decode + port plan in
  `notes/laffc-decode.md`.
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
