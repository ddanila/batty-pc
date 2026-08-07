/* Host-side test for src/rng.cpp — the byte-exact RNG walk.
 *
 * The ground truth is the ORIGINAL's own sequence, read from ZEsarUX at
 * $8D48/$8D4A on successive frames from the L3 `l3-brick-flash` state
 * (see notes/rng-model.md). Seeded to the original's frame-0 value, our
 * walk must reproduce it exactly — that is the prerequisite for every
 * RNG-dependent parity claim: enemy steering, bonus drops, magnet coins.
 *
 * This replaces a ~10 s QEMU boot with a few microseconds, and unlike the
 * boot it can afford to walk thousands of ticks looking for divergence. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../src/rng.cpp"

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

/* The $8000-$9FFF image the walk reads. Shipped as an asset because the
 * sequence is a function of these bytes. */
static u8 rom[RANDOM_ROM_SIZE];

static bool load_rom(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t n = fread(rom, 1, sizeof(rom), f);
    fclose(f);
    return n == sizeof(rom);
}

/* Original, captured at each $BA83 frame boundary. random_number is D:E. */
struct Frame { u16 number; u16 seed; };
static const Frame ORIGINAL_WALK[] = {
    { 0x3793, 0x962A },   /* f0 — the seeded start */
    { 0xBB53, 0x962B },
    { 0x460D, 0x962C },
    { 0x0990, 0x962D },
    { 0x6A76, 0x962E },
};
static const int WALK_FRAMES = int(sizeof(ORIGINAL_WALK) / sizeof(ORIGINAL_WALK[0]));

static void test_matches_original_walk() {
    printf("  %-28s ", "matches_original_walk");
    rng_seed(ORIGINAL_WALK[0].number, ORIGINAL_WALK[0].seed);
    check(rng_current() == ORIGINAL_WALK[0].number,
          "f0 number: got %04X want %04X\n", rng_current(), ORIGINAL_WALK[0].number);
    check(rng_seed_addr() == ORIGINAL_WALK[0].seed,
          "f0 seed: got %04X want %04X\n", rng_seed_addr(), ORIGINAL_WALK[0].seed);
    for (int f = 1; f < WALK_FRAMES; f++) {
        /* No keys held during the capture, so the control byte is 0. */
        const u16 got = rng_next(0);
        check(got == ORIGINAL_WALK[f].number,
              "f%d number: got %04X want %04X\n", f, got, ORIGINAL_WALK[f].number);
        check(rng_seed_addr() == ORIGINAL_WALK[f].seed,
              "f%d seed: got %04X want %04X\n", f, rng_seed_addr(), ORIGINAL_WALK[f].seed);
    }
    printf("%s\n", failures ? "FAIL" : "5 frames vs original   ok");
}

/* The walk address must never leave $8000-$9FFF: outside it, the read
 * would index past the shipped image. */
static void test_seed_addr_stays_in_image() {
    const int before = failures;
    printf("  %-28s ", "seed_addr_stays_in_image");
    rng_seed(0x0000, 0x8000);
    int escapes = 0;
    for (long i = 0; i < 200000; i++) {
        rng_next(u8(i));
        const u16 a = rng_seed_addr();
        if (a < 0x8000 || a > 0x9FFF) escapes++;
    }
    check(escapes == 0, "walk left $8000-$9FFF %d times\n", escapes);
    /* Seeding out of range must be clamped in, not accepted. */
    rng_seed(0, 0x0000);
    check(rng_seed_addr() >= 0x8000 && rng_seed_addr() <= 0x9FFF,
          "seeding 0x0000 left the address at %04X\n", rng_seed_addr());
    printf("%s\n", failures > before ? "FAIL" : "200k ticks             ok");
}

/* Control-button input is folded in, so it must actually change the
 * sequence — if it were dropped, replays with input would silently
 * diverge from the original. */
static void test_ctrl_buttons_perturb() {
    const int before = failures;
    printf("  %-28s ", "ctrl_buttons_perturb");
    rng_seed(0x3793, 0x962A);
    const u16 quiet = rng_next(0);
    rng_seed(0x3793, 0x962A);
    const u16 pressed = rng_next(0x01);
    check(quiet != pressed, "control byte did not affect the tick (%04X)\n", quiet);
    check(u16(quiet + 1) == pressed,
          "control byte must add into the low half: %04X vs %04X\n", quiet, pressed);
    printf("%s\n", failures > before ? "FAIL" : "folded into low half   ok");
}

/* Reading must not advance — the original ticks once per frame and every
 * consumer then reads the same value. Advancing on read consumes the
 * sequence faster than the original and desynchronises everything after
 * it (notes/rng-model.md). */
static void test_current_does_not_advance() {
    const int before = failures;
    printf("  %-28s ", "current_does_not_advance");
    rng_seed(0x3793, 0x962A);
    const u16 a = rng_current();
    for (int i = 0; i < 10; i++) rng_current();
    check(rng_current() == a, "rng_current() advanced the sequence\n");
    check(rng_seed_addr() == 0x962A, "rng_current() moved the walk address\n");
    printf("%s\n", failures > before ? "FAIL" : "read is pure           ok");
}

int main(int argc, char **argv) {
    const char *rom_path = argc > 1 ? argv[1] : "assets/random_seed.bin";
    printf("rng tests\n");
    if (!load_rom(rom_path)) {
        printf("  cannot read %s — run `make assets` first\n", rom_path);
        return 1;
    }
    rng_set_rom(rom);

    test_matches_original_walk();
    test_seed_addr_stays_in_image();
    test_ctrl_buttons_perturb();
    test_current_does_not_advance();

    printf("\n%s\n", failures ? "FAILED" : "4 tests, 0 failed");
    return failures ? 1 : 0;
}
