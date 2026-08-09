/* Replay overrides — stage 1 of the replay/probe extraction.
 *
 * This holds the BATTY_REPLAY_* seeders that depend ONLY on state other
 * modules already own: `objects` (objects.h), the bullet and blast
 * arrays (weapons.h), and the RNG (rng.h). That is the whole boundary,
 * and it is why these five could come out while the rest could not —
 * the bonus, bomb, pts400, rocket, big-ball, multiball and brick
 * seeders write structs that still live in main.cpp, so moving them
 * means moving the state first.
 *
 * Value parsing lives one level down in replay_parse.{cpp,h}, which has
 * no game state at all and came out first (stage 1a). The split is:
 * replay_parse turns a string into numbers, replay applies them.
 */
#ifndef BATTY_REPLAY_H
#define BATTY_REPLAY_H

#include "types.h"

/* getenv + replay_parse_ints. Returns false when the variable is unset
 * or malformed, which every caller treats as "no override". */
bool replay_env_ints(const char *name, long *out, int count);

/* Overwrite an object slot from a hex byte string — the whole 22-byte
 * descriptor, so a gate can place an object exactly as the original
 * had it. */
void replay_apply_object(const char *name, u8 slot);

/* BATTY_REPLAY_RANDOM / _RANDOM_SEED: the RNG value and the ROM-walk
 * position. Both are needed for byte-exact RNG parity. */
void replay_apply_random(void);

/* BATTY_REPLAY_BOMB: drop the enemy's bomb at "x,y". Moved here once
 * bomb_launch left main.cpp — the seeder was blocked on the FUNCTION,
 * not on the state. */
void replay_apply_bomb(void);

/* BATTY_REPLAY_BULLET / _BLAST: place bullet or blast slot 0. */
void replay_apply_bullet(void);
void replay_apply_blast(void);

#endif
