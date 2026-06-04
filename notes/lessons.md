# Lessons learned

Accumulated wisdom from past iterations on this project. Each entry is a
specific class of mistake to watch for — readable in isolation, with
enough "why" that the rule survives without its incident.

## Audit post-processing when changing pipelines

**Rule.** When you change how data is acquired, every post-processing
step that "cleaned up" assumptions of the old pipeline is suspect.
Re-justify each one against the new pipeline's properties before
re-running.

**Why.** During the Batty GT capture work we burned a full session
iterating on snap-load / hard_reset / fresh-emulator-per-level / NOP-
this-and-that to fix "missing magnets on L4". Every iteration produced
the same "magnet missing" output. The actual cause was
`scripts/clean_gts.py`, a leftover from the original snap3-based
pipeline. It wiped pixel bytes in y=120..160 (mid-playfield band) to
clear snap3's mid-game debris. Once we switched to fresh-tape-boot-per-
level captures, that band was clean and contained the magnet sprite —
but clean_gts still ran in the workflow and erased it. The capture
script had been correct the whole time; the renderer was looking at
post-processed `.scr` files that had been wiped.

**How to apply.** Whenever you rip out an upstream stage and replace
it, list every downstream "cleanup" / "normalize" / "fixup" step and
ask: *was this step compensating for a defect that no longer exists?*
If yes, remove it before debugging output mismatches. Symptoms that
should trigger this audit:

- "The captures look right standalone but wrong after the workflow runs"
- Output that differs depending on whether you run a single step or the
  whole chain
- Post-step diagnostics showing data that was present pre-step is gone

When introducing a new pipeline stage, ship it as a separate PR that
*removes* the now-obsolete cleanup rather than running both side-by-side.

## Trapping a watchpoint doesn't tell you what the routine does

**Rule.** A VRAM-watchpoint hit gives you a PC, not a purpose. If two
routines share an inner blitter, your trap may land in the wrong one.
Confirm the routine's role by tracing where it's *called from*, not
just what it writes to.

**Why.** We watched writes to `0xE800` during level-1 startup and
trapped at `0xAD8F`. We named it "the brick-field blitter" in
`notes/sprites.md` (Phase A1) and built a whole narrative around it:
multi-pass compositor, sprite cache as pre-shifted bricks, "days of
Z80 RE" to port. When we finally read the named disasm, `0xAD8F` was
`all_metal_briks_frame` — the metal-brick *shimmer animation*, called
once-per-N-frames from a side path, not the level paint at all. The
actual level paint (`print_briks` at `0xADE1`) shares the same inner
8-row × 2-byte blit, which is why the trap looked plausible.

The mistake cost us ~3.5 KB of "sprite cache" theorising and a
shortcut-bypass for L1 (shipping `brick_bitmaps.bin`) when the real
routine was ~30 lines we could port directly. The disasm replacement
of "Days" with "hours" was almost entirely this single misread.

**How to apply.** When a watchpoint traps, before naming the routine:

- Look at the call site — what *caller* enters this PC? Is it a
  level-init path, a per-frame path, or a periodic animation? The
  named disasm or even a cycle counter on the watchpoint hits will
  tell you.
- Check the routine's neighbours in the same file: an *animation*
  driver often sits next to a *paint* driver with a similar inner
  blit. Don't assume the trap landed in the one you expected.
- When two routines share inner code, name the inner code by its
  shape (`blit_16x8`) and the outer drivers by their *purpose*
  (`paint_bricks` vs `animate_metal_bricks`). Avoid letting the
  outer name leak from a wrong assumption about what called it.

Symptoms that should trigger this audit:

- The routine you trapped is much simpler than the behaviour it's
  supposed to drive.
- The "cache" it reads from is mostly populated with one obvious
  asset class (here: bricks) and the rest is dark — that's an
  animation cache, not a sprite library.
- You're inventing multi-pass / SMC / sentinel-pointer machinery to
  explain how this routine produces the observed pixels. The real
  routine probably doesn't.

