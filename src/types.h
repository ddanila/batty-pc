/* Shared scalar types and compile-time assertions. */

#ifndef BATTY_TYPES_H
#define BATTY_TYPES_H

/* Fixed-width aliases. Watcom's 32-bit `long` is 4 bytes but a 64-bit
 * host's is 8, which silently doubles the width of a store through a
 * cast — use these in anything the host test build also compiles. */
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;      /* 4 bytes on both */

/* Open Watcom's C++ is C++98 plus static_assert (behind -zastd=c++0x);
 * the host build is strict C++98, which has neither. One macro so the
 * assertions read the same in both, and cost nothing in either. */
#if defined(__WATCOMC__)
#  define ZX_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#  define ZX_SA_CAT_(a, b) a##b
#  define ZX_SA_CAT(a, b)  ZX_SA_CAT_(a, b)
#  define ZX_STATIC_ASSERT(cond, msg) \
       typedef char ZX_SA_CAT(zx_static_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

#endif /* BATTY_TYPES_H */
