# Rocket bonus flight + level clear

The ROCKET bonus ($06): when caught, the bat turns into a rocket that flies
up and ends the round. Fully ported and faithful in all three parts —
motion, no destruction during flight, and the sequential end-of-flight
tally. Gated by `test-rocket-bonus`, `test-rocket-flight-redraw`,
`test-rocket-completion-no-ball` and `test-midgame-brick-replay`.

## Launch — `LBAED_6`

The play loop checks `object_rocket` before `balls_quantity`, and when
`get_rocket` ($AA9D) has set it, jumps once to:

    LBAED_6:
      LD B,$0B; LD DE,$0016; LD IX,object_ball_1
    LBAED_7:                       ; deactivate all 11 objects
      LD A,(IX+$00); AND A; JR Z,LBAED_8
      SET 7,(IX+$00)
    LBAED_8:
      ADD IX,DE; DJNZ LBAED_7
      LD A,$01
    LBB83:
      LD HL,$0000; LD (HL),A       ; self-modified target = the caught bat
      LD A,$06; LD (object_rocket),A
      LD A,$05; LD (sounds_queue),A
      XOR A; LD (counter_misc),A    ; *** the frame counter is RESET to 0 ***
      JR LBB97_0

The order matters twice. Checking the rocket BEFORE `balls_quantity` is what
lets the level-clear sequence hide every ball without entering the bat-death
path — `test-rocket-bonus` fails if the port's no-ball death guard stops
excluding `rocket_active`.

And `XOR A / LD (counter_misc),A` is why the port's PER-ROCKET counter is
byte-equivalent to the original's global one. `get_rocket` also zeroes the
velocity accumulator.

## Flight — `LBB97`, a loop of its own

    LBB97:
      LD A,(counter_misc); INC A; LD (counter_misc),A   ; ++ per flight frame
      CALL random_generate
      CALL call_hl_for_all_obj                          ; -> handling_rocket
      CALL fill_briks_data                              ; shimmer slots only
    LBB97_0:
      ... save/print objects ... EI/HALT/DI ... render/restore ... pause_game
      LD A,(object_rocket); AND A; JP Z,LBBFB           ; gone -> award
      JP LBB97

**There is no brick destruction anywhere in this loop.** `handling_rocket`
is motion-only, `fill_briks_data` only cycles the metal-shimmer slots, and
bricks are not objects. The rocket sprite is simply drawn OVER the intact
brick field as it rises.

The port's `step_rocket` had an in-flight bbox sweep destroying cells; it is
gone, and the dirty redraw restores the bricks behind the rocket from
`scr_buff`. `test-rocket-flight-redraw` seeds an already-attached rocket,
lets it lift the bat for 18 rendered frames, and compares the dirty redraw
against a forced full-flush baseline — it catches stale bat and rocket
pixels left behind when the bat's Y changes during level clear.

Note this loop is `$BB97`, not the `$BA83` main loop, so a rocket GT capture
must frame-step on PC=`$BB97`.

## Motion — `handling_rocket` ($A89A)

    handling_rocket:
      LD A,(counter_misc); AND $01; LD (IX+$01),A   ; 2-frame sprite flicker
      CALL calc_write_spr_addr
      LD HL,(LA8CF); LD DE,$FFE0; ADD HL,DE          ; velocity += -32
      LD A,(counter_misc); CP $38; JR C,LA89A_0      ; PERSIST only past 56
      LD (LA8CF),HL
    LA89A_0:
      LD A,(LA8D1); LD E,A; LD D,(IX+$04); ADD HL,DE ; pos(16.8) += velocity
      LD A,L; LD (LA8D1),A; LD A,H; LD (IX+$04),A
      SUB $06; LD (object_bat_1+$04),A; LD (object_bat_2+$04),A
      RET

16.8 fixed-point position with a velocity accumulator that grows by -32 per
frame but only PERSISTS once the counter reaches $38 (56). Since the counter
was zeroed at launch, that means a slow near-constant lift for the first 56
frames (velocity = 0 + -32 = -0.125 px/frame) and then quadratic
acceleration. Bat y = rocket y - 6.

`step_rocket` reproduces this with an `unsigned char` per-rocket counter so
it wraps at 256 like the byte counter.

## End of flight — `add_points_for_left_briks` ($AF0D)

    LBBFB:
      CALL add_points_for_left_briks
      ... play_sounds_queue / increment_round_number / pause_long ... next round

The tally walks the grid row-major and for every brick that is not
empty or undestructible (`AND $A0`) adds its points, updates the score,
alternates `need_change_player`, plays the queue and pauses — **per brick**.
It never clears a cell: the bricks stay on screen through the count-up, and
the next-level load is what removes them.

`play_rocket_award_tally` does that, one brick per PIT tick with the scene
and score redrawn each tick, then clears every cell at the END so
`live_bricks_remaining() == 0` drives the round advance. That final clear is
load-bearing: without it the advance never fires, or the no-ball death
branch triggers instead.

The pace is a deliberate non-byte-exact choice. The original's per-brick
`pause_short` is a CPU busy-wait (`LD E,$FF; DEC E/JR NZ; DEC D/JR NZ`), so
`D=$03` is ~765 Z80 iterations ≈ 3 ms — the whole 180-cell tally is a
~0.5-1 s score-count blip, and its speed is tied to the Z80 clock. The port
paces off PIT ticks: the score TOTAL, the bricks-stay-visible render and the
row-major order are faithful; the timing is not reproducible without an
arbitrary fudge factor.

`test-rocket-completion-no-ball` covers the frame after the fly-off — the
rocket has left, the bricks are awarded, the level-clear pause is about to
begin. It holds the frame via `BATTY_HOLD_ROCKET_CLEAR` and compares the old
bat/ball band against a full-flush baseline, catching the regression where
the primary ball briefly reappeared.
