/* See bricks.h. */

#include "bricks.h"
#include "zxvga.h"

#include <string.h>   /* memcpy, for the band attr re-base */

namespace {

static const unsigned char spr_brik_1[16] = {
    0xFF, 0xFE, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00,
    0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x00, 0x00
};

/* Mirror of `briks_colors` at $AEEC. Original ASM uses
 *   LD HL,briks_colors-$01 ; CALL hl_add_a (A = cell low nibble 1..14)
 * so it reads briks_colors[low_nibble - 1]. Our C-side does
 *   briks_colors[iy_byte & 0x0F]
 * which expects 1-based indexing — slot [0] is a never-indexed placeholder. */
static const unsigned char briks_colors[16] = {
    0x00,                          /* [0] never indexed (low nibble == 0
                                    * means "skip" per the cell format). */
    0x57, 0x4F, 0x5F, 0x20, 0x70,  /* [1..5] normal bricks */
    0x47, 0x57, 0x5F, 0x4F,        /* [6..9] hard (multi-hit) bricks */
    0x00,                          /* [10] unused per original */
    0x47, 0x57, 0x4F, 0x5F,        /* [11..14] indestructible bricks (e.g. L5 $2E) */
    0x00                           /* [15] unused per original */
};

/* Cursors into the two planes, walked a brick row at a time.
 * orig: brik_addr_buf, brik_attr_buf */
unsigned int brik_addr_buf;
unsigned int brik_attr_buf;

/* Port of print_one_brik_buf ($AE82). Paints one brick into scr_buff at
 * `hl` (the top-left byte of the brick's 2-byte body), plus decoration:
 *   - 2 zero bytes one pixel-row above the brick (top edge)
 *   - bit 0 cleared in the 8 bytes left of the brick (left edge),
 *     unless the brick sits in the leftmost column
 *   - 2 zero bytes one pixel-row below (bottom edge)
 *   - bit 7 cleared in the 8 bytes right of the brick (right edge),
 *     unless the brick sits in the rightmost column
 * Finally the brick's two char cells in attr_buff are set to the colour
 * from briks_colors keyed by the descriptor's low nibble. */
void print_one_brik(unsigned int hl, unsigned char iy_byte) {
    unsigned int h;
    int col_byte = (int)(hl & 0x1F);
    int i;
    unsigned int brick_top_y_px = hl >> 5;
    unsigned int attr_off       = (brick_top_y_px >> 3) * 32u + (unsigned int)col_byte;
    unsigned char attr;

    /* Top edge: 2 zero bytes one pixel-row above the brick. */
    scr_buff[hl - 32] = 0;
    scr_buff[hl - 31] = 0;

    /* Left edge: only if not the leftmost brick column (col_byte == 1). */
    if (col_byte != 1) {
        h = hl - 1;
        for (i = 0; i < 8; i++) { scr_buff[h] &= 0xFE; h += 32; }
    }

    /* Body: 8 rows * 2 bytes from spr_brik_1. */
    h = hl;
    for (i = 0; i < 8; i++) {
        scr_buff[h]     = spr_brik_1[i * 2];
        scr_buff[h + 1] = spr_brik_1[i * 2 + 1];
        h += 32;
    }
    /* h == hl + 256 now (= byte at col 0 of the row one below the brick). */

    /* Bottom edge: 2 zero bytes one pixel-row below the brick. */
    scr_buff[h]     = 0;
    scr_buff[h + 1] = 0;

    /* Right edge: only if not the rightmost brick column. The original
     * tests `(L+1) AND $1F == $1E`, which is `col_byte == 29`. */
    if (col_byte != 29) {
        h = hl + 2;
        for (i = 0; i < 8; i++) { scr_buff[h] &= 0x7F; h += 32; }
    }

    /* Color attr: 1-indexed lookup, write to both char cells the brick
     * spans. Mirror of LAE82_4 ($AE9C): straight briks_colors lookup
     * by low nibble, no per-state dimming. Earlier port dimmed
     * multi-hit bricks after their first hit as a UX cue but the
     * original draws them with their fresh colour throughout. */
    attr = briks_colors[iy_byte & 0x0F];
    attr_buff[attr_off]     = attr;
    attr_buff[attr_off + 1] = attr;
}

/* Port of brik_shadow ($AE2A). Clears the bright bit (bit 6) on the 2
 * attr cells one char-row below each non-skip brick, dimming them so
 * they read as a drop shadow. Skips the second cell when it would
 * wrap past the rightmost attr column. */
void paint_shadow_row(unsigned int hl_attr, const unsigned char *cell_row) {
    unsigned int h = hl_attr;
    int col;
    for (col = 0; col < 15; col++) {
        unsigned char cell = cell_row[col];
        if (!(cell & 0x80)) {
            attr_buff[h] &= 0xBF;
            if (((h + 1) & 0x1F) != 31) attr_buff[h + 1] &= 0xBF;
        }
        h += 2;
    }
}

}  /* namespace */

