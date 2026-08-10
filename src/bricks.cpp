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

/* Port of print_one_brik_buf ($AE82). `hl` is the top-left byte of the
 * brick's 2-byte body. */
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

    /* Bottom edge: 2 zero bytes one pixel-row below the brick. */
    scr_buff[h]     = 0;
    scr_buff[h + 1] = 0;

    /* Right edge: only if not the rightmost brick column. The original
     * tests `(L+1) AND $1F == $1E`, which is `col_byte == 29`. */
    if (col_byte != 29) {
        h = hl + 2;
        for (i = 0; i < 8; i++) { scr_buff[h] &= 0x7F; h += 32; }
    }

    /* Mirror of LAE82_4 ($AE9C): a straight briks_colors lookup by low
     * nibble with NO per-state dimming — the original draws a multi-hit
     * brick in its fresh colour even after its first hit. */
    attr = briks_colors[iy_byte & 0x0F];
    attr_buff[attr_off]     = attr;
    attr_buff[attr_off + 1] = attr;
}

/* Port of brik_shadow ($AE2A). Clearing the bright bit is the whole of the
 * drop shadow. The second cell is skipped where it would wrap past the
 * rightmost attr column. */
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

int bricks_live_count(const u8 *cells) {
    int i, n = 0;
    for (i = 0; i < FIELD_ROWS * FIELD_COLS; i++) {
        if (!(cells[i] & 0xA0)) n++;
    }
    return n;
}

/* Port of print_briks ($ADE1). */
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

/* The brick COLOUR attrs a row contributes, and nothing else. The partial
 * rebuild needs the colour of the row just BELOW its window without the
 * pixels — that row's cells are not being recomposited, only the char row
 * they share with the window's bottom shadow row. */
static void paint_row_brick_attrs(const u8 *cells, int row) {
    const unsigned char *cell_row = &cells[row * FIELD_COLS];
    unsigned int attr_off = (unsigned int)(4 + row) * ATTR_COLS + 1u;
    int col;
    for (col = 0; col < FIELD_COLS; col++) {
        if (!(cell_row[col] & 0x80)) {
            const unsigned char attr = briks_colors[cell_row[col] & 0x0F];
            attr_buff[attr_off]     = attr;
            attr_buff[attr_off + 1] = attr;
        }
        attr_off += 2;
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

/* Contract and the [cr0, cr1] / one-row-overshoot reasoning: bricks.h.
 *
 * The shadow row beneath a reset cell goes too: a live brick below
 * repaints its own body attr afterwards, and where there is none the
 * stale dimmed shadow should leave with the brick.
 *
 * Only RUNTIME-destroyed cells reset: bit 7 set, bit 6 clear. The
 * empty-cell sentinel $C0 has both bits and must keep its level_attrs
 * value, which carries per-side-strip cell colours.
 *
 * A destroyed cell shows a non-bright LEFT char only when its left
 * neighbour is still live, because the original casts an inter-brick
 * shadow rightwards (GT: destroyed col 6 beside live col 5 gives left
 * char $05; with col 5 also gone it keeps the bright $45). The right
 * char is always bg_attr. */
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

/* Why these repairs are needed at all: bricks.h. Here, only the ORDER
 * matters — bottom edge first, because each later one overwrites part of
 * the earlier one's output. */
void repair_band_row_boundaries(const u8 *cells,
                                       int r0, int r1) {
    int col;
    /* Row r1's print zeroed its bottom-edge row, which in the full
     * ascending paint is overwritten by row r1+1's body row 0 where that
     * brick is live — re-paint those two bytes plus the side-edge bit
     * clears print_one_brik would apply on that row. */
    if (r1 + 1 < FIELD_ROWS) repaint_row_body_top(cells, r1 + 1);
    /* Row r1's body row 7 is canonically overwritten by row r1+1's
     * TOP-edge zeros where that brick is live.
     *
     * MEASURED REDUNDANT: over 15 levels x 10 windows this changes 0
     * bytes, because the call above already writes those rows. Kept as
     * defence in depth — the redundancy holds only while
     * repaint_row_body_top keeps covering the same bytes, so removing it
     * is safe today and silently unsafe if that changes. */
    if (r1 + 1 < FIELD_ROWS) repaint_row_top_edge(cells, r1 + 1);
    /* print's brik_shadow_c(r1) dimmed char row 5+r1, which is row r1+1's
     * CELL row — in the full ascending paint, row r1+1's own print
     * re-brightens its live cells' attrs right after. */
    if (r1 + 1 < FIELD_ROWS) repaint_row_attrs(cells, r1 + 1);
    /* A destroyed cell in row r0 sits under row r0-1's bottom-edge zeros;
     * the caller's bg repaint erased them — restore where the brick above
     * is live. */
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

/* Contract, and why the paint and capture windows differ: bricks.h. */
void band_rebuild_window(int lo, int hi,
                         int *r0, int *r1,
                         int *py_capture0, int *py_paint0, int *py1,
                         int *cr0, int *cr1) {
    const int R0 = (lo > 0) ? lo - 1 : 0;
    const int R1 = (hi + 1 < FIELD_ROWS) ? hi + 1 : FIELD_ROWS - 1;
    *r0 = R0;
    *r1 = R1;
    *py_capture0 = BRICK_BAND_Y_TOP + R0 * 8;
    /* The only asymmetry: with no row above, row 0's shared top-edge row
     * has no other owner, so it is repainted rather than inherited. */
    *py_paint0 = (R0 == 0) ? *py_capture0 : *py_capture0 + 1;
    *py1 = 40 + R1 * 8;
    *cr0 = 4 + R0;
    *cr1 = 5 + R1;
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

    /* The window's two boundary char rows are SHARED with the brick rows
     * outside it, and the base copy above has just wiped both. Each
     * neighbour's contribution has to go back, in the order the full
     * ascending paint would apply it:
     *
     *   cr0 = 4+r0 = 5+(r0-1)   row r0-1's SHADOW, then row r0's colour
     *   cr1 = 5+r1 = 4+(r1+1)   row r1's shadow, then row r1+1's colour
     *
     * so the shadow above goes in BEFORE paint_brick_rows and the colour
     * below goes in AFTER it. Getting the order wrong is invisible
     * wherever only one of the two applies to a cell.
     *
     * Omitting this entirely is invisible while the base band is a
     * capture taken with every brick alive, because the copy then already
     * carries both neighbours' attrs. Against the EMPTY-playfield band it
     * is 92 px of stale bright. known-bugs #18. */
    if (r0 - 1 >= 0 && cr0 <= 5 + (r0 - 1) && 5 + (r0 - 1) <= cr1)
        paint_shadow_row(0xA2u + (unsigned int)(r0 - 1) * 0x20u,
                         &cells[(r0 - 1) * FIELD_COLS]);

    paint_brick_rows(cells, r0, r1);

    if (r1 + 1 < FIELD_ROWS && cr0 <= 4 + (r1 + 1) && 4 + (r1 + 1) <= cr1)
        paint_row_brick_attrs(cells, r1 + 1);

    repair_band_row_boundaries(cells, r0, r1);
}
