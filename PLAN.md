# PLAN — road to a 100% functional Batty port

Goal of the repo: a **fully functional MS-DOS/8086 port of Batty**
(Elite/Hit-Pak, 1987, ZX Spectrum 48K) — everything the original game
does, the port does, verified against the original wherever it can be
measured. This file is the roadmap to that goal; status lives in
`notes/parity-status.md`, open fidelity gaps in `notes/parity-gaps.md`.

Last updated: 2026-07-06.

## Definition of done

The port is "100%" when all of the following hold:

| # | Criterion | Today |
|---|-----------|-------|
| 1 | All three game modes work: 1 Player, 2 Players (alternating), Double Play (simultaneous split-court co-op) | 1P and 2P done; Double Play has its court, both bats, ball physics and scoring but no bat-2 INPUT |
| 2 | Menu semantics match the original (0 starts the selected game directly; A/B input-device cycling affects play) | Key 0 starts the game (`test-menu-start`); the device byte is per-player state but still selects nothing |
| 3 | Core gameplay byte-exact where an oracle exists (ball, bat, collision, RNG, enemy, bonuses, scoring) | **Done** — regression-locked by 90 gates |
| 4 | Full game FLOW gated end-to-end: level-clear → next, life-loss → respawn, game-over → initials, level wrap | **Done** — `test-level-advance`, `test-life-loss`, `test-game-over-visual`, `test-name-entry-visual` |
| 5 | Sound faithful to the original's 5-slot beeper queue (envelope/timing, not just effect IDs) | ids, slot count, pitches and envelope ARITHMETIC faithful; durations still round to 20 ms because the sound clock is the 50 Hz frame counter |
| 6 | All assets derived from the tape at build time; no captured emulator blobs | **Done** — all 13 loaded assets build from `original/blocks/`, held by `test-asset-provenance` |
| 7 | Runs correctly on real-hardware-representative targets (XT-class + 386) | QEMU + 86Box `ibmxt` verified; real iron untested |
| 8 | Historical completeness: Kinnock easter egg, pause semantics, hi-score behaviour | **Done** — pause, hi-score, and the easter egg (`BATTY_KINNOCK=1`) |

*Table refreshed 2026-08-09. Four rows were stale, and two of them
UNDERSTATED the state — criterion 4's transitions had been gated for
weeks while this table still said "unverified", and criterion 6 was met
during the WS7 work. A status table nobody re-reads is the same defect
as a parity note nobody re-reads; `notes/testing.md` and
`notes/parity-gaps.md` both had the transition gates listed.*

Byte-exactness of the already-achieved core (criterion 3) is a floor,
not a ceiling to re-litigate: every workstream below must keep
`make parity-check` green, and milestone-level work runs
`parity-check-full` before merge.

## Where we are (August 2026)

Playable end-to-end in 1- and 2-player mode: title → menu → hi-score →
all 15 levels → game-over → initials → title, with 2-player handing the
turn over on each life loss and on a player running out. All 15 levels pixel-perfect at
entry; ball motion, LAFFC brick collision, bat deflection, RNG walk,
enemy motion/steering/animation, bonus economy, scoring, and all
per-frame animations are byte-exact vs the Spectrum and gate-locked.
Rendering perf is concluded ("at its floor"); an optional 386 build
exists. The hard RE is done — `original/disasm/` (CityAceE) is a
complete, named, build-verified disassembly; the port workflow is
"read the disasm, port the routine, gate it."

What follows is ordered by value. Workstreams 1–4 make the port
*functionally complete*; 5–7 close the remaining *fidelity* gaps;
8–9 are infrastructure and polish.

---

## WS1 — Menu start semantics (small, do first)

