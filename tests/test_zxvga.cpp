/* Host-side tests for the video engine (src/zxvga.cpp).
 *
 * These compile the REAL engine source natively — no DOS, no emulator —
 * the __WATCOMC__ guard in zxvga.c just points `vga` at a plain array. So
 * what is under test is the shipping expansion table and the shipping
 * blit logic, not a model of them.
 *
 * What they lock down, in one sentence: the Spectrum's attribute display
 * model, including colour clash, must survive exactly as-is.
 *
 *   1  pal_tables_exhaustive      attr -> ink/paper for all 256 attrs
 *   2  clash_expansion_vs_ula     every (attr, byte) vs an independent
 *                                 ULA reference written from the
 *                                 hardware spec, not from zxvga.cpp
 *   3  flash_bit_ignored          attr and attr|0x80 render identically
 *   4  blit_stays_in_playfield    the 320x200 border is never touched
 *   5  rect_flush_equiv_full      a rect flush == a full repaint, inside
 *                                 the rect, and a no-op outside it
 *   6  dirty_flush_equiv_full     mark what you changed and the dirty
 *                                 flush is indistinguishable from a full
 *                                 repaint
 *   7  dirty_cell_rect_rounding   attr-writing sprites mark whole cells
 *   8  sprite_blit_preserves_attrs   the CLASH INVARIANT: pixel blits
 *                                 never touch the attribute plane
 *   9  clash_confines_to_cell_pair   after any pixel blitting, every
 *                                 pixel in a cell is that cell's ink or
 *                                 that cell's paper — nothing else
 *  10  attr_blit_recolours_cells  attribute writes take whole 8x8 cells
 *                                 (clash spills to neighbours)
 *  11  golden_original_scr        a real screen from the original game
 *                                 expands to the expected pixels
 *
 * Build: see the test-video target in the Makefile. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/zxvga.cpp"

/* ===================================================================== */
/* Harness                                                               */
/* ===================================================================== */

static int tests_run = 0;
static int tests_failed = 0;
static const char *current_test = "";
static int current_failures = 0;

#define CHECK(cond, ...)                                                  \
    do {                                                                  \
        if (!(cond)) {                                                    \
            if (current_failures < 5) {                                   \
                printf("\n    %s:%d: ", __FILE__, __LINE__);              \
                printf(__VA_ARGS__);                                      \
            }                                                             \
            current_failures++;                                           \
        }                                                                 \
    } while (0)

static void begin(const char *name) {
    current_test = name;
    current_failures = 0;
    printf("  %-30s ", name);
    fflush(stdout);
}

static void end(const char *detail) {
    tests_run++;
    if (current_failures) {
        tests_failed++;
        printf("\n  %-30s %s (%d failures)\n", current_test, "FAIL", current_failures);
    } else {
        printf("%-22s ok\n", detail);
    }
}

/* Deterministic PRNG — a failing case must be reproducible. */
static unsigned long rng_state = 0x13579BDFUL;
static void rng_seed(unsigned long s) { rng_state = s; }
static unsigned rng_next(void) {
    rng_state = rng_state * 1103515245UL + 12345UL;
    return (unsigned)((rng_state >> 16) & 0x7FFF);
}
static int rng_range(int lo, int hi) {   /* inclusive */
    return lo + (int)(rng_next() % (unsigned)(hi - lo + 1));
}

/* ===================================================================== */
/* Independent reference model of the ZX ULA                             */
/* ===================================================================== */
/* Written from the hardware description, deliberately NOT reusing
 * attr_ink/attr_paper/the expansion table — otherwise a bug in those would
 * agree with itself and the test would pass. */

#define REF_ATTR_INK(a)    (((a) & 0x07) | (((a) & 0x40) ? 8 : 0))
#define REF_ATTR_PAPER(a)  ((((a) >> 3) & 0x07) | (((a) & 0x40) ? 8 : 0))

/* Colour of playfield pixel (x, y) given the two planes. Every pixel is
 * decided by ONE bit and the attribute of the 8x8 cell it falls in —
 * that is the whole model, and the reason clash exists. */
static unsigned char ref_pixel(const unsigned char *scr, const unsigned char *attr,
                               int x, int y) {
    unsigned char byte = scr[y * 32 + (x >> 3)];
    unsigned char a    = attr[(y >> 3) * 32 + (x >> 3)];
    int set = (byte >> (7 - (x & 7))) & 1;
    return (unsigned char)(set ? REF_ATTR_INK(a) : REF_ATTR_PAPER(a));
}

