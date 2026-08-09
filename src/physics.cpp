/* See physics.h. */

#include <stdlib.h>

#include "physics.h"

namespace {

/* orig: direction_table $AD58 — 17 sine values in [0, $FF]. */
const u8 dir_sin_tbl[17] = {
    0xFF,0xFD,0xFA,0xF4,0xE6,0xE0,0xD4,0xC5,
    0xB4,0xA1,0x8D,0x78,0x61,0x4A,0x31,0x18,
    0x00
};

/* orig: LABEE / LABFC — (threshold, zone) pairs per bat width. */
const u8 zone_tbl_normal[14] = {
    0x04,0x07, 0x08,0x06, 0x0C,0x05, 0x10,0x00,
    0x14,0x01, 0x18,0x02, 0xFF,0x03
};
const u8 zone_tbl_big[14] = {
    0x06,0x07, 0x0C,0x06, 0x12,0x05, 0x1A,0x00,
    0x20,0x01, 0x26,0x02, 0xFF,0x03
};

/* orig: LAC0A — [zone & 3][incoming dir index]. */
const u8 deflect_tbl[4][6] = {
    {0x3C,0x38,0x34,0x2C,0x28,0x24},
    {0x3C,0x38,0x34,0x34,0x34,0x34},
    {0x3C,0x38,0x38,0x34,0x38,0x38},
    {0x3C,0x3C,0x38,0x38,0x3C,0x3C}
};

const int DIR_QUADRANT = 0x30;
const int DIR_ANGLE    = 0x0F;
const int BALL_SPEED   = 2;

}  /* namespace */

ZX_STATIC_ASSERT(sizeof(zone_tbl_normal) == sizeof(zone_tbl_big),
                 "both bat widths use the same number of zones");

void dir_to_dxdy(u8 dir, u8 speed, int *out_dx, int *out_dy) {
    const u8 angle = u8(dir & DIR_ANGLE);
    const int c = dir_sin_tbl[angle];
    const int l = dir_sin_tbl[16 - angle];

    int hl, bc;
    switch (dir & DIR_QUADRANT) {
        case 0x00: hl =  l; bc =  c; break;
        case 0x10: hl =  c; bc = -l; break;
        case 0x20: hl = -l; bc = -c; break;
        default:   hl = -c; bc =  l; break;
    }
    /* Crossed on purpose — see physics.h. */
    *out_dx = bc * int(speed);
    *out_dy = hl * int(speed);
}

void dir_to_delta(u8 dir, int *dx, int *dy) {
    int mag_x, mag_y;
    switch (dir & DIR_ANGLE) {
        case 0x04: mag_x = 2; mag_y = 1; break;
        case 0x08: mag_x = 1; mag_y = 1; break;
        default:   mag_x = 1; mag_y = 2; break;
    }
    switch (dir & DIR_QUADRANT) {
        case 0x00: *dx =  mag_x; *dy =  mag_y; break;
        case 0x10: *dx =  mag_x; *dy = -mag_y; break;
        case 0x20: *dx = -mag_x; *dy = -mag_y; break;
        default:   *dx = -mag_x; *dy =  mag_y; break;
    }
}

u8 delta_to_dir(int dx, int dy) {
    u8 quadrant;
    if      (dx >= 0 && dy >= 0) quadrant = 0x00;
    else if (dx >= 0)            quadrant = 0x10;
    else if (dy < 0)             quadrant = 0x20;
    else                         quadrant = 0x30;
    const u8 angle = (abs(dx) >= BALL_SPEED) ? 0x08 : 0x04;
    return u8(quadrant | angle);
}

u8 bat_reflect_dir(u8 dir) {
    return u8(((dir ^ 0x1F) + 1) & 0x3F);
}

int bat_dir_index(u8 dir) {
    int candidate = 0x04;
    for (int index = 0; index < 6; index++) {
        if (u8(candidate) == dir) return index;
        candidate += 4;
        if (candidate == 0x10) candidate += 4;   /* skips pure vertical */
    }
    return -1;
}

u8 bat_deflect_dir(u8 dir, int offset, bool big_bat) {
    const u8 *zones = big_bat ? zone_tbl_big : zone_tbl_normal;

    u8 zone;
    if (offset < 0) {
        zone = zones[1];                      /* orig: LAB1F_5 carry path */
    } else {
        int i = 0;
        while (i < 12 && u8(offset) >= zones[i]) i += 2;
        zone = zones[i + 1];
    }

    if (zone & 0x04) {                        /* orig: LAB1F_8 */
        dir = bat_reflect_dir(dir);
        const int index = bat_dir_index(dir);
        if (index < 0) return bat_reflect_dir(dir);
        return bat_reflect_dir(deflect_tbl[zone & 3][index]);
    }

    const int index = bat_dir_index(dir);     /* orig: LAB1F_10 */
    if (index < 0) return bat_reflect_dir(dir);
    return deflect_tbl[zone & 3][index];
}

