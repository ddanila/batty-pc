# Modularising main.cpp

Turning one 7,700-line file into modules that each have a fast, exhaustive
test. Started 2026-08-07.

## Where this stands

The stage table below is complete except stage 1, which is blocked for a
reason rather than for want of effort — see the end of this section.

**The code.** `main.cpp`: 7,747 → 6,868 lines (-11.3%) across 15
modules. The longest function is `run_level` at 113 lines, and it is an
orchestrator of named phases, which is what it should be.

**The tests.** 75 gates, indexed in `notes/testing.md` and kept complete
by `test-gate-index`. They fall in three groups:

  - 59 QEMU gates — `make parity-check-parallel`, ~6 min, twelve clean
    runs, the latest covering the `handle_input` split.
  - 16 emulator-free source gates plus 14 host suites — `make test-fast`,
    seconds. CI runs exactly this.
  - 3 ZEsarUX-oracle gates — `make parity-check-full`.

**The defects.** Eight surfaced by this refactor, #8 through #15.
`notes/known-bugs.md` holds the status table; this file deliberately
keeps no second copy, because the copy that used to be here was already
missing #15 by the time anyone noticed.

### What is actually left

1. **Stage 1 is done as far as it should go.** Six seeders are out; the
   remaining three are blocked BY DESIGN, not by placement, and that is
   now recorded at the code rather than only here:

   - `BATTY_FORCE_SPAWN_BONUS` calls `try_spawn_bonus`, which calls
     `pick_bonus_type` — and that reads SEVEN pieces of live game state
     to reject inappropriate draws. It is the game deciding what is
     appropriate, not geometry. Moving it drags most of the game's state
     into whatever module received it.
   - the two brick seeders need `live_level`, which is game state rather
     than compositor state.

   Forcing either would make the modules worse, so the honest end of
   this stage is here. `pick_bonus_type` carries the reasoning in a
   comment so the next person does not spend an afternoon on it.

