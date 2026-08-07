/* scoring — extra lives from score milestones.
 *
 * orig: score_update_3 / live_add_steps $0395
 *
 * Crossing a threshold awards one life, ONCE. The original stores its
 * thresholds as BCD high bytes ($03, $06, $10 …), which read as six-digit
 * scores are 30000, 60000, 100000 and so on.
 *
 * A single score jump can cross more than one threshold — a 5000-point
 * bonus landing on 29000 crosses only the first, but scoring is
 * unbounded and nothing stops a bigger jump crossing two. So the award
 * loops rather than testing once, and the count of awards already made
 * is what makes it idempotent. */

#ifndef BATTY_SCORING_H
#define BATTY_SCORING_H

#include "types.h"

const int LIVE_ADD_COUNT = 8;
extern const unsigned long live_add_thresholds[LIVE_ADD_COUNT];

/* How many extra lives `score` has earned that have not been awarded
 * yet, given `already_awarded`. Zero once every threshold is spent. */
int lives_earned(unsigned long score, int already_awarded);

#endif /* BATTY_SCORING_H */
