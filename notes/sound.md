# The beeper queue, and what the port keeps of it

The decode. The one open decision is in PLAN.md WS5.

## The queue

`sounds_queue` ($C0B8) is **5 rows of 7 bytes**, not 4:

- `play_sounds_queue` walks all five every frame (`LD B,$05`);
- `get_free_sound_slot` ALLOCATES from the first four only (`LD B,$04`);
- the fifth is written directly by `LAFFC_37`.

Byte +0 is the effect id, 0 meaning free. The remaining six are that
effect's state; several routines use +1 as a countdown shaping the sound
across frames.

## The id is a table position

    play_selected_sound:
      LD HL,play_sounds_list-2 / ADD A,A / LD E,A / LD D,$00
      ADD HL,DE / LD E,(HL) / INC HL / LD D,(HL) / EX DE,HL / JP (HL)

so an id indexes `play_sounds_list` directly, and `test-sound-ids` checks
the port's `SND_*` constants against it. `SND_MAGNET` sits deliberately past
the end: the original calls `play_sound_magnet` synchronously from
`magnets.asm` and never queues it.

## The primitives

    sound_beep:                        ; E = half period, in DJNZ turns
      LD B,E / EI / LD A,$10 / OUT ($FE),A
      DJNZ $ / LD B,E / XOR A / OUT ($FE),A
      DJNZ $ / RET

    sound_beep_cont_d:                 ; D = how many cycles
      CALL sound_beep / DEC D / JR NZ,sound_beep_cont_d

    sound_beep2 ($C136):               ; ON for B turns, OFF for D — one cycle

One `sound_beep` is one square-wave cycle, and a looping DJNZ is 13
T-states, so:

    frequency = 3_500_000 / (2 * E * 13)
    duration  = D * 2 * E * 13 T-states

`src/sound.cpp` carries the original's `(D, E)` pairs verbatim with
citations and converts E to a PIT divisor honestly:
`period = 1193180 / (3500000 / (26 * E)) = 8.86 * E -> 9 * E`.

Read each routine INDIVIDUALLY. A regex that carried `LD DE` forward across
routine boundaries produced a plausible and wrong table on the first
attempt — most effects do not use `LD DE,$xxxx` at all.

| routine | D | E | frequency | duration |
|---|---|---|---|---|
| `play_sound_normall_brik` | `$08` | `$44` | 1980 Hz | 4.04 ms |
| `play_sound_bat_beat` | `$04` | `$66` | 1320 Hz | 3.03 ms |
| `play_sound_metal_brik` | `$18` | `$30` | 2804 Hz | 8.56 ms |

## Sweeps: `C` counts down and the operands are recomputed

`play_sound_LC122` ($C122) is a LOOP:

    play_sound_LC122:
      LD A,C / XOR E / ADD A,A / LD B,A
      AND $0F / LD D,A / LD A,B / AND $0C / ADD A,$08 / LD B,A
      CALL sound_beep2
      DEC C / JR NZ,play_sound_LC122
      RET

`B` and `D` are recomputed from `C XOR E` every turn, so it is a descending
sweep of C beeps — **nine** for `play_sound_ball_start` (C=$09, E=$14) and
**four** for `play_sound_shot` (C=$04, E=$0F). The port computed B and D
once and played a single beep, so the launch blip was 1/9 of itself and the
laser shot 1/4. Ported as a per-frame sweep with `state` holding C, which is
how every other multi-frame effect here already works.

`play_sound_live_add` sweeps differently — pitch from its own counter:

    LD A,(IX+$01) / AND $03 / JR NZ,...      ; only every 4th frame
    LD A,(IX+$01) / ADD A,$14 / LD E,A
    LD D,$03 / CALL sound_beep_cont_d
    DEC (IX+$01) / DEC (IX+$01)              ; walks down by 2

**DEC, DEC, then test.** The port tested for zero BEFORE decrementing, so it
reached state $00 with the counter still live and beeped a ninth time at the
lowest pitch. The original's last beep is at state $04: eight beeps.

