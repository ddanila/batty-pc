/* sound — PC speaker envelopes and the event queue.
 *
 * orig: sounds_queue $C0B8, play_sounds_queue, play_sound_<event> $C0F3+
 *
 * The Spectrum's beeper is driven by timing loops; a PC's speaker is driven
 * by latching a PIT channel-2 divisor and gating it on. So the envelopes
 * here reproduce the original's *shape* — how many frames a sound lasts and
 * how its pitch walks — rather than its instruction timing, which has no
 * analogue.
 *
 * Five slots, as in the original: `sounds_queue` is 5 rows of 7 bytes, and
 * play_sounds_queue walks all five each frame while get_free_sound_slot
 * only ALLOCATES from the first four; the fifth is written directly by
 * LAFFC_37. */

#ifndef BATTY_SOUND_H
#define BATTY_SOUND_H

#include "types.h"

const int SOUND_SLOTS = 5;

/* Event ids = POSITIONS in the original's play_sounds_list ($C0BC).
 * play_selected_sound indexes that table with the id, so these are not
 * arbitrary names — `test-sound-ids` checks each against the table.
 *
 * SND_MAGNET is the exception and is deliberately past the end. The
 * original never queues it: magnets.asm finishes its draw with a plain
 * `CALL play_sound_magnet`, synchronously, and the table's $0D slot is
 * commented out as unused. Routing it through the queue is the port's own
 * convention. */
const u8 SND_NORMAL_BRIK  = 0x01;
const u8 SND_BAT_BEAT     = 0x03;
const u8 SND_BALL_START   = 0x04;
const u8 SND_ALIEN_BLAST  = 0x06;
const u8 SND_LIVE_ADD     = 0x07;
const u8 SND_SPARK_FANOUT = 0x08;
const u8 SND_BAT_RESIZE_1 = 0x09;
const u8 SND_TRIPLE_BALL  = 0x0A;
const u8 SND_SHOT         = 0x0B;
const u8 SND_BAT_RESIZE_2 = 0x0C;
const u8 SND_MAGNET       = 0x0D;

/* Where "now" comes from — the 50 Hz PIT counter in the game, anything
 * monotonic in a test. Must be set before the first sound. */
void sound_set_clock(unsigned long (*now)());

/* How many clock ticks make a second — the rate of the function above.
 * Defaults to 50, the game's PIT frame counter.
 *
 * It exists so DURATION can be expressed at all. The original's envelopes
 * are `D` square-wave cycles of half-period `E`, which is `D * 2 * E * 13`
 * T-states at 3.5 MHz — 3 to 9 ms for the effects measured
 * (notes/sound.md). At 50 Hz every one of those rounds to a single tick of
 * 20 ms, so the port holds each note about five times too long. That is the
 * whole of WS5's remaining gap.
 *
 * Setting a finer rate here is what fixes it; the envelope arithmetic below
 * already computes the real duration and converts through this. */
void sound_set_clock_hz(unsigned long hz);

/* One envelope — the magnet zip — is noise and consumes the game's RNG.
 * Injected so a test's sequence is its own. */
void sound_set_random(u8 (*source)());

/* Muting drops queued events on the floor rather than playing silently,
 * which is what the profile and replay runs want. */
void sound_set_enabled(bool on);
bool sound_is_enabled();

/* Queue an event. Brick, bat and shot events collapse: re-queuing one
 * already in flight is ignored, so a frame that destroys six bricks clicks
 * once, as the original does. */
void sound_queue(u8 id);

/* Played directly, not queued: the level-intro metal shimmer runs its own
 * timing loop outside the frame body. orig: play_sound_metal_brik */
void sound_play_metal_brik();

/* Call once per game frame. */
void sound_frame();

/* Release the speaker if a sound's duration has expired. Call from any wait
 * loop, so a held note ends on time even when frames are not being
 * produced. */
void sound_tick();

void sound_silence();     /* speaker off now, current sound abandoned */
void sound_stop_all();    /* the above, and empty the queue */

#endif /* BATTY_SOUND_H */
