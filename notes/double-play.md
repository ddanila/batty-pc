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

### ENTER is bat 2's right in the ORIGINAL, and is dropped here

$BFFE bit 0 is the Enter key, so on the Spectrum ENTER steers bat 2
right. The port does not bind it, and this is the one key of the two
clusters that is deliberately missing.

ENTER is this port's attract-chain affordance (PLAN.md WS1) and
`capture_frame_timeline.py --wait-key` presses it to start every
capture, so the binding made the harness's own keystroke nudge bat 2 one
4 px step at a moment nothing controls. It was not only a harness
problem: a player pressing ENTER to get through a screen would move bat
2 in the next level.

It first showed as a race in `test-double-play-input` ($B4 on one run,
$B0 on the next), which I wrongly accommodated by widening the
assertion. It reappeared in `test-double-play-court`, which measures a
pixel extent and had no tolerance to hide behind. See known-bugs #19 and
notes/lessons.md — a gate written around a defect makes the defect
permanent.

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


## Ported: bat 2 catches the ball (2026-08-10)

`LAB1F` tries bat 1 and falls through to bat 2 in mode $02, and the
catch branch (`LAB1F_1..3`, bonus $03) belongs to whichever bat the ball
actually met. The port had it on bat 1 only, so a ball arriving on bat
2's half bounced off a magnet bat that should have held it.

`deflect_ball_off_bat_2` gained the branch and now returns a flag like
bat 1's, so `step_ball` stops the frame on a catch.

The comment that used to sit on that function said the missing catch was
blocked by the stuck system being written around the primary ball and
bat 1. That was accurate, and it is now unblocked: the stuck fields
became per-ball in `cce6483`, the stuck functions took a bat, and this
is the four lines that follow. See notes/bat-deflection.md.

No `extra_px` term on bat 2's branch: bat 2 is always the plain 28-wide
sprite, because the width bonuses are bat-1 globals.

### It depends on the bonus-sharing divergence, deliberately

Bat 2's branch reads `objects[OBJ_BAT_2].bonus_applied`, which
`set_bat_bonus` keeps in step with bat 1's — so in practice both bats
hold the MAGNET at once. That sharing is the open divergence (the
original applies a bonus to the CATCHING bat only, `DEC (IY+$14)` with
IY the bat that caught it).

The branch is correct either way and needs no change when ownership
splits. The GATE does: it drops a CATCH bonus on bat 1 and relies on it
reaching bat 2. Its docstring says so, so that day is a one-line edit
rather than a mystery failure.


## Ported: the extras meet bat 2 too (2026-08-10)

`LAB1F` runs once per BALL object and falls through to bat 2 when bat 1
did not overlap, so this was never a primary-only rule. The port's
`step_extra_ball` tested bat 1 only — its bat block read
`eff_bat_left`/`eff_bat_right` and nothing else — so a secondary fell
straight through bat 2 and was lost.

This is the gap that was named and left whole when the secondary CATCH
landed, closed the next commit.

`extra_ball_meets_bat` is the block factored out, which is what makes
adding bat 2 four lines instead of a duplicated paragraph. Inlining it
was the reason bat 2 had been skipped: the cost of adding it was
copying the whole block.

### Three outcomes, not two

    0  no contact — try the next bat
    1  deflected  — *next_y snapped, direction rewritten
    2  caught     — stop the frame

The first draft returned a boolean "was it caught". A bat-1 DEFLECTION
then read as "no contact", so the ball was offered to bat 2 and could be
handled TWICE in one frame. LAB1F falls through only when bat 1 did not
OVERLAP, so a deflection has to stop the search as firmly as a catch.

Caught by reading the control flow rather than by a test, and it is not
gated: the two bats are 28 px apart at their closest, and both
deflections landing in one frame is not something a seeded trajectory
here produces. The comment in `extra_ball_meets_bat` is what carries it,
and the gate's docstring says as much rather than implying coverage.

### What the helper deliberately does NOT do

It never touches `ball_owner_side`. That is the PRIMARY's owner bit, and
the original keeps one per ball — `RES 7,(IX+$12)` on whichever ball
LAB1F is handling. The port models only the primary's, so an extra
bouncing off bat 2 must not rewrite it, or brick points would change
hands on the strength of a ball that has no owner of its own.

A real per-extra owner is its own item. Silently reusing the primary's
would be worse than not having one, because it would look correct.


