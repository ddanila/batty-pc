/* Host-side tests for src/hud.cpp — the markup decode and score digits.
 *
 * The markup stream is a format, and formats fail quietly: a
 * misinterpreted byte moves text one cell or drops a record, which a
 * screenshot diff reports as "1,200 pixels differ" without saying why.
 * These check the decode directly, on the real MARKUP.BIN the game ships
 * as well as on hand-built streams for the cases the shipped data does
 * not happen to contain. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../src/hud.cpp"

/* The screen the glyph renderer draws into, and the one zxvga entry point
 * the markup path needs — supplied here so this test links against the
 * hud module alone. */
u8 *vga = 0;
u8 attr_to_palette(u8 attr) { return attr_ink(attr); }
static u8 screen[SCREEN_W * SCREEN_H];

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

static void reset_screen() {
    vga = screen;
    memset(screen, 0, sizeof(screen));
}

/* --- Score ------------------------------------------------------------ */

static void test_score_digits() {
    const int before = failures;
    u8 d[6];

    score_to_digits(0, d);
    for (int i = 0; i < 6; i++)
        check(d[i] == 0, "zero: digit %d is %u\n", i, d[i]);

    score_to_digits(123456, d);
    const u8 want[6] = { 1, 2, 3, 4, 5, 6 };
    for (int i = 0; i < 6; i++)
        check(d[i] == want[i], "123456: digit %d is %u, want %u\n", i, d[i], want[i]);

    /* Leading zeros are kept — the display is fixed-width. */
    score_to_digits(42, d);
    check(d[0] == 0 && d[3] == 0 && d[4] == 4 && d[5] == 2,
          "42 encoded as %u%u%u%u%u%u\n", d[0], d[1], d[2], d[3], d[4], d[5]);

    /* Every digit must be a legal font index for a digit glyph. */
    int illegal = 0;
    for (unsigned long s = 0; s < 200000; s += 37) {
        score_to_digits(s, d);
        for (int i = 0; i < 6; i++) if (d[i] > 9) illegal++;
    }
    check(illegal == 0, "%d digits outside 0..9\n", illegal);
    report("score_digits", before, "0, 42, 123456, 5.4k  ok");
}

/* --- Markup ------------------------------------------------------------ */

static void test_row_marker_rule() {
    const int before = failures;
    check(!is_row_marker(0), "zero must not be a marker\n");
    int wrong = 0;
    for (int b = 1; b < 256; b++)
        if (is_row_marker(u8(b)) != ((b & 7) == 0)) wrong++;
    check(wrong == 0, "%d bytes disagreed with the multiple-of-8 rule\n", wrong);
    report("row_marker_rule", before, "all 256 bytes        ok");
}

/* A record's glyphs must land one cell apart, and an inline colour change
 * must NOT advance the cursor. Getting that wrong shifts every glyph
 * after the first colour change. */
/* render_markup stops AT markup_len, not one past it.
 *
 *     while (p < markup_len)
 *
 * `<=` reads `markup[markup_len]` — a byte the stream does not own. In
 * the shipped build that is the rest of a fixed buffer, so it is stale
 * data rather than an out-of-bounds access, but a row marker or glyph
 * sitting there gets rendered.
 *
 * The existing tests all set `markup_len` to `sizeof(stream)` of a
 * local array and never write past it, so the byte was always whatever
 * the previous test left — usually harmless, never asserted. */
static void test_markup_stops_at_its_length() {
    const int before = failures;
    memset(font, 0xFF, sizeof(font));

    const u8 stream[] = { 0x10, 40, 0x07, 1, 0x01 };
    memcpy(markup, stream, sizeof(stream));
    /* A row marker just past the end. If the loop overruns it starts a
     * whole extra record and draws from it. */
    markup[sizeof(stream) + 0] = 0x10;
    markup[sizeof(stream) + 1] = 80;
    markup[sizeof(stream) + 2] = 0x07;
    markup[sizeof(stream) + 3] = 1;
    markup[sizeof(stream) + 4] = 0x01;
    markup_len = sizeof(stream);

    reset_screen();
    render_markup();

    /* The legitimate record draws around y = 40 - 5; the phantom one
     * would draw around y = 80 - 5. Nothing may appear below y = 60. */
    int below = 0;
    for (int y = 60; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++)
            if (screen[y * SCREEN_W + x]) below++;
    check(below == 0,
          "render_markup ran past markup_len and drew %d pixels from a "
          "record it does not own\n", below);

    report("markup_stops_at_length", before, "no overrun           ok");
}

