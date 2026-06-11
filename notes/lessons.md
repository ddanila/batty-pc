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
`all_metal_briks_frame` — one frame of the PRE-ROUND all-metal-bricks
animation (driven by `all_metal_briks_animation_snd` from
`game_restart`), not the level paint at all. The
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

## Count-down-to-underflow counters: the gate semantics shift the reset value (2026-06-05)

The original's laser fire-rate counter (`handling_bullet` $A12C) gates on
`SUB $02; JR C` — i.e. it fires when the counter *underflows* (goes below
2), one frame *after* it reaches 0. The DOS port reproduced the `-= 2`
per-frame decrement but gated on `cooldown == 0`, which trips one frame
*earlier* than the underflow. Same decrement, same reset constant (`0x16`),
but an 11-frame cadence instead of the original's 12 — the laser fired
~8% too fast. The author had even noted "~11 frames" without flagging it
as wrong.

The fix is NOT to change the gate but to bump the reset so the `==0` gate
trips on the same frame the underflow would: `0x16` → `0x18` (24 → 0 at
frame 12 → fire frame 13 = 12-frame period). Verified by hand-simulating
both loops frame-by-frame.

A second subtlety: the original's reset is *parity-adjusted*
(`LD A,(bullet); CPL; AND $01; ADD A,$16` = `0x16` or `0x17`). With the
carry gate, that parity bit is what keeps the period at exactly 12 for
*both* values (and across not-holding-fire transients where the counter
sits at 0/1). Under the port's `==0` gate the parity bit is moot — a
fixed `0x18` reproduces the period deterministically.

**How to apply.** When porting a Z80 countdown that gates on the *carry*
of a `SUB`/`DEC` (underflow) rather than on reaching zero, account for the
one-frame offset: a `== 0` (or `< step`) gate in C trips one tick sooner,
so the equivalent reset constant is one decrement-step larger. Don't just
copy the reset literal — simulate the loop and match the *period between
fires*, not the literal value. Cadence bugs like this pass every static
test (no QEMU/ZEsarUX gate covers laser rate) and only show as a subtly
faster weapon.

## Brick-band / dirty-redraw changes MUST re-run capture-timeline-both

`make test` (4-state visual) and `make parity-check` do NOT cover the L3
frame-step destruction transient — only `make capture-timeline-both` (needs
ZEsarUX) does. A perf commit (`27c4d69`) cut `BRICK_FLASH_TICKS` 2→1,
reasoning the carry-flush covers the destroyed cell's 2nd frame. It covers
the single-buffer ERASE (so `test-brick-flash` passed) but NOT a full-dynamic
band REBUILD of the destroy render, regressing the L3 frame-step residual
4px → 88px undetected for the whole rest of the campaign. Rule: any change to
the brick-band cache / dirty-redraw / flash path is a parity change — re-run
`capture-timeline-both`, don't trust the headless gates alone. See
notes/metal-shimmer.md (BRICK_FLASH_TICKS regression).

UPDATE (2026-06-11): this is now ENFORCED — `make test-frame-step` pins the
documented floor (frames 0-6 vs budgets 0,0,0,4,0,4,1 in the brick ROI) and
runs as part of `parity-check-full`, so the manual-discipline rule above has
a gate behind it.

## Wall reflect masks: side = $1F (negate dx), top = $3F (negate dy)

The ball's wall bounce (reflect_obj_dir) is the SAME change_direction
($ACEE) the brick path uses: `dir = ((dir ^ mask) + 1) & 0x3F`, with mask
$1F for L/R walls and $3F for the top (bounce_wall $AC75). A long-standing
port bug had them swapped + off by one (0x3F-dir / 0x1F-dir), so a ball
hitting a side wall kept its dx INTO the wall and pinned/juggled at x=8 or
x=240. Lesson: wall bounces had NO gate (the L3 ball gate never reaches a
bare wall; `make test` is static-only) — that class of bug is invisible
until a player hits it. `make test-wall-bounce` now guards it. See
notes/wall-bounce.md.

## Don't invent input short-circuits in ported blocking sequences

The original's blocking sequences (pause_long, the pre-round
all-metal-bricks animation) read NO input. The port added "any key
skips" conveniences to `play_brik_anim` — which in real play meant a
key held or typematic-repeating at level entry (moving the bat,
pressing FIRE) silently skipped the animation, reported as "the initial
shimmer is very fast" (known-bugs #4). Worse, the consumed key was the
player's gameplay input. Rule: when porting a timed sequence, reproduce
its input semantics too — if the original doesn't read keys, don't; if
you must support a quit key (ESC), PEEK the BIOS buffer
(`_bios_keybrd(_KEYBRD_READY)`) and consume only that key, leaving
everything else for the main loop. And pace HALT-style waits as full
tick-edge waits (`do {} while (pit_ticks() == t)` twice), not
`pit_ticks() - t < 2` from a mid-tick sample — the latter waits 1..2
ticks, not 2.

## Global-counter-phase cadences need the counter PINNED in tests

Anything gated on the global frame counter's low bits (= counter_misc:
enemy steer &3, ball speed ramp &7, bat resize, sprite flicker) has a
boot-wall-clock-random phase at the replay WAIT_KEY release — a probe-frame
assertion on such behavior WILL flake (test-enemy-steer did: phase 0 boots
read dir one steer-step ahead). Widening tolerances (the old ±1 x slack)
papers over it; the fix is determinism: BATTY_REPLAY_COUNTER=<hex> pins
pit_frame_counter at the WAIT_KEY release. Pick the pin so probe frames sit
mid-interval, far from the cadence boundaries (steer test: phase 2, turns
at f=2,6,...,22 vs probes at 16/20/24). See notes/enemy-movement.md.

Second instance (same day): A/B comparison tests are bitten too, not just
GT assertions. test-ball-object-dirty-redraw boots TWICE (dirty vs full
floppy) and diffs the screens; with an active enemy seeded, the two boots'
random phases gave the enemy 12-frame paths 1-2 px apart — a constant
~145px "dirty-redraw failure" that was really the same phase roulette
(passed only when both boots shared a phase, ~1/4). It had been
misdiagnosed as a SLEEP/screendump flake in performance.md. Rule: ANY test
that boots the game more than once and compares outputs must pin
BATTY_REPLAY_COUNTER if a steer/cadence-driven object is on screen.
