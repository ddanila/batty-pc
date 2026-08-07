/* See objects.h. */

#include "objects.h"

/* Port of ix_buf_addr_calc at $B684. Computes the scr_buff offset
 * from (x_coord, y_coord) and stores it in the descriptor's +0A/+0B
 * fields. Our scr_buff is row-major (32 B per row, 192 rows), so the
 * offset is y*32 + x/8. We pack big-endian into +0A:+0B to match the
 * original's H:L convention. */
void object_update_buffer_offset(Object &o) {
    unsigned int off = (unsigned int)o.y_coord * 32u
                     + (unsigned int)(o.x_coord >> 3);
    o.buf_addr_hi = u8((off >> 8) & 0xFF);
    o.buf_addr_lo = u8(off & 0xFF);
}

void object_reflect(Object &o, bool flip_x, bool flip_y) {
    u8 dir = o.dir;
    /* Port of the original's bounce_wall ($AC75) -> change_direction
     * ($ACEE): dir = ((dir ^ mask) + 1) & 0x3F. The original passes
     * mask=$1F for the LEFT/RIGHT walls (horizontal bounce, negate dx) and
     * mask=$3F for the TOP wall (vertical bounce, negate dy) — the SAME
     * change_direction LAFFC uses for brick faces (laffc_change_dir).
     *
     * The previous formulas (flip_x: 0x3F-dir, flip_y: 0x1F-dir) had the
     * axes SWAPPED and were off by one: a ball hitting a side wall kept its
     * dx pointing into the wall (e.g. dir 0x20 = (-255,0) -> 0x3F-0x20 =
     * 0x1F = (-253,+24), still moving left), so it pinned at x=8 / x=240 and
     * juggled its dy forever. Now matched to change_direction. */
    if (flip_x) dir = u8(((dir ^ 0x1F) + 1) & 0x3F);
    if (flip_y) dir = u8(((dir ^ 0x3F) + 1) & 0x3F);
    o.dir = dir;
}

void object_step_animation(Object &o) {
    u8 a = o.misc_12;
    if (a >= 0x40) {
        o.misc_12 = u8(a - 0x40);
        return;
    }
    {
        u8 e = u8((o.sprite_num & 0x3F) + 1);
        u8 d = o.misc_13;
        if (u8((d >> 4) & 0x0F) < e)
            e = u8(d & 0x0F);
        o.sprite_num = e;
    }
    o.misc_12 = u8(((a << 2) & 0xC0) | a);
}
