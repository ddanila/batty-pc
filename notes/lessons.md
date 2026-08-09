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

## Two-layer test parallelism: cap inner jobs or you re-trigger the TCG flake

The slow gates (bat-deflection, ball-no-tunnel) parallelize their own case
loops (each case on its own floppy via BATTY_TEST_FLOPPY — and the per-case
`make` MUST pass that env or TEST_FLOPPY_OUT won't resolve to the per-case
path and the build silently no-ops, reading -1). Standalone that's a big
win (bat-deflection 162s -> 23s on 14 cores). BUT under run_gates_parallel
(outer fan-out), inner×outer can exceed core count, and too many concurrent
QEMUs each run SLOWER than real time — which breaks the harness's
wall-clock frame waits (the port hasn't reached frame N when the capture
fires) and every gate "diverges". Same failure mode as hosted-CI TCG.
Fix: gates use a MODEST inner pool (2) when BATTY_TEST_FLOPPY is set (=
running under the outer runner) and a high pool standalone. Net fast-core:
538s serial -> 178s (outer only) -> 99s (outer + capped inner) on 14 cores.
The deeper fix (make the harness wait for the port to REPORT it reached
frame N, instead of a wall-clock sleep) would remove the fragility entirely
and is the prerequisite for QEMU-on-CI.

## Parallelizing the suite: per-floppy isolation, not per-process

The serial-only constraint (below) was lifted by making the floppy path an
env var (`BATTY_TEST_FLOPPY`) instead of a hardcoded literal: each concurrent
gate gets its own `build/batty-test-<i>.img` AND its own AUTOEXEC scratch
(`AUTOEXEC_T` derived from the floppy name in the Makefile — the scratch
file was the *other* shared-state collision, easy to miss). Pre-build the
shared `TEST_EXE` once before fanning out so workers don't race on
`build/main-test.obj`. ZEsarUX gates can't parallelize this way — they bind
a single ZRCP port (10000) + one snapshot — so the parallel runner covers
only the QEMU-only gates; ZEsarUX gates stay serial. Net result: same gates,
same assertions, ~Jx faster (`make parity-check-parallel J=8`). See
notes/testing.md.

## Floppy-based tests share ONE image — never run two concurrently

Every headless gate builds and boots `build/batty-test.img` and reads back
`PROBE.TXT` from it. They ALL use that single path. Running two at once
(e.g. a long sweep in the background while you spot-check another gate in
the foreground) makes them clobber each other's floppy and PROBE.TXT
mid-boot: the reader gets the OTHER run's state. Symptom that wasted ~20
min on 2026-06-17: a freshly-seeded enemy probe (and even the shipped
`test-enemy-descend`) read a constant garbage `object_enemy` (x=120 y=136
set=0x00) that had nothing to do with the seed — it was another boot's
default enemy. Stashing the source change "reproduced" it, falsely
implicating the edit; the real cause was a background sweep booting the
same image. Rule: floppy gates MUST run SERIALLY (this is why
`parity-check` chains them with `$(MAKE)` one at a time). If you background
a sweep, do NO other floppy work until it finishes — or give the sweep its
own floppy path. A green/red result from a gate run during another gate is
meaningless.

## Moving objects blit PIXELS only — they never recolour cell attrs

The original draws every moving object (ball, bullet, bonus, bomb,
pts400, ENEMY, rocket, bat) with `print_obj_to_buff` ($B82C), a
shifted masked PIXEL blit. It never calls `print_sprite_attrib` ($B656) —
that routine runs exactly 4 times, all in `game_screen_draw_to_buffer`
(static texture + border). So a moving sprite shows ZX colour-clash in
whatever attr its cells already hold (bg over open texture, the BRICK's
attr over bricks). known-bugs #7 was the port force-recolouring the
enemy's cells to `bg_attr` (a `blit_sprite_attrs_to_buff` invention) —
wrong over bricks, and the source of the bug-#2 stale-clash-attr saga
(there is nothing to leave stale if you never recolour). Rule: when
porting a sprite draw, reproduce its ATTR semantics too — if the original
writes no attrs, don't; the cell's existing colour IS the intended clash
colour. Verify with an attr read of the original `.scr` (the last 768
bytes), not just a pixel diff. Gate: `test-enemy-attr-parity`.

