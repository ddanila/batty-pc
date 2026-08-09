/* Host tests for src/replay_parse.cpp — the BATTY_REPLAY_* value
 * formats.
 *
 * These matter more than their size suggests. Every gate seeds its
 * scenario through one of these strings, and a parser that half-accepts
 * a malformed value would leave the game in a state nobody asked for —
 * which surfaces as a failing gate blaming the game. So the property
 * under test is not "does it parse", it is "does a bad value change
 * nothing".
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../src/replay_parse.cpp"

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
    printf("  %-28s %s\n", name, failures == before ? detail : "FAIL");
}

static void test_int_lists_parse() {
    const int before = failures;
    long v[3];

    check(replay_parse_ints("40,40", v, 2) && v[0] == 40 && v[1] == 40,
          "\"40,40\" gave %ld,%ld\n", v[0], v[1]);
    check(replay_parse_ints("5,118,167", v, 3)
          && v[0] == 5 && v[1] == 118 && v[2] == 167,
          "\"5,118,167\" gave %ld,%ld,%ld\n", v[0], v[1], v[2]);
    check(replay_parse_ints("0x10,0x20", v, 2) && v[0] == 16 && v[1] == 32,
          "hex fields gave %ld,%ld\n", v[0], v[1]);
    check(replay_parse_ints("-2,3", v, 2) && v[0] == -2 && v[1] == 3,
          "negative field gave %ld,%ld\n", v[0], v[1]);
    check(replay_parse_ints("7", v, 1) && v[0] == 7, "single field gave %ld\n", v[0]);
    report("int_lists_parse", before, "dec/hex/neg/1..3     ok");
}

/* A malformed value must leave `out` ALONE. A parser that fills the
 * fields it managed before failing would seed half a scenario. */
static void test_bad_int_lists_change_nothing() {
    const int before = failures;
    static const char *bad[] = {
        NULL,        /* the variable is not set at all */
        "",          /* set but empty */
        "40",        /* too few fields */
        "40,",       /* trailing comma, no second field */
        "40;40",     /* wrong separator */
        "x,40",      /* first field not a number */
        "40,x",      /* second field not a number */
        ",40",       /* missing first field */
    };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        long v[2] = { 111, 222 };
        const bool ok = replay_parse_ints(bad[i], v, 2);
        check(!ok, "%s was accepted\n", bad[i] ? bad[i] : "(null)");
        check(v[0] == 111 && v[1] == 222,
              "%s left %ld,%ld behind instead of the caller's values\n",
              bad[i] ? bad[i] : "(null)", v[0], v[1]);
    }
    report("bad_int_lists_change_nothing", before, "8 rejects, out intact ok");
}

static void test_hex_blobs_parse() {
    const int before = failures;
    u8 out[4];
    check(replay_parse_hex_bytes("01A2b3FF", out, 4)
          && out[0] == 0x01 && out[1] == 0xA2 && out[2] == 0xB3 && out[3] == 0xFF,
          "mixed-case blob gave %02X%02X%02X%02X\n",
          out[0], out[1], out[2], out[3]);
    report("hex_blobs_parse", before, "mixed case, 4 bytes  ok");
}

/* Length is exact: a truncated blob must not seed a partial object, and
 * trailing characters must not be skipped. */
static void test_bad_hex_blobs_rejected() {
    const int before = failures;
    static const char *bad[] = {
        NULL,
        "",
        "01A2b3",      /* one byte short */
        "01A2b3FF00",  /* one byte long */
        "01A2b3F",     /* odd number of digits */
        "01A2b3FG",    /* non-hex digit */
        "01A2b3FF ",   /* trailing space */
    };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        u8 out[4] = { 9, 9, 9, 9 };
        check(!replay_parse_hex_bytes(bad[i], out, 4),
              "%s was accepted as 4 bytes\n", bad[i] ? bad[i] : "(null)");
    }
    report("bad_hex_blobs_rejected", before, "7 rejects            ok");
}

/* BATTY_VISUAL_PROBE_FRAMES. The port walks this list by subtracting
 * consecutive entries, so every delta must be positive — a repeated or
 * out-of-order value would stall the run or skip a checkpoint. Those
 * values are DROPPED rather than rejecting the whole list, so a sloppy
 * env still produces a usable ascending sequence. */
static void test_frame_lists_parse() {
    const int before = failures;
    unsigned int f[8];

    check(replay_parse_frame_list("20,40,60", f, 8) == 3
          && f[0] == 20 && f[1] == 40 && f[2] == 60,
          "\"20,40,60\" gave %u,%u,%u\n", f[0], f[1], f[2]);
    check(replay_parse_frame_list("12", f, 8) == 1 && f[0] == 12,
          "single checkpoint gave %u\n", f[0]);
    check(replay_parse_frame_list(" 20 , 40 ", f, 8) == 2
          && f[0] == 20 && f[1] == 40, "spaces were not tolerated\n");
    report("frame_lists_parse", before, "1..3 + spaces        ok");
}

/* Every kept value must be strictly greater than the last, so the
 * deltas the port subtracts are never zero or negative. */
static void test_frame_lists_stay_ascending() {
    const int before = failures;
    unsigned int f[8];
    static const char *specs[] = {
        "20,20,40",     /* repeat */
        "20,10,40",     /* backwards */
        "40,20,60",     /* backwards then recovering */
        "0,20,40",      /* leading 0 is not > the initial prev */
    };
    for (unsigned i = 0; i < sizeof(specs) / sizeof(specs[0]); i++) {
        const int n = replay_parse_frame_list(specs[i], f, 8);
        for (int k = 1; k < n; k++) {
            check(f[k] > f[k - 1],
                  "%s kept %u after %u — the delta would be <= 0\n",
                  specs[i], f[k], f[k - 1]);
        }
        check(n > 0, "%s was reduced to nothing\n", specs[i]);
    }
    check(replay_parse_frame_list(NULL, f, 8) == 0, "NULL gave a list\n");
    check(replay_parse_frame_list("", f, 8) == 0, "empty gave a list\n");
    report("frame_lists_stay_ascending", before, "4 sloppy specs       ok");
}

/* The list is written into a fixed slot array; overrunning it would
 * corrupt whatever follows. */
static void test_frame_lists_respect_max() {
    const int before = failures;
    /* Deliberately roomier than `max`, with sentinels past it. Sizing it
     * to exactly `max` would make an overrun undefined behaviour, and a
     * test that corrupts its own stack cannot report anything — the
     * first version of this test smashed the failure counter and
     * reported success while the parser wrote 6 values into 4 slots. */
    unsigned int f[8];
    for (int i = 0; i < 8; i++) f[i] = 0xDEAD;

    const int n = replay_parse_frame_list("10,20,30,40,50,60", f, 3);
    check(n == 3, "max 3 kept %d values\n", n);
    check(f[0] == 10 && f[1] == 20 && f[2] == 30,
          "max 3 kept %u,%u,%u\n", f[0], f[1], f[2]);
    check(f[3] == 0xDEAD && f[4] == 0xDEAD,
          "wrote past max: slot 3 = %u, slot 4 = %u\n", f[3], f[4]);
    report("frame_lists_respect_max", before, "6 values into 3      ok");
}

int main() {
    printf("replay_parse tests\n");
    test_int_lists_parse();
    test_bad_int_lists_change_nothing();
    test_hex_blobs_parse();
    test_bad_hex_blobs_rejected();
    test_frame_lists_parse();
    test_frame_lists_stay_ascending();
    test_frame_lists_respect_max();
    printf("\n%d tests, %d failed\n", tests_run, failures);
    return failures ? 1 : 0;
}
