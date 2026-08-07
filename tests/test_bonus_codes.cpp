/* Host-side tests for src/bonus_codes.cpp.
 *
 * Replays seed a bonus by the ORIGINAL's code and the game then acts on
 * ours, so the two translations must be exact inverses. If they drift,
 * a replay drops the wrong bonus and diverges from that frame on —
 * with nothing in the output pointing at the mapping. */

#include <stdarg.h>
#include <stdio.h>

#include "../src/bonus_codes.cpp"

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

/* Every original code the port supports must survive a round trip. */
static void test_round_trip_from_original() {
    const int before = failures;
    int supported = 0;
    for (int code = 0; code < 256; code++) {
        const u8 ours = bonus_from_original(u8(code));
        if (ours == BONUS_TYPE_UNSUPPORTED) continue;
        supported++;
        check(bonus_to_original(ours) == code,
              "original %02X -> ours %u -> original %02X\n",
              code, ours, bonus_to_original(ours));
    }
    check(supported == BONUS_TYPE_COUNT,
          "%d original codes map to a bonus, the port has %d\n",
          supported, int(BONUS_TYPE_COUNT));
    report("round_trip_from_original", before, "10 of 256 codes      ok");
}

/* And every one of ours must survive the other way. */
static void test_round_trip_from_ours() {
    const int before = failures;
    for (int type = 0; type < BONUS_TYPE_COUNT; type++) {
        const u8 orig = bonus_to_original(u8(type));
        check(orig != BONUS_ORIG_NONE, "our bonus %d has no original code\n", type);
        check(bonus_from_original(orig) == type,
              "ours %d -> original %02X -> ours %u\n",
              type, orig, bonus_from_original(orig));
    }
    report("round_trip_from_ours", before, "10 bonus types       ok");
}

/* The mapping must be injective in both directions — two bonuses
 * sharing a code would make one of them unreachable. */
static void test_mapping_is_injective() {
    const int before = failures;
    int seen[256];
    for (int i = 0; i < 256; i++) seen[i] = 0;
    for (int type = 0; type < BONUS_TYPE_COUNT; type++) {
        const u8 orig = bonus_to_original(u8(type));
        check(seen[orig] == 0, "original code %02X is claimed by two bonuses\n", orig);
        seen[orig] = 1;
    }
    for (int i = 0; i < 256; i++) seen[i] = 0;
    for (int code = 0; code < 256; code++) {
        const u8 ours = bonus_from_original(u8(code));
        if (ours == BONUS_TYPE_UNSUPPORTED) continue;
        check(seen[ours] == 0, "our bonus %u is claimed by two original codes\n", ours);
        seen[ours] = 1;
    }
    report("mapping_is_injective", before, "both directions      ok");
}

/* Unknown codes must be rejected, not guessed at. */
static void test_unknown_codes_rejected() {
    const int before = failures;
    for (int code = 0x0A; code < 256; code++)
        check(bonus_from_original(u8(code)) == BONUS_TYPE_UNSUPPORTED,
              "original %02X mapped to bonus %u instead of UNSUPPORTED\n",
              code, bonus_from_original(u8(code)));
    check(bonus_to_original(BONUS_TYPE_COUNT) == BONUS_ORIG_NONE,
          "an out-of-range bonus produced a real original code\n");
    report("unknown_codes_rejected", before, "246 unknown codes    ok");
}

int main() {
    printf("bonus_codes tests\n");
    test_round_trip_from_original();
    test_round_trip_from_ours();
    test_mapping_is_injective();
    test_unknown_codes_rejected();
    printf("\n%s\n", failures ? "FAILED" : "4 tests, 0 failed");
    return failures ? 1 : 0;
}