## Don't let your test exclude the surface you're iterating on

**Rule.** A visual-regression checkpoint that compares against a GT
captured with the very surface you care about *removed* gives you a
green light over a blind spot. Catch this whenever a residual diff is
explained by "the GT can't show this region".

**Why.** state4_level1 sat at ~228 px diff for many iterations of
bat / ball / lives rendering. The README explained the residual as
"the bat + ball + lives overlay over a bat-free GT snapshot — the
absolute floor without recapturing the GT mid-render." That phrasing
*sounded* principled — and it was technically true — but the practical
effect was that bat sprite, bat Y position, ball start position, and
lives indicator drawing were all in a regression-test blind spot.
After bumping the modded-batty spin trap so the GT *does* contain
bat / ball / lives (PC 0xBB61, after the first paint+flush), the same
checkpoint immediately surfaced 427 px of real drift — most of it
shape / position differences in the bat.

**How to apply.** Whenever you write or accept a residual-diff
rationalization that mentions a specific shape ("residual is the bat",
"residual is the HUD strip", "the GT doesn't capture X"), one of two
things must follow:

- recapture the GT so it does include X, or
- split that region into its own ROI checkpoint with its own number,
  so regressions in X don't hide inside the whole-frame metric.

If the answer is "neither, because we can't easily capture X" — that's
exactly when you have a blind spot, and the test is misleadingly green
for the surface you're iterating on. Fix the capture pipeline (a few
lines of patch + re-baseline) before doing more renderer work in that
region. `INFO` is for accepted drift, not unmeasured surface.

## Read the sprite-ID table before writing per-slot draw logic

**Rule.** When the original game dispatches sprites via a numbered
table, look at the *table* before assuming the ID numbers map to
labels in the obvious order. The names in `gfx_screen_elements` are
ordered by entry, not by sprite-data layout.

**Why.** Iter-21 added a "magnet ON-coin pin" for test mode — slots 0/1
draw ON, slots 2-3 draw OFF only. The implementation drew "OFF first,
ON conditional on top". Worked for the levels that happen to have
balanced ON/OFF densities. For 3-4-magnet levels it produced
wrong-direction residuals because the C variable named
`spr_magnet_off` was being drawn first when the original draws sprite
**$06** first — and `gfx_screen_elements[$06]` is
`spr_magnet_circle_ON`, not OFF. The names overlap with the sprite-
data labels but the table index drives the actual draw order.

Iter-34 flipped the order ("ON first, OFF conditional second") and
dropped total residual 1383 → 660 px in one commit. Three previously-
failing levels (L8, L13, L14) flipped to pixel-perfect. Later
inner-border/frame-order and top-frame cache fixes moved the profile
to 15/15 levels pixel-perfect.

**How to apply.** When porting a sprite-dispatch routine:

- Read `gfx_screen_elements` (or equivalent) and write down which
  sprite_num maps to which sprite-data label.
- Cross-check by looking at what bytes the SMC patches modify — for
  magnets it patches `spr_magnet_circle_on+1` (the height field of
  what gets drawn first), which only makes sense if ON is the always-
  drawn sprite and OFF is the conditional overlay.
- If the C-port variable names don't match the slot intent, rename
  them; otherwise it'll look correct but render the wrong sprite.

## Compare your assembled binary against the reference

**Rule.** When the disasm has a `tools/batty_for_compare.sna` (or
equivalent reference image), assemble your patched version and diff
the two. Any unexpected non-patch byte differences are bugs in the
assembler or your patch set.

**Why.** An earlier iter documented "the disasm formula doesn't match
GT; the C-port formula does" — implying the disasm was unreliable.
Iter-35 verified our `build/modded_batty/batty.sna` differs from
`original/disasm/tools/batty_for_compare.sna` at exactly 35 bytes,
all matching our `PATCHES` list in `build_modded_batty.py`. The
non-patch code is byte-perfect; the disasm is fully consistent.

