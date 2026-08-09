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

static void report(const char *name, int before, const char *detail) {
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
    printf("\n%s\n", failures ? "FAILED" : "8 tests, 0 failed");
    return failures ? 1 : 0;
}