An audit of every queued effect against `original/disasm/routines/sound.asm`
found no other collapsed loops. `play_sound_normall_brik`,
`play_sound_metal_brik`, `play_sound_bat_beat` and `play_sound_bat_resize_2`
are a single `sound_beep_cont_d` and a slot clear; `play_sound_live_add`,
`play_sound_08` (SPARK_FANOUT), `play_sound_bat_resize_1` and
`play_sound_triple_ball` are one beep per frame with their counter in
`(IX+$01)`, which is the shape the port uses.

## The 20 ms floor, and why it is not a clock problem

`sound_beep_cont_d` computes the real duration now (`D * 2 * E * 13`
T-states through `sound_set_clock_hz`) rather than discarding D, and
`sound_beep2_bd` is `(B + D) * 13` rather than a literal 1 tick. So the
arithmetic is faithful.

But the port's sound clock is the 50 Hz frame counter, so **each beep still
rounds up to one 20 ms tick** against the original's 3-9 ms. A nine-beep
sweep takes nine frames where the original takes about 9 x 0.5 ms inside
one. The pitches and the envelope shapes are right; the timebase is the open
question.

Making the clock finer does not fix it, and that is the point:

**The original's beeper BLOCKS.** `sound_beep` is DJNZ spin loops around
`OUT ($FE),A` — no timer, the CPU makes the wave — so `play_sounds_queue`
consumes real time. And the callers know:

    CALL play_sounds_queue
    JR NZ,LBAED_4

`play_sounds_queue` latches the frame counter on entry and compares it on
exit, returning Z only if the whole queue fitted inside one interrupt. The
main loop branches on that: `LBAED_4` skips ahead to the running-dot draw,
so the frame gives up some of its remaining work when the sound ran long.

A PC needs none of that — the PIT holds a tone with no CPU, which is why the
port latches a divisor and returns. That is an ADAPTATION, and a defensible
one, so the choice is:

1. **Block like the original.** Spin for `D * 2 * E * 13` T-states' worth of
   time inside the frame and port the overrun branch. Faithful to the
   millisecond, and it imports the original's frame-pacing behaviour. Costs
   3-9 ms of every frame that plays a sound.
2. **Keep latching, stop on a finer tick.** The resolution of STOPPING is
   whatever calls `sound_tick()`, not the clock's units, so this needs the
   stop scheduled from an interrupt — and the port's timer runs at the same
   50 Hz.

(1) is the faithful one. Neither is small, and it is a decision about how
much of the original's frame timing to import rather than a bug to fix.

`beep_duration_follows_d` pins the three measured envelopes and then asserts
the 50 Hz floor itself, so nobody can believe this closed while it is not.

## The last divergence in the queue belongs elsewhere

    play_sound_bat_resize_1:
      LD A,(bonus_flag) / AND A / RET NZ

With `bonus_flag` non-zero the original skips the beep AND leaves its
counter alone, so the shrink sweep PAUSES while a bonus transition is in
flight. The port has no `bonus_flag`, and it cannot get one cheaply: the
flag is part of the `(IX+$15)` bit-gated resize state machine, written by
`handling_bat`'s transform paths and read there for the gun sprite. So this
is not a sound item that happens to touch bat state; it is the resize
machine seen from the sound side. `notes/parity-gaps.md` item 1 owns both.

## Two things a length test does not cover

Pinning the frame counts left a live mutant: passing a constant `C` to the
beep keeps all nine frames and flattens the pitch to one note.
`sweeps_last_multiple_frames` also requires the PERIOD to change across the
sweep. "It runs for nine frames" and "it is a nine-step sweep" are different
claims, and only the first is obvious to assert.

That same test kept two lists — effects that must outlive their frame, and
effects that must not — and had `SND_BALL_START` and `SND_SHOT` in the
second, so a one-beep-of-nine effect passed as correct. The test was
describing the PORT's structure rather than the original's. Both moved, and
the clicks list carries a note to check the disassembly for a loop before
adding to it.
