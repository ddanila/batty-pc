# Double Play, traced

`game_mode == $02`. PLAN.md's WS3 described it as "court split into
halves with a divider, each bat confined to its half", and named
"per-bat margin clamps at the divider" as one of the things to port.

**Both halves of that are wrong.** Traced 2026-08-09, before building
anything on it.

## The bats are not confined

`handling_bat` moves the bat by ±4 from the control word and then, in
`handling_bat_no_transform`, clamps it:

    handling_bat_no_transform:
      CALL check_left_margin
      CALL check_right_margin

Those are the same two routines the alien uses (`notes/enemy-movement.md`):
`x < $08 -> x = $08`, and `(u8)(w + x) >= $F9 -> x = $F8 - w`. Full
playfield, no divider term. The resize path calls `check_margins`, which
is those two plus the top clamp — again no divider.

Both bats go through the same `handling_bat` with the same clamps; the
only difference is which object and which control word:

    LD A,(ctrl_btns_pressed) / PUSH AF
    LD A,(ctrl_type_2up)
    CALL get_right_player_ctrl_state
    LD A,(ctrl_btns_pressed) / LD (ctrl_btns_pressed_copy),A
    LD IX,object_bat_2 / CALL handling_bat
    POP AF / LD (ctrl_btns_pressed),A

So player 2's device drives `object_bat_2`, and nothing stops either bat
crossing the middle. Both can occupy the same half.

## The divider is a marker, not a wall

`spr_separator` is 16 px wide and 24 rows tall at (125, 169) — down in
the bat band, not spanning the court. Nothing tests the ball against it.
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

So nothing in flight ever changes it. It is set once, at
`all_var_init`, from which side the ball STARTS on — and the start side
alternates on every entry:

    LD A,(ball_x_coord+$01) / XOR $88 / LD (ball_x_coord+$01),A
    ...
    ball_x_coord: LD A,$48          ; self-modified: $48 <-> $C0
    LD (object_ball_1+$02),A
    CP $C0 / JR NZ,LB7F8_1
    LD A,(object_ball_1+$12) / OR $80 / LD (object_ball_1+$12),A

`$48 XOR $88 = $C0` and back, so the ball starts left, right, left...

**Brick points therefore go to whoever the BALL belongs to, for that
ball's whole life, wherever the brick is.** Only the bat, bullet and
bonus sites are positional. Ported as `ball_owner_side`, with
`ball_start_right` carrying the alternation; `PROBE.TXT` reports it as
the `own` field of `scores=`.

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
