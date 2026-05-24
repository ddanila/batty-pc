# Visual regression test

Once the original screen content is reproducible by *our* renderer, we
lock it in as a pixel-identical regression test.

## How `make test` works

1. **Build the floppy** via the normal Makefile path.
2. **Launch QEMU headless** with `-display none -monitor stdio`. The
   Python harness drives the monitor: sleep for DOS boot, `screendump`
   the framebuffer to a PPM, `sendkey` to advance our state machine,
   repeat, `quit`.
3. **Decode the captured PPM** for each checkpoint:
   - QEMU emits mode-13h frames at 2× scale (640×400, each VGA pixel
     doubled in both dimensions for aspect correction).
   - DAC scaling: `DAC=56 → 224`, `DAC=63 → 255` (plain `<<2` for
     non-bright, max-out for bright). Not the textbook `(v<<2)|(v>>4)`.
   - Sample one pixel per VGA cell, look up the RGB in our 16-entry ZX
     palette → palette-index byte.
4. **Build the expected indices** from the matching ZEsarUX snapshot's
   `screen.scr` via `extract_scr.py` — gives the same palette-index
   buffer.
5. **Compare in RGB space, not index space.** Indices 0 and 8 both
   render as `(0,0,0)` (non-bright vs bright black) — visually identical
   but `extract_scr` emits each per the ZX attr's bright bit. Comparing
   `PALETTE_RGB[a] == PALETTE_RGB[e]` makes them equivalent.
6. **Diff PNG on failure.** Mismatches are saved to
   `build/test_visual/<checkpoint>_diff.png` (red where pixels disagree,
   grey for background context).

## Current checkpoints

The test exercises the full attract-mode flow (TITLE → MENU → HISCORE → LEVEL)
on the `batty-test.img` floppy, which sets `BATTYALL=1` in AUTOEXEC so
the C side disables auto-advance — every transition is driven by
`sendkey ret` from the Python harness.

| State | Renderer in C            | Expected snapshot                  | Notes                                                                       |
|-------|--------------------------|------------------------------------|-----------------------------------------------------------------------------|
| 1     | `LOADING.BIN` static blit | `original/Batty.scr`              | Title / loading screen, decoded from the tape's screen$ block.              |
| 2     | Markup + sprites + blink   | `20260513T202041Z` (snap2)        | Main menu rendered from `MENUMARK.BIN` + indicators + bottom sprites.       |
| 3     | Markup hi-score            | `20260513T202038Z` (snap1)        | Hi-score table rendered from `MARKUP.BIN`.                                  |
| 4     | Full level-N gameplay paint | `build/level_gt/level_NN.scr` (modded-tape GT, *post-first-paint*) | Bricks + frame + bat + ball + lives + magnets. Pixel-identical for all 15 levels via `BATTY_LEVEL=N`. |
| 5     | Same captured frame, ROI'd to bat band (y=160..192) | same GT | Sub-diff of state4 — surfaces bat-render regressions on their own so they don't hide inside the whole-frame number. |

All five states are FAIL-gated on L1 default. All 15 level-entry
captures are pixel-identical via `BATTY_LEVEL=N` — see
[`per-level-profile.md`](per-level-profile.md).

`make test-brick-flash` drives a dynamic L3 gameplay path and fails if
the bright-white brick destruction flash remains after it should clear,
or if no brick-sized cell stays visibly removed after the hit. The
stale-flash decision is reference-derived: the test compares each brick
cell's bright-white coverage against the original-captured L3 render in
`build/level_gt/level_03.scr`, allowing only a small margin above the
original brick art. That catches the dirty-line white-block failure and
the stale-static-background failure without hard-coding that every white
pixel is wrong.

`make test-rocket-bonus` is a source-level regression for the rocket /
next-level bonus. The original main loop checks `object_rocket` before
`balls_quantity` (`LBAED -> LBAED_6`), so catching the rocket can hide
all balls while the level-clear sequence runs without entering the
bat-death path. The test fails if the port's no-ball death guard stops
excluding `rocket_active`.

`make test-death-sparks` is a source-level regression for the bat death
fanout. It locks the port to the original `LBC10` spawn constants
(`$1B` direction seed, `$05` direction step, `$AE` Y, speed `$02`,
`bat_x + body_width/2 - $0C`, 3 px X spacing) and `bounce_wall`
reflection thresholds, especially the right wall clamp at `$F8 -
spark_body_width`. It also checks the post-spark `pause_long B=$03`
hold before the life is decremented and the bat respawns.

