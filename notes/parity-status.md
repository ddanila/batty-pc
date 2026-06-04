# Gameplay parity status

Definitive snapshot of where "100% visual frame parity in gameplay with
the original" stands, and exactly what's left. See `replay-harness.md`
and `laffc-decode.md` for the detailed trail.

> **CORRECTION (2026-06-04, see laffc-decode.md Update 13).** The
> "byte-exact collision, 40-frame stable" claims below were overstated.
> The ball is byte-exact at **frame 1**, but on L3 it **diverges by frame
> 5**: the port misses a *horizontal/side* brick bounce the original
> makes (at f5 both balls are at (112,64) but orig dir=0x21 vs port
> dir=0x3F). The frame-step gate's sparse frames (0/1/3/5) plus the ball
> position still matching at f5 hid it (pixels matched, only direction
> differed). So LAFFC's **vertical** bounce is exact; its **side** bounce
> is missed/mis-handled. Read the "Done" section with that caveat — the
> *vertical*-bounce + motion chain is byte-exact; full collision is not.

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
- **Brick collision** — `LAFFC` port (cell-find, open-face neighbour
  mask, direction gate, `change_direction` reflect, penetration corner
  case) behind `BATTY_LAFFC=1`. On L3 the hit cell, bounce axis, snapped
  position, and direction are all byte-exact; stable over 40 frames
  (`make gate-laffc-long`).
- **Regression guards** — `make test-laffc-ball-frame1` (ZEsarUX-free)
  locks the L3 frame-1 ball to the Spectrum probe; the 5-checkpoint +
  per-level static suite (`make test`) stays green.
- **Safety** — the `BATTY_LAFFC` path falls back to `brick_collision`
  when LAFFC reports no hit, so it can never pass a brick through.

## Remaining (each needs a decision or new data — not blocked on more analysis)

1. **Cosmetic: brick-hit render.** The only frame-by-frame residual on
   the L3 gate is the just-hit brick's render (the damaged multi-hit
   frame and/or the `briks_data` cyan shimmer). Position and attr are
   correct; it's a sprite-frame/timing detail, shared by both collision
   paths, that has resisted repeated investigation. Worth it only if
   pixel-perfect *mid-game* frames are the goal.

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

## Bottom line

The hard core of the parity goal — exact ball motion and brick collision
— is achieved, gate-verified on L3, and regression-locked. Extending
verified parity to other scenarios is gated on capturing aligned
original snapshots, not on more porting or analysis of the existing one.