## Ported: the owner bit is per BALL (2026-08-10)

`LAB1F_0` writes it on the ball being handled —

    RES 7,(IX+$12) / BIT 7,(IY+$02) / JR Z / SET 7,(IX+$12)

— and `handling_ball` runs once per ball object, so every ball has one.
The port kept a single `ball_owner_side` and spent it on the primary, so
brick points from a secondary were credited to whoever last deflected
the PRIMARY: a ball the player may not have touched for seconds, and
possibly on the other side of the court.

Now `ball_owner_side[3]`, indexed like the stuck and `mag_*` arrays.
`brick_hit_resolve` takes the slot, and so do `brick_collision` and
`laffc_collision` — each of their four call sites already knew which
ball it was stepping, so nothing had to be derived or guessed.

Three writers:

- level entry seeds all three from the alternating start side. No extras
  are alive then, so seeding all three costs nothing and means a slot
  cannot be read before it is written.
- `spawn_extra_ball` copies the primary's. The original does not copy
  anything — `+$12` is part of the object and the spawn writes into
  slots that already carry a bit — but inheriting is the only reading
  that does not credit a brick to a player who never touched the ball
  that broke it.
- `extra_ball_meets_bat` re-owns on a deflection, which is LAB1F_0 on
  the right ball. That is the line the previous commit deliberately left
  out, with a comment saying it had nowhere correct to record itself.

### The gate asserts a relationship, not literals

`ball_start_right` alternates every level entry (`XOR $88`), so the
entry side is a fact about WHICH entry a run happens to be, not about
the code. The first draft of `test-extra-ball-owner` asserted
`(0, 0, 0)` at frame 2 and failed against a run that legitimately
started on the right.

It now reads the entry side from frame 2, requires all three to agree
there, and then requires exactly slot 1 to differ at frame 12 — bat 1
touched one ball, so one bit may move. That is the whole difference
between a per-ball owner and a shared one, and it holds whichever side
the entry picked.

Finding the frame took a measurement rather than a guess: the three
balls spawn together and spread out on their derived directions, so
there is a window where one has bounced and the others have not. Frames
6 through 16 all show `010001`.


## Ported: bat 2 catches falling bonuses (2026-08-10)

`get_bonus` ($A67B) has the same fall-through shape as LAB1F:

    LD IY,object_bat_1 / CALL obj_compare_2pix / JR C,LA67B_0
    LD A,(game_mode) / CP $02 / RET NZ
    LD IY,object_bat_2 / CALL obj_compare / RET NC
    CALL bonus_flag_swap / CALL LA67B_0 / JP bonus_flag_swap

bat 1 first, bat 2 only in mode $02 and only when bat 1 missed. The port
tested bat 1 alone, so a bonus falling on player 2's half was caught by
nobody — it went straight through the bat and off the bottom.

`bonus_catching_bat` is the fall-through, returning which bat or -1.

`LA67B_1` sets `need_change_player` from the CATCHING bat's x, so the
400 follows the bat that got it — as does SCORE_5K's 5000 and
KILL_ALIENS' 350, both of which sat inside `bonus_apply` reading
`BAT_X`. `bonus_apply` takes `catcher_x` now. The gate uses SCORE_5K
precisely so both awards are on the same probe: 400 + 5000 = 5400, and
all of it on 2UP.

### What is still shared, and it is the bigger half

The EFFECT. `set_bat_bonus` writes both `bonus_applied` bytes, and the
width and laser state (`bat.extra_px`, `bat.extra_target`,
`bat.big_ticks`) are bat-1 globals with nowhere to put bat 2's.

The original keeps them apart with `bonus_flag_swap` around the bat-2
call — it exchanges `bonus_flag` with `bonus_flag_copy`, so `LA67B_0`
runs against the catching bat's state throughout and swaps back
afterwards. The same trick wraps bat 2's `handling_bat` and its LASER
branch (`LA67B_0` / `LA55A`).

So the port now agrees with the original about WHO CATCHES and WHO IS
PAID, and still differs about WHO GETS THE EFFECT. That is the open
remainder of WS3, and it is a state-shape problem rather than a missing
branch: `bonus_flag_swap` presupposes two copies of everything the
bonus touches.

### The seed had to be measured, twice

