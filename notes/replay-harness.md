# Replay and frame-step harness

`scripts/replay_harness.py` runs timestamped replay specs from
`replays/*.json` against the DOS port, the original in ZEsarUX, or both.
The frame-step layer built on top of it is what makes byte-exact gameplay
parity measurable at all.

## Replay spec shape

Each replay declares:

- `events` — timestamped key actions (`tap` or `hold`);
- `captures` — timestamped screen captures with an optional playfield ROI;
- `port` / `original` — boot inputs for each runner;
- `state_probe` — port/original state values recorded before replay input;
- `comparison.aligned_start` — whether both sides start from the same
  gameplay state, i.e. whether this is a parity gate;
- `comparison.required_probe_rows` — probe rows that must exist on both
  sides and match even when capture diffs are only INFO;
- `comparison.capture_max_diff_pixels` — per-capture pixel ceilings.

Outputs land in `build/replay/<name>/`: `port/*.ppm` + `*.idx`,
`original/*.scr` + `*.idx`, `<side>/state_probe.txt`, and for every
differing capture a cropped `compare/<capture>-diff.png` (matches dimmed,
mismatches red) plus `compare/summary.json`. The console prints the
mismatch bounding box in playfield coordinates.

Port probes combine the DOS port's `BATTY_REPLAY_PROBE` output with hashes
of captured ROIs; original probes read named memory ranges over ZRCP after
setup and before input.

**Comparisons are in RGB palette space**, matching `test_visual.py`, so
bright-black (index 8) and non-bright black (index 0) stay equivalent. This
is not a detail: `compare_timelines.py` originally diffed raw palette
indices and reported a phantom 1568 px "content difference at frame 0",
which sent a whole bisection after the seed, the brick reveal and the
magnet band. All of it was black-on-black.

## Commands

```sh
make replay-l3-entry            # fail-gated L3-entry pixel-exact parity
make replay-l3-brick-flash      # port-side smoke
make replay-l3-brick-flash-both # two-runner gameplay gate
make capture-timeline           # port timeline only
make capture-timeline-original  # original timeline only
make capture-timeline-both      # both, plus the ROI diff
```

`make test-frame-step` is the gated form of the last one and asserts the
documented per-frame floor (`notes/metal-shimmer.md`).

## `replay-l3-entry` — the aligned start

Both runners pause at the original's main-loop entry (`LB9E8_2 = $BA83`):
the port spins on `kbhit()` after `play_brik_anim` under
`BATTY_REPLAY_WAIT_KEY=1`, the original halts on a ZRCP `run_until_pc`.
Override env vars inject the original's probed `$BA83` state into the port's
bat / ball / enemy descriptors so the pause-time state matches
byte-for-byte. A single `at=0.1` capture fires while both are paused, then
an `at=0.5` `enter` tap wakes the port so QEMU exits cleanly.

The original setup NOPs the `$BA6C` call to
`all_metal_briks_animation_snd`, matching the modded GT capture path and
the port's static level-entry renderer — otherwise the pre-round shimmer
leaves a different lower brick band even though every probed byte matches.

`--fail-on-diff` reports `l3_entry: 0/23040 px differ`.

**It fail-gates captures and REQUIRED probe rows only.** Volatile rows stay
INFO, and that distinction was earned: four probe rows (`object_ball_1`,
`bat_1`, `enemy`, `random_number`) can never match by construction, because
the original is probed while paused at `$BA83` while the port's `PROBE.TXT`
is REWRITTEN by its ESC handler at teardown — after the wake tap let the
port play on. Those mismatches GREW as the port got more faithful (the
per-frame RNG tick, the enemy spawn, the stuck-ball snap), so a
faithfulness improvement read as a gate regression. `l3-entry.json` declares
the stable rows required (`bricks_quantity`, `current_level`,
`round_number`, `current_level_copy`) and probes `random_number` at its REAL
address `$8D48`.

## Frame-stepping both runners

**Port side.** `BATTY_VISUAL_PROBE_FRAMES` takes a comma-separated list of
ascending ABSOLUTE frame indices (`30,60,90`). The port runs to each in
turn, writes `PROBE.TXT`, then halts on `kbhit()` so the harness can grab a
drift-free capture; the wake key resumes it toward the next, and it quits
after the last so QEMU exits. A single value reproduces the old
capture-and-quit behaviour exactly. Values not strictly greater than their
predecessor are dropped; the list caps at `VISUAL_PROBE_MAX` (16).

