# Double Play, traced

`game_mode == $02`. PLAN.md's WS3 described it as "court split into
halves with a divider, each bat confined to its half", and named
"per-bat margin clamps at the divider" as one of the things to port.

**On 2026-08-09 this file said both halves of that were wrong. The
first half was: the bats ARE confined, by exactly the per-bat clamps
the plan named.** Corrected 2026-08-10, and see
"How the wrong conclusion was reached" below — the mistake is a
repeatable one, not a slip.

The half that stands: the divider is nothing to the BALL, and the
halves' real job is scoring.

## The bats ARE confined — by LACCE and LACAD, not by handling_bat

`handling_bat` moves the bat by ±4 from the control word and then, in
`handling_bat_no_transform`, clamps it:

    handling_bat_no_transform:
      CALL check_left_margin
      CALL check_right_margin

Those are the same two routines the alien uses (`notes/enemy-movement.md`):
`x < $08 -> x = $08`, and `(u8)(w + x) >= $F9 -> x = $F8 - w`. Full
playfield, no divider term. The resize path calls `check_margins`, which
is those two plus the top clamp — again no divider.

All of that is true, and it is not the whole story. The divider clamps
are a SEPARATE pair of routines, run once per frame after BOTH bats have
been handled:

    LD IX,object_bat_1 / CALL LACCE       ; $ACCE
    LD IX,object_bat_2 / CALL LACAD       ; $ACAD

    LACCE:  A = (IX+$0C) + (IX+$02)       ; width + x, 8-bit
            CP $80 / RET C                ; below the divider, nothing to do
            (IX+$02) = $80 - (IX+$0C)     ; right edge pinned ON $80
            if width == $1C or $2C: SET 0,(IX+$01)

    LACAD:  CP $80 / RET NC               ; at or above it, nothing to do
            (IX+$02) = $80                ; left edge pinned ON $80
            RES 0,(IX+$01)

They are asymmetric, and that is the point: bat 1's RIGHT edge stops at
the divider (the comparison includes the width), bat 2's LEFT edge does
(it does not). Written symmetrically, bat 1 would overlap the separator
by its own 28 px.

The `(IX+$01)` pokes are the 4-px sub-character sprite shift, normally
derived from BIT2 of x by `bat_resize_ready`. A bat whose x has just
been overwritten carries a stale one, so the clamp sets it by hand.

### How the wrong conclusion was reached

The 2026-08-09 trace quoted the caller and stopped here:

    LD IX,object_bat_2 / CALL handling_bat
    POP AF / LD (ctrl_btns_pressed),A

The next four instructions are the two clamps. The quote ended one
instruction early, and the conclusion — "nothing stops either bat
crossing the middle" — was drawn from the truncation rather than from
the code.

This is the same shape as the LAFFC_30 "teleport" and the ball-owner
"never changes in flight": a routine read to its apparent end, a
conclusion published, the missing tail found later. `scripts/disasm.py`
now prints a FALLS THROUGH warning for the first of those. It cannot
help here, because nothing was falling through — the reader simply
stopped scrolling. **Quote to the next RET or the next label, and if a
claim is "X never happens", grep for X before writing it down.**

`grep -n 'CALL LACCE' original/disasm/batty.asm` would have taken
seconds and would have prevented it.

## What the divider is NOT: a wall for the ball

Both bats go through the same `handling_bat`; the only difference is
which object and which control word:

    LD A,(ctrl_btns_pressed) / PUSH AF
    LD A,(ctrl_type_2up)
    CALL get_right_player_ctrl_state
    LD A,(ctrl_btns_pressed) / LD (ctrl_btns_pressed_copy),A
    LD IX,object_bat_2 / CALL handling_bat
    POP AF / LD (ctrl_btns_pressed),A

So player 2's device drives `object_bat_2`, and bat 1's control word is
saved and restored around it — `handling_bat` reads one global, so the
two bats take turns owning it.