Type 6 is ROCKET, which jumps out to `get_rocket` before the award — a
bonus that is caught but pays nothing, so the first gate read 0 and
looked like a missing catch. And the fall is SLOW (`motion_accel_step`
from a standing start): a bonus seeded at y=150 had moved six pixels in
twenty frames. Seeded at 158 with a 30-frame window it lands.

Both were visible only because the gate prints `bonus_state` alongside
the scores. A gate that printed the score alone would have said
"not caught" in both cases and sent the next reader to the wrong code.


## Ported: the bonus BYTE stays on the catching bat (2026-08-10)

`set_bat_bonus` wrote BOTH `bonus_applied` bytes, justified by a comment
saying bat 2 was "the second bat the original keeps for the rocket
flight" and that the two "must not disagree".

**Both halves were wrong.** `object_bat_2` is player 2's bat, and the
original keeps the bytes deliberately APART — `LA67B_0` runs inside
`bonus_flag_swap` for a bat-2 catch, so only the catching bat's byte
moves.

What the original does where an effect belongs to the BALL rather than
to a bat is check both. `LA27E`'s big-ball test is the model:

    LD A,(object_bat_1+$14) / CP $07 / JR Z,set_big_ball
    LD A,(object_bat_2+$14) / CP $07 / JR NZ,obj_processing

and its expiry a few lines on clears each byte independently, testing
each for $07 first — a bat holding some OTHER bonus keeps it.
`big_ball_active` and `tick_big_ball_timer` follow both shapes now.

Level entry and respawn clear both, which is a reset rather than an
expiry and so needs no test.

### What this does NOT split, and why

The WIDTH and the LASER. `bat.extra_px`, `bat.extra_target` and
`bat.big_ticks` are one bat's worth of state, so BIG_BAT caught by bat 2
is now GUARDED to bat 1 and widens nobody.

That is still wrong, and it is less wrong than before: previously a
BIG_BAT caught by bat 2 widened BAT 1. A wrong bat became a missing one.
The remaining fix needs two copies of everything a bonus touches, which
is what `bonus_flag_swap` presupposes, and it is the last item in WS3.

### Two gates encoded the old claim, and both said so

`test-invariant-owners` carried the "must not disagree" sentence as the
REASON for its one-writer rule, and failed with exactly that text
printed back. `test-double-play-bat2-catch` dropped its CATCH bonus on
bat 1 and relied on the mirror to reach bat 2 — and its docstring had
predicted this: *"when ownership splits, the seed has to drop the bonus
on bat 2 instead."*

Both fixes were mechanical because both gates said what they assumed.
Writing down what a test depends on, especially when you expect it to
change, converts a future debugging session into an edit.

Reseeding the second one took two goes: at y=158 the bonus is still
falling when the ball arrives at frame 12, so the MAGNET was not up yet
and the ball simply bounced. Seeded at 168 it overlaps the bat on frame
1 and is caught immediately.


## The bat's width state is per-bat now — shape only (2026-08-10)

`BatState` was one struct, `bat`. It is `bats[2]` with `BAT_SLOT()`
mapping an object index to a slot, and `bat1` naming bat 1's.

**No behaviour change**, deliberately: every one of the 58 existing
sites still reads bat 1's, and the four bat-2 geometry sites that used
to open-code `x_coord + BAT_BODY_W` now call `bat_left_of` /
`bat_right_of`, which return exactly the same numbers while
`bats[1].extra_px` is 0. That switch is what makes slot 1 a live read
rather than a field waiting for a later commit to notice it.

Same staging as the stuck-ball fields: shape, then threading, then the
feature. What follows is bat 2's width actually growing, which needs
`render_bat_2` to draw the resize sides and the big body — bat 1's
renderer already does both, and generalising it is the work.

### The bug this introduced, and it was not the rename

`OBJ_BAT_1` is 6 and `OBJ_BAT_2` is 5 — OBJECT-table positions, not 0
and 1. Written as `bats[OBJ_BAT_1]` on a two-element array, every access
ran four elements past the end. It compiled clean under `-w4 -we`; the
symptom was the ball flying to the ceiling on launch and nineteen gates
red, behind a diff that was otherwise a pure rename.

Found by refusing the "a rename cannot break anything, so the sweep must
be flaky" instinct: HEAD passed 2/2 and the change failed 4/4, and then
`sed 's/bat1\./bat./' | diff` against HEAD reduced seventy diff lines to
ONE — the declaration. Full write-up in notes/lessons.md.

