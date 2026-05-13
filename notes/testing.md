# Visual regression test

Once the original screen content is reproducible by *our* renderer, we
lock it in as a pixel-identical regression test. Same pattern
[`generaly`](https://github.com/ddanila/generaly) uses for its
12-checkpoint diff against the original BASIC.

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

| State | Source     | Expected snapshot          | Notes                              |
|-------|------------|----------------------------|------------------------------------|
| 1     | `HISCORE.BIN` static blit  | `20260513T202038Z`  | The bitmap decoded from the snapshot itself — sanity baseline. |
| 2     | C markup renderer           | `20260513T202038Z`  | Programmatic re-render. The real test of the toolchain. |
| 3     | `MAINMENU.BIN` static blit  | `20260513T202041Z`  | Snap2 (main menu). |

All three pass pixel-identical (49 152 px each).

## What the test does *not* yet cover

- Anything dynamic (gameplay). For that we'll need a **replay file**:
  `(tick_N, key)` pairs driving both ZEsarUX (via ZRCP) and QEMU (via
  `sendkey`) in lockstep, snapshot/compare at fixed checkpoints. The
  original is deterministic — same RNG state + same inputs = same
  output — so this approach should work once we wire up
  frame-synchronised input.
- RAM-state diffs. Even more diagnostic than pixel diffs: when our
  recreation owns named game-state addresses, we can compare flat byte
  ranges between our DOS run and the ZX run and pinpoint *exactly*
  which byte diverged. Useful once we get into physics / collision /
  AI work.
