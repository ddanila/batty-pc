# Seeded L3-entry render gap (frame-step gate triage)

## Symptom

The frame-step parity gate (`make capture-timeline-both`,
`notes/replay-harness.md`) cannot reach 0 px at frame 0: the port's
WAIT_KEY-pause frame-0 brick-band render (ROI `8,32,248,128`) differs
from the canonical L3 ground truth (`build/level_gt/level_03.scr`) by
**1568 px**, while the original side's frame 0 is byte-identical to that
GT (0 px).

## What it is NOT (ruled out 2026-06-04)

- **Not the replay seed.** Bisected by dropping seed fields one at a
  time (`ENEMY_OBJECT`, then in-flight ball, then everything): the
  port's frame-0 vs-GT diff stays **1568 px even with no seed at all**
  (just `BATTY_LEVEL=3` + WAIT_KEY). So `BALL_OBJECT` / `ENEMY_OBJECT` /
  `BALL_STUCK` are not the cause.
- **Not a frame-step / timing offset.** The port[n]-vs-orig[m] diff
  matrix (n,m ∈ 0..5) is minimised at (0,0); no frame shift aligns them.
- **Not ball motion.** The diff is present at the first frame, before
  the ball has moved.

## What it IS

The 1568 px decomposes (port frame-0 vs L3 GT) as:

- **bricks, y=32..104: 1129 px** — a *transient* brick state.
  `play_brik_anim()` (`src/main.c`) runs the 8-step metal-brick reveal
  and **leaves its last animation frame on screen**; the static brick
  repaint only happens in the first main-loop iteration *after* the
  WAIT_KEY pause. So the pause captures bricks mid/post-reveal, not
  settled. `BATTY_LEVEL=3 make test` gets 0 px vs the same GT precisely
  because it captures *after* the field settles, not at the pause.
- **magnets, y=104..128: 439 px** — the magnet ON/OFF blink state
  differs. `render_level_screen` (called twice at level entry, 6107 +
  6109) calls `render_magnets` (`src/main.c:2836`), whose blink uses
  `next_random()` (`src/main.c:2671`) unless `test_mode_pin_blink`
  (`BATTYALL=1`) forces the deterministic `i>=2` pattern — which itself
  need not match the original's actual blink at entry.

The original side shows the *settled* bricks at `$BA83` (its metal-brick
animation is NOP'd via the `$BA6C` setup poke), hence == GT. So the
port's WAIT_KEY pause (transient) and the original's `$BA83` (settled)
are not the same render state even though both are "main-loop entry".

## Related regression: replay-l3-entry no longer 0 px

`make replay-l3-entry` — documented as `0/23040 px` — now **FAILS at
1885 px**, diff bounds `(8,104,248,128)` (the magnet/lower-brick band).
Its state probes show the port RNG has diverged: `random_number
port=A187 vs original=8E49`, and `object_enemy` / `object_ball_1` /
`object_bat_1` differ (INFO). The RNG divergence is consistent with the
entry-path `render_magnets` -> `next_random` consumption above; the
original's RNG stays pinned at 8E49 because its shimmer is NOP'd. Logged
in `notes/known-bugs.md`.

## Next step

Two independent fixes, both real parity work:

1. **Settle the brick field before the parity capture.** Either repaint
   the static bricks after `play_brik_anim` (so the WAIT_KEY pause shows
   the settled field, matching the original's `$BA83`), or capture the
   port one main-loop paint later. Confirm against the original's actual
   per-frame paint phase (`print_obj_from_buf_to_scr`) so both sides
   capture the same point in the frame cycle.
2. **Pin / match the entry RNG + magnet blink.** Find why the port
   consumes RNG before the pause that the original (NOP'd) does not, and
   either pin it or make the magnet blink deterministic-and-matching, so
   `replay-l3-entry` returns to 0 px. That restores the aligned start
   the frame-step gate depends on.

Only after the frame-0 baseline is ~0 does the residual growing diff
become the `handling_ball` / `LAFFC` signal.
