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