`scripts/capture_frame_timeline.py` drives it — boot, screendump while
halted, `sendkey ret`, sleep the inter-checkpoint delta, repeat — reusing
`test_visual.py`'s QEMU drive and PPM decode. `--wait-key` captures the
`BATTY_REPLAY_WAIT_KEY` pause as frame 0, which is what puts the port on the
original's `$BA83` phase. `--require-motion` makes "the capture advanced" a
hard assertion.

**Original side.** `scripts/capture_frame_timeline_original.py` frame-steps
the Z80 in ZEsarUX. `$BA83` is reached once per 50 Hz frame, so "advance one
frame" is "run until PC hits `$BA83` again"; to count N frames it steps one
opcode off `$BA83` first, so the breakpoint does not retrap at zero opcodes.
`--setup-from-replay <spec.json>` pokes level and RNG and jumps into active
play, which is required — the raw L3 `.sna` free-runs through the IM1 vector
but never reaches `$BA83` and stays visually static until the `$BA24` setup
runs.

**Use `--frame-pc 0x0038` for anything touching the bat.** `$BA83` is
skipped whenever the game branches into the bat or ball-lost paths; the IM1
vector fires once per frame regardless. It samples at a different point in
the frame, so a byte comparison needs the port aligned to that phase too.
See `notes/bat-deflection.md`.

**Comparison.** `scripts/compare_timelines.py` diffs a port timeline
directory against an original one frame-for-frame, with `--roi
x0,y0,x1,y1` (the established gameplay ROI is `8,32,248,128`) and
`--budgets` for per-frame ceilings. Each timeline is reproducible run to
run — two back-to-back original captures produce byte-identical
`frame_*.idx`.

### Two confounders the gate had to have removed

**Seeded objects the other side does not have.** Dropping
`BATTY_REPLAY_ENEMY_OBJECT` from `L3_SEED_ENV` collapsed frame 1 from 363
to 212 px and shrank the bounds to a compact box at the ball. The port was
painting a mid-flight enemy the original's fresh alien was nowhere near.
Note the reason, which is not the one first written down: the L3 original
DOES have an active enemy (`sprite_set = $09`, probed) — it is just
descending at y=1..28, ABOVE the ROI, through the compared frames. A
seeded MID-flight enemy at y=27 drops into the ROI and mismatches. For an
enemy-specific gate, seed the same FRESH y=1 descriptor instead
(`notes/bird-render-parity.md`).

**The RNG seed.** The stale seed made the port drop a spurious SLOW bonus
whose falling letters polluted the brick ROI. `notes/rng-model.md`.

## `replay-l3-brick-flash` and `-both`

The port-side run validates replay timing, QEMU driving and capture
extraction. The Make target rebuilds the floppy with `BATTY_LEVEL=3` and
`BATTY_START_LEVEL=1` so the port starts directly in L3 gameplay, and
enables `BATTY_REPLAY_PROBE=1` so it writes `PROBE.TXT` with the entry
counters, RNG bytes, object bytes, original-shaped `briks_data` and the live
level copy. `BATTY_REPLAY_BAT_OBJECT` / `_BALL_OBJECT` / `_ENEMY_OBJECT`
match the original's probe at the same point. The ball seed relies on the
descriptor's direction, speed and fractional bytes rather than an integer
velocity override, which is what keeps the ball in play instead of dropping
and respawning it on the bat.

`-both` also drives ZEsarUX and is fail-gated on `bricks_quantity`,
`current_level`, `round_number` and `current_level_copy` — the destroyed
`$13` cells now match on both sides, which was the last thing this gate was
waiting for. The original side starts from the tracked `20260513T202101Z`
snapshot converted to `.sna`, then uses ZRCP setup to poke the level and
round counters, NOP the `$BA6C` shimmer call, jump to `$BA24`, and run to
the `$BA83` breakpoint. The harness clears that temporary breakpoint and
re-enters CPU-step immediately; otherwise the original either retraps at
`$BA83` for ever or drifts before the initial capture.

Its later `l3_after` capture still uses wall-clock scheduling and remains a
bounded drift gate rather than a pixel-exact one. `make test-frame-step` is
the pixel-exact gate.

## Two ZEsarUX traps

**`enable-breakpoints` must precede `set-breakpoint`**, or the set silently
fails and `run` sails past the target PC.

**An incoherent poke hangs the game outright.** Poking three bytes of a
22-byte object descriptor and leaving the rest produces a state that spins
with interrupts disabled — ~5M opcodes without the IM1 vector even firing.
That looks exactly like a bad frame boundary and is not: reposition a
coherent descriptor instead (`notes/bat-deflection.md`).
