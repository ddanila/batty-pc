/* bonus_codes — translating between the original's bonus numbering and
 * this port's.
 *
 * The original numbers its bonuses by sprite order ($00 = the size
 * bonus, $01 = the gun, and so on). The port numbers them by its own
 * enum. Neither order is meaningful; what matters is that the two
 * translations are exact inverses, because replays seed a bonus by the
 * ORIGINAL's code and the game then acts on ours. A mismatch would drop
 * the wrong bonus and diverge silently from the frame it happened.
 *
 * Anything the port has no equivalent for maps to UNSUPPORTED rather
 * than to a plausible neighbour. */

#ifndef BATTY_BONUS_CODES_H
#define BATTY_BONUS_CODES_H

#include "types.h"

enum BonusType {
    BONUS_TYPE_LIFE        = 0,
    BONUS_TYPE_SLOW        = 1,
    BONUS_TYPE_BIG_BAT     = 2,
    BONUS_TYPE_BIG_BALL    = 3,
    BONUS_TYPE_KILL_ALIENS = 4,
    BONUS_TYPE_CATCH       = 5,
    BONUS_TYPE_ROCKET      = 6,
    BONUS_TYPE_SCORE_5K    = 7,
    BONUS_TYPE_LASER       = 8,
    BONUS_TYPE_MULTI_BALL  = 9,
    BONUS_TYPE_COUNT       = 10
};

const u8 BONUS_TYPE_UNSUPPORTED = 0xFF;
const u8 BONUS_ORIG_NONE        = 0xFF;

/* The original's code -> ours, and back. Exact inverses over every
 * supported bonus. */
u8 bonus_from_original(u8 original_code);
u8 bonus_to_original(u8 type);

#endif /* BATTY_BONUS_CODES_H */
