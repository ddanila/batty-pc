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
0 with `l3_entry: 0/23040 px differ` and `PASS` on every state-probe
row.

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

  Still open: the original side. A matching ZEsarUX driver must step the
  Z80 the same frame counts (breakpoint on the frame-interrupt vector,
  or `run N frames`) and dump each `.scr`, so the two timelines can be
  diffed frame-for-frame. That promotes the sweep from a port-side
  regression baseline to a true original-vs-port gameplay parity gate.
- **Trampoline pauses between captures.** Reuse the existing
  `BATTY_REPLAY_WAIT_KEY` mechanism but rearm the port for a second
  pause at a known game-state point (e.g. after the first brick hit),
  and add a matching `run_until_pc` setup op for the original. Less
  invasive than frame-stepping; works well for "checkpoint" captures
  but doesn't help for sweeps that need a continuous timeline.

The first path is the structurally cleaner answer; the second is the
faster way to land a second parity gate while we design the frame-step
hooks.
