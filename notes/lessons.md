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

**Z80 bit twiddling is `RES`/`SET`/`BIT`, not `LD`.** Hunting for every
write to the ball's owner bit (`object_ball_1+$12` bit 7), I grepped for
`LD (IX+$12)` and `LD (object_ball_1+$12)`, found only the counter
writes and one initialisation, and concluded — in a commit and in two
notes — that nothing in flight ever changes it. `LAB1F_0` changes it on
every bat deflection with `RES 7,(IX+$12)` / `SET 7,(IX+$12)`.

The tell was there: the conclusion was strange. "Brick points go to
whoever the ball spawned toward, for the ball's whole life" is a worse
game than "whoever last hit it", and a rule that sounds wrong usually
is. Strangeness is a reason to widen the search, not to write the
finding up more carefully.

When looking for every write to a BIT, search the bit ops too — or
better, read the routines that consume it rather than grepping for
writers.

**Sprites in this game are drawn bottom-up.** `print_sprite_pix` copies
width bytes per row and then moves to the PREVIOUS buffer line
(`SUB width+$1F` on the low byte, `DEC H` on borrow), so the first data
row lands at the given y and the rest stack ABOVE it.

Checking whether the frame's top strip is generatable, I laid the eight
`set_border_horizontal` sprites out top-down, got 58 bytes of 256, and
had written most of a note concluding the strip was not reducible to the
tape's sprites. Reading the routine instead of trusting the layout gave
256 of 256, on all four colour cycles.

This is the same shape as the round banner's bottom-anchored text
coordinates and the Kinnock egg's — the ZX side of this codebase
consistently anchors at the BOTTOM. Assume it, and check the routine
before concluding that data does not match.

**An INFO row is a measurement, not a gate.** `test-visual`'s
`state4_level1` — the whole level screen — was `assert_match=False` for a
rendering residual that had long since been fixed. It printed
"pixel-identical" run after run and failed nothing.

That went unnoticed until the perimeter frame started being GENERATED
from the tape's sprites rather than loaded from a capture. Mutating that
generator (truncating the inner-border band, shifting a column, moving
the top border a row) left the entire suite green, because the only gate
that watches those pixels was not asserting.

So: when a gate is downgraded to INFO for a known residual, the residual
is a TODO with an expiry, not a permanent state. Re-check it when the
thing it was measuring changes — and promoting it back is usually one
line.

**Re-run the comparison before believing the gap.** PLAN.md carried
"the port and original destroy different cells" as an open WS6 item.
Running `replay-l3-brick-flash-both` once showed `current_level_copy`
byte-identical — the whole 180-cell grid. It had presumably been equal
since the LAFFC work landed, and stayed in the INFO tier because an INFO
row that starts matching says nothing: it prints PASS either way and
nobody promotes it.

Same shape as `state4_level1` sitting at `assert_match=False` while
printing "pixel-identical" for weeks. A diagnostic row is a question
somebody asked once; the answer can change without anyone noticing,
because the row's whole point is that it does not fail.

So when a plan names a diagnostic row as evidence of a gap, run it
before doing the work it implies.

**Third instance, and now a lint.** `state2_menu` in `test-visual` was
`assert_match=False` with no comment at all — reporting
"pixel-identical" on every run, failing nothing. It predates
`test_mode_pin_blink`, which pins the menu's blink phase under
`BATTYALL=1` and made the screen deterministic: the reason for the
downgrade went away and the downgrade did not.

