/* See enemies.h. */

#include "enemies.h"

unsigned long enemy_turn_calls      = 0;
unsigned long enemy_arrival_repicks = 0;
unsigned long enemy_margin_repicks  = 0;

namespace {
u8 (*read_current)() = 0;
u8 (*read_sample)()  = 0;
}

void enemy_set_random(u8 (*current)(), u8 (*sample)()) {
    read_current = current;
    read_sample  = sample;
}

void enemy_turn_towards_target(Object &o) {
    unsigned char target = u8(o.bonus_applied & 0x3F);
    unsigned char delta = u8((o.dir - target) & 0x3F);
    enemy_turn_calls++;
    if (delta == 0) {
        enemy_arrival_repicks++;
        o.bonus_applied = u8(read_current() & 0x3F);   // orig: LAA7D_1
        return;
    }
    if (delta & 0x20) o.dir = u8((o.dir + 1) & 0x3F);
    else             o.dir = u8((o.dir - 1) & 0x3F);
}

void enemy_pick_new_target(Object &o) {
    /* Read-current consumer (rng_sample): the original samples
     * random_number for the enemy target without its own advance. */
    o.bonus_applied = u8(read_sample() & 0x3F);
}

void enemy_target_away_from_margins(Object &o) {
    enemy_margin_repicks++;
    if (o.x_coord <= 8) {
        o.bonus_applied = (o.y_coord <= 12) ? 0x08 : 0x00;
    } else if (o.x_coord >= PLAYFIELD_W - 8 - o.w_body_px) {
        o.bonus_applied = (o.y_coord <= 12) ? 0x38 : 0x20;
    } else if (o.y_coord <= 8) {
        o.bonus_applied = (o.x_coord < PLAYFIELD_W / 2) ? 0x08 : 0x38;
    } else {
        enemy_pick_new_target(o);
    }
}
