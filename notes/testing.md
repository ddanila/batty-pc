# Testing

> **`make test-fast` needs no emulator and runs in seconds** — 14 host test
> suites plus 30 source gates. Start there; it is exactly what CI runs.
>
> The QEMU gates cost ~10 s per boot. `make parity-check-parallel --full`
> runs all 79 gates of the full sweep in about seven minutes (78 QEMU plus
> `test-asan`, which is host-only but belongs to the same sweep).
> `make parity-check-full` adds the 3 ZEsarUX-oracle gates, serially.
> 109 gates in all, indexed below.

## How `make test` works

1. **Build the floppy** via the normal Makefile path.
2. **Launch QEMU headless** with `-display none -monitor stdio`. The Python
   harness drives the monitor: sleep for DOS boot, `screendump` to a PPM,
   `sendkey` to advance the state machine, repeat, `quit`.
3. **Decode the PPM.** QEMU emits mode-13h frames at 2x scale (640x400).
   DAC scaling is `56 -> 224`, `63 -> 255` — plain `<<2` for non-bright,
   max-out for bright, NOT the textbook `(v<<2)|(v>>4)`. Sample one pixel
   per VGA cell and look up the RGB in the 16-entry ZX palette.
4. **Build the expected indices** from the matching ZEsarUX snapshot's
   `screen.scr` via `extract_scr.py`.
5. **Compare in RGB space, not index space.** Indices 0 and 8 both render as
   `(0,0,0)` — bright and non-bright black — but `extract_scr` emits each per
   the attr's bright bit. Comparing `PALETTE_RGB[a] == PALETTE_RGB[e]` makes
   them equivalent, and every comparison tool in the repo must do this.
6. **Diff PNG on failure**, to
   `build/test_visual/<checkpoint>_diff.png` — red where pixels disagree,
   grey for context.

### The five checkpoints

The test drives the full attract flow (TITLE -> MENU -> HISCORE -> LEVEL) on
`batty-test.img`, which sets `BATTYALL=1` in AUTOEXEC so the C side disables
auto-advance; every transition comes from `sendkey ret`.

| State | Renderer | Expected |
|-------|----------|----------|
| 1 | `LOADING.BIN` static blit | `original/Batty.scr` |
| 2 | markup + sprites + blink | snap2 `20260513T202041Z` |
| 3 | markup hi-score | snap1 `20260513T202038Z` |
| 4 | full level-N gameplay paint | `build/level_gt/level_NN.scr` |
| 5 | state 4 ROI'd to the bat band (y=160..192) | same GT |

All five are FAIL-gated on L1, and all 15 levels are pixel-identical via
`BATTY_LEVEL=N` (`notes/per-level-profile.md`). State 5 is a sub-diff of
state 4 so bat-render regressions surface on their own instead of hiding
inside a whole-frame number.

`BATTYALL` also pins the menu blink, the running dot, the magnet toggle and
natural alien spawns, so the captures are deterministic. Tests that need an
alien seed one via `BATTY_REPLAY_ENEMY_OBJECT`.

## INFO is for accepted drift, not unmeasured surface

`test-visual`'s checkpoint table has an `assert_match` flag. False keeps the
row — it still captures, diffs and prints "pixel-identical" — but it fails
nothing. That is the right tool for a known residual and the wrong one to
leave lying around: three rows sat at False for residuals long since fixed.
The table is linted now — a bare `False` fails, `False,  # INFO: <why>`
passes — because the row itself will never tell you it has gone stale.

If a residual is explained by "the GT can't show this region", that is a
blind spot, not a floor: recapture the GT, or split the region into its own
ROI with its own number. A diagnostic row's answer can also change without
anyone noticing — that is the point of a row that never fails — so re-run it
before doing the work a plan says it implies.

## Every gate, and what it is for

Kept complete by `scripts/check_gate_index.py`, which fails if any of the
THREE places gates are defined — `run_gates_parallel.py`, the
`test-source-gates` recipe, and `parity-check-full` — names one this section
does not. A gate nobody can find is a gate nobody reasons about: before this
list existed, 30 of 59 were mentioned nowhere in this file, including several
of the oldest.

