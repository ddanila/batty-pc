/* Host-side tests for src/sound.cpp — the event queue and envelopes.
 *
 * Sound is the one subsystem with no visual gate at all: the QEMU harness
 * screenshots frames, so every audio behaviour here has been untested
 * until now. The speaker is behind __WATCOMC__, so on the host a "tone"
 * is a recorded period and these can assert on what would have played.
 *
 * The behaviours that matter are the ones a player notices: a frame that
 * destroys six bricks must click once, not six times; a sweep must run
 * its full length and then stop; and muting must drop events rather than
 * play them silently. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../src/sound.cpp"

static int failures = 0;

static void check(bool ok, const char *fmt, ...) {
    if (ok) return;
    failures++;
    va_list ap;
    va_start(ap, fmt);
    printf("\n    ");
    vprintf(fmt, ap);
    va_end(ap);
}

static void report(const char *name, int before, const char *detail) {
    printf("  %-28s %s\n", name, failures > before ? "FAIL" : detail);
}

/* --- Fakes ------------------------------------------------------------ */

static unsigned long fake_clock = 0;
static unsigned long clock_read() { return fake_clock; }

static u8 fake_rng_state = 0;
static u8 rng_read() { fake_rng_state = u8(fake_rng_state * 37 + 11); return fake_rng_state; }

static void reset() {
    sound_set_clock(clock_read);
    sound_set_random(rng_read);
    sound_set_enabled(true);
    sound_stop_all();
    fake_clock = 0;
    sound_test_tones = 0;
    sound_test_silences = 0;
    sound_test_last_period = 0;
}

/* Run `frames` game frames, advancing the clock each time so held notes
 * can expire. */
static void run_frames(int frames) {
    for (int f = 0; f < frames; f++) {
        sound_frame();
        fake_clock++;
        sound_tick();
    }
}

static int queued_slots() {
    int n = 0;
    for (int i = 0; i < SOUND_SLOTS; i++) if (queue[i].id != 0) n++;
    return n;
}

/* --- Tests ------------------------------------------------------------ */

/* A frame that destroys six bricks must click once. The original
 * collapses repeats of the brick, bat and shot events. */
static void test_repeat_events_collapse() {
    const int before = failures;
    reset();
    for (int i = 0; i < 6; i++) sound_queue(SND_NORMAL_BRIK);
    check(queued_slots() == 1, "six brick events filled %d slots, expected 1\n",
          queued_slots());

    reset();
    for (int i = 0; i < 4; i++) sound_queue(SND_SHOT);
    check(queued_slots() == 1, "four shot events filled %d slots\n", queued_slots());

    /* Distinct events must still coexist. */
    reset();
    sound_queue(SND_NORMAL_BRIK);
    sound_queue(SND_BAT_BEAT);
    sound_queue(SND_BALL_START);
    check(queued_slots() == 3, "three distinct events filled %d slots\n",
          queued_slots());
    report("repeat_events_collapse", before, "brick/shot/bat       ok");
}

/* Every event must eventually leave the queue — a stuck slot would
 * permanently occupy one of only five. */
static void test_every_event_terminates() {
    const int before = failures;
    const u8 ids[] = { SND_NORMAL_BRIK, SND_BAT_BEAT, SND_BALL_START,
                       SND_ALIEN_BLAST, SND_LIVE_ADD, SND_SPARK_FANOUT,
                       SND_BAT_RESIZE_1, SND_TRIPLE_BALL, SND_SHOT,
                       SND_BAT_RESIZE_2, SND_MAGNET };
    for (unsigned i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        reset();
        sound_queue(ids[i]);
        run_frames(200);
        check(queued_slots() == 0, "event %02X still queued after 200 frames\n",
              ids[i]);
    }
    report("every_event_terminates", before, "11 events, 200 frames ok");
}

/* Which events are clicks and which are sweeps is set by their handlers'
 * return values, each citing the original ($C0F3+). Pinning the split
 * here means a handler that stopped advancing its state — or started —
 * fails loudly instead of quietly changing how the game sounds. */
