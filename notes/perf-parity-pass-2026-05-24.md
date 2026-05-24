# Performance/parity pass 2026-05-24

Scope: continue the profiling-driven speed work, fix one more original
behavior mismatch, and add regression coverage for both.

## Performance finding

Brick disappearance was fixed by marking the static background cache
dirty when `live_level` changes. That was correct, but the first fix
rebuilt the whole static level image on every brick destruction.

The faster path now rebuilds only the brick-band cache slice
(`BRICK_BAND_Y_TOP..BRICK_BAND_Y_BOT`, byte columns 1..30) after a brick
is destroyed, then restores the previous moving-object dirty ranges from
the updated cache. This keeps the hit path from repainting HUD, frame,
magnet and full background content just to remove one brick.

## Parity finding

The bat death fanout was still slightly different from the original
`LBC10` path:

- sparks were spawned from the 32 px sprite/shadow width rather than the
  object body width (`IX+$0C = $1C`),
- right-wall reflection clamped too far right instead of using
  `check_right_margin`'s `$F8 - IX+$0C`,
- the post-spark `pause_long B=$03` hold before `lives--` was missing.

The port now uses the original body-width spawn, original right-wall
clamp for 8 px sparks, and the 45-PIT-tick post-spark hold.

## Regression coverage

- `make test-brick-flash` now asserts that a brick-sized cell remains
  removed after the flash clears.
- `make test-death-sparks` locks the original `LBC10` spawn constants,
  `bounce_wall` reflection shape, and post-spark pause.
