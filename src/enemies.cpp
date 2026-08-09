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

void enemy_repick_target_current(Object &o) {
    /* orig LAA7D_1, reached from the handler tail when `flag_2` says
     * LAFFC hit something: `LD A,(random_number) / AND $3F /
     * LD (IX+$14),A`. That is a CURRENT read, like the arrival re-pick
     * and unlike enemy_pick_new_target's sample — see notes/rng-model.md
     * for why the two are not interchangeable.
     *
     * Deliberately not counted in any of the three enemy_repicks fields:
     * they are a fixed probe format that gates parse, and this is
     * neither an arrival nor a margin re-pick. */
    o.bonus_applied = u8(read_current() & 0x3F);
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

/* --- the brick-hit home target (orig LAA7B / LAA44) ------------------
 *
 * One global word, shared by every alien, holding a POSITION rather than
 * an angle. While it is set the alien does not steer, move or collide:
 * it walks to that position a pixel at a time and then resumes. See the
 * trace in notes/enemy-movement.md for how a brick hit fills it in. */
EnemyHomeTarget enemy_home_target = { 0, 0 };

void enemy_home_step(Object &o, EnemyHomeTarget &t) {
    /* LAA44: `LD A,L / CP $10 / JR NC / LD L,$10 / LD (LAA7B),HL` —
     * only the LOW clamp exists, and it is written back, so every later
     * call sees the clamped value. It keeps the walk out of the left
     * border; there is no matching clamp on the right or on y. */
    if (t.x < 0x10) t.x = 0x10;

    /* LAA44_0. The "too far right" case does DEC,DEC and then falls
     * THROUGH LAA44_1's INC, so it is a net -1: both directions move one
     * pixel. Equality skips the INC entirely by jumping to LAA44_2. */
    if (o.x_coord != t.x) {
        if (o.x_coord > t.x) o.x_coord = u8(o.x_coord - 2);
        o.x_coord = u8(o.x_coord + 1);
    }

    /* LAA44_2. Both y branches RET, so the arrival test below is reached
     * only on a frame where y was ALREADY equal. */
    if (o.y_coord != t.y) {
        o.y_coord = u8(o.y_coord + (o.y_coord > t.y ? -1 : 1));
        return;
    }

    /* LAA44_4: `CP L / RET NZ`, against the x we may have just moved —
     * so arrival lands on the frame x reaches the target, not the one
     * after. Clearing the word is what hands the alien back to normal
     * flight; y == 0 is the "no target" marker the caller tests. */
    if (o.x_coord != t.x) return;
    t.x = 0;
    t.y = 0;
}
