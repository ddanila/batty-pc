/* Host-side tests for src/bricks.cpp — the brick compositor.
 *
 * The strong one is golden: `build/level_gt/level_NN.scr` holds the
 * ORIGINAL's screen for each level, captured from ZEsarUX. Painting the
 * same level's grid must reproduce its brick pixels exactly.
 *
 * The GT is a whole screen — frame, bat, ball, HUD — so the comparison is
 * restricted to the cells a brick actually occupies. That is the part
 * this module owns; everything else in the band belongs to the background
 * and frame painters.
 *
 * The rest are properties the QEMU gates cannot state directly: that a
 * scoped repaint of every row equals a full paint, that destroyed bricks
 * leave no pixels, and that painting stays inside the band. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../src/bricks.cpp"

/* The planes the compositor writes into. Defined here rather than linked
 * against zxvga.cpp so this test needs nothing but the module. */
u8 scr_buff[SCR_BUFF_SIZE];
u8 attr_buff[ATTR_BUFF_SIZE];

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

/* --- Level data ------------------------------------------------------- */

const int LEVELS = 15;
static u8 all_levels[LEVELS * FIELD_ROWS * FIELD_COLS];

static bool load_levels(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    const size_t n = fread(all_levels, 1, sizeof(all_levels), f);
    fclose(f);
    return n == sizeof(all_levels);
}

static const u8 *level_cells(int level) {
    return &all_levels[level * FIELD_ROWS * FIELD_COLS];
}

/* --- The original's captured screen ----------------------------------- */

/* ZX screen memory interleaves pixel rows; undo it the way the asset
 * pipeline does (scripts/extract_scr.py). */
static int zx_scr_addr(int y, int x_byte) {
    return (((y >> 6) & 3) << 11) | ((y & 7) << 8) | (((y >> 3) & 7) << 5) | x_byte;
}

static u8 gt_pixels[SCR_BUFF_SIZE];

static bool load_gt(int level) {
    char path[64];
    snprintf(path, sizeof(path), "build/level_gt/level_%02d.scr", level + 1);
    u8 scr[6912];
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    const size_t n = fread(scr, 1, sizeof(scr), f);
    fclose(f);
    if (n != sizeof(scr)) return false;
    for (int y = 0; y < PLAYFIELD_H; y++)
        for (int xb = 0; xb < BYTES_PER_ROW; xb++)
            gt_pixels[y * BYTES_PER_ROW + xb] = scr[zx_scr_addr(y, xb)];
    return true;
}

/* Byte columns and pixel rows a standing brick's BODY occupies. Edges
 * reach into neighbouring cells and are checked by the row-scoping test
 * rather than here. */
static void brick_body_extent(int row, int col, int *y0, int *y1,
                              int *xb0, int *xb1) {
    *y0  = FIELD_Y0 + row * BRICK_H_PX;
    *y1  = *y0 + BRICK_H_PX - 1;
    *xb0 = 1 + 2 * col;
    *xb1 = *xb0 + 1;
}

/* --- Tests ------------------------------------------------------------ */

/* Every standing brick's body pixels must match the original's screen. */
static void test_matches_original_screens() {
    const int before = failures;
    int levels_checked = 0, bytes_compared = 0, mismatched = 0;

    for (int level = 0; level < LEVELS; level++) {
        if (!load_gt(level)) continue;          /* GT not captured locally */
        memset(scr_buff, 0, sizeof(scr_buff));
        memset(attr_buff, 0, sizeof(attr_buff));
        paint_bricks(level_cells(level));

        const u8 *cells = level_cells(level);
        for (int row = 0; row < FIELD_ROWS; row++) {
            for (int col = 0; col < FIELD_COLS; col++) {
                if (cells[row * FIELD_COLS + col] & 0x80) continue;
                int y0, y1, xb0, xb1;
                brick_body_extent(row, col, &y0, &y1, &xb0, &xb1);
                for (int y = y0; y <= y1; y++)
                    for (int xb = xb0; xb <= xb1; xb++) {
                        const int at = y * BYTES_PER_ROW + xb;
                        bytes_compared++;
                        if (scr_buff[at] != gt_pixels[at]) {
                            if (mismatched < 4)
                                check(false, "L%02d brick (%d,%d) y=%d xb=%d: "
                                             "painted %02X, original %02X\n",
                                      level + 1, row, col, y, xb,
                                      scr_buff[at], gt_pixels[at]);
                            mismatched++;
                        }
                    }
            }
        }
        levels_checked++;
    }

    if (levels_checked == 0) {
        printf("  %-28s SKIP (no build/level_gt/*.scr)\n", "matches_original_screens");
        return;
    }
    check(mismatched == 0, "%d brick body bytes differ from the original\n",
          mismatched);
    char detail[48];
    snprintf(detail, sizeof(detail), "%d levels, %d bytes", levels_checked, bytes_compared);
    report("matches_original_screens", before, detail);
}