**What:** Pressing 0/ENTER in the menu currently falls through the
attract chain (`src/main.cpp` ~L4006 "would start a game; advance for
now") instead of starting the selected game directly. Port the
original `main_menu` dispatch (see `routines/main_menu.asm` in the
disasm): 0/ENTER → start game in `selected_mode`, and make
`p1_dev`/`p2_dev` actually select the input source.

**Input-device adaptation (decision):** the original's device list
(Kempston / Sinclair / cursor joysticks) is Spectrum hardware — a
literal port makes no sense on PC. Adapt, don't transcribe: keep the
menu's A/B cycling and layout, but offer PC-meaningful devices —
**keyboard set 1**, **keyboard set 2** (a second key cluster, needed so
two players can share one keyboard in Double Play), and the **game-port
analog joystick** (port 0x201 / INT 15h AH=84h) where present, with a
graceful "not detected" fallback. Menu strings change accordingly; this
is a deliberate, documented deviation from pixel-parity on the menu
screen (gate the menu checkpoint against an updated reference, or keep
original strings and remap meaning — decide when implementing;
recommendation: update the strings, honesty beats fake parity here).

**Why first:** It's the entry gate for WS2/WS3 — mode wiring is
meaningless while mode selection can't start a game. Small, sharply
scoped, and it converts a documented stub into original behaviour.

**Exit:** menu 0/ENTER boots straight into the round-1 banner of the
chosen mode; a source-level gate pins the dispatch; attract-timeout
behaviour unchanged.

**Dispatch DONE (2026-08-09).** Key 0 now returns ST_LEVEL, which runs
`new_game_reset` and enters the round-1 banner; unrecognised keys re-poll
instead of leaving the menu; the attract timeout is untouched. Gated by
`test-menu-start`.

Two corrections to what this section said:

- It is **0 only**, not "0/ENTER". The original's tail is
  `LD A,$EF / CALL in_a_fe / AND $01 / RET NZ` on the $EFFE row and
  there is no ENTER in `main_menu` at all. ENTER is kept as the port's
  own attract-chain affordance, and that is load-bearing: `test-visual`
  walks title -> menu -> hi-score -> level by sending ENTER at each
  state, so making ENTER start a game would have turned its
  `state3_hiscore` checkpoint into a level capture — and since it diffs
  each checkpoint against its own reference, the failure would have read
  as a rendering regression on the wrong screen.
- "boots into the banner of the chosen mode" is only true for mode 1.
  Modes 2 and 3 are still inert, so picking one and pressing 0 starts a
  1-player game. Recorded here rather than papered over in the code.

**Device STATE done (2026-08-09), selection still open.** `p1_dev` and
`p2_dev` are no longer standalone globals: they alias
`players[0].ctrl_type` / `players[1].ctrl_type`, because the original
keeps the device byte as `ctrl_type`, +7 of the eight-byte per-player
block at `lives_1up` that `players_swap` exchanges wholesale. A player's
device travels with their lives, level and score. As two loose bytes it
would have drifted the first time a turn changed.

The menu still only CYCLES it. Nothing reads it to choose an input
source, and `get_right_player_ctrl_state` — the original's bat-2 reader —
has no port equivalent.

**The blocker I described here was wrong** (2026-08-09). I said the
labels could not be relabelled honestly because the menu screen is a
captured blob. It is not: `render_menu_screen` builds it from
`MENUMARK.BIN` markup, and `MAINMENU.BIN` is only `test-visual`'s
reference. Changing the four option labels is a MARKUP edit plus a new
reference capture — not a screen-generation project.

So the decision is just the adaptation itself: which three PC devices
the four slots should offer, and whether to keep the original's strings
with remapped meaning (fake parity, which this section argues against)
or write new ones and re-baseline `state2_menu`. Recommendation stands:
write new strings.

## WS2 — 2 Players mode (alternating)

**Stage 1 done (2026-08-09): per-player state.** `PlayerState` is now
`players[2]` with an `active_player` index, `new_game_reset` clears both
and returns the turn to 1UP, and the HUD's 2UP slot reads
`players[1].score` instead of the literal `0` it printed before. The
high score moved OUT of `PlayerState` — there is one per machine
(`hi_score_in_game`), and per-player it would have made the middle HUD
column change when the players swapped. The static cache's dirty test is
per-slot for the same class of reason: with two players the 2UP score
changes while `player` does not, so a cache keyed on the active player
would show a stale number.

**Stage 2 done (2026-08-09): per-player progress.** `round_number` and
the level index are now `players[active_player]` fields. The original's
per-player block is eight bytes at `lives_1up`, exchanged wholesale by
`players_swap`:

    +0  lives
    +1  briks_quantity        bricks left on THAT player's level
    +2  current_level_number  0..14
    +3  round_number          may exceed 15; the level wraps, this does not
    +4..6 current_score       three BCD cells
    +7  ctrl_type             the input device, PER PLAYER

so a player resumes their own round and level rather than a shared one.
That last byte is also the answer to where WS1's device selection
belongs: `p1_dev`/`p2_dev` are the menu's copy of `ctrl_type`.

Nothing moves `active_player` off 0 through either stage, so behaviour
is byte-identical and all 61 QEMU gates are unchanged.
`test-two-player-state` holds both: no screendump can tell a literal `0`
from `players[1].score` in a 1-player game, nor a per-player counter
from a global while there is one player.

**The level-reset question is settled (2026-08-09).** The grid IS
preserved per player. `current_level_2up_copier` exchanges the live
180-cell grid with the arriving player's level slot — the level table
doubles as per-player storage, there is no separate save area — and then
FALLS THROUGH into `players_swap`, so one call does the grid, the
counters and the turn toggle. It is called from the life-loss path for
`game_mode == 1` only, and its `lives_2up == 0` guard covers both halves
so a solo player keeps playing.

Also settled: `game_mode` is 0-based (0 = 1 Player, 1 = 2 Players,
2 = Double Play) while the port's `selected_mode` is 1..3, so the
dispatch has to subtract one.

**Stage 3 done (2026-08-09): the mode itself, and seeing it.** The port
now has `game_mode`, 0-based like the original, produced from the menu's
1..3 `selected_mode` by the single conversion
`game_mode_from_selection`. `BATTY_GAME_MODE` sets it directly for
gates, which reach gameplay through `BATTY_START_LEVEL` and never touch
the menu — without that knob the mode is unreachable from a test.
`PROBE.TXT` reports `game_mode=<n>_player<n>`.

It also closes a TODO that had been sitting in the round banner: the
"PLAYER 1" digit was a hardcoded `$01` with a comment saying to swap it
in once the 2-player wiring landed. It reads `active_player + 1` now,
which is the original's `LD A,(player_number) / INC A`, and renders
identically while nothing moves the turn off 0.

**Stage 4 done (2026-08-09): the hand-over.** A life loss in mode 1 ends
the turn. `two_player_turn_change` saves the live 180-cell grid into
`player_grid[active]`, toggles `active_player`, and marks the arriving
player's grid for restore; `enter_level` puts it back over the pristine
copy `reset_level_state` has just loaded, so destroyed bricks stay
destroyed. `run_level` unwinds the frame loop to the level-entry point —
the arriving player may be on a different level — and does NOT advance
the round, since only a cleared level does that.

`player_grid_valid` is port bookkeeping the original does not need: its
level table always holds a playable grid, whereas `player_grid[1]` is
zeroed until player 2 has had a turn and restoring zeros would read as
"every brick destroyed".

Gated by `test-two-player-turn`, an A/B on `BATTY_GAME_MODE` with a
third case (`BATTY_REPLAY_LIVES_2UP=0`) for the guard that lets a solo
player keep playing.

**Stage 5 (2026-08-09): `LBC10_7` ported and GATED.** When one player
runs out of lives the port now shows their GAME OVER screen and then
hands over to the other player if they still have lives, instead of
returning to the title and ending a game the other player is still in.
It reuses `two_player_turn_change`, whose guards already cover both of
LBC10_7's conditions — asking them again at the call site is the
duplicate-guard mistake stage 4 ran into.

Gating it took three attempts, and the two failures are the useful part.

`active_player` cannot carry it: `PROBE.TXT` is rewritten at every level
entry and these scenarios die repeatedly, so a probe read at any moment
reports whoever entered LAST. Hence `turn_changes_life` /
`turn_changes_over`, counters that accumulate and survive every later
write.

Counters alone were not enough either. `play_game_over` holds for 178
PIT frames (~3.5 s) and the capture window ended INSIDE that hold, so
the file still held the level-ENTRY write from before the death — even
`go`, incremented on the line before the hold, read 0. Hence
`BATTY_FAST_HOLDS=1`, which cuts the wait to 2 frames. Separate from
`BATTY_HOLD_GAME_OVER`, which makes the hold wait for a KEY: that is for
visual gates that want the screen to STAY up, the opposite need.

With both in place the path is observed — `over=1`, turn on player 2 —
and disabling the hand-over is caught, as is ignoring the fast-holds
knob. **WS2's flow is complete**, and as of 2026-08-09 so is the GAME OVER
screen's "PLAYER n" line — `active_player + 1`, the original's
`LD A,(player_number) / INC A` patched into `txt_player_0+$0C`. Before
that the screen did not say whose game had ended, which is harmless with
one player and wrong with two.

The screen's LAYOUT remains the port's own: the original prints exactly
those two lines at ($60,$4F) and ($60,$67) and no score lines, while the
port adds SCORE and HIGH and stacks four lines 12 px apart. Recorded as
a deliberate divergence in notes/parity-gaps.md rather than left
implicit.

What is left of WS2 is nothing; Double Play's court split is WS3.

**What:** Classic turn-taking (research-confirmed: on the Spectrum,
2 PLAYERS alternates turns, unlike the C64 version). Per-player state
swap on life loss — score, lives, round, brick field — plus 1UP/2UP HUD
labels, the round-banner player digit (hardcoded `$01` today,
`src/main.cpp` ~L7302), and per-player hi-score entry. The object model
already carries `object_bat_2` ($9B3E) and the original's per-player
data structures are in the disasm (`preparation`/`record_table`
routines, `txt_player_N` strings).

**Approach decision:** follow the disasm's swap mechanism rather than
inventing a "two game structs" C-side abstraction — the original keeps
one live playfield and banks the waiting player's state; mirroring that
keeps parity reasoning (and any future oracle capture) tractable.

**Exit:** full 2P game playable; a transition gate proves the swap
(P1 dies with distinct score/round → P2's banked state restores
exactly); 1P gates untouched.

## WS3 — Double Play mode (simultaneous co-op)

**Stage 1 done (2026-08-09): the court divider.** `LBE8B_10` draws
`object_separator` when `game_mode == $02`, immediately before the
1UP/HI/2UP sprites. Ported: `assets/separator.bin` is a new 98-byte
extraction (`spr_separator` at `$7A2A` — it sits just BELOW
`sprites.bin`'s `$7A8C..$8F50` range, its body ending exactly where that
blob begins, so widening the range would have shifted every existing
offset), and `render_separator` blits it at the coordinates
`object_separator` carries: x=`$7D`, y=`$A9`.

Worth knowing before building the rest of the mode: the divider is
**not a full-height wall**. It is 16 px wide and 24 rows tall at y=169,
down in the bat band — a marker between the two bats' halves, not a
barrier the ball interacts with. The per-bat margin clamps below are
what actually confine the bats.

**Stage 2 done: the court layout.** `all_var_init` (LB7F8) does not add
a bat beside the existing one — it MOVES both:

    LD A,(game_mode) / CP $02 / JR NZ,LB7F8_1
    LD A,$01 / LD (object_bat_2),A        ; sprite_set: activate
    LD A,$38 / LD (object_bat_1+$02),A    ; bat 1 x = 56
    LD A,$B0 / LD (object_bat_2+$02),A    ; bat 2 x = 176

Ported in `reset_level_state`, with `render_bat_2` drawing the second
one. It uses the PLAIN sprite: `bat.extra_px`, the gun frames and the
resize sides are all bat-1 state, and bonus ownership for bat 2
(`object_bat_2+$14`) is a later item — drawing it big or armed before
that state exists would be inventing behaviour.

`render_bat_2` is called from the STATIC background compose, not the
per-frame path. Bat 1 is a moving object and the dirty path redraws it
every frame; bat 2 has no input yet, so for now it is scenery. When it
gets a device it moves to the per-frame path and that call goes with it.

Gated by `test-double-play-court`, an A/B on `BATTY_GAME_MODE` frozen at
a visual checkpoint: 610 pixels differ, none above the bat band, and
each of the three regions — bat 1 at $38, the divider at $7D, bat 2 at
$B0 — must contain some. That last check is not decoration: the first
run of this gate passed with bat 2 missing entirely, because the call
sat under `with_bat`, which the static compose does not use.

The frozen frame matters too. The first version slept 9 s and
screendumped, which worked while mode 2 changed only the divider. The
moment bat 1 moved to x=56 the runs stopped being comparable at all —
the ball auto-launches off a bat in a different place, so by 9 s they
have destroyed different bricks and the diff is the whole screen.

**What:** The distinctive mode: both bats on screen at once, a divider,
one shared ball, shared pool of 3 lives. Needs: bat_2 render + input
(second device from `p2_dev`), `handling_ball`'s bat-2 deflection
branch, side-attributed scoring, bonus ownership (`object_bat_2+$14`
paths already partially maintained in the port), and the `LBC10_4`
death-spark branch (shift 5 sparks by `bat_2.x - bat_1.x`) that
`parity-status.md` explicitly parked as "out of scope (port is 1P)".

**Two things this section used to say, both wrong** (traced 2026-08-09,
before anything was built on them — see notes/double-play.md):

- *"each bat confined to its half"* and *"per-bat margin clamps at the
  divider"*. There are no such clamps. `handling_bat_no_transform` calls
  `check_left_margin` and `check_right_margin`, the same two the alien
  uses, over the full playfield; both bats run through the same
  `handling_bat` and differ only in which object and control word. Both
  can cross the middle and occupy the same half. Adding confinement
  would have been an invented mechanic — the same mistake as the enemy's
  reflect-and-re-aim, which sat in the port for months.
- The halves are a SCORING rule, which this section missed entirely.
  `handling_bat` (and four other sites) records `need_change_player` from
  the object's `x AND $80`, and `add_points_to_score` swaps the two
  score blocks around `score_update` when it is set. So in Double Play
  **points are credited by WHERE the event happened, not by whose bat
  did it**: a brick broken on the right half scores for player 2 even if
  player 1 sent the ball there.

**Stage 3 done (2026-08-09): the scoring owner.** `add_points_to_score`
is the single place a score is added — gated, because a second `+=` site
would bypass the side rule. Three of the five flag sites are ported (the
alien kill and both bonus paths take the BAT's x; the bullet takes its
own). Two are not, and pass an explicit `SIDE_ACTIVE`:

- the end-of-round leftover bricks are split EVENLY by alternating the
  flag — ported 2026-08-09, see stage 5.

**Stage 4 done (2026-08-09): the ball's owner bit.** `+$12` is a counter
in bits 0..6 with bit 7 a separate flag that every counter operation
preserves on purpose, so nothing in flight changes it. It is set once at
`all_var_init` from which side the ball STARTS on, and the start side
alternates every entry (`XOR $88` flips the self-modified `$48` <-> `$C0`).

So brick points go to whoever the BALL belongs to. **Corrected again
2026-08-09:** the owner is NOT fixed for the ball's life —
`LAB1F_0` does `RES 7,(IX+$12) / BIT 7,(IY+$02) / SET 7,(IX+$12)` on
every bat deflection, from the x of the bat that hit it. The first read
missed it because those are bit ops, not `LD`, and the grep looked for
assignments. So brick points follow whoever last HIT the ball. Ported;
see notes/double-play.md.

Not ported: the mode-2 start X itself. The alternation drives the owner
either way, and moving the ball off the bat at Double Play entry is a
visible change that belongs with bat 2's input.

**Stage 5 done (2026-08-09): the leftover-brick split.** The fifth flag
site attributes nothing by side — `add_points_for_left_briks` zeroes the
flag and XORs it after every award, so the surviving bricks alternate
1UP, 2UP, 1UP... wherever they sit ("Добавляет двум игрокам поровну
очки"). With it ported, all five sites pass a real side and the
`SIDE_ACTIVE` sentinel is gone; the gate asserts it stays gone.

**Stage 6 done (2026-08-09): the death-spark split.** `LBC10_4`
translates the ODD-indexed half of the ten seeded sparks by
`bat_2.x - bat_1.x`, so each bat explodes with five. It is a split, not
a second spawn. `parity-status.md` had it parked as "out of scope (port
is 1P)". Gated inside `test-death-sparks`.

**Stage 7 done (2026-08-09): bat 2 deflects the ball.** `LAB1F` tries
bat 1, then bat 2 in mode $02 only, and `LAB1F_0` re-owns the ball to
whichever bat hit it. Gated by `test-double-play-bat2`, an A/B on
`BATTY_GAME_MODE` with the ball seeded straight at bat 2.

**WS3's scoring, effects and ball physics are complete.** What is left
is bat 2's INPUT (`p2_dev` selects nothing — see WS1) and bonus
OWNERSHIP.

The ownership gap is not what an earlier note in `src/main.cpp` claimed.
Bat 2's bonus byte is maintained — `set_bat_bonus` writes BOTH bats —
and that is precisely the divergence: the original applies a bonus to
the bat that CAUGHT it (`DEC (IY+$14)`, IY being the catching bat, with
the bat-2 branch wrapped in `bonus_flag_swap`), so it can arm one bat
and leave the other bare. The port arms both.

Splitting it is entangled with two things that are not small: the CATCH
bonus needs the stuck-ball system, which is written around the primary
ball and bat 1 (WS6 item 2 scopes that at ~32 sites), and the width
bonuses are bat-1 globals with nowhere to put a second bat's state. See
notes/double-play.md.

**Why after WS2:** WS2 builds the per-player state plumbing (HUD,
banner, hi-score) that Double Play reuses; Double Play then adds the
simultaneous-play mechanics on top.

**Exit:** co-op playable with two input devices; invariant gates
(bat confinement to halves, shared-lives accounting) + the spark-shift
branch gated; ideally one oracle capture of a Double Play frame for a
static checkpoint.

## WS4 — Game-flow transition gates

**What:** The top-listed test gap (`parity-gaps.md`): level-clear →
next-level, life-loss → respawn, game-over → initials entry, and the
level wrap after L15 (`round_number` keeps incrementing, `% 15` selects
the layout — matches `increment_round_number` $BBE0; gate it so it
stays true) are not end-to-end verified. The deterministic serial
harness (`BATTY_SERIAL_PROBE` + `WAITSERIAL`) and the replay/bake hooks
make these gateable without ZEsarUX.

**Why now:** WS1–3 all mutate exactly these flows (game start, player
swap on death, mode-specific game-over). Landing the transition gates
first-or-alongside means the mode work is born protected instead of
retro-gated.

**Exit:** a `test-flow-*` family in `parity-check-full` covering the
four transitions (plus mode-specific variants as WS2/WS3 land).

## WS5 — Sound: faithful beeper-queue port

**Two corrections to this section's premise, traced 2026-08-09 before
starting the work.**

*It is a FIVE-slot queue, not four.* `sounds_queue` ($C0B8) is 5 rows of
7 bytes. `play_sounds_queue` walks all five every frame (`LD B,$05`)
while `get_free_sound_slot` only ALLOCATES from the first four
(`LD B,$04`); the fifth is written directly by `LAFFC_37`. The port
already has `SOUND_SLOTS = 5`, so this was a doc error rather than a
port one.

*The id is a table POSITION, not a label.* `play_selected_sound` does
`LD HL,play_sounds_list-2 / ADD A,A / ... / JP (HL)`, so an id indexes
the routine table directly — a wrong one plays a different effect, and
one past the end jumps into whatever follows. `test-sound-ids` now
checks each `SND_*` against its namesake's position.

**And the remaining gap is one thing, not thirteen.** `src/sound.cpp`
already carries the original's `(D, E)` pairs verbatim with citations,
and converts E to a PIT divisor honestly
(`period = 8.86 * E -> 9 * E`). What it drops is D:

    void sound_beep_cont_d(unsigned char d, unsigned char e) {
        (void)d;
        sound_beep_e(e);
    }

`sound_beep_e` starts the tone for one tick, and a tick is a 50 Hz PIT
frame — so every effect lasts **20 ms** against the original's 3-9 ms
(`normall_brik` 4.04, `bat_beat` 3.03, `metal_brik` 8.56). The pitches
are right; the durations are all ~5x too long, from one cause.

**Half of that is now fixed.** `sound_beep_cont_d` computes the real
duration — `D * 2 * E * 13` T-states through `sound_set_clock_hz` — and
a host test drives the module at a microsecond clock to pin the three
measured envelopes. What still rounds them to 20 ms is the clock rate,
not the model, and the same test asserts that floor so it cannot be
believed fixed while it is not.

**What remains is a DESIGN question, and I had it wrong.** "Give it a
finer clock" was my own framing two commits ago; it does not survive
reading the primitive. The original's beeper BLOCKS — `sound_beep` is a
pair of DJNZ spin loops around `OUT ($FE),A`, so the CPU makes the wave
and `play_sounds_queue` consumes 3-9 ms of real frame time. The callers
know it:

    CALL play_sounds_queue
    JR NZ,LBAED_4

`play_sounds_queue` latches the frame counter on entry, compares on
exit, and returns Z only if the queue fitted inside one interrupt; the
main loop branches on that and skips ahead to the running-dot draw when
it did not.

A PC does not need any of that — the PIT holds a tone with no CPU, which
is why the port latches a divisor and returns. So the choice is between
blocking like the original (faithful to the millisecond, hands back the
frame-pacing behaviour, costs 3-9 ms of any frame that makes a noise)
and keeping the latch (needs the stop scheduled from an interrupt, and
the port's timer is the same 50 Hz).

That is a decision about how much of the original's frame timing to
import, not a defect to fix — so it wants a deliberate call. Decode,
verified examples and both options in notes/sound.md.

`SND_MAGNET` is the exception and stays outside the table: the original
never queues it. `magnets.asm` ends its draw with a plain
`CALL play_sound_magnet`, and the table's `$0D` slot is commented out as
"Неиспользуемый звук". `src/sound.h` claimed the ids "match the
original's play_sounds_list" with no exception noted; it says so now,
and the gate pins `$0D` past the end.

**What:** Close the one openly approximate subsystem. The original has
**no music** — a 4-slot, 7-bytes-per-slot effect queue
(`routines/sound.asm`), frame-synced under IM 1, driving the 1-bit
beeper. The port currently maps the 13 effect IDs onto PIT-channel-2
tones with approximated envelopes (and `SND_MAGNET` as a blocking
sweep). Both machines are 1-bit toggles (Spectrum port $FE bit 4 vs PC
port 0x61 bit 1), so the faithful path is **direct-gate toggling with
PIT-calibrated delay loops**, porting each `play_sound_<id>` envelope's
period/step structure rather than re-synthesizing it. Prior art says
this is enough: the Warajevo DOS emulator reproduced Spectrum beeper
audio (incl. polyphony) on PC speaker; Batty's queued mono effects are
far simpler.

**Decision (recommended):** target *envelope-faithful*, not
cycle-exact — port the per-slot frequency walks and durations,
timed by PIT so it's CPU-speed independent; accept timbre differences
from bus/ISR jitter. Cycle-exact beeper emulation stays a non-goal
(below).

**Verification:** audio can't be pixel-diffed. Add a `BATTY_SOUND_LOG`
hook that records gate-toggle (period, duration) sequences per effect
and pin them source-level against tables derived from the disasm;
final judgement by ear vs ZEsarUX (`make run-original`).

**Exit:** all 13 effects (incl. a non-blocking magnet sweep) ported
from their `$C0F3+` handlers; toggle-log gates green; the
parity-gaps "sound envelopes are approximate" section rewritten to
"faithful, not cycle-exact".

## WS6 — Remaining gameplay-parity residuals

In priority order (all pre-scoped in `parity-gaps.md` / notes):

1. **Enemy vs bricks:** the original's bird runs `LAFFC` brick
   collision and exact `check_margins`; the port's bird doesn't
   (`enemy-movement.md` marks this the "next step"). Port both; gate
   with a seeded flight into a brick.
   *Brick half DONE (2026-08-09):* traced, ported and gated. The walk
   is `enemy_home_step` (LAA44, host-tested); the detection is
   `enemy_brick_reaction`, which reuses `laffc_sweep`/`laffc_bounce` and
   does the alien's half of `LAFFC_30` — keep the reflected dir, latch
   the snap point, leave the alien put, re-target off `flag_2` — without
   ever calling `brick_hit_resolve`. Gated by `test-enemy-brick-walk`,
   which needed a new `enemy_home` probe word because the reaction
   leaves no trace on screen.
   *`check_margins` DONE too (2026-08-09):* it is three clamps and
   nothing else — the port's reflect-and-re-aim was an invention, since
   `LAA7D` never looks at the alien's position. Gated by
   `test-enemy-margin-clamp`. **This item is closed**; the only residue
   is the original's 8-bit overflow in `check_right_margin`, reproduced
   and documented rather than fixed. See notes/enemy-movement.md.
2. **MAGNET catch for secondary balls:** catching the unified
   secondaries means making the stuck state per-ball. Deliberately
   deferred as substantial new code for the niche MAGNET+TRIPLE case —
   schedule it, don't ignore it.

   *Sized 2026-08-09.* "~32 sites" is **24**, across 15 functions, and
   ten of those are primary-by-construction — an extra ball spawns in
   flight and is never stuck, so `reset_level_state`,
   `respawn_primary_ball` and the replay overrides need no index at all.
   **14 sites in 8 functions** are the actual refactor: the catch, the
   ride/auto-launch, the FIRE release, the two step early-outs, and the
   held ball following the bat. Breakdown in notes/bat-deflection.md.

   It is the same refactor bat 2's catch needs (WS3): `catch_ball_on_bat`
   reads `BAT_X` directly, so the index has to name a BAT as well as a
   ball. Doing it once serves both, which makes it better value than its
   "niche" framing suggests.
3. ~~**Seeded destroyed-cell mismatch**~~ — **CLOSED 2026-08-09, and it
   had already fixed itself.** This entry said the port and original
   destroy different *cells* on `replay-l3-brick-flash-both`. They do
   not: `current_level_copy` — the whole 180-cell grid — comes back
   byte-identical. It was sitting in the INFO tier because nobody
   re-ran the comparison after the LAFFC work landed, so the row that
   would have said so was never promoted.

   Promoted to `required_probe_rows`. Verified it bites: mutating the
   destroyed marker from `$80` to `$C0` fails the gate.

   Two rows stay INFO deliberately, with the reason recorded in the
   replay's `note` rather than left implicit:
   - `briks_data` — the port does not maintain the original's five
     metal-shimmer slots; it tracks the same animation in
     `brick_hit_anim_ticks`.
   - `random_number` and the object rows — by this frame the two
     runners are at different points in their RNG walks.
4. **Byte-exact enemy target gating:** blocked on a boot-phase-
   normalized comparison harness (test-infra, not port code). Do it if
   and when WS8's harness work makes it cheap.
   *Partly sidestepped (2026-08-09):* `test-enemy-descend` needed the
   phase and did not get the harness — it reads `enemy_repicks` out of
   the probe and asserts the implication (`turns == 0 -> target $10`,
   `turns == 1 -> target $29`) instead. Where a capture already says
   which outcome happened, no phase normalisation is needed.
   known-bugs #17.

**Exit:** items 1–3 gated; parity-gaps.md's "behavioral" section
reduced to the accepted-residuals list below.

## WS7 — Asset self-sufficiency (de-capture)

**What:** Replace the remaining captured-from-emulator blobs with
runtime generation from tape-derived data, so the tape is the *only*
original artifact:

1. **Frame ornament** — port the `spr_bord_horiz_*`/`spr_bord_left/
   right_*` compositor (disasm ~L6940) and drop `frame_l1.bin`
   (also kills the manual L6853 re-extract procedure in
   `modded-batty.md`).
   *Top arm proven 2026-08-09.* `test-frame-derivable` lays out the
   eight sprites `set_border_horizontal` names and matches
   `frame_l1.bin` exactly — 1024 of its 4968 bytes, all four cycles.

   **The detail that makes this work is `print_sprite_pix`.** It is a
   plain UNMASKED copy (`LD A,(DE) / LD (HL),A`, no OR, no mask) and each
   row moves to the PREVIOUS buffer line, so a sprite at `y=$07` occupies
   y 0..7 with its rows REVERSED. Laid out top-down the same data matches
   58 bytes of 256, which reads as "not derivable" rather than "drawn the
   other way up".

   The sprites live at `$6B17..` — below every current extraction
   (`sprites.bin` starts at `$7A8C`), so porting this needs a new asset
   like `separator.bin` was.

   *Side arms proven too.* `LBE8B_1` holds two register sets and swaps
   them every iteration, so it alternates `spr_bord_left_bold` (32 rows
   from y=`$BF`) and `spr_bord_left_thin` (24 rows from y=`$9F`), each
   stepping up 56. The RIGHT-hand variants are never named:
   `print_sprite_pix` walks DE through the sprite it drew and
   `print_sprite_attrib` walks it further, so the second call lands on
   the next block in memory — which is why the disassembly says of each
   pair "следующие два спрайта должны идти строго друг за другом".

   *Attribute rows too.* Each sprite carries its own attr block after
   the pixels — `(aw, ah)` then `aw*ah` bytes — and those stack UPWARD
   as well (downward matches 48 of 168).

   Together: **2664 of `frame_l1.bin`'s 4968 bytes**, all four cycles,
   held by `test-frame-derivable`.

   **And the rest of that blob is not frame work.** `extract_frame.py`'s
   own comment says it: of its 24 top pixel rows, y 0..7 are the
   ornament and y 8..23 are the HUD's labels and score digits, which the
   port draws itself in `render_hud_to_buff`. Same for top attr rows 1
   and 2 — they match these sprites 26 and 13 times out of 128, i.e. not
   at all.

   So **everything in `frame_l1.bin` is either ornament that derives
   from the tape or HUD the port already generates.** Retiring it needs
   the `LBE8B` port, not more data — and the sprites at `$6B17..` need a
   new asset extraction the way `separator.bin` did.

   *And the attrs the port actually uses.* `paint_frame_to_buff` takes
   its PIXELS from `frame_l1.bin` but its ATTRS from `level_attrs.bin` —
   all three `paint_strip_to_buff` calls pass `lattr`. The frame blob's
   own attr sections were loaded and never read: 138 bytes per cycle,
   552 in all. **Removed 2026-08-09** — `extract_frame.py` no longer
   emits them, `FRAME_SIZE` no longer skips past them, and the blob is
   4416 B instead of 4968. The full pixel suite is the proof nothing
   changed on screen.

   The cells that DO matter — `level_attrs.bin`'s char row 0 across the
   top, and columns 0 and 31 down char rows 3..23 — come from the same
   sprites: 1110 bytes over 15 levels, verified. That takes
   `level_attrs.bin` to 6510 of 11520 derived, 56.5%.

   *The HUD rows are ornament too.* `frame_l1.bin`'s top block is 24
   rows, and rows 8..23 are not just background: byte columns 0 and 31
   carry the side ornament down to y=8. That is `LBE8B_1`'s SEVENTH
   placement — bold from y=`$17` — which this list previously wrote off
   as "runs off the top and is covered by the top border". It is not
   covered. Measured the hard way: blitting only rows 0..7 and letting
   the background show through moved 345 pixels in `test-visual`'s
   `state4_level1`, which is pixel-identical otherwise.

   *And the addon strip.* `LBE8B`'s last pixel pass ANDs a 30-byte
   `border_horizontal_addon` into `scr_buff+$101` — row 8, bytes 1..30,
   a one-pixel inner outline. AND, not copy, so the background shows
   through. Verified against `bg_tile.bin`.

   *Closed 2026-08-09: every byte of `frame_l1.bin` now derives.* The
   last residue was `LBE8B_2`'s inner-outline pass. It runs **four**
   bands of 28 rows, 56 apart; the port's `inner_border_line_c` has only
   the lower three (y0 = 50, 106, 162) because the first one's effect is
   already baked into the blob. It starts 56 rows above 50 — y=-6 — so
   it clears bit 7 of byte 1 and bit 0 of byte 30 across rows 0..21.

   And the reason piecewise checking could not find it: **ordering**.
   That band runs BEFORE the top border, which then overwrites rows
   0..7 of what it did. A generator gets that for free; a predicate over
   positions cannot. Same lesson as the brick zone's empty cells, in the
   same week.

   `test-frame-derivable` now GENERATES the 24-row top block —
   background, seventh side placement, inner-outline band, eight top
   sprites, addon strip, in LBE8B's order — and compares it whole:
   3072 + 1344 side + 1110 `level_attrs` cells = 5526 bytes.

   **DONE 2026-08-09.** `assets/border.bin` is a new 382-byte extraction
   (`$6B3F..$6CBC` — the bold/thin side pair with their right-hand twins,
   and the six distinct horizontal pieces), and
   `build_frame_from_sprites` runs LBE8B's pixel passes at startup to
   fill `frame_l1[]`. `FRAMEL1.BIN` is off the floppy; the captured
   `assets/frame_l1.bin` stays as `test-frame-derivable`'s reference,
   the same role `main_menu.bin` and `hi_score.bin` have.

   All five `test-visual` states are pixel-identical with the frame
   built rather than loaded.

   That promotion is part of this item: `state4_level1` had been
   INFO-only for a residual that is long gone, so mutating the new
   generator — truncating the inner band, moving a column — left the
   whole suite green. It asserts now, and all three mutations are
   caught. An INFO row is a measurement, not a gate.
2. **`level_attrs.bin` residue** — brick-body attrs are already
   computed; port the writer for frame-strip columns and pre-dimmed
   shadow attrs. This is also the root of the accepted 4px frame-step
   residual, which may fall out for free.
   *Measured 2026-08-09, before porting anything else:* the live-brick
   cells — 2412 bytes, 20.9% of the blob — are reproduced EXACTLY by
   `briks_colors` + `print_border_shadow`'s left arm, and
   `test-level-attrs-derivable` now holds that. The rest is empty
   brick-zone cells (26.0%) and the HUD/side/bottom rows (53.1%), so the
   blob cannot go yet.

   *Second pass:* the empty cells take exactly two values per colour
   cycle — `bg_attr_per_cycle[]` and the same with bit 6 cleared — so
   the only question is which are dimmed. A neighbour predicate reaches
   94.4% and stops being derivation somewhere around the second term;
   abandoned deliberately. The value depends on the ORDER of
   `print_briks` / `brik_shadow` / `print_border_shadow`, not on a local
   rule.

   *Settled 2026-08-09 by simulation.* `tests/test_bricks.cpp`'s
   `attrs_generate` fills the band with `bg_attr_per_cycle[]`, runs
   `paint_bricks`, applies `print_border_shadow`'s left arm, and
   compares char rows 4..15, cols 1..30 against the blob: **all 5400
   cells match, all 15 levels.** The whole brick zone — 46.9% — is
   generated from `assets/levels.bin` plus the tape's `briks_colors`,
   with no reference to the capture.

   It had to be `paint_bricks`, not `paint_brick_band`: the latter
   re-bases from `level_attrs.bin` first, so comparing its output
   against the blob compares the blob with itself.

   *Fully accounted for, 2026-08-09.* Every one of the 768 cells per
   level is now derived and gated:

   | cells | region | gate |
   |---|---|---|
   | 630 | char rows 3..23, cols 1..30 | `attrs_generate` (host) |
   |  32 | char row 0, all cols | `test-frame-derivable` |
   |  42 | char rows 3..23, cols 0/31 | `test-frame-derivable` |
   |   4 | char rows 1..2, cols 0/31 | `test-frame-derivable` |
   |  60 | char rows 1..2, cols 1..30 | `test-level-attrs-derivable` |

   The last two fell out of the frame work: rows 1..2's frame columns
   come from `LBE8B_1`'s SEVENTH placement (the same one that fills the
   HUD band's side pixels), and rows 1..2's interior is plain `bg_attr`
   with `print_border_shadow`'s two arms. The 1UP/HI/2UP labels and the
   score digits are PIXELS — they leave the attribute cells alone, which
   is the colour-clash rule this port keeps meeting.

   **DONE 2026-08-09.** `build_level_attrs_from_data` runs the
   attribute passes in `game_screen_draw_to_buffer`'s order — bg_attr,
   the frame sprites' own attr blocks (row 0 from the eight horizontal
   pieces, columns 0/31 from all seven side placements), `paint_bricks`
   with `paint_shadow_row` interleaved, then `print_border_shadow` last
   — and fills `level_attrs[]` at startup. `LVLATTR.BIN` is off the
   floppy. All 15 levels are pixel-identical
   (`test-levels-sweep` runs `test-visual`'s now-asserting
   `state4_level1` per level).

   The brick pass turned out to be redundant and was removed after
   measuring: bg + frame attrs alone FAILS (L01 off by 1696 px), adding
   the border shadow makes all 15 levels pixel-identical, and adding
   `paint_bricks` on top changes nothing — the port repaints live bricks
   at every level entry. So the generated band is the EMPTY playfield's
   attributes, and `reset_destroyed_cell_attrs`' reset half is now a
   no-op (its shadow half still earns its keep; `bricks.h` says so).

   The array itself stays: `paint_frame_to_buff` reads char rows 0..2 in
   full and rows 3..23's columns 0/31, and `paint_brick_band` re-bases
   rows 3..16 so the `$C0` sentinel cells keep their background. What has
   gone is any dependence on a capture.
3. ~~**Menu / hi-score screens**~~ — **already done, and the entry was
   wrong** (checked 2026-08-09). `render_menu_screen` and
   `render_hiscore_screen` build both screens from `MENUMARK.BIN` /
   `MARKUP.BIN` markup plus the font and sprites; nothing in `src/`
   loads `MAINMENU.BIN` or `HISCORE.BIN` at all. The two 48 KB captures
   are `test-visual`'s references and nothing more — but they were still
   being mcopied onto every floppy image, unread, which is what made
   this entry look true. Removed from both floppy recipes;
   `test-floppy-assets` now holds the line in both directions (a missing
   asset breaks the boot, a dead one is evidence of exactly this kind of
   stale belief).

   Only `LOADING.BIN`, the title screen, is still a captured screen the
   game displays.

**Unlock:** deterministic builds from `batty.tap` alone, per-level
frame variations the captured blob can't cover, and a much cleaner
distribution posture (WS9): the repo could stop shipping any
emulator-derived original imagery.

**Exit:** `assets/` contains only tape-extracted data; all visual
gates stay pixel-identical (they are the proof the generation is
right).

**EXIT MET 2026-08-09**, and `test-asset-provenance` holds it: every one
of the 13 assets the port loads is built from `original/blocks/*.dat.bin`.

The last three took repointing, and none of them needed a capture in the
first place:

- `loading.bin` came from `original/Batty.scr`, which is byte-identical
  to tape block 02 — the tape's own 6912-byte SCREEN$.
- `main_menu_markup.bin` was cut out of a snapshot's RAM dump at `$954D`,
  an address inside block 03.
- `markup.bin` had NO build rule at all: 273 bytes checked in with its
  provenance recorded nowhere. It is at `$8FD1`, found by searching the
  block for its contents.

All three rebuilt byte-identical from the tape.

`bg_tile.bin` was the hold-out before them and
was never captured out of necessity: the four textures are tape data at
`$C015`/`$8EE8`/`$8F10`/`$8F38`, and switching the extractor to them
produced a byte-identical file. Their attribute bytes are
`bg_attr_per_cycle[]`, which `test-bg-tile-derivable` now ties to its
source — it was a hand-written copy of four tape bytes with nothing
checking it.

`assets/frame_l1.bin`, `level_attrs.bin`, `main_menu.bin` and
`hi_score.bin` remain as gate REFERENCES; none is loaded or shipped.

## WS8 — Infrastructure: CI, real hardware, load time

1. **Re-test KVM on hosted CI.** The "hosted runners have no KVM"
   conclusion predates GitHub enabling nested virtualization
   (`/dev/kvm`) on Linux runners (2024). If `qemu-system-i386 -accel
   kvm` works there, restore at least the QEMU smoke (the deterministic
   serial harness already removed the wall-clock flakiness). If not:
   self-hosted runner, or accept local-only (documented dead end for
   TCG stands — don't retry TCG).
2. **Real hardware smoke.** `make floppy` produces a bootable 1.44 MB
   image; one verified boot each on a real XT-class (8086 build) and a
   386 (`batty386.exe`) would retire criterion 7. Follow-ups already
   scoped in `performance.md`: batch the small asset `fread`s (floppy
   load time — the one untouched perf lever) and optionally a single
   auto-dispatching binary (runtime 386 detect) instead of two EXEs.
3. ~~**Boot-phase-normalized harness**~~ — for the PORT side this was
   already built and I had not noticed: `BATTY_REPLAY_COUNTER` pins
   `pit_frame_counter` at the aligned start (`pin_replay_frame_counter`,
   called from `enter_level`). Set it and every counter-phase decision is
   deterministic. Now used by the three enemy gates. What remains for a
   byte-exact comparison against the ORIGINAL is aligning the port's pin
   with the original's `counter_misc` at the same moment — that part is
   still open. known-bugs #17.

## WS9 — Polish, history, distribution

1. **Kinnock easter egg: DONE (2026-08-09).** Behind `BATTY_KINNOCK=1`,
   off by default; gated by `test-kinnock`, which parses the expected
   text out of `txt/txt_kinnock.asm` instead of copying it. Two
   surprises worth recording: it fires at the start of every LEVEL, not
   once per game (`print_kinnock` is called from the per-level entry
   `LB9E8_1`), and it is up for only ~0.3 s (`pause_short` with D=0 is
   ~1.05M T-states). See notes/shortcuts.md.
2. **Docs hygiene: pass done 2026-08-09.** Both named items fixed, plus
   three more the pass turned up:
   - `parity-gaps.md`'s priority list led with two struck-through CLOSED
     items, burying the two actually open. Reordered to OPEN first.
   - its "Some motion is approximate" section had six DONE entries under
     that heading, and the enemy bullet said "still approximate" one
     line above the list of four routines the port matches.
   - its closing paragraph had been edited in place three times and read
     "...end-to-end coverage. and the byte-exact frame-step oracle...".
   - `rng-model.md`'s "remaining work" paragraph outlived both its items
     by two months.
   - **known-bugs #16 asserted the opposite of the code** in the present
     tense, naming a routine deleted the same day.
   New tool from it: `scripts/notes_symbols.py` lists identifiers the
   notes cite and the tree no longer defines. A report, not a gate —
   see notes/testing.md for why.
   *Fully triaged 2026-08-09.* 41 candidates, 6 real. The one that
   mattered was not a rename: `rocket-flight.md`'s parity table still
   listed BOTH rocket divergences as `DIVERGENT ✗` — the in-flight brick
   tunnel and the instant end-of-flight award — long after both were
   fixed. `step_rocket` has no cell loop, and `play_rocket_award_tally`
   ticks one brick per PIT frame with the bricks left on screen. A table
   claiming a fixed divergence invites someone to fix it again.
3. **Distribution decision (user call, before any publicity):** Elite
   Systems still actively monetizes Batty (official iOS/macOS app) and
   historically denied archive distribution; the CityAceE disasm repo
   has no license. The repo currently vendors the tape, TZX, and
   captured screens. Before making the repo public or shipping
   binaries, decide between: (a) keep private (status quo, zero risk),
   (b) publish code only, with originals user-supplied at build time
   (WS7 makes this clean — the build already extracts assets from the
   tape), or (c) ask Elite. Until decided: don't distribute built
   images containing original assets.

---

## Suggested sequencing

```
WS1 menu start ──► WS2 2-players ──► WS3 double play
      │                  ▲
      └── WS4 flow gates ┘   (land alongside; gates protect the mode work)

WS5 sound          — independent, any time
WS6 parity residuals — independent, any time (item 1 first)
WS7 de-capture     — independent; prerequisite for WS9.3(b)
WS8 infra          — KVM re-test is a one-hour experiment; do early
WS9 polish         — docs pass early; distribution decision before publicity
```

A reasonable milestone cut:
- **M1 "feature-complete":** WS1 + WS4 + WS2 + WS3 → all three modes,
  transitions gated.
- **M2 "faithful":** WS5 + WS6.1–3 → sound + last behavioural parity.
- **M3 "self-contained & proven":** WS7 + WS8 → tape-only assets, CI
  restored, real-hardware boot.

## Accepted residuals (explicit non-goals — do not reopen)

- **Cycle-exact sound timbre** (Z80-clock-timed duty cycles, ISR
  jitter) — envelope-faithful (WS5) is the target.
- **The 4px L3 frame-step brick-edge nuance** — unless it falls out of
  WS7.2 for free.
- **Metal-shimmer 1-frame phase offset** — root-caused, cosmetic,
  both sides render out of phase.
- **Rocket-tally pacing** — the original's `pause_short` busy-wait is
  Z80-clock-bound; 1 brick/PIT-tick is the port's documented answer.
- **Big-bat resize as a literal bit-gated state machine** — visually
  matched; revisit only if a defect surfaces.
- **QEMU-under-TCG in CI** — calibrated dead end; KVM or nothing.

## Standing constraints (hard-won — see notes/lessons.md before touching)

Every workstream inherits: `make parity-check` green per change,
`parity-check-full` before milestones; disasm is the oracle (never
"fix" it — patch via `build_modded_batty.py` PATCHES); moving objects
blit pixels only (never attrs); no invented input short-circuits in
blocking sequences; pin `BATTY_REPLAY_COUNTER` for anything gated on
frame-counter low bits; ZEsarUX gates stay serial; the capture pipeline
(`notes/modded-batty.md`) is settled — don't redesign it.