/* Port of print_briks ($ADE1). Walks the 12x15 level cell grid and
 * compositess each non-skip cell into scr_buff/attr_buff via
 * print_one_brik_buf_c + brik_shadow_c. */
int bricks_live_count(const u8 *cells) {
    int i, n = 0;
    for (i = 0; i < FIELD_ROWS * FIELD_COLS; i++) {
        if (!(cells[i] & 0xA0)) n++;
    }
    return n;
}

void paint_bricks(const u8 *cells) {
    int row, col;
    brik_addr_buf = 0x401;   /* scr_buff + $401 = pixel (8, 32). */
    brik_attr_buf = 0xA2;    /* attr_buff + $A2 = char (2, 5). */
    for (row = 0; row < FIELD_ROWS; row++) {
        unsigned int hl = brik_addr_buf;
        const unsigned char *cell_row = &cells[row * FIELD_COLS];
        for (col = 0; col < FIELD_COLS; col++) {
            if (!(cell_row[col] & 0x80)) {
                print_one_brik(hl, cell_row[col]);
            }
            hl += 2;
        }
        paint_shadow_row(brik_attr_buf, cell_row);
        brik_addr_buf += 0x100;   /* +8 pixel rows (next brick row). */
        brik_attr_buf += 0x20;    /* +1 char row. */
    }
}

/* Row-scoped variant of print_briks_c: paint only brick rows [r0, r1].
 * Same per-row addressing (brik_addr_buf = 0x401 + row*0x100, attr base
 * 0xA2 + row*0x20), used by the incremental band-cache rebuild so a single
 * brick hit need not repaint the whole band. */
void paint_brick_rows(const u8 *cells, int first_row, int last_row) {
    int row, col;
    for (row = first_row; row <= last_row; row++) {
        unsigned int hl = 0x401u + (unsigned int)row * 0x100u;
        const unsigned char *cell_row = &cells[row * FIELD_COLS];
        for (col = 0; col < FIELD_COLS; col++) {
            if (!(cell_row[col] & 0x80)) print_one_brik(hl, cell_row[col]);
            hl += 2;
        }
        paint_shadow_row(0xA2u + (unsigned int)row * 0x20u, cell_row);
    }
}

void repaint_row_body_top(const u8 *cells, int row) {
    unsigned int hl = 0x401u + (unsigned int)row * 0x100u;
    for (int col = 0; col < FIELD_COLS; col++) {
        if (!(cells[row * FIELD_COLS + col] & 0x80)) {
            const int col_byte = 1 + 2 * col;
            scr_buff[hl]     = spr_brik_1[0];
            scr_buff[hl + 1] = spr_brik_1[1];
            if (col_byte != 1)  scr_buff[hl - 1] &= 0xFE;
            if (col_byte != 29) scr_buff[hl + 2] &= 0x7F;
        }
        hl += 2;
    }
}

void repaint_row_top_edge(const u8 *cells, int row) {
    unsigned int hl = 0x401u + (unsigned int)row * 0x100u - 32u;
    for (int col = 0; col < FIELD_COLS; col++) {
        if (!(cells[row * FIELD_COLS + col] & 0x80)) {
            scr_buff[hl]     = 0;
            scr_buff[hl + 1] = 0;
        }
        hl += 2;
    }
}

void repaint_row_attrs(const u8 *cells, int row) {
    for (int col = 0; col < FIELD_COLS; col++) {
        const u8 cell = cells[row * FIELD_COLS + col];
        if (cell & 0x80) continue;
        const u8 attr = briks_colors[cell & 0x0F];
        attr_buff[(4 + row) * ATTR_COLS + 1 + 2 * col] = attr;
        attr_buff[(4 + row) * ATTR_COLS + 2 + 2 * col] = attr;
    }
}

/* level_attrs.bin was captured with every brick alive, so it still
 * carries brick colour in cells whose brick is now destroyed. Reset
 * those to the band background, plus the shadow row beneath — a live
 * brick below repaints its own body attr afterwards, and where there is
 * none the stale dimmed shadow goes with the brick.
 *
 * Only RUNTIME-destroyed cells reset: bit 7 set, bit 6 clear. The
 * empty-cell sentinel $C0 has both bits and must keep its level_attrs
 * value, which carries per-side-strip cell colours.
 *
 * A destroyed cell shows a non-bright LEFT char only when its left
 * neighbour is still live, because the original casts an inter-brick
 * shadow rightwards (GT: destroyed col 6 beside live col 5 gives left
 * char $05; with col 5 also gone it keeps the bright $45). The right
 * char is always bg_attr.
 *
 * Writes are clipped to [cr0, cr1], the char rows the caller just
 * re-based from level_attrs; rows outside it are already correct. The
 * row scan runs one brick row beyond [r0, r1] on each side because
 * cr0 doubles as row r0-1's shadow row and cr1 as row r1+1's cell row —
 * that overlap is what known-bugs #1/#2 were. */
