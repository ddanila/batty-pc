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

### Why the rest is not a one-line fix

20 ms is the port's finest unit. The original's envelopes are sub-frame
— `sound_beep_cont_d` returns after a few thousand T-states, well inside
one interrupt — so reproducing them needs a finer clock than the frame
counter, or a speaker driver that can schedule a stop between frames.

That is the actual content of WS5, and it is a timing-infrastructure
change rather than a table of constants. The constants are already
there, and so is the arithmetic — what is left is `sound_set_clock_hz`
being given something finer than the frame counter, and a speaker
driver that can stop a note between frames.
