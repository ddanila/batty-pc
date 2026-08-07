/* See rng.h. */

#include "rng.h"

namespace {

const u8 *walk_rom = 0;

/* The original keeps random_number as the register pair D:E. */
u8  number_high = 0x8E;
u8  number_low  = 0x17;
u16 seed_addr   = 0x8000;

/* The walk covers $8000-$9FFF and wraps back into it, so the address
 * never leaves the image. orig: `INC HL / RES 6,H / SET 7,H`. */
inline u16 advance_seed_addr(u16 addr) {
    addr = u16((addr + 1) & 0x9FFF);
    return addr < 0x8000 ? u16(addr | 0x8000) : addr;
}

}  /* namespace */

ZX_STATIC_ASSERT(RANDOM_ROM_SIZE == 0x9FFF - 0x8000 + 1,
                 "the walk table covers exactly $8000-$9FFF");

void rng_set_rom(const u8 *rom) { walk_rom = rom; }

void rng_seed(u16 number, u16 addr) {
    number_high = rng_high(number);
    number_low  = rng_low(number);
    seed_addr   = addr < 0x8000 ? u16((addr & 0x9FFF) | 0x8000)
                                : u16(addr & 0x9FFF);
}

u16 rng_next(u8 ctrl_buttons) {
    const u8 src = walk_rom ? walk_rom[seed_addr & (RANDOM_ROM_SIZE - 1)] : 0;
    number_low  = u8(number_low  + src + 0x05 + ctrl_buttons);
    number_high = u8(number_high + u8(~src) + 0x16 + u8(seed_addr));
    seed_addr   = advance_seed_addr(seed_addr);
    return rng_current();
}

u16 rng_current()   { return u16((u16(number_high) << 8) | number_low); }
u16 rng_seed_addr() { return seed_addr; }