`BAT_SLOT()` is the fix and the guard: an out-of-range argument lands on
a real element instead of off the end.


## One bat renderer for both bats (2026-08-10)

`render_bat_of(b, attr)` takes every input from the bat's object and its
`BatState`, so the same code draws either. `render_bat` and
`render_bat_2` are two-line wrappers.

Bat 2 had its own three-line copy that could only ever draw the plain
body at a fixed 32 px. That copy is why bat 2's width had nowhere to
show even once the state existed — and, more to the point, why nobody
had noticed: the renderer physically could not express the thing that
was missing.

Behaviour is unchanged for the sweep, which is the check: while bat 2's
`extra_px` is 0 and its bonus byte is not LASER, `render_bat_of` emits
the same x, y, 32x13 attr window and `SPR_BAT_NORMAL` the copy did.

**One change is real and deliberate:** a bat 2 holding LASER now shows
the gun-mounted sprite. It could not before, and it should — the
original picks each bat's sprite from that bat's own state
(`(IX+$01)`, set in `handling_bat`, which runs per bat). Reachable since
the bonus byte stopped being mirrored. Not gated: no scenario yet drops
a LASER on bat 2.

### The lint caught it, correctly

`test-visual`'s "no stray blit_sprite_attrs_to_buff in moving-object
renderers" failed, because the blit moved into a function not on its
approved list. That is the lint working, not the lint being in the way —
the same note its docstring already made about `render_bat_2` in
August.

`render_bat_of` is on the list now. `render_bat` and `render_bat_2` stay
on it even though they no longer blit: a future edit could give either
its own blit back, and the lint should catch that rather than pass
because the name is old.

### Still to come

Bat 2's width does not yet GROW — `bonus_apply` still guards BIG_BAT to
bat 1 and `tick_bat_resize` still ticks one bat. The renderer and the
collision extents are ready for it; what wants checking first is the
dirty-redraw path, which knows bat 1's bounds
(`redraw_bat`, `bat_sprite_bounds`) and draws bat 2 only on a full
compose. A widening bat 2 could leave residue there.

## Bat 2's sprite was frozen at level entry (2026-08-10)

**A visible defect that a green gate was hiding.**

`test-double-play-input` proved bat 2 steers, by reading `object_bat_2`
out of the probe. That was true and insufficient: the object moved the
whole way while the SPRITE stayed where the level-entry compose put it.

Measured with a screendump: object at `$DC`, sprite still at `$B4`.

### Why

`render_bat_2` was called from exactly one place — `compose_level_scene`,
which runs at level entry. None of the per-frame redraw paths knew about
it. And `bat_moved` was `BAT_X != BAT_PREV_X`, bat 1's x alone, so a
frame in which only player 2 moved took `redraw_frame`'s early-out and
drew nothing at all.

The `$B4` rather than `$B0` is the tell: bat 2 had taken one step before
the entry compose ran, and that one step is all that was ever drawn.

### The fix

- `render_bats()` draws both, and every bat-band path calls it.
- `bat_changed(b)` compares a bat against what was last DRAWN — x, y,
  width, bonus byte, fire ticks — and `bat_moved` and
  `bat_needs_full_redraw` both use it for both bats, so the dirty and
  full paths cannot disagree about whether the band is stale.
- `remember_bat_draw_state_of(b)` keeps that cache per bat, using the
  object's own `prev_x` as its last-drawn x, exactly as bat 1 did.
- Both redraw windows widen to cover whatever bat 2 vacated and whatever
  it now covers. One window serves both bats because they share the
  band; the cost is a wide flush when they are at opposite ends, which
  is what the full path would have paid anyway.

### The lesson, and it is about the gate not the code

A state gate cannot see a rendering bug. `test-double-play-input`
asserted everything about bat 2 except the only thing that was wrong.

`test-double-play-bat2-redraw` is a PIXEL gate: it diffs a run with
bat 2 driven right against one with it still, and requires the
difference to span both footprints — the one it vacated and the one it
now covers, with the right edge derived from `object_bat_2` in the same
run rather than hardcoded.

It is shaped for the actual failure, which was not "bat 2 never moved"
but "bat 2 moved 4 px and froze". A gate asserting merely that something
changed would have passed against the bug.
