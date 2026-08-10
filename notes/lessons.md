# Lessons learned

Classes of mistake this project has actually made. Each is a rule plus
just enough incident to make it stick.

## Reverse-engineering the original

**A routine boundary is a LABEL, not an end — read past it.** `LAFFC_30`
falls into `LB1C3`, which undoes the position snap it just recorded;
`current_level_2up_copier` falls into `players_swap`, which is the whole
of 2-player turn alternation. `scripts/disasm.py` prints a FALLS THROUGH
warning when a routine's last instruction is not an unconditional
`RET`/`JP`/`JR`.

**A quote that stops early is a claim you did not check.** Four wrong
conclusions came from listings that ended one instruction too soon —
including `notes/double-play.md` "proving" the bats are not confined by
stopping just before the two `CALL`s that confine them. A claim of the
form "X never happens" is about the whole binary, so grep the whole
binary before writing it down.

**Z80 bit twiddling is `RES`/`SET`/`BIT`, not `LD`.** Grepping for
`LD (IX+$12)` missed the ball owner bit being flipped on every bat
deflection. The tell was that the conclusion sounded wrong — strangeness
is a reason to widen the search, not to write the finding up better.

**Sprites are drawn bottom-up.** `print_sprite_pix` moves to the
PREVIOUS buffer line each row, so the first data row lands at y and the
rest stack above. The round banner, the Kinnock egg and the frame's top
strip all anchor at the bottom. Laying the border sprites out top-down
gave 58 bytes of 256; reading the routine gave 256 of 256.

**A watchpoint gives you a PC, not a purpose.** `0xAD8F` was named "the
brick-field blitter" and cost days of theorising; it is
`all_metal_briks_frame`, an animation that happens to share an inner
blit with the level paint. Trace where a routine is CALLED FROM before
naming it.

**Read the sprite-ID table before writing per-slot draw logic.** The
names in `gfx_screen_elements` are ordered by entry, not by sprite-data
layout: sprite `$06` is `spr_magnet_circle_ON`, drawn first and
unconditionally. Getting the order backwards cost 1383 → 660 px of
residual to undo.

**Two ports of one routine disagree until proven otherwise.** Trust the
one derived against the disasm and re-point the callers.
`enemy_dir_delta_q8` indexed a 16-entry table where the original needs
the 17th, so every ball and enemy X step got the next direction's
magnitude — sub-pixel, invisible for ~40 frames, invisible to short
tests.

**Countdown counters that gate on the CARRY of a `SUB` trip one tick
later than a `== 0` gate in C.** The laser's reset had to go `0x16` →
`0x18` to keep the 12-frame period. Match the period between fires, not
the literal.

**Reproduce input semantics, not just timing.** The original's blocking
sequences read no keys; the port added "any key skips" and a held bat key
silently skipped the pre-round animation, eating the player's input.

**Moving objects blit PIXELS only — they never recolour cell attrs.**
`print_obj_to_buff` writes no attrs, so a moving sprite shows ZX clash in
whatever the cell already holds. Force-recolouring the enemy's cells was
the source of a long stale-attr saga. Verify against the `.scr`'s last
768 bytes, not just a pixel diff.

**Wall reflect masks: side = `$1F` (negate dx), top = `$3F` (negate
dy).** Same `change_direction` the brick path uses. Having them swapped
pinned the ball at x=8 for a long time, because no gate ever reached a
bare wall.

**Check the plan against the disassembly before building from it.**
PLAN.md said Double Play confines each bat to its half and listed the
clamps as work to do; the halves actually control SCORING. A roadmap
written from watching the game is a hypothesis — and watching is also
how the enemy's invented reflect-and-re-aim got in.

**Assemble the disasm and byte-diff it against the reference SNA.** If
only your patch bytes differ, the disasm is trustworthy and the mismatch
is indirection you have not found (for us, `table_shifts` pre-shifting
the blit operands).

## Writing gates

