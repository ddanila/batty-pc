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

So in Double Play **points are credited by WHERE the event happened, not
by whose bat did it.** A brick broken on the right half scores for
player 2 even if player 1's bat sent the ball there. Outside Double Play
the whole block is skipped and everything scores to the active player.

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
- The real work is `need_change_player` + `add_points_to_score`: an
  x-derived side flag threaded through every scoring site. The port
  credits `player.score` directly in several places, so this needs the
  event's x at each of them — that is the next WS3 stage, and it is
  plumbing rather than a mechanic.
