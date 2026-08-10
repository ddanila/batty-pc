/* weapons — things in flight: the bat's laser bullets, their impact
 * blasts, and the enemy's bomb.
 *
 * orig: LA5A3 (alien hit), LAFFC_31 (brick hit), the bullet slots at
 * object_bullet_1/2
 *
 * Two bullets at most, because the original has two slots.
 *
 * A blast snaps to the 8-pixel column grid (the sprite is byte-aligned), so
 * the impact point is rounded down rather than used raw.
 *
 * The original shares object_bonus between the bomb and a falling bonus;
 * the port keeps them separate. */

#ifndef BATTY_WEAPONS_H
#define BATTY_WEAPONS_H

#include "level.h"
#include "physics.h"
#include "objects.h"
#include "types.h"

const int N_BULLETS   = 2;
const int BULLET_BODY_W = 4;
const int BULLET_BODY_H = 8;
const int BULLET_SPEED  = 6;

const int BULLET_BLAST_FRAMES          = 4;
const int BULLET_BLAST_TICKS_PER_FRAME = 2;

/* --- State ------------------------------------------------------------ */

extern u8  bullet_active[N_BULLETS];
/* Each bullet's own animation index, advanced once per step. The two bullet
 * sprites share their pixels and differ only in mask, so this selects a
 * one-pixel transparency shimmer. Per-bullet, not shared: the original
 * indexes the frame table at $77E6 by the object's own sprite_num, the same
 * way the bird and UFO animate (known-bugs #10). */
extern u8  bullet_frame[N_BULLETS];
extern int bullet_x[N_BULLETS];
extern int bullet_y[N_BULLETS];

extern int bullet_blast_ticks[N_BULLETS];
extern int bullet_blast_x[N_BULLETS];
extern int bullet_blast_y[N_BULLETS];

bool any_bullet_active();
bool any_bullet_blast();

/* --- Motion ----------------------------------------------------------- */

struct BulletHit {
    enum What { NOTHING, LEFT_SCREEN, ENEMY, BRICK };
    What what;
    int  row, col;          /* BRICK only */
};

/* Advance bullet `i` one step and report what it reached. The bullet is
 * deactivated and its blast started for every outcome except NOTHING and
 * LEFT_SCREEN (which leaves no blast — nothing was struck).
 *
 * `enemy` is consulted but not modified; applying the kill is the caller's
 * business. */
BulletHit bullet_advance(int i, const Object &enemy, const BrickField &field);

void bullet_blasts_tick();

/* Clear every bullet and blast — level change, death, level clear. */
void bullets_clear();

/* The enemy's bomb: one at a time, as in the original.
 * orig: bomb_appear $A977, which sets the body to 8x8. */
struct BombState {
    u8           active;
    int          x, y;
    motion_acc_t motion;
};
extern BombState bomb;

/* Drop a bomb at (x, y), with the fall accumulator reset so it starts from
 * rest rather than inheriting the last bomb's speed. */
void bomb_launch(int x, int y);

/* Advance the fall one frame, deactivating the bomb once it is past
 * `bottom_y`. Does NOT test the bat — that costs a life, which is a game
 * rule. */
void bomb_fall_step(int bottom_y);

#endif /* BATTY_WEAPONS_H */