**A gate that cannot fail is a decoration.** Check a new test against a
MUTATION of the line it aims at, not against its own green. A host test
for `laffc_sweep`'s left clamp passed, and so did the mutant that
deleted the clamp: the clamped value is only read inside
`if (!field.standing(...))`, and the test filled the grid.

**Assert the implication, or pin the antecedent — never one arm.**
`test-enemy-descend` asserted one of two legal outcomes of a
counter-phase-gated branch and failed two runs in three. In order: does
the capture already distinguish the cases (assert the implication)? If
not, pin the phase (`BATTY_REPLAY_COUNTER`). Dropping the assertion is
last and costs coverage.

**Widening a gate to cover a flake makes the flake permanent.** When a
measurement comes out two ways, the question is which behaviour is
right. The tell is whether you can name the code that produced the
variance; if you can, it is not noise.

**Where sites differ deliberately, assert the difference, not the
count.** A checker requiring "two of three scoring sites pass
`ball_owner_side`" was satisfied by the wrong two, and failed the commit
that fixed the third.

**When a feature's output is pixels, one gate has to read pixels.** A
probe-row gate proved bat 2 steers while its sprite sat frozen at level
entry. And ask what the BUG would look like: the failure was "moved 4 px
and froze", which a gate asserting merely that the screen changed would
have passed.

**Pick a background the defect cannot hide in.** Three times a
measurement failed this way: counting "lit pixels" on a non-zero
playfield, a whole-frame diff hiding a shape, and a brick test whose
`memset(0)` baseline was the same value as the zeros it should have
caught.

**Containment cannot catch under-draw.** "Writes nothing outside the
playfield" is passed trivially by a blit that skips the leftmost column.
Assert what SHOULD be written too.

**Find the observation point, not just the assertion.**
`mark_dirty_bytes`' clamps are invisible through the screen, because the
flush loop never reads the rows they protect. The screen is perfect and
the array is corrupt.

**Aim a boundary test AT the bound.** `>=` vs `>` differ at exactly one
value. "Deliberately out of range" arguments overshoot the clamp and
prove nothing — twice in one session.

**An INFO row is a measurement, not a gate.** Three checkpoints sat at
`assert_match=False` for residuals long since fixed, printing
"pixel-identical" and failing nothing. `test-visual` now lints its own
table: a bare `False` fails, `False,  # INFO: <why>` passes.

**A diagnostic row's answer can change without anyone noticing** — that
is the point of a row that never fails. Re-run it before doing the work
a plan says it implies.

**Two checks of one condition means one is untested.** A duplicated
`lives <= 0` guard let the real one's mutant survive. Delete the
duplicate; more test cases do not help.

**A test per CONSUMER, not per output.** Dropping data from the shared
attr band was verified across all 15 levels — all of them level-ENTRY
captures, which never exercise the partial rebuild. That measured one
consumer fifteen times, and CI stayed red for ten commits.

**A transition has two halves; count both.** `test-life-loss` proves a
life was TAKEN; nothing pinned what the player gets BACK until
`test-life-respawn`.

**"Not gated" is a note to write and then a note to delete.** The
precondition that made gating expensive is usually removed by some later
commit, and nothing announces it.

**Wire a new gate into the path people actually run.** `test-asan`
landed in a target nobody runs. `test-no-orphan-gates` and
`test-host-tests-wired` exist for exactly this.

## Mutation testing

**A survivor count is a coverage figure, not a bug count.** 35 survivors
out of 51 comparisons; the first three triaged were not defects.

**"Equivalent mutant" hides two answers.** Equivalent by ARITHMETIC (no
input distinguishes them) versus equivalent because a guard elsewhere
absorbs it. The second is a real coupling worth recording.

**One argument can retire a class of survivors.** Relaxing an early-out
can only cause more work, never less — that settled six at once.

**Choose the operator to suit the idiom.** `>=` → `>` found 17 real gaps;
the symmetric sweep found 70 survivors of which 30 are uncatchable by
construction (19 are clamps, where re-assigning N when x is already N has
no observable effect).

