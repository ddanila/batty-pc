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
  same gameplay state and can be treated as a parity gate.

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
drift checks easier to compare across repeated runs.

## Commands

```sh
make replay-l3-entry            # fail-gated L3-entry pixel-exact parity
make replay-l3-brick-flash      # port-side smoke
make replay-l3-brick-flash-both # informational two-runner comparison
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
cleanly. `--fail-on-diff` returns 0 with `l3_entry: 0/23040 px differ`
and `PASS` on every state-probe row.

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
the original's probe at the same point.

`replay-l3-brick-flash-both` also drives ZEsarUX and prints INFO diffs.
The original side starts from the tracked `20260513T202101Z` RAM
snapshot converted to `.sna`, then uses ZRCP setup commands to poke
the level and round counters to L3, pin `random_number` at `$8E17` to
the same `8E49` the port forces, jump to `BA24`, and step the Z80 a
fixed number of opcodes via the harness's `run` op (replaces the
earlier wall-clock `sleep 2.0`, which let the original land at a
non-deterministic PC because emulator speed varies). At one million
opcodes the original ends up with RNG, bat, ball, enemy, brick-hit
animation slots, and level-copy bytes that match the port's probe
byte-for-byte, so the probe comparison is now `PASS` on every named
state line — including `object_enemy`, which was the
previously-non-deterministic ($40 vs $A8 spawn-column) value.

The harness still does not mark `comparison.aligned_start=true`, so
`--fail-on-diff` remains refused. Captures continue to diverge:
`l3_initial` ≈ 246 / 23040 px (1.07%) and `l3_after` ≈ 81%. Both diffs
come from runtime drift in the wall-clock window between the probe and
the screendump — QEMU and ZEsarUX advance at different effective rates,
so even with identical L3-entry state the alien lands at a slightly
different pixel position 0.8 s later and at a very different state
13 s later (after the ball-release event). Use the generated
`build/replay/l3-brick-flash/compare/*-diff.png` files and printed
`bounds=(x0, y0, x1, y1)` values to distinguish small phase drift from
whole-playfield divergence while designing the next pause or frame-step
checkpoint.

## Next required step

Extend the pause-on-both-sides pattern to mid-gameplay captures so the
brick-flash window (`l3_after` style) can be fail-gated too. Two
plausible paths:

- **Frame-step both sides.** Replace the harness's wall-clock `at`
  scheduling with explicit "step N PIT frames" / "step N Z80 frames"
  commands so each capture fires after the same number of game frames
  on each side. Requires a port-side PIT-tick step hook (pause, run
  N frames, pause again) and a matching ZEsarUX breakpoint on the
  Z80 frame-interrupt vector.
- **Trampoline pauses between captures.** Reuse the existing
  `BATTY_REPLAY_WAIT_KEY` mechanism but rearm the port for a second
  pause at a known game-state point (e.g. after the first brick hit),
  and add a matching `run_until_pc` setup op for the original. Less
  invasive than frame-stepping; works well for "checkpoint" captures
  but doesn't help for sweeps that need a continuous timeline.

The first path is the structurally cleaner answer; the second is the
faster way to land a second parity gate while we design the frame-step
hooks.
