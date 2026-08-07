/* objects — the moving-object descriptor and the slot table.
 *
 * orig: the 22-byte property blocks at $9AD0..$9BC1
 *
 * Everything that moves — balls, bullets, the bat, the bonus, the enemy,
 * the rocket — is one of eleven fixed slots holding the same 22-byte
 * descriptor. The original walks them by advancing IX in steps of $16, so
 * both the size and the field offsets are load-bearing: they are asserted
 * below rather than trusted.
 *
 * The slot ORDER is load-bearing too. call_hl_for_all_obj starts at
 * object_ball_1 and walks eleven slots, so anything that iterates has to
 * see them in this order to consume the RNG and resolve collisions the
 * way the original does. */

#ifndef BATTY_OBJECTS_H
#define BATTY_OBJECTS_H

#include "types.h"

/* Field comments give the original's offset, since that is how the disasm
 * refers to them. */
struct Object {
    u8 sprite_set;      /* +00  which sprite set; BIT7 = slot inactive */
    u8 sprite_num;      /* +01  frame within the set */
    u8 x_coord;         /* +02 */
    u8 x_coord_hi;      /* +03 */
    u8 y_coord;         /* +04 */
    u8 y_coord_hi;      /* +05 */
    u8 dir;             /* +06  6-bit angle, as physics.h describes */
    u8 speed;           /* +07 */
    u8 w_shadow;        /* +08  width in bytes, including the drop shadow */
    u8 h_shadow;        /* +09  height in px, including the drop shadow */
    u8 buf_addr_hi;     /* +0A  scr_buff offset, big-endian to match H:L */
    u8 buf_addr_lo;     /* +0B */
    u8 w_body_px;       /* +0C  the part that collides */
    u8 h_body_px;       /* +0D */
    u8 prev_x;          /* +0E  last frame, for the erase pass */
    u8 prev_y;          /* +0F */
    u8 prev_w_shadow;   /* +10 */
    u8 prev_h_shadow;   /* +11 */
    u8 misc_12;         /* +12  animation pace (enemy) */
    u8 misc_13;         /* +13  animation range (enemy) / 3-ball slow */
    u8 bonus_applied;   /* +14  $FF = none */
    u8 bat_props;       /* +15  BIT0 expanded, BIT1 expanding,
                         *      BIT5 expansion running, BIT6 reduction
                         *      running, BIT7 not transforming */
};

ZX_STATIC_ASSERT(sizeof(Object) == 0x16,
                 "the descriptor must match the original's $16 stride");

/* Slot indices, in the order call_hl_for_all_obj walks them. */
enum ObjectSlot {
    OBJ_BALL_1   = 0,
    OBJ_BALL_2   = 1,
    OBJ_BALL_3   = 2,
    OBJ_BULLET_1 = 3,
    OBJ_BULLET_2 = 4,
    OBJ_BAT_2    = 5,
    OBJ_BAT_1    = 6,
    OBJ_BAT_TEMP = 7,
    OBJ_BONUS    = 8,
    OBJ_ENEMY    = 9,
    OBJ_ROCKET   = 10,
    N_OBJECTS    = 11
};

extern Object objects[N_OBJECTS];

/* BIT7 of sprite_set marks a slot inactive — off-screen, or simply not
 * processed this frame. */
inline bool object_active(const Object &o) { return (o.sprite_set & 0x80) == 0; }
inline void object_activate(Object &o)     { o.sprite_set = u8(o.sprite_set & 0x7F); }
inline void object_deactivate(Object &o)   { o.sprite_set = u8(o.sprite_set | 0x80); }

/* --- Pure helpers ----------------------------------------------------- */

/* Recompute the descriptor's cached scr_buff offset from its position.
 * orig: ix_buf_addr_calc $B684 */
void object_update_buffer_offset(Object &o);

/* Bounce: dir = ((dir ^ mask) + 1) & 0x3F, mask $1F horizontal, $3F
 * vertical — the same change_direction the brick faces use. The axes are
 * easy to swap; doing so leaves a ball pinned against a wall juggling its
 * other component forever. orig: bounce_wall $AC75 */
void object_reflect(Object &o, bool flip_x, bool flip_y);

/* Advance one animation frame, if this object's pace says it is due.
 * misc_12 carries both the countdown and the reload in one byte. */
void object_step_animation(Object &o);

#endif /* BATTY_OBJECTS_H */
