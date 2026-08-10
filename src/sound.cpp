/* See sound.h. */

#include "sound.h"

#ifdef __WATCOMC__
#  include <conio.h>
#  include <i86.h>
#endif

#ifndef __WATCOMC__
/* Visible to tests; the DOS build has no equivalent. */
unsigned sound_test_last_period = 0;
long     sound_test_tones = 0;
long     sound_test_silences = 0;
#endif

namespace {

bool muted = false;
unsigned long (*clock_now)() = 0;
unsigned long clock_hz = 50;   /* the game's PIT frame counter */
/* One envelope (the magnet zip) is noise, so it consumes the game RNG.
 * Injected rather than reached for, so the sequence a test sees is
 * its own. */
u8 (*random_byte)() = 0;

unsigned int  ticks_left = 0;
unsigned long last_tick  = 0;

struct Slot { u8 id; u8 state; };
Slot queue[SOUND_SLOTS];

#ifdef __WATCOMC__
/* Interrupts are masked around each port pair: a timer tick landing
 * between the two PIT writes would latch half a divisor. */
void speaker_off() {
    _disable();
    outp(0x61, u8(inp(0x61) & 0xFC));
    _enable();
}

void speaker_tone(unsigned period) {
    if (period < 20) period = 20;
    _disable();
    outp(0x43, 0xB6);                 /* counter 2, lo+hi, mode 3 */
    outp(0x42, u8(period & 0xFF));
    outp(0x42, u8((period >> 8) & 0xFF));
    outp(0x61, u8(inp(0x61) | 0x03));
    _enable();
}
#else
/* Host: record what would have been played, so tests can read it back. */
void speaker_off() { sound_test_silences++; }
void speaker_tone(unsigned period) {
    if (period < 20) period = 20;
    sound_test_last_period = period;
    sound_test_tones++;
}
#endif

void sound_start_period(unsigned int period, unsigned int ticks) {
    if (muted || ticks == 0) return;
    speaker_tone(period);
    ticks_left = ticks;
    last_tick = clock_now();
}

static unsigned int beep_ticks(unsigned char d, unsigned char e);

/* orig: sound_beep2 ($C136) — speaker ON for B DJNZ turns, OFF for D.
 * ONE cycle, so the duration is (B + D) * 13 T-states, and the half
 * period is (B + D) / 2 in the same units `beep_period` takes.
 *
 * The duration used to be the literal 1 tick below. `sound_beep_cont_d`
 * was given real arithmetic on 2026-08-09 and this primitive was
 * missed, so the two effects that reach it — BALL_START and SHOT — kept
 * the old one-tick model while every other effect got honest lengths.
 * beep_ticks(1, period) is exactly (B + D) * 13 T-states. */
void sound_beep2_bd(unsigned char b, unsigned char d) {
    unsigned char period = (unsigned char)(((unsigned int)b + (unsigned int)d) / 2u);
    if (period == 0) period = 1;
    sound_start_period((unsigned int)period * 9u, beep_ticks(1, period));
}

/* orig: sound_beep_cont_d ($C25C) — D square-wave cycles of half-period
 * E. One `sound_beep` is one cycle; a DJNZ that loops is 13 T-states,
 * so the whole thing is D * 2 * E * 13 T-states at 3.5 MHz.
 *
 * D used to be discarded here — `(void)d` — and every effect played for
 * one tick. At the game's 50 Hz that is 20 ms against the original's 3
 * to 9 ms. The arithmetic is honest now; what still rounds it to a
 * single tick is the clock rate, not this. */
/* The PIT divisor for half-period E. Original period is proportional to
 * E: 1193180 / (3500000 / (26*E)) = 8.86*E, and 9*E is the close
 * integer form. (26 = 2 * 13, a full cycle of DJNZ turns.) */
static unsigned int beep_period(unsigned char e) {
    return (unsigned int)e * 9u;
}

static unsigned int beep_ticks(unsigned char d, unsigned char e) {
    /* T-states -> microseconds: 3.5 MHz, so us = T * 2 / 7. Done in
     * that order to stay inside 32 bits on a 16-bit target. */
    const unsigned long t_states = (unsigned long)d * 2ul
                                 * (unsigned long)e * 13ul;
    const unsigned long us = t_states * 2ul / 7ul;
    unsigned long ticks = us * clock_hz / 1000000ul;
    if (ticks == 0) ticks = 1;          /* never silent */
    if (ticks > 60000ul) ticks = 60000ul;
    return (unsigned int)ticks;
}

void sound_beep_cont_d(unsigned char d, unsigned char e) {
    sound_start_period(beep_period(e), beep_ticks(d, e));
}

void sound_beep_cont_de(unsigned char d, unsigned char e) {
    /* orig $C263: same shape, with its own tail. The duration model is
     * the one above. */
    sound_start_period(beep_period(e), beep_ticks(d, e));
}

void sound_play_lc122(unsigned char c, unsigned char e) {
    unsigned char a = (unsigned char)(c ^ e);
    unsigned char b = (unsigned char)((a << 1) & 0x0C);
    unsigned char d = (unsigned char)((a << 1) & 0x0F);
    sound_beep2_bd((unsigned char)(b + 0x08), d);
}

int tick_one(Slot *s) {
    switch (s->id) {
        case SND_NORMAL_BRIK:
            /* $C0F3: D=$08,E=$44. */
            sound_beep_cont_d(0x08, 0x44);
            return 1;

        case SND_BAT_BEAT:
            /* $C16F: D=$04,E=$66. */
            sound_beep_cont_d(0x04, 0x66);
            return 1;

        case SND_LIVE_ADD: {
            /* $C1CF: state starts $20, every 4th frame plays a beep
             * at E = state + $14 (ascending pitch as state shrinks).
             * state -= 2 per frame; cleared when 0. */
            if ((s->state & 3) == 0) {
                sound_beep_cont_d(0x03, (unsigned char)(s->state + 0x14));
            }
            /* DEC / DEC / RET NZ — the original decrements FIRST and
             * clears the slot only when the counter reaches zero, so
             * state $00 is never reached with a beep. Testing for zero
             * before the decrement (as this did) added a ninth beep at
             * the lowest pitch, E = $14. Eight is right. */
            s->state -= 2;
            return s->state == 0;
        }

        /* play_sound_LC122 ($C122) is a LOOP, and the port used to play
         * one beep of it:
         *
         *     play_sound_LC122:
         *       ... derive B,D from (C XOR E) ... CALL sound_beep2
         *       DEC C / JR NZ,play_sound_LC122
         *
         * C counts DOWN, and B and D are recomputed from `C XOR E` each
         * turn, so it is a descending sweep of C beeps — nine for
         * BALL_START, four for SHOT. The port fired the first one and
         * cleared the slot, dropping 8/9 and 3/4 of each effect.
         *
         * Ported as a per-frame sweep with `state` holding C, which is
         * how every other multi-frame effect here already works. It is
         * one beep per frame instead of the original's back-to-back
         * run — the same approximation the rest of the queue makes, and
         * the one WS5's open decision is about. */
        case SND_BALL_START: {
            /* $C116: C=$09, E=$14. */
            sound_play_lc122(s->state, 0x14);
            if (s->state <= 1) return 1;
            s->state--;
            return 0;
        }

        case SND_SHOT: {
            /* $C235: C=$04, E=$0F. */
            sound_play_lc122(s->state, 0x0F);
            if (s->state <= 1) return 1;
            s->state--;
            return 0;
        }

        case SND_BAT_RESIZE_1: {
            /* $C200: state starts $C0 (from the bonus_resize push at
             * \$3212), decrements by $0B per frame until below $10. */
            sound_beep_cont_d(0x01, s->state);
            if (s->state < 0x10 + 0x0B) return 1;
            s->state -= 0x0B;
            return 0;
        }

        case SND_TRIPLE_BALL: {
            /* $C21D: state starts $10 (from the LA67B_8 push at
             * \$3072), increments by $0B per frame until past $C0. */
            sound_beep_cont_d(0x01, s->state);
            if (s->state >= 0xC1 - 0x0B) return 1;
            s->state += 0x0B;
            return 0;
        }

        case SND_ALIEN_BLAST: {
            /* $C1A8: state starts $30, each frame plays a noisy tone at
             * E = (random & $3F) + state with D=1; state += 8; wraps
             * from $60 to $21 once; stops at $A1. ~22 frames of zip-
             * style noise. */
            unsigned int e = (unsigned int)(random_byte() & 0x3F) + (unsigned int)s->state;
            sound_beep_cont_d(0x01, (unsigned char)e);
            s->state = (unsigned char)(s->state + 8);
            if (s->state == 0x60) s->state = 0x21;
            if (s->state == 0xA1) return 1;
            return 0;
        }

        case SND_SPARK_FANOUT: {
            /* $C1ED: E = ((state >> 2) & $3F) + $20, D=2. */
            unsigned int e = (((unsigned int)s->state >> 2) & 0x3Fu) + 0x20u;
            sound_beep_cont_de(0x02, (unsigned char)e);
            s->state++;
            return s->state == 0xA1;
        }

        case SND_BAT_RESIZE_2: {
            /* $C241: D=$0A,E=$30. */
            sound_beep_cont_de(0x0A, 0x30);
            return 1;
        }

        case SND_MAGNET: {
            /* play_sound_magnet ($C151): a blocking 24-step sweep —
             * per-beep period C climbs from $18 (descending pitch) over
             * E=$18 iterations via sound_beep2. Approximated as a queued
             * per-frame descending sweep, consistent with the rest of
             * the PC-speaker layer (see parity-gaps.md). */
            sound_beep_cont_d(0x02, s->state);
            s->state += 4;
            return s->state >= 0x18 + 0x60;
        }

        default:
            return 1;
    }
}

}  /* namespace */

