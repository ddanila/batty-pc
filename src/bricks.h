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

/* How many bricks still stand between the player and the next level.
 *
 * There are TWO "is this brick there" rules in this codebase and they
 * are deliberately different:
 *
 *   BrickField::standing (level.h)  `!(cell & 0x80)`  — bit 7, DESTROYED.
 *       Used for collision. An undestructible brick is very much still
 *       there to bounce off.
 *   bricks_live_count (here)     `!(cell & 0xA0)`  — bit 7 or bit 5,
 *       destroyed OR UNDESTRUCTIBLE. Used for level completion. An
 *       undestructible brick can never be cleared, so counting it would
 *       make the level uncompletable.
 *
 * Reading one rule as the other is an easy mistake with no visible
 * symptom until a level either never ends or ends early, which is why
 * both are stated here and covered in tests/test_bricks.cpp. */
int bricks_live_count(const u8 *cells);

/* Paint every standing brick in `cells` into scr_buff / attr_buff. */
void paint_bricks(const u8 *cells);

/* Reset destroyed cells to the band background across char rows
 * [cr0, cr1] — the rows the caller just re-based from level_attrs.
 *
 * The RESET half is a no-op today: the band is generated and carries no
 * brick colour, since the port repaints live bricks at every entry. (It
 * mattered when the base band was a capture taken with every brick
 * ALIVE.) What earns its keep is the SHADOW half: a destroyed cell's
 * left char goes non-bright when its left neighbour is still live, and
 * nothing else writes that.
 *
 * The row scan runs one brick row beyond [r0, r1] on each side, because
 * cr0 doubles as row r0-1's shadow row and cr1 as row r1+1's cell row.
 * That overlap is what known-bugs #1 and #2 were. */
void reset_destroyed_cell_attrs(const u8 *cells, u8 bg_attr,
                                int r0, int r1, int cr0, int cr1);

/* The whole brick band from a level's captured attrs: re-base the band's
 * attr rows, reset the cells whose bricks are gone, then paint what is
 * still standing. The caller adds the border shadow, which is a frame
 * concern rather than a brick one. */
void paint_brick_band(const u8 *cells, const u8 *lattr, u8 bg_attr);

/* The same, scoped to brick rows [r0, r1] and the char rows [cr0, cr1]
 * they re-base — the incremental rebuild's path. Unlike the full paint
 * this must repair its own edges, because a repaint that stops at a
 * boundary does not get the next row's overwrite for free.
 *
 * Like the full paint, it leaves the border shadow to the caller. */
void paint_brick_band_rows(const u8 *cells, const u8 *lattr, u8 bg_attr,
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

/* Put back what a row-scoped repaint disturbed at its edges. A full
 * ascending paint gets these for free — each row's print overwrites the
 * previous row's edge bytes as it goes — so painting only [r0, r1]
 * leaves the boundary rows holding whatever the background repaint
 * left. Getting this wrong is what known-bugs #1 and #2 were. */
void repair_band_row_boundaries(const u8 *cells, int r0, int r1);

/* The geometry of an incremental band rebuild: which brick rows get
 * re-composited, which pixel rows the caller must repaint from
 * background first, and which pixel rows it then captures into its
 * static cache.
 *
 * The two pixel windows are NOT the same, and that is the whole reason
 * this is a function rather than four expressions at the call site.
 *
 * `py_capture0` is the top-edge row SHARED between brick row r0 and the
 * row above it: repaint_row_top_edge writes it from the row below, so
 * the rebuild has to capture it. Whether it may also be ERASED first
 * depends on whether a row above exists:
 *
 *   r0 > 0   the row above owns that pixel row's content and is not
 *            being re-composited. Erasing it would drop the neighbour's
 *            bottom edge, so repainting starts one row LOWER and print
 *            re-zeros it under r0's live bricks.
 *   r0 == 0  there is no row above. The zeros there belong to row 0's
 *            own bricks, so when one of those is destroyed nobody
 *            rewrites the row — it has to be erased with the rest.
 *
 * Missing the r0 == 0 case leaves a solid black line where the top
 * brick row used to be, baked into the static cache and therefore
 * permanent. known-bugs.md #20. */
void band_rebuild_window(int lo, int hi,
                         int *r0, int *r1,
                         int *py_capture0, int *py_paint0, int *py1,
                         int *cr0, int *cr1);

#endif /* BATTY_BRICKS_H */
