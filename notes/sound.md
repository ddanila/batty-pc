# The beeper queue, and what the port keeps of it

Traced 2026-08-09, before starting WS5, and the section's premise was
wrong twice. Both corrections are in PLAN.md; this file is the decode.

## The queue

`sounds_queue` ($C0B8) is **5 rows of 7 bytes**, not 4.

- `play_sounds_queue` walks all five every frame (`LD B,$05`)
- `get_free_sound_slot` ALLOCATES from the first four only (`LD B,$04`)
- the fifth is written directly by `LAFFC_37`

Byte +0 is the effect id, 0 meaning free. The remaining six are that
effect's state; several routines use +1 as a countdown that shapes the
sound across frames.

## The id is a table position

    play_selected_sound:
      LD HL,play_sounds_list-2 / ADD A,A / LD E,A / LD D,$00
      ADD HL,DE / LD E,(HL) / INC HL / LD D,(HL) / EX DE,HL / JP (HL)

so an id indexes `play_sounds_list` directly. `test-sound-ids` checks
the port's `SND_*` constants against it. `SND_MAGNET` is deliberately
past the end: the original calls `play_sound_magnet` synchronously from
`magnets.asm` and never queues it.

## The primitive

    sound_beep:                        ; E = half period, in DJNZ turns
      LD B,E / EI / LD A,$10 / OUT ($FE),A
      DJNZ $ / LD B,E / XOR A / OUT ($FE),A
      DJNZ $ / RET

    sound_beep_cont_d:                 ; D = how many cycles
      CALL sound_beep / DEC D / JR NZ,sound_beep_cont_d

One `sound_beep` is one square-wave cycle. A DJNZ that loops is 13
T-states, so:

    frequency = 3_500_000 / (2 * E * 13)
    duration  = D * 2 * E * 13 T-states

## Verified examples

Read individually, because a regex that carried `LD DE` forward across
routines produced a plausible and wrong table on the first attempt —
most effects do NOT use `LD DE,$xxxx`.

| routine | D | E | frequency | duration |
|---|---|---|---|---|
| `play_sound_normall_brik` | `$08` | `$44` | 1980 Hz | 4.04 ms |
| `play_sound_bat_beat` | `$04` | `$66` | 1320 Hz | 3.03 ms |
| `play_sound_metal_brik` | `$18` | `$30` | 2804 Hz | 8.56 ms |

Not every effect is a fixed pair. `play_sound_live_add` sweeps:

    LD A,(IX+$01) / AND $03 / JR NZ,...      ; only every 4th frame
    LD A,(IX+$01) / ADD A,$14 / LD E,A
    LD D,$03 / CALL sound_beep_cont_d
    DEC (IX+$01) / DEC (IX+$01)              ; state walks down by 2

and `play_sound_bat_resize_1` returns early on `bonus_flag`. So the
envelope work is per-effect, not one conversion.

## What the port keeps, and the one thing it drops

`src/sound.cpp` carries the original's `(D, E)` pairs verbatim, with
citations — `sound_beep_cont_d(0x08, 0x44)` and so on — and converts E
to a PIT divisor honestly:

    period = 1193180 / (3500000 / (26 * E)) = 8.86 * E  ->  9 * E

**But `sound_beep_cont_d` ignores D:**

    void sound_beep_cont_d(unsigned char d, unsigned char e) {
        (void)d;
        sound_beep_e(e);
    }

and `sound_beep_e` starts the tone for `ticks = 1`. The port's sound
clock is the 50 Hz PIT frame counter, so every effect lasts **20 ms**
against the original's 3-9 ms. The pitches are right; the durations are
all ~5x too long, from one cause.

### Half of it is fixed (2026-08-09)

`sound_beep_cont_d` computes the real duration now —
`D * 2 * E * 13` T-states, converted to clock ticks through
`sound_set_clock_hz` — instead of `(void)d`. A host test drives the
module at a microsecond clock and pins the three measured envelopes:

    beep_duration_follows_d      3 envelopes + the 50 Hz floor