/* --- Collision sweeps -------------------------------------------------- */

BrickHit brick_sweep(const BrickField &field, int ball_w, int ball_h,
                     int prev_x, int prev_y, int new_x, int new_y) {
    BrickHit miss = { false, -1, -1, 0 };

    const int left   = new_x;
    const int right  = new_x + ball_w - 1;
    const int top    = new_y;
    const int bottom = new_y + ball_h - 1;

    if (bottom < FIELD_Y0 || top >= FIELD_Y_END) return miss;
    if (right < FIELD_X0 ||
        left >= FIELD_X0 + FIELD_COLS * BRICK_W_PX) return miss;

    const int col0 = (left < FIELD_X0) ? 0 : (left - FIELD_X0) / BRICK_W_PX;
    const int col1 = (right >= FIELD_X0 + FIELD_COLS * BRICK_W_PX)
                   ? FIELD_COLS - 1 : (right - FIELD_X0) / BRICK_W_PX;
    const int row0 = (top < FIELD_Y0) ? 0 : (top - FIELD_Y0) / BRICK_H_PX;
    const int row1 = (bottom >= FIELD_Y_END)
                   ? FIELD_ROWS - 1 : (bottom - FIELD_Y0) / BRICK_H_PX;

    int row = -1, col = -1;
    for (int r = row0; r <= row1 && row < 0; r++) {
        for (int c = col0; c <= col1; c++) {
            if (field.standing(r, c)) { row = r; col = c; break; }
        }
    }
    if (row < 0) return miss;

    /* Which axis reflects: whichever face the ball was outside of last
     * frame. If it was already overlapping both, the shallower overlap
     * wins. */
    const int brick_top   = FIELD_Y0 + row * BRICK_H_PX;
    const int brick_bot   = brick_top + BRICK_H_PX;
    const int brick_left  = FIELD_X0 + col * BRICK_W_PX;
    const int brick_right = brick_left + BRICK_W_PX;

    int axis;
    if (prev_y + ball_h <= brick_top || prev_y >= brick_bot) {
        axis = 1;
    } else if (prev_x + ball_w <= brick_left || prev_x >= brick_right) {
        axis = 2;
    } else {
        const int overlap_x = (right < brick_right ? right : brick_right - 1)
                            - (left > brick_left ? left : brick_left) + 1;
        const int overlap_y = (bottom < brick_bot ? bottom : brick_bot - 1)
                            - (top > brick_top ? top : brick_top) + 1;
        axis = (overlap_y <= overlap_x) ? 1 : 2;
    }

    BrickHit hit = { true, row, col, axis };
    return hit;
}

u8 laffc_change_dir(u8 dir, u8 mask) {
    return u8(((dir ^ mask) + 1) & 0x3F);
}

