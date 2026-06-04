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
  byte-exact path, as expected). Multi-ball secondaries still use
  `brick_collision` (integer motion, not yet ported to the q8.8 + LAFFC
  model — would need a multi-ball original snapshot to validate).
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

## Bottom line

The hard core of the parity goal — exact ball motion and brick collision
— is achieved, gate-verified on L3, and regression-locked. Extending
verified parity to other scenarios is gated on capturing aligned
original snapshots, not on more porting or analysis of the existing one.
