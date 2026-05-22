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

## Commands

```sh
make replay-l3-brick-flash
make replay-l3-brick-flash-both
```

`replay-l3-brick-flash` is a port-side smoke run. It validates replay
timing, QEMU driving, and capture extraction. The Make target rebuilds
the test floppy with `BATTY_LEVEL=3` and `BATTY_START_LEVEL=1` so the
DOS port starts directly in L3 gameplay instead of spending replay time
in title/menu screens. It also enables `BATTY_REPLAY_PROBE=1`, which
causes the DOS port to write `PROBE.TXT` with the L3 entry counters,
RNG bytes, object bytes, and live level copy. The target sets
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
opcodes the original ends up with RNG, bat, ball, and enemy bytes that
match the port's overrides byte-for-byte, so the probe comparison is
now `PASS` on every named state line — including `object_enemy`, which
was the previously-non-deterministic ($40 vs $A8 spawn-column) value.

The harness still does not mark `comparison.aligned_start=true`, so
`--fail-on-diff` remains refused. Captures continue to diverge:
`l3_initial` ≈ 246 / 23040 px (1.07%) and `l3_after` ≈ 81%. Both diffs
come from runtime drift in the wall-clock window between the probe and
the screendump — QEMU and ZEsarUX advance at different effective rates,
so even with identical L3-entry state the alien lands at a slightly
different pixel position 0.8 s later and at a very different state
13 s later (after the ball-release event).

## Next required step

Resolve cross-emulator runtime drift before the captures can be
fail-gated. The two emulators we drive — QEMU (DOS port) and ZEsarUX
(original) — run at independent effective speeds: QEMU is host-bound,
ZEsarUX is real-time-clamped, and neither is frame-locked to the
harness clock. As a result, 0.8 s of wall-clock advances the port and
the original by different numbers of game frames, and the alien sprite
lands at different pixel positions. Three plausible paths:

- **Frame-step both sides.** Replace the harness's wall-clock `at`
  scheduling with explicit "step N PIT frames" / "step N Z80 frames"
  commands, so each capture fires after the same number of game frames
  on each side. Cleanest for parity but requires both DOS-port hooks
  (pause/step on a PIT tick) and ZEsarUX breakpoints on the frame
  interrupt vector.
- **Make the port run-bound too.** Add a port-side env var that pauses
  the main loop until a "go" signal, then runs a fixed number of frames
  before capture. Mirror that on the ZEsarUX side with `run N opcodes`
  per captured beat. Less invasive than full frame-stepping but still
  needs a new pause hook in `run_level`.
- **Compare cycle-equivalent windows.** Split the L3 spec into two
  short-window replays whose captures land within the first few frames
  after probe (where drift is small), and accept that long-window
  captures stay informational. Quickest win, no harness changes; gives
  a real parity gate for level-entry visuals.

Of the three, the third path is the lowest-risk near-term win: it lets
`l3_initial` become a fail-gated parity check while we work on the
infrastructure for the longer-window captures.
