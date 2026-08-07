/* See bonus_codes.h. */

#include "bonus_codes.h"

u8 bonus_from_original(u8 code) {
    switch (code) {
        case 0x00: return BONUS_TYPE_BIG_BAT;       /* spr_bonus_size */
        case 0x01: return BONUS_TYPE_LASER;         /* spr_bonus_gun */
        case 0x02: return BONUS_TYPE_MULTI_BALL;    /* spr_bonus_triple_ball */
        case 0x03: return BONUS_TYPE_CATCH;         /* spr_bonus_hand */
        case 0x04: return BONUS_TYPE_SLOW;
        case 0x05: return BONUS_TYPE_LIFE;
        case 0x06: return BONUS_TYPE_ROCKET;        /* spr_bonus_rocket_1 */
        case 0x07: return BONUS_TYPE_BIG_BALL;
        case 0x08: return BONUS_TYPE_SCORE_5K;      /* spr_bonus_5000_points */
        case 0x09: return BONUS_TYPE_KILL_ALIENS;
        default:   return BONUS_TYPE_UNSUPPORTED;
    }
}

u8 bonus_to_original(u8 type) {
    switch (type) {
        case BONUS_TYPE_BIG_BAT:     return 0x00;
        case BONUS_TYPE_LASER:       return 0x01;
        case BONUS_TYPE_MULTI_BALL:  return 0x02;
        case BONUS_TYPE_CATCH:       return 0x03;
        case BONUS_TYPE_SLOW:        return 0x04;
        case BONUS_TYPE_LIFE:        return 0x05;
        case BONUS_TYPE_ROCKET:      return 0x06;
        case BONUS_TYPE_BIG_BALL:    return 0x07;
        case BONUS_TYPE_SCORE_5K:    return 0x08;
        case BONUS_TYPE_KILL_ALIENS: return 0x09;
        default:                     return 0xFF;
    }
}