The "mismatch" was real but local — for *unshifted* blits the disasm
formula `(mask | screen) ^ pix` is what the binary runs; for *shifted*
blits it indexes pre-shifted operands from `table_shifts` at $F200,
so the OR/XOR computes a pre-transformed value whose **output**
matches our direct-bitops formula `(~mask & screen) | (mask & pix)`.
The `table_shifts` indirection had been missed when computing
expected values.

**How to apply.** When the disasm-vs-GT story doesn't add up:

- Assemble the disasm's source and byte-diff against the reference SNA.
- If non-patch bytes differ → assembler / patch bug.
- If only patch bytes differ → disasm is trustworthy. Then look for
  table lookups, SMC, or other indirection that transforms operands
  between the named instructions and the actual ALU input.

## ZEsarUX silently no-ops breakpoint commands

**Rule.** Call `enable-breakpoints` *before* `set-breakpoint` in ZRCP.
If breakpoints are globally disabled, `set-breakpoint N <cond>` returns
`"Error. You must enable breakpoints first"` — but the harness's
`zrcp.py` wrapper discards command output, so the failure is invisible.
Subsequent `run` calls don't trip the breakpoint and execution sails
past the target PC.

**Why.** Building `replay-l3-entry`, the harness's `run_until_pc` op
set breakpoint 1 to `PC=BA83H` then ran 5M opcodes. The Z80 ended at
`PC=$BB38` (the `HALT/DI` inside the main loop's frame wait) and the
op raised "expected PC=$BA83, got PC=$BB38." Probing via raw `command`
showed the set-breakpoint returned an error string but the wrapper had
swallowed it, and `get-breakpoints` listed `Disabled 1: None`. Flipping
the order — `enable-breakpoints` first, then `set-breakpoint` — got
`Enabled 1: PC=BA83H` and the next `run` halted exactly at `$BA83`.

**How to apply.** Whenever a ZRCP `run`-based control flow lands past
the target:

- Call `get-breakpoints` and confirm the row shows `Enabled N: <cond>`
  (not `Disabled`).
- If the wrapper isn't surfacing errors, drop to `zc.command(raw)` to
  see the actual response string while debugging.
- Treat the global enable as a precondition for the per-slot set, not
  the other way around. The same pattern almost certainly applies to
  ZEsarUX's other conditional facilities (watchpoints, memory traps).

## Two ports of one routine drift — diff them against the disasm

**Rule.** When the same Z80 routine has been ported twice (e.g. a
"generic" helper and a later "exact" one), assume they disagree until
proven otherwise, and trust the one whose derivation is written against
the disasm. Re-point callers at the validated copy.

**Why.** `handling_ball`'s motion runs through the q8.8 direction
decoder. Two C ports existed: `enemy_dir_delta_q8` (older, used by the
ball and enemies) and `dir_to_dxdy` (later, documented as the exact port
of `hl_bc_calc_direction` at `$AD22`, validated against death-spark
motion). They differed by one table index on the X component:
`hl_bc_calc_direction` computes the X magnitude as
`direction_table[(idx XOR $0F)+1]` = `direction_table[16 - idx]`, which
needs the 17th entry `$00` (`dir_sin_tbl[16]`). `enemy_dir_delta_q8`
indexed `direction_table_q8[15 - idx]` on a 16-entry table — every ball
and enemy X step got the *next* direction's magnitude. The frame-step
parity gate (`make capture-timeline-both`) is what made it findable, but
the bug is sub-pixel for small `idx` deltas (e.g. dir `$1F` differs by
only 2 q8.8 units/frame), so it stays invisible for ~40 frames and does
not move the gate's first few captured frames — confirm such fixes
against the disasm, not only against a short pixel diff.

**How to apply.** When you spot a second port of a routine you already
ported, byte-diff their outputs across the full input domain (here: all
64 directions × a speed), not just the inputs your current test
exercises. Make the validated one the single source of truth and have
the others delegate to it. A sub-pixel-per-frame numerical bug will pass
every short test and only surfaces as slow drift — the disasm is the
oracle, the pixel gate is a long-horizon confirmation.
