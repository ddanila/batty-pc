# Original parity gaps

Known places where the current recreation is intentionally approximate
or still depends on captured original data. This is separate from
`known-bugs.md`: these are not necessarily user-reported defects, but
they are good next targets when tightening original fidelity.

## Visible / behavioral

### RNG is not original

The original `random_generate` at `$8EB4` updates `random_number`
from:

- two bytes of state initialized to `$8E17`,
- a `random_seed` pointer walking `$8000..$9FFF`,
- `ctrl_btns_pressed`, which perturbs one byte from player input.

The port currently uses a generic C LCG in `next_random`. This means
the broad probabilities are close, but exact sequences differ for:

- bonus drop / type selection,
- bomb appearance,
- enemy side selection,
- `+400` drift direction,
- alien-blast noise.

This is probably the highest-value remaining behavioral parity task.

### Some motion is approximate, not descriptor-exact

Several paths use gameplay-equivalent but not byte-exact motion:

- enemy movement now uses the original 6-bit direction-table shape and
  q8.8 subpixel coordinates, but target selection/collision steering is
  still simplified versus `LAA7B`, brick/ball collision, and
  `check_margins`,
- triple-ball fan-out uses integer `dx/dy` approximations instead of
  the original 6-bit direction split,
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

### Rocket clear behavior needs capture comparison

The rocket now follows the original high-level loop: it flies upward,
destroys destructible bricks, then awards points for remaining
destructible bricks and clears the round. Its body overlap, spawn
position, and per-tick destruction cadence should still be checked
against original gameplay captures.

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
- death animation,
- level-clear/rocket flow.

Adding short deterministic ZEsarUX-vs-QEMU gameplay traces would catch
most remaining visible mismatches.
