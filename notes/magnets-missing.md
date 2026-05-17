# Magnets — unimplemented feature, blocks L2+ visual parity

Found while extending the test to per-level checkpoints (`BATTY_LEVEL`
env): L2 has a **magnet** sprite at the top-centre of the brick
formation. Our renderer doesn't draw it. Diff: 271 px concentrated at
y=44..70 (the magnet's vertical span).

## What's missing

- `original/disasm/routines/magnets.asm` defines:
  - `spr_magnet_circle_off` at `$78AC` (3 bytes × 23 rows = 140 B incl
    header) — base magnet sprite.
  - `spr_magnet_circle_on` at `$7938` (4 bytes × 30 rows = 242 B) —
    "lit" overlay drawn on top per a per-magnet random check.
  - Per-level `magnet_level_NN` records: `[count, x0, y0, x1, y1, …]`.
  - `print_magnets` walks the level's record, paints all-off, then
    50% of the time stacks an on at `(x, y+5)`.

Both sprite addresses are *outside* the `sprites.bin` range
(`$7A8C..$8F50`) we currently ship, so they'd need to come in via a
new asset file similar to `spr_bomb_data` (which is also out-of-range,
shipped inline in `src/main.c`).

## Per-level magnet positions (from the disasm)

| Level | Count | Positions (x, y)                       |
|-------|-------|----------------------------------------|
| L1    |     0 | (uses `magnet_level_03` = empty)       |
| L2    |     1 | (116, 44)                              |
| L3    |     0 |                                        |
| L4    |     1 | (116, 124)                             |
| L5    |     0 |                                        |
| L6    |     3 | (116, 16), (72, 115), (160, 115)       |
| L7    |     2 | (48, 92), (216, 92)                    |
| L8    |     3 | (116, 24), (76, 116), (156, 116)       |
| L9    |     4 | (64, 60), (168, 60), (84, 108), (148, 108) |
| L10   |     1 | (116, 68)                              |
| L11   |     2 | (92, 132), (140, 132)                  |
| L12   |     3 | (116, 8), (32, 68), (200, 68)          |
| L13   |     4 | (16, 32), (216, 32), (24, 108), (208, 108) |
| L14   |     4 | (140, 36), (196, 36), (140, 100), (196, 100) |
| L15   |     2 | (76, 130), (156, 130)                  |

L1, L3, L5 have NO magnets — so the existing state4_level1 checkpoint
can't catch this. L2 (= what `BATTY_LEVEL=2 make test` would exercise)
catches it immediately with a 271 px diff.

## Implementation sketch (for the next iter)

1. **Asset.** Either ship the two sprite bodies inline in `src/main.c`
   like `spr_bomb_data` (382 B total) OR add an `assets/magnets.bin`
   that bundles both with their headers.
2. **Per-level positions.** Hard-code the table above as a `static
   const` 16-entry array. Each entry: `{ count, max_4_xy_pairs }`.
3. **Render hook.** Call `render_magnets(level_idx)` from
   `render_level_screen` AFTER `render_brick_band` (the magnet sits
   inside the brick band) but BEFORE the bat/ball/lives paints — so
   later moving sprites still composite on top.
4. **Off + on stack.** Per magnet: blit off at (x, y), then blit on at
   (x, y+5). The original's 50% on/off depends on the live RNG; for
   visual parity at the deterministic test capture, always paint both
   (matches the GT's snapshot moment, where the on was painted —
   verified by the 271-px diff falling entirely in the magnet's bbox).

Expected outcome: L2 state-equivalent checkpoint drops to ~0 px diff.

## Test infrastructure ready

`src/main.c` now honours `BATTY_LEVEL=N` (1..15) — sets the initial
`round_number = N-1` so the level-entry render is for level N instead
of level 1. Use it from a test floppy with
`SET BATTY_LEVEL=2` in `AUTOEXEC.BAT`. Quick recipe:

```sh
cp build/batty-test.img /tmp/batty_l2.img
printf '@ECHO OFF\r\nSET BATTYALL=1\r\nSET BATTY_LEVEL=2\r\nBATTY\r\n' \
    > /tmp/AUTOEXEC.BAT
mcopy -i /tmp/batty_l2.img -o /tmp/AUTOEXEC.BAT ::AUTOEXEC.BAT
python3 scripts/test_visual.py --floppy /tmp/batty_l2.img --out /tmp/level2_test
```

Currently reports state4 vs the L1 GT (huge diff, expected). For a
fair L2 diff, compare the captured PPM against
`build/level_gt/level_02.scr` — that's the 271-px finding above.