void reset_destroyed_cell_attrs(const u8 *cells,
                                       u8 bg_attr,
                                       int r0, int r1, int cr0, int cr1) {
    int row, col;
    for (row = r0 - 1; row <= r1 + 1; row++) {
        if (row < 0 || row >= FIELD_ROWS) continue;
        for (col = 0; col < FIELD_COLS; col++) {
            int cr, cc1, cc2, left_live;
            u8 latt;
            if ((cells[row * FIELD_COLS + col] & 0xC0) != 0x80) continue;

            cr  = 4 + row;
            cc1 = 1 + 2 * col;
            cc2 = cc1 + 1;
            left_live = (col > 0) && !(cells[row * FIELD_COLS + col - 1] & 0x80);
            latt = left_live ? u8(bg_attr & 0xBF) : bg_attr;

            if (cr >= cr0 && cr <= cr1) {
                attr_buff[cr * ATTR_COLS + cc1] = latt;
                attr_buff[cr * ATTR_COLS + cc2] = bg_attr;
            }
            if (cr + 1 >= cr0 && cr + 1 <= cr1) {
                attr_buff[(cr + 1) * ATTR_COLS + cc1] = latt;
                attr_buff[(cr + 1) * ATTR_COLS + cc2] = bg_attr;
            }
        }
    }
}

/* Put back what a row-scoped repaint disturbed at its edges.
 *
 * A full ascending paint gets these for free: each row's print
 * overwrites the previous row's edge bytes as it goes. Painting only
 * [r0, r1] stops short, so the boundary rows keep whatever the bg
 * repaint left. Getting this wrong is what known-bugs #1 and #2 were.
 *
 * The comments' numbers are the order these were FOUND, not the order
 * they run; they are applied bottom-edge first because each later one
 * overwrites part of the earlier one's output.
 */
void repair_band_row_boundaries(const u8 *cells,
                                       int r0, int r1) {
    int col;
    /* Edge fix-up 1: row r1's print zeroed its bottom-edge row, which in
     * the full ascending paint is overwritten by row r1+1's body row 0
     * where that brick is live — re-paint those two bytes plus the
     * side-edge bit clears print_one_brik would apply on that row. */
    if (r1 + 1 < FIELD_ROWS) repaint_row_body_top(cells, r1 + 1);
    /* Edge fix-up 4: row r1's body row 7 (pixel row 39+8*r1, bg-erased
     * and re-painted above) is canonically overwritten by row r1+1's
     * TOP-edge zeros where that brick is live — re-apply them.
     *
     * MEASURED REDUNDANT (2026-08-09): over 15 levels x 10 windows this
     * changes 0 bytes, because fix-up 1 above already writes those rows.
     * Kept as defence in depth — it is one pass over 15 columns, and the
     * redundancy holds only while repaint_row_body_top keeps covering
     * the same bytes. Removing it is safe today and silently unsafe if
     * that changes, which is why the note is here rather than the
     * deletion. */
    if (r1 + 1 < FIELD_ROWS) repaint_row_top_edge(cells, r1 + 1);
    /* Edge fix-up 3: print's brik_shadow_c(r1) dimmed char row 5+r1,
     * which is row r1+1's CELL row — in the full ascending paint, row
     * r1+1's own print re-brightens its live cells' attrs right after.
     * Re-apply that write since r1+1 isn't printed here. */
    if (r1 + 1 < FIELD_ROWS) repaint_row_attrs(cells, r1 + 1);
    /* Edge fix-up 2: a destroyed cell in row r0 sits under row r0-1's
     * bottom-edge zeros; the caller's bg repaint erased them — restore
     * where the brick above is live. */
    if (r0 > 0) {
        unsigned int hl = 0x401u + (unsigned int)r0 * 0x100u;
        for (col = 0; col < FIELD_COLS; col++) {
            if ((cells[r0 * FIELD_COLS + col] & 0x80)
                && !(cells[(r0 - 1) * FIELD_COLS + col] & 0x80)) {
                scr_buff[hl]     = 0;
                scr_buff[hl + 1] = 0;
            }
            hl += 2;
        }
    }

}

void paint_brick_band(const u8 *cells, const u8 *lattr, u8 bg_attr) {
    /* The per-level attrs cover char rows 3..16: the brick band plus the
     * frame's side strips and the pre-dimmed shadow rows. */
    memcpy(&attr_buff[3 * ATTR_COLS], &lattr[3 * ATTR_COLS], 14 * ATTR_COLS);
    reset_destroyed_cell_attrs(cells, bg_attr, 0, FIELD_ROWS - 1, 3, 16);
    paint_bricks(cells);
}

void paint_brick_band_rows(const u8 *cells, const u8 *lattr, u8 bg_attr,
                           int r0, int r1, int cr0, int cr1) {
    memcpy(&attr_buff[cr0 * ATTR_COLS], &lattr[cr0 * ATTR_COLS],
           (unsigned)((cr1 - cr0 + 1) * ATTR_COLS));
    reset_destroyed_cell_attrs(cells, bg_attr, r0, r1, cr0, cr1);
    paint_brick_rows(cells, r0, r1);
    repair_band_row_boundaries(cells, r0, r1);
}