/* Painting row by row must equal painting the whole band, once the
 * boundary repairs put back what the next row would have written. */
static void test_scoped_repaint_equals_full() {
    const int before = failures;
    int differing_levels = 0;

    for (int level = 0; level < LEVELS; level++) {
        const u8 *cells = level_cells(level);

        memset(scr_buff, 0, sizeof(scr_buff));
        memset(attr_buff, 0, sizeof(attr_buff));
        paint_bricks(cells);
        u8 full_scr[SCR_BUFF_SIZE], full_attr[ATTR_BUFF_SIZE];
        memcpy(full_scr, scr_buff, sizeof(full_scr));
        memcpy(full_attr, attr_buff, sizeof(full_attr));

        memset(scr_buff, 0, sizeof(scr_buff));
        memset(attr_buff, 0, sizeof(attr_buff));
        for (int row = 0; row < FIELD_ROWS; row++) {
            paint_brick_rows(cells, row, row);
            /* Exactly what main.cpp's incremental band rebuild does
             * after a scoped repaint — all four edge repairs, not a
             * hand-copied subset. This test used to replicate three of
             * them and omit the r0 bottom-edge restore. */
            repair_band_row_boundaries(cells, row, row);
        }
        if (memcmp(full_scr, scr_buff, sizeof(full_scr)) != 0 ||
            memcmp(full_attr, attr_buff, sizeof(full_attr)) != 0)
            differing_levels++;
    }
    check(differing_levels == 0,
          "%d levels: row-by-row repaint differs from a full paint\n",
          differing_levels);
    report("scoped_repaint_equals_full", before, "15 levels            ok");
}

/* A destroyed brick must leave no body pixels behind. */
static void test_destroyed_bricks_leave_nothing() {
    const int before = failures;
    u8 cells[FIELD_ROWS * FIELD_COLS];
    memset(cells, 0x80, sizeof(cells));         /* everything gone */
    memset(scr_buff, 0, sizeof(scr_buff));
    paint_bricks(cells);

    int painted = 0;
    for (int i = 0; i < SCR_BUFF_SIZE; i++) if (scr_buff[i]) painted++;
    check(painted == 0, "%d pixel bytes written for an empty grid\n", painted);
    report("destroyed_leave_nothing", before, "empty grid           ok");
}

/* Painting must stay inside the band — above and below it belong to the
 * HUD and the bat. */
static void test_painting_stays_in_the_band() {
    const int before = failures;
    u8 cells[FIELD_ROWS * FIELD_COLS];
    memset(cells, 0x01, sizeof(cells));         /* all standing */
    memset(scr_buff, 0xAA, sizeof(scr_buff));
    paint_bricks(cells);

    int escaped = 0;
    for (int y = 0; y < PLAYFIELD_H; y++) {
        if (y >= BRICK_BAND_Y_TOP && y <= BRICK_BAND_Y_BOT) continue;
        for (int xb = 0; xb < BYTES_PER_ROW; xb++)
            if (scr_buff[y * BYTES_PER_ROW + xb] != 0xAA) escaped++;
    }
    check(escaped == 0, "%d bytes written outside rows %d..%d\n",
          escaped, BRICK_BAND_Y_TOP, BRICK_BAND_Y_BOT);
    report("painting_stays_in_band", before, "rows 31..128         ok");
}

