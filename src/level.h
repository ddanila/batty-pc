/* level — the brick grid's shape. The collision sweeps and the compositor
 * must agree on where a brick is to the pixel, so the geometry lives here
 * rather than in either of them. */

#ifndef BATTY_LEVEL_H
#define BATTY_LEVEL_H

#include "types.h"

/* 15x12 cells of 16x8 px, the top-left at (8, 32). Bit 7 of a cell means
 * destroyed; every other bit is the brick's type and hit state, which
 * only the effects half cares about. */
const int BRICK_W_PX  = 16;
const int BRICK_H_PX  = 8;
const int FIELD_COLS  = 15;
const int FIELD_ROWS  = 12;
const int FIELD_X0    = 0x08;
const int FIELD_Y0    = 0x20;
/* Left/top edge of the last cell, and the first pixel past the band. The
 * original tests against these as literals ($E8, $78, $80). */
const int FIELD_X_LAST = FIELD_X0 + (FIELD_COLS - 1) * BRICK_W_PX;   /* $E8 */
const int FIELD_Y_LAST = FIELD_Y0 + (FIELD_ROWS - 1) * BRICK_H_PX;   /* $78 */
const int FIELD_Y_END  = FIELD_Y0 + FIELD_ROWS * BRICK_H_PX;         /* $80 */

ZX_STATIC_ASSERT(FIELD_X_LAST == 0xE8, "column edge must match the original");
ZX_STATIC_ASSERT(FIELD_Y_LAST == 0x78, "row edge must match the original");
ZX_STATIC_ASSERT(FIELD_Y_END  == 0x80, "band end must match the original");

class BrickField {
public:
    BrickField(const u8 *cells) : c(cells) {}
    bool in_range(int row, int col) const {
        return row >= 0 && row < FIELD_ROWS && col >= 0 && col < FIELD_COLS;
    }
    /* A cell the ball can hit. Out of range counts as gone. */
    bool standing(int row, int col) const {
        return in_range(row, col) && !(c[row * FIELD_COLS + col] & 0x80);
    }
private:
    const u8 *c;
};


#endif /* BATTY_LEVEL_H */