/* The two markup ranges are inclusive at both ends.
 *
 *     if (c == 0x26)            explicit no-draw
 *     else if (c <= 0x2A)       a GLYPH
 *     else if (c >= 0x40 && c <= 0x4F)  an attribute
 *
 * $2A is the last glyph code and $40/$4F the attribute range's ends.
 * The 2026-08-10 sweep mutated `c <= 0x2A` and `c >= 0x40` and both
 * survived: the shipped markup never uses a boundary code, so the
 * existing tests never produce one.
 *
 * A glyph at $2A must DRAW; an attribute at $40 must change colour
 * without advancing x — the property test_colour_change_does_not_advance
 * checks at $47. */
static void test_markup_range_ends_are_inclusive() {
    const int before = failures;
    memset(font, 0xFF, sizeof(font));

    /* $2A: the last glyph code. It must draw something. */
    const u8 last_glyph[] = { 0x10, 40, 0x07, 1, 0x2A };
    memcpy(markup, last_glyph, sizeof(last_glyph));
    markup_len = sizeof(last_glyph);
    reset_screen();
    render_markup();
    int drawn = 0;
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) if (screen[i]) drawn++;
    check(drawn > 0, "glyph code $2A is the last one and must draw\n");

    /* $40: the first attribute code. "Draws nothing" is NOT enough to
     * pin it — an unrecognised code draws nothing either, by falling
     * off the end of the chain. What separates them is that an
     * attribute does `x -= 8` so it consumes no column, while an
     * unrecognised code still advances. So compare the glyph's landing
     * column with and without the $40 in front of it. */
    const u8 plain[] = { 0x10, 60, 0x07, 1, 0x01 };
    memcpy(markup, plain, sizeof(plain));
    markup_len = sizeof(plain);
    reset_screen();
    render_markup();
    int plain_col = -1;
    for (int x = 0; x < SCREEN_W && plain_col < 0; x++)
        for (int y = 0; y < SCREEN_H; y++)
            if (screen[y * SCREEN_W + x]) { plain_col = x; break; }

    const u8 with_attr[] = { 0x10, 60, 0x07, 2, 0x40, 0x01 };
    memcpy(markup, with_attr, sizeof(with_attr));
    markup_len = sizeof(with_attr);
    reset_screen();
    render_markup();
    int attr_col = -1;
    for (int x = 0; x < SCREEN_W && attr_col < 0; x++)
        for (int y = 0; y < SCREEN_H; y++)
            if (screen[y * SCREEN_W + x]) { attr_col = x; break; }

    check(plain_col >= 0 && attr_col == plain_col,
          "code $40 is the first ATTRIBUTE and must consume no column: "
          "the glyph landed at %d with it and %d without\n",
          attr_col, plain_col);

    report("markup_range_ends", before, "$2A draws, $40 is attr  ok");
}

