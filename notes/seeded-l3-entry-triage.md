# Seeded L3-entry render gap (frame-step gate triage)

## Symptom

With the `replay-l3-brick-flash` seed (ball in flight + enemy + RNG),
the port's frame-0 brick-band render (ROI `8,32,248,128`) differs from
the canonical L3 ground truth (`build/level_gt/level_03.scr`) by
**1568 px**, while the original side's frame 0 is byte-identical to that
GT (0 px). Surfaced by `make capture-timeline-both` (see
`notes/replay-harness.md`).

It is **not** a frame-step or ball-motion bug, and **not** a timing
offset: the port[n]-vs-orig[m] diff matrix (n,m ∈ 0..5) is minimised at
(0,0), so no frame shift aligns the two. The difference exists at the
first captured frame.

The per-level static regression passes L3 at 0 px because it captures
the **default stuck-ball entry**, not this seed — so the gap is specific
to the in-flight-ball + enemy seed.

## Shape of the diff

Localised by row inside the ROI (port frame0 vs L3 GT):

- steady bands of ≈6–26 px on most brick rows (y≈32..120),
- spikes of 90–164 px at brick-row boundaries (e.g. y=48, y=72).

That is a brick-field-wide render/state difference, not a compact
ball/enemy sprite footprint. Candidate causes, to bisect:

1. **Metal-brick shimmer state.** The original `l3-brick-flash` setup
   NOPs `all_metal_briks_animation` (`$BA6C`); if the port paints a
   metal-brick shimmer frame at seeded entry, it would diff field-wide.
2. **Seeded enemy (`BATTY_REPLAY_ENEMY_OBJECT`).** A UFO/bird painted by
   the port but not yet painted by the original at its pre-paint `$BA83`.
3. **In-flight ball + secondary balls** painted by the port at entry.

## Next step

Bisect by dropping seed fields one at a time and re-running
`capture-timeline-both` (frame 0 only): build the port with the seed
minus `ENEMY_OBJECT`, then minus the in-flight ball, etc., and watch the
frame-0 vs-GT number. Whichever removal collapses the 1568 px is the
culprit. Then decide whether to (a) match the original's capture phase
(capture the original after its per-frame `print_obj_from_buf_to_scr`
paint, not at the pre-paint `$BA83` loop top, so both show painted
objects), or (b) suppress the offending element on both sides for the
physics-only gate.