/* A brick colours both of its cells, and dims the two below as a shadow.
 * The dimming is the bright bit only. */
static void test_colour_and_shadow() {
    const int before = failures;
    u8 cells[FIELD_ROWS * FIELD_COLS];
    memset(cells, 0x80, sizeof(cells));
    cells[3 * FIELD_COLS + 5] = 0x02;           /* one standing brick */
    memset(attr_buff, 0, sizeof(attr_buff));
    paint_bricks(cells);

    const int cell_row = 4 + 3;
    const int cell_col = 1 + 2 * 5;
    const u8 attr = attr_buff[cell_row * ATTR_COLS + cell_col];
    check(attr != 0, "the brick coloured neither of its cells\n");
    check(attr_buff[cell_row * ATTR_COLS + cell_col + 1] == attr,
          "the brick's two cells got different colours\n");

    const u8 shadow_l = attr_buff[(cell_row + 1) * ATTR_COLS + cell_col];
    check((shadow_l & 0x40) == 0, "the shadow row kept its bright bit\n");
    report("colour_and_shadow", before, "2 cells + shadow     ok");
}

/* level_attrs.bin holds the LIVE look of every cell, so a destroyed
 * brick keeps its colour until this resets it. The empty-cell sentinel
 * $C0 must survive: it carries the per-side-strip colours, and treating
 * it as destroyed would repaint the frame's edge cells. */
static void test_destroyed_cells_reset_but_sentinels_survive() {
    const int before = failures;
    const u8 BG = 0x45;
    u8 field[FIELD_ROWS * FIELD_COLS];
    for (int i = 0; i < FIELD_ROWS * FIELD_COLS; i++) field[i] = 0x05;
    field[3 * FIELD_COLS + 4] = 0x80;   /* destroyed at runtime */
    field[3 * FIELD_COLS + 9] = 0xC0;   /* empty-cell sentinel */

    memset(attr_buff, 0x77, sizeof(attr_buff));
    reset_destroyed_cell_attrs(field, BG, 0, FIELD_ROWS - 1, 3, 16);

    const int cr = 4 + 3;
    check(attr_buff[cr * ATTR_COLS + 1 + 2 * 4 + 1] == BG,
          "destroyed cell's right char is %02X, expected the background %02X\n",
          attr_buff[cr * ATTR_COLS + 1 + 2 * 4 + 1], BG);
    check(attr_buff[cr * ATTR_COLS + 1 + 2 * 9] == 0x77,
          "the $C0 sentinel was reset to %02X; it must keep its attrs\n",
          attr_buff[cr * ATTR_COLS + 1 + 2 * 9]);
    report("destroyed_cells_reset", before, "1 destroyed, $C0 kept ok");
}

/* A destroyed cell shows a NON-bright left char only when its left
 * neighbour is still live — the original casts an inter-brick shadow
 * rightwards. Ground truth: destroyed col 6 beside live col 5 gives
 * $05; with col 5 also gone it keeps the bright $45. */
static void test_destroyed_cell_shadow_follows_left_neighbour() {
    const int before = failures;
    const u8 BG = 0x45;
    u8 field[FIELD_ROWS * FIELD_COLS];
    const int cr = 4 + 2, cc = 1 + 2 * 6;

    for (int i = 0; i < FIELD_ROWS * FIELD_COLS; i++) field[i] = 0x05;
    field[2 * FIELD_COLS + 6] = 0x80;                 /* left neighbour live */
    memset(attr_buff, 0x77, sizeof(attr_buff));
    reset_destroyed_cell_attrs(field, BG, 0, FIELD_ROWS - 1, 3, 16);
    check(attr_buff[cr * ATTR_COLS + cc] == u8(BG & 0xBF),
          "with a live left neighbour the left char is %02X, expected %02X\n",
          attr_buff[cr * ATTR_COLS + cc], u8(BG & 0xBF));

    field[2 * FIELD_COLS + 5] = 0x80;                 /* left neighbour gone */
    memset(attr_buff, 0x77, sizeof(attr_buff));
    reset_destroyed_cell_attrs(field, BG, 0, FIELD_ROWS - 1, 3, 16);
    check(attr_buff[cr * ATTR_COLS + cc] == BG,
          "with the left neighbour gone the left char is %02X, expected %02X\n",
          attr_buff[cr * ATTR_COLS + cc], BG);
    report("destroyed_cell_shadow", before, "live/gone neighbour  ok");
}