/* Address of playfield pixel (x, y) in the mode 13h framebuffer. */
static unsigned char *fb_at(int x, int y) {
    return &vga[(BORDER_Y + y) * SCREEN_W + BORDER_X + x];
}

static void randomize_buffers(void) {
    int i;
    for (i = 0; i < 6144; i++) scr_buff[i] = (unsigned char)rng_range(0, 255);
    for (i = 0; i < 768; i++)  attr_buff[i] = (unsigned char)rng_range(0, 255);
}

/* ===================================================================== */
/* 1. Attribute -> ink/paper tables                                      */
/* ===================================================================== */

static void test_pal_tables_exhaustive(void) {
    int a;
    begin("pal_tables_exhaustive");
    init_pal_tables();
    for (a = 0; a < 256; a++) {
        CHECK(ink_table[a] == REF_ATTR_INK(a),
              "attr 0x%02X ink: got %u want %u\n", a, ink_table[a], REF_ATTR_INK(a));
        CHECK(paper_table[a] == REF_ATTR_PAPER(a),
              "attr 0x%02X paper: got %u want %u\n", a, paper_table[a], REF_ATTR_PAPER(a));
    }
    /* Bright black (8) and black (0) are distinct indices that must both
     * be black in the palette — the visual-regression harness relies on
     * comparing in RGB space because of this. */
    CHECK(zx_palette[0] == 0 && zx_palette[1] == 0 && zx_palette[2] == 0,
          "palette[0] is not black\n");
    CHECK(zx_palette[8 * 3] == 0 && zx_palette[8 * 3 + 1] == 0 && zx_palette[8 * 3 + 2] == 0,
          "palette[8] (bright black) is not black\n");
    end("256 attrs");
}

/* ===================================================================== */
/* 2. The clash expansion, exhaustively                                  */
/* ===================================================================== */

static void test_clash_expansion_vs_ula(void) {
    int a, i, x, y;
    long checked = 0;
    begin("clash_expansion_vs_ula");
    init_pal_tables();
    for (a = 0; a < 256; a++) {
        /* One attribute everywhere, every byte value across the pixels. */
        memset(attr_buff, a, sizeof(attr_buff));
        for (i = 0; i < 6144; i++) scr_buff[i] = (unsigned char)((i + a) & 0xFF);
        buff_to_vga();
        for (y = 0; y < PLAYFIELD_H; y++) {
            for (x = 0; x < PLAYFIELD_W; x++) {
                unsigned char got  = *fb_at(x, y);
                unsigned char want = ref_pixel(scr_buff, attr_buff, x, y);
                CHECK(got == want,
                      "attr 0x%02X px(%d,%d): got %u want %u\n", a, x, y, got, want);
                checked++;
            }
        }
    }
    end("256 attrs x 49152 px");
    (void)checked;
}

/* ===================================================================== */
/* 3. Flash is not emulated                                              */
/* ===================================================================== */

static void test_flash_bit_ignored(void) {
    int a;
    static unsigned char steady[SCREEN_W * SCREEN_H];
    begin("flash_bit_ignored");
    init_pal_tables();
    rng_seed(0xF1A50000UL ^ 0x1234);
    for (a = 0; a < 128; a++) {
        int i;
        for (i = 0; i < 6144; i++) scr_buff[i] = (unsigned char)rng_range(0, 255);
        memset(attr_buff, a, sizeof(attr_buff));
        buff_to_vga();
        memcpy(steady, vga, sizeof(steady));
        memset(attr_buff, a | 0x80, sizeof(attr_buff));   /* same attr, flashing */
        buff_to_vga();
        CHECK(memcmp(steady, vga, sizeof(steady)) == 0,
              "attr 0x%02X renders differently with the flash bit set\n", a);
    }
    end("128 attrs");
}

/* ===================================================================== */
/* 4. Nothing escapes the playfield                                      */
/* ===================================================================== */