The last assertion in that test is the gap itself: at 50 Hz a 4 ms
effect still rounds to one 20 ms tick. It is written as an assertion so
nobody can believe this fixed while it is not.

### The rest is a DESIGN question, not a clock

Two commits ago I wrote that WS5 needs "a finer clock, or a speaker
driver that can schedule a stop between frames". That was the wrong
frame for it.

**The original's beeper BLOCKS.** `sound_beep` is a pair of DJNZ spin
loops around `OUT ($FE),A` — there is no timer, the CPU makes the wave
itself — so `play_sounds_queue` consumes real time, 3 to 9 ms of it per
effect. And the callers know:

    CALL play_sounds_queue
    JR NZ,LBAED_4

`play_sounds_queue` latches the frame counter on entry and compares it
on exit, returning Z only if the whole queue fitted inside one
interrupt. The main loop branches on that — `LBAED_4` skips ahead to the
running-dot draw, i.e. the frame gives up some of its remaining work
when the sound ran long.

A PC does not need that. The PIT holds a tone with no CPU at all, which
is why the port latches a divisor and returns. That is an ADAPTATION,
and a defensible one — but it means "faithful envelopes" cannot be
reached by making the clock finer. The choice is:

1. **Block like the original.** Spin for `D * 2 * E * 13` T-states'
   worth of time inside the frame, and port the overrun branch. Faithful
   to the millisecond, and it hands back the frame-pacing behaviour the
   original has. Costs 3-9 ms of every frame that plays a sound, on
   hardware where that budget was measured for something else.
2. **Keep latching, stop on a finer tick.** Needs `sound_tick()` called
   far more often than once a frame — the resolution of STOPPING is
   whatever calls it, not the clock's units — so realistically it needs
   the stop scheduled from an interrupt, and the port's timer runs at
   the same 50 Hz.

Neither is a small change, and (1) is the faithful one. It is a decision
about how much of the original's frame-timing behaviour to import, not a
bug to fix, so it wants a deliberate call rather than an autonomous one.

### Why the arithmetic fix was worth doing anyway

20 ms is the port's finest unit. The original's envelopes are sub-frame
— `sound_beep_cont_d` returns after a few thousand T-states, well inside
one interrupt — so reproducing them needs a finer clock than the frame
counter, or a speaker driver that can schedule a stop between frames.

That is the actual content of WS5, and it is a timing-infrastructure
change rather than a table of constants. The constants are already
there, and so is the arithmetic — what is left is `sound_set_clock_hz`
being given something finer than the frame counter, and a speaker
driver that can stop a note between frames.


## The lc122 effects were one beep of nine (2026-08-10)

`play_sound_LC122` ($C122) is a LOOP, and the port was firing one turn
of it:

    play_sound_LC122:
      LD A,C / XOR E / ADD A,A / LD B,A
      AND $0F / LD D,A / LD A,B / AND $0C / ADD A,$08 / LD B,A
      CALL sound_beep2
      DEC C / JR NZ,play_sound_LC122
      RET

`C` counts DOWN and `B`/`D` are recomputed from `C XOR E` every turn, so
it is a descending sweep of C beeps — **nine** for `play_sound_ball_start`
(C=$09, E=$14) and **four** for `play_sound_shot` (C=$04, E=$0F).

`sound_play_lc122` computed B and D once, played a single beep and
cleared the slot. So the ball-launch blip was 1/9 of itself and the
laser shot 1/4.

Ported as a per-frame sweep with `state` holding C, which is how every
other multi-frame effect in this queue already works — `SND_LIVE_ADD`,
`SND_BAT_RESIZE_1`, `SND_TRIPLE_BALL`, `SND_ALIEN_BLAST`,
`SND_SPARK_FANOUT` and `SND_MAGNET` are all shaped that way. One beep
per frame rather than the original's back-to-back run is the same
approximation the rest of the queue makes, and it is exactly what WS5's
open decision is about.