/* The edge repairs only matter when a repaint STOPS at a boundary.
 *
 * scoped_repaint_equals_full above walks every row ascending, so row
 * r+1's own paint does whatever the repairs would have done — remove
 * any repair and that test stays green (verified by mutation). It
 * proves row-by-row == full, which holds either way.
 *
 * This paints ONE window and stops, which is what the incremental band
 * rebuild does, then compares the window's canonical rows against the
 * full paint. That is the property known-bugs #1 and #2 violated. */
static void test_window_repaint_matches_full_at_its_edges() {
    const int before = failures;
    int differing = 0;
    for (int level = 0; level < LEVELS; level++) {
        const u8 *cells = level_cells(level);

        memset(scr_buff, 0, sizeof(scr_buff));
        memset(attr_buff, 0, sizeof(attr_buff));
        paint_bricks(cells);
        u8 full_scr[SCR_BUFF_SIZE], full_attr[ATTR_BUFF_SIZE];
        memcpy(full_scr, scr_buff, sizeof(full_scr));
        memcpy(full_attr, attr_buff, sizeof(full_attr));

        for (int r0 = 0; r0 + 2 < FIELD_ROWS; r0++) {
            const int r1 = r0 + 2;
            memset(scr_buff, 0, sizeof(scr_buff));
            memset(attr_buff, 0, sizeof(attr_buff));
            paint_brick_rows(cells, r0, r1);
            repair_band_row_boundaries(cells, r0, r1);

            /* The rows the rebuild captures: the window's bodies plus
             * r1's bottom edge, and the shared top-edge row above. */
            bool same = true;
            for (int y = 32 + r0 * 8; same && y <= 40 + r1 * 8; y++) {
                if (memcmp(&full_scr[y * BYTES_PER_ROW + 1],
                           &scr_buff[y * BYTES_PER_ROW + 1], 30) != 0)
                    same = false;
            }
            /* Attrs too: repaint_row_attrs exists to re-brighten the
             * shared boundary row, and comparing only pixels leaves it
             * unguarded. */
            for (int cr = 4 + r0; same && cr <= 5 + r1; cr++) {
                if (memcmp(&full_attr[cr * ATTR_COLS + 1],
                           &attr_buff[cr * ATTR_COLS + 1], 30) != 0)
                    same = false;
            }
            if (!same) differing++;
        }
    }
    check(differing == 0,
          "%d level/window pairs: a window repaint differs from the full "
          "paint inside its own rows\n", differing);
    report("window_repaint_matches_full", before, "15 levels x 10 windows ok");
}


/* The two "is this brick there" rules must stay different.
 *
 * BrickField::standing is `!(cell & 0x80)` — an undestructible brick is
 * still there to bounce off. bricks_live_count is `!(cell & 0xA0)` — an
 * undestructible brick can never be cleared, so counting it toward level
 * completion would make the level uncompletable.
 *
 * Reading one as the other has no visible symptom until a level either
 * never ends or ends early. BATTY_REPLAY_CLEAR_BRICKS depends on the
 * distinction too: it must not turn an undestructible cell into rubble.
 */
