/* bricks — compositing the brick band into the display planes.
 *
 * orig: print_briks $ADE1, print_one_brik_buf $AE82, brik_shadow $AE2A
 *
 * A brick is 16x8 px: two character cells wide, one tall. Painting one
 * writes four things — its body pixels, a one-pixel dark edge on each of
 * its four sides, its colour into both of its cells, and a dimmed copy of
 * that colour into the two cells below, which reads as a drop shadow.
 *
 * The edges are why a brick cannot be painted in isolation: each writes a
 * pixel row or column into the NEIGHBOURING cell. Repainting a row of
 * bricks therefore disturbs the rows either side of it, which is what the
 * incremental band rebuild has to account for (known-bugs #1/#2).
 *
 * Cells are the 12x15 grid from level.h: bit 7 means gone, the low nibble
 * selects the colour. Nothing here reads or writes game state — give it a
 * grid and it paints, which is what lets tests/test_bricks.cpp diff the
 * result against the original's captured screens. */

#ifndef BATTY_BRICKS_H
#define BATTY_BRICKS_H

#include "level.h"
#include "types.h"

/* Pixel rows the band occupies: the top edge above the first brick row,
 * twelve rows of bricks, and the bottom edge below the last. */
const int BRICK_BAND_Y_TOP = 31;
const int BRICK_BAND_Y_BOT = 128;

/* Paint every standing brick in `cells` into scr_buff / attr_buff. */
void paint_bricks(const u8 *cells);

/* level_attrs.bin was captured with every brick alive, so it still
 * carries brick colour in cells whose brick is now destroyed. Reset
 * those to the band background across char rows [cr0, cr1] — the rows
 * the caller just re-based from level_attrs.
 *
 * The row scan runs one brick row beyond [r0, r1] on each side, because
 * cr0 doubles as row r0-1's shadow row and cr1 as row r1+1's cell row.
 * That overlap is what known-bugs #1 and #2 were. */
void reset_destroyed_cell_attrs(const u8 *cells, u8 bg_attr,
                                int r0, int r1, int cr0, int cr1);

/* The same, for brick rows [first_row, last_row] only. Callers must
 * repaint or otherwise account for the neighbouring rows' edges. */
void paint_brick_rows(const u8 *cells, int first_row, int last_row);

/* --- Edge repairs for a scoped repaint --------------------------------
 * A full ascending paint has each row overwrite parts of the one above.
 * Painting a sub-range breaks that chain at its boundaries, so these put
 * back what the row beyond the range would have written. */

/* `row`'s live bricks own the first pixel row of their body, which the
 * row above's bottom edge would otherwise have zeroed. */
void repaint_row_body_top(const u8 *cells, int row);

/* `row`'s live bricks zero the pixel row ABOVE themselves as their top
 * edge, over the previous row's last body row. */
void repaint_row_top_edge(const u8 *cells, int row);

/* `row`'s live bricks re-assert their colour over the previous row's
 * shadow, which dimmed the same cells. */
void repaint_row_attrs(const u8 *cells, int row);

#endif /* BATTY_BRICKS_H */