static void test_blit_stays_in_playfield(void) {
    int x, y, escaped = 0;
    begin("blit_stays_in_playfield");
    init_pal_tables();
    rng_seed(4242);
    randomize_buffers();
    memset(vga, 0xAA, SCREEN_W * SCREEN_H);   /* sentinel */
    buff_to_vga();
    for (y = 0; y < SCREEN_H; y++) {
        for (x = 0; x < SCREEN_W; x++) {
            int inside = (x >= BORDER_X && x < BORDER_X + PLAYFIELD_W &&
                          y >= BORDER_Y && y < BORDER_Y + PLAYFIELD_H);
            if (!inside && vga[y * SCREEN_W + x] != 0xAA) escaped++;
        }
    }
    CHECK(escaped == 0, "buff_to_vga wrote %d bytes outside the playfield\n", escaped);
    /* And a rect flush must stay inside its own rect as well as the
     * playfield — clamping of out-of-range args included. */
    memset(vga, 0xAA, SCREEN_W * SCREEN_H);
    buff_to_vga_rect_bytes(-20, 400, -5, 99);   /* deliberately out of range */
    escaped = 0;
    for (y = 0; y < SCREEN_H; y++)
        for (x = 0; x < SCREEN_W; x++) {
            int inside = (x >= BORDER_X && x < BORDER_X + PLAYFIELD_W &&
                          y >= BORDER_Y && y < BORDER_Y + PLAYFIELD_H);
            if (!inside && vga[y * SCREEN_W + x] != 0xAA) escaped++;
        }
    CHECK(escaped == 0, "clamped rect flush wrote %d bytes outside the playfield\n", escaped);
    end("borders intact");
}

/* A sprite straddling the LEFT edge still writes byte 0.
 *
 * `blit_masked_to_scr_buff` splits each source byte across two
 * destination bytes when x is not byte-aligned:
 *
 *     const int left  = start_col + col;      // start_col = x_px >> 3
 *     const int right = left + 1;
 *     if (left  >= 0 && left  < BYTES_PER_ROW) ...
 *     if (right >= 0 && right < BYTES_PER_ROW) ...
 *
 * At `x_px = -7` the shift is 1 and `start_col` is -1, so the first
 * source byte has `left = -1` (correctly dropped) and `right = 0` —
 * the row's first byte, which must still be written. `right > 0`
 * silently loses it.
 *
 * x = -8 does NOT exercise this: the shift is 0 and the aligned branch
 * runs instead. The existing clipping test uses -8, which is why this
 * survived. */
static void test_scr_blit_writes_byte_zero_from_the_left(void) {
    int y;
    begin("scr_blit_left_straddle");

    /* ONE byte wide. A 2-byte sprite writes byte 0 from its SECOND
     * column via the `left` guard, which masks the one under test — the
     * first draft did that and the mutant survived. With a single
     * column, `right` is the only path to byte 0. */
    static u8 solid[2 + 2 * 1 * 4];
    solid[0] = 1; solid[1] = 4;
    for (unsigned i = 2; i < sizeof(solid); i++) solid[i] = 0xFF;
    const Sprite spr(solid);

    memset(scr_buff, 0, SCR_BUFF_SIZE);
    blit_masked_to_scr_buff(spr, -7, 20);      /* shift 1, start_col -1 */

    int wrote_byte0 = 0;
    for (y = 20; y < 24; y++)
        if (scr_row(y)[0] != 0) wrote_byte0++;
    CHECK(wrote_byte0 == 4,
          "a sprite straddling the left edge wrote byte 0 on %d of 4 "
          "rows; the `right >= 0` guard is what allows it\n",
          wrote_byte0);
    end("byte 0 written");
}

/* The attr blit's cell clamps keep it inside attr_buff.
 *
 *     if (col_hi >= ATTR_COLS) col_hi = ATTR_COLS - 1;
 *     if (row_hi >= ATTR_ROWS) row_hi = ATTR_ROWS - 1;
 *
 * Both survived the 2026-08-10 sweep, and both are unreachable through
 * today's callers — every one passes clip_right_px = PLAYFIELD_W - 8,
 * which caps col_hi at 30, and no sprite sits low enough to push row_hi
 * past 23.
 *
 * That is a fact about the CALLERS, not about the clamps, and the same
 * reasoning that put `bullet_band_includes_top_row` in the weapons
 * suite applies: change a clip argument or the playfield height and the
 * branch goes live silently. So it is asserted directly.
 *
 * The column case is observable from inside attr_buff — an unclamped
 * col_hi writes past the row's last cell, which IS the next row's
 * first — so the assertion is that a rect ending in column 31 leaves
 * row 1 alone. */