**"Caught" can mean "did not compile".** `mutate.py` read a make target's
exit status, so a malformed replacement counted as detected. It checks
that the mutant BUILDS now.

**Reach for the tool when the fixture stops working.** Nine
memory-safety defects each needed a bespoke fixture, and two defeated it
entirely — writes outside a static array that nothing in the program can
observe. That is what ASan is for.

## Test infrastructure

**Floppy gates share one image — never run two concurrently.** Or give
each its own `BATTY_TEST_FLOPPY`, and derive the AUTOEXEC scratch from
it too (that was the second shared-state collision). A result from a
gate run alongside another is meaningless.

**Cap inner parallelism when running under the outer runner.** Too many
concurrent QEMUs each run slower than real time, which breaks wall-clock
frame waits and makes every gate "diverge".

**Counter-phase cadences need `BATTY_REPLAY_COUNTER` pinned** — in
assertions AND in any test that boots twice and diffs. The pin does NOT
survive a checkpoint halt, so only the FIRST checkpoint of a timeline is
valid for an A/B.

**"Only passed on retry" is unexplained until measured.** The parallel
runner printed `(starved when parallel)`, which is a guess; the real
cause was a coin flip.

**A multi-edit script that aborts writes NOTHING.** Verify the file with
`grep -c`, not the script's intent. A missing edit usually still
compiles.

**When a new array is indexed by an existing enum, check the enum's
VALUES.** `bats[OBJ_BAT_1]` reads perfectly and is nonsense —
`OBJ_BAT_1` is 6, a slot in the object table. Compiles clean under
`-w4 -we`.

**Reproducibility first, then normalise and diff.** 2/2 vs 4/4 settled
that a "pure rename" really had changed behaviour; `sed`-normalising the
rename reduced a 70-line diff to the one semantic line.

**ZEsarUX: `enable-breakpoints` before `set-breakpoint`.** Otherwise the
set silently fails and `run` sails past the target PC.

## Documentation and process

**Gate the drift, not the truth.** No gate can read "Done" and know. It
CAN insist a document is re-read when work lands —
`test-plan-table-fresh` fails when a workstream entry is newer than the
table's refresh date. Writing a better note is the intervention that
already failed, applied harder.

**A rename is not done until you grep the PROSE.** Present-tense prose
about a renamed thing is a lie with a citation attached.
`notes_symbols.py` was itself defeated by stale prose — its corpus
included comments, so a name surviving only in the comment that was
wrong about it counted as defined. Stripping prose took the report from
36 to 58 names.

**When a checker's haystack includes documentation, decide whether a
MENTION is a USE.** Usually it is not.
`check_no_dead_constants`' own docstring listed eleven dead constants,
which made all eleven look alive.

**One commit can rot five files and nothing sweeps for it.** The 386
switch listed everything it deleted and updated none of the prose
describing it.

**Audit post-processing when you change a pipeline.** A leftover
`clean_gts.py` wiped the very band the new capture had got right, and
cost a session of "missing magnets" debugging.

**Don't let the test exclude the surface you are iterating on.** A
residual explained as "the GT can't show the bat" is a blind spot, not a
floor. Recapturing surfaced 427 px of real drift immediately.

**When a new gap traces back to an accepted residual, update the
residual.** The "do not reopen" list is the one entry nobody re-reads,
so the trade-off recorded there has to stay current.

**Check whether the fix already exists before designing around its
absence.** `BATTY_REPLAY_COUNTER` had been in the port for months, its
comment naming the exact flake being worked around — and this file
already discussed it by name.

**Re-test conclusions that were true when measured.** "Hosted runners
have no KVM" was written into the workflow header and PLAN.md and left
to age for two years. `ls -l /dev/kvm` overturned it in seconds — and
the correction itself was still half wrong, because a device node
present is not the same as usable by the container user. Three answers,
each cheaper than the last: measure the thing you actually need.

**Look at CI.** It was red for 163 runs, about 24 hours, and was found
only by accident — broken by the commit that made it honest.