**The four-state cycle and the levels**
- `test` — the MENU/TITLE/HISCORE/LEVEL screens against captured originals.
- `test-levels-sweep` — the same state-4 check for each of the 15 levels.
- `test-laffc-levels-sane` — every level's grid loads and paints sanely.
- `test-level-advance` — clearing a level advances the round; the index wraps at 15.
- `test-hud` — score, lives and round digits in the HUD band. A separate
  normal-build check, because `make test` uses `BATTY_SCORELESS_HUD`; it
  compares the stable `1UP`/`HI`/`2UP` and zero-score regions and excludes
  the high-score digits, which vary with the local `HISCORE.DAT`.

**Ball physics**
- `test-normal-ball-launch` — the launch trajectory from the bat.
- `test-wall-bounce`, `test-ball-left-wall-escape` — side walls, including the clamp.
- `test-bat-deflection` — the LAB1F deflection table against captured ground truth.
- `test-ball-speed-ramp` — the speed-up over a rally.
- `test-ball-no-tunnel`, `test-ball-paths-no-tunnel` — the ball never passes through a brick.
- `test-laffc-ball-frame1`, `test-laffc-ball-l5-metal` — byte-exact ball-vs-brick, oracle-confirmed.
- `test-magnet-ball` — capture, curve and release.
- `test-stuck-ball-offset` — one function decides where a held ball rests (#12).
- `test-ball-sign-cache-owner` — only the primary ball writes its own sign cache (#13).
- `test-stuck-auto-launch` — a held ball launches itself after 192 ticks.

**Bricks and scoring**
- `test-brick-scoring` — points per row, doubled for metal.
- `test-brick-flash`, `test-brik-anim-pace` — the hard-brick shimmer and its cadence.
- `test-l3-replay-seed` — the L3 seed the oracle gates depend on.
- `test-midgame-brick-replay` — a mid-game grid against the original.
- `test-frame-step` — the byte-exact frame-step oracle (ZEsarUX).
- `test-gameplay-soak` — a long multi-level run holding every invariant.

**Enemies**
- `test-enemy-descend`, `test-enemy-steer` — the entry slide and the turn cadence.
- `test-enemy-anim` — the LAAD2 sprite walk.
- `test-enemy-attr-parity` — the alien's attribute cells.
- `test-enemy-flyover-redraw`, `test-enemy-brick-residue` — no residue when it passes over.
- `test-enemy-brick-walk` — it hits a brick: nothing breaks, and it walks to the snap point.
- `test-enemy-margin-clamp` — it reaches a wall: clamped, not bounced.

**Weapons, bonuses, the rocket**
- `test-bullet-fly`, `test-laser-cadence`, `test-bullet-blast` — the laser and its hit.
- `test-bomb-fall`, `test-pts400-fall`, `test-bonus-fall` — the three falling objects.
- `test-bonus-drop`, `test-bonus-typepick` — when a bonus drops and which one.
- `test-bonus-effects`, `test-bonus-effects2` — every bonus effect.
- `test-rocket-bonus`, `test-rocket-flight-redraw`, `test-rocket-completion-no-ball` — the level-clear rocket.
- `test-death-sparks` — the bat explosion: the `LBC10` spawn constants, the
  `bounce_wall` thresholds, the `LAD13` signed direction math, and the
  post-spark hold before the life is decremented.
- `test-round-banner-border` — the PLAYER/ROUND window's top band.

**Dirty-redraw A/B (the narrow path must equal the full path)**
- `test-ball-dirty-redraw`, `test-ball-object-dirty-redraw`, `test-stuck-ball-dirty-redraw`
- `test-bat-redraw-window`, `test-bat-fire-dirty-redraw`
- `test-bullet-dirty-redraw`, `test-bomb-dirty-redraw`, `test-blast-dirty-redraw`
- `test-multiball-dirty-redraw`, `test-bigball-dirty-redraw`
- `test-sprite-attr-parity` — no sprite corrupts an attribute cell.
- `test-visual-checkpoints` — the multi-checkpoint capture path itself.

**Game flow**
- `test-life-loss` — losing a life removes exactly one indicator.
- `test-life-respawn` — a death gives back a centred bat, a fresh ball on it
  and no leftover bonuses. A transition has two halves; `test-life-loss`
  covers only the taking.
- `test-respawn-redraw` — after a death the bat and magnets are drawn the
  same whether or not the life count changed (#21).
- `test-game-over`, `test-game-over-visual` — the sequence's order, and the screen.
- `test-name-entry-visual` — the NEW HIGH SCORE name entry.
- `test-two-player-state`, `test-two-player-turn` — two sets of counters, and
  the hand-over on a life loss in mode 1 but not mode 0.

**Double Play**
- `test-double-play-court` — mode $02 moves both bats and draws the divider.
- `test-double-play-input` — the split keyboard steers one bat each, and the
  court clamps hold the divider.
- `test-double-play-bat2` — bat 2 deflects the ball and takes ownership of it.
- `test-double-play-bat2-catch` — bat 2 catches the ball and holds it on its own bat.
- `test-double-play-bat2-redraw` — bat 2's sprite tracks its object (a PIXEL gate).
- `test-double-play-bat2-width` — BIG_BAT widens the bat that caught it.
- `test-double-play-bat2-laser`, `test-double-play-bat2-gun` — player 2's
  laser leaves bat 2, and an armed bat 2 draws the gun body.
- `test-double-play-bonus-catch` — bat 2 catches a falling bonus and is paid for it.
- `test-double-play-alien-kill` — bat 2 kills the alien; the 350 lands on 2UP.
- `test-secondary-ball-catch` — a MAGNET bat holds a secondary ball too.
- `test-extra-ball-bat2` — a secondary ball meets bat 2.
- `test-extra-ball-owner` — the owner bit is per ball; a deflection moves only
  the ball it hit.

**Structural (no emulator)**
- `test-gate-greps` — the gates' source needles still match the source.
- `test-gate-index` — every gate is named in this section.
- `test-gate-freshness` — no gate can be satisfied by a previous run's
  captures or `PROBE.TXT`.
- `test-no-orphan-gates` — every gate script is run by a target.
- `test-host-tests-wired` — every host suite runs under `make test-fast`.
- `test-switch-defaults` — each debug switch's documented default matches the
  initialiser.
- `test-env-passthrough` — every `BATTY_*` knob reaches DOS on the test floppy.
- `test-no-dead-constants` — every `#define` in `src/` is used.
- `test-module-ownership` — a module that declares state defines it.
- `test-invariant-owners` — two-place state changes have one writer each.
- `test-frozen-clock` — nothing times anything with `bios_ticks()` (#15).
- `test-shimmer-one-pass` — the metal-brick shimmer plays one pass (#3); no
  QEMU gate covers it.
- `test-multiball-source` — the multiball spawn reads the primary's dir byte,
  and `delta_to_dir` has no production caller (#14).
- `test-hud-patch-extent` — the in-place HUD patch covers every row the score
  digits occupy. A source gate by necessity: the visual executable is built
  `-dBATTY_SCORELESS_HUD`, so no screendump in the repo has a score on it
  (#22).
- `test-menu-start` — key 0 in the menu starts a game; ENTER still walks the
  attract chain.
- `test-kinnock` — the easter egg's text, coordinates and placement.
- `test-sound-ids` — effect ids are `play_sounds_list` positions.
- `test-rng-walk` — the RNG walk matches the original's sequence.
- `test-floppy-assets` — the image carries exactly what the port loads.
- `test-frame-derivable` — the frame's top and side strips are tape sprites,
  not capture.
- `test-bg-tile-derivable` — the hex tile and `bg_attr_per_cycle` are tape data.
- `test-level-attrs-derivable` — the live-brick fifth of `level_attrs.bin` is
  computed, not captured.
- `test-asset-provenance` — every loaded asset is built from the tape.
- `test-doc-links` — every file path cited in a comment or note exists.
- `test-known-bugs-table` — the bug table agrees with the sections below it.
- `test-plan-table-fresh` — PLAN.md's definition-of-done table is no older
  than the newest dated workstream entry.
- `test-notes-numbers` — the plan's status block still states true numbers.
- `test-asan` — the same 14 suites rebuilt under ASan + UBSan. Not a source
  gate but wired into the full sweep: nine memory-safety defects had each
  needed a bespoke fixture, two of them were invisible to a normal host
  build, and it found a real out-of-bounds read in `replay_parse_hex_bytes`
  on its first run.

### Notes on a few of the harder ones

`test-ball-no-tunnel` is the collision-invariant sweep. For each level it
boots once to read the initial grid, picks solid target bricks, then for each
(target x approach x speed) seeds the ball one step away aimed into the brick
and asserts the INVARIANT: a ball aimed into a still-solid brick must change
that brick's state or reverse direction; if it crosses the brick's far edge
while still overlapping its column and nothing changed, it tunnelled.
ZEsarUX-free. The default subset covers L1/L5/L7 — L5 and L7 have row-0 metal
bricks against the top boundary, the exact known-bugs #6 repro. `FULL=1` runs
all 15 levels x speeds 2/4/6 x straight and diagonal approaches. It also
carries a field-bounds invariant: the ball must never escape (x in [8,244],
y >= 8).

`test-ball-paths-no-tunnel` extends that to the NON-primary paths —
`step_extra_ball` and a ball captured inside an ON magnet — asserting that no
active ball's CENTRE sits inside a solid brick cell (a bounced ball snaps to
the cell edge, so its centre is outside).

`test-gameplay-soak` drives sustained play on L1/L3/L5/L9 and samples
checkpoints from 30 to 150 frames, asserting per checkpoint that no ball is
inside a brick or outside the walls, and across checkpoints that brick count
only falls and score only rises. Those hold whether the ball is bouncing or
has dropped and respawned, so it needs no ball pinning. It uses the
`BATTY_SERIAL_PROBE` deterministic frame wait — required, because its 20
concurrent per-case boots oversubscribe cores and wall-clock waits read the
pre-gameplay seed state and produced false "bricks rose / score fell"
violations.

`test-brick-flash` drives a dynamic L3 path and fails if the bright-white
destruction flash remains after it should clear, or if no brick-sized cell
stays visibly removed. The stale-flash decision is reference-derived — each
cell's bright-white coverage is compared against the original-captured L3
render — so it catches both the dirty-line white-block and stale-static-
background failures without hard-coding that every white pixel is wrong.

## CI is a second compiler, and authoritative for one thing

`.github/workflows/parity-check.yml` runs `make test-fast` and
`make test-asan` on `ubuntu-latest`, where `c++` is g++. Nothing local stands
in for it: Apple clang and g++ disagree about uninitialised analysis, and
that gap kept `main` red for **163 runs** (~24 hours) while every local sweep
was green. **Read the run after you push** — `gh run list --limit 3` is
enough. A green local sweep is not the same claim as a green CI.

It delegates to `test-fast` rather than naming targets. It used to name
`test-video` and three gates by hand, so CI ran 1 of 14 suites and 3 of 10
gates while showing a green tick — and naming targets there made it a THIRD
copy of a list that had already drifted twice. `check_host_tests_wired.py`
guards the one list that is left.

CI fetches only the `original/disasm` submodule (`--depth 1`), which
`make test-fast` needs: `test-kinnock` parses its expected text straight out
of the disassembly, and three other checks read it too.
`tools/zesarux` is not fetched, since CI runs no oracle gates.

**The QEMU gates are not in CI, and the reason on file is now out of date.**
The original calibration concluded "hosted runners have no KVM, so QEMU runs
under TCG slower than real time". A 2026-08-10 probe found `/dev/kvm`
PRESENT on `ubuntu-latest`, so that premise is false and every timing in that
calibration stands only for TCG. What is still true is that the measured
attempts were slow and flaky under TCG — a 2-gate smoke took ~9 minutes — and
that the wall-clock frame-wait problem which made every gate diverge is
fixed independently (the `BATTY_SERIAL_PROBE` COM1 frame-completion signal is
frame-exact at any speed). `qemu-smoke-kvm` is a `continue-on-error` job that
measures the KVM path; nothing depends on it yet. Re-measure with
`-accel kvm` before treating any of this as settled. PLAN.md WS8.1.

**Re-test conclusions that were true when measured.** "Hosted runners have no
KVM" was written into the workflow header and PLAN.md and left to age for two
years; `ls -l /dev/kvm` overturned it in seconds — and that correction was
itself still half wrong, because a device node present is not the same as
usable by the container user. Three answers, each cheaper than the last.

## Running the suite in parallel

`make parity-check-parallel J=8`. The gates are boot-dominated and were
historically serial because every script hardcoded the one floppy. The path
now comes from **`BATTY_TEST_FLOPPY`**: the Makefile's `TEST_FLOPPY_OUT`
honours it and derives a per-floppy AUTOEXEC scratch, and the gate scripts
read it through `test_visual.test_floppy()`. `run_gates_parallel.py`
pre-builds the shared `TEST_EXE` once so workers do not race on the object
file, then runs each gate on its own image.

**If you add a gate, take the floppy from `test_floppy()` — never a
literal.** Thirty gates once hardcoded `build/batty-test.img` while a comment
claimed they all read the variable; under the parallel runner they either
died in 0.1 s or built one image and read `PROBE.TXT` from another. It took
three passes to find them all, because the same bug was spelled three ways:
`Path("…")`, `FLOPPY = "…"` and `FLOPPY = '…'`.

**A gate is not one boot.** `test-ball-no-tunnel` boots dozens of times,
`test-levels-sweep` fifteen. `--full` at J=8 starved QEMU below real time and
produced pure-contention failures, so `--full` defaults to a quarter of the
core count and **any failure is retried once alone** — only a gate that fails
twice is reported, and the ones that needed the retry are named, so a growing
list means J is too high for that machine.

The ZEsarUX gates are EXCLUDED from the parallel runner: they drive a single
ZRCP port (10000) and a shared snapshot, so they go through the serial
`make parity-check-full`.

Reliability net: a wait-key gate that reads a probe written at level init
(`probe_phase=init`, i.e. a missed `BATTY_REPLAY_WAIT_KEY` wake on a slow
boot) re-boots until it sees a real checkpoint write (`probe_phase=play`).
Centralised in `capture_frame_timeline.py`, the shared driver ~20 wait-key
gates route through; gates that drive `run_qemu` directly use
`test_visual.boot_until_gameplay()`.

**`make test-gate-greps`** guards the other recurring failure: 20 gates
assert on the SHAPE of the source — that a constant is still `$1B`, that a
guard still excludes `rocket_active` — by searching for a literal, and those
rot silently when code moves. Twice in one session that cost six commits of
red CI and a gate that passed while testing nothing. The check resolves
needles through variables and list comprehensions and only considers
*required* ones.

## Mutation testing (`scripts/mutate.py`)

A green suite says the tests pass, not that they would fail if the code were
wrong. Asking the second question found five real gaps: `check_gate_greps`
skipping a third of the gates, `test_objects` and `test_weapons` pinning the
SHAPE of a value rather than the value, and `zxvga` never checking where a
sprite lands.

    scripts/mutate.py <file> <find> <replace> <make-target> [label]

    exit 0  caught     — the tests failed, which is the good outcome
    exit 1  SURVIVED   — a gap, or an equivalent mutant. Decide which.
    exit 2  ERROR      — the substitution matched nothing

Doing this by hand went wrong four ways, twice producing a confident false
result, so the script handles all four:

- **Stale binary, same second.** Restoring a source within the same second as
  the last build leaves the timestamp unchanged, `make` reruns the OLD
  binary, and the mutation looks caught.
- **Stale DOS EXE.** The QEMU gates boot `build/batty-test.exe`. A module
  change rebuilds its own `.obj`, but if the link lands inside the same
  filesystem second the EXE is untouched — mutating `src/physics.cpp` changed
  `physics-test.obj` (md5-verified) and left `batty-test.exe` byte-identical,
  so the gates ran the ORIGINAL code and every QEMU result was meaningless.
- **Stale binary, wrong name.** `make test-video` builds
  `build/test_zxvga`, so deleting `build/test_video` deletes nothing. Every
  run then used a stale binary and a REAL gap was reported as caught; it
  surfaced only because a restored source still failed, which cannot happen.
  The script deletes every `build/test_*` file rather than guessing.
- **Silent no-op.** A substitution matching nothing leaves the source clean
  and the test passing, which reads as "not caught" — the worst outcome,
  because it looks like a finding.

### Known equivalent mutants

Survive by design. Do not re-investigate; if one starts being caught,
something else changed.

- `zxvga.cpp` `byte_hi = (x_end - 1) >> 3` -> `x_end >> 3`. Marks one extra
  dirty byte; the flush then copies a byte that is already correct.
- `bricks.cpp` `repaint_row_top_edge` — no test can distinguish it, and
  `notes/levels.md` explains why (the runtime passes redo the work).
- `physics.cpp` `laffc_sweep`'s four boundary terms (`cell_x == FIELD_X0` and
  friends). `BrickField::standing` treats out-of-range as gone, so the
  neighbour check alone already opens an edge face — deleting a term changes
  nothing, while INVERTING one does, and `test_boundary_faces_stay_open`
  catches that.

### One class that is NOT gated, and why

Comments that duplicate an explanation and then drift have caused four real
problems: the bricks header copied into its `.cpp`, two blocks in front of
the SPACE handler, the RNG default saying OFF after it flipped, and a
bat-resize note claiming "roughly matches" long after the gate that made it
exact. The last cost an afternoon chasing a bug that was not there.

That looks gateable. It is not, and the reasons are worth recording so the
next person does not build the gate and then trust it:

- **Exact-sentence matching finds nothing.** A scan for identical sentences
  across and within `src/*.{cpp,h}` reports ZERO. All four real cases were
  PARAPHRASES, so a sentence gate would have been green for every one of them
  while feeling like coverage.
- **Provenance-address co-citation is too noisy.** 47 original addresses are
  cited from two or more comment blocks, and nearly all are legitimate —
  `$A67B` alone appears in four places, all correct. A gate here would fire
  constantly and be switched off inside a week.

What catches these is reading the code near what you are changing, and
noticing when two explanations of one thing disagree.
`test-switch-defaults` gates the one sub-case that IS mechanical.

## Counter-phase sweeps (`scripts/phase_sweep.py`)

`pit_frame_counter` free-runs from boot and cadences key off its low bits —
the enemy steer (`& 3`), the ball speed ramp (`& 7`) — so how long boot took
decides which phase a probe frame lands on, and a gate whose expectations
depend on the phase passes or fails by luck. That is known-bugs #17.

Running a gate repeatedly is how it was found, but repetition is a weak
instrument: it samples whatever phases the machine happened to produce.
`BATTY_REPLAY_COUNTER` pins the counter at the aligned start, so the phase
can be varied on purpose:

    scripts/phase_sweep.py test-enemy-anim test-bat-deflection

runs each gate at phases 0..3 — every case `& 3` can produce. Passing at all
four means the gate does not depend on the phase; failing at some means it
was passing by luck. **Run it on any new QEMU gate before trusting it.**

Gates that set `BATTY_REPLAY_COUNTER` in their own env are reported SKIPPED
rather than swept: their inline value overrides the outer environment, so all
four runs would use the same pin and report a confident, meaningless
"phase-independent". (The first version detected that by searching the whole
file, which skipped every gate whose DOCSTRING merely explains the variable —
a false negative wearing the costume of a decision. It strips docstrings and
comments now.)

Validated by removing the pin from `test-enemy-margin-clamp`, whose `dir`
expectations are exact: the sweep reported PHASE-DEPENDENT at pins 1, 2 and
3. A tool that can only ever say "fine" is not a tool.

Audited so far, all phase-independent: `test`, `test-laffc-ball-frame1`,
`test-bat-deflection`, `test-ball-no-tunnel`, `test-rng-walk`,
`test-enemy-anim`, `test-enemy-attr-parity`, `test-l3-replay-seed`. The rest
has not been swept — at four boots per gate it is a couple of hours, worth
spending when a gate next behaves oddly rather than pre-emptively.

Note the pin does NOT survive a checkpoint halt, so only the FIRST checkpoint
of a timeline is valid for an A/B.

## Reading the original (`scripts/disasm.py`)

    scripts/disasm.py handling_bird     # by label
    scripts/disasm.py 0xA67B            # by address, via "; Routine at XXXX"
    scripts/disasm.py margin -l         # list labels containing a substring

`original/disasm/batty.asm` answers questions that otherwise cost emulator
runs, and it settled three in one week: whether the enemy is reflected at a
wall (no — `check_margins` clamps, `bounce_wall` reflects, and the enemy gets
the first), whether the multiball spawn reads a velocity (no — it reads the
dir byte), and what the bat resize's gating is (every other frame, which the
port already matched). Each started as a plausible guess that turned out
wrong.

A routine prints from its label to the next one, with the following
routine's comment header trimmed. Mid-routine entry points (`LA67B_8` and
the like) are labels too, so they print just their own stretch. It prints a
**FALLS THROUGH** warning when a routine's last instruction is not an
unconditional `RET`/`JP`/`JR`, which is the single most expensive class of
misreading here (`notes/lessons.md`).

## Stale symbol citations (`scripts/notes_symbols.py`)

`check_doc_links` catches a note that points at a file which no longer
exists. Nothing caught a note that names a ROUTINE which no longer exists,
and that is how these notes actually rot: something is renamed or deleted,
and prose that was true keeps naming it.

    scripts/notes_symbols.py

lists every backticked `snake_case` identifier in `notes/*.md` that nothing
in `src/`, `tests/`, `scripts/`, the Makefile or the disassembly defines.

The case that prompted it: `bounce_enemy_off_margins` was deleted and three
notes still named it — one of them, known-bugs #16, in the PRESENT TENSE,
asserting the exact opposite of the code. The reasoning around it was still
correct; only its premise had rotted, which is the hard kind to notice. The
tool was itself defeated by stale prose at first, because its corpus included
comments, so a name surviving only in the comment that was wrong about it
counted as defined.