static void test_attr_blit_clamps_to_the_grid(void) {
    int i, spilled = 0;
    begin("attr_blit_clamps");

    memset(attr_buff, 0x77, sizeof(attr_buff));
    /* col_hi must come out EXACTLY at ATTR_COLS, or `>=` and `>` agree.
     * col_hi = (x1 - 1) / 8, so x1 = 260 gives 32. clip_right is
     * deliberately out of range, as the existing rect-flush test does.
     * A first attempt used x1 = 310 -> col_hi 38, where both forms
     * clamp and the mutation is invisible. */
    blit_sprite_attrs_to_buff_clipped(250, 0, 10, 8, 0x11, 0, 400);

    for (i = 0; i < ATTR_COLS; i++)
        if (attr_buff[1 * ATTR_COLS + i] != 0x77) spilled++;
    CHECK(spilled == 0,
          "an unclamped col_hi spilled %d cells into the next attr row\n",
          spilled);

    /* The ROW clamp cannot be caught from here, and pretending otherwise
     * would be worse than saying so. `row_hi == ATTR_ROWS` makes the
     * loop write `attr_buff[24 * ATTR_COLS + c]` — past the end of a
     * 768-byte global, into whatever the linker put next. Nothing
     * inside attr_buff changes, so no assertion on it can tell.
     *
     * The check below is still worth keeping as a plain regression
     * guard — a rect below the grid must not write INSIDE attr_buff
     * either — but it does not kill the `row_hi > ATTR_ROWS` mutant,
     * and notes/testing.md records that rather than leaving a reader to
     * assume this line covers it. */
    memset(attr_buff, 0x77, sizeof(attr_buff));
    blit_sprite_attrs_to_buff_clipped(16, ATTR_ROWS * CELL_PX + 8, 16, 16,
                                      0x22, 0, PLAYFIELD_W);
    spilled = 0;
    for (i = 0; i < (int)sizeof(attr_buff); i++)
        if (attr_buff[i] != 0x77) spilled++;
    CHECK(spilled == 0,
          "a rect below the grid wrote %d cells; row_lo alone should "
          "have put it past row_hi\n", spilled);

    end("cols and rows clamped");
}

/* blit_masked_sprite clips to the PLAYFIELD, both edges.
 *
 * It writes pixel-at-a-time through `vga_at(x, y)`, guarded by
 * `x >= PLAYFIELD_W` / `y >= PLAYFIELD_H`. Those are exclusive bounds:
 * `>` instead of `>=` lets a sprite straddling the right edge write one
 * pixel past the row, which lands on the NEXT row's first pixel — or
 * past the buffer entirely on the last row.
 *
 * The existing bounds test covers `buff_to_vga` and the rect flush, not
 * this path. An exhaustive mutation sweep of every inclusive comparison
 * in `src/` (2026-08-10) is what surfaced it: both guards survived the
 * whole host suite. */
static void test_sprite_blit_clips_to_playfield(void) {
    int x, y, escaped = 0;
    begin("sprite_blit_clips");
    init_pal_tables();

    /* 2 bytes wide, 4 rows, every mask bit set and every pixel set — so
     * any unclipped write shows up as a changed byte. */
    static u8 solid[2 + 2 * 4 * 2];
    solid[0] = 2; solid[1] = 4;
    for (unsigned i = 2; i < sizeof(solid); i++) solid[i] = 0xFF;
    const Sprite spr(solid);

    memset(vga, 0xAA, SCREEN_W * SCREEN_H);
    /* Straddling each edge in turn, including exactly ON the far bound. */
    blit_masked_sprite(spr, PLAYFIELD_W - 8, 10, 1, 2);   /* off the right */
    blit_masked_sprite(spr, PLAYFIELD_W,     20, 1, 2);   /* wholly past   */
    blit_masked_sprite(spr, -8,              30, 1, 2);   /* off the left  */
    blit_masked_sprite(spr, 40, PLAYFIELD_H - 2, 1, 2);   /* off the bottom*/
    blit_masked_sprite(spr, 40, PLAYFIELD_H,     1, 2);   /* wholly below  */

    for (y = 0; y < SCREEN_H; y++)
        for (x = 0; x < SCREEN_W; x++) {
            int inside = (x >= BORDER_X && x < BORDER_X + PLAYFIELD_W &&
                          y >= BORDER_Y && y < BORDER_Y + PLAYFIELD_H);
            if (!inside && vga[y * SCREEN_W + x] != 0xAA) escaped++;
        }
    CHECK(escaped == 0,
          "blit_masked_sprite wrote %d bytes outside the playfield\n",
          escaped);
    end("both edges clipped");
}

/* ===================================================================== */
/* 5. Rect flush == full repaint, within the rect                        */
/* ===================================================================== */