Third instance (2026-06-12): **the BATTY_REPLAY_COUNTER pin does NOT
survive checkpoint halts.** A multi-checkpoint VISUAL_PROBE_FRAMES
timeline halts at each checkpoint for wall-clock time while the PIT
keeps ticking, so the counter PHASE at each resume is re-randomized —
and two A/B boots (dirty vs full-flush) halt for different durations,
diverging phase-gated behaviour (the enemy's repick timing) after the
first halt. Chasing the fly-over trail, a frames-50..100 A/B showed a
"713 px clash-attr bug" at frame 100 that was really the two boots'
UFOs steered to different positions; single-checkpoint runs at frame
100 are byte-identical (probes confirmed lockstep sims). Rule: for A/B
screen comparisons, only the FIRST checkpoint of a timeline is valid —
use one checkpoint per boot, or re-pin the counter on every checkpoint
wake.

Fourth instance (2026-08-09): **when the phase makes a gate flaky, look
for something in the capture that says which outcome happened.**
`test-enemy-descend` asserted `target == $10` on the frame the entry
slide ends, and failed about two runs in three. That frame is the first
one the alien can steer, and steering is gated on a GLOBAL counter's
phase — so both `$10` (not steered) and `$29` (steered, arrival re-pick,
fixed RNG) are correct, and which one you get depends on how long boot
took.

The lesson from the third instance was "re-pin the counter". Here that
was unnecessary: `PROBE.TXT` already carried
`enemy_repicks=arrival<N>_margin<N>_turns<N>`, so the gate could assert
the IMPLICATION — `turns == 0 -> $10`, `turns == 1 -> $29` — plus that
the slide frames cannot steer at all. That pins BOTH legal outcomes,
where the old assertion pinned one and rolled a die.

So the order to try is: (1) does the capture already distinguish the
cases? assert the implication; (2) if not, pin the phase; (3) dropping
the assertion is the last resort, and it costs coverage.

The other half of this: `parity-check-parallel` retries a failure once
alone and prints `(starved when parallel)` when the retry passes. That
message is a GUESS about the cause, and it was the wrong one here — the
retry passed because it was a coin flip, not because it was alone. A
runner that explains nondeterminism as contention hides exactly the
gates worth finding. Treat "only passed on retry" as unexplained until
measured. See known-bugs #17.

Fifth instance, same day, and it is about me rather than the code:
**check whether the fix already exists before designing around its
absence.** Having worked out that `test-enemy-descend`'s flake was the
free-running counter's phase, I wrote that pinning the phase would be
"more machinery" and built an implication-based assertion instead.
`BATTY_REPLAY_COUNTER` had been in the port for months, is applied by
`pin_replay_frame_counter` at exactly the right moment, and its comment
names this exact flake. One env var.

The implication work was not wasted — it is the stronger assertion and
it survives the pin being dropped — but it was presented as the only
option when it was not. The tell was available: the third instance in
this file already discusses `BATTY_REPLAY_COUNTER` by name. I had read
that entry and did not connect it.

So: when a mechanism is diagnosed, grep for it in `src/` and in these
notes before deciding what to build.

**Read past the end of a routine.** Twice now the load-bearing detail
has been in the instructions AFTER a listing stopped. `LAFFC_30` ends
`LD (LAA7B),HL` and falls into `LB1C3`, which UNDOES the position snap
it just recorded — miss it and the alien's brick reaction reads as
"teleports to the corner". `current_level_2up_copier` ends its copy loop
on `JR NZ` and falls into `players_swap`, which is the entire turn
alternation in 2-player mode — miss it and the mode looks like it never
changes turns.

Both times the listing looked complete, and both times `disasm.py` made
it worse: it trims the next routine's comment header (deliberately, so
output is not padded with someone else's documentation), and that header
is the only visual cue that another routine begins. It now prints a
FALLS THROUGH warning when a routine's last instruction is not an
unconditional `RET`/`JP`/`JR`. Verified on both cases, silent on a
`JP`-tailed control.

The general rule: a Z80 routine boundary is a LABEL, not an end. Before
concluding what a routine does, check how it leaves.

**Two checks of one condition means one of them is untested.** WS2's
turn change guarded "is the other player still in?" in two places: in
`lose_a_life` before setting the pending flag, and in
`two_player_turn_change` itself. Mutating the real guard from
`lives <= 0` to `lives < 0` SURVIVED — the duplicate upstream stopped
the mutant ever being asked. Adding a gate case that reached the guard
(`BATTY_REPLAY_LIVES_2UP=0`) did not help either, for the same reason.

The fix was to delete the duplicate, not to add more test cases. A
condition tested in two places has one owner and one echo, and mutation
testing cannot tell you which is which — it just reports the echo as
covered.

**Check the plan against the disassembly before building from it.**
PLAN.md's WS3 said Double Play splits the court into halves with "each
bat confined to its half" and listed "per-bat margin clamps at the
divider" as work to do. Neither exists. `handling_bat_no_transform`
calls `check_left_margin` and `check_right_margin` — the same two the
alien uses, over the full playfield — and both bats go through the same
`handling_bat`, differing only in which object and control word.

That plan text was written from watching the game, and watching is how
the enemy's reflect-and-re-aim got invented too: a bat that mostly stays
on its side because that is where its ball is looks confined. The cost
of checking was twenty minutes; the cost of not checking would have been
a mechanic in the port that the original does not have, plus gates
pinning it.

What the halves actually control is SCORING — `need_change_player` from
`x AND $80`, and `add_points_to_score` swapping the score blocks around
`score_update`. The plan had missed that entirely. So the check did not
just remove work, it found the real work.

Applies to my own PLAN.md entries as much as to inherited ones: a
roadmap written from observation is a hypothesis.
