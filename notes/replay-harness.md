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
- `comparison.aligned_start`: whether original and port start from the
  same gameplay state and can be treated as a parity gate.

The harness writes decoded palette-index buffers next to raw captures:

- DOS port: `build/replay/<name>/port/*.ppm` and `*.idx`,
- original: `build/replay/<name>/original/*.scr` and `*.idx`.

Comparisons are made in RGB palette space, matching `test_visual.py`,
so bright-black and non-bright black remain visually equivalent.

## Commands

```sh
make replay-l3-brick-flash
make replay-l3-brick-flash-both
```

`replay-l3-brick-flash` is a port-side smoke run. It validates replay
timing, QEMU driving, and capture extraction.

`replay-l3-brick-flash-both` also drives ZEsarUX and prints INFO diffs.
It is not a parity gate yet because the checked-in original snapshot is
not aligned to the DOS port's L3 test-mode route. The harness refuses
`--fail-on-diff` unless a replay marks `comparison.aligned_start=true`.

## Next required step

Capture or generate an original-game start snapshot aligned to a replay
fixture. For L3 brick/bonus work, that means an original state at the
same L3 entry point, same RNG state, same ball/bat/object state, and the
same input route as the DOS test floppy. Once that exists, update the
replay's `original.snapshot`, mark `comparison.aligned_start=true`, and
promote the replay to a fail-gated target.
