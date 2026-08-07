/* sound — PC speaker envelopes and the event queue.
 *
 * orig: sounds_queue $C0B8, play_sounds_queue, play_sound_<event> $C0F3+
 *
 * The Spectrum's beeper is driven by timing loops; a PC's speaker is
 * driven by latching a PIT channel-2 divisor and gating it on. So the
 * envelopes here reproduce the original's *shape* — how many frames a
 * sound lasts and how its pitch walks — rather than its instruction
 * timing, which has no analogue.
 *
 * Events are queued rather than played directly, because several can be
 * triggered in one frame and the speaker is monophonic. Five slots, each
 * holding an event id and its progress; single-frame sounds clear on
 * their first tick, sweeps advance a state byte until exhausted.
 *
 * The clock is injected (sound_set_clock) so the queue and envelopes can
 * be driven from a fake one in tests; the speaker itself is behind
 * __WATCOMC__, the same way zxvga's framebuffer is. */

#ifndef BATTY_SOUND_H
#define BATTY_SOUND_H

#include "types.h"

const int SOUND_SLOTS = 5;

/* Event ids, matching the original's play_sounds_list at $C0BC. */
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

/* One envelope — the magnet zip — is noise and consumes the game's RNG.
 * Injected so a test's sequence is its own. */
void sound_set_random(u8 (*source)());

/* Muting drops queued events on the floor rather than playing silently,
 * which is what the profile and replay runs want. */
void sound_set_enabled(bool on);
bool sound_is_enabled();

/* Queue an event. Brick, bat and shot events collapse: re-queuing one
 * already in flight is ignored, so a frame that destroys six bricks
 * clicks once, as the original does. */
void sound_queue(u8 id);

/* Played directly, not queued: the level-intro metal shimmer runs its own
 * timing loop outside the frame body. orig: play_sound_metal_brik */
void sound_play_metal_brik();

/* Advance every queued event one frame. Call once per game frame. */
void sound_frame();

/* Release the speaker if a sound's duration has expired. Call from any
 * wait loop, so a held note ends on time even when frames are not
 * being produced. */
void sound_tick();

void sound_silence();     /* speaker off now, current sound abandoned */
void sound_stop_all();    /* the above, and empty the queue */

#endif /* BATTY_SOUND_H */
