# Replay harness

`scripts/replay_harness.py` runs timestamped replay specs from
`replays/*.json` against the DOS port, the original game in ZEsarUX, or
both.

## Shape

Each replay declares:

- `events`: timestamped key actions (`tap` or `hold`),
- `captures`: timestamped screen captures with optional playfield ROI,
- `port`: QEMU/DOS-port boot inputs,
- `original`: ZEsarUX original-game boot inputs,
- `state_probe`: optional port/original state values to record before
  replay input is applied,
- `comparison.aligned_start`: whether original and port start from the
  same gameplay state and can be treated as a parity gate,
- `comparison.required_probe_rows`: optional state-probe rows that must
  exist on both sides and match even when capture diffs are only INFO.
- `comparison.capture_max_diff_pixels`: optional per-capture pixel-diff
  ceilings. Captures under the ceiling are treated as PASS; captures
  over the ceiling fail the replay.

The harness writes decoded palette-index buffers next to raw captures:

- DOS port: `build/replay/<name>/port/*.ppm` and `*.idx`,
- original: `build/replay/<name>/original/*.scr` and `*.idx`.

If a replay has state probes, each side also writes
`build/replay/<name>/<side>/state_probe.txt`. Port probes can combine
values emitted by the DOS port's `BATTY_REPLAY_PROBE` hook with hashes
of captured ROIs. Original probes read named memory ranges through ZRCP
after the original setup commands run and before replay input begins.

Comparisons are made in RGB palette space, matching `test_visual.py`,
so bright-black and non-bright black remain visually equivalent.
For every differing capture, comparison runs also write a cropped diff
image to `build/replay/<name>/compare/<capture>-diff.png`. Matching
pixels are dimmed for context and mismatches are red. The console line
prints the mismatch bounding box as playfield coordinates, which makes
drift checks easier to compare across repeated runs. Each comparison
also writes `build/replay/<name>/compare/summary.json` with the
aligned-start flag, common probe-row matches, side-only probe rows,
capture diff counts, mismatch bounds, and diff artifact paths.

## Commands

```sh
make replay-l3-entry            # fail-gated L3-entry pixel-exact parity
make replay-l3-brick-flash      # port-side smoke
make replay-l3-brick-flash-both # bounded two-runner gameplay gate
```

`replay-l3-entry` is the first true parity gate. Both runners pause at
the original's main-loop entry (`LB9E8_2 = $BA83`): the port spins on
`kbhit()` after `play_brik_anim` (gated by `BATTY_REPLAY_WAIT_KEY=1`),
the original halts via a ZRCP `run_until_pc` op (`enable-breakpoints`
must precede `set-breakpoint`, learned the hard way). Override env vars
inject the original's probed `$BA83` state into the port's bat / ball /
enemy descriptors so the pause-time state matches byte-for-byte. The
harness fires a single `at=0.1` capture while both sides are still
paused, then an `at=0.5` `enter` tap wakes the port so QEMU can exit
cleanly. The original setup NOPs the `$BA6C` call to
`all_metal_briks_animation_snd`, matching the modded GT capture path and
the port's static level-entry renderer; otherwise the original's
pre-round metal-brick shimmer leaves a different lower brick band even
though the probed object and level bytes match. `--fail-on-diff` returns
0 with `l3_entry: 0/23040 px differ` and `PASS` on every fail-gated
state-probe row.