`make test-midgame-brick-replay` is the first fail-gated dynamic replay
state test. It seeds L3 with an in-flight ball near destructible bricks,
runs the DOS port through the replay harness, then checks the extracted
post-run probe: brick count must drop below the seeded `$1A`, score must
increase, RNG must advance, and the level copy must contain a destroyed
`$13` brick marker. `make replay-l3-brick-flash-both` also runs the
original side for comparison, but those moving-object rows remain
informational until the replay gains a frame-step synchronization point.

`make test-hud` is a separate normal-build check because `make test`
uses `BATTY_SCORELESS_HUD`. It boots the regular floppy to L1 and
compares the stable original HUD regions (`1UP` / `HI` / `2UP`, player
1 zero score, player 2 zero score) against the original
`20260513T202101Z` capture. The high-score digits are intentionally
excluded because `HISCORE.DAT` can vary between local runs.

## Per-level testing via `BATTY_LEVEL` env

```sh
BATTY_LEVEL=9 make test    # builds floppy with SET BATTY_LEVEL=9 in AUTOEXEC,
                           # boots into level 9 directly, diffs against L9 GT
```

The `BATTY_LEVEL=N` env var:
- Makefile injects `SET BATTY_LEVEL=N` into the test floppy's AUTOEXEC.BAT
  (the bytes don't change with env, so the `test` target also `rm -f`s the
  floppy first to force a rebuild on env changes).
- Makefile also passes `BATTY_START_LEVEL=1` through for replay targets;
  when set, the DOS port starts directly in `ST_LEVEL` after asset load.
- `src/main.c` `getenv("BATTY_LEVEL")` in `run_level` sets
  `round_number = N-1` so the run-level loop enters at level N.
- `scripts/test_visual.py` switches `state4_level1`'s expected snapshot
  to `build/level_gt/level_NN.scr` (default = L1).

## INFO is for accepted drift, not unmeasured surface

State 4's *original* GT was captured before the gameplay loop drew bat /
ball / lives — so the renderer's bat sat in a regression-test blind
spot. Diff stayed at ~228 px (the bat-overlay overhead), rationalized
as "the absolute floor without recapturing the GT mid-render". A green
check on a metric that excluded the surface under iteration. Fixed by:

- `scripts/build_modded_batty.py` patches line 6261 (`CALL
  restore_objs_and_magnet` → `JR $`) so the spin trap fires *after*
  the first gameplay-loop iter has painted bat / ball / lives into
  scr_buff and flushed to VRAM. Trap PC: **0xBB61**.
- `scripts/test_visual.py` adds `state5_bat_band` — same captured
  frame, ROI'd to `y=160..192`. ROI-only checkpoints reuse another
  state's PPM via the `source_label` field.

**Rule of thumb.** A residual diff that's always the same shape (a
band, a sprite, a strip) is a signal that the metric is excluding the
surface where you're iterating. Either:

- recapture a GT that covers it, or
- split it into its own ROI checkpoint with its own number.

`INFO` should mean "we accept this drift while we focus elsewhere",
not "the test can't see this region". The latter is just an alibi.

### Determinism for the menu checkpoint

The menu has an active blink: by default `selected_mode = 1` (= "1 -
1 PLAYER") and that line strobes white ↔ invisible at ~4.5 Hz. snap2
was captured during the BLACK half (selected row's 11 attr cells at
`0x58AE..0x58B8` are all `0x00`).

To keep the test pixel-identical regardless of capture timing, the C
helper `blink_phase()` pins the phase to 0 (BLACK) when `auto_advance`
is off (= the test mode signalled by `BATTYALL`). `make run`'s floppy
leaves `BATTYALL` unset and the user sees the natural blink.

## What the test does *not* yet cover

Mid-game frames. State 4 is a single level-init paint; nothing
exercises ball physics / collisions / bonus drops under parity.
The natural next step is replay files: timestamped key events driving
both ZEsarUX (via ZRCP) and QEMU (via `sendkey`), then
snapshot/compare at fixed checkpoints. The original is deterministic
(same RNG state + same inputs = same output), so these become parity
gates once a replay starts both runners from an aligned state. Ad-hoc
smoke scripts under `scripts/exercise_*.py` cover individual scenarios
in the meantime.

The first reusable replay harness is in `scripts/replay_harness.py`;
see [`replay-harness.md`](replay-harness.md). It currently supports
DOS-port and ZEsarUX-original runs plus INFO comparisons. Replays only
become fail-gated once their spec marks the original and port start
states as aligned.