static void test_live_count_vs_solid() {
    const int before = failures;
    u8 cells[FIELD_ROWS * FIELD_COLS];
    const int ALL = FIELD_ROWS * FIELD_COLS;

    memset(cells, 0x01, sizeof(cells));          /* all plain, all live */
    check(bricks_live_count(cells) == ALL,
          "a full grid counted %d, expected %d\n",
          bricks_live_count(cells), ALL);

    memset(cells, 0x81, sizeof(cells));          /* all destroyed */
    check(bricks_live_count(cells) == 0,
          "an all-destroyed grid counted %d\n", bricks_live_count(cells));

    memset(cells, 0x21, sizeof(cells));          /* all undestructible */
    check(bricks_live_count(cells) == 0,
          "an all-undestructible grid counted %d — a level made only of "
          "metal could never be completed\n", bricks_live_count(cells));

    memset(cells, 0xA1, sizeof(cells));          /* both bits */
    check(bricks_live_count(cells) == 0,
          "destroyed AND undestructible counted %d\n",
          bricks_live_count(cells));

    memset(cells, 0x01, sizeof(cells));
    cells[0] = 0x81;    /* destroyed      */
    cells[1] = 0x21;    /* undestructible */
    cells[2] = 0xA1;    /* both           */
    check(bricks_live_count(cells) == ALL - 3,
          "mixed grid counted %d, expected %d\n",
          bricks_live_count(cells), ALL - 3);

    /* ...while collision still sees the undestructible one. */
    BrickField f(cells);
    check(f.standing(0, 1),
          "BrickField::standing says the undestructible brick at col 1 is "
          "gone; it must still be there to bounce off\n");
    check(!f.standing(0, 0),
          "BrickField::standing says the DESTROYED brick at col 0 is there\n");
    report("live_count_vs_solid", before, "0x80 vs 0xA0 rules   ok");
}


/* The band's bounds must come from the GEOMETRY, not from themselves.
 *
 * test_painting_stays_in_the_band asserts that nothing is painted
 * outside [BRICK_BAND_Y_TOP, BRICK_BAND_Y_BOT] — but it reads those
 * bounds from bricks.h, the same header the painter uses. Mutating
 * BRICK_BAND_Y_TOP from 31 to 30 moves the code and the expectation
 * together, and the test cannot fail. A self-referential test looks
 * like coverage and is not.
 *
 * level.h is the independent authority: it owns the field's origin and
 * carries its own static asserts against the original's addresses. The
 * band is one edge row above the first brick row and one below the
 * last, so both bounds are derivable from it. */
static void test_band_bounds_follow_the_field_geometry() {
    const int before = failures;
    check(BRICK_BAND_Y_TOP == FIELD_Y0 - 1,
          "BRICK_BAND_Y_TOP is %d; the band's top edge sits one row above "
          "FIELD_Y0 (%d), so it must be %d\n",
          BRICK_BAND_Y_TOP, FIELD_Y0, FIELD_Y0 - 1);
    check(BRICK_BAND_Y_BOT == FIELD_Y_END,
          "BRICK_BAND_Y_BOT is %d; the band's bottom edge sits on "
          "FIELD_Y_END (%d)\n", BRICK_BAND_Y_BOT, FIELD_Y_END);
    /* and the original's own numbers, which level.h static-asserts */
    check(FIELD_Y0 == 0x20 && FIELD_Y_END == 0x80,
          "the field geometry moved: Y0=%02X END=%02X\n",
          FIELD_Y0, FIELD_Y_END);
    report("band_bounds_from_geometry", before, "vs level.h           ok");
}

/* Can the brick zone's ATTRS be generated, or must they stay captured?
 *
 * `assets/level_attrs.bin` is 15 x 768 bytes of ZX attribute cells taken
 * from emulator screens, and PLAN.md WS7 wants it gone.
 * `test-level-attrs-derivable` already proves the LIVE-BRICK fifth is
 * reproducible from `briks_colors` plus the border shadow. This asks the
 * harder half: the EMPTY cells, whose value is whatever survives
 * `print_briks`' row-by-row pass with `brik_shadow` interleaved.
 *
 * An earlier attempt tried to answer that with a neighbour predicate and
 * reached 94.4% — curve-fitting, abandoned (notes/levels.md). The right
 * instrument is the port's own painter, which implements the pass ORDER:
 * `paint_bricks` walks the rows calling `paint_shadow_row` (the
 * `brik_shadow` port) exactly where the original does.
 *
 * So: fill the band with bg_attr, run paint_bricks, apply
 * print_border_shadow's left arm, and compare against the capture. No
 * re-base from level_attrs — that is what `paint_brick_band` does and it
 * would make this circular. */