`spr_separator` is 16 px wide and 24 rows tall at (125, 169) — down in
the bat band, not spanning the court. **Nothing tests the BALL against
it**, and that part of the original note stands: a ball crosses freely,
which is what makes the mode co-operative rather than two solo games.
The divider is a wall for the bats and a marker for everything else.
See PLAN.md WS3 stage 1.

## What the halves ARE for: scoring

The halves are a SCORING rule, and this is the mechanic the plan
missed entirely.

`handling_bat` records which side it is dealing with, straight off the
x coordinate's top bit:

    LD A,(IX+$02) / AND $80
    LD (need_change_player),A   ; 0 if x < 128

and four other sites do the same for other objects (the bullet, the
bonus, the enemy kill). `add_points_to_score` then reads it:

    add_points_to_score:
      LD A,(game_mode) / CP $02 / JR NZ,score_update
      LD A,(need_change_player) / AND A / JR Z,score_update
      ; Double Play, right-hand side: swap the players, add, swap back
      LD HL,score_1up_in_game / LD DE,score_2up_in_game
      LD B,$0A / CALL hl_swap_de
      LD HL,current_score_1up / LD DE,current_score_2up
      LD B,$03 / CALL hl_swap_de
      CALL score_update
      ... swap both blocks back ...

So in Double Play the score goes to whichever player the flag names.
Outside Double Play the whole block is skipped and everything scores to
the active player.

**Correction (2026-08-09).** An earlier version of this file, and the
commit that introduced it, said "points are credited by WHERE the event
happened... a brick broken on the right half scores for player 2 even if
player 1's bat sent the ball there". That is true for the BAT, BULLET
and BONUS sites, whose flag comes from a live coordinate. It is NOT true
for bricks: `handling_ball` sets the flag from `(IX+$12) & $80`, which
is an OWNER bit, not a position. See below.

The swap-add-swap idiom is the same one `players_swap` uses (see
notes/menu.md): the game always operates on the "1up" block and moves
the data underneath it.

## What this means for the port

- Do NOT add per-bat margin clamps. That would be an invented mechanic,
  the same mistake as the enemy's reflect-and-re-aim, which sat in the
  port for months before `check_margins` turned out to be three clamps
  and nothing else.
- The port's clamps for the bat already are `check_left_margin` /
  `check_right_margin` in effect, so both bats are already free to roam
  once bat 2 gets input. Nothing to do.
- `need_change_player` + `add_points_to_score` is the real work, and
  it is partly done (2026-08-09). See below.


## Ported: the scoring owner (2026-08-09)

`add_points_to_score(pts, side_x)` is now the single place a score is
added, and `check_two_player_state` holds that — a second `+=` site
would bypass the side rule, and two places deciding the same thing means
one of them is untested.

WHICH object's top bit is tested differs by caller, which is the part
worth having written down:

| site | flag from |
|---|---|
| `handling_bat` -> `kill_enemy_by_bat` | the BAT's x, `(IX+$02)` |
| `LA67B_1` -> `get_bonus` | the BAT's x, `(IY+$02)` |
| `handling_bullet` | the BULLET's x, `(IX+$02)` |
| `handling_ball` | the ball's `(IX+$12)` — an OWNER flag, not its x |
| `add_points_for_left_briks` | zero, then alternated to split evenly |

The first three are ported. The last two are not, and their call sites
pass `SIDE_ACTIVE`, which credits the active player exactly as before:

- the end-of-round leftover bricks are split EVENLY between the players
  by alternating the flag ("Добавляет двум игрокам поровну очки") —
  ported 2026-08-09, see below.

## The ball's owner bit (2026-08-09)

The fourth row is ported now, and it is the one that is easy to get
wrong. `+$12` is a COUNTER in bits 0..6 with bit 7 as a separate flag,
and every operation on the counter preserves that bit on purpose:

    LA27E_17:  LD A,(IX+$12) / AND $80 / LD (IX+$12),A     ; zero counter
    LA27E_22:  ... INC A ... AND $7F / CP $7F ...          ; wrap counter
               LD A,(IX+$12) / AND $80 / LD (IX+$12),A

