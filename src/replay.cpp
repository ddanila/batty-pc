#include <stdlib.h>
#include <string.h>

#include "replay.h"
#include "replay_parse.h"
#include "objects.h"
#include "weapons.h"
#include "rng.h"

bool replay_env_ints(const char *name, long *out, int count) {
    return replay_parse_ints(getenv(name), out, count);
}

void replay_apply_object(const char *name, u8 slot) {
    u8 bytes[sizeof(Object)];
    if (!replay_parse_hex_bytes(getenv(name), bytes, (int)sizeof(bytes))) return;
    memcpy(&objects[slot], bytes, sizeof(bytes));
}

void replay_apply_random(void) {
    const char *p = getenv("BATTY_REPLAY_RANDOM");
    const char *s;
    char *endp;
    unsigned long v;
    if (p != NULL && *p != '\0') {
        v = strtoul(p, &endp, 16);
        if (*endp == '\0' && v <= 0xFFFFUL) {
            rng_seed(u16(v), rng_seed_addr());
        }
    }
    /* Also seed the ROM-walk position (the original's random_seed at
     * $8D4A). Needed for byte-exact RNG-dependent parity: with both
     * random_number ($8D48) and random_seed seeded to the original's
     * frame-0 values, next_random reproduces random_generate frame for
     * frame (validated offline against the original's $8D48 sequence —
     * see notes/rng-model.md). Without it the walk reads a different ROM
     * offset. Only the low 14 bits matter ($8000-$9FFF). */
    s = getenv("BATTY_REPLAY_RANDOM_SEED");
    if (s != NULL && *s != '\0') {
        v = strtoul(s, &endp, 16);
        if (*endp == '\0' && v <= 0xFFFFUL) {
            rng_seed(rng_current(), u16(v));
        }
    }
}

void replay_apply_bullet(void) {
    long v[2];
    if (!replay_env_ints("BATTY_REPLAY_BULLET", v, 2)) return;
    bullet_active[0] = 1;
    bullet_frame[0] = 0;
    bullet_x[0] = (int)v[0];
    bullet_y[0] = (int)v[1];
}

void replay_apply_blast(void) {
    long v[2];
    if (!replay_env_ints("BATTY_REPLAY_BLAST", v, 2)) return;
    bullet_blast_ticks[0] = BULLET_BLAST_FRAMES * BULLET_BLAST_TICKS_PER_FRAME;
    bullet_blast_x[0] = (int)v[0];
    bullet_blast_y[0] = (int)v[1];
}
