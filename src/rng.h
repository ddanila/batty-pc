/* rng — the original's random_generate, bit for bit.
 *
 * orig: random_generate $0072, random_number $8D48, random_seed $8D4A
 *
 * Each tick walks one byte of the game's own $8000-$9FFF image and folds
 * it into a 16-bit accumulator. That image is an input, not a detail:
 * feed it a different 8 KB and the sequence changes, which is why
 * assets/random_seed.bin is a shipped asset.
 *
 * The original ticks this ONCE PER FRAME at the main-loop top, and
 * consumers then read the current value without advancing. Callers that
 * the original advances-then-reads call next() directly; the rest read
 * current(). Getting that distinction wrong consumes the sequence faster
 * than the original and desynchronises everything downstream of it —
 * enemy steering, bonus drops. See notes/rng-model.md. */

#ifndef BATTY_RNG_H
#define BATTY_RNG_H

#include "types.h"

const int RANDOM_ROM_SIZE = 0x2000;      /* the $8000-$9FFF walk table */

/* The walk table. Not copied — must outlive the last rng call. */
void rng_set_rom(const u8 *rom);

/* `number` is the D:E pair as the original stores it; `seed_addr` is the
 * walk position, which is clamped into $8000-$9FFF exactly as the
 * original's wrap does. */
void rng_seed(u16 number, u16 seed_addr);

/* Advance one tick and return the new value. `ctrl_buttons` is the
 * original's control-button byte, folded into the low half — real input
 * perturbs the sequence, which is why it is a parameter rather than
 * something this module reaches for. */
u16 rng_next(u8 ctrl_buttons);

/* Read without advancing. */
u16 rng_current();
u16 rng_seed_addr();

inline u8 rng_high(u16 r) { return u8(r >> 8); }
inline u8 rng_low(u16 r)  { return u8(r); }

#endif /* BATTY_RNG_H */
