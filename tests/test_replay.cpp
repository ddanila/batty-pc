/* Host tests for src/replay.cpp — the BATTY_REPLAY_* seeders that apply
 * a parsed value to state.
 *
 * replay_parse's tests cover turning a string into numbers. These cover
 * what happens next, and the property is the same one that matters
 * there: a malformed or absent value must change NOTHING. Every gate in
 * the suite sets up its scenario through one of these, so a seeder that
 * half-applies leaves the game in a state nobody asked for — and it
 * surfaces as a failing gate blaming the game, not the seeding.
 *
 * That half-applied case is not hypothetical. replay_parse_ints itself
 * was found writing into the caller's array as it went, so ",40" left a
 * 0 behind; it now fills a scratch buffer and copies out only on
 * success. These tests check the layer above keeps that guarantee.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/replay.cpp"

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
    printf("  %-28s %s\n", name, failures == before ? detail : "FAIL");
}

static void set_env(const char *name, const char *value) {
    if (value) setenv(name, value, 1);
    else       unsetenv(name);
}

/* --- the getenv wrapper ------------------------------------------------ */

static void test_env_ints() {
    const int before = failures;
    long v[2];

    set_env("BATTY_TEST_ENV_INTS", "12,34");
    check(replay_env_ints("BATTY_TEST_ENV_INTS", v, 2) && v[0] == 12 && v[1] == 34,
          "set value gave %ld,%ld\n", v[0], v[1]);

    set_env("BATTY_TEST_ENV_INTS", NULL);
    check(!replay_env_ints("BATTY_TEST_ENV_INTS", v, 2),
          "an UNSET variable must read as no-override, not as zeros\n");

    check(!replay_env_ints("BATTY_NEVER_SET_AT_ALL", v, 2),
          "a name nothing ever set must read as no-override\n");
    report("env_ints", before, "set/unset/absent      ok");
}

/* --- bullet ------------------------------------------------------------ */

static void test_bullet_seed() {
    const int before = failures;
    memset(bullet_active, 0, sizeof(bullet_active));
    memset(bullet_x, 0, sizeof(bullet_x));
    memset(bullet_y, 0, sizeof(bullet_y));
    memset(bullet_frame, 9, sizeof(bullet_frame));

    set_env("BATTY_REPLAY_BULLET", "118,90");
    replay_apply_bullet();
    check(bullet_active[0] == 1 && bullet_x[0] == 118 && bullet_y[0] == 90,
          "seeded slot 0 as active=%d x=%d y=%d\n",
          (int)bullet_active[0], bullet_x[0], bullet_y[0]);
    /* The animation frame must be reset with it — a bullet seeded mid-walk
     * would render a phase the gate did not ask for (known-bugs #10). */
    check(bullet_frame[0] == 0, "bullet_frame[0] = %d, expected 0\n",
          (int)bullet_frame[0]);
    /* Only slot 0. */
    check(bullet_active[1] == 0, "seeding slot 0 also touched slot 1\n");
    report("bullet_seed", before, "slot 0, frame reset   ok");
}

static void test_bullet_bad_value_changes_nothing() {
    const int before = failures;
    memset(bullet_active, 0, sizeof(bullet_active));
    bullet_x[0] = 0x5A5A;
    bullet_y[0] = 0x3C3C;

    static const char *const bad_values[] = {",90", "118", "118,", "x,y", ""};
    for (unsigned i = 0; i < sizeof(bad_values) / sizeof(bad_values[0]); i++) {
        const char *bad = bad_values[i];
        set_env("BATTY_REPLAY_BULLET", bad);
        replay_apply_bullet();
        check(bullet_active[0] == 0 && bullet_x[0] == 0x5A5A
              && bullet_y[0] == 0x3C3C,
              "\"%s\" changed state: active=%d x=%d y=%d\n",
              bad, (int)bullet_active[0], bullet_x[0], bullet_y[0]);
    }
    set_env("BATTY_REPLAY_BULLET", NULL);
    replay_apply_bullet();
    check(bullet_active[0] == 0, "an unset variable activated a bullet\n");
    report("bullet_bad_value", before, "5 bad + unset inert   ok");
}

/* --- blast ------------------------------------------------------------- */

static void test_blast_seed() {
    const int before = failures;
    memset(bullet_blast_ticks, 0, sizeof(bullet_blast_ticks));
    set_env("BATTY_REPLAY_BLAST", "60,70");
    replay_apply_blast();
    check(bullet_blast_x[0] == 60 && bullet_blast_y[0] == 70,
          "blast at %d,%d\n", bullet_blast_x[0], bullet_blast_y[0]);
    /* A seeded blast must start at the FULL duration, or the gate sees a
     * partly-elapsed animation. */
    check(bullet_blast_ticks[0]
          == BULLET_BLAST_FRAMES * BULLET_BLAST_TICKS_PER_FRAME,
          "blast ticks %d, expected %d\n", bullet_blast_ticks[0],
          BULLET_BLAST_FRAMES * BULLET_BLAST_TICKS_PER_FRAME);

    bullet_blast_ticks[0] = 0;
    set_env("BATTY_REPLAY_BLAST", "60");
    replay_apply_blast();
    check(bullet_blast_ticks[0] == 0, "a 1-field value seeded a blast\n");
    set_env("BATTY_REPLAY_BLAST", NULL);
    report("blast_seed", before, "full duration + inert ok");
}

