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

The test exercises the full attract-mode flow (TITLE → MENU → HISCORE)
on the `batty-test.img` floppy, which sets `BATTYALL=1` in AUTOEXEC so
the C side disables auto-advance — every transition is driven by
`sendkey ret` from the Python harness.

| State | Renderer in C            | Expected snapshot                  | Notes                                                                       |
|-------|--------------------------|------------------------------------|-----------------------------------------------------------------------------|
| 1     | `LOADING.BIN` static blit | `original/Batty.scr`              | Title / loading screen, decoded from the tape's screen$ block.              |
| 2     | Markup + sprites + blink   | `20260513T202041Z` (snap2)        | Main menu rendered from `MENUMARK.BIN` + indicators + bottom sprites.       |
| 3     | Markup hi-score            | `20260513T202038Z` (snap1)        | Hi-score table rendered from `MARKUP.BIN`.                                  |
| 4     | Full level-1 gameplay paint | `build/level_gt/level_01.scr` (modded-tape GT, *post-first-paint*) | Bricks + frame + bat + ball + lives. Diff is `INFO` — currently 427 px, all in the bat band. |
| 5     | Same captured frame, ROI'd to bat band (y=160..192) | same GT | Sub-diff of state4 — surfaces bat-render regressions on their own so they don't hide inside the whole-frame number. |

States 1–3 pass pixel-identical. State 4 and state 5 report the same
427-px diff under different denominators — same data, sharper framing
for state 5 (5.2% of the bat band differs vs 0.9% of the whole frame).

## INFO is for accepted drift, not unmeasured surface

State 4's old GT was captured *before* the gameplay loop drew bat /
ball / lives — so the renderer's bat sat in a regression-test blind
spot. Diff stayed at ~228 px (the bat-overlay overhead), and the README
rationalized it as "the absolute floor without recapturing the GT mid-
render". A green check on a metric that excluded the surface under
iteration. Fixed by:

- `scripts/build_modded_batty.py` now patches line 6261 (`CALL
  restore_objs_and_magnet` → `JR $`) so the spin trap fires *after*
  the first gameplay-loop iter has painted bat / ball / lives into
  scr_buff and flushed to VRAM. New trap PC: **0xBB61**.
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
The natural next step is a **replay file**: `(tick_N, key)` pairs
driving both ZEsarUX (via ZRCP) and QEMU (via `sendkey`) in lockstep,
snapshot/compare at fixed checkpoints. The original is deterministic
(same RNG state + same inputs = same output), so this works once
frame-synchronised input is wired up. Ad-hoc smoke scripts under
`scripts/exercise_*.py` cover individual scenarios in the meantime.