**Gate semantics corrected (2026-06-11).** The known-bugs "regressed 0 →
1885 px" entry turned out stale on both counts: the SCREEN compares
0/23040 px again (the render was fixed by the intervening campaigns), and
the residual FAIL was 4 *probe-row* mismatches (`object_ball_1/bat_1/
enemy`, `random_number`) that can never match by construction — the
original is probed while paused at `$BA83`, but the port's `PROBE.TXT` is
REWRITTEN by its ESC handler at harness teardown (replay_harness always
sends `esc` so QEMU exits), i.e. after the wake tap let the port play on
(RNG ticked, enemy spawned, stuck ball snapped to `$A6`). The mismatches
GREW as the port got more faithful (per-frame RNG tick default, enemy
spawn, stuck-ball snap) — a faithfulness improvement read as a gate
regression. Fixes: `--fail-on-diff` now fail-gates captures + REQUIRED
probe rows only (volatile rows stay INFO); `l3-entry.json` declares the
stable rows required (`bricks_quantity/current_level/round_number/
current_level_copy`) and probes `random_number` at its REAL address
`$8D48` (it used to read `$8E17` — an echo of the setup pin, which is
where the note's phantom "original=8E49" came from); the port is seeded
with the true f0 RNG (`3793/962A`). Gate green again, screen still
pixel-exact-gated.

`replay-l3-brick-flash` is a port-side smoke run. It validates replay
timing, QEMU driving, and capture extraction. The Make target rebuilds
the test floppy with `BATTY_LEVEL=3` and `BATTY_START_LEVEL=1` so the
DOS port starts directly in L3 gameplay instead of spending replay time
in title/menu screens. It also enables `BATTY_REPLAY_PROBE=1`, which
causes the DOS port to write `PROBE.TXT` with the L3 entry counters,
RNG bytes, object bytes, original-shaped `briks_data`, and live level
copy. The target sets
`BATTY_REPLAY_RANDOM=8E49`, `BATTY_REPLAY_BAT_OBJECT`,
`BATTY_REPLAY_BALL_OBJECT`, and `BATTY_REPLAY_ENEMY_OBJECT` so the
port's RNG and bat/ball/enemy descriptors at L3 entry exactly match
the original's probe at the same point. The ball seed now relies on
the descriptor's direction/speed and +03/+05 fractional bytes rather
than the old `BATTY_REPLAY_BALL_VEL` integer-velocity override; that
keeps the primary ball in play through this replay instead of dropping
and respawning on the bat.

`replay-l3-brick-flash-both` also drives ZEsarUX. It is a stable
fail-gated replay, but not yet an exact moving-object parity replay.
The gate compares stable rows (`bricks_quantity`, `current_level`,
`round_number`) and evaluates side-specific probe assertions: the DOS
port must award score, advance RNG, and mark destroyed `$13` bricks in
`current_level_copy`; the original active L3 buffer at `$6E43` must also
contain destroyed `$13` cells and a nonzero `briks_data` interaction
state. Moving-object rows and the exact destroyed-cell positions remain
diagnostic because the current seed exposes the next real mismatch:
the two runners destroy the same count of bricks but not the same cells.
The original side starts from the tracked `20260513T202101Z` RAM
snapshot converted to `.sna`, then uses ZRCP setup commands to poke
the level and round counters to L3, pin `random_number` at `$8E17` to
the same `8E49` the port forces, NOP the `$BA6C` metal-brick shimmer
call to match the static level-entry setup, jump to `BA24`, and run to
the `$BA83` main-loop breakpoint. The harness clears that temporary
breakpoint and re-enters CPU-step immediately; otherwise the original
either retraps at `$BA83` forever or drifts before the initial capture.
The port uses
`BATTY_REPLAY_WAIT_KEY=1`, captures `l3_initial` while paused, then
wakes with ENTER before the SPACE release. The later `l3_after` capture
still uses wall-clock scheduling and remains a bounded drift gate rather
than a pixel-exact parity gate. Use the generated
`build/replay/l3-brick-flash/compare/*-diff.png` files and printed
`bounds=(x0, y0, x1, y1)` values to distinguish small phase drift from
whole-playfield divergence while designing the next pause or frame-step
checkpoint.

## Next required step

Fix the remaining seeded brick-collision mask mismatch surfaced by the
stable gate. The primary ball now uses descriptor direction/speed and
stays in flight, but both runners still destroy different `$13` cells:
the DOS port marks row 5/6 col 6 while the original active level marks
row 4 col 7 and row 5 col 6. The next parity step is a closer port of
`LAFFC`'s neighbor-bit direction mask and cell-selection priority, then
promote `current_level_copy` and the relevant object row from INFO to
required equality. Two plausible paths for the capture timing remain:

- **Frame-step both sides.** Replace the harness's wall-clock `at`
  scheduling with explicit "step N PIT frames" / "step N Z80 frames"
  commands so each capture fires after the same number of game frames
  on each side. Requires a port-side PIT-tick step hook (pause, run
  N frames, pause again) and a matching ZEsarUX breakpoint on the
  Z80 frame-interrupt vector.

  **Port side landed (2026-06-04).** `BATTY_VISUAL_PROBE_FRAMES` now
  accepts a comma-separated list of *ascending absolute frame indices*
  (e.g. `30,60,90`) instead of a single count. The port runs to each
  checkpoint in turn, writes `PROBE.TXT`, then halts on `kbhit()` so
  the harness can grab a deterministic, drift-free capture; the wake
  key resumes play toward the next checkpoint, and the port quits after
  the last one so QEMU exits cleanly. A single value (`90`) reproduces
  the original single-shot behaviour exactly, so existing callers are
  unaffected (the 5-checkpoint + per-level static regression still
  passes). Values not strictly greater than their predecessor are
  dropped, and the list is capped at `VISUAL_PROBE_MAX` (16) entries.
  This is the port half of the frame-step sweep: a single boot can now
  yield a *timeline* of byte-deterministic mid-game frames rather than
  one capture-and-quit.

  **Capture driver landed (2026-06-04).** `scripts/capture_frame_timeline.py`
  drives the port through those checkpoints: boot, screendump while the
  port is halted, `sendkey ret` to wake it toward the next checkpoint,
  sleep the inter-checkpoint frame delta, repeat. It reuses
  `test_visual.py`'s QEMU drive + PPM→palette-index decode, writes
  `build/frame_timeline/frame_NNNN.{ppm,idx}`, and reports the pixel
  delta between consecutive captures. `make capture-timeline
  FRAMES=150,250,350 LEVEL=1` builds the gameplay floppy with the probe
  env and runs it. Verified end-to-end: with checkpoints spanning the
  192-frame stuck-ball auto-launch, captures advance 317 then 356 px,
  confirming the port halts at every checkpoint and the simulation
  steps the exact frame delta between halts (`--require-motion` makes
  that a hard assertion; a static ball-on-bat scene is otherwise valid).

  **Original side landed (2026-06-04).**
  `scripts/capture_frame_timeline_original.py` frame-steps the Z80 in
  ZEsarUX the same checkpoint frame counts and dumps each `.scr`. Frame
  boundary: the main-loop top `LB9E8_2 = $BA83` is reached once per
  50 Hz frame, so "advance one frame" == run until PC hits `$BA83`
  again; to count N frames it steps one opcode off `$BA83` (so the
  breakpoint doesn't retrap at zero opcodes) and runs until it re-traps,
  N times. `--setup-from-replay <spec.json>` reuses
  `replay_harness.apply_original_setup` to poke level/RNG and jump into
  active play first (the raw L3 `.sna` free-runs through the IM1 vector
  `$0038` but never reaches `$BA83` and stays visually static until the
  `$BA24` setup runs). `make capture-timeline-original` runs it against
  the tracked L3 snapshot with the `l3-brick-flash` setup and
  `--require-motion`; verified the timeline advances (≈600–800 px over
  the first 5 frames, then ≈220 px over the next 5).

  Both halves now exist. **Open: start-frame alignment.** The
  original-side per-step pixel deltas vary run-to-run (e.g. frame 0→5
  measured 775 px then 663 px on consecutive runs). The frame *stepping*
  is deterministic — N frames is always N `$BA83` trips — so the
  variance is in *where frame 0 lands* relative to the ball's sub-frame
  state after the wall-clock `boot_wait` + setup. Before the two
  timelines can be diffed as a parity gate, both sides must start from a
  byte-identical object state on the same frame (as `replay-l3-entry`
  already achieves for its single paused capture). Pinning that — drive
  the original setup to a fixed `$BA83` trip count rather than a
  wall-clock settle, and seed the port with the same probed descriptors
  — is the next step, after which a frame-by-frame `.idx` diff of the
  two timelines becomes the real gameplay parity gate that unblocks
  porting `handling_ball`'s exact 64-direction motion.

  **Comparison + combined gate landed (2026-06-04).**
  `scripts/compare_timelines.py` diffs a port timeline dir against an
  original one frame-for-frame in palette-index space, with an optional
  `--roi x0,y0,x1,y1` (the established gameplay ROI is `8,32,248,128` =
  the brick-play region the `replay-l3-entry` gate uses). `make
  capture-timeline-both` wires the whole sweep: build the L3 floppy
  seeded to the original's probed `$BA83` descriptors (same overrides as
  `replay-l3-brick-flash`), capture the port timeline, capture the
  original timeline, diff in the ROI. Each timeline is reproducible
  run-to-run (verified: two back-to-back original captures produced
  byte-identical `frame_*.idx`).

  **First quantified port-vs-original gameplay diff** (frames 1/3/5):
  full-frame ≈ 4361 / 4493 / 4677 px; brick-ROI ≈ 1917 / 2022 / 2205 px,
  spread across the whole ROI. That is far more than one frame of ball
  drift, so it is *not yet* a ball-physics signal — it is dominated by
  two alignment gaps the gate now makes measurable:

  1. **Start frame not byte-aligned.** The port timeline counts visual
     probe frames from gameplay start; the original counts `$BA83` trips
     from the post-setup point. They are phase-offset, so "frame 1" is
     not the same game instant on both sides. Fix: start the port from
     the proven `BATTY_REPLAY_WAIT_KEY` pause (== `$BA83`) and capture
     that as frame 0, so both share the `replay-l3-entry` aligned start
     (0 px on the ROI) before stepping.
  2. **Metal-brick shimmer out of phase.** `replay-l3-entry` NOPs
     `all_metal_briks_animation` (`$BA6C`) to reach 0 px; once gameplay
     runs, that shimmer animates brick cells across the whole field and,
     if its phase differs between sides, paints ≈2000 px of ROI diff
     unrelated to the ball. Fix: NOP it on both sides, or align its
     frame phase, so the residual diff is purely moving-object.

  Only after both are pinned does the residual ROI diff become the
  `handling_ball` / `LAFFC` divergence signal we actually want to drive
  to zero. The gate and its numbers now exist to measure that work.

  **Aligned start landed + a seeded-entry render gap found (2026-06-04).**
  `capture_frame_timeline.py --wait-key` now captures the port's
  `BATTY_REPLAY_WAIT_KEY` main-loop-entry pause as frame 0, and
  `make capture-timeline-both` wakes from it so the port's frame counts
  match the original's `$BA83` trips. With that aligned start, the
  comparison surfaced a sharper, more useful fact than expected:

  - `orig frame0` is **byte-identical to the canonical L3 GT**
    (`build/level_gt/level_03.scr`, 0 px in the ROI),
  - `port frame0` differs from that same GT by **1568 px**,
  - the phase-offset matrix (port[n] vs orig[m] for n,m in 0..5) has its
    minimum at (0,0); **no frame shift aligns them**, so this is not a
    timing offset — it is a content difference present at the very first
    frame.

  So the port, when seeded with the `replay-l3-brick-flash` descriptors,
  paints something in the brick band at seeded entry that canonical L3
  (and the original) does not. The diff is structured along brick rows
  (per-row bands of ≈6–26 px with spikes of 90–164 px at row
  boundaries), i.e. a brick-field render/state difference, not a compact
  ball/enemy sprite.

  **RESOLVED — it was a comparison bug, the gate works (2026-06-04).**
  The ≈1568 px above was an artifact of `compare_timelines.py` diffing
  **raw palette indices** instead of **RGB palette space**: bright-black
  (index 8) and non-bright black (index 0) both render as `(0,0,0)` but
  are different indices, so the whole black background counted as a diff.
  `test_visual.py` / `replay_harness.py` always compared in RGB space
  (where the two blacks are equal); `compare_timelines.py` now does too
  (`_IDX_RGB` mapping). With that fixed, the bisection findings above
  collapse: the seed, the metal-brick reveal (`play_brik_anim` ends on
  the same pixels — `BATTY_SKIP_BRIK_ANIM` had zero effect and was
  dropped), and the magnet band were all bright-black/black noise, not
  real divergence.

  The frame-step gate (`make capture-timeline-both`, with `--wait-key`
  aligned start) now reports, in RGB space, port vs original:

  - **frame 0: 0 / 23040 px — PASS** (clean byte-aligned start),
  - frame 1: 363 px, frame 3: 473 px, frame 5: 653 px — a small,
    **growing residual localized to bounds ≈(88–191, 32–89)**, the upper
    brick/ball region (the magnet band y=104–128 is clean).

  That growing, localized residual is exactly the `handling_ball` /
  `LAFFC` ball-physics divergence the whole sweep was built to isolate:
  the port's 5-zone/integer ball motion vs the original's 64-direction
  q8.8 motion, diverging frame by frame from an identical start. The
  gate "FAILs" at `--max-diff 0` by design — that number IS the parity
  signal to drive to zero by porting the exact ball motion. The
  milestone (a frame-exact port-vs-original gameplay gate with a 0 px
  aligned start) is reached.

  **Gate cleaned to isolate ball/brick collision (2026-06-04).** The
  residual had two x-clusters: the ball (x≈108, from the seed's
  `BALL_OBJECT` x=$6C) and a second at x≈164. The second was the port's
  seeded `ENEMY_OBJECT` painting an enemy the original snapshot does not
  have active in these frames (its probed enemy descriptor is inactive,
  and frame 0 stays 0 px with the seed removed). Dropping
  `BATTY_REPLAY_ENEMY_OBJECT` from `L3_SEED_ENV` removed that confounder:

  - frame 1: 363 → **212** px, bounds collapse to a compact box at the
    ball `(104,65,127,89)`,
  - frame 3: 473 → 333, frame 5: 653 → 494.

  The remaining residual is purely the ball interacting with nearby
  bricks, growing frame by frame from the 0 px start — a clean
  `handling_ball` / `LAFFC` signal. Pixel classification of the residual
  is mostly *both-sides-ink, different colour* plus presence diffs, i.e.
  a brick near the ball changes on one side and not the other: the ball
  starts at y=$4E (78) already inside the brick band, so the divergence
  is **brick-collision cell/timing** (the port's `brick_collision` vs the
  original `LAFFC` neighbour-bit mask), not ball *motion* (now exact via
  `dir_to_dxdy`). Porting `LAFFC` is the next step to drive 212→0.
- **Trampoline pauses between captures.** Reuse the existing
  `BATTY_REPLAY_WAIT_KEY` mechanism but rearm the port for a second
  pause at a known game-state point (e.g. after the first brick hit),
  and add a matching `run_until_pc` setup op for the original. Less
  invasive than frame-stepping; works well for "checkpoint" captures
  but doesn't help for sweeps that need a continuous timeline.

The first path is the structurally cleaner answer; the second is the
faster way to land a second parity gate while we design the frame-step
hooks.