/* --- object descriptor ------------------------------------------------- */

static void test_object_seed() {
    const int before = failures;
    u8 want[sizeof(Object)];
    char hex[sizeof(Object) * 2 + 1];
    for (unsigned i = 0; i < sizeof(Object); i++) {
        want[i] = (u8)(i * 7 + 1);
        snprintf(hex + i * 2, 3, "%02X", want[i]);
    }

    memset(&objects[OBJ_BALL_1], 0, sizeof(Object));
    set_env("BATTY_TEST_OBJ", hex);
    replay_apply_object("BATTY_TEST_OBJ", OBJ_BALL_1);
    check(memcmp(&objects[OBJ_BALL_1], want, sizeof(Object)) == 0,
          "the whole %u-byte descriptor did not round-trip\n",
          (unsigned)sizeof(Object));

    /* One byte short must apply NOTHING, not a truncated descriptor. */
    memset(&objects[OBJ_BALL_1], 0xEE, sizeof(Object));
    hex[sizeof(Object) * 2 - 2] = '\0';
    set_env("BATTY_TEST_OBJ", hex);
    replay_apply_object("BATTY_TEST_OBJ", OBJ_BALL_1);
    u8 untouched[sizeof(Object)];
    memset(untouched, 0xEE, sizeof(untouched));
    check(memcmp(&objects[OBJ_BALL_1], untouched, sizeof(Object)) == 0,
          "a short hex string partly overwrote the descriptor\n");
    set_env("BATTY_TEST_OBJ", NULL);
    report("object_seed", before, "22 bytes, all or none ok");
}

/* --- rng --------------------------------------------------------------- */

static void test_random_seed() {
    const int before = failures;
    rng_seed(0x1111, 0x8000);

    set_env("BATTY_REPLAY_RANDOM", "ABCD");
    set_env("BATTY_REPLAY_RANDOM_SEED", "9F00");
    replay_apply_random();
    check(rng_current() == 0xABCD, "rng value %04X\n", (unsigned)rng_current());
    check(rng_seed_addr() == 0x9F00, "walk position %04X\n",
          (unsigned)rng_seed_addr());

    /* Both are optional and independent: setting only one must leave the
     * other where it was. Seeding the value while clobbering the walk
     * position would break byte-exact RNG parity in a way no gate
     * inspects directly. */
    rng_seed(0x2222, 0x8800);
    set_env("BATTY_REPLAY_RANDOM", "1234");
    set_env("BATTY_REPLAY_RANDOM_SEED", NULL);
    replay_apply_random();
    check(rng_current() == 0x1234 && rng_seed_addr() == 0x8800,
          "value-only seed gave %04X/%04X, expected 1234/8800\n",
          (unsigned)rng_current(), (unsigned)rng_seed_addr());

    rng_seed(0x3333, 0x9100);
    set_env("BATTY_REPLAY_RANDOM", NULL);
    set_env("BATTY_REPLAY_RANDOM_SEED", "9F00");
    replay_apply_random();
    check(rng_current() == 0x3333 && rng_seed_addr() == 0x9F00,
          "walk-only seed gave %04X/%04X, expected 3333/9F00\n",
          (unsigned)rng_current(), (unsigned)rng_seed_addr());

    /* An address outside $8000-$9FFF is MASKED into the window, not
     * clamped and not rejected. rng_seed uses `addr & 0x9FFF`, which is
     * the same operation that makes the walk wrap $9FFF -> $8000 in one
     * step (rng_next does `(addr + 1) & 0x9FFF`). So $A100 lands on
     * $8100, not $A100 and not $9FFF. Written down because the first
     * version of this test expected verbatim storage, and because the
     * comment in replay.cpp called this "the low 14 bits" when the
     * window is 8 KB. */
    rng_seed(0x5555, 0x8000);
    set_env("BATTY_REPLAY_RANDOM_SEED", "A100");
    replay_apply_random();
    check(rng_seed_addr() == 0x8100,
          "out-of-window seed $A100 gave %04X, expected 8100\n",
          (unsigned)rng_seed_addr());

    /* Out of range must be rejected, not truncated. */
    rng_seed(0x4444, 0x8A00);
    set_env("BATTY_REPLAY_RANDOM", "1FFFF");
    set_env("BATTY_REPLAY_RANDOM_SEED", NULL);
    replay_apply_random();
    check(rng_current() == 0x4444, "a >16-bit value was applied: %04X\n",
          (unsigned)rng_current());

    set_env("BATTY_REPLAY_RANDOM", NULL);
    report("random_seed", before, "value/walk independent ok");
}

int main(void) {
    printf("replay:\n");
    test_env_ints();
    test_bullet_seed();
    test_bullet_bad_value_changes_nothing();
    test_blast_seed();
    test_object_seed();
    test_random_seed();
    printf("\n%d tests, %d failed\n", 6, failures);
    return failures == 0 ? 0 : 1;
}