static void test_rect_flush_equiv_full(void) {
    static unsigned char full[SCREEN_W * SCREEN_H];
    int iter;
    begin("rect_flush_equiv_full");
    init_pal_tables();
    rng_seed(0x5EC7);
    for (iter = 0; iter < 500; iter++) {
        int y0 = rng_range(0, PLAYFIELD_H - 1);
        int h  = rng_range(1, PLAYFIELD_H - y0);
        int lo = rng_range(0, 31);
        int hi = rng_range(lo, 31);
        int x, y, bad_in = 0, bad_out = 0;

        randomize_buffers();
        buff_to_vga();
        memcpy(full, vga, sizeof(full));

        memset(vga, 0x5A, SCREEN_W * SCREEN_H);
        buff_to_vga_rect_bytes(y0, h, lo, hi);

        for (y = 0; y < PLAYFIELD_H; y++) {
            for (x = 0; x < PLAYFIELD_W; x++) {
                int in_rect = (y >= y0 && y < y0 + h &&
                               (x >> 3) >= lo && (x >> 3) <= hi);
                unsigned char got = *fb_at(x, y);
                if (in_rect) {
                    if (got != full[(BORDER_Y + y) * SCREEN_W + BORDER_X + x]) bad_in++;
                } else {
                    if (got != 0x5A) bad_out++;
                }
            }
        }
        CHECK(bad_in == 0, "rect(y0=%d h=%d lo=%d hi=%d): %d px differ from full repaint\n",
              y0, h, lo, hi, bad_in);
        CHECK(bad_out == 0, "rect(y0=%d h=%d lo=%d hi=%d): %d px written outside the rect\n",
              y0, h, lo, hi, bad_out);
    }
    end("500 random rects");
}

/* ===================================================================== */
/* 6. Dirty flush == full repaint                                        */
/* ===================================================================== */
/* The contract callers depend on every frame: mark everything you
 * changed, and the partial flush leaves VGA exactly as a full repaint
 * would. Pixel edits are marked with mark_dirty_rect_px; attribute edits
 * with mark_dirty_cell_rect_px, because an attribute repaints its whole
 * cell (that is clash, and it is why the two mark calls differ). */

static void test_dirty_flush_equiv_full(void) {
    static unsigned char expected[SCREEN_W * SCREEN_H];
    int iter;
    begin("dirty_flush_equiv_full");
    init_pal_tables();
    rng_seed(0xD147);
    for (iter = 0; iter < 400; iter++) {
        int n = rng_range(1, 6);
        int k, diff = 0, i;

        /* State A on screen. */
        randomize_buffers();
        buff_to_vga();
        clear_dirty_ranges(dirty_min_byte, dirty_max_byte);

        /* Mutate into state B, marking each edit. */
        for (k = 0; k < n; k++) {
            if (rng_range(0, 1)) {
                /* pixel edit */
                int x = rng_range(0, PLAYFIELD_W - 1);
                int y = rng_range(0, PLAYFIELD_H - 1);
                int w = rng_range(1, 40);
                int h = rng_range(1, 24);
                int yy, xx;
                if (x + w > PLAYFIELD_W) w = PLAYFIELD_W - x;
                if (y + h > PLAYFIELD_H) h = PLAYFIELD_H - y;
                for (yy = y; yy < y + h; yy++)
                    for (xx = x >> 3; xx <= (x + w - 1) >> 3; xx++)
                        scr_buff[yy * 32 + xx] = (unsigned char)rng_range(0, 255);
                mark_dirty_rect_px(x, y, w, h);
            } else {
                /* attribute edit — recolours whole cells */
                int cx = rng_range(0, ATTR_COLS - 1);
                int cy = rng_range(0, ATTR_ROWS - 1);
                int cw = rng_range(1, 6);
                int ch = rng_range(1, 4);
                int r, c;
                if (cx + cw > ATTR_COLS) cw = ATTR_COLS - cx;
                if (cy + ch > ATTR_ROWS) ch = ATTR_ROWS - cy;
                for (r = cy; r < cy + ch; r++)
                    for (c = cx; c < cx + cw; c++)
                        attr_buff[r * 32 + c] = (unsigned char)rng_range(0, 255);
                mark_dirty_cell_rect_px(cx * 8, cy * 8, cw * 8, ch * 8);
            }
        }

        flush_dirty_to_vga();
        memcpy(expected, vga, sizeof(expected));
        buff_to_vga();                       /* what a full repaint gives */
        for (i = 0; i < SCREEN_W * SCREEN_H; i++)
            if (expected[i] != vga[i]) diff++;
        CHECK(diff == 0, "iter %d (%d edits): dirty flush differs from full repaint in %d px\n",
              iter, n, diff);
    }
    end("400 random edit sets");
}

/* ===================================================================== */
/* 7. Cell-rounded dirty marks                                           */
/* ===================================================================== */

