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
    test_colour_change_does_not_advance();
    test_space_draws_nothing();
    test_bad_glyph_code_is_ignored();
    test_shipped_markup_is_well_formed();
    printf("\n%s\n", failures ? "FAILED" : "6 tests, 0 failed");
    return failures ? 1 : 0;
}
