# Modularising main.cpp

Turning one 7,700-line file into modules that each have a fast, exhaustive
test. Started 2026-08-07.

## The method

Extract a module → give it a host test → *then* refactor it → gates green
→ commit. In that order. The test comes before the cleanup so the cleanup
has something to fail against; `zxvga` proved the pattern and every stage
since has followed it.

Order is by (test speedup × clarity payoff) ÷ risk, not by file order.

## Conventions

Modules are **separately compiled** `.cpp` + `.h`. The flat 32-bit model
removed the single-code-segment reason for one translation unit, so the
linker enforces boundaries that were previously convention. What a module
exposes is a deliberate list in its header; everything else stays private.

**Comments** sort into three buckets:

- *Delete* — restatement of the code, and historical narrative ("an
  earlier version…", "this misled a triage"). Naming fixes the first;
  these notes hold the second.
- *Promote to code* — anything assertable becomes `ZX_STATIC_ASSERT` or a
  named constant. Buffer sizes derived from geometry, `sizeof(Object) ==
  22`, the blit's alignment invariant.
- *Keep, one line* — provenance, as `// orig: $B684 ix_buf_addr_calc`.
  This is the only link between the port and the reverse-engineered Z80
  and cannot be recovered from the code.

Long-form rationale lives here in `notes/`, referenced by one line.

**Types**: `u8`/`u16`/`u32` from `types.h` in anything the host build also
compiles. Watcom's 32-bit `long` is 4 bytes and a 64-bit host's is 8; a
cast through the wrong one silently doubles a store's width. That bug
happened, and `make test-video` caught it.

## Stages

| # | Module | Lines | State |
|---|--------|------:|-------|
| — | `zxvga` — video engine | 593 | **done** — 11 tests; own TU since stage 4b |
| 2 | `rng` | 68 | **done** — 4 tests, byte-exact vs the original's walk |
| 3a | `physics` — direction + bat deflection | 217 | **done** — 10 tests vs captured hardware tables |
| 3b | collision geometry/effects split | 166 | **done** — 7 more tests |
| 4 | `assets` | 167 | **done** — 6 tests |
| 5 | level / bricks | 629 | next |
| 6 | entities — objects, enemies, bonuses, weapons | ~950 | |
| 7 | hud + text | 601 | |
| 8 | sound | 272 | |
| 9 | `run_level` decomposition | 684 | |
| 1 | replay / probe scaffolding | 480 | **last** — see below |

`main.cpp`: 7,746 → 7,403 so far. 49 host tests, all under a second.

### Stage 5 is delicate

The brick compositor (`print_one_brik_buf_c`, `brik_shadow_c`,
`print_briks_c`, `render_brick_band`) walks `scr_buff` through raw offsets
that mirror the original's Z80 addressing (`hl`, `hl - 32`, `hl + 2`),
with edge cases keyed to literal column numbers (`col_byte != 1`,
`col_byte != 29`). It is parity-critical and currently guarded only by
10-second QEMU boots.

Do the test first: `build/level_gt/level_NN.scr` holds the original's
captured screen for all 15 levels, so a host test can paint a level's
brick band and diff the band region against it — before touching the
compositor.

### Why replay is last, not first

Its ~480 lines look like the easy win — they are test-only code and
already contiguous (lines ~4410-4894), so nothing is tangled. But
extracting it to a real TU means exposing the game state it pokes
(`objects[]`, bonus/bomb/rocket state) and the ~50 variables
`write_replay_probe` dumps. That needs the game-state API stages 6-9
build, so it is the *hardest* stage, not the easiest.

## What this has already found

`known-bugs.md` #8 — multiball extra balls use a direction convention
mirrored from the primary ball's in two of four quadrants. It sat in
`main.cpp` for the whole project and surfaced within minutes of those
functions becoming pure and testable. Not fixed: which side is right is
unknown without an oracle capture.

That is the argument for the whole exercise. The gates prove the port
still matches the original; they do not make the code answerable to
questions. Pure functions with fast tests do.