static void test_dirty_cell_rect_rounding(void) {
    int y, marked_lo, marked_hi;
    begin("dirty_cell_rect_rounding");
    /* A 1px-tall sprite at y=11 sits inside char row 1 (y=8..15). An attr
     * write there recolours all 8 rows, so the mark must cover 8..15. */
    clear_dirty_ranges(dirty_min_byte, dirty_max_byte);
    mark_dirty_cell_rect_px(16, 11, 8, 1);
    marked_lo = -1; marked_hi = -1;
    for (y = 0; y < PLAYFIELD_H; y++) {
        if (dirty_min_byte[0][y] != DIRTY_NONE) {
            if (marked_lo < 0) marked_lo = y;
            marked_hi = y;
        }
    }
    CHECK(marked_lo == 8 && marked_hi == 15,
          "cell mark for y=11 h=1 covered rows %d..%d, want 8..15\n", marked_lo, marked_hi);
    /* Spanning a cell boundary rounds out both ends. */
    clear_dirty_ranges(dirty_min_byte, dirty_max_byte);
    mark_dirty_cell_rect_px(0, 6, 8, 4);      /* y = 6..9, crosses row 0/1 */
    marked_lo = -1; marked_hi = -1;
    for (y = 0; y < PLAYFIELD_H; y++) {
        if (dirty_min_byte[0][y] != DIRTY_NONE) {
            if (marked_lo < 0) marked_lo = y;
            marked_hi = y;
        }
    }
    CHECK(marked_lo == 0 && marked_hi == 15,
          "cell mark for y=6 h=4 covered rows %d..%d, want 0..15\n", marked_lo, marked_hi);
    end("rounds to 8px rows");
}

/* ===================================================================== */
/* Synthetic sprites                                                     */
/* ===================================================================== */
/* Original format: [w_bytes][h_rows] then h*w (mask, pixel) pairs. No
 * shipped sprite has pixel bits outside its mask (see zxvga.c §6), so the
 * generator honours that too. */

static unsigned char sprite_buf[2 + 8 * 24 * 2];

static Sprite make_sprite(int w, int h) {
    int i;
    sprite_buf[0] = (unsigned char)w;
    sprite_buf[1] = (unsigned char)h;
    for (i = 0; i < w * h; i++) {
        unsigned char mask = (unsigned char)rng_range(0, 255);
        unsigned char pix  = (unsigned char)(rng_range(0, 255) & mask);
        sprite_buf[2 + i * 2]     = mask;
        sprite_buf[2 + i * 2 + 1] = pix;
    }
    return Sprite(sprite_buf);
}


/* ===================================================================== */
/* 12. A sprite lands at the x it was given, to the PIXEL                 */
/* ===================================================================== */
/* blit_masked_to_scr_buff takes x in pixels, not bytes: an odd x means
 * every source byte straddles two destination bytes and is shifted into
 * both. Nothing here checked WHERE it landed. test_blit_stays_in_playfield
 * checks clipping, the clash test checks it does not touch attributes,
 * and both hold if the shift is wrong — mutating `x_px & 7` to `x_px & 6`
 * left the whole suite green while moving every odd-x sprite one pixel.
 *
 * The check: a single solid byte blitted at x, read back as a bit index,
 * must start exactly at x. Run for every sub-byte offset and both sides
 * of a byte boundary. */
static void test_blit_lands_on_the_given_x(void) {
    int x;
    begin("blit_lands_on_given_x");
    for (x = 0; x < 32; x++) {
        int found = -1, bit, count = 0;
        memset(scr_buff, 0, sizeof(scr_buff));
        sprite_buf[0] = 1;                 /* 1 byte wide */
        sprite_buf[1] = 1;                 /* 1 row tall  */
        sprite_buf[2] = 0xFF;              /* mask: all   */
        sprite_buf[3] = 0x80;              /* pixel: leftmost bit only */
        {
            Sprite spr(sprite_buf);
            blit_masked_to_scr_buff(spr, x, 10);
        }
        for (bit = 0; bit < PLAYFIELD_W; bit++) {
            const u8 byte = scr_buff[10 * BYTES_PER_ROW + (bit >> 3)];
            if (byte & (0x80 >> (bit & 7))) {
                if (found < 0) found = bit;
                count++;
            }
        }
        CHECK(found == x, "a sprite blitted at x=%d lit bit %d\n", x, found);
        CHECK(count == 1, "x=%d lit %d bits, expected 1\n", x, count);
    }
    end("32 x offsets, exact");
}

/* ===================================================================== */
/* 8. THE CLASH INVARIANT: pixel blits never touch attributes            */
/* ===================================================================== */