So the COUNTER half never disturbs it.

**Correction (2026-08-09), the second on this bit.** The paragraph that
used to follow said "nothing in flight ever changes it". Wrong. The bit
is written in two places, and the one in play uses `RES`/`SET` rather
than `LD`, which is why a grep for `LD (IX+$12)` and
`LD (object_ball_1+$12)` found only the counter writes and the initial
one:

    LAB1F_0:
      RES 7,(IX+$12)      ; the ball's owner bit
      BIT 7,(IY+$02)      ; the HITTING bat's x
      JR Z,LAB1F_1
      SET 7,(IX+$12)

`IY` is whichever bat `obj_compare` matched, so **the ball changes hands
on every bat deflection**, and brick points follow whoever last hit it.
That is a much more sensible rule than "whoever it spawned toward", and
it should have been the suspicion when the first answer sounded odd.

Search lesson: Z80 bit twiddling is `RES`/`SET`/`BIT`, not `LD`. A grep
for assignments to a byte will miss every one of them.

The other write is the initial one, at `all_var_init`, from which side
the ball STARTS on — and the start side alternates on every entry:

    LD A,(ball_x_coord+$01) / XOR $88 / LD (ball_x_coord+$01),A
    ...
    ball_x_coord: LD A,$48          ; self-modified: $48 <-> $C0
    LD (object_ball_1+$02),A
    CP $C0 / JR NZ,LB7F8_1
    LD A,(object_ball_1+$12) / OR $80 / LD (object_ball_1+$12),A

`$48 XOR $88 = $C0` and back, so the ball starts left, right, left...

**Brick points therefore go to whoever last HIT the ball, wherever the
brick is** — the ball carries its owner, and a deflection transfers it.
Only the bat, bullet and bonus sites read a live coordinate. Ported as
`ball_owner_side`: set from the start side at level entry, reassigned
from `BAT_X`'s top bit at every deflection. `PROBE.TXT` reports it as
the `own` field of `scores=`.

`LAB1F`'s second-bat test is ported too (2026-08-09):

    LD IY,object_bat_1 / CALL obj_compare / JR C,LAB1F_0
    LD A,(game_mode) / CP $02 / RET NZ
    LD IY,object_bat_2 / CALL obj_compare / RET NC

Bat 1 wins an overlap; bat 2 is only tried if bat 1 missed, and only in
mode $02. `ball_lands_on_bat_2` / `deflect_ball_off_bat_2` mirror that,
and the owner comes from bat 2's x on a bat-2 hit.

No catch branch on bat 2: `LAB1F`'s MAGNET test is
`LD A,(IY+$14) / CP $03` on the HITTING bat, and `object_bat_2+$14` is
not maintained by the port. A bat-2 hit is therefore always a plain
deflection — a gap, not a simplification.

### A seed outside a table's domain measures the table

Gating this took two attempts. The first seeded the ball with `dir=$10`,
straight down at bat 2, and the ball STUCK: it snapped to the bat top
every frame and never left. `$10` is not one of the directions
`LAB1F_11` searches for ({$04,$08,$0C,$14,$18,$1C}), so the lookup
returns its input unchanged and the ball re-collides forever.

Real trajectories are diagonal, so the original never presents that
input — but a seeded gate can. `test-double-play-bat2` uses `$08`,
down-right, and gets a proper deflection to `$38`.

Not ported: the mode-2 START X itself ($48 / $C0 instead of resting on
a bat). The alternation drives the owner either way, and moving the
ball off the bat at level entry in Double Play is a visible change that
belongs with bat 2's input rather than with scoring.


## The leftover-brick tally splits evenly, not by side (2026-08-09)

The fifth flag site is the odd one out. `add_points_for_left_briks` is
the rocket-clear tally, and it does not attribute by side at all:

    XOR A / LD (need_change_player),A       ; start on 1UP
    ...for each cell...
      LD A,(IY+$00) / AND $A0 / JR NZ,...   ; skip destroyed/undestructible
      CALL points_calc_and_add
      CALL scr_score_update
      LD A,(need_change_player) / XOR $01 / LD (need_change_player),A