static void test_colour_change_does_not_advance() {
    const int before = failures;

    /* One glyph per column, all bits set, so any drawn cell is visible. */
    memset(font, 0xFF, sizeof(font));

    /* marker 0x10 -> column 2; y so the record lands on screen. */
    const u8 stream_plain[] = { 0x10, 40, 0x07, 2, 0x01, 0x02 };
    memcpy(markup, stream_plain, sizeof(stream_plain));
    markup_len = sizeof(stream_plain);
    reset_screen();
    render_markup();

    int first_col = -1, last_col = -1;
    for (int x = 0; x < SCREEN_W; x++)
        for (int y = 0; y < SCREEN_H; y++)
            if (screen[y * SCREEN_W + x]) {
                if (first_col < 0) first_col = x;
                last_col = x;
            }
    check(first_col >= 0, "the plain record drew nothing\n");
    const int plain_span = last_col - first_col;

    /* Same two glyphs with a colour change between them. The colour byte
     * occupies a payload slot but must not move the cursor, so the span
     * must be unchanged. */
    const u8 stream_coloured[] = { 0x10, 40, 0x07, 3, 0x01, 0x42, 0x02 };
    memcpy(markup, stream_coloured, sizeof(stream_coloured));
    markup_len = sizeof(stream_coloured);
    reset_screen();
    render_markup();

    first_col = last_col = -1;
    for (int x = 0; x < SCREEN_W; x++)
        for (int y = 0; y < SCREEN_H; y++)
            if (screen[y * SCREEN_W + x]) {
                if (first_col < 0) first_col = x;
                last_col = x;
            }
    check(last_col - first_col == plain_span,
          "a colour change shifted the line: span %d vs %d\n",
          last_col - first_col, plain_span);
    report("colour_change_no_advance", before, "2 glyphs +/- colour  ok");
}

/* 0x26 is an explicit no-draw: the font's space glyph carries stray bits
 * that must not be painted. */
static void test_space_draws_nothing() {
    const int before = failures;
    memset(font, 0xFF, sizeof(font));
    const u8 stream[] = { 0x10, 40, 0x07, 1, 0x26 };
    memcpy(markup, stream, sizeof(stream));
    markup_len = sizeof(stream);
    reset_screen();
    render_markup();
    int painted = 0;
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) if (screen[i]) painted++;
    check(painted == 0, "the space code painted %d pixels\n", painted);
    report("space_draws_nothing", before, "code 0x26            ok");
}

/* Out-of-range glyph codes must draw nothing rather than read past the
 * font — the stream is data, and data can be wrong. */
static void test_bad_glyph_code_is_ignored() {
    const int before = failures;
    memset(font, 0xFF, sizeof(font));
    reset_screen();
    for (int code = FONT_GLYPHS; code < 256; code++)
        draw_glyph(100, 100, 0x0F, u8(code));
    int painted = 0;
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) if (screen[i]) painted++;
    check(painted == 0, "out-of-range codes painted %d pixels\n", painted);
    report("bad_glyph_code_ignored", before, "codes 43..255        ok");
}

/* The shipped stream must decode cleanly: every record's payload has to
 * fit inside the buffer. A record that ran off the end would read
 * whatever followed it in memory. */
static void test_shipped_markup_is_well_formed() {
    const int before = failures;
    int checked = 0;
    const char *files[] = { "assets/markup.bin", "assets/main_menu_markup.bin" };
    for (int f = 0; f < 2; f++) {
        FILE *fp = fopen(files[f], "rb");
        if (!fp) continue;
        markup_len = unsigned(fread(markup, 1, MARKUP_MAX, fp));
        fclose(fp);

        int records = 0, overruns = 0;
        unsigned p = 0;
        while (p < markup_len) {
            if (!is_row_marker(markup[p])) { p++; continue; }
            if (p + 4 > markup_len) { overruns++; break; }
            const unsigned count = markup[p + 3];
            if (p + 4 + count > markup_len) overruns++;
            p += 4 + count;
            records++;
        }
        check(overruns == 0, "%s: %d record(s) run past the end\n",
              files[f], overruns);
        check(records > 0, "%s: no records decoded\n", files[f]);
        checked++;
    }
    if (checked == 0) {
        printf("  %-28s SKIP (run `make assets`)\n", "shipped_markup_well_formed");
        return;
    }
    report("shipped_markup_well_formed", before, "2 shipped streams    ok");
}

int main() {
    printf("hud tests\n");
    test_score_digits();
    test_row_marker_rule();
    test_markup_stops_at_its_length();
    test_markup_range_ends_are_inclusive();
    test_colour_change_does_not_advance();
    test_space_draws_nothing();
    test_bad_glyph_code_is_ignored();
    test_shipped_markup_is_well_formed();
    printf("\n%s\n", failures ? "FAILED" : "6 tests, 0 failed");
    return failures ? 1 : 0;
}