static void test_sprite_blit_preserves_attrs(void) {
    static unsigned char attr_before[768];
    int iter;
    begin("sprite_blit_preserves_attrs");
    rng_seed(0x0B1EC7);
    for (iter = 0; iter < 2000; iter++) {
        Sprite spr(sprite_buf);
        int w = rng_range(1, 6);
        int h = rng_range(1, 20);
        /* Include off-playfield positions on every side — clipping paths
         * must not be a back door into the attribute plane. */
        int x = rng_range(-24, PLAYFIELD_W + 8);
        int y = rng_range(-24, PLAYFIELD_H + 8);
        randomize_buffers();
        memcpy(attr_before, attr_buff, sizeof(attr_before));
        spr = make_sprite(w, h);
        blit_masked_to_scr_buff(spr, x, y);
        CHECK(memcmp(attr_before, attr_buff, sizeof(attr_before)) == 0,
              "sprite %dx%d at (%d,%d) modified attr_buff\n", w, h, x, y);
    }
    end("2000 blits");
}

/* ===================================================================== */
/* 9. Clash confines every pixel to its cell's two colours               */
/* ===================================================================== */
/* The positive form of the invariant. Draw a busy scene the way the game
 * does — background attributes laid down first, then sprites blitted as
 * pixels only — and no pixel anywhere may show a colour its cell does
 * not carry. A sprite that "kept its own colour" would fail here. */

static void test_clash_confines_to_cell_pair(void) {
    int iter;
    begin("clash_confines_to_cell_pair");
    init_pal_tables();
    rng_seed(0xC1A54);
    for (iter = 0; iter < 40; iter++) {
        int k, cy, cx, py, px, offenders = 0;
        randomize_buffers();
        for (k = 0; k < 12; k++) {
            Sprite spr = make_sprite(rng_range(1, 5), rng_range(1, 16));
            blit_masked_to_scr_buff(spr, rng_range(-8, PLAYFIELD_W),
                                             rng_range(-8, PLAYFIELD_H));
        }
        buff_to_vga();
        for (cy = 0; cy < ATTR_ROWS; cy++) {
            for (cx = 0; cx < ATTR_COLS; cx++) {
                unsigned char a = attr_buff[cy * 32 + cx];
                unsigned char ink = REF_ATTR_INK(a), paper = REF_ATTR_PAPER(a);
                for (py = 0; py < 8; py++) {
                    for (px = 0; px < 8; px++) {
                        unsigned char got = *fb_at(cx * 8 + px, cy * 8 + py);
                        if (got != ink && got != paper) offenders++;
                    }
                }
            }
        }
        CHECK(offenders == 0,
              "iter %d: %d px show a colour outside their cell's ink/paper pair\n",
              iter, offenders);
    }
    end("40 scenes");
}

/* ===================================================================== */
/* 10. Attribute writes take whole cells                                 */
/* ===================================================================== */

static void test_attr_blit_recolours_cells(void) {
    int cy, cx, wrong_inside = 0, wrong_outside = 0;
    begin("attr_blit_recolours_cells");
    rng_seed(0xA77B);
    randomize_buffers();
    memset(attr_buff, 0x07, sizeof(attr_buff));
    /* A 10x8 px rect at (13, 19) spans x=13..22 and y=19..26, i.e. cells
     * cx=1..2, cy=2..3 — it covers only part of each, and every one of
     * them must end up fully recoloured. That spill IS clash. */
    blit_sprite_attrs_to_buff_clipped(13, 19, 10, 8, 0x42, 0, PLAYFIELD_W);
    for (cy = 0; cy < ATTR_ROWS; cy++) {
        for (cx = 0; cx < ATTR_COLS; cx++) {
            unsigned char a = attr_buff[cy * 32 + cx];
            int touched = (cx >= 1 && cx <= 2 && cy >= 2 && cy <= 3);
            if (touched  && a != 0x42) wrong_inside++;
            if (!touched && a != 0x07) wrong_outside++;
        }
    }
    CHECK(wrong_inside == 0, "%d cells inside the rect were not recoloured\n", wrong_inside);
    CHECK(wrong_outside == 0, "%d cells outside the rect were recoloured\n", wrong_outside);
    /* Clipping must drop everything outside [clip_left, clip_right). */
    memset(attr_buff, 0x07, sizeof(attr_buff));
    blit_sprite_attrs_to_buff_clipped(0, 0, PLAYFIELD_W, 8, 0x42, 64, 128);
    wrong_inside = wrong_outside = 0;
    for (cx = 0; cx < ATTR_COLS; cx++) {
        unsigned char a = attr_buff[cx];
        int inside = (cx >= 8 && cx < 16);
        if (inside  && a != 0x42) wrong_inside++;
        if (!inside && a != 0x07) wrong_outside++;
    }
    CHECK(wrong_inside == 0 && wrong_outside == 0,
          "clipped attr blit: %d cells short, %d cells over\n", wrong_inside, wrong_outside);
    end("whole 8x8 cells");
}

