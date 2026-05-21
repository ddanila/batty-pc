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
RNG bytes, object bytes, and live level copy.

`replay-l3-brick-flash-both` also drives ZEsarUX and prints INFO diffs.
The original side starts from the tracked `20260513T202101Z` RAM
snapshot converted to `.sna`, then uses ZRCP setup commands to poke the
level and round counters to L3 and jump through the original level-init
path. The current probe comparison proves that the brick count, current
level, round number, and level byte copy match; it also shows that RNG
and ball/bat object bytes still differ. It is not a parity gate yet
because the exact frame-sync point still needs to be verified. The
harness refuses `--fail-on-diff` unless a replay marks
`comparison.aligned_start=true`.

## Next required step

Verify and lock an original frame-sync point for the L3 replay. For L3
brick/bonus work, that means proving the ZRCP setup leaves original and
port at the same L3 entry point, same RNG state, and same
ball/bat/object state before the first capture. Once that is proven,
mark `comparison.aligned_start=true` and promote the replay to a
fail-gated target.
