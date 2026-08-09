/* Host-side tests for src/assets.cpp.
 *
 * Loading is the one place where a fault is silent: a short read leaves a
 * half-filled buffer that renders as garbage and looks like a blitter bug,
 * which is why every reader here fails rather than truncating. These tests
 * exercise that against real files and against deliberately damaged ones.
 *
 * They also check the shipped assets are the size the game assumes — the
 * kind of mismatch that otherwise shows up as a corrupted level fifteen
 * screens into a run. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../src/assets.cpp"

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

static const char *TMP = "build/test_assets.tmp";

static bool write_file(const char *path, const u8 *bytes, unsigned n) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    const size_t put = fwrite(bytes, 1, n, f);
    fclose(f);
    return put == n;
}

/* A short file must fail outright, leaving no partially-filled buffer for
 * the caller to mistake for data. */
static void test_short_file_fails() {
    const int before = failures;
    u8 src[16];
    for (int i = 0; i < 16; i++) src[i] = u8(i);
    check(write_file(TMP, src, 10), "could not write the fixture\n");

    u8 dest[16];
    memset(dest, 0xAA, sizeof(dest));
    check(!asset_load(TMP, dest, 16), "a 10-byte file satisfied a 16-byte load\n");

    check(asset_load(TMP, dest, 10), "an exact-size load of the same file failed\n");
    check(memcmp(dest, src, 10) == 0, "the exact-size load returned wrong bytes\n");
    report("short_file_fails", before, "10 of 16 bytes       ok");
}

static void test_missing_file_fails() {
    const int before = failures;
    u8 dest[4];
    unsigned size = 999;
    check(!asset_load("build/definitely-not-here.bin", dest, 4),
          "a missing file reported success\n");
    check(!asset_load_variable("build/definitely-not-here.bin", dest, 4, &size),
          "a missing file reported success (variable)\n");
    check(!asset_load_chunked("build/definitely-not-here.bin", dest, 4, dest, 4),
          "a missing file reported success (chunked)\n");
    report("missing_file_fails", before, "all three readers    ok");
}

/* Chunked reading must produce exactly what a single read would, for any
 * scratch size — including sizes that do not divide the total. */
static void test_chunked_matches_whole() {
    const int before = failures;
    u8 src[300];
    for (int i = 0; i < 300; i++) src[i] = u8(i * 7 + 3);
    check(write_file(TMP, src, sizeof(src)), "could not write the fixture\n");

    u8 whole[300];
    check(asset_load(TMP, whole, sizeof(whole)), "whole-file load failed\n");

    int mismatches = 0;
    for (unsigned scratch_size = 1; scratch_size <= 64; scratch_size++) {
        u8 scratch[64];
        u8 chunked[300];
        memset(chunked, 0, sizeof(chunked));
        if (!asset_load_chunked(TMP, chunked, sizeof(chunked), scratch, scratch_size)
            || memcmp(chunked, whole, sizeof(whole)) != 0) mismatches++;
    }
    check(mismatches == 0, "%d scratch sizes disagreed with a whole-file read\n",
          mismatches);
    report("chunked_matches_whole", before, "scratch 1..64        ok");
}


/* A truncated file must fail the CHUNKED path too.
 *
 * test_short_file_fails covers asset_load. asset_load_chunked has its
 * own short-read branch, and it had no test: mutating its
 * `return false` to `return true` left the suite green. The shipped
 * assets go through the chunked path (they are larger than the scratch
 * buffer), so a truncated SPRITES.BIN would have loaded "successfully"
 * with the tail left as whatever was in dest.
 *
 * Run across scratch sizes so the failure is caught whether the short
 * read lands on the first chunk or a later one. */
static void test_chunked_short_file_fails() {
    const int before = failures;
    u8 src[300];
    for (int i = 0; i < 300; i++) src[i] = u8(i * 3 + 1);
    check(write_file(TMP, src, 200), "could not write the fixture\n");

    int wrongly_ok = 0;
    for (unsigned scratch_size = 1; scratch_size <= 64; scratch_size++) {
        u8 scratch[64];
        u8 dest[300];
        memset(dest, 0xAA, sizeof(dest));
        if (asset_load_chunked(TMP, dest, sizeof(dest), scratch, scratch_size))
            wrongly_ok++;
    }
    check(wrongly_ok == 0,
          "%d scratch sizes accepted a 200-byte file for a 300-byte load\n",
          wrongly_ok);

    /* and the exact size still succeeds, so this is not just "always fail" */
    u8 scratch[64];
    u8 dest[200];
    check(asset_load_chunked(TMP, dest, sizeof(dest), scratch, 64),
          "an exact-size chunked load of the same file failed\n");
    check(memcmp(dest, src, 200) == 0,
          "the exact-size chunked load returned wrong bytes\n");
    report("chunked_short_file_fails", before, "scratch 1..64        ok");
}