Three in one week (`state2_menu`, `state4_level1`, the replay
comparison's `current_level_copy`), all the same shape, so it stopped
being worth re-learning. `test-visual` now lints its own checkpoint
table: a bare `False` fails, `False,  # INFO: <why>` passes. The point
is not that downgrading is banned — sometimes it is right — but that the
reason has to exist and be re-readable, because the row itself will
never tell you it has gone stale.

**A checker that scans prose counts prose as use.** `check_no_dead_constants`
scans `src/`, `tests/` and `scripts/` for a constant's name, so a gate
that greps for one counts as a user. Its own DOCSTRING lists the eleven
dead constants it first found — which made all eleven look alive, and
let a reintroduced `BAT_X_MAX` survive the mutation that was supposed to
prove the gate worked.

Third time this week in a different costume: `notes_symbols` flagged its
own write-up, and the fix there was to name retired symbols without
backticks. Here it is to strip Python docstrings and comments before
searching, which is what `phase_sweep` already does for the same reason.

The rule: when a checker's haystack includes documentation, decide
explicitly whether a MENTION is a USE. Usually it is not.


## A quote that stops early is a claim you did not check (2026-08-10)

`notes/double-play.md` said, in bold, that PLAN.md was wrong and the
Double Play bats were not confined to their halves. It quoted the caller
to prove it and stopped one instruction before `CALL LACCE` / `CALL
LACAD` — the two routines that confine them. The plan had been right,
and had been "corrected" away on the strength of a truncated listing.

This is the fourth time this shape has cost a wrong conclusion: LAFFC_30
"teleports" (missed the LB1C3 fallthrough), the ball owner "never
changes in flight" (missed the RES/SET bit ops), an 11-row sound table
(a regex that carried `LD DE` across a routine boundary), and now this.

`scripts/disasm.py` warns on FALLS THROUGH, which covers the first.
It cannot cover this one — nothing was falling through, the reader
simply stopped scrolling.

**The rule that would have caught all four:** a claim of the form "X
never happens" is a claim about the WHOLE binary, so grep the whole
binary before writing it down. `grep -n 'CALL LACCE' batty.asm` is
seconds. Quoting a routine to its apparent end proves nothing about
what its caller does next.

The cost is not just the wrong note. Everything built on it inherits the
error, and here the note was the stated reason a plan item had been
struck out — so the work would not have been done at all.

## A checker that counts the right answer accepts it in the wrong place
(2026-08-10)

`check_two_player_state` required that TWO brick-scoring sites pass
`ball_owner_side`, calling them "the LAFFC path and the sweep path".
There is no sweep scoring path. The second site was the BULLET, which
the original credits by the bullet's own x — so the check was pinning a
mis-attribution in place, and it FAILED the commit that fixed it.

The shape: three sites that must differ from one another on purpose, a
check that counted how many matched one of the three. Any two-of-three
satisfies it, including the two wrong ones.

**Where sites differ deliberately, assert the difference, not the
count.** The corrected version reads all three sides and compares the
SET against the expected set, so swapping two of them fails even though
the count is unchanged.

The tell was in the failure message itself: it named "the sweep path",
which does not exist. A checker's own error text is a claim about the
code, and it had gone unread for as long as the check had been green.


## Widening a gate to cover a flake makes the flake permanent (2026-08-10)

`test-double-play-input` measured bat 2 at $B4 on one run and $B0 on the
next. I traced the mechanism correctly — ENTER is bat 2's right key and
also what the capture harness presses — and then widened the assertion
to accept both, with a paragraph calling it the honest bound.

The mechanism was right and the conclusion was wrong. The behaviour was
a defect OUTSIDE the harness too: a player pressing ENTER to get through
a screen would move bat 2 in the next level. Widening the gate hid it
from the only test that could see it. It came back four hours later in
`test-double-play-court`, which measures a pixel extent and has no such
tolerance, and only then got fixed — by dropping ENTER from the binding,
a two-line change.

**When a measurement comes out two ways, the question is which behaviour
is correct, not which bounds cover both.** Tolerance is right for
genuine environmental noise; it is wrong for anything the port itself
decides. The tell is whether you can name the code that produced the
variance. If you can — and here I had written it down — it is not noise.

Related: `notes/double-play.md` on the ENTER binding, known-bugs #19.

## Verify a redraw refactor against the DYNAMIC path, not just entry
(2026-08-10)

`ec8c03d` dropped the brick pass from the generated attr band and
measured it properly: three builds, all 15 levels, pixel-identical. All
15 were level-ENTRY captures, which are static and never exercise a
partial rebuild. `test-enemy-brick-residue` — the dirty-vs-full redraw
comparison — went red in the same commit and stayed red for ten.

The base band is read by two consumers with different needs: the full
paint, which repaints every row over it, and the partial rebuild, which
copies it into a window and repaints only that window's rows. Removing
data the full path recomputes is safe; the partial path was relying on
the same data for the rows it does NOT repaint.

**A change to shared base data needs a test per CONSUMER, not per
output.** "All 15 levels identical" measured one consumer fifteen times.

## Object-table indices are not array indices (2026-08-10)

Making the bat's width state per-bat, I wrote:

    static BatState bats[2];
    #define bat1 bats[OBJ_BAT_1]

`OBJ_BAT_1` is **6** and `OBJ_BAT_2` is **5** — positions in the
eleven-slot OBJECT table, not 0 and 1. So every access ran four
elements past the end of a two-element array and scribbled on whatever
`.data` followed.

It compiled clean under `-w4 -we`. The symptom was the ball flying to
the ceiling on launch, and nineteen gates red, behind a diff that was
otherwise a pure rename.

### How it was found, and what nearly went wrong

The instinct was "a rename cannot change behaviour, so the sweep must be
flaky or the machine loaded". Two things stopped that:

- **Reproducibility first.** HEAD passed 2/2, the change failed 4/4. A
  single run of each would have proved nothing, and I have been burned
  by exactly that this week (the ENTER race).
- **Normalise, then diff.** `sed 's/bat1\./bat./g' | diff` against HEAD
  reduced a 70-line diff to ONE semantic line: the declaration. At that
  point "a struct became an array of the same struct and behaviour
  changed" is obviously impossible unless the indexing is wrong, and
  the enum values settle it in seconds.

The false trail was a layout hypothesis — that adding 28 bytes of
`.data` had shifted some pre-existing overrun onto live data. Testing it
took one build (HEAD plus a same-sized dummy static: PASSED) and ruled
out the entire theory. Cheap experiments that can only confirm or kill a
hypothesis are worth more than more staring.

### The fix, and the rule

    #define BAT_SLOT(o)  ((o) == OBJ_BAT_2 ? 1 : 0)

Every access converts explicitly, so an out-of-range argument lands on a
real element instead of off the end.

**When a new array is indexed by an existing enum, check the enum's
VALUES, not its names.** `bats[OBJ_BAT_1]` reads perfectly and is
nonsense. The names carry the domain; only the numbers carry the range.

## A state gate cannot see a rendering bug (2026-08-10)

`test-double-play-input` proved bat 2 steers by reading `object_bat_2`
out of the probe. Green, and correct about everything it looked at. The
sprite meanwhile never moved after level entry — object at `$DC`, bat
drawn at `$B4` — because bat 2 was drawn only by `compose_level_scene`
and `bat_moved` was bat 1's x alone.

The gate asserted everything about bat 2 except the only thing wrong
with it, and it did so while I was writing commit messages saying bat 2
now steers. It does; it just could not be seen to.

**When a feature's output is pixels, at least one gate has to read
pixels.** Probe rows are cheaper, more precise and easier to debug, and
they are the right default — but a feature whose whole point is visible
needs one gate that looks at the visible thing.

The corollary is about gate SHAPE. The failure was not "bat 2 never
moved", it was "bat 2 moved 4 px and froze" — one step taken before the
entry compose ran. A gate asserting merely that the screen changed would
have passed against the bug. `test-double-play-bat2-redraw` requires the
pixel difference to span both footprints, with the right edge derived
from the object's actual x in the same run.

Ask what the bug would look like, not just whether the feature works.

## A multi-edit script that aborts writes NOTHING (2026-08-10)

My edit scripts collect changes in a string and `write_text` once at the
end. When an assertion fails partway — a needle that no longer matches —
NONE of the earlier edits land, including the ones that matched.

Twice today I then applied the failing edit on its own and carried on,
believing the rest had stuck. The second time cost a full debugging
round: bat 2 caught BIG_BAT, took the bonus byte, and did not grow. The
code I "had just written" read correctly when I looked, because I looked
at the one edit that DID apply.

**Verify the file, not the script's intent.** `grep -c` for a symbol the
edit introduces costs nothing and answers it exactly. The build is not
enough: a missing edit usually still compiles, which is the whole
problem.

Related: the `bats[OBJ_BAT_1]` out-of-bounds above. Both are the same
failure — trusting what I meant to write over what is on disk.

## Gate the drift, not the truth (2026-08-10)

PLAN.md's definition-of-done table has gone stale three times. Each
refresh wrote a paragraph about how bad that is — including one saying
"a status table nobody re-reads is the same defect as a parity note
nobody re-reads" — and changed nothing structural. So it went stale
again, claiming "bonus ownership and bat-2 catch remain" after both
landed the same day.

Writing a better note is not a fix. It is the same intervention that
already failed, applied harder.

**What a freshness gate can honestly do:** not decide whether prose is
true — no gate reads "Done" and knows — but insist the document is
RE-READ when work lands. `test-plan-table-fresh` compares the table's
"Table refreshed <date>" against the newest date in any workstream
section and fails when work is newer.

That targets the actual failure, which was never belief: work lands in a
WS section and nobody scrolls back up four hundred lines. Bumping the
date without reading the rows defeats it, and that is the point — an
omission becomes a choice someone has to make deliberately.

The general shape: when a document keeps rotting, look for the
mechanical precondition of the rot (here, "was it opened?") rather than
trying to verify the content. `test-notes-numbers` has never been the
thing that went stale, because numbers get checked. Prose does not, so
gate the reading of it instead.

## A rename is not done until you grep the PROSE (2026-08-10)

Renaming `bat` to `bats[2]` + a `bat1` macro, I rewrote the code lines
with a scripted substitution that deliberately skipped comment lines —
so as not to mangle sentences describing the ORIGINAL's `bat.bonus_applied`
concept, which is a fair thing to name.

The result was six stale `bat.<field>` mentions left in `src/` comments
and four in notes/ and a gate docstring, three of them present-tense
claims that were now false. The worst was on `render_bat_2`: "rendered
with the PLAIN sprite: `bat.extra_px`, the gun frames and the resize
sides are all bat-1 state" — by then bat 2 had its own width, its own
bonus byte and a shared renderer that draws it big or armed.

`notes_symbols.py`, the tool that exists for exactly this, reported
NONE of it. Two reasons:

- it only matches bare backticked identifiers, so `bat.extra_px` is
  invisible to it — still true, and extending to `foo.bar` was tried and
  reverted because it matches filenames too;
- its corpus was raw text INCLUDING comments, so a name still mentioned
  in a stale comment counted as "defined". Three of the six occurrences
  were in `src/` comments, and that was enough.

**The second half is now fixed** (`strip_prose`, the same day). It had
been masking the tool's own founding case: `bounce_enemy_off_margins`,
deleted 2026-08-09, is still named in `src/` comments and had quietly
stopped being reported. The report went 36 -> 58 names.

That is the sharper lesson here. A tool built to catch stale prose was
being defeated BY stale prose, and nobody noticed because its output
looked reasonable — a shorter list reads like good news.

**So: after a rename, grep for the OLD name across src, notes and
scripts, and read each hit.** Past tense is fine and often required.
Present tense is a lie with a citation attached, and a tool that scans
raw text cannot tell you which is which — a name that survives only in
the prose that is wrong about it looks exactly like a name in use.

## "Not gated" is a note to write and then a note to delete (2026-08-10)

Landing bat 2's shared renderer I wrote: *"a bat 2 holding LASER now
shows the gun-mounted sprite ... Not gated: no scenario yet drops a
LASER on bat 2."* Honest at the time — building that scenario was more
work than the change.

Two commits later, `test-double-play-bat2-laser` built exactly that
scenario for a different reason. The hole then cost ONE capture to
close, and it closed the same day it was named.

The habit worth keeping is both halves:

- **write it.** An ungated behaviour that nobody records is
  indistinguishable from one nobody thought about, and the note is what
  makes the cost visible later.
- **re-read it.** The precondition that made gating expensive is
  usually removed by some later commit, and nothing announces that. The
  note went stale in the good direction — the work got cheaper — which
  is the kind of staleness nobody goes looking for.

Same shape as `test-double-play-bat2-catch`'s docstring predicting its
own failure when bonus ownership split. A note about a future condition
is worth writing precisely because it turns a future search into a
future edit.

## An accepted residual can acquire consequences (2026-08-10)

PLAN.md's non-goals list carried: *"Big-bat resize as a literal
bit-gated state machine — visually matched; revisit only if a defect
surfaces."* A reasonable call, and the list is explicitly headed **do
not reopen**.

Auditing the sound queue turned up `play_sound_bat_resize_1`'s
`LD A,(bonus_flag) / AND A / RET NZ` guard, which the port does not
have. Tracing `bonus_flag` led straight back to that same resize
machine — it is written by `handling_bat`'s transform paths and read
there for the gun sprite. So the residual is not free any more: it owns
a sound divergence that cannot be closed without it.

The decision has not changed — both halves are cosmetic — but its COST
has, and the entry said nothing that would let a reader notice. Three
places described the resize machine, all as an isolated cosmetic
approximation, none of them mentioning each other.

**When a new gap traces back to an accepted residual, update the
residual, not just the new gap.** The whole point of a "do not reopen"
list is that people stop re-deriving the trade-off — which means the
trade-off recorded there has to stay current, and it is the one entry
nobody re-reads precisely because it says the question is closed.

## A test that never reaches the code under test passes anyway (2026-08-10)

Answering whether `laffc_sweep`'s left clamp is guarded, I wrote a host
test sweeping `new_x` left of `FIELD_X0` and comparing the resolved
column against the edge case. It passed. The mutation that deletes the
clamp ALSO passed it.

The clamped value feeds exactly one expression, and that expression
lives inside `if (!field.standing(row, col))`. My test filled the whole
grid, so `standing(row, 0)` was true, the branch never ran, and the
value under test was never read. The test exercised the function and
missed the line.

`field_destroy(0, 0)` fixed it and the mutant died.

**A test aimed at one line should be checked against a mutation of that
line, not against its own green.** Green means the assertions hold on
the path taken; it says nothing about which path that was. This is the
cheapest possible use of mutate.py — one line, one run — and it is the
difference between a test and a decoration.

Related: `check_floppy_assets` and `check_known_bugs_table` were both
found to have haystacks narrower than the text they read. Same family —
the check ran, and ran past the thing it was for.

## Finding a case by reading the OUTPUT of the thing you are testing
(2026-08-10)

Pinning `laffc_sweep`'s corner tie-break, I needed positions where the
two penetration depths come out equal. I wrote a finder that recomputed
them from the returned `face_mask`:

    xp = (h.face_mask & 1) ? ... : ...
    yp = (h.face_mask & 4) ? ... : ...

`face_mask` is what the tie-break PRODUCES. Selecting the penetration
formulas with it means selecting them by the answer, so the "ties" it
reported were not ties. The test built on them passed, and the mutation
survived it — a second decoration, written while fixing the first.

What worked: dump every hit under BOTH rules and diff. 236 positions
change; the test uses six. No derivation, no chance of picking the case
by the property being tested.

**When you need an input that triggers a branch, get it from a
differential run, not from the function's own output.** The output has
already had the branch applied to it.

## "Equivalent mutant" hides two different answers (2026-08-10)

A mutation that survives and turns out to change nothing can mean either
of these, and they call for different work:

- **Equivalent by arithmetic.** `addr < 0x8000 ? (addr | 0x8000) : addr`
  mutated to `<=` differs only at `addr == 0x8000`, where
  `0x8000 | 0x8000 == 0x8000`. There is no input that distinguishes
  them, ever. A sentence is the right response.

- **Equivalent because two constants happen not to line up.**
  `bullet_y >= FIELD_Y0` mutated to `>` differs only at
  `bullet_y == 32`, and bullets launch at 172 stepping 6 — 4 (mod 6)
  against 32's 2 (mod 6), so they never meet. Change the bat row or
  BULLET_SPEED and the branch becomes live, silently.

The second kind needs a TEST, not a note. The test is what tells the
person who changes `BULLET_SPEED` that they have just made a dead branch
live — and nothing else will, because the constants that made it dead
are three files away from the boundary that depends on them.

So when a survivor turns out to be equivalent, ask WHY. "No input can
tell" and "no input currently does" look identical in a mutation report.

## The gate that found a bug need not cover the fix (2026-08-10)

`test-enemy-brick-residue` found known-bugs #18: the partial brick
rebuild lost both boundary char rows. It is a whole-screen diff of the
dirty path against a full redraw, which is why it caught something no
targeted test was looking for.

It does not, however, cover the guards that implement the interlock.
Mutating `reset_destroyed_cell_attrs`' `cr + 1 >= cr0` to `>` — the
exact test that lets a destroyed cell above the window own the window's
first row — passed that gate, and the whole host suite.

Both facts are ordinary. A screen diff catches a class of failure at
one seeded scenario; it says nothing about a boundary that scenario
never reaches. And every host test called the function with the FULL
window, where the boundary cannot arise at all.

**After fixing a bug a broad gate caught, mutate the LINES of the fix.**
The gate proved the symptom is gone on one path. It does not prove the
condition you wrote is the condition you meant, and the two feel like
the same evidence when the gate goes green.

## A survivor count is a coverage figure, not a bug count (2026-08-10)

Mutating every unique inclusive comparison in `src/` — 51 of them — gave
16 caught and 35 survived. "35 untested boundaries" would be a fair
headline and a misleading one.

The first three triaged were not defects at all:

- `bat_court_clamp_2`'s `bat_x >= 0x80` — both branches return `0x80` at
  the boundary, so no input can ever tell.
- `delta_to_dir`'s quadrant tests, twice — the function has no
  production consumer.

The next two were real, and one of them mattered: `blit_masked_sprite`'s
clipping guards, where `>` lets a sprite write one pixel past the row.

**So the number tells you where to LOOK, and nothing else.** Reporting it
as a defect count would have been wrong in three cases out of the first
five, and reporting only the two real ones would have hidden how much of
the sweep is still untriaged.

The honest form is the one in notes/testing.md: the count, the method,
the cases decided so far, and an explicit statement that the rest is a
backlog. A measurement that gets summarised into a verdict stops being
a measurement.

## A defensive guard elsewhere decides whether a boundary is a bug
(2026-08-10)

`brick_sweep` had seven untested inclusive comparisons. Three were real.
Four were equivalent, and not for four different reasons — for one:

- `top >= FIELD_Y_END` and `left >= x_end`: relaxing them lets an
  out-of-band ball through, but the row/col clamps then compute
  `row0 > row1` (or `col0 > col1`) and the scan loop never executes.
- the `col1` / `row1` clamps: relaxing them scans one extra column or
  row, and `BrickField::standing` returns false for it.

`standing`'s comment is *"Out of range counts as gone"* — one line, in
`level.h`, written as ordinary defensiveness. It is what makes half this
cluster harmless.

Two things follow. First, a mutation survivor can be equivalent because
of code in a different file, so "is this reachable?" is not answerable
from the function alone. Second, and less comfortable: if that guard
were ever tightened for performance — an obvious enough thing to try in
a per-frame path — four latent boundary bugs would appear at once, in a
function whose own tests would still pass.

The four are recorded in notes/testing.md rather than left implicit,
because "we checked and it was fine" is not the same as knowing why.

## "Caught" can mean "did not compile" (2026-08-10)

`mutate.py` applies a replacement, runs a make target and reads its exit
status. A replacement that does not COMPILE fails the target, and that
is indistinguishable from a test detecting the mutation.

Driving batches from a shell loop, I wrote `&&` in the replacement as
`\&\&`. The backslashes survived the quoting and went into the source.
Three results came back "caught"; two were compile errors, and one of
those — `bricks.cpp`'s `left_live = (col > 0) && ...` — went into a
commit message as covered. It was not. Re-run properly it survives, and
`col >= 0` reads the byte before the grid on row 0.

The tell was available and I walked past it: three consecutive "caught"
results on lines nothing new had tested. That should have been
surprising rather than satisfying.

**Re-run any "caught" you did not expect, singly, with the strings
quoted so the shell cannot reach them.** Same shape as
`make | grep -i error` matching the word inside "no errors": a green
signal is not evidence until you know the thing you meant to change
actually changed.

## `>=` vs `>` differ at exactly the bound — aim there (2026-08-10)

Twice in one session I wrote a test for an inclusive comparison that
overshot the boundary and therefore proved nothing:

- `laffc_sweep`'s left clamp: I passed `x = $FF`, far outside, where the
  8-bit sum wraps and both forms behave the same.
- the attr blit's column clamp: I passed a rect giving `col_hi = 38`,
  where `>= ATTR_COLS` and `> ATTR_COLS` both clamp.

The obvious instinct for "test the clamp" is to go a long way out of
range. That is the one place the two forms always agree. The value that
distinguishes them is the bound ITSELF — `col_hi == ATTR_COLS`,
`a == BRICK_W_PX`, `offset == zones[i]` — and it is usually the only
one.

So the arithmetic has to be run backwards from the comparison: what
input makes this expression exactly equal to that constant? For the attr
blit, `col_hi = (x1 - 1) / 8 == 32` means `x1` in 257..264, so the rect
is `x = 250, w = 10` and not something comfortably enormous.

## One argument can retire a whole class of survivors (2026-08-10)

Six of the last mutation survivors were early-out guards — `y0 >= y_end`,
`width <= 0`, `x1 <= x0` and so on. Triaging them one at a time would
have meant six traces through six functions.

They share a property that settles all six at once: **relaxing an
early-out can only cause MORE work, never less.** A degenerate rect that
gets through then runs a zero-trip loop, or widens a dirty range, or
bumps a profiling counter. None of those can skip a pixel that should
have been drawn.

And the output claim did not rest on that reasoning alone.
`dirty_flush_equiv_full` compares a dirty flush against a full repaint,
so any mutation that NARROWED the refresh would fail it. All six pass —
which is what makes "equivalent in output" a measurement rather than an
opinion.

The general move: before triaging survivors individually, look for a
STRUCTURAL property they share. Six one-off traces is not six times the
value of one argument that covers them, and it is much easier to get one
of the six subtly wrong.
