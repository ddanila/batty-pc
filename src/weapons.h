/* weapons — the bat's laser: bullets in flight and their impact blasts.
 *
 * orig: LA5A3 (alien hit), LAFFC_31 (brick hit), the bullet slots at
 * object_bullet_1/2
 *
 * Two bullets at most, because the original has two slots. A bullet
 * travels straight up until it reaches something or leaves the screen;
 * on impact it becomes a four-frame blast at the point of contact.
 *
 * As with brick collision, this module answers WHAT a bullet reached,
 * not what that costs. Scoring, brick damage and killing the alien stay
 * with the game, because none of them is geometry.
 *
 * One detail that is geometry: a blast snaps to the 8-pixel column grid
 * (the sprite is byte-aligned), so the impact point is rounded down
 * rather than used raw. */

#ifndef BATTY_WEAPONS_H
#define BATTY_WEAPONS_H

#include "level.h"
#include "objects.h"
#include "types.h"

const int N_BULLETS   = 2;
const int BULLET_BODY_W = 4;
const int BULLET_BODY_H = 8;
const int BULLET_SPEED  = 6;

/* Four frames, two ticks each. */
const int BULLET_BLAST_FRAMES          = 4;
const int BULLET_BLAST_TICKS_PER_FRAME = 2;

/* --- State ------------------------------------------------------------ */

extern u8  bullet_active[N_BULLETS];
/* Each bullet's own animation index, advanced once per step. The two
 * bullet sprites share their pixels and differ only in mask, so this
 * selects a one-pixel transparency shimmer. Per-bullet, not shared:
 * the original indexes the frame table at $77E6 by the object's own
 * sprite_num, the same way the bird and UFO animate (known-bugs #10). */
extern u8  bullet_frame[N_BULLETS];
extern int bullet_x[N_BULLETS];
extern int bullet_y[N_BULLETS];

extern int bullet_blast_ticks[N_BULLETS];
extern int bullet_blast_x[N_BULLETS];
extern int bullet_blast_y[N_BULLETS];

bool any_bullet_active();
bool any_bullet_blast();

/* --- Motion ----------------------------------------------------------- */

/* What a bullet reached this step. */
struct BulletHit {
    enum What { NOTHING, LEFT_SCREEN, ENEMY, BRICK };
    What what;
    int  row, col;          /* BRICK only */
};

/* Advance bullet `i` one step and report what it reached. The bullet is
 * deactivated and its blast started for every outcome except NOTHING and
 * LEFT_SCREEN (which leaves no blast — nothing was struck).
 *
 * `enemy` is consulted but not modified; applying the kill is the
 * caller's business. */
BulletHit bullet_advance(int i, const Object &enemy, const BrickField &field);

/* Age every blast by one tick. */
void bullet_blasts_tick();

/* Clear every bullet and blast — level change, death, level clear. */
void bullets_clear();

#endif /* BATTY_WEAPONS_H */