so the surviving bricks alternate 1UP, 2UP, 1UP... regardless of where
they sit. The comment on the routine says as much: "Добавляет двум
игрокам поровну очки за оставшиеся на раунде кирпичи" — adds the points
for the round's remaining bricks equally to both players.

Note `XOR $01`, not `$80`. Everywhere else the flag is the top bit of a
coordinate; here it is bit 0. `add_points_to_score` only tests
`AND A / JR Z`, so any non-zero value works and the two conventions
coexist. Reproducing that literally would mean carrying the flag's
numeric value around; the port passes `0x80` for "2UP" everywhere and
keeps the alternation, which is the same behaviour.

With this ported, every one of the five sites passes a real side and the
`SIDE_ACTIVE` sentinel is gone. `check_two_player_state` asserts it
stays gone: a sentinel with no callers is an invitation to add one.


## The death sparks split across both bats (2026-08-09)

`LBC10` seeds ten object slots as `anim_spark` around bat 1. In Double
Play `LBC10_4` then translates half of them onto bat 2:

    LD A,(object_bat_1+$02) / LD C,A
    LD A,(object_bat_2+$02) / SUB C / LD (LBCE6+$01),A
    ...
    LD A,(game_mode) / CP $02 / JR NZ,LBC10_5
    LD IX,object_ball_2 / LD DE,$0016 / LD B,$05
    LBC10_4:
      LD A,(IX+$02)
    LBCE6:
      ADD A,$00            ; self-modified with the delta above
      LD (IX+$02),A
      ADD IX,DE / ADD IX,DE
      DJNZ LBC10_4

It starts at `object_ball_2` — the SECOND seeded slot — and steps two
objects at a time, five times, so it moves the odd-indexed half. The
delta is `bat_2.x - bat_1.x`. Five sparks stay on bat 1 and five land on
bat 2.

It is a SPLIT, not a second spawn: both bats explode with half a fan
each, not a full one. `parity-status.md` had this parked as "out of
scope (port is 1P)".

Ported in `spawn_death_sparks` as a post-pass over the odd indices,
which is the same set — the port seeds `death_sparks[i]` in the same
order the original seeds its object slots.

The gate for it had to be ANCHORED to `spawn_death_sparks`' body. The
first version checked `if (game_mode == 2) {` against the whole file,
where `reset_level_state` has the identical line, so mutating the
spark one to `>= 1` survived.


## Bonuses are shared, and the original owns them per bat (2026-08-09)

`set_bat_bonus` writes both bats:

    static void set_bat_bonus(unsigned char code) {
        objects[OBJ_BAT_1].bonus_applied = code;
        objects[OBJ_BAT_2].bonus_applied = code;
    }

which is invisible with one bat and wrong with two. The original applies
a bonus to the bat that CAUGHT it:

    LA67B_1:
      LD A,(IY+$02) / AND $80 / LD (need_change_player),A
      ...
      DEC (IY+$14)                 ; IY = the catching bat

and reaches bat 2 by trying it only after bat 1 misses, wrapping the
handling in `bonus_flag_swap` so the shared `bonus_flag` byte belongs to
whichever bat is being handled:

    LD IY,object_bat_1 / CALL obj_compare_2pix / JR C,LA67B_0
    LD A,(game_mode) / CP $02 / RET NZ
    LD IY,object_bat_2 / CALL obj_compare / RET NC
    CALL bonus_flag_swap / CALL LA67B_0 / JP bonus_flag_swap

So in Double Play the original gives LASER to one bat and leaves the
other unarmed; the port arms both.

### Why this is not the next commit

Two things are entangled with it, and neither is small:

- the CATCH bonus needs the stuck-ball system, which is written around
  the primary ball and bat 1 (`catch_ball_on_bat` reads `BAT_X`,
  `ball.stuck_offset_x` is a single value). PLAN.md WS6 item 2 already
  scopes that at ~32 sites.