### And the primitive had no duration at all

`sound_beep2` ($C136) is speaker ON for B DJNZ turns then OFF for D —
one cycle, so `(B + D) * 13` T-states. The port's `sound_beep2_bd`
passed a literal `1` tick.

`sound_beep_cont_d` was given real duration arithmetic on 2026-08-09 and
this primitive was missed, so two effects kept the one-tick model while
the other eleven got honest lengths. `beep_ticks(1, period)` is exactly
`(B + D) * 13` T-states, since `period` is already `(B + D) / 2` in the
units `beep_period` takes.

### The host test had them filed as "clicks"

`sweeps_last_multiple_frames` keeps two lists: effects that must outlive
their frame, and effects that must not. `SND_BALL_START` and `SND_SHOT`
were in the second, so a one-beep-of-nine effect passed as correct — the
test was describing the PORT's structure rather than the original's.

Both moved, with a note on the clicks list to check the disassembly for
a loop before adding to it. The remaining three genuinely have no loop
in their `play_sound_` routine.

### What this does NOT change

The 20 ms floor. Each beep still rounds up to one 50 Hz tick, so a
nine-beep sweep now takes nine frames where the original takes about
9 * 0.5 ms inside one. The sweep is right; the timebase is still the
open question.

## Every queued effect audited against its routine (2026-08-10)

Finding the LC122 loop by accident was worth asking whether more of the
queue had been flattened. Walked `original/disasm/routines/sound.asm`
routine by routine against `tick_one`.

**No other collapsed loops.** `play_sound_normall_brik`,
`play_sound_metal_brik`, `play_sound_bat_beat` and
`play_sound_bat_resize_2` are a single `sound_beep_cont_d` and a slot
clear. `play_sound_live_add`, `play_sound_08` (SPARK_FANOUT),
`play_sound_bat_resize_1` and `play_sound_triple_ball` are one beep per
frame with their counter in `(IX+$01)`, which is exactly the shape the
port uses. Only `play_sound_LC122` looped, and it is the one that was
wrong.

The audit did turn up two more things.

### live_add played a ninth beep

    play_sound_live_add:
      ... beep when (IX+$01) AND $03 == 0 ...
    play_sound_07_0:
      DEC (IX+$01) / DEC (IX+$01) / RET NZ / LD (IX+$00),$00

DEC, DEC, then test. The port tested for zero BEFORE decrementing, so it
reached state $00 with the counter still live and beeped once more at
the lowest pitch, E = $14. The original's last beep is at state $04:
eight beeps, not nine. Fixed, and the sweep LENGTHS are now asserted —
9 frames for BALL_START, 4 for SHOT, 16 for LIVE_ADD.

### bat_resize_1 has a guard the port does not

    play_sound_bat_resize_1:
      LD A,(bonus_flag) / AND A / RET NZ

With `bonus_flag` non-zero the original skips the beep AND leaves the
counter alone, so the shrink sweep PAUSES while a bonus transition is in
flight. The port has no `bonus_flag`: it is a transient bitfield the
original sets to $80 when a new bonus lands on a machine-gun bat and to
$01 when the machine gun itself is caught (`LA67B_2`, `$A67B`), read
by `handling_bat`'s resize path as well.

Not ported, and not a one-liner — it needs the port to model
`bonus_flag`, which is a bat-state question rather than a sound one.
Recorded here as the last known divergence in the queue.

### A length test does not cover a sweep

Pinning the frame counts left a live mutant: passing a constant `C` to
the beep instead of the counter keeps all nine frames and flattens the
pitch to one note. `sweeps_last_multiple_frames` now also requires the
period to CHANGE across the sweep, which catches it.

Worth remembering — "it runs for nine frames" and "it is a nine-step
sweep" are different claims, and only the first is obvious to assert.
