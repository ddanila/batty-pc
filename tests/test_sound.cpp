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

static int tests_run = 0;

static void report(const char *name, int before, const char *detail) {
    tests_run++;
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
    /* BALL_START and SHOT joined this list on 2026-08-10. They are
     * play_sound_LC122 ($C122), which LOOPS `DEC C / JR NZ` — nine
     * beeps and four — and the port had been firing the first one and
     * clearing the slot. They were in the `clicks` list below, which is
     * how a one-beep-of-nine effect passed for correct. */
    const u8 sweeps[] = { SND_ALIEN_BLAST, SND_LIVE_ADD, SND_SPARK_FANOUT,
                          SND_BAT_RESIZE_1, SND_TRIPLE_BALL, SND_MAGNET,
                          SND_BALL_START, SND_SHOT };
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
    /* Genuine one-call effects: their play_sound_ routine has no loop.
     * Check that against the disassembly before adding to this list —
     * BALL_START and SHOT sat here for months on the strength of the
     * PORT's structure rather than the original's. */
    const u8 clicks[] = { SND_NORMAL_BRIK, SND_BAT_BEAT,
                          SND_BAT_RESIZE_2 };
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


/* sound_silence() and sound_stop_all() are one word apart and mean
 * different things. silence stops what is SOUNDING; stop_all also
 * empties the QUEUE. Pick the wrong one and a sound queued before a
 * screen change plays after it — audible, brief, and something nobody
 * would think to file a bug about.
 *
 * play_game_over depends on the difference: it calls sound_stop_all()
 * before drawing, mirroring the original's pause_clear_screen_attrib,
 * which drains the queue while the screen clears. test-game-over checks
 * that the CALL is there. Nothing checked that the call does this. */
static void test_silence_keeps_the_queue_stop_all_empties_it() {
    const int before = failures;

    reset();
    sound_queue(SND_LIVE_ADD);
    sound_queue(SND_MAGNET);
    check(queued_slots() == 2, "setup queued %d, expected 2\n", queued_slots());
    sound_silence();
    check(queued_slots() == 2,
          "sound_silence() emptied the queue (%d slots left); it must only "
          "stop what is sounding\n", queued_slots());
    /* and the queue must still be LIVE, not merely non-zero. How many
     * frames that takes is measured below rather than guessed — the
     * first version of this test used 4 and failed, which says more
     * about the guess than about the code. */
    int frames = 0;
    while (queued_slots() == 2 && frames < 120) { run_frames(1); frames++; }
    check(queued_slots() < 2,
          "after silence the queued sounds never played in 120 frames — "
          "silence must not strand the queue\n");
    /* Measured at 17 frames. The bound is loose on purpose: the point is
     * that the queue still drains, not how fast. The first version of
     * this test asserted 4 frames and failed, which said more about the
     * guess than about the code. */
    check(frames <= 60, "queue took %d frames to drain after silence; "
          "measured 17, so something has changed materially\n", frames);

    reset();
    sound_queue(SND_LIVE_ADD);
    sound_queue(SND_MAGNET);
    sound_stop_all();
    check(queued_slots() == 0,
          "sound_stop_all() left %d queued slot(s) — a sound queued before "
          "the game-over screen would play over it\n", queued_slots());
    const int tones_before = sound_test_tones;
    run_frames(8);
    check(sound_test_tones == tones_before,
          "after stop_all, %d tone(s) still played\n",
          sound_test_tones - tones_before);
    report("silence_vs_stop_all", before, "queue kept / emptied  ok");
}

/* sound_stop_all also silences: it is documented as "the above, and
 * empty the queue", so a held note must not survive it. */
static void test_stop_all_silences_a_held_note() {
    const int before = failures;
    reset();
    sound_queue(SND_BAT_RESIZE_1);
    run_frames(1);
    const int silences_before = sound_test_silences;
    sound_stop_all();
    check(sound_test_silences > silences_before,
          "sound_stop_all() did not turn the speaker off\n");
    check(ticks_left == 0, "a held note survived stop_all (%d ticks)\n",
          (int)ticks_left);
    report("stop_all_silences", before, "speaker off + no hold ok");
}

/* The metal-brick click is its own entry point, not a queue id — the
 * original plays it inline. It must still produce a tone, and must
 * still respect muting like everything else. */
static void test_metal_brik_plays_and_obeys_mute() {
    const int before = failures;
    reset();
    const int tones_before = sound_test_tones;
    sound_play_metal_brik();
    check(sound_test_tones > tones_before,
          "sound_play_metal_brik() produced no tone\n");

    reset();
    sound_set_enabled(false);
    const int muted_tones = sound_test_tones;
    sound_play_metal_brik();
    check(sound_test_tones == muted_tones,
          "sound_play_metal_brik() played %d tone(s) while muted — every "
          "gate runs with sound off, so this path is only ever exercised "
          "here\n", sound_test_tones - muted_tones);
    sound_set_enabled(true);
    report("metal_brik", before, "tone + honours mute   ok");
}

/* D is the envelope's LENGTH, and it used to be thrown away.
 *
 * sound_beep_cont_d(D, E) is D square-wave cycles of half-period E:
 * D * 2 * E * 13 T-states at 3.5 MHz. The port discarded D and held
 * every note for one tick, which at the game's 50 Hz is 20 ms against
 * the original's 3 to 9 ms (notes/sound.md).
 *
 * At 50 Hz the arithmetic still rounds to one tick — that is the clock,
 * not the model, and swapping it is what WS5 needs. This drives the
 * module at a microsecond clock so the durations are visible, and pins
 * the three effects measured against the disassembly. */
static void test_beep_duration_follows_d() {
    const int before = failures;
    struct { const char *name; unsigned char d, e; unsigned long us; } cases[] = {
        { "normall_brik", 0x08, 0x44, 4043 },   /* 8 * 2 * 68 * 13 T */
        { "bat_beat",     0x04, 0x66, 3030 },   /* 4 * 2 * 102 * 13 T */
        { "metal_brik",   0x18, 0x30, 8557 },   /* 24 * 2 * 48 * 13 T */
    };
    sound_set_clock_hz(1000000ul);
    for (int i = 0; i < 3; i++) {
        reset();
        sound_beep_cont_d(cases[i].d, cases[i].e);
        /* Hold until the module releases the speaker, counting ticks. */
        unsigned long held = 0;
        while (sound_test_silences == 0 && held < 100000ul) {
            fake_clock++;
            held++;
            sound_tick();
        }
        const long slack = (long)cases[i].us / 50 + 2;   /* 2% + rounding */
        const long diff = (long)held - (long)cases[i].us;
        check(diff <= slack && diff >= -slack,
              "%s held %lu us, expected ~%lu (D=%02X E=%02X)\n",
              cases[i].name, held, cases[i].us,
              cases[i].d, cases[i].e);
    }
    /* Back to the game's rate, and there every effect is one tick —
     * which is the gap, stated as an assertion so it cannot be believed
     * fixed while it is not. */
    sound_set_clock_hz(50);
    reset();
    sound_beep_cont_d(0x08, 0x44);
    fake_clock++;
    sound_tick();
    check(sound_test_silences == 1,
          "at 50 Hz a 4 ms effect should still round to one 20 ms tick\n");
    report("beep_duration_follows_d", before, "3 envelopes + the 50 Hz floor");
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
    test_silence_keeps_the_queue_stop_all_empties_it();
    test_stop_all_silences_a_held_note();
    test_metal_brik_plays_and_obeys_mute();
    test_beep_duration_follows_d();
    printf("\n%d tests, %d failed\n", tests_run, failures);
    return failures ? 1 : 0;
}