- the width bonuses are bat-1 globals (`bat.extra_px`,
  `bat.extra_target`, `bat.big_ticks`), so "give it to one bat" has
  nowhere to put the other bat's state.

Splitting those is the work. Recording the divergence is not a
substitute for doing it, but an inaccurate comment claiming bat 2's
byte was unmaintained WAS worse than nothing — it pointed at the wrong
end of the problem.

## Ported: bat 2's input and the court clamps (2026-08-10)

The two halves landed together because neither is worth anything alone.
Input without the clamps lets bat 1 walk through the separator and
across bat 2's court; the clamps without input leave bat 2 parked on
$B0 forever, which is where it had been sitting since stage 1.

### The split keyboard

The original seats two players at one Spectrum by cutting the keyboard
down the middle:

    $FDFE  A S D F G     AND $05 -> A,D = LEFT    AND $0A -> S,F = RIGHT
    $BFFE  Ent L K J H   AND $0A -> J,L = LEFT    AND $05 -> K,Ent = RIGHT

The interleaving is not a transcription error. Each direction gets two
keys straddling the other direction's, so the cluster works whichever
way a player rests their hand.

These are LETTERS, so unlike the device list in WS1 — Kempston,
Sinclair and cursor joysticks, which are Spectrum hardware and have to
be adapted — they transcribe to PC scancodes unchanged. `SC_A`, `SC_S`,
`SC_D`, `SC_F`, `SC_J`, `SC_K`, `SC_L`, `SC_ENTER` in `src/main.cpp`.

**One deliberate addition:** player 1's arrow keys keep working in
Double Play. The original takes them away — mode $02 forces both players
onto the split keyboard — but on a PC the arrows sit next to the numpad,
a long way from HJKL, so leaving them live costs player 2 nothing and
spares player 1 a control scheme that changes with the mode. A superset,
gated as such (`test-double-play-input` has a row for it).

**Not ported:** both original readers bail to the standard per-device
poll unless BOTH players are on `ctrl_type` 0. Nothing selects a device
yet (WS1), so `ctrl_type` is 0 for both by construction and the gate has
nothing to gate. It goes in with the device selection.

### The clamps

`bat_court_clamp_1` / `bat_court_clamp_2` in `src/physics.cpp`, pure and
host-tested (`double_play_court_clamps` in `tests/test_physics.cpp`).
They take the ORIGINAL's (left edge, width) pair, so a grown bat 1
passes `eff_bat_left()` rather than the port's centre-ish `BAT_X`.

The `(IX+$01)` sprite-shift pokes are deliberately NOT ported. That bit
picks a copy of the bat sprite pre-shifted by 4 px, because ZX bitmaps
are byte-aligned; mode 13h blits at any pixel column, so the port has no
shifted variant and nothing reads bat `sprite_num` at all. Reproducing
the pokes would write state no reader consumes. Recorded in physics.h
instead of implemented.

### LACCE's 8-bit sum wraps, and it does not matter here

`LD A,(IX+$0C) / ADD A,(IX+$02) / CP $80 / RET C` is an 8-bit add, and
the port keeps it as one. Feed it x=$FF, w=$1C and the sum is $1B, below
$80, so the clamp lets the bat straight through.

It is unreachable in play, and the two halves of that are worth keeping
apart: `bat_step_x` has already capped the right edge at $F8, eight
short of the wrap, for both settled widths. The arithmetic is not safe —
the CALLER makes it safe. That is why the u8 stays rather than being
quietly widened, and why the host test characterises the wrap instead of
pretending it is absent. The enemy's copy of this same idiom does
overflow for real; see notes/known-bugs.md.

The first draft of the host test found this by accident, passing $FF as
"a big x" and watching the clamp do nothing.

### BATTY_HOLD_KEYS

New harness knob: `BATTY_HOLD_KEYS=1F,24` seeds `key_state[]` with
scancodes that are then never released. The capture harness runs
headless, so INT 9 never fires and the seeded bits survive the whole
run — a held key is the only kind this harness can express, and steering
is the only thing that needs one.

