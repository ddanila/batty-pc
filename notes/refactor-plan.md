# Module layout

One oversized `main.cpp` turned into modules that each have a fast,
exhaustive host test. Complete.

## Where this stands

**The code.** Fourteen modules extracted, each with its own host suite in
`tests/` — a 1:1 mapping, kept that way by `test-host-tests-wired`. What is
left in `main.cpp` is the game itself: screen states, level flow, and the
frame loop. Its longest function is `run_level`, an orchestrator of named
phases rather than a god-function.

Line counts are deliberately not tracked. The claims worth pinning are the
ones above: every module testable on its own, nothing left in `main.cpp`
that belongs elsewhere.

**The tests.** 110 gates, indexed in `notes/testing.md` and kept complete by
`test-gate-index`:

  - 79 sweep gates (78 QEMU + `test-asan`) —
    `make parity-check-parallel FULL=1`, ~7 min. Without `FULL=1` it runs
    only the fast core, ~100 s.
  - 31 emulator-free source gates plus 14 host suites — `make test-fast`,
    seconds. CI runs exactly this.
  - 3 ZEsarUX-oracle gates — `make parity-check-full`.

**The defects.** Eight surfaced by this refactor (#8-#15) and three more by
playing the game (#20-#22). `notes/known-bugs.md` holds the table.

`scripts/check_notes_numbers.py` pins the figures above on every
`make test-fast`.

## The method

Extract a module → give it a host test → *then* refactor it → gates green →
commit. In that order, so the cleanup has something to fail against. Order by
(test speedup x clarity payoff) ÷ risk.

## Conventions

Modules are **separately compiled** `.cpp` + `.h`. The flat 32-bit model
removed the single-code-segment reason for one translation unit, so the linker
enforces boundaries that used to be convention. A header is a deliberate list;
everything else stays private.

**Comments** sort three ways:

- *Delete* — restatement of the code, and historical narrative.
- *Promote to code* — anything assertable becomes `ZX_STATIC_ASSERT` or a
  named constant.
- *Keep, one line* — provenance, as `// orig: $B684 ix_buf_addr_calc`. This is
  the only link to the reverse-engineered Z80 and cannot be recovered from the
  code.

**Types**: `u8`/`u16`/`u32` from `types.h` in anything the host build also
compiles. Watcom's 32-bit `long` is 4 bytes and a 64-bit host's is 8; a cast
through the wrong one silently doubles a store's width.

## The modules

| module | owns |
|---|---|
| `zxvga` | the video engine — `notes/video-engine.md` |
| `rng` | the per-frame RNG walk |
| `physics` | directions, bat deflection, collision geometry |
| `assets` | tape-derived asset loading |
| `bricks` | the brick compositor and band orchestration |
| `objects` | the 22-byte descriptor and the object slots |
| `weapons` | bullets and blasts |
| `enemies` | enemy steering and the brick-hit walk |
| `bonus_codes` | original ↔ port bonus numbering |
| `hud` | glyphs, markup, score |
| `sound` | the queue and the envelopes |
| `scoring` | points, milestones, side attribution |
| `replay_parse` | `BATTY_REPLAY_*` value formats |
| `replay` | replay / probe scaffolding |

## What is left

1. **Three replay seeders stay in `main.cpp`** — blocked by design, not
   placement. `BATTY_FORCE_SPAWN_BONUS` reaches `pick_bonus_type`, which reads
   seven pieces of live game state, and the two brick seeders need
   `live_level`. Moving either drags game state into a compositor module.
2. **known-bugs #14** is a question about the ORIGINAL, not a port defect, and
   needs a Spectrum.
3. **`notes/parity-gaps.md`** — fidelity work, not refactor work.

## Decisions that still bind

**The bullet's brick damage duplicates `brick_hit_resolve` on purpose.**
Bullets skip the click, because the original tests `sprite_set == $05` and
lets the impact blast be the feedback. Unifying the two would quietly delete
that difference; do it only with the divergence written into whatever replaces
both.

**`auto_advance` is never assigned**, so all three `TIMED_OUT` branches are
permanently false — the attract auto-cycle is intentionally absent. Marking it
`const` makes Watcom emit `W368 always false` and refuse to build under `-we`,
so it stays a plain `static int` with the reason attached.

**The ROCKET catch increments the bonus byte on both bats by hand** rather
than through `set_bat_bonus`, because routing it through the setter would
assume the two are always equal — the assumption `enemy_prepare`'s separate
reads decline to make.

## Known coverage holes

- **No gate sets `BATTY_LEGACY_COLLISION`**, so the pre-LAFFC fallback path is
  unexercised. It boots, but with a stuck ball no collision occurs. Know that
  before trusting it as an A/B baseline.
- **No gate exercises the delta between two visual checkpoints** — every gate
  passes a single one. `test-visual-checkpoints` says so itself.
- **No screendump in the suite has a score on it**; the visual build is
  `-dBATTY_SCORELESS_HUD`. See known-bugs #22.

## Why this was worth doing

known-bugs #8 sat in `main.cpp` for the whole project and surfaced within
minutes of those functions becoming pure and testable. The gates prove the
port still matches the original; they do not make the code answerable to
questions. Pure functions with fast tests do.
