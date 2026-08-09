/* enemies — how the alien picks a heading and turns onto it.
 *
 * orig: LAA7D and its neighbours
 *
 * The alien does not fly toward a POINT; it flies toward a DIRECTION.
 * It holds a target angle in the descriptor's `bonus_applied` byte
 * (that slot is unused for enemies) and each frame turns one 6-bit step
 * toward it, choosing the shorter way round. On arrival it picks a new
 * target at random, which is what makes its path wander.
 *
 * Near a wall it stops choosing randomly and aims at a fixed angle that
 * leads away, so it cannot grind along an edge.
 *
 *
 * THE RNG DISTINCTION MATTERS
 * ---------------------------
 * Two different reads are used, and they are not interchangeable:
 *
 *   arrival at a target  reads the CURRENT random number without
 *                        advancing it
 *   picking a new target SAMPLES, which may or may not advance
 *                        depending on the model in force
 *
 * The original ticks its RNG once per frame and lets consumers read it;
 * a consumer that advances on read eats the sequence faster and
 * desynchronises everything downstream. Both are injected here rather
 * than reached for, so this distinction is visible instead of implied.
 * See notes/rng-model.md. */

#ifndef BATTY_ENEMIES_H
#define BATTY_ENEMIES_H

#include "objects.h"
#include "zxvga.h"   /* PLAYFIELD_W — the margins are screen-relative */
#include "types.h"

/* `current` must read without advancing; `sample` follows the game's
 * model. Both return the low byte of the random number. */
void enemy_set_random(u8 (*current)(), u8 (*sample)());

/* Turn one step toward the held target, the shorter way round. On
 * arrival, pick a fresh target. */
void enemy_turn_towards_target(Object &o);

/* Choose a target at random. */
void enemy_pick_new_target(Object &o);

/* Aim away from whichever edge the alien is against; if it is not
 * against one, choose at random as usual. */
void enemy_target_away_from_margins(Object &o);

/* The brick-hit home target: the original's LAA7B, a single global word
 * shared by every alien (L = x at $AA7B, H = y at $AA7C). `y == 0` means
 * "no target" — the original tests only H (`LD HL,(LAA7B) / LD A,H /
 * AND A / JR Z`), so a target with y == 0 could not be expressed anyway.
 *
 * An alien that hits a brick is not destroyed and does not destroy: it
 * is put back where it was and given the position the collision would
 * have snapped it to, which it then walks to. */
struct EnemyHomeTarget { u8 x, y; };
extern EnemyHomeTarget enemy_home_target;

/* One step of that walk (orig LAA44). At most one pixel on each axis;
 * clears `t` on arrival, which is what ends the mode. */
void enemy_home_step(Object &o, EnemyHomeTarget &t);

/* How often each path is taken — read by the render profile. */
extern unsigned long enemy_turn_calls;
extern unsigned long enemy_arrival_repicks;
extern unsigned long enemy_margin_repicks;

#endif /* BATTY_ENEMIES_H */