LaffcHit laffc_sweep(const BrickField &field, u8 dir,
                     int ball_w, int ball_h, int new_x, int new_y) {
    LaffcHit miss = { false, -1, -1, 0, 0, 0 };

    if (new_y >= FIELD_Y_END)        return miss;
    if (new_y + ball_h < FIELD_Y0)   return miss;

    /* Row band. The original walks rows comparing in 8-bit arithmetic, so
     * the borrow case is a separate branch rather than a signed compare. */
    int row = -1, cell_y = 0;
    {
        int band_y = FIELD_Y0;
        for (int r = 0; r < FIELD_ROWS; r++) {
            const int a = (band_y - new_y) & 0xFF;
            if (new_y > band_y) {
                if (a + BRICK_H_PX > 0xFF) { row = r; cell_y = band_y; break; }
            } else {
                if (a < ball_h)            { row = r; cell_y = band_y; break; }
            }
            band_y += BRICK_H_PX;
        }
    }
    if (row < 0) return miss;

    /* Column, plus how far into the cell the ball has penetrated in x. */
    int col = 0, cell_x = FIELD_X0, x_pen_in_cell = 0;
    {
        int a = new_x - FIELD_X0;
        if (a < 0) a = 0;
        while (a >= BRICK_W_PX && col < FIELD_COLS - 1) {
            a -= BRICK_W_PX; col++; cell_x += BRICK_W_PX;
        }
        x_pen_in_cell = a;
    }

    /* The ball's body can straddle into the next column or row, so if its
     * own cell is gone the original tries right, down, then down-right. */
    if (!field.standing(row, col)) {
        const bool straddles_x = (x_pen_in_cell + ball_w) >= BRICK_W_PX
                              && cell_x != FIELD_X_LAST;
        const bool straddles_y = (new_y + ball_h - cell_y) >= BRICK_H_PX
                              && cell_y < FIELD_Y_LAST;
        bool landed = false;
        if (straddles_x && field.standing(row, col + 1)) {
            col++; cell_x += BRICK_W_PX; landed = true;
        } else if (straddles_y) {
            row++; cell_y += BRICK_H_PX;
            if (field.standing(row, col)) {
                landed = true;
            } else if (straddles_x && field.standing(row, col + 1)) {
                col++; cell_x += BRICK_W_PX; landed = true;
            }
        }
        if (!landed) return miss;
    }

    /* Open faces: a face is open when its neighbour is gone, OR when the
     * cell sits against that playfield boundary. The boundary term is the
     * half that known-bugs #6 had inverted. */
    u8 mask = 0;
    if (cell_x == FIELD_X0     || !field.standing(row, col - 1)) mask |= 1;
    if (cell_x == FIELD_X_LAST || !field.standing(row, col + 1)) mask |= 2;
    if (cell_y <  FIELD_Y0 + 1 || !field.standing(row - 1, col)) mask |= 4;
    if (cell_y >= FIELD_Y_LAST || !field.standing(row + 1, col)) mask |= 8;

    /* Only faces the ball is actually travelling towards can be hit. */
    if (dir < 0x20) mask &= u8(~8); else mask &= u8(~4);
    if (((dir + 0x10) & 0x3F) >= 0x20) mask &= u8(~1); else mask &= u8(~2);

    /* If both a horizontal and a vertical face survive, the ball came in
     * through the one it has penetrated least. */
    if ((mask & 0x03) && (mask & 0x0C)) {
        int x_pen = (mask & 1) ? ((ball_w + new_x) - cell_x)
                               : ((cell_x + BRICK_W_PX) - new_x);
        int y_pen = (mask & 4) ? ((ball_h + new_y) - cell_y)
                               : ((cell_y + BRICK_H_PX) - new_y);
        x_pen &= 0xFF; y_pen &= 0xFF;
        if (y_pen >= x_pen) mask &= u8(~0x0C); else mask &= u8(~0x03);
    }

    LaffcHit hit = { true, row, col, cell_x, cell_y, mask };
    return hit;
}

BallBounce laffc_bounce(const LaffcHit &hit, u8 dir,
                        int ball_w, int ball_h, int new_x, int new_y) {
    BallBounce out;
    if (hit.face_mask & 1) {                    /* open left */
        out.x = u8(hit.cell_x - ball_w);
        out.y = u8(new_y);
        out.dir = laffc_change_dir(dir, 0x1F);
    } else if (hit.face_mask & 2) {             /* open right */
        out.x = u8(hit.cell_x + BRICK_W_PX);
        out.y = u8(new_y);
        out.dir = laffc_change_dir(dir, 0x1F);
    } else if (hit.face_mask & 4) {             /* open up */
        out.x = u8(new_x);
        out.y = u8(hit.cell_y - ball_h);
        out.dir = laffc_change_dir(dir, 0x3F);
    } else {                                    /* open down, or enclosed */
        out.x = u8(new_x);
        out.y = u8(hit.cell_y + BRICK_H_PX);
        out.dir = laffc_change_dir(dir, 0x3F);
    }
    return out;
}

int bat_step_x(int bat_x, int extra_px, bool move_left, bool move_right) {
    const int min_x = BAT_MARGIN_LEFT + extra_px;
    const int max_x = BAT_MARGIN_RIGHT - BAT_BODY_W - extra_px;

    /* int throughout: the caller's x is a u8 and would wrap on the
     * subtract before the clamp ever saw it. */
    if (move_left)  bat_x -= 4;
    if (move_right) bat_x += 4;

    if (bat_x < min_x) bat_x = min_x;
    if (bat_x > max_x) bat_x = max_x;
    return bat_x;
}

ExtraBallDirs extra_ball_dirs(u8 base_dir) {
    const u8 quadrant = (u8)(base_dir & 0x30);
    const u8 low      = (u8)(base_dir & 0x0F);
    ExtraBallDirs out;
    if (low == 0x04) {
        out.second = (u8)(quadrant | 0x0C);
        out.third  = (u8)(quadrant | 0x08);
    } else if (low == 0x08) {
        out.second = (u8)(quadrant | 0x0C);
        out.third  = (u8)(quadrant | 0x04);
    } else {
        out.second = (u8)(quadrant | 0x08);
        out.third  = (u8)(quadrant | 0x04);
    }
    return out;
}

int motion_accel_step(motion_acc_t *m, unsigned int de, unsigned char cap_hi) {
    unsigned int acc = (unsigned int)(m->acc + de);
    unsigned int sum;
    if ((unsigned char)(acc >> 8) == cap_hi) acc = (unsigned int)cap_hi << 8;
    m->acc = acc;
    sum = acc + m->frac;
    m->frac = (unsigned char)sum;
    return (int)((signed char)(sum >> 8));
}