void sound_tick() {
    unsigned long now;
    if (muted || ticks_left == 0) return;
    now = clock_now();
    if (now == last_tick) return;
    last_tick = now;
    if (--ticks_left == 0) speaker_off();
}

/* --- Sound queue (port of sounds_queue at $C0B8 + play_sounds_queue) ---
 *
 * 5 slots, each tracks a sound id + per-sound state byte. snd_q_push
 * adds an event; snd_q_tick is called from the 50 Hz frame body and
 * dispatches each active slot to its play_sound_<id> handler.
 * Single-shot sounds clear their slot on first tick; multi-frame
 * sounds (live-add ascending sweep, ball-launch / shot descending
 * sweep) advance state per frame and clear when exhausted.
 *
 * Each handler mirrors the D/E/B/C parameters of the original's
 * play_sound_<event> routine (sound.asm at $C0F3+). */

void sound_play_metal_brik() { sound_beep_cont_d(0x18, 0x30); }

void sound_queue(u8 id) {
    int i;
    if (muted) return;
    if (id == SND_NORMAL_BRIK || id == SND_BAT_BEAT || id == SND_SHOT) {
        for (i = 0; i < SOUND_SLOTS; i++) {
            if (queue[i].id == id) return;
        }
    }
    for (i = 0; i < SOUND_SLOTS; i++) {
        if (queue[i].id == 0) {
            queue[i].id = id;
            switch (id) {
                case SND_LIVE_ADD:    queue[i].state = 0x20; break;
                case SND_BALL_START:  queue[i].state = 0x09; break;  /* C at $C116 */
                case SND_SHOT:        queue[i].state = 0x04; break;  /* C at $C235 */
                case SND_SPARK_FANOUT:queue[i].state = 0x3D; break;  /* LBC10 push */
                case SND_BAT_RESIZE_1:queue[i].state = 0xC0; break;  /* matches \$3212 push */
                case SND_TRIPLE_BALL: queue[i].state = 0x10; break;  /* matches \$3072 push */
                case SND_ALIEN_BLAST: queue[i].state = 0x30; break;
                case SND_MAGNET:      queue[i].state = 0x18; break;  /* C starts $18 */
                default:              queue[i].state = 0; break;
            }
            return;
        }
    }
}

/* Returns 1 when the slot should be cleared (sound done). */
void sound_frame() {
    int i;
    for (i = 0; i < SOUND_SLOTS; i++) {
        if (queue[i].id == 0) continue;
        if (tick_one(&queue[i])) queue[i].id = 0;
        break;      /* PC speaker is one voice; avoid N port-programs/frame. */
    }
}

void sound_stop_all() {
    int i;
    for (i = 0; i < SOUND_SLOTS; i++) queue[i].id = 0;
    sound_silence();
}

void sound_set_clock(unsigned long (*now)()) { clock_now = now; }
void sound_set_clock_hz(unsigned long hz) { if (hz) clock_hz = hz; }
void sound_set_random(u8 (*source)()) { random_byte = source; }

void sound_set_enabled(bool on) { muted = !on; }
bool sound_is_enabled() { return !muted; }

void sound_silence() {
    speaker_off();
    ticks_left = 0;
}