/* Variable-length loads report what actually arrived. */
static void test_variable_reports_size() {
    const int before = failures;
    u8 src[40];
    for (int i = 0; i < 40; i++) src[i] = u8(0x80 + i);
    check(write_file(TMP, src, sizeof(src)), "could not write the fixture\n");

    u8 dest[100];
    unsigned size = 0;
    check(asset_load_variable(TMP, dest, sizeof(dest), &size), "load failed\n");
    check(size == 40, "reported %u bytes, file holds 40\n", size);
    check(memcmp(dest, src, 40) == 0, "bytes differ from the file\n");

    /* Capacity is a ceiling, not a requirement. */
    check(asset_load_variable(TMP, dest, 16, &size), "capped load failed\n");
    check(size == 16, "capped load reported %u, expected 16\n", size);

    /* An empty file is a failure — there is no useful zero-length asset. */
    check(write_file(TMP, src, 0), "could not write the empty fixture\n");
    check(!asset_load_variable(TMP, dest, sizeof(dest), &size),
          "an empty file reported success\n");
    report("variable_reports_size", before, "size + cap + empty   ok");
}

/* The high-score file predates its own name field: 4-byte files must still
 * load, leaving the caller's default name in place. */
static void test_high_score_roundtrip() {
    const int before = failures;
    const u8 name[3] = { 0x11, 0x22, 0x33 };
    high_score_save(0x12345678UL, name);

    unsigned long score = 0;
    u8 got[3] = { 0x0A, 0x0A, 0x0A };
    high_score_load(&score, got);
    check(score == 0x12345678UL, "score round-tripped as %08lX\n", score);
    check(got[0] == 0x11 && got[1] == 0x22 && got[2] == 0x33,
          "name round-tripped as %02X %02X %02X\n", got[0], got[1], got[2]);

    /* A pre-name 4-byte file: score loads, name is left alone. */
    const u8 old_file[4] = { 0x40, 0x30, 0x20, 0x10 };
    check(write_file(HIGH_SCORE_FILE, old_file, sizeof(old_file)),
          "could not write the legacy fixture\n");
    u8 defaults[3] = { 0x0A, 0x0A, 0x0A };
    score = 0;
    high_score_load(&score, defaults);
    check(score == 0x10203040UL, "legacy score loaded as %08lX\n", score);
    check(defaults[0] == 0x0A && defaults[1] == 0x0A && defaults[2] == 0x0A,
          "legacy file overwrote the default name\n");

    /* A missing file must leave the caller's values untouched. */
    remove(HIGH_SCORE_FILE);
    unsigned long keep = 0xABCDEF;
    u8 keep_name[3] = { 1, 2, 3 };
    high_score_load(&keep, keep_name);
    check(keep == 0xABCDEF, "a missing file changed the score to %08lX\n", keep);
    check(keep_name[0] == 1, "a missing file changed the name\n");
    report("high_score_roundtrip", before, "v2 + legacy + absent ok");
}

/* The shipped assets must be the size the game's buffers assume. */
struct ShippedAsset { const char *path; unsigned size; };
static const ShippedAsset SHIPPED[] = {
    { "assets/font.bin",        43 * 6 },
    { "assets/levels.bin",      15 * 12 * 15 },
    { "assets/level_attrs.bin", 15 * 24 * 32 },
    { "assets/random_seed.bin", 0x2000 },
    { "assets/hud_sprites.bin", 0x0128 },
    { "assets/sprites.bin",     0x12BA },
};

static void test_shipped_assets_are_the_expected_size() {
    const int before = failures;
    int checked = 0;
    for (unsigned i = 0; i < sizeof(SHIPPED) / sizeof(SHIPPED[0]); i++) {
        FILE *f = fopen(SHIPPED[i].path, "rb");
        if (!f) continue;              /* not built yet — `make assets` */
        fseek(f, 0, SEEK_END);
        const long actual = ftell(f);
        fclose(f);
        check(actual == long(SHIPPED[i].size), "%s is %ld bytes, game expects %u\n",
              SHIPPED[i].path, actual, SHIPPED[i].size);
        checked++;
    }
    if (checked == 0) {
        printf("  %-28s SKIP (run `make assets`)\n", "shipped_assets_sizes");
        return;
    }
    report("shipped_assets_sizes", before, "6 shipped blobs      ok");
}

int main() {
    printf("assets tests\n");
    test_short_file_fails();
    test_missing_file_fails();
    test_chunked_matches_whole();
    test_chunked_short_file_fails();
    test_variable_reports_size();
    test_high_score_roundtrip();
    test_shipped_assets_are_the_expected_size();
    remove(TMP);
    printf("\n%d tests, %d failed\n", tests_run, failures);
    return failures ? 1 : 0;
}
