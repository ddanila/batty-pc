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

static void report(const char *name, int before, const char *detail) {
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

int main() {
    printf("replay_parse tests\n");
    test_int_lists_parse();
    test_bad_int_lists_change_nothing();
    test_hex_blobs_parse();
    test_bad_hex_blobs_rejected();
    printf("\n%d tests, %d failed\n", 4, failures);
    return failures ? 1 : 0;
}
