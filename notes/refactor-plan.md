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
| 3a | `physics` — direction + bat deflection | 231 | **done** — 12 tests vs captured hardware tables |
| 3b | collision geometry/effects split | 166 | **done** — 7 more tests |
| 4 | `assets` | 167 | **done** — 6 tests |
| 5 | `bricks` — the compositor | 278 | **done** — 5 tests, byte-exact vs 15 captured screens |
| 5b | level paint / band orchestration | ~310 | **started** — destroyed-cell reset + scene compose unified |
| 6a | `objects` — the 22-byte descriptor + slots | 60 | **done** — 5 tests |
| 6b-i | `weapons` — bullets + blasts | 95 | **done** — 6 tests |
| 6b-ii | `enemies` — steering | 145 | **done** — 5 tests |
| 6b-iii | `bonus_codes` — original <-> port numbering | 40 | **done** — 4 tests |
| 6b-iv | bonus effects, rocket, sparks | ~500 | **started** — `blast_active_alien` split out; rest needs the game-state step below |
| 7 | `hud` — glyphs, markup, score | 175 | **done** — 6 tests |
| 8 | `sound` — queue + envelopes | 366 | **done** — 7 tests; had NO coverage before |
| 9 | `run_level` decomposition | 684 -> 370 | **in progress** — prologue, input, bat steering, scoring extracted |
| 10 | state owners — structs at file scope | 113 vars | **done** — 11 clusters, see below |
| 1 | replay / probe scaffolding | 480 | **last** — see below |

`main.cpp`: 7,746 → 6,797. 100 host tests + source gates, all via `make test-fast` in seconds.

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
