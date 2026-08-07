/* See scoring.h. */

#include "scoring.h"

const unsigned long live_add_thresholds[LIVE_ADD_COUNT] = {
    30000UL, 60000UL, 100000UL, 150000UL,
    200000UL, 250000UL, 500000UL, 750000UL
};

int lives_earned(unsigned long score, int already_awarded) {
    int earned = 0;
    while (already_awarded + earned < LIVE_ADD_COUNT
           && score >= live_add_thresholds[already_awarded + earned]) {
        earned++;
    }
    return earned;
}