2. **known-bugs #14 and #16.** Both need the Spectrum rather than more
   work in the port: whether the original derives the extra balls'
   launch angle from a real velocity (#14), and whether two enemy
   margin-escape angles really aim out of the field (#16).

3. **The parity gaps in `notes/parity-gaps.md`** — enemy RNG not
   byte-exact, multi-ball plus MAGNET catch, and the cosmetic/timing
   items. Those are fidelity work, not refactor work.

### A note on the numbers here

Line counts are `wc -l`, measured against `wc -l` at `e0bb447` (the C++
conversion, before any extraction). Earlier revisions quoted Watcom's
`N lines` report instead. The two agreed within 1 at the baseline and
have since drifted apart by a constant 96 for this file — adding 10
lines moves both by 10, so it is an offset, not a scaling, and the cause
is not established. Mixing them understated the reduction.
`scripts/check_notes_numbers.py` pins this section to reality on every
`make test-fast`.

## How to read this file

`Where this stands` first, then `Stages` for the table of modules.
Everything after that is narrative kept for its reasoning, not its
chronology: each section records WHY a change was made and, where a
diagnosis turned out wrong, what refuted it. Sections were appended
as the work happened, so a heading names where a thread STARTED, not
everything under it.

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
| 3a | `physics` — direction + bat deflection | 231 | **done** — 12 tests vs captured hardware tables |
| 3b | collision geometry/effects split | 166 | **done** — 7 more tests |
| 4 | `assets` | 167 | **done** — 6 tests |
| 5 | `bricks` — the compositor | 278 | **done** — 5 tests, byte-exact vs 15 captured screens |
| 5b | level paint / band orchestration | ~125 | **started** — both band painters, the reset and the edge repairs now in `bricks`; 3 new tests |
| 6a | `objects` — the 22-byte descriptor + slots | 60 | **done** — 5 tests |
| 6b-i | `weapons` — bullets + blasts | 95 | **done** — 6 tests |
| 6b-ii | `enemies` — steering | 145 | **done** — 5 tests |
| 6b-iii | `bonus_codes` — original <-> port numbering | 40 | **done** — 4 tests |
| 6b-iv | bonus effects, rocket, sparks | ~180 | **done** — every switch arm is a named call or a two-liner |
| 7 | `hud` — glyphs, markup, score | 175 | **done** — 6 tests |
| 8 | `sound` — queue + envelopes | 366 | **done** — 7 tests; had NO coverage before |
| 9 | `run_level` decomposition | 684 -> 115 | **done** — every phase named |
| 10 | state owners — structs at file scope | 113 vars | **done** — 11 clusters, see below |
| 1a | `replay_parse` — the BATTY_REPLAY_* value formats | 75 | **done** — 7 tests |
| 1 | replay / probe scaffolding | ~430 | **as far as it should go** — 6 overrides out in `replay`, 6 host tests; the remaining 3 are blocked by design, see below |

`main.cpp`: 7,747 → 6,868 (`wc -l`; see the status block on why this is not Watcom's count).
100 host tests + source gates, all via `make test-fast` in seconds.

### Stage 5b: one destroyed-cell reset, not two

`render_brick_band` and `render_brick_band_rows` each carried the loop
that resets destroyed cells' attrs to the band background — the full
one writing unconditionally, the row-scoped one clipping each write to
the rebased char rows. The full paint is just the row-scoped case over
`[0, LVL_ROWS-1]` clipped to `[3, 16]`, so there is now one
`reset_destroyed_cell_attrs`.

Worth having one copy: this is the code known-bugs #1/#2 came from. The
one-row overscan on each side is load-bearing — `cr0` doubles as row
`r0-1`'s shadow row and `cr1` as row `r1+1`'s cell row — and it was
previously stated in only one of the two copies.

`render_level_screen` and `render_level_screen_static` then folded into
one `compose_level_scene(level_idx, with_bat)`. They shared eight calls
in a fixed order and differed only by the bat; what stays outside is the
dynamic path's cache invalidation before and its flush after.

The paint order is the point of that function, and its rationale lived
in only one of the two copies: score before magnets before bricks, per
`$BE8B`. On levels where magnets overlap HUD rows, magnets may overwrite
the score area but not the reverse; and the brick top row must overwrite
the magnets' lower shadow rows, or those rows punch through the brick
tops.

### Stage 5, done

The compositor moved to `src/bricks.cpp` and is now golden-tested against
the original: `test-bricks` paints each of the 15 levels and compares
19,296 brick body bytes against `build/level_gt/level_NN.scr`, byte-exact,
in milliseconds.

Its three boundary repairs are named operations (`repaint_row_body_top`,
`repaint_row_top_edge`, `repaint_row_attrs`) instead of `main.cpp`
reaching into the module's tables to re-derive them. `test-bricks` proves
a row-by-row repaint plus those repairs equals a full paint, on every
level — the property the incremental band rebuild depends on and that
known-bugs #1/#2 were violations of.

What remains in `main.cpp` is the level orchestration around it:
`render_brick_band` / `render_brick_band_rows` copy per-level attrs, reset
destroyed cells to background, and drive the border shadow.

### Take the least-coupled group next

Measured by how many file-scope names a group's functions touch:

| group | functions | distinct state names |
|-------|----------:|---------------------:|
| sound | 15 | 4 |
| hud / text | 13 | 12 |
| entities | 31 | 64 |

That ordering is also what unblocks replay fastest — every variable that
becomes a module's business is one fewer for `write_replay_probe` to
reach for.

### Stage 9: what run_level actually is

612 lines in three parts:

| part | lines | character |
|------|------:|-----------|
| setup — new game, score/lives, level override | 78 | linear |
| level entry — var reset, magnets, round banner, brick anim, replay hooks | ~127 | linear, two early exits |
| **the per-frame loop** | **398** | nesting 6 |

The level-entry prologue is a safe slice: linear code whose two
`return ST_QUIT` points become a `bool`. The frame loop is not — it wants
its own pass, and unlike every extraction so far it has no fast test
guarding it, only 10-second boots.

### Three verification failures, and what they have in common

Worth writing down because they repeated:

1. `test-death-sparks` greps source for code stage 3a moved to another
   module. Red for six commits. I was running `parity-check-parallel`,
   which excludes the source gates CI runs.
2. `parity-check-parallel` without `--full` is **8 of 51 gates**. Every
   "all gates green" in this refactor meant the fast core only. The full
   suite now runs green end-to-end in 341s (notes/testing.md), so there
   is no longer a cost argument for skipping it before a push.
3. `test-rocket-bonus` greps for `!ball2_active`, which the BallState
   grouping renamed. I reported that commit as "test-fast green" having
   piped test-fast's output past myself without reading it.

The common thread is not which suite — it is claiming a check's result
without looking at it. `make test-fast` is the answer to (1) and (3);
`--full` is the answer to (2); reading the output is the answer to all
three.

### Verify with `make test-fast`, not just the QEMU suite

`test-death-sparks` greps SOURCE for code that stage 3a moved to another
module. It failed for six commits and nobody noticed, because every local
check ran `parity-check-parallel` — which does not include the three
emulator-free source gates. Those were reachable only through CI.

`make test-fast` now runs every host test AND those source gates, in
seconds, matching exactly what CI checks. Run it before pushing.

### Stage 10: giving the state owners

~113 file-scope variables, grouped one cluster at a time into a struct at
file scope. Access stays direct (`bomb.y`, not a threaded `GameState&`),
so each cluster is a mechanical rename with no call-site churn — the
consolidation into one addressable state comes later, once the clusters
exist to consolidate.

Done: `ProbeState`, `RenderProfile`, `BallState`, `BatState`,
`BombState`, `MagnetState`, `PtsMarkerState`, `BrickFlashState`,
`BonusState`, `RocketState`, `PlayerState`, `StaticCache`.

`StaticCache` was not on the original list — the eight variables behind
the static-background cache (`static_bg_dirty`, `static_bg_cache_dirty`,
the brick-row dirty range, `force_full_flush`, and the three `prev_*`
values the HUD compares against) read as unrelated flags scattered
across the render code. They are one thing: what the cache holds and
what still needs rebuilding. The `prev_*` names became `drawn_*`, which
says what they are rather than when they were set.

Finding it after declaring the stage complete is the point: clusters are
easier to see once the surrounding code has names.

The five `last_primary_launch_*` variables then folded into `ProbeState`
as a nested `last_launch`. They were never game state — only
`record_primary_launch` writes them and only `write_replay_probe` reads
them — so they were harness state sitting in the middle of the ball
declarations. Five fewer loose names for stage 1 to account for.

`DebugSwitches dbg` then took the nine `BATTY_*` switches that change how
the port behaves — `auto_fire`, `use_laffc`, `rng_perframe`,
`suppress_no_ball_death`, the three `force_*_redraw` flags,
`full_band_rebuild` and `profile_auto_frames`. Unlike `ProbeState` these
DO affect play, which is what they are for, so they are a separate
struct rather than more of `probe`.

Moving them stranded the three long rationale blocks that had sat above
their declarations (the LAFFC default, the RNG per-frame model, the
band-rebuild A/B). Those moved with the switches. Writing "see the git
history" instead would have been the same mistake this file already
records twice.

`main` then gave its 30-line env preamble to `apply_env_switches`, which
returns the state to start in. Grouped by struct and aligned, the eight
plain presence checks read as a table; the two that parse a value
(`BATTY_RNG_PERFRAME`, which alone can force either state, and
`BATTY_PROFILE_AUTO_FRAMES`) now stand out instead of hiding among
them.

`write_replay_probe`'s five object dumps became `probe_write_object`.
The catch was that PROBE.TXT's newlines are written as a *prefix* of the
next key, not a suffix of the current one, so a helper that appends `\n`
also has to strip the leading one from the key that follows — otherwise
every gate parsing the file sees a blank line. Checked by diffing a real
PROBE.TXT against the pre-change one: byte-identical.

Five `apply_replay_*_override` functions then lost their hand-rolled
`strtol` chains to one `parse_replay_ints(name, out, count)` — four
`"x,y"` and the bonus's `"type,x,y"`, which was the same shape with a
third field. It is all-or-nothing by design: a typo in a gate's env
leaves the game untouched rather than seeding a half-parsed state.

The three object-blob overrides went the same way: already sharing
`replay_parse_hex_bytes`, their wrappers differed only by env name and
slot, so `apply_replay_object_override(name, slot)` replaces all three
and the dispatcher names which object each line seeds.

`redraw_full_with_ball`'s opening became `refresh_static_background`:
rebuild the cache whole, rebuild just the brick band, or restore from it.
The rule it encodes is worth stating once — a score change alone does
not force a full rebuild (the HUD top can be patched in place) but only
where no magnet overlaps the HUD rows, and a lives change always does,
because the indicators sit in the bat band rather than the patchable
strip.

The tidy version of that function was wrong in a way worth recording.
The original sets `bg_dirty = 0` inside the rebuild branch, so its later
`if (!cache.bg_dirty && score_dirty ...)` still fires after a full
rebuild; returning early "because the HUD is already current" silently
dropped that call. It is very likely redundant — the rebuild repaints
the HUD and sets `full_flush` — but likely is not proven, so the call
stays and the comment says why.

`compose_bat_full` and `bat_needs_full_redraw` followed. The predicate
is the interesting half: the bat needs its whole body redrawn not only
when it moves, but when it resizes, when a caught bonus changes the body
sprite, or when a laser fire-animation frame is playing. Five clauses
that only read as one idea once named.

`redraw_full_with_ball` is 123 -> ~75 lines, and what is left is the
compose order the original fixes ($9AD0 slot table) rather than
bookkeeping.

`compose_scene_no_objects` is the third scene composer, shared by the
death animation and the rocket-clear tally. It is deliberately NOT
`compose_level_scene`: it paints no bat and no magnets, and it *does*
paint the brick flash and hit animations, because those sub-loops drive
their own frames with no dirty carry to restore them from. Folding the
two together would have silently added magnets and the top-frame repair
to both.

`flush_composed_frame` took the tail. The non-obvious half is the full
flush: it still copies this frame's dirty rects forward before calling
`mark_all_dirty()`, because that call widens what goes out *now*, not
what the next frame restores from. Inline, the copy loop looked like
something the `mark_all_dirty()` on the next line made redundant.

`rest_ball_on_bat` then removed the last copy of the ball-on-bat rest
rule. `step_ball`'s stuck early-out and `ride_stuck_ball_on_bat` each
carried it, with a long comment apiece explaining the same `$A6`/`$A7`
MAGNET offset — the shape that lets two copies drift. Both anti-
regression notes survive in the one place: the 1 px MAGNET drop, and
that `BALL_H_PX` is deliberate where the effective ball size would put
the ball at 165 and clobber `respawn_primary_ball`.

`catch_ball_on_bat` followed, and the surrounding block had two comments
that had stopped being true. Its header still described "a 5-zone
deflection" that `bat_deflect_dir`'s exact LAB1F port replaced, and
`hit_x`/`span` were computed, silenced with `(void)` casts, and
annotated "retained only for the catch branch above" — which does not
use them. Both deleted.

That is a third comment failure mode, after orphaning and drift: a
comment that stayed attached to its code while the code beneath it
changed meaning. Nothing catches these either.

`lose_a_life` and `lose_primary_ball` closed out `step_ball`. The
life-loss rule — explode, decrement, respawn — existed in both
`step_ball` and `handle_no_ball_death`; the two `if (player.lives > 0)`
checks are separate on purpose, since with the last life gone there is
nothing to respawn onto.

Sharing it broke `test_rocket_bonus.py`, which asserts the rocket-safe
guard is followed by the explosion. The gate now asserts through the
helper *and* that the helper still explodes the bat, so the assertion is
no weaker. `check_gate_greps` reported PASS throughout — see
notes/testing.md for why.

`step_bonus` did four unrelated things behind one name: the BIG_BAT
timer, the bat width ramp, the BIG_BALL timer, and the falling bonus
itself. It is now `tick_bat_resize` (timer and ramp together, since the
timer sets the target the ramp chases), `tick_big_ball_timer`,
`spawn_pts_marker`, and a `step_bonus` that steps the bonus.

A third dead `(void)`-silenced local turned up there — `caught_type`,
whose comment explained that every catch uses the +400 marker including
SCORE_5K. The fact was worth keeping; the variable was not.

`PlayerState` needed a different tool from the rest. `score` and `lives`
are English words that occur in comments and — the trap —
in `fprintf(f, "score=%06lu\n", score)`, whose *literal* is the
PROBE.TXT key a dozen gates parse. A word-boundary rename over the whole
file rewrites that key and every probe-reading gate fails at once, with
nothing pointing at the rename. `scripts/rename_code_only.py` segments
the source into code / string / char / comment runs and substitutes only
in code.

Each rename must be checked against the source-grepping gates first —
`make test-gate-greps` names the ones that would break, in a second.

### The step that unblocks the rest

What remains after stage 9 is not extractable by lifting. `bonus_apply`
is 180 lines touching 25 state names because it is where every subsystem
meets: balls, bat, lives, magnets, the rocket. The frame loop is the
same. Neither becomes a module by moving it.

They need the game state itself to become addressable — a `GameState` the
modules take by reference, instead of ~100 file-scope variables. That is
also what finally unblocks replay: `write_replay_probe`'s 50 loose reads
become a handful of queries on a struct.

`run_level`'s prologue is now three named steps — `new_game_reset`,
`probe_init_from_env`, `initial_round_number` — leaving the per-frame
loop as what is actually left to decompose. Inside it, the launch and
frame replay checkpoints were two identical eight-line blocks; they are
now one `probe_checkpoint_due` predicate called twice.

`||` short-circuiting there is load-bearing, not incidental: the first
checkpoint to fire returns, so the second never ticks its countdown —
which is exactly what the separate inline blocks did.

`step_active_entities` then took the twelve-call run of `step_*` plus
the two timer decays — everything that moves independently of the
primary ball. `kill_enemies_by_balls` followed: three near-identical
blocks differing only in which ball slot and which liveness flag, now
one call per ball over a shared `kill_enemy_by_ball_slot`.
`entities_need_redraw` replaced twelve consecutive `if (x) ball_moved =
1;` lines with the question they were collectively asking: is anything
besides the primary ball drawn over the playfield this frame.
`award_score_milestones` and `roll_high_score` then split two unrelated
ideas that were sharing a stretch of the redraw bookkeeping.

The milestone loop and the HI roll-forward have **no dedicated QEMU
gate** — `test_scoring.cpp` covers the pure `lives_earned`, and nothing
plays long enough to cross a threshold in an emulator. Changes there
rest on being pure code motion, not on a gate.

`handle_no_ball_death` was last. Extracting it tripped
`test-gate-greps`: `test_rocket_bonus.py` greps for the breadcrumb
`before balls_quantity`, and rewrapping the comment split that phrase
across two lines. A comment reflow is enough to break a source needle,
which is the argument for the check running in a second rather than
only in CI.

`ride_stuck_ball_on_bat` took the last inline block: the ball's ride on
the bat between a catch and its launch.

Named so far, in frame order: `handle_input`, `ride_stuck_ball_on_bat`,
`probe_checkpoint_due`, `step_active_entities`, `handle_no_ball_death`,
`kill_enemies_by_balls`, `award_score_milestones`, `roll_high_score`,
`entities_need_redraw`. What is left in the loop body is the frame tick,
the RNG/magnet sampling that must stay ordered against it, and the
redraw path selection.

### The two redraw paths were drifting apart

`redraw_full_with_ball` and the dirty path each carried their own copy
of the bomb / +400 / bonus blit-and-mark blocks — thirteen identical
lines, twice. That is the shape known-bugs #1 and #2 had: two paints of
the same thing, one of them updated. `render_falling_objects_to_buff`
makes it one.

The sweep found one more identical block — the multi-ball extras, now
`render_extra_balls_to_buff` — and one genuine divergence: the two paths
mark different rect heights for a bullet blast — resolved since, by
measuring the sprite instead of guessing: known-bugs #9.

`redraw_ball_only` and `redraw_ball_with_simple_objects` were eighteen
identical lines apart from one call and one profile counter; both are
now thin wrappers over `redraw_ball_dirty(level_idx, with_objects)`.
The two names stay, so the call site still reads as a choice between
two paths rather than a bare boolean.

The enemy was the largest copy: 29 lines inline in the full path
duplicating `render_enemy_to_buff_and_mark`, including the whole
known-bugs #7 rationale twice. The full path now calls the helper.

What is left differing between the paths: the full path paints the brick
flash and hit animations where the dirty path relies on the carry (that
one is deliberate), and the bullet render's guard, which is not —
known-bugs #10.

`bonus_apply`'s KILL_ALIENS arm became `blast_active_alien`, and the
ROCKET arm's object-hiding became `hide_objects_for_rocket_clear` —
that one moved a needle in `test_rocket_completion_no_ball.py`, which
greps the block with its exact indentation.

The MULTI_BALL arm split three ways. The direction derivation was pure,
so it moved to `physics.cpp` as `extra_ball_dirs` and picked up two host
tests — quadrant preservation across all 64 inputs, and the three-way
low-nibble split with a check that the two extras never share a
direction. The two eight-line spawn blocks became one
`spawn_extra_ball`.

That is the first new coverage on known-bugs #8's territory: the extras'
*derivation* is now pinned, even though which quadrant convention is
right still needs an oracle.

The ROCKET arm's remaining body became `attach_rocket_to_bat`. Its two
long comment blocks collapsed into one header — the placement rule
(`bat_x + 4`, `+ 12` when big, `bat_y + 6`) and the `INC (IY+$14)`
quirk that makes the ROCKET catch cancel any prior bat-side bonus, which
is the reason `bonus_apply`'s universal assignment skips ROCKET.

`bonus_apply` is now 180 -> ~85 lines: a sound, one assignment, and a
switch whose arms are named calls or two-liners.

`play_bat_explosion` then gave up its two setup blocks —
`clear_objects_for_death` (the LBC10 slot sweep) and
`spawn_death_sparks` — leaving the function as the PIT-driven sub-loop
it actually is. A third orphaned comment went with them: the
`hl_bc_calc_direction` note, stranded since stage 3a moved that maths to
`physics.h`.

Coverage note: `test-death-sparks` is source-grep only, 0.1s, no boot.
Its seven needles are content-based so they survived the move, but
nothing renders the death animation in an emulator — this rests on code
motion.

### Comments drift away from what they document

Four have turned up now. Three were orphans — prose left behind when
stage 3a/6b-iii moved the code to another module
(`hl_bc_calc_direction`, `bonus_to_original`, and the
`bonus_apply` header my own extractions pushed off its function). The
fourth was different: `brick_collision`'s documentation had drifted 150
lines above the function as other code was inserted between them, so it
read as a header for `try_spawn_bonus`.

Both failure modes are invisible to every gate — the compiler does not
care and no test reads prose. Two heuristics for finding them
automatically produced only noise (a comment ending before a blank line
matches every section banner; matching identifiers against other modules
matches English words like "pixel" and "queue"). They were found by
reading. If a cheap check exists it has not been found yet.

Deleting historical narrative wholesale is *not* safe: most "Earlier port
used X" notes record a fixed bug and stop it coming back. Only the two
that documented removed code were dropped. Note the
coverage limit: `test-bonus-effects` checks only that the catch sets
`bat.bonus_applied = $09`, so the blast body — alien to sprite_set $0A,
+350, SND_ALIEN_BLAST — is not gated by anything. That arm rests on
being pure code motion.

The pattern across #9 and #10 is worth naming. Two copies of a render
block look like duplication to delete, but each divergence has to be
*resolved* before merging, not averaged. #9 was answerable by measuring
the sprite; #10 needs the original. Merging blind would have picked a
winner silently in both cases.

Order from here:

1. the frame loop, into named phases
3. game-state consolidation — the real prize
4. replay, which falls out of 3

### Session log

Each entry below is one commit's worth of reasoning. Until the headings
were added this was a single 700-line block titled after ONE of its
entries ("Stage 1a"), which is what made it unnavigable.

Do not read position as chronology. Entries were mostly prepended, so
the run from `BATTY_REPLAY_SCORE` down to `show_round_banner` is roughly
newest-first — but `Stage 1a` and its neighbours at the top are among
the OLDEST, and I got that wrong in the first draft of this very
paragraph. Use the headings, not the order. Nothing was reordered or
removed when they were added; a multiset diff confirmed zero lines
lost.


#### The new tool paid for itself in one sitting

Used `disasm.py` on the last enemy gap — what `CALL LAFFC` does for the
bird — and it turned out to depend entirely on WHO called it:

    LAFFC_30:                      ; the on-hit path
      LD (flag_2),A                ; "something was hit"
      LD A,(IX+$00) / AND $7F
      CP $02 / JR Z,LAFFC_32       ; BALL   -> destroy and score
      CP $05 / JR Z,LAFFC_31       ; BULLET -> its own path
      AND $FE / CP $08 / RET NZ    ; bird/UFO -> on; anything else RET
      LD L,(IX+$02) / LD H,(IX+$04) / LD (LAA7B),HL

So the alien never reaches the destroy path — that is `sprite_set $02`
only. That is the MECHANISM behind last week's grid capture showing no
cell changing, where before I had only the observation.

What it does instead is latch its own position into `LAA7B`, which is
the word `handling_bird` tests at its top to divert onto `LAA44`, the
homing routine. A brick hit puts the alien into a different mode. And
the tail re-targets randomly off `flag_2`.

The port implements none of it. So the last enemy gap is now specified
down to the branch: hit DETECTION without `brick_hit_resolve`, the
`LAA7B` latch, the `LAA44` branch, the `flag_2` re-target, and LAFFC's
band guard.

Three greps, no emulator. The tool was written to make tracing cheaper
than guessing and the first thing it produced was the answer to a
question two captures had only circled.

#### Making the disassembly cheaper to reach than a guess

Three of my hypotheses died on `original/disasm/batty.asm` in a week, and
I noted the pattern twice without doing anything about it. Part of the
cause is friction: reading a routine meant
`sed -n "$(grep -n '^name:' ...)"` and counting lines by eye, which I
did clumsily half a dozen times this session.

`scripts/disasm.py` takes a label, an address (via the
`; Routine at XXXX` headers), or `-l` to list labels containing a
substring. A routine prints from its label to the next, with the
FOLLOWING routine's comment header trimmed — that block describes what
comes after, and leaving it in was the one thing wrong with the first
version.

Not a gate, and deliberately so — this is the opposite kind of tool. The
last few entries have been about not adding gates that cannot pay their
way; this one lowers the cost of the thing I should have been doing
first anyway. Whether it works is measurable in whether the next
hypothesis gets traced before it gets written down.

#### Both halves of the "dir $00 moves UP" puzzle were mine

Last entry left an open observation: the original might drive enemy
motion by `(dir - $10) & $3F`, because an alien with `dir` pinned to
`$00` moved upward. Traced it instead of leaving it, and both halves
dissolved.

**The `$10` rotation is a FACING test, not motion.** `LAA02` is
`LD C,$00` with the `$00` patched, so `C` is the rotated dir from before
the move; the code recomputes it after the move, `XOR`s, and tests bit 5.
On a change it runs `LD A,(IX+$01) / XOR $07 / ADD A,$07` — `(IX+$01)`
is the sprite number. It selects which way the alien is drawn. `LAD69`
still gets the raw dir, exactly as the port does.

**The upward drift was my experiment.** I pinned the target to `$00` to
stop steering interfering, with `dir` also `$00` —
`enemy_turn_towards_target` treats `delta == 0` as ARRIVAL and re-picks
a random target. Pinning the target to the current dir is the one value
that guarantees the steering fires. The alien flew toward whatever it
drew.

So the port is right here and I invented the anomaly. Worth the two
greps: the alternative was leaving "the enemy's dir may be read with a
`$10` rotation" in the notes for someone to act on. That is the third
hypothesis of mine this week to die on contact with the disassembly, and
the pattern is consistent — the guesses were plausible, the traces were
cheap, and I did the guessing first every time.

#### Checking my own capture was on the right code path

An anomaly in a follow-up run nearly retracted the previous entry.
Driving the alien with `dir` pinned to `$00` moved it UPWARD, which
`dir_to_dxdy` says is impossible — `$00` is straight right. The obvious
worry: `handling_bird` branches on `LD HL,(LAA7B) / LD A,H / AND A`, and
a non-zero high byte diverts to `LAA44`, a HOMING routine that never
reaches `LAFFC`. If the brick capture had run down that branch, its
result would have been about a path that cannot damage bricks anyway.

Read `$AA7B` in the L3 state: `HL = $0000`. The Z branch is taken,
`LAFFC` IS called, and the previous finding stands. Worth the one
capture — the alternative was a published result resting on an
assumption I had not tested.

The anomaly is real though, and now recorded as an open observation
rather than explained away. `handling_bird` does
`LD A,(IX+$06) / SUB $10 / AND $3F` and patches the result into
`LAA02+$01` before moving, so the original's enemy motion may be driven
by `(dir - $10) & $3F` — and `$00 - $10` is `$30`, which is up. The port
passes the raw `dir` to `dir_to_dxdy`. If that offset is real, the
enemy's direction byte and the ball's are read with a `$10` rotation
between them.

Not chased further: `LAA02` is self-modifying code and what consumes it
has not been traced. Guessing at it is how the `$38` hypothesis went
wrong.

#### Does the original's alien eat bricks? Measured: no

Last entry flagged the risk in wiring `LAFFC` into the bird: the port's
`laffc_collision` also calls `brick_hit_resolve`, which destroys and
scores the cell. Settled it by capture rather than leaving it as a
caution.

`--poke-at-frame` puts the alien at (24, 40) — brick row 1 column 1,
solid `$06` in the L3 grid — and `--probe-grid 0x6E43` dumps all 180
cells per checkpoint. Over frames 10..26 the alien drifts y 40 -> 28,
straight up through brick rows 1 and 0, and the grid is IDENTICAL at
every capture. Not one cell changed.

So the original's alien passes through standing bricks: no damage, no
score, no visible bounce here either. Whatever `CALL LAFFC` does for the
bird, it is not the ball's destroy-and-score path — and a ported bird
must not call `brick_hit_resolve`.

I verified the alien's POSITION separately before believing the grid.
An unchanged grid means nothing if the poke silently failed, which is
exactly how the first right-margin capture went wrong three entries ago.

Stated limit: one scenario, one direction. It establishes that the alien
does not destroy bricks. It does not establish that LAFFC never deflects
it, and what the bird should do on contact is still open.

#### handling_bird decoded, so the last enemy gap is specified

`parity-gaps.md` said "the bird doesn't yet run LAFFC brick collision or
the exact check_margins" — true, but not enough to act on. The
disassembly gives the whole sequence:

    entry slide (y < 8)  ->  bomb_appear  ->  steer every 4 frames
    ->  LAD69 (move)  ->  LAFFC (brick collision)  ->  check_margins
    ->  deactivate below y=$C0

The port matches five of those seven and diverges at exactly two
CONSECUTIVE calls: it runs no brick collision for the bird, and it
substitutes a reflect-and-re-aim for what is only a clamp.

Two things that a future implementation needs and that "doesn't yet run
LAFFC" did not say. The ORDER matters — brick collision before margins,
because LAFFC can move the alien into one. And the port's
`laffc_collision` also calls `brick_hit_resolve`, which destroys and
scores the cell; that is right for a ball and unverified for an alien.
Wiring it in naively would have aliens eating the level.

Recorded rather than implemented. The change is small in lines and not
small in consequence, and the brick-damage question needs settling
first — which is a capture, not an opinion.

#### A class I decided NOT to gate

Four times now a duplicated comment has drifted and misled: the bricks
header copied into its `.cpp`, two blocks in front of the SPACE handler,
the RNG default saying OFF after it flipped, and the bat-resize note
still saying "roughly matches" long after the gate that made it exact.
The last cost an afternoon chasing a bug that was not there. Every other
recurring class this month became a gate, so this should too.

It should not, and finding out why was the work:

- An exact-sentence scan over `src/*.{cpp,h}`, within files as well as
  across them, reports ZERO duplicates. All four real cases were
  PARAPHRASES. A sentence gate would have been green for every one of
  them — a gate that cannot catch any of its own motivating examples.

- Provenance-address co-citation is the better signal and still too
  noisy: 47 original addresses are cited from two or more comment
  blocks, and nearly all are legitimate — a declaration summary plus an
  implementation detail, or several call sites naming the routine they
  port. `$A67B` appears four times, all correct. I checked those by hand
  rather than assuming, including the two I had just written.

So `notes/testing.md` now records the class as deliberately un-gated,
with both failed approaches. A gate here would fire constantly, get
switched off, and leave everyone worse off than an honest note. The one
mechanical sub-case — a documented default versus the initialiser — is
already `test-switch-defaults`.

Worth saying plainly after a month of adding gates: the answer is not
always another gate.

#### The bat resize was more faithful than its own notes said

Taking the disassembly to the next parity gap — "big-bat resize timing
is matched visually but not a literal port". Decoding `bat_resize`
($9D2C): the body grows 2 px every OTHER frame (`RR E` on
`counter_misc` returns on the odd parity), from `$1C` (28) to `$2C`
(44).

I worked out from the code that the port grew twice as fast, and was
wrong: `tick_bat_resize` already has an every-other-tick gate, correctly
documented right there — "ungated it grew twice as fast as the
disassembly prescribes". Someone had already found and fixed this.

What was actually wrong was a SECOND copy of that explanation 2,300
lines earlier, saying the port "roughly matches" at 1 px/tick. Written
before the gate went in, left behind after. The same
two-copies-one-stale pattern as the bricks header and the SPACE-key
comment, and it is what sent me looking for a bug that was not there.

`parity-gaps.md` had inherited the same understatement. Both now say the
rate and endpoints are EXACT, with the arithmetic (8 steps of `extra_px`
per side over `BAT_BODY_W = 28` gives 28 -> 44 in ~16 frames), and name
what genuinely is not literal: the original interleaves an x adjustment
plus `check_margins` on the alternate frames, where the port keeps
`BAT_X` fixed and widens symmetrically. Same visible extent, different
means.

A gap listed as open for months was mostly closed, and the record said
otherwise because a duplicated comment outlived its subject.

#### Guarding a fix no QEMU gate can see

The #14 fix went in with the full suite green both before and after,
which is precisely the problem: nothing there observes it. No scenario
reaches a multiball spawn from a primary whose low nibble is not `$04`,
so a revert would be invisible to all 59 gates — the same way the bug
was.

`test-multiball-source` guards it as source, checking two things. That
`extra_ball_dirs` is given `objects[OBJ_BALL_1].dir`, and that
`delta_to_dir` has no production caller anywhere in `src/`.

The second is the sharper one, and the reason for writing it that way:
`delta_to_dir` now exists only for its host tests, so a call to it
reappearing in production IS the round trip coming back — whatever
function it turns up in, and whatever it is passed. A gate naming only
`apply_multi_ball_bonus` would miss the same mistake made somewhere
else.

Mutation-checked both ways: restoring the old expression, and sneaking
`delta_to_dir` back in under a different guise.

#### #14 fixed, applying #16's lesson straight away

#16's lesson was that the disassembly answers questions I was paying
emulator runs for. Applied it to the other open item immediately, and it
took one grep.

#14's open half asked whether the original derives the extra balls'
launch angle from the ball's real VELOCITY. LA67B_8 (`$A67B`):

    LD A,(IY+$06)   ; the primary's DIR BYTE
    AND $0F         ; low nibble picks the branch
    ...
    AND $30 / OR E  -> ball2   ;  AND $30 / OR D -> ball3

No velocity anywhere. It reads the dir byte and splits it.

`extra_ball_dirs` was already a faithful port of that table — the bug was
its INPUT. The port passed `delta_to_dir(ball.dx, ball.dy)`, a dir
reconstructed from the {-1,0,+1} sign cache. `delta_to_dir` picks its
angle with `abs(dx) >= BALL_SPEED`, and a sign is never >= 2, so the low
nibble came out `$04` EVERY time. The port always took the first branch
where the original varies with the primary's actual angle.

One line: pass `objects[OBJ_BALL_1].dir`, which is what `(IY+$06)` is.
The round trip is gone and the sign cache is no longer involved in the
multiball spawn at all — which also retires the last production caller of
`delta_to_dir`.

Full suite 59/59 green after the change, and that is itself the
explanation for how it survived: no gate reaches a multiball spawn from a
primary whose low nibble is not `$04`. The three tests added for this
area over the last week all pinned the round trip's behaviour, not the
original's.

#### #16 resolved, by reading the disassembly I already had

Four emulator runs narrowed #16; `original/disasm/batty.asm` finished it
in one grep. It was there the whole time.

    check_margins:   three CLAMPS, nothing else   <- handling_bird/_ufo
    bounce_wall:     the same three, plus change_direction on carry
                                                  <- handling_ball/_spark

Two routines over the same checks. The ENEMY gets the clamp-only one;
the reflecting one belongs to the ball. So the original never turns an
alien at a wall and never re-aims it — which is what the captures showed,
now with a reason.

`check_right_margin` also explains the wrap I measured:

    LD A,(IX+$0C) / ADD A,(IX+$02) / CP $F9 / RET C
    LD A,$F8 / SUB (IX+$0C) / LD (IX+$02),A

The bird's width is 24, so the clamp fires at `x >= 225` and sets
`x = 224`. But that `ADD` is 8-BIT: from `x >= 232` the sum passes 255,
wraps below `$F9`, and the clamp silently does not fire. An overflow
escape window in the original — and my poked alien at x=240 sat inside
it.

Falsifiable prediction, then tested: an alien at x=228 is inside the
WORKING window and must clamp to exactly 224. It did, with `dir`
unchanged through the clamp.

The port's clamp VALUE matches the original's (224 both ways). The
reflect, the re-aim and the absence of the overflow window are all port
inventions. So #16 stops being "is this constant wrong" and becomes a
recorded design question: reproducing the original means reproducing an
overflow that lets an alien cross the screen edge. Nothing anywhere says
that choice was ever made — it looks like ball-style bouncing was simply
reused for the enemy.

The lesson is cheaper than the finding: I ran the emulator four times
before grepping the disassembly sitting in the repo.

#### What the original actually does at an alien's right edge

One more run past x=255 settled the shape of #16, and it is not the
shape I started with.

    frame 29  x=254  dir=$3C  target=$2C
    frame 30  x=255  dir=$3C  target=$2C
    frame 31  x=8    dir=$3C  target=$2C

The alien runs off the right, its x byte overflows, and the original's
own LEFT clamp catches it at 8. Across the entire run it never re-aimed
(`target` is `$2C` from frame 10 to 44), never reflected `dir` (`$3C`
straight through the wrap), and never clamped at the right edge at all.

The port does all three: clamp to `x_max`, reflect with
`(0x20 - dir) & 0x3F`, and call `enemy_target_away_from_margins`.

So the question #16 opened with — is the escape angle `$38` or `$18`? —
is the wrong question. That angle table belongs to a re-aim the original
does not perform. Two of six entries aiming outward is real and
self-inconsistent, but it is a detail of a port invention.

Still not fixed, and the reason has improved again. One artificial
scenario shows what the original does THERE; it does not show that
`check_margins` never fires anywhere. And the port's clamp may well have
been added deliberately — an alien whose x wraps to the far side of the
screen is startling. That reasoning is recorded nowhere, which is the
actual gap.

Three measured differences at an alien's right margin, where one
suspicious constant was previously noticed. Worth the four emulator runs.

#### The capture tool gained a mid-run poke, and #16 got bigger

The blocker from last entry was precise, so it was fixable:
`capture_frame_timeline_original.py --poke-at-frame FRAME:ADDR:BYTES`
writes memory after reaching a frame and before its probe. Setup ops run
before the game spawns its objects; this runs after.

It refuses a poke frame that is not also a capture frame. That is not
tidiness — a poke only fires when the loop STOPS at that frame, so
naming a frame outside `--frames` is a silent no-op, and the run then
looks entirely normal while measuring unpoked state. I made exactly that
mistake and read x=167 where I expected x=240, which is the only reason
I noticed.

Then the measurement, which did not say what I expected. With the alien
placed at x=240, y=4, dir=`$00` and stepped:

    frame 14  x=240 dir=$00  target=$2C
    frame 30  x=255 dir=$3C  target=$2C

The target NEVER CHANGES. The original does not re-aim anywhere between
x=240 and x=255; `dir` just walks toward the `$2C` it already had. The
alien is not clamped or bounced either — it travels to x=255.

So "does the original pick `$38` or `$18` at the right edge?" has no
answer at these coordinates, because the original picks nothing here.
The port re-aims where the original does not, which makes #16 larger
than an angle table and smaller than a bug: two behaviours differ, and
this capture does not reach the original's own margin threshold to say
which is right.

#### Trying to settle #16 against the original

Having said #16 was bounded rather than blocked, the next step was to
prove that by doing it. Partly done, and the failure is the useful half.

WORKS: the oracle path runs in this environment.
`capture_enemy_flight.py` frame-steps the original's `object_enemy`, and
the target byte is readable by offsetting the probe —
`--probe-ball 0x9BA8` puts `$9B96+$14` in the printed `x=` field.
Baseline on the unmodified L3 flight: target `$10` for the first frames,
`$2C` later. The `$10` matches the fresh alien's documented target,
which is what confirms the offset rather than assuming it.

DOES NOT WORK: poking the alien to the right edge through a replay's
`write_memory` SETUP ops. The L3 state spawns a FRESH alien during the
run — x=168 y=1, exactly as `test-enemy-descend` documents — so a
setup-time poke is overwritten before the first probe. Verified rather
than assumed: with `$9B98`/`$9B9A` set to `$F0`/`$04`, the capture still
reads x=168 y=1.

So #16 needs a MID-RUN poke: write the position after the spawn, then
step. `capture_frame_timeline_original.py` runs its setup once, before
stepping, so this is a change to the capture TOOL rather than to the
game. The entry says that now, with the working half recorded so the
next attempt starts from the blocker instead of rediscovering it.

#### My own hypothesis for #16, refuted from the repo

I had written that `$38` looked like a transcription slip for `$18` out
of LAA7D — one bit apart, and the symmetry fits. Before going near the
emulator I checked what the decode actually says, and
`notes/enemy-movement.md` lists in its own open items:

> **Margins.** The original's `check_margins` vs the port's
> `enemy_target_away_from_margins` is still an approximation.

There is nothing to have slipped from. These six angles are the PORT's
invention.

That makes the finding sharper rather than weaker. The routine exists to
aim an alien away from a wall — its header says "so it cannot grind
along an edge" — and two of its six cases aim into one. It is
self-inconsistent on its own terms, provable without a capture. And
since the routine runs immediately AFTER the bounce reflects `dir` off
that wall, at the top-right the alien is moving left while being steered
up-right, back into the corner it just left.

Still not fixed, and the reason changed with the diagnosis. It is no
longer "the original might do this"; it is that swapping `$38` for `$18`
trades one approximation for another. More self-consistent is not closer
to the original, nothing here measures the original's margin behaviour,
and the port has no gate that would notice either way.

Also checked, since #16 said it needed hardware: `tools/zesarux/src/zesarux`
IS built and present, the replays support `write_memory` setup ops, and
`capture_enemy_flight.py` already frame-steps `object_enemy`. So this is
bounded work, not blocked work — #16 now says so instead of implying a
wall.

#### The $38 oddity, run down

Last entry left "the `$38` escape angle looks wrong" as an aside. It is
worth more than an aside, so I measured the whole convention instead of
reasoning from one value.

`dir_to_dxdy` over all eight octants gives `$00` right, `$10` down,
`$20` left, `$30` up — anchored by `$10` being straight down, which
matches the note in `handling_bird_obj` about an earlier port getting
that exact direction wrong. So `$38` is up-right.

Against that, four of the six margin-escape angles aim inward and two do
not: the right edge's upper case and the top edge's right half both use
`$38`, which is outward on the x axis at the right edge and outward on
the y axis at the top. The top-right case is both at once. `$18`
(down-left) would be inward for both and differs by one bit.

Recorded as known-bugs #16, NOT fixed. There is no capture of the
original at a right-edge alien, the routine mirrors LAA7D, and a 1980s
routine that lets an alien clip a corner is entirely plausible. Changing
it on a symmetry argument would replace measured behaviour with a guess,
which is the mistake this file already records twice.

The useful part is that the test written last entry pins all six values,
so whoever does get the capture will find the test failing and have to
decide explicitly.

#### "margins_aim_inward" did not check that it aimed inward

Fourth instance of the shape, and the most explicit promise of the four.
`test_margins_aim_inward` asserted that the margin path was TAKEN
(a counter went up) and that the result was 6-bit. Neither says where
the alien is now pointed, so swapping the two left-edge escape angles
survived — the alien would turn the WRONG WAY at an edge, which is the
one thing this routine exists to prevent.

The obvious fix — assert the target aims away from the wall by sign —
was rejected after measuring. `dir_to_dxdy` gives `$38` a POSITIVE dx,
yet `$38` is the RIGHT edge's upper-corner angle. Either the convention
is subtler than it looks or something is off there; after the
19200-bounce lesson, building an assertion on a convention I had not
verified was the wrong move.

So the escape angles are pinned as a VALUE TABLE, the way the bat
deflection table is: six (edge, half) → angle pairs from LAA7D. Plus a
bracketing pair for the threshold, since the existing cases sit at x=4
and pass whether the margin is `x <= 8` or `x <= 9`. Three mutations
caught: left angles swapped, top halves swapped, threshold moved.

The `$38` oddity is left recorded rather than chased — it may be correct
and merely counter-intuitive, and I have no ground truth to say
otherwise.

#### A bounce that changed direction, on any axis it liked

`test_bounce_changes_direction` swept the whole band and asserted the
direction CHANGED. It never asked on which axis — so mutating the LEFT
face's reflect mask from `$1F` to `$3F` survived, which would send a
ball hitting a brick's side away vertically. Third instance now of the
same shape: a test that checks something happened rather than that it
happened correctly.

Extended in place, and as a PROPERTY rather than a restatement: a
horizontal face must flip the sign of dx and preserve dy, a vertical
face the reverse. Repeating `laffc_change_dir(dir, 0x1F)` in the test
would only prove the test can copy the code.

The first attempt reported all 19200 bounces wrong, which is a result
about the test, not the code. It read the signs through `dir_to_delta`,
whose quadrant convention is MIRRORED from the ball's in two of four
quadrants — that is known-bugs #8, recorded in this very repo. Switching
to `dir_to_dxdy`, the motion the ball actually uses, gives a clean pass
and catches both the left-face and up-face mask swaps.

Worth remembering: an assertion failing everywhere is usually the
assertion. 19200 of 19200 was never going to be a real defect.

#### Both straddle boundaries were off by an inclusive

Sweeping the physics functions the earlier passes had not touched. The
direction gate and `change_dir` are caught; the two straddle boundaries
were not.

When the ball's own cell is gone, LAFFC_5-6 tries the neighbour if the
body reaches into it: `(x_pen_in_cell + ball_w) >= BRICK_W_PX`. The `>=`
is the whole question — with `>`, a ball whose body ends EXACTLY on the
cell edge stops straddling, misses the standing brick next door and
passes through it. That is the same failure family as known-bugs #6, and
neither the horizontal nor the vertical form was pinned.

Both bracketed by measurement rather than argument. Horizontal, 8px
ball, own cell col 2 destroyed and col 3 standing: penetration 6 and 7
miss, 8 and 9 straddle. Vertical, 4px-tall ball, r2c2 gone and r3c2
standing: 3 misses, 4 straddles. So 7/8 and 3/4 bracket the two
boundaries exactly, and the test asserts both sides of each — a test
that only checked the hit would pass with the boundary moved either way.

My first probe of the vertical case started its range past the boundary
and showed six identical hits, which says nothing. Widening it to
straddle the value is what made it a measurement.

#### A test named for the property it did not check

Closing the laffc question. It turned out `test_physics` already had a
test called `boundary_faces_stay_open` — and it never looked at
`face_mask`. It asserted that a hit OCCURRED on a boundary cell and
named the right cell, which is why inverting the boundary term survived
it. A name can carry all the reassurance of coverage without any of it.

It also used dir `$28` for the left case, which the direction gate
strips bit 1 from — so the left face could not have been asserted even
had someone tried. Measured rather than reasoned: with every brick
standing, a left-boundary cell reports its left face open for dirs
`$00-$0C` and `$30-$3C`, closed for `$10-$2C`.

Fixed in place rather than by adding a second test beside it — a
duplicate would have left the misleading name in the suite. It now
asserts the face bits on the top and left edges, plus an INTERIOR
negative control so neither check can pass by being always-open.

And the honest part. The `cell_x == FIELD_X0` term is REDUNDANT:
`BrickField::standing` treats out-of-range as gone, so
`!standing(row, -1)` is already true at the edge. Deleting the term
survives, correctly — an equivalent mutant, now listed as one. What the
interior control catches is the INVERSION, which is bug #6's actual
shape. A green run here does not prove the boundary term is
load-bearing, and the test says so.

#### Re-verifying the two published claims

Finding that `mutate.py` could report on a stale DOS EXE put two
already-published claims in doubt, both of the form "all 59 QEMU gates
pass with this broken". Each was the evidence for a gate I then added
and, in one case, for a row in `known-bugs.md`. If the EXE had been
stale, some gate might have caught the mutation after all and the claim
would have overstated a coverage gap.

Both re-run with every `build/*.obj` and `build/batty*.exe` deleted
first, and the EXE's md5 checked to differ from the clean build so the
mutation demonstrably reached the binary:

  - entry slide `y < 8` → `y < 9`: only `test-enemy-descend` fails, and
    only because of the frame-8 checkpoint added for it. The other 58
    pass. The claim holds.
  - shimmer wrap `& 0x0F` → `& 0x1F`: all 59 pass. The claim holds, and
    so does the `known-bugs.md` row saying #3's fix was unguarded.

Neither needed correcting, which is the boring outcome and the one worth
having checked. A claim published on evidence from a tool later found
faulty is not automatically wrong, but it is unverified until re-run —
and "it probably still holds" is not a result.

Still open and still unclaimed: whether `laffc_sweep`'s left-boundary
term is guarded by anything. One trajectory gate does not catch it;
whether any of the other 58 do would cost a third full-suite run.

#### mutate.py was reporting on stale DOS builds

Sampling further into the QEMU layer, a mutation of `src/physics.cpp`
came back SURVIVED. Before writing it up I checked whether it had
reached the binary. It had not: `physics-test.obj` changed (md5-verified)
while `build/batty-test.exe` stayed byte-identical, because the relink
landed inside the same filesystem second. The gates had run the ORIGINAL
code.

`mutate.py` deleted `build/test_*` — the HOST suites' binaries — and
nothing else. For a QEMU gate the artefact that matters is the DOS EXE,
and it was never cleared. It now removes `build/*.obj` and
`build/batty*.exe` as well.

So the two findings already committed needed re-checking, not assuming.
Both hold: the entry-slide mutation is caught by the new descend
checkpoint, and the shimmer wrap by the new source gate (which touches
no EXE at all, so it was never at risk). Re-ran both against the fixed
tool rather than reasoning about whether they could have been affected.

What is NOT established is the physics result. With a guaranteed-fresh
build, inverting `laffc_sweep`'s left-boundary term still survives
`test-laffc-ball-frame1` — but that is one trajectory, and it may simply
never decide a left-boundary cell. Recorded as unverified rather than
claimed as a gap; proving it needs the full suite, which is six minutes
for one bit.

This is the third distinct form of the stale-artefact trap, after the
same-second host binary and the wrong binary NAME. Each time it produced
a confident wrong answer, and each time only an impossible result — a
restored source still failing, an EXE that would not change — exposed it.

#### A ground-truth constant no gate guarded

The 59 QEMU gates were the last unverified layer — mutation-testing them
is expensive, so I sampled. The first sample found something.

`handling_bird` slides a fresh alien down 1 px/frame `while y < 8`. That
8 is ZEsarUX ground truth, quoted in `test-enemy-descend`'s own
docstring. Changing it to 9 left **all 59 gates green**, while making the
alien enter one pixel lower than the original.

`test-enemy-descend` passing was CORRECT, not a defect: its docstring
says it asserts the slope and the held fields, deliberately not the
threshold. The gap was that nothing else did either — a constant can be
documented as ground truth, quoted in a gate's own preamble, and still
be unguarded.

The fix is one more checkpoint, chosen by measurement rather than
reasoning. Correct code gives frame 7 → y=8 (the slide's last step),
frame 8 → y=8 (stopped; the descriptor motion has not yet produced a
whole pixel) and frame 9 → y=9. Frame 8 holding at 8 is the
discriminator: with the threshold at 9 the slide runs one frame longer
and y is 9 there. Mutation now caught.

Worth noting the cost: proving the constant was unguarded meant running
the full suite against a mutated build, six minutes for one bit of
information. That is why sampling the QEMU layer is the right approach
and exhaustive mutation of it is not.

#### The stale-output class, encoded

Last commit fixed one gate that could pass on a previous run's captures.
`check_gate_freshness.py` now covers the class, in both shapes: a gate
that reads or tests for files under its own `build/` directory must
`rmtree` it first, and a gate that mcopies `PROBE.TXT` must guarantee a
fresh one — by rebuilding the floppy image, which carries no probe, or
by `mdel`ing it.

The probe half finds nothing today, and writing it down is the point. 20
gates read `PROBE.TXT` with no `mdel`, which LOOKS wrong until you
notice they unlink and rebuild the image first. I did that analysis to
be sure none were at risk; without the gate the next person repeats it,
or "fixes" 20 safe gates.

The first version was too loose to catch its own case. It required an
`unlink(missing_ok=True)` and a `make FLOPPY` anywhere in the file, so
deleting a gate's floppy rebuild still passed — the LOCAL probe's unlink
satisfied the first half. Both halves now have to be about the floppy.
Two mutations, both caught: reverting the rmtree fix, and removing a
rebuild.

#### A gate that could pass on last run's captures

Mutation-testing `test-visual-checkpoints` — the one fast gate never
verified — found that it does NOT clear its output directory before
running. It asserts each checkpoint PPM `exists()`, so PPMs left by any
previous run satisfied it. On a clean checkout it works; on a developer
machine that has run it once, it could report green while the port
produced nothing. That is the wrong way round: it tells the truth only
where nobody runs it interactively.

Fixed with `shutil.rmtree(OUT)` at the top, which the newer visual gates
already do. `test-brick-flash` reads captures back out of the same kind
of directory and got the same fix.

Then the mutations themselves taught something. Two of my first three
SURVIVED and I nearly wrote them up as gate defects. They were misaimed:
`probe_checkpoint_due` drives the PROBE.TXT writes, not the visual
capture, which is `visual_checkpoint_tick`. A substitution can apply
cleanly and still touch code the gate never exercises — the silent no-op
in a subtler form, and one `mutate.py` cannot detect, since the text
really was there.

Aimed correctly, the gate comes out honest: "stop after the first
checkpoint" is CAUGHT, and the one that survives — treating the
countdown delta as an absolute — is exactly the hole its own docstring
declares. A gate that states its limits and then demonstrably has
precisely those limits is the good case.

#### Making a default impossible to misdocument

Six stale claims in six readings is a pattern, not luck, so this closes
the subclass that can actually be checked.

Prose about behaviour cannot be gated in general. Debug-switch DEFAULTS
can: each field of `DebugSwitches` now carries `default=0` or
`default=1`, and `check_switch_defaults.py` compares those against the
`dbg = { ... }` initialiser positionally. Adding a field without a
default fails; so does reordering the initialiser.

This is aimed squarely at the RNG case. That comment opened "OFF by
default" for two months after the flip, and `parity-gaps.md` then
carried the closed gap as its TOP priority — a documentation error that
would have sent someone to fix working code.

The honest limit is in the gate's own docstring: it cannot read the long
prose notes elsewhere in the file, which is where the wrong claim
actually lived. What it does is make the struct the single checked place
a default is written, so any prose that disagrees now has something
adjacent and verified to disagree with.

Mutation-checked three ways: flipping a documented default, flipping the
initialiser, and removing a `default=` marker.

#### testing.md's first line, and one I broke myself

`notes/testing.md` opened with "**`make test-fast`**... " — no, it
opened with "`make test-video` is the one gate here that needs no
emulator". That was true when zxvga was the only host suite. There are
now 14 suites and 12 source gates that need no emulator, and every one I
added made the first line of the file more wrong.

The CI section was stale too, and that one is mine: it still described
CI as running `test-video` plus three named gates, which I replaced with
`make test-fast` a few commits ago. Changing a thing and not its
description is how all of these start.

Both fixed, and the numbers are now CHECKED rather than trusted —
`check_notes_numbers.py` reads testing.md's opening blockquote the same
way it reads the plan's status block. Adding numbers to a document
without a gate is the trap `make help` taught; the right answer there
was to delete them, and here it is to check them, because orientation is
what this file is for.

Getting that check right took two corrections of its own. The intro is a
blockquote, so the numbers wrap across lines behind `> ` markers and a
line-oriented regex missed them — a mutation of "14 host suites" to 13
PASSED. And "runs all 59 in about six minutes" never says *gates*, so it
was unmatchable; the doc now says "all 59 QEMU gates". All four
mutations are caught.

#### The TOP-priority parity gap was closed too

Having found one stale gap, I checked the one above it — priority 1,
"Enemy RNG not byte-exact ... the last remaining motion approximation".

`BATTY_RNG_PERFRAME` has DEFAULTED ON since 2026-06-05, implements the
original's model (tick once per frame at the loop top, consumers read
without advancing), and `make test-rng-walk` proves the port's
`random_number` walk equals the original's byte for byte. The gap has
been closed for two months.

The source comment was contradicting itself: it opened "OFF by default:
the port advances the RNG on demand" and then said "Now the DEFAULT
(2026-06-05)" ten lines later, with the initialiser agreeing with the
second. A reader who stopped at the first line — which is what an
opening line is for — got the wrong answer.

What actually remains of enemy motion is smaller and different: the bird
runs `bounce_enemy_off_margins` rather than `LAFFC` and the exact
`check_margins`. Verified by reading the call sites — only the balls
take the `LAFFC` path. That is now what the entry says, instead of a
claim about the RNG.

Two stale gaps in two readings of the same file, both on items presented
as open work. The lesson is not "check parity-gaps" — it is that a
document nobody re-derives from the code drifts fastest exactly where it
matters most, at the top of the priority list.

#### A parity gap that had been closed for two months

With the refactor stages finished, the remaining work lives in
`notes/parity-gaps.md`. Reading it for something small turned up a claim
that was wrong rather than a gap that was open.

It said the metal-brick shimmer "currently loops it forever — that is
known-bugs.md #3, pending fix". The port does not: `step_brick_hit_anim`
does exactly the original's `(c + 1) & $0F`, so the slot frees itself
after one ~15-tick pass, `test-brik-anim-pace` gates the cadence, and
`known-bugs.md` records #3 resolved on **2026-06-11** — two months
before. Someone reading the gaps file for a target would have gone to
fix something already right.

Corrected in both places it appeared: the entry itself and the priority
list at the top, which is the second time that file's summary and its
body have disagreed.

Checked the siblings rather than fixing one and leaving them: #4 and #5
are correctly reflected, and the MAGNET-plus-multiball item is genuinely
open. One stale claim, not a pattern.

#### Stage 1's blocker, taken on rather than worked around

The remaining replay seeders were blocked on main.cpp FUNCTIONS, not on
state, and I had said the next step was a real move rather than another
easy slice. The bomb is that move.

`BombState`, `bomb` and `bomb_launch` went to `weapons`, whose framing
widened honestly from "the bat's laser" to "things in flight" — the bomb
is a position, an active flag and a fall, cleared on the same events as
a bullet. What it COSTS on reaching the bat (the explosion, the lost
life, the hidden extra balls) stayed in `main.cpp`, exactly as bullet
scoring did. `replay_apply_bomb` followed into `replay`.

Two decisions worth recording.

`bomb_fall_step` takes the floor as a PARAMETER. `PLAYFIELD_H` lives in
`zxvga.h`, so reading it directly would mean including the whole video
engine to drop a bomb. The module owns the fall; the caller owns where
the floor is — the same reason `bricks` takes its cells.

The off-bottom deactivate moved ABOVE the bat check, since the fall now
owns it. That is an order change, so it needs an argument rather than a
shrug: off-bottom means `bomb.y > 192`, while `overlaps_bat_body`
requires `bomb.y < BAT_Y + 10 = 186`. Both cannot hold, so no frame
behaves differently. The reasoning is in the code, not just here.

Two host-test link lines needed `physics.cpp` afterwards, because the
fall goes through `motion_accel_step` — the linker found both.

#### Two fall curves, not three sets of magic numbers

`physics.h` documented three falling things and their constants; the
three call sites each wrote the numbers out again, in three different
functions. There are only TWO curves — bonuses and enemy bombs share
one — and that is easy to miss when both are spelled `0x0008, 0x02`
pages apart. `FALL_DE_SLOW` / `FALL_CAP_SLOW` / `FALL_DE_FAST` /
`FALL_CAP_FAST` now say it.

The replay-knob comments quoted the raw numbers too, so grepping
`0x0008` found the documentation and not the code. Those now name the
constants, and the accelerator tests use them as well — which is what
makes the change more than cosmetic: the measured distances (20 frames →
6 px, 40 → 25, 80 → 97) now PIN the constants. Mutating `FALL_DE_SLOW`
from 8 to 9 is caught, which it would not have been when the test spelled
the number out itself. That is the same self-reference trap as the band
bounds, avoided by having the numbers come from one place and the
expectation from measurement.

#### A test that read its expectation from the thing it was testing

Third and fourth sweeps: thirteen more mutations. Eleven caught, one
survivor, one badly-chosen mutation of my own that changed no behaviour
and was not a finding.

The survivor was a shape I had not hit before. `BRICK_BAND_Y_TOP` 31 →
30 was green, because `test_painting_stays_in_the_band` reads its bound
from `bricks.h` — the SAME header the painter reads. Mutating the
constant moves the code and the expectation together, so the test can
never fail on it. Self-referential coverage looks like coverage and is
not, and it is invisible to inspection: the test asserts a real property
and names a real constant.

The fix takes the expectation from an independent authority. `level.h`
owns the field geometry and static-asserts it against the original's
addresses, so the band's top is `FIELD_Y0 - 1` and its bottom is
`FIELD_Y_END`. Mutating either bound is now caught, and so is moving the
field itself.

Yield is falling — three sweeps found three real gaps, the fourth found
one. That is the signal to stop sweeping broadly and mutate deliberately
when touching something, which is what `mutate.py` is for.

#### The accelerator's fraction, and a truncated asset accepted

Second sweep: eight more mutations, two survivors, both real.

`motion_accel_step`'s `m->frac = (unsigned char)sum` is what turns a
sub-pixel velocity into an occasional whole-pixel step. Dropping it left
all three accelerator tests green — the steps are still 0..2, it still
settles at the cap, and 120 frames still covers more than 100 px. But
the object does not move AT ALL for the first 20 frames instead of
falling 6 px. Every falling thing in the game would start late.

The fix pins exact cumulative distances (20 frames → 6 px, 40 → 25,
80 → 97) rather than a loose bound. My `total > 100` was the assertion
that let it through; a threshold chosen to be safely true is a threshold
that proves little.

`asset_load_chunked`'s short-read branch had no test — only
`asset_load`'s did. Mutating its `return false` to `return true` was
green. The shipped assets all take the chunked path, since they are
larger than the scratch buffer, so a truncated `SPRITES.BIN` would have
loaded "successfully" with the tail left as whatever was in `dest`. Now
checked across every scratch size 1..64, so it is caught whether the
short read lands on the first chunk or a later one.

Both new tests also assert the positive case, so neither can pass by
failing always. `test_physics` and `test_assets` were two more suites
printing a hardcoded count.

#### A bullet that hit a brick stayed in flight

The first sweep with `mutate.py` — eight mutations, several per module
rather than one per suite, which is what the earlier pass under-sampled.
Seven were caught. One survived, and it was the serious kind.

`start_blast` turns a bullet into a blast and deactivates it. Mutating
`bullet_active[i] = 0` to `= 1` there left the whole weapons suite
green: the hit tests check WHAT was hit, the blast tests check the
blast, and neither looks at the bullet afterwards. In the game that
bullet keeps climbing and clears the rest of the column from one shot.

`bullets_clear` had the same shape of gap. The suite called it
constantly as setup but only ever asserted that it reset the animation
frame — never that it deactivated anything. It runs at level entry, on
death and on level clear, so a bullet surviving it is a phantom shot in
the next level.

`test_hit_consumes_the_bullet` covers both, and both mutations are now
caught. Also confirmed: mutating `bullets_clear` to skip only the active
flag is caught, so the test is not passing on the frame reset alone.

One sweep entry errored rather than reporting — a substitution that
matched nothing, which under the old hand-rolled method would have read
as a survived mutation and sent me looking for a gap that was not there.

#### Mutation testing made reliable, after it went wrong three ways

The technique found five real gaps in this repo's own tests. It also
produced two confident false results along the way, which is a poor
ratio for the thing being used to judge everything else.
`scripts/mutate.py` removes all three failure modes rather than leaving
them to be re-learned: it deletes every `build/test_*` FILE instead of
guessing which binary a target builds (the `test-video` →
`build/test_zxvga` trap), which also defeats the same-second timestamp
problem, and it treats a substitution that matches nothing as an ERROR
rather than as a survived mutation.

That last one matters most. A no-op substitution leaves the source
clean, the suite passes, and it reads as a FINDING — the failure mode
that manufactures work rather than losing it. It now has its own exit
code, distinct from both outcomes.

`notes/testing.md` gains a section for the tool and, deliberately, a
list of the KNOWN EQUIVALENT MUTANTS — the two that survive by design.
Without that list the next audit re-investigates them and reaches the
same conclusion at the same cost.

#### zxvga never checked WHERE a sprite lands

Finishing the host-suite audit: `bricks`, `hud`, `sound` and `assets`
caught their mutations. `zxvga` did not catch `x_px & 7` → `x_px & 6`,
nor `start_col = x_px >> 3` → `(x_px + 1) >> 3`. Both move every sprite
at a non-byte-aligned x, and the suite stayed green — it checked
clipping (`blit_stays_in_playfield`), the clash invariant, and
flush-equivalence, none of which care about placement.

`blit_lands_on_given_x` now blits a single-pixel sprite at all 32 x
offsets and requires exactly one lit bit, at exactly x. Both mutations
are caught.

One mutation is deliberately NOT closed: `byte_hi = (x_end - 1) >> 3`
→ `x_end >> 3` marks one extra dirty byte. The flush then copies a byte
that is already correct, so the output is identical and the
flush-equivalence tests pass BY DESIGN. It is an equivalent mutant, like
`repaint_row_top_edge` earlier — recorded rather than chased.

**The trap that nearly made all of this wrong.** The make target is
`test-video`; the binary is `build/test_zxvga`. Every mutation run here
deleted `build/test_video`, which does not exist, so each result came
from a stale binary — and the first pass reported the shift mutation as
"caught" when it was not. It surfaced only because a restored source
still failed, which cannot happen and forced a real diagnosis. Deleting
"the obvious binary name" is not enough; delete the one the rule
actually builds.

#### Two host suites tested the shape, not the value

With all twelve source gates now mutation-verified, the same audit ran
over the fourteen host suites. `rng`, `scoring`, `enemies`,
`bonus_codes` and `replay_parse` all caught their mutations. Two did
not.

`test_objects` checked that `object_reflect` is an INVOLUTION and is not
the identity. Both hold for the wrong constant: changing the `+ 1` to
`+ 2` left every assertion green. That constant is the original's
`change_direction` (`$ACEE`), and the file's own comment records that
getting it wrong once pinned the ball against a side wall juggling its
dy forever — a property test that survives the historical bug is not
guarding much.

The fix ties the two ports of ONE original routine together:
`object_reflect(flip_x)` must equal `laffc_change_dir(dir, $1F)` and
`flip_y` must equal `laffc_change_dir(dir, $3F)`, over all 64
directions. That pins the value without a hardcoded table and makes it
impossible for the two to drift apart. One anchored case (dir `$20` →
`$00`) catches a change to BOTH.

`test_weapons` never pinned how FAR a bullet travels. The tunnel test
walks it until it leaves the field and the hit tests only care what it
meets, so `BULLET_SPEED` 6 → 7 was green. A bullet climbing faster
reaches a brick a frame early, which the QEMU cadence gates would report
as a mystery rather than as this.

Two suites also still printed a hardcoded test count, so adding a test
left the total unchanged — the same defect fixed in four suites earlier.

The stale-binary trap bit twice during this: `make` sees a restored
source file with a timestamp inside the same second and reruns the OLD
binary, reporting a mutation as caught (or a new test as absent). Every
mutation run here does `rm -f build/test_<name>` first.

#### The gate that guards the other gates was skipping a third of them

Last commit found `check_notes_numbers` checking almost nothing, so I
mutation-tested the four source gates I had never verified — the ones
predating this session. `test-death-sparks`, `test-rocket-bonus` and
`test-l3-replay-seed` all caught their mutations.

`check_gate_greps` did not. Renaming `rest_ball_on_bat` — which
`test-stuck-ball-offset` greps for BY NAME — left it green.

The cause: it selected gates to inspect by matching the literal string
`src/main.cpp`, so any gate building its path from pathlib segments
(`ROOT / "src" / "main.cpp"`) was invisible. Four were skipped, and they
were exactly the invariant gates — `test-stuck-ball-offset`,
`test-invariant-owners`, `test-game-over`, `test-ball-sign-cache-owner`.
The `check_*.py` gates were not scanned at all.

Widening the filter to "mentions a C source filename" then produced a
FALSE positive: `check_notes_numbers` reads `main.cpp` for a line count
but greps the PLAN document, so its `## Where this stands` was reported
as a stale source needle. Judging by filename is the wrong axis — what
matters is the HAYSTACK. It now tracks which variables in a gate hold
source text (following simple transforms like
`compact = "".join(src.split())`) and only checks needles compared
against those.

One more classifier bug fell out: a needle compared against an INLINE
`"".join(src.split())` was treated as raw and warned about "matching
only after whitespace normalisation" — a complaint about the gate's own
parsing, not the source.

Coverage went from 36 needles across 22 gates to 42 across 33. All three
mutations are now caught, each naming the gate and needle.

#### The status block rewritten, and a gate that was not checking it

`Where this stands` had drifted into a summary of the last few commits
rather than a description of the state: it named the wrong "latest full
run", stated the defect count TWICE in the same section, and listed the
new gates in a sentence whose grammar had not survived four
appendings. It now says what the code, the tests and the defects are,
and — new — **what is actually left**, which nothing said anywhere.

Two things were wrong beyond the prose. `notes/parity-gaps.md` still
listed "full game-flow transitions" as priority 3 while its own "Test
coverage gaps" section, lower in the SAME FILE, recorded them closed;
that is the eighth duplicated list, and the first one found inside a
single document.

And `check_notes_numbers.py` was not checking what it appeared to. Three
separate defects, each found by a mutation that PASSED:

  - it knew only the runner's 59 gates, so a status block stating the
    true total of 71 failed against a gate that was less accurate than
    the text it was judging;
  - its regex needed the number ADJACENT to "gates", so "59 QEMU gates"
    was invisible and mutating 59 to 60 passed;
  - widened by one word, "12 emulator-free source gates" was still
    invisible, and 12 to 11 passed.

It now knows all four counts — QEMU, source, oracle, total — computing
them as set differences rather than subtracting totals (the groups
overlap, and a subtraction version reported 0 oracle gates). All four
mutations are caught. The limit is stated in the source: swapping two
VALID counts is not caught, because that needs the gate to parse the
sentence.

#### Stage 1a: the parsers came out first

The replay scaffolding is still blocked on game-state ownership, but its
*parsers* never were. `replay_parse_ints` and `replay_parse_hex_bytes`
take the value, not the variable — `main.cpp` keeps the `getenv` — so
they moved to `src/replay_parse.cpp` with four host tests.

Writing the tests found the contract was false. `replay_parse_ints`
assigned each field into the caller's array as it went, so a value that
failed on its third field had already overwritten the first two. The
"all-or-nothing" claim I made when consolidating those five hand-rolled
parsers was wrong at the time. It now parses into a scratch buffer and
copies out only on success.

That is worth more than the line count: a half-seeded replay value puts
the game in a state nobody asked for, and the gate then reports a game
bug.

#### One level-clear tail, and an invariant broken three commits after naming it

`finish_cleared_level` took the level-clear tail, and the new-game bat
reset folded into `new_game_reset` where the rest of the new-game state
already lives.

That block cleared `extra2_active` and `extra3_active` WITHOUT the
`sprite_set` halves — the exact invariant `hide_extra_balls` was named
to protect, violated three commits after naming it. Inert today, because
the port's ball object handler is a stub, but `call_for_all_obj`
dispatches on `sprite_set` and the probe dumps those objects. It uses
the helper now.

#### The alien-to-blast transition existed four times

The alien-to-blast transition existed **four** times — killed by the
bat, by a ball, by a bullet, and by the KILL_ALIENS bonus — each with
its own copy of the nine lines that centre the 16x13 blast, set
sprite_set $0A, award 350 and queue the sound. Three of them now call
`blast_active_alien`, which was extracted for the fourth back at stage
6b-iv.

#### reset_destroyed_cell_attrs into bricks, and what testing it exposed

`reset_destroyed_cell_attrs` moved into `src/bricks.cpp`, the first
piece of stage 5b to leave `main.cpp` rather than merely be named there.
It needed nothing new: the module already writes `attr_buff`, and
`level.h` already owns the field geometry.

Being in a module means it can be host-tested, and two tests now pin the
rules that were previously only assertable through a 267-second
emulator sweep: the `$C0` empty-cell sentinel must SURVIVE the reset
(treating it as destroyed would repaint the frame's edge cells), and a
destroyed cell takes the non-bright left char only when its left
neighbour is still live.

Mutation-checking those found something about the guards, not the tests:
of three mutations tried, two failed to COMPILE under `-Werror` (an
unused parameter, an unused variable) and only the third reached the
test. Worth knowing which guard is doing the work — `-Werror` is
catching more of these than the tests are.

`repair_band_row_boundaries` followed it into `bricks.cpp` — the
`repaint_row_*` primitives it drives were already there — and pointing
the existing test at it exposed something worse than duplication.

`scoped_repaint_equals_full` looked like it guarded the edge repairs. It
does not: **removing any of the three leaves it green** (mutation-
checked). It walks every row ascending, so row r+1's own paint does
whatever the repair would have done. It proves row-by-row equals full,
which holds either way. The repairs only matter when a repaint STOPS at
a boundary — which is what the incremental band rebuild does and what
known-bugs #1 and #2 violated.

`window_repaint_matches_full` is that test: paint ONE window over
15 levels x 10 windows, repair, compare the window's own rows against
the full paint. Dropping `repaint_row_body_top` fails it; so does
dropping `repaint_row_attrs`, but only after the comparison was extended
to attributes as well as pixels — comparing pixels alone left it
unguarded.

`repaint_row_top_edge` remains unguarded, and the reason turned out to
be measurable rather than speculative. Instrumenting the same 15 levels
x 10 windows: **135 calls, 0 bytes changed**. Fix-up 1
(`repaint_row_body_top`), which runs immediately before it, already
writes those rows.

So it is an equivalent mutant, not a coverage gap — no test can catch
its removal because its removal changes nothing. It is kept anyway: one
pass over 15 columns, and the redundancy holds only while
`repaint_row_body_top` keeps covering the same bytes. Deleting it is
safe today and silently unsafe if that changes. The measurement is
recorded at the call site so the next person does not re-derive it.

`paint_brick_band` followed: re-base the band's attr rows from the
level's captured attrs, reset the cells whose bricks are gone, paint
what stands. Taking `(cells, lattr, bg_attr)` instead of a level index
is what let it move — the module never needs `level_attrs` or
`bg_attr_per_cycle`. `print_border_shadow_c` stays with the caller,
being a frame concern rather than a brick one.

`main.cpp`'s `render_brick_band` is now four lines: guard, paint,
shadow.

`paint_brick_band_rows` went the same way, so both painters — full and
row-scoped — now live beside the primitives they drive. What is left in
`main.cpp` is the two thin wrappers that turn a level index into
`(cells, lattr, bg_attr)`, which is the only thing the module could not
know.

#### BATTY_REPLAY_SCORE reaches the other half of the game-over screen

`BATTY_REPLAY_SCORE` seeds a score, which reaches the other side of
`render_game_over`'s `if (high_score_beaten_this_game)` — the NEW HIGH
line and the saved initials. `test-game-over-visual` now runs both, and
asserts the line is ABSENT on the plain run, so neither branch can quietly
stop rendering.

Two things that cost a run each, again:

Seeding a score hands out the extra lives that score earns.
`award_score_milestones` runs on the first frame, so `BATTY_REPLAY_LIVES=1`
silently became lives=N and a run set up to die immediately never died.
The knob seeds `live_adds_awarded` too.

And the first version of that seeding sat inline in `new_game_reset`
ABOVE its `player.live_adds_awarded = 0;`, which wiped it. The knobs now
live in `apply_player_seed_env`, called at the END of `new_game_reset` —
so a reset added later cannot clobber them. That is the same ordering
mistake as the PlayerState rename earlier in this refactor: inserting
code above a line that resets what it just set.

#### Every screen in the game now has visual coverage

**Every screen in the game now has visual coverage.** Name entry was the
last, and reaching it took a stack: one life, no ball (death on frame 1),
a seeded score to beat the high score — and then one key, because of the
hold below.

`input_new_record_name` went 58 → 26 lines, split into
`draw_name_entry_screen` (the furniture, drawn once),
`draw_name_row` (the blinking three letters) and `step_name_letter`
(the wrap between $0A and $23, which has no clamp — stepping off either
end lands on the other).

This one waited for its evidence. Every earlier note about this function
carried a "rests on being pure code motion" caveat because the game-over
path had no visual coverage. It has some now, so the caveat is gone —
and the gate was extended in the same commit to press LEFT and require
the letter row to change, since placement alone would look identical
whether `step_name_letter` worked or not. Mutation-checked by making the
LEFT arm a no-op.

#### bricks kept its header's reasoning in the .cpp too

A scan for sentences repeated across comment blocks — the mechanical
version of what the last two commits found by hand — turned up two in
`bricks`, where the header's WHY had been copied into the `.cpp`.

They had already partly diverged, which is the whole hazard: each copy
carried a paragraph the other lacked, so neither was complete and a
reader had to find both. The convention applied is the obvious one and
is now visible in the file: the HEADER states the contract a caller
needs, the `.cpp` states only what the implementation has to know, and
where the reasoning is shared the `.cpp` points at the header. Nothing
was dropped — the unique halves moved rather than being merged away.

One cross-file repeat remains and is legitimate: `zxvga.cpp` and
`zxvga.h` share a title line, and the `.cpp` already says "Read
zxvga.h first".

#### handle_input split; one explanation of the SPACE key

`handle_input` went 70 → 26 lines: `toggle_pause` and `launch_or_fire`
came out, leaving a function that is just the key dispatch it claims to
be.

The SPACE handler was carrying two comment blocks in front of a
ONE-LINE call, both explaining the laser, and the sentence "Independent
of ball state — SPACE refires the laser while the ball flies" appeared
verbatim in each. One of them also described the cadence maths that
moved into `try_fire_laser` when that was extracted; the block stayed
behind. `launch_or_fire` now states the property once — the launch is
conditional on a WAITING ball, the laser is not — because that
independence is the whole reason the two live in one branch.

#### make help named 13 gates and omitted test-fast

`make help` was the SEVENTH stale list, and the one a newcomer meets
first. It named 13 individual gates from an earlier era and omitted the
two entry points that matter — `test-fast` and `parity-check-parallel`.
Someone reading it would not learn that everything emulator-free runs in
seconds.

It now names the entry points and points at `notes/testing.md` for the
per-gate index, which is itself checked by `test-gate-index`. Listing 90
targets would just have restarted the rot.

Writing it, I put "59 gates" into the help text — a number in a place
nothing checks, which is precisely the failure this session has spent
several turns cleaning up. Removed rather than gated: the count is
useful in the checked index, not in a one-line orientation. Adding an
eighth checked location for it would be exactly the over-engineering
these notes keep warning about.

#### known-bugs.md said there were none; there is one

`notes/known-bugs.md` opened with "user-reported, unfixed" and "(none
currently)". Neither had been true for a while: #8-#15 were surfaced by
this refactor rather than reported, and #14 is open. Someone scanning
the top would have concluded there was nothing outstanding. It has a
status table now, and the sections it names all exist — checked, because
a table pointing at absent sections is the same "looks documented"
failure as a dead citation.

That made SIX duplicated lists: this file kept its own copy of the same
findings table, and that copy was already missing #15. Replaced with a
pointer rather than synced, per the rule the fourth one taught — derive
or point, do not duplicate.

#### Three rotted file citations, and a gate for the other 240

Three file citations had rotted (named below without paths, per the
convention this turned into). A `magnets-missing` note was folded into
`notes/magnets.md` by a notes audit, so the 271-px measurement
justifying why BOTH magnet sprites are painted led nowhere; a `laser`
note was cited for "the long note" and was never written at all; a
`repro_enemy_flyover_trail` script was deleted once the bug it
reproduced was fixed, and was still cited as a thing to run.

None of these break a build, which is why they lasted. A provenance
comment whose evidence cannot be found is worse than one that states the
fact inline, because it LOOKS documented. So in each case the thing the
citation pointed at moved into the comment rather than the citation
being deleted — the laser cadence now spells out reset $18, decrement 2,
`== 0` tested before the decrement, so a shot lands on the 12th frame.

`check_doc_links.py` guards the remaining 239 citations. Writing it hit
the self-reference trap for the third time: explaining that a file is
gone means naming it, and naming it in path form trips the check. The
convention now is to name a dead file without its directory or extension
— a `laser` note — so it reads as history rather than a live pointer.
That trap has now appeared four times — the frozen-clock gate, the gate
index, this gate's own docstring, and this very paragraph, which named
all three dead paths and failed the check I had just written. Worth
expecting rather than rediscovering.

#### A comment block that had been wrong for a long time

A comment block in `main.cpp` listed the gun, triple-ball, rocket and
kill-aliens bonuses as "deferred" and claimed the port supported four
effects. All TEN are implemented, each with an arm in `bonus_apply` and
gate coverage. The block also pointed at `map_orig_to_our_bonus`, a
function renamed to `bonus_from_original` long ago, and kept a second
copy of a mapping that `bonus_codes.{cpp,h}` owns.

It was true once. Nothing re-read it, so it became confidently wrong —
the failure this file already has a section about, found this time in
the source rather than in the notes. The replacement points at the
authority instead of duplicating it, and says what was wrong so the next
reader knows the block has been checked rather than merely reformatted.

Comment-only, and provably so: build, `git stash`, rebuild, `cmp` —
byte-identical EXEs.

#### .PHONY derived instead of hand-listed

`.PHONY` was the fourth hand-maintained list found out of sync: 85 of
109 task targets, with everything added in the last stretch missing —
`test-life-loss`, `test-level-advance`, `test-gate-index`,
`parity-check-parallel` and 21 others. A target that is not `.PHONY`
silently does nothing if a file of that name ever appears, which is the
same "green while running nothing" failure as a host suite that is never
invoked.

This one is DERIVED rather than gated. The list is now a `$(shell grep)`
over the Makefile itself, so there is nothing to drift — strictly better
than adding 25 names and checking them. Verified two ways: all 109 task
targets are `.PHONY` by `make -p`, and creating a file named
`test-life-loss` no longer shadows the target.

Four lists have now drifted (`test-fast` vs `parity-check` vs CI, the
`BATTY_*` passthrough, the gate index, `.PHONY`). The pattern is worth
naming: this repo keeps growing parallel lists of the same set, and each
looks complete on its own. Derive where possible, gate where not.

#### Indexing every gate in testing.md

`notes/testing.md` had no index. It grew as a narrative of how
particular gates came to be — worth keeping — but 30 of 59 gates were
mentioned nowhere in it, including several of the oldest, so "what
covers this behaviour?" had no answer short of reading the runner.
There is an index now, kept complete by `check_gate_index.py`.

Building it turned up that gates are defined in THREE places, not one:
`run_gates_parallel.py` (the QEMU suite), the `test-source-gates` recipe
(emulator-free), and `parity-check-full` (the ZEsarUX oracle). The first
version of the checker knew only the first and reported ten false stale
entries. The index is worth having precisely BECAUSE the definitions are
scattered.

The checker cannot judge a description, only a name, so a one-word entry
would satisfy it. That is the honest limit: what it buys is that adding
a gate without saying what it is for becomes a failing build rather than
a silent omission — the failure that already happened 30 times.

#### CI was running 1 of 14 host suites

That drift had a THIRD copy, and it was the worst of them: the CI
workflow named `test-video` and three source gates by hand, so CI ran
**1 of 14 host suites and 3 of 10 source gates** while showing a green
tick. `parity-check` kept its own copy too.

Both now delegate to `make test-fast`, which is the one list, guarded by
`check_host_tests_wired.py`. Verified rather than assumed: with
`build/level_gt` moved aside and `assets/random_seed.bin` deleted — the
two things a fresh CI checkout lacks — `make test-fast` regenerates the
asset from the tracked tape data (8192 bytes), `test_bricks` SKIPs its
golden half cleanly, and all 14 suites pass. That is a better check than
linting the YAML, which is all that was available locally.

#### A host suite that existed but never ran

`tests/test_replay_parse.cpp` had seven tests, a working make target,
and never ran under `make test-fast`. It was reachable only from
`parity-check`, the full QEMU suite — so the tests guarding the
`BATTY_REPLAY_*` value formats ran once every six minutes instead of
once every few seconds.

The cause is two hand-maintained lists of the same thing that drifted:
`test-fast`'s prerequisites and `parity-check`'s recipe. `test_replay`
went into the first and not the second; `test_replay_parse` was in the
second and not the first. Each list looked complete on its own, which is
why neither looked wrong.

A suite that exists but does not run is the same defect as a knob that
never reaches DOS: nothing errors, the green tick still appears, and the
coverage silently is not there. `check_host_tests_wired.py` now checks
every `tests/test_*.cpp` resolves to a target that is a `test-fast`
prerequisite. The suite-name-to-target mapping is not mechanical
(`test_zxvga.cpp` is `make test-video`), so the two aliases are explicit
and anything unresolvable is reported rather than assumed fine.

Mutation-checked both ways: removing a suite from `test-fast`, and
adding a suite with no target at all.

#### One bat-overlap test for falling objects

The falling bonus and the falling bomb each carried their own copy of
the bat-overlap test AND their own explanation of why it uses body
extents rather than sprite extents. `overlaps_bat_body(x, y, w, h)` now
holds both, once. Two distinctions meet in it and both fail the same
way — by reaching for the larger sprite, which makes the hit register
early:

  the BAT is 10 px, not 13. `obj_compare_2pix` at `$94BC` reads the body
  dimensions; the extra 3 px are shadow and are not a catch surface.

  the OBJECT's body too. A bomb's is 8x8 (`bomb_appear` at `$A977` sets
  `$08,$08`), not its 8x12 sprite — an earlier port used the sprite and
  triggered when the bomb's bottom reached the bat's top instead of when
  the bodies overlapped.

The `8` at the bomb's call site is therefore deliberate and says so;
`BOMB_H_PX` is 12 and would be the wrong constant. The enemy-vs-bat test
looks similar but is a general AABB over two passed-in rects, so it is
left alone rather than forced through this.

Watcom caught all five orphaned locals the extraction left behind.

#### The shared fall accelerator moved into physics

The shared fall accelerator (`motion_acc_t` + `motion_accel_step`, the
original's LA55A_0) moved to `physics` with three host tests. It is pure
arithmetic with no state, shared by falling bonuses, enemy bombs and the
+400 marker with different constants — exactly the kind of thing that
belongs in the module that owns motion, and exactly the kind of
fixed-point code that is easy to get subtly wrong. It had no test.

The interesting property is the CLAMP: it compares the accumulator's
HIGH BYTE for equality with the cap, not the value for `>=`, which is
what the Z80 did. A `de` large enough to step past the cap in one add
therefore never triggers it and the value runs away. All three
production constants are safe, and the test pins that fact rather than
the bug — so a fourth caller with a big `de` gets told why it
misbehaves. Mutation-checked: changing the clamp to `>=`, or perturbing
the accumulator by one, is caught by that test and by neither of the two
curve tests.

Both of the curve tests failed on their first run because I guessed the
frame counts. `de=$0008` needs 64 steps to reach a cap of `$02`, not the
40 I assumed, and the fast variant needs 820, not 200. Measured, then
written down next to the numbers.

#### Two 'is this brick there' rules, both correct

Writing the clear-bricks knob surfaced that this codebase has TWO
"is this brick there" rules, and both are correct:

  `BrickField::standing`  `!(cell & 0x80)` — bit 7, destroyed. Collision.
                          An undestructible brick is still there to
                          bounce off.
  `bricks_live_count`     `!(cell & 0xA0)` — destroyed OR undestructible
                          (bit 5). Level completion. An undestructible
                          brick can never be cleared, so counting it
                          would make the level uncompletable.

Reading one as the other has no visible symptom until a level either
never ends or ends early. The rule was an unexported four-liner in
`main.cpp` with no test; it moved to `bricks` as `bricks_live_count`
(taking `const u8 *cells`, the module's existing style) with both rules
stated together and a host test that asserts the same grid gives
different answers to each.

Also, a per-override audit of what still blocks stage 1: the remaining
seeders need main.cpp FUNCTIONS, not just state — `apply_replay_bomb_
override` calls `bomb_launch`, `apply_replay_force_bonus` calls
`try_spawn_bonus`, and the brick ones need `live_level`, which is game
state rather than compositor state and belongs where it is. So the next
increment is a real move, not another easy slice, and the plan should
stop implying otherwise.

#### All five game-FLOW transitions closed

`parity-gaps.md` listed five game-FLOW transitions as ungated. **All
five are now closed.** The last two — level-clear → next and the level
wrap — needed a level that clears without destroying ~50 bricks with the
ball, so `BATTY_REPLAY_CLEAR_BRICKS` marks every destructible cell
destroyed at entry. It leaves cells that already carry bit 5 or bit 7
alone, because `live_bricks_remaining` does not count those as live and
an indestructible cell must not become rubble.

With the grid empty the game runs through levels as fast as it can draw
them — about one per 1.3 s — which is what makes the wrap reachable at
all. `test-level-advance` asserts `current_level == round_number %
N_LEVELS` past the boundary; that identity holds at every round, so an
overshoot proves as much as landing exactly on 15. Observed: round 29 →
level 14. Mutation-checked by changing the modulus to 16.

The wrap half is load-sensitive and the gate says so in its own failure
message rather than leaving someone to guess: reaching round 15 depends
on host speed, and falling short is not a port regression unless the
advance assertion failed too.

The other three were closed earlier: game-over and name entry visually, and
life-loss by `test-life-loss` — an A/B where BOTH runs seed 3 lives and
hide the ball (so `handle_no_ball_death` is reached on frame 1) and the
only difference is `BATTY_SUPPRESS_NO_BALL_DEATH`. Same level, same
seed, same frame, so a change in the indicator strip IS the life loss.
Level-clear → next and level wrap remain open, and the note now says
exactly that instead of listing all five.

Measuring the indicator took two wrong attempts, both worth recording
because they generalise. "Lit pixels" fails — the playfield background
is not colour 0. "Any non-background pixel" fails too: the background
carries a vertical line every 16 px, so every cell contains one and the
first version counted four icons where there are two. Density works, and
the gap is an order of magnitude: icons measure 74 and 46 non-background
pixels per 16×6 cell, empty cells exactly 6.

#### The one module the QEMU suite cannot cover

`sound` is the one module the QEMU suite structurally CANNOT cover:
every gate runs with sound off, so its 366 lines are guarded by host
tests alone. Three contracts had none.

`sound_silence()` and `sound_stop_all()` are one word apart and mean
different things — silence stops what is SOUNDING, stop_all also empties
the QUEUE. `play_game_over` depends on that difference, mirroring the
original's `pause_clear_screen_attrib`, and `test-game-over` only checked
that the CALL is present. Pick the wrong one and a sound queued before a
screen change plays over it: audible, brief, and not something anyone
would file. Also covered: `stop_all` really does silence a held note,
and `sound_play_metal_brik` — an inline entry point, not a queue id —
produces a tone and honours muting.

One assertion in that first draft was a guess and failed: the queue
drains 17 frames after a silence, not the 4 I assumed. Measured, then
pinned with a loose bound, because the property is that the queue still
drains rather than how fast.

The suite also could not count. `test_sound` printed the LITERAL string
"7 tests, 0 failed" on success, so it never reflected reality and could
not; `test_replay` and `test_replay_parse` hardcoded their totals.
All three now count in `report()`.

#### Module state ownership, and a gate that classified nothing

`objects[]` turned out to be the ONLY module declaring state it did not
own — a full scan of every `extern` in `src/*.h` against the matching
`.cpp` found nothing else, and `check_module_ownership.py` now keeps it
that way. The scan is worth having because this class of defect is
invisible to the DOS build: `main.cpp` is always in the link, so the
header looks right and nothing fails. It only shows up when a module
gets a host test that links it WITHOUT `main.cpp`.

The gate's own history is the caution. Three versions produced wrong
answers before one worked: the first could not parse multi-word types
(`unsigned char *vga`), the second could not see uninitialised scalars
(`unsigned markup_len;`), and the third compiled its pattern without
`re.M`, so `^` matched only the file start and all 25 declarations came
back "unclassifiable" — printing PASS, and still printing PASS when the
real `objects[]` bug was restored. A gate that classifies nothing passes
everything. It now reports its unknowns out loud, and a growing unknown
list is documented as a broken matcher rather than a curiosity.

Testing that slice found two things the extraction alone would not
have.

`objects[]` was declared `extern` in `objects.h` but DEFINED in
`main.cpp` — stage 6a moved the object model and left the storage
behind, so the module described an array it did not own and nothing
linking `objects.cpp` alone could touch it. Nobody noticed for eleven
stages because `main.cpp` was always in the link. The host test could
not link, which is how it surfaced. The table moved.

And `rng_seed` does not store a walk address verbatim: it applies
`addr & 0x9FFF`, so `$A100` lands on `$8100`. That is not sloppiness —
it is the same operation that wraps the walk from `$9FFF` back to
`$8000` in one step (`rng_next` does `(addr + 1) & 0x9FFF`). My first
test expected verbatim storage and was wrong. The comment carried into
`replay.cpp` was wrong too: it called this "the low 14 bits" when the
window is 8 KB and the mask is not a bit-width truncation at all. Both
the test and the comment now say what actually happens.

#### Stage 1's first real slice

Stage 1 has a first real slice. `src/replay.{cpp,h}` holds the five
`BATTY_REPLAY_*` seeders that depend ONLY on state other modules already
own — `objects` (objects.h), the bullet and blast arrays (weapons.h) and
the RNG (rng.h). That is the entire boundary, and it is why exactly
those five could come out: the bonus, bomb, pts400, rocket, big-ball,
multiball and brick seeders write structs still living in `main.cpp`, so
moving them means moving the state first, which is the part that was
always the blocker.

The split against stage 1a is clean: `replay_parse` turns a string into
numbers and has no game state at all; `replay` applies them.

`check_env_passthrough` failed on this, correctly, and about its own
scope: it scanned `main.cpp` only, so three knobs whose literals moved to
`replay.cpp` came back as ORPHANED. It now scans every `src/*.cpp`. Worth
noting WHICH design made that a loud failure — the gate reads string
literals, so moving them moved what it sees. Had it scanned `getenv(`
call sites it would simply have gone quiet, which is the same reason it
reported nine false orphans the first time and had to be built this way.

#### What was left on the frozen clock

Auditing what was LEFT on the frozen clock found `run_level`'s `start`:
seeded from `bios_ticks()`, threaded into `handle_input` by reference so
it could be assigned twice more, and read by nothing at all. Its only
possible consumer, `TIMED_OUT`, does not appear in `run_level`. Removing
it also removed a by-reference parameter that made `handle_input` look
like it managed timing.

The menu, title and hiscore seeds are NOT the same thing and were left:
each is read by a `TIMED_OUT` that `auto_advance` keeps permanently
false. They record the cycle the original's screens would use and act on
nothing.

That distinction is what `check_frozen_clock.py` encodes. The rule is not
"never call `bios_ticks`" — that would delete working documentation. It
is **never compute with it**: a bare assignment is inert, an arithmetic
or comparison is a live dependency on a clock that does not move, which
is exactly the shape #15 was. Mutation-checked by restoring the old
65-BIOS-tick hold, which is reported by line and text.

#### known-bugs #15, and two wrong conclusions on the way

known-bugs #15 is fixed, and the route to it is worth more than the fix.
`bios_ticks()` does not advance during gameplay. I concluded that,
then talked myself out of it with a bad measurement, then proved it.

The bad measurement: `clocks=bios<N>` read from three separate runs
showed the counter moving, so the guess was declared refuted. But three
runs are three BOOTS, and QEMU seeds the BIOS tick count from the host
clock at power-on — what moved was host wall time between runs, not the
guest's counter. The entry even noted a rate could not be derived from
those numbers, and then used them as proof anyway. Being explicit about
a limitation is not the same as respecting it.

What settled it was two readings inside ONE run: latch both clocks at
the first gameplay frame, report them again at the probe checkpoint.
`dbios0_dpit678` — 678 PIT frames, about 13.6 s, with the BIOS counter
advancing by zero.

Two visible bugs followed from that frozen clock and nobody had noticed
either, because a player always presses a key: the game-over screen
waited forever, and the name-entry cursor never blinked. Both live users
now count PIT frames (178 for the 3.57 s hold, 6 for the 4.5 Hz blink
half-period). The BIOS chaining is left alone — its only other consumer
is `TIMED_OUT`, which `auto_advance` keeps permanently false.**Superseded, kept for the record**: the
game-over hold does not expire; a gate presses one key instead. The
cause is still open — see known-bugs.md #15 for what was measured and
what was refuted.

#### The game-over screen's first visual coverage

The game-over screen had no visual coverage at all, and `test-game-over`
said so in its own docstring. The blocker was never the assertions — it
was that reaching game over meant three deaths on wall-clock waits, the
shape that made `test-bat-redraw-window` flaky. `BATTY_REPLAY_LIVES=1`
plus the existing `BATTY_HIDE_BALL` makes `handle_no_ball_death` fire on
the FIRST frame, and `BATTY_HOLD_GAME_OVER` holds the screen for a key
exactly as `BATTY_HOLD_ROUND_BANNER` does. No wall clock anywhere.

Two things that cost a run each, both worth writing down:

A new `BATTY_*` knob does not reach DOS just because `src` reads it. The
test floppy bakes a HAND-MAINTAINED list of `SET` lines into
`AUTOEXEC.BAT`, and a variable missing from it is silently absent — the
gate then runs a DIFFERENT SCENARIO and can still pass, depending on what
it asserts. Mine failed only because it asserted on screen content; a
gate checking "did it not crash" would have passed while testing nothing
it named.

`check_env_passthrough.py` closes that permanently, in both directions.
MISSING is the bug above. ORPHANED is a `SET` line for a knob nothing
reads — harmless at runtime, but it is what someone copies as a template,
and it makes the list look maintained when it is not. Five knobs turned
out to be missing (`BATTY_NOSOUND`, `BATTY_SOUND_OFF`,
`BATTY_RENDER_PROFILE`, `BATTY_PROFILE_AUTO_FRAMES`,
`BATTY_FULL_BAND_REBUILD`) — including one the new game-over gate was
already setting to no effect.

Knob names come from every `"BATTY_..."` STRING LITERAL, not from
`getenv(` call sites: `parse_replay_ints("BATTY_REPLAY_BOMB", v, 2)`
reaches `getenv` one level down, and scanning call sites reported nine
false orphans on the first attempt.

"How many pixels are lit" does not distinguish the game-over screen from
the playfield: the playfield background is not black, so both are ~100%
non-zero. The real discriminator is UNIFORMITY — 98% one colour cleared
versus 54% in play. The first version of the gate used lit-pixel count
and failed against a perfectly correct capture.

Mutation-checked: moving the SCORE line two pixels up is reported as
`text bands are [(70,75),(93,100),(110,115)], expected [...(95,100)...]`.

#### known-bugs #14: signs and speeds in one cache

Fixing that ownership bug (#13) turned up another (#14). Two writers
store `-BALL_SPEED` (= -2) into `ball.dy` where the other four store a
sign, and `delta_to_dir` — whose single production caller is the
multiball spawn — selects its angle by MAGNITUDE (`abs(dx) >=
BALL_SPEED`). The mixed units do not bite today only because the
magnitude lands in `dy` and the test is on `dx`, so the `0x08` angle is
unreachable and every multiball spawn gets `0x04`. Meanwhile the only
host test of that function passes ±2 — exercising the angle production
never selects, skipping the one it always does. Pinned by
`test_delta_to_dir_sign_inputs`; whether the ORIGINAL wants a magnitude
there needs the Spectrum, so it is recorded, not guessed at.

#### known-bugs #13: extra balls writing the primary's cache

Deduplicating the ball's sign cache turned up an ownership bug that the
line count alone would never have flagged — `notes/known-bugs.md` #13.
`ball.dx`/`ball.dy` summarise the PRIMARY ball's direction, but two of
the four places that refresh them take an `Object*` that may be an
EXTRA ball: `laffc_collision` (called from `step_extra_ball`) and
`magnet_ball_frame` (slots 1 and 2). They write the primary's cache from
the extra's direction regardless of which ball they were handed.

Nothing observes it today, and the reason is pure ordering: `step_ball`
recomputes both from the primary before `ball_lands_on_bat` reads
`ball.dy`, and `apply_multi_ball_bonus` early-returns while extras are
active. Neither is a property of the writing code — this is exactly the
shape of known-bugs #8, where a mirrored convention wrote fields nothing
read. Recorded rather than fixed: changing it alters behaviour in the one
reachable scenario, and that needs a gate first.

The duplicate-block scan that found it now reports ZERO six-line
duplicates across `src/*.cpp`. At a four-line window the only non-table
hit was this one.

#### One explanation of the slot-paint order

The slot-paint order had THREE explanations of itself. Once the two
compose paths were unified behind `compose_moving_objects`, each caller
kept its own copy of the $9AD0 provenance and the f50 21px A/B delta
that motivated it — including a one-line wrapper whose entire body is
the shared call. Copies of a comment drift exactly like copies of code;
the explanation now lives on the function that enforces the order, and
the callers point at it.

Doing that surfaced something undocumented: the rocket ($9BAC) is named
in the order but composed OUTSIDE the shared function. The reason is 150
lines away — `entities_need_redraw` returns true while the rocket is
active and `can_redraw_ball_with_simple_objects` bails on it, so no
dirty-path frame can ever need to compose one. It is full-path-only by
construction, and now says so.

Comment-only changes are verifiable as such: build, `git stash`,
rebuild, `cmp` the two EXEs. Byte-identical is proof the emitted code
did not move, which is stronger than reading the diff and stronger than
any gate could be.

#### step_ball's bat contact and brick resolution

`step_ball` went 101 → 52 lines by lifting out its two self-contained
blocks: `deflect_ball_off_bat` (snap to rest height, then CATCH or
deflect — it returns 1 for a catch, which fully handles the frame) and
`resolve_primary_brick_hit` (sweep, then reverse and unwind the axis the
ball entered through, pixel position AND q8.8 fraction). What is left
reads as the sequence it always was: move, side walls, ceiling, bat,
floor, bricks, commit. The provenance moved with the code — LAB1F's
deflection table and the quantised catch offset now sit in the function
that implements them, and LAFFC's "keep the moved low byte" note sits in
the brick resolver.

#### show_round_banner: drawing and holding

`show_round_banner` split into `draw_round_banner` and
`hold_round_banner`, which separates a long provenance note from a
control-flow one. The drawing carries why the text sits where it does —
the original's coordinates are BOTTOM-anchored, so `$8F`/`$9E` are the
glyphs' lowest rows and the ink lands 5 px above; an earlier port read
them as top-Y and jammed both lines against the box. The hold carries
the 60-tick timer and `BATTY_HOLD_ROUND_BANNER`, which waits
indefinitely so a gate can capture the banner without racing it.

`test-invariant-owners` now keeps this from recurring: seven state
changes that must have exactly ONE writer, checked by count in about a
second. It is aimed squarely at the failure mode above — a helper exists,
and code written before it (or beside it) hand-rolls the same thing. The
message names the owner to call instead.

Mutation-checked both directions: re-introducing `reset_level_state`'s
hand-rolled extra-ball clear fails it with "expected 1 writer
(hide_extra_balls), found 2", and changing a mask so the owner no longer
matches fails with "found 0".

Sweeping for the rest of that pattern — every helper written this
session, grepped for hand-rolled copies — came back clean except one:
`restore_inner_border_line` re-implemented the two masks
`black_inner_border_pixels` owns, so `0x7F` on byte 1 and `0xFE` on
byte 30 each appeared twice. Split per side, since a window repaint can
reach one byte and not the other, and both callers now use them.

Everything else the sweep found was inside the helper itself. The three
`BALL_X = BAT_X + ...` sites are the known ones: the catch, the rest
rule, and `respawn_primary_ball`'s documented exception.

`reset_level_state` was re-implementing two helpers that already
existed: it cleared the extra balls' flags and sprite bits by hand
(`hide_extra_balls`) and the bullet slots by index (`bullets_clear`).
The first is the extra-ball liveness invariant violated a SECOND time,
after `finish_cleared_level` — which suggests the pattern to watch is
not "did I write this twice" but "does a helper for this already exist".

Using `bullets_clear` also means level entry now resets each bullet's
animation frame, which the hand-rolled version did not — harmless, and
consistent with what a fresh level should be.

`enemy_spawn_allowed` split `enemy_prepare` into the question and the
act. Six reasons a natural spawn might not happen, and the first is a
TEST hook rather than a game rule: `BATTYALL` pins it off, because on
levels already under the $2C brick gate an alien would spawn inside the
first gameplay frame and race the wall-clock screendump — the L3/L9
"186 px drift". Mixing that with the five real rules made the function
read as if the test mode were part of the game's logic.

### The two band painters do not have the same attr contract

Having moved both into `bricks.cpp`, the obvious next move was to point
`window_repaint_matches_full` at THEM rather than at the primitives
underneath — comparing `paint_brick_band_rows` against
`paint_brick_band` is closer to what the game does.

It fails, on all 150 level/window pairs, and the pixels are fine: the
difference is entirely in the attrs. Two reasons, both deliberate.

- `paint_brick_band_rows` dimmed char column 1 itself; `paint_brick_band`
  leaves the border shadow to the caller's `print_border_shadow_c`, a
  frame concern the module does not own. **Fixed since:** the scoped
  painter no longer dims it either, and `main.cpp`'s wrapper calls
  `dim_border_shadow_column(cr0, cr1)` — the same split
  `restore_inner_border_line` got. Both painters now have one contract.
- The row-scoped painter re-bases only `[cr0, cr1]`, and its two
  BOUNDARY attr rows are inherited rather than produced: `cr0` is row
  `r0-1`'s shadow row and `cr1` is row `r1+1`'s cell row. In the game
  both come from the static cache.

Measured exactly, after the shadow fix: comparing the two band painters
leaves 106 of 150 pairs differing; excluding `cr1` alone still leaves
106; excluding BOTH boundary rows passes. So the residue is entirely
those two rows, which is what `repair_band_row_boundaries`' own comment
predicts.

A band-level test is therefore viable for pixels and for the INTERIOR
attr rows — but excluding `cr1` would drop the mutation-verified
coverage of `repaint_row_attrs`, which writes exactly there. The test
stays at the primitive level for now; the trade is coverage against
realism, and coverage wins while the cache fixture does not exist.

One asymmetry was real and is fixed; the other is inherent.
The test stays where it was — at the primitive level, where its attr
comparison is mutation-verified to catch `repaint_row_attrs` going
missing. Strengthening it properly would mean giving the test the cache
the rebuild restores from, which is a bigger fixture than the property
justifies right now.

`release_brick_hit_anim_if_gone` came from the same pass: the stepper
and the renderer both freed a slot whose brick had been destroyed. Note
what that means — the RENDERER changes animation state. It is safe
because freeing is idempotent, but it is the same shape as known-bugs
#10, so the helper says so rather than leaving the next reader to work
out whether it matters.

Re-running the sweep across every module, at a 4-line window, found the
two kill functions still shared their HEADS — the targetable guard and
the alien's extents — after only their tails had been unified. The bat's
kill zone is just a rect, so `kill_enemy_by_bat` collapsed into a call
to what is now `kill_enemy_in_rect`, used by the bat, all three balls
and the KILL_ALIENS path alike.

Worth re-running a detector after acting on it: fixing the tails made
the heads the largest remaining duplicate, and they had been invisible
underneath.

`compose_moving_objects` was the third and best find. Both redraw paths
held their own copy of the $9AD0 slot order, and that is precisely what
drifted before: the dirty path drew the enemy BEFORE the bomb and the
full path after, so a fresh bomb still overlapping its parent UFO
rendered differently on each — the f50 21 px A/B delta in
notes/bird-render-parity.md. One copy makes that drift structurally
impossible rather than merely commented against.

It was only unifiable because known-bugs #10 is fixed. The dirty path
guarded its bullet draw with `if (any_bullet_active())`, and until the
animation frame moved out of the renderer, dropping that guard would
have advanced the shared phase on bulletless frames. Fixing the bug
removed the reason the two copies had to differ.

`place_rocket_on_bat` came from the same sweep. `attach_rocket_to_bat`
and `apply_replay_rocket_override` share the placement exactly, and
differ in what else they clear — the real catch hides every object and
bumps the bat's bonus byte, the replay seed only hides the primary ball.
Only the geometry is shared; the divergence stays visible, since a
replay hook that seeds MORE state than it used to would change what the
gates capture.

Found by a duplicate-block detector rather than by reading: normalise
the source, hash every 5-line window, report the collisions. It also
surfaced the rocket placement appearing twice and the compose order
shared by both redraw paths. Worth running again after a batch of
extractions — the eye stops seeing these.

`bat_body_sprite` and `fill_bat_resize_sides` split `render_bat` into
what it draws and the gap it has to fill. The second is the one worth
naming: mid-resize the bat is wider than any sprite, so solid bits are
stuffed into the gaps on each side for `buff_to_vga` to light in the
background's ink. There is no sprite for the in-between widths — only
the normal and big bodies — and that is why the code exists at all.

`advance_run_dot` split the bat dot's state advance out of its render.
The split is the point: `render_running_dot` mutates animation state
from inside a draw, which is exactly the shape that made known-bugs #10
a bug for bullets. Here it is safe ONLY because `redraw_frame`'s
dispatch is mutually exclusive, so one path draws the bat per frame.
Call both and the dot moves at double speed. That reasoning was nowhere;
it is now on the function that depends on it.

Not restructured further, deliberately. Test mode pins `run_dot_frame`
to `$0E` so the visual gates never see the phase advance — moving the
advance to a per-frame tick, as the bullet fix did, would be a change no
gate could check.

`repair_band_row_boundaries` grouped the four edge fix-ups the
row-scoped repaint needs. A full ascending paint gets them for free —
each row's print overwrites the previous row's edge bytes as it goes —
so painting only `[r0, r1]` leaves the boundary rows holding whatever
the bg repaint left. Getting that wrong is what known-bugs #1 and #2
were.

Their comments are numbered 1, 4, 3, 2: the order they were FOUND, not
the order they run. That is now said out loud, since the sequence is
load-bearing (each later repair overwrites part of an earlier one's
output) and the numbering suggests otherwise.

`render_brick_effects_and_mark` collected the brick band's transients —
the destruction marker and the multi-hit animations. Neither draws
anything of its own (the marker exists purely for dirty-rect
scheduling), but both must be marked because the background beneath them
was repainted. Dropping the redundant `if (any_brick_hit_anim())` around
a loop that already skips empty slots left one less thing to read.

`step_primary_ball` took the last inline block from the frame tick: ride
the bat while stuck, otherwise ramp and step, and end the run if a
replay checkpoint fired. Its `return ST_QUIT` from four levels deep
became a `false` the caller acts on — the same shape as
`visual_checkpoint_tick`.

`run_level`'s per-frame body is now a list of named calls: input, the
frame tick, RNG, steering, the primary ball, the other entities, the
no-ball death, the enemy pass, scoring, the redraw choice, checkpoints.

`ball_lands_on_bat` pulled the bat-hit test out of `step_ball` — five
conditions whose rationale ran to eleven lines above them, so the
condition and its justification were competing for the same space. It is
the most-explained line in the port: the Y test uses the ball's HEIGHT,
not its width, with a strict `>`, because matching the FRAME the
original fires on is what makes the ball's x — and so the deflection
zone derived from it — match the Spectrum.

`apply_slow_bonus` and `apply_multi_ball_bonus` finished `bonus_apply`:
every arm of the switch is now a named call or a two-liner, so the
function reads as the list of effects it is. Both are ball-side bonuses
that clear the bat's own bonus byte, and the multi-ball one does so even
when the extras are already out — the original writes `$FF` before
testing anything, so the early return sits after it.

`bounce_enemy_off_margins` did for the alien what
`bounce_ball_off_side_walls` did for the ball: three near-identical
branches, each clamping, reflecting and re-aiming, became one. The
ordering it preserves is the interesting part —
`enemy_target_away_from_margins` reads the object's CURRENT position, so
the clamped coordinates must be written before it is called or the new
target is picked from the edge it just hit.

`try_spawn_bonus` split in two: whether to drop (two guards and a 5/16
roll that does NOT advance the RNG) and `pick_bonus_type`, the 16-try
re-roll loop that does. Keeping those apart matters because the two
halves use the RNG differently, and that difference was previously
explained in a comment spanning both.

`build_static_brick_band_cache` split into `rebuild_band_cache_full` and
`rebuild_band_cache_rows`, leaving the outer function as the choice
between them plus its profile timing.

Its incremental branch also carried an INLINE copy of the border-line
restoration, with the band bounds written out as literals — `(y >= 50 &&
y < 78) || (y >= 106 && y < 134)`, silently omitting the third band
because the brick window never reaches it. That is the same rule
`restore_inner_border_line` was extracted for two commits earlier, so it
uses the helper now and the band definition lives in exactly one place.

`bounce_ball_off_side_walls` and `bounce_ball_off_ceiling` then took
`step_ball`'s wall handling — two near-identical branches that differed
only in which limit they snapped to. Naming them made room to state the
part that is not obvious: the sub-pixel fraction is dropped along with
the snap, because carrying a fraction past a clamp would drift the ball
back off the wall. And the right-hand limit moves with the ball's size,
244 normally against 240 under BIG_BALL.

`replay_parse_frame_list` followed — `BATTY_VISUAL_PROBE_FRAMES`. Its
rule is the one `visual_checkpoint_tick` depends on: values not strictly
greater than the previous are DROPPED, because the port walks the list
by subtracting consecutive entries and a repeat would give a zero delta.
That was a comment inside `probe_init_from_env`; it is now three tests.

Mutation-testing it produced a lesson of its own. The first
`respect_max` test sized its array to exactly `max`, so the mutation
that ignores `max` overran the buffer, corrupted the failure counter and
reported SUCCESS. A test that provokes undefined behaviour cannot report
anything. It now uses a roomier array with sentinels past the limit, and
an off-by-one mutation fails it with `wrote past max: slot 3 = 40`.

### Why replay is last, not first

Its ~480 lines look like the easy win — they are test-only code and
already contiguous (lines ~4410-4894), so nothing is tangled. But
extracting it to a real TU means exposing the game state it pokes
(`objects[]`, bonus/bomb/rocket state) and the ~50 variables
`write_replay_probe` dumps. That needs the game-state API stages 6-9
build, so it is the *hardest* stage, not the easiest.

### A duplication left deliberately alone

The bullet's brick-damage rules duplicate `brick_hit_resolve` almost
exactly — undestructible → anim, multi-hit → set bit 4, else destroy +
score + flash + bonus. The one difference is deliberate: bullets skip the
click, because the original tests `sprite_set == $05` and lets the impact
blast be the feedback. Unifying them looks like an obvious simplification
and would quietly delete that difference. Worth doing only with the
divergence written into whatever replaces both.

### The (void) casts were hiding something bigger

Three dead locals silenced with `(void)` turned up while splitting
`step_ball` and `step_bonus`. Sweeping the rest found a fourth —
`(void)in_dx; (void)in_dy;` in `step_extra_ball` — and that one was not
a local but two parameters, fed from `ball.extra2_dx/dy` and
`extra3_dx/dy`.

Those four fields were **written and never read**. That is what
known-bugs #8 turned on: the mirrored `dir_to_delta` convention was
computed into them and discarded, while `step_extra_ball` moved the
extras with `dir_to_dxdy` exactly like the primary. A documented bug
with no gameplay effect, found by following a `(void)` cast.

Sweeping every state struct and file-scope static for the same pattern
afterwards found **nothing further** — a useful negative result, and it
bounds #8 to the one instance. The sweep did surface `auto_advance`,
which is read by `TIMED_OUT` and never assigned: the attract auto-cycle
is intentionally absent (commit 45cad07, "no attract auto-cycle, per the
original"), so all three timeout branches are permanently false. That is
now said at the declaration, because nothing else in the file admits it.

Marking it `const` to prove the point made Watcom emit `W368 always
false` and `W013 unreachable code` — the compiler confirming the
analysis, and, under `-we`, refusing to build. So it stays a plain
`static int` with the explanation attached.

`enter_level` then took `run_level`'s per-level setup — reset, replay
overrides, magnets, the seed probe write, the screen, the intro — with
its `ST_QUIT` exit becoming a `bool`. Its one ordering constraint is
stated once: magnets initialise after the RNG seed override so their
ON/OFF coins consume the seeded walk exactly as `print_magnets` does,
and before `render_level_screen`, which paints from that state.

That also removed `i`, a function-scope alias for `lvl_idx` whose own
comment said it existed only because "the cycle / bg_attr code below
reads `i`". Six call sites now name the level they are drawing.

`play_game_over` followed: the GAME OVER hold and the name-entry screen.

Coverage note — **no gate covers it**. `scripts/exercise_gameloop.py`
drives the loop until all three lives are gone and screendumps the
screen, but it is a manual tool, not a gate; `make test`'s four-state
cycle stops at LEVEL. So the game-over path, the high-score save and
name entry rest on being code motion. Worth a gate at some point: it is
the one screen a player always reaches and nothing checks.

`test-game-over` is now that gate — a source gate, not a visual one.
`exercise_gameloop.py` drives the real screen but loses three lives on
three 9-second wall-clock waits, which is exactly what makes
`test-bat-redraw-window` flaky; a gate built that way would flake too.
So it checks the four LBC10_6 ordering facts a screendump could not
distinguish anyway: capture the high score before drawing, save after
name entry, hold ~65 BIOS ticks with any key cutting it short, and queue
no sound. A visual gate is still worth adding on top.

`step_death_spark` then reduced `play_bat_explosion`'s inner loop to
`if (step_death_spark(i)) alive = 1;`, leaving the function as the
PIT-paced pump it is. The helper keeps `death_sparks[i]` indexing rather
than taking a pointer — `test-death-sparks` greps four compacted
fragments containing `death_sparks[i].dir`, and a pointer rewrite would
have broken all four for no gain.

`write_replay_probe` then split into `probe_write_entities` (the
playfield's own state) and `probe_write_harness_state` (what the replay
knobs seeded and the checkpoints counted), leaving the header and the
bulk level dumps inline. PROBE.TXT's newline-as-prefix convention makes
any such split a byte-stream risk, so it was checked the same way as
last time: captured before, captured after, diffed — identical.

`tick_frame_rng` and `steer_bat_from_keys` then took the top of the
frame tick. The first is the one with a real constraint: the magnet
toggle samples the CURRENT RNG value, which is last frame's, because the
per-frame tick has not run yet. Swapping the two lines changes which
frames toggle a magnet, and nothing but a comment said so. Now the
comment sits on a function whose two statements are the whole of it.

`visual_checkpoint_tick` took the BATTY_VISUAL_PROBE_FRAMES countdown,
turning a `return ST_QUIT` buried four levels deep into a `false`.

Verifying it needed a step off the gate suite: every gate passes a
SINGLE checkpoint, so none exercises the delta arithmetic between
checkpoints or the not-the-last-one branch. Driving
`capture_frame_timeline.py --frames 20,40,60` by hand does, and it
reported three deterministic checkpoints. `test-visual-checkpoints` now gates its
resume half — and says in its own docstring that it does not gate the
delta, because the capture tool names files after the requested frame
rather than the one that fired.

`set_bat_bonus` then named an invariant rather than a step: the bat's
active bonus lives in BOTH bat objects, and seven separate places wrote
the pair by hand. Writing one without the other is a plausible bug that
nothing would catch — `enemy_prepare` even reads the two separately, a
defensive check that implies someone once worried about it.

The ROCKET catch's `INC (IY+$14)` is left as two increments rather than
routed through the setter: converting it would assume the two are always
equal, which is exactly the assumption the defensive read declines to
make.

`hide_extra_balls` is the same shape: an extra ball's liveness is
recorded twice — the flag the step loop reads, and bit 7 of the object's
`sprite_set` that the compositor reads. Clear one without the other and
the ball is drawn but never stepped, or stepped but never drawn. Three
sites cleared the four fields by hand, in two different orders.

`step_extra_ball`'s own per-slot deactivation was checked first and does
clear both — no bug, just the pattern.

Both of these are worth more than the line count says: a step extracted
is easier to read, an invariant extracted is harder to violate.

`sweep_bricks_for_primary` then named the collision *policy* rather than
another step: LAFFC where it fires, `brick_collision` as the backstop
for cases it declines, and the four return codes documented at the one
place that produces them.

Coverage note: **no gate sets `BATTY_LEGACY_COLLISION`**, so the
fallback-only path is unexercised by the suite. It was booted by hand
here and runs, but with a stuck ball no collision occurs — the switch is
effectively untested. Worth knowing before trusting it as an A/B
baseline.

`redraw_frame` took the path selection out of the frame loop — and
reading it closely turned up known-bugs #12: its bat-only branch
repositions a stuck ball with the constant `BALL_X_OFFSET_ON_BAT` where
every other path uses the recorded catch offset. Fixed in the next commit, gate first: `test-stuck-ball-offset` was
written against the unfixed code, failed, and passed after the one-line
change. A pixel gate cannot reach that branch, so the guard is the
invariant — one function decides where a stuck ball sits.

## What this has already found

`known-bugs.md` #8 — multiball extra balls appeared to use a direction
convention mirrored from the primary ball's in two of four quadrants. It
sat in `main.cpp` for the whole project and surfaced within minutes of
those functions becoming pure and testable.

It has since been shown to have no gameplay effect: the mirrored values
went into four `BallState` fields nothing read. Finding that took the
same tools — a pure function, a characterisation test naming the
disagreement, and then a sweep for `(void)`-silenced parameters.

That is the argument for the whole exercise. The gates prove the port
still matches the original; they do not make the code answerable to
questions. Pure functions with fast tests do.