static void test_sweeps_last_multiple_frames() {
    const int before = failures;
    const u8 sweeps[] = { SND_ALIEN_BLAST, SND_LIVE_ADD, SND_SPARK_FANOUT,
                          SND_BAT_RESIZE_1, SND_TRIPLE_BALL, SND_MAGNET };
    for (unsigned i = 0; i < sizeof(sweeps) / sizeof(sweeps[0]); i++) {
        reset();
        sound_queue(sweeps[i]);
        int frames_alive = 0;
        while (queued_slots() && frames_alive < 200) {
            sound_frame();
            fake_clock++;
            sound_tick();
            frames_alive++;
        }
        check(frames_alive > 1, "event %02X finished in %d frame(s)\n",
              sweeps[i], frames_alive);
    }
    /* The clicks must stay clicks. */
    const u8 clicks[] = { SND_NORMAL_BRIK, SND_BAT_BEAT, SND_BALL_START,
                          SND_SHOT, SND_BAT_RESIZE_2 };
    for (unsigned i = 0; i < sizeof(clicks) / sizeof(clicks[0]); i++) {
        reset();
        sound_queue(clicks[i]);
        sound_frame();
        check(queued_slots() == 0, "click %02X outlived its frame\n", clicks[i]);
    }
    report("sweeps_last_multiple_frames", before, "6 sweeps + 5 clicks  ok");
}

/* Muting drops events rather than playing them silently. */
static void test_muting_drops_events() {
    const int before = failures;
    reset();
    sound_set_enabled(false);
    for (int i = 0; i < 5; i++) sound_queue(u8(SND_NORMAL_BRIK + i));
    check(queued_slots() == 0, "%d events queued while muted\n", queued_slots());
    run_frames(20);
    check(sound_test_tones == 0, "%ld tones played while muted\n", sound_test_tones);

    sound_set_enabled(true);
    sound_queue(SND_NORMAL_BRIK);
    check(queued_slots() == 1, "unmuting did not restore queueing\n");
    report("muting_drops_events", before, "queue + speaker      ok");
}

/* The queue is finite: a flood must not corrupt it or wrap. */
static void test_queue_overflow_is_safe() {
    const int before = failures;
    reset();
    for (int i = 0; i < 200; i++) sound_queue(SND_ALIEN_BLAST);
    check(queued_slots() <= SOUND_SLOTS, "%d slots in use, capacity is %d\n",
          queued_slots(), SOUND_SLOTS);
    run_frames(300);
    check(queued_slots() == 0, "queue did not drain after a flood\n");
    report("queue_overflow_is_safe", before, "200 events           ok");
}

/* A held note must be released when its duration expires, even if no
 * further frames are produced — that is what sound_tick is for. */
static void test_held_note_is_released() {
    const int before = failures;
    reset();
    sound_queue(SND_NORMAL_BRIK);
    sound_frame();
    check(sound_test_tones > 0, "the brick event played no tone\n");
    const long silences_before = sound_test_silences;
    for (int i = 0; i < 10; i++) { fake_clock++; sound_tick(); }
    check(sound_test_silences > silences_before,
          "the speaker was never released after the note expired\n");
    report("held_note_is_released", before, "released on tick     ok");
}

/* Every tone must be a period the PIT can actually latch. */
static void test_periods_are_legal() {
    const int before = failures;
    const u8 ids[] = { SND_NORMAL_BRIK, SND_BAT_BEAT, SND_BALL_START,
                       SND_ALIEN_BLAST, SND_LIVE_ADD, SND_SPARK_FANOUT,
                       SND_BAT_RESIZE_1, SND_TRIPLE_BALL, SND_SHOT,
                       SND_BAT_RESIZE_2, SND_MAGNET };
    int illegal = 0;
    for (unsigned i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        reset();
        sound_queue(ids[i]);
        for (int f = 0; f < 200; f++) {
            sound_frame();
            fake_clock++;
            sound_tick();
            if (sound_test_last_period != 0 &&
                (sound_test_last_period < 20 || sound_test_last_period > 0xFFFF))
                illegal++;
        }
    }
    check(illegal == 0, "%d tones outside the PIT's latchable range\n", illegal);
    report("periods_are_legal", before, "11 events            ok");
}

int main() {
    printf("sound tests\n");
    test_repeat_events_collapse();
    test_every_event_terminates();
    test_sweeps_last_multiple_frames();
    test_muting_drops_events();
    test_queue_overflow_is_safe();
    test_held_note_is_released();
    test_periods_are_legal();
    printf("\n%s\n", failures ? "FAILED" : "7 tests, 0 failed");
    return failures ? 1 : 0;
}