Two traps it walked into, both worth remembering:

- **It has to be in the Makefile's AUTOEXEC passthrough list.** It was
  not, so the first gate run read no keys at all — and still showed bat
  1 at $64, because that is where the court clamp puts a bat seeded at
  the stock $74. A green-looking number produced by the wrong mechanism.
  `check_env_passthrough` catches exactly this and was not run first.
- **The seeded bat has to start inside its own court.** With the stock
  `BATTY_REPLAY_BAT_OBJECT` x of $74, bat 1's right edge is already at
  $90, past the divider, so it arrives pinned at $64 whether or not a
  key was ever read. The gate seeds $38 — `new_game_reset`'s own mode-2
  placement — so every row distinguishes a working key from a dead one.

### ENTER is bat 2's right, and the harness presses ENTER

$BFFE bit 0 is the Enter key, so in Double Play ENTER steers bat 2
right. It is also what `capture_frame_timeline.py --wait-key` presses to
start a capture, so the harness's own keystroke can nudge bat 2 one step
before its key-up lands.

Whether the step falls inside the gate's 20-frame window is a race, and
it was measured going both ways on consecutive runs ($B4, then $B0). The
gate allows either. It was nearly pinned at $B4 on one observation; the
re-run after tightening is what caught it.

## Ported: bat 2 kills the alien, and three wrong score attributions
(2026-08-10)

`kill_enemy_by_bat` ($A4B8) is reached from `handling_bat`, which in
mode $02 runs for BOTH bats. There is no bonus condition on it — the
routine checks only that an alien exists and is not already exploding,
then `obj_compare_2pix` against the bat. **Any bat touching an alien
destroys it.** The port checked bat 1 only, so an alien drifting over
bat 2's half flew straight through it.

Bat 2's kill zone is the plain 28-wide body with no `extra_px` term,
because the width bonuses are bat-1 globals — the open WS3 residual.

### The side the 350 goes to, and the four routines that disagree

`add_points_to_score` credits whichever side `need_change_player` names,
and every routine that can reach `kill_enemy` sets it from something
different:

    handling_bat      (IX+$02) & $80   the BAT's x
    LA67B_1  (bonus)  (IY+$02) & $80   the CATCHING BAT's x
    handling_bullet   (IX+$02) & $80   the BULLET's x
    handling_ball     (IX+$12) & $80   the ball's OWNER bit, not its x

All four are the first thing their routine does, before any collision
test, so the value is unambiguous at the moment of the kill.

The port passed `BAT_X` to all four. That is right for one. Now each
path passes its own: `blast_active_alien` and `kill_enemy_in_rect` take
the side as a parameter instead of reading `BAT_X` behind the caller's
back.

The bullet's BRICK points were wrong the same way, and differently: they
took the ball's owner. A bullet is not the ball, and `handling_bullet`
says so in its first three instructions.

### The gate that was pinning the bug in place

`check_two_player_state` demanded that TWO brick-scoring sites take
`ball_owner_side`, describing them as "the LAFFC path and the sweep
path". There is no sweep scoring path. The second site is the BULLET,
and the original credits it by the bullet's own x — so the check was
holding a mis-attribution in place, and it FAILED on the fix.

It now asserts by IDENTITY: three sites, three different sides (ball
owner, bullet x, alternating leftover). A count cannot tell three
different-but-correct sides from two correct and one wrong, which is
precisely what went unnoticed.

Worth generalising: a checker that counts occurrences of the RIGHT
answer will also accept the right answer in the wrong place. Where the
sites differ from each other on purpose, assert the difference.

### What is measured and what is only cited

`test-double-play-alien-kill` measures the bat-2 kill and the 350
landing on 2UP, A/B on `BATTY_GAME_MODE` with the alien parked on bat 2
at ($B8, $AD).

The other three attributions are NOT measured. Reaching them from a
seeded scenario needs bonus ownership and a bat-2 laser, both open WS3
items. They are a code fix with a source citation, and the gate's
docstring says so rather than leaving it to be found later.