static const u8 BG_ATTR_PER_CYCLE[4] = { 0x46, 0x44, 0x45, 0x47 };

static bool load_level_attrs(u8 *out) {
    FILE *f = fopen("assets/level_attrs.bin", "rb");
    if (!f) return false;
    const size_t n = fread(out, 1, 15u * 768u, f);
    fclose(f);
    return n == 15u * 768u;
}

static void test_attrs_generate_without_the_capture() {
    const int before = failures;
    static u8 captured[15 * 768];
    if (!load_level_attrs(captured)) {
        printf("  %-28s SKIP (no assets/level_attrs.bin)\n",
               "attrs_generate");
        return;
    }
    int checked = 0, wrong = 0, first_lvl = -1, first_cr = 0, first_cc = 0;
    u8 first_want = 0, first_got = 0;
    for (int lvl = 0; lvl < 15; lvl++) {
        const u8 bg = BG_ATTR_PER_CYCLE[lvl & 3];
        const u8 *cells = level_cells(lvl);
        memset(attr_buff, bg, ATTR_BUFF_SIZE);
        memset(scr_buff, 0, SCR_BUFF_SIZE);
        paint_bricks(cells);
        /* print_border_shadow's left arm: col 1 of char rows 1..23. */
        for (int cr = 1; cr <= 23; cr++) attr_buff[cr * 32 + 1] &= 0xBF;
        /* Char rows 3..23, cols 1..30 — not just the brick cells.
         *
         * Row 16 is the shadow row for field row 11, written by
         * paint_shadow_row like every other, and it is where a
         * bg-plus-border-shadow rule falls down: 152 of its cells across
         * the 15 levels are dimmed and nothing local says why. Rows 17
         * and below are plain background, and row 3 sits above the band.
         * Including them all costs nothing and pins what the painter
         * does OUTSIDE the cells it fills.
         *
         * Columns 0 and 31 are the frame's, and char rows 0..2 are the
         * HUD's; test-frame-derivable covers the first and nothing
         * covers the second yet. */
        for (int cr = 3; cr <= 23; cr++) {
            for (int cc = 1; cc <= 30; cc++) {
                const u8 want = captured[lvl * 768 + cr * 32 + cc];
                const u8 got  = attr_buff[cr * 32 + cc];
                checked++;
                if (got != want && wrong++ == 0) {
                    first_lvl = lvl + 1; first_cr = cr; first_cc = cc;
                    first_want = want; first_got = got;
                }
            }
        }
    }
    check(wrong == 0,
          "%d of %d generated attrs differ from the capture; first at "
          "level %d char (%d,%d): captured %02X, generated %02X\n",
          wrong, checked, first_lvl, first_cr, first_cc,
          first_want, first_got);
    if (failures == before) {
        char detail[64];
        snprintf(detail, sizeof(detail), "%d cells, 15 levels", checked);
        report("attrs_generate", before, detail);
    } else {
        report("attrs_generate", before, "");
    }
}

int main(int argc, char **argv) {
    const char *levels_path = argc > 1 ? argv[1] : "assets/levels.bin";
    printf("bricks tests\n");
    if (!load_levels(levels_path)) {
        printf("  cannot read %s — run `make assets` first\n", levels_path);
        return 1;
    }
    test_matches_original_screens();
    test_scoped_repaint_equals_full();
    test_destroyed_bricks_leave_nothing();
    test_painting_stays_in_the_band();
    test_colour_and_shadow();
    test_destroyed_cells_reset_but_sentinels_survive();
    test_destroyed_cell_shadow_follows_left_neighbour();
    test_window_repaint_matches_full_at_its_edges();
    test_attrs_generate_without_the_capture();
    test_live_count_vs_solid();
    test_band_bounds_follow_the_field_geometry();
    printf("\n%d tests, %d failed\n", tests_run, failures);
    return failures ? 1 : 0;
}