/* ===================================================================== */
/* 11. Golden: a real screen from the original game                      */
/* ===================================================================== */
/* ZX screen memory interleaves pixel rows; this is the de-interleave the
 * asset pipeline uses (scripts/extract_scr.py). Feeding a genuine 6912-
 * byte .scr through the engine exercises real attribute distributions
 * rather than random ones. */

static int zx_scr_addr(int y, int x_byte) {
    int third     = (y >> 6) & 3;
    int pixel_row =  y       & 7;
    int char_row  = (y >> 3) & 7;
    return (third << 11) | (pixel_row << 8) | (char_row << 5) | x_byte;
}

static int load_scr(const char *path) {
    unsigned char scr[6912];
    FILE *f = fopen(path, "rb");
    int y, xb;
    if (!f) return 0;
    if (fread(scr, 1, sizeof(scr), f) != sizeof(scr)) { fclose(f); return 0; }
    fclose(f);
    for (y = 0; y < PLAYFIELD_H; y++)
        for (xb = 0; xb < 32; xb++)
            scr_buff[y * 32 + xb] = scr[zx_scr_addr(y, xb)];
    memcpy(attr_buff, scr + 6144, 768);
    return 1;
}

static void test_golden_original_scr(const char *path) {
    int x, y, bad = 0;
    begin("golden_original_scr");
    init_pal_tables();
    if (!load_scr(path)) {
        printf("SKIP (no %s)\n", path);
        return;                       /* not counted as run */
    }
    buff_to_vga();
    for (y = 0; y < PLAYFIELD_H; y++)
        for (x = 0; x < PLAYFIELD_W; x++)
            if (*fb_at(x, y) != ref_pixel(scr_buff, attr_buff, x, y)) bad++;
    CHECK(bad == 0, "%s: %d of 49152 px differ from the ULA reference\n", path, bad);
    end("49152 px");
}

/* ===================================================================== */
/* --dump: canonical framebuffer, for diffing against another build      */
/* ===================================================================== */
/* Fixed input data, so two builds' dumps are directly comparable. */

static int dump_canonical(const char *path) {
    FILE *f;
    int i;
    init_pal_tables();
    memset(vga, 0, SCREEN_W * SCREEN_H);
    for (i = 0; i < 6144; i++) scr_buff[i] = (unsigned char)((i * 7 + (i >> 5)) & 0xFF);
    for (i = 0; i < 768; i++)  attr_buff[i] = (unsigned char)(i & 0xFF);
    buff_to_vga();
    /* Exercise the rect path too — it carries its own copy of the blit. */
    for (i = 0; i < 768; i++)  attr_buff[i] = (unsigned char)((i * 3) & 0xFF);
    buff_to_vga_rect_bytes(17, 61, 3, 28);
    buff_to_vga_rect_bytes(0, PLAYFIELD_H, 0, 31);
    f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    fwrite(vga, 1, SCREEN_W * SCREEN_H, f);
    fclose(f);
    return 0;
}

/* ===================================================================== */

int main(int argc, char **argv) {
    const char *golden = "original/Batty.scr";

    if (argc >= 3 && strcmp(argv[1], "--dump") == 0)
        return dump_canonical(argv[2]);
    if (argc >= 2) golden = argv[1];

    printf("zxvga video-engine tests\n");

    test_pal_tables_exhaustive();
    test_clash_expansion_vs_ula();
    test_flash_bit_ignored();
    test_blit_stays_in_playfield();
    test_scr_blit_writes_byte_zero_from_the_left();
    test_attr_blit_clamps_to_the_grid();
    test_sprite_blit_clips_to_playfield();
    test_rect_flush_equiv_full();
    test_dirty_flush_equiv_full();
    test_dirty_cell_rect_rounding();
    test_sprite_blit_preserves_attrs();
    test_blit_lands_on_the_given_x();
    test_clash_confines_to_cell_pair();
    test_attr_blit_recolours_cells();
    test_golden_original_scr(golden);

    printf("\n%d tests, %d failed\n", tests_run, tests_failed);
    return tests_failed ? 1 : 0;
}
