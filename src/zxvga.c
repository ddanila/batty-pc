/* zxvga — ZX Spectrum display emulation on VGA mode 13h.
 *
 * Read zxvga.h first: it documents the attribute/colour-clash model this
 * file implements and the invariants callers rely on.
 *
 * Sections:
 *   §1  VGA surface        — mode set, DAC palette, rectangle fill
 *   §2  Attribute model    — attr byte -> ink/paper -> expansion tables
 *   §3  ZX framebuffer     — scr_buff / attr_buff
 *   §4  Clash blit to VGA  — the two planes -> 8bpp pixels
 *   §5  Dirty rectangles   — track and flush only what changed
 *   §6  Masked blits       — pixels-only (clashing) vs attribute writes
 *
 * This file is #included by main.c (single translation unit — the 8086
 * small-memory-model build keeps everything in one code segment) and is
 * also compiled standalone by tests/test_zxvga.c under a host compiler.
 * Everything Watcom-specific sits behind __WATCOMC__; the DOS code path
 * is byte-for-byte what it was before the extraction. */

#include <string.h>

#include "zxvga.h"

#ifdef __WATCOMC__
#  include <conio.h>
#  include <dos.h>
#  include <i86.h>
#endif

/* ===================================================================== */
/* §1  VGA surface                                                       */
/* ===================================================================== */

#ifdef __WATCOMC__
#  define ZXVGA_FAR __far
static unsigned char __far *vga = (unsigned char __far *)0xA0000000L;
#else
/* Host builds render into a plain array so the tests can inspect it.
 * There is no far/near distinction, and `_fmemset` / `_fmemcpy` (Watcom's
 * far-pointer string ops) become the standard functions. */
#  define ZXVGA_FAR
static unsigned char zxvga_host_fb[SCREEN_W * SCREEN_H];
static unsigned char *vga = zxvga_host_fb;
#  define _fmemset memset
#  define _fmemcpy memcpy
#endif

/* ZX Spectrum "acid" palette in 6-bit VGA DAC units. Non-bright slot
 * 0..7, bright slot 8..15. Order matches ULA ink/paper semantics:
 *   0 black, 1 blue, 2 red, 3 magenta, 4 green, 5 cyan, 6 yellow, 7 white.
 *
 * Non-bright at 56 (~228/255) gives the saturated CRT look — punchier
 * than the conservative 47 (~192/255) used by many emulators. Bright
 * stays at full 63. */
#define ZX_LO  56
#define ZX_HI  63
static const unsigned char zx_palette[16 * 3] = {
        0,     0,     0,        0,     0, ZX_LO,
    ZX_LO,    0,     0,    ZX_LO,    0, ZX_LO,
        0, ZX_LO,    0,        0, ZX_LO, ZX_LO,
    ZX_LO, ZX_LO,    0,    ZX_LO, ZX_LO, ZX_LO,

        0,     0,     0,        0,     0, ZX_HI,
    ZX_HI,    0,     0,    ZX_HI,    0, ZX_HI,
        0, ZX_HI,    0,        0, ZX_HI, ZX_HI,
    ZX_HI, ZX_HI,    0,    ZX_HI, ZX_HI, ZX_HI,
};

#ifdef __WATCOMC__
static void set_mode(unsigned char mode) {
    union REGS r;
    r.h.ah = 0x00;
    r.h.al = mode;
    int86(0x10, &r, &r);
}

static void set_palette(const unsigned char *rgb, int count) {
    int i;
    outp(0x3C8, 0);
    for (i = 0; i < count * 3; i++) outp(0x3C9, rgb[i]);
}
#else
/* Host stand-ins: no BIOS, no DAC ports. set_palette records what would
 * have been written so a test can check the uploaded DAC values. */
static unsigned char zxvga_host_dac[256 * 3];
static int zxvga_host_dac_len;

static void set_mode(unsigned char mode) { (void)mode; }

static void set_palette(const unsigned char *rgb, int count) {
    int i;
    for (i = 0; i < count * 3 && i < (int)sizeof(zxvga_host_dac); i++)
        zxvga_host_dac[i] = rgb[i];
    zxvga_host_dac_len = count * 3;
}
#endif

static void fill(int x, int y, int w, int h, unsigned char c) {
    int row;
    for (row = 0; row < h; row++) {
        _fmemset(vga + (long)(y + row) * SCREEN_W + x, c, (size_t)w);
    }
}

/* The playfield is 256x192 centred in the 320x200 mode — paint the
 * surrounding letterbox. */
static void clear_playfield_border(void) {
    fill(0, 0, SCREEN_W, BORDER_Y, COL_BORDER);
    fill(0, BORDER_Y + PLAYFIELD_H, SCREEN_W,
         SCREEN_H - BORDER_Y - PLAYFIELD_H, COL_BORDER);
    fill(0, BORDER_Y, BORDER_X, PLAYFIELD_H, COL_BORDER);
    fill(BORDER_X + PLAYFIELD_W, BORDER_Y,
         SCREEN_W - BORDER_X - PLAYFIELD_W, PLAYFIELD_H, COL_BORDER);
}

/* ===================================================================== */
/* §2  Attribute model                                                   */
/* ===================================================================== */

/* ZX attribute byte (paper=0 always here) -> our palette index. */
static unsigned char attr_to_palette(unsigned char attr) {
    /* bit 6 = bright, bits 0..2 = ink colour. */
    unsigned char bright = (attr >> 6) & 1;
    unsigned char ink    = attr & 7;
    return (unsigned char)(bright * 8 + ink);
}

/* ZX attribute byte -> our 16-entry palette indices for ink + paper.
 * attr = flash | bright | paper(3) | ink(3). bit 6 = bright. */
static unsigned char ink_pal(unsigned char attr) {
    return (unsigned char)((attr & 7) | ((attr & 0x40) >> 3));
}
static unsigned char paper_pal(unsigned char attr) {
    return (unsigned char)(((attr >> 3) & 7) | ((attr & 0x40) >> 3));
}

static unsigned char ink_table[256];
static unsigned char paper_table[256];

/* The clash expansion table: for every attribute (flash bit dropped —
 * see zxvga.h) and every 4-bit pixel nibble, the 4 VGA palette indices
 * that nibble expands to. Leftmost pixel first, i.e. in the low byte, so
 * the table can be blasted straight out with string stores. */
#ifdef BATTY_CPU386
/* 386 build: one packed dword (4 px) per (attr, nibble), so the blit
 * emits two `stosd` (8 px) instead of four `stosw`. Byte order matches
 * the 8086 word table — leftmost pixel in the low byte — so the VGA
 * bytes written are bit-for-bit identical. This REPLACES the word table
 * (same 8 KB) rather than adding to it, keeping DGROUP within 64 KB. */
static unsigned long vga_attr_nibble_dwords[128][16];
#else
static unsigned short vga_attr_nibble_words[128][16][2];
#endif

static void init_pal_tables(void) {
    int i;
    for (i = 0; i < 256; i++) {
        ink_table[i] = ink_pal((unsigned char)i);
        paper_table[i] = paper_pal((unsigned char)i);
    }
    for (i = 0; i < 128; i++) {
        int n;
        unsigned char ink = ink_table[i];
        unsigned char paper = paper_table[i];
        for (n = 0; n < 16; n++) {
            unsigned char p0 = (n & 8) ? ink : paper;
            unsigned char p1 = (n & 4) ? ink : paper;
            unsigned char p2 = (n & 2) ? ink : paper;
            unsigned char p3 = (n & 1) ? ink : paper;
#ifdef BATTY_CPU386
            vga_attr_nibble_dwords[i][n] =
                (unsigned long)p0 | ((unsigned long)p1 << 8) |
                ((unsigned long)p2 << 16) | ((unsigned long)p3 << 24);
#else
            vga_attr_nibble_words[i][n][0] = (unsigned short)(p0 | ((unsigned short)p1 << 8));
            vga_attr_nibble_words[i][n][1] = (unsigned short)(p2 | ((unsigned short)p3 << 8));
#endif
        }
    }
}

/* ===================================================================== */
/* §3  ZX framebuffer                                                    */
/* ===================================================================== */

/* `scr_buff` and `attr_buff` mirror the original's offscreen buffers
 * at $DA00 / $D700. Layout: scr_buff is row-major, 32 bytes/row * 192
 * rows; attr_buff is 32 cols * 24 rows of ZX attribute bytes. Pixel
 * (x, y) lives at scr_buff[y*32 + x/8] bit (7 - x%8); attribute at
 * attr_buff[(y/8)*32 + x/8]. */
static unsigned char scr_buff[6144];
static unsigned char attr_buff[768];

/* Bulk copy used by the compositors that rebuild bands of these buffers.
 * Hot enough on XT-class hardware to be worth the string op. */
static void fast_memcpy(void *dest, const void *src, unsigned int n_bytes);

#ifdef __WATCOMC__
#  ifdef BATTY_CPU386
/* 386 build: copy 4 bytes per element with `rep movsd`, then the 0..3
 * byte remainder with `rep movsb`. Halves the element count on the
 * band-rebuild / bg-cache copies that dominate full-recompose frames. */
static void fast_memcpy(void *dest, const void *src, unsigned int n_bytes) {
    _asm {
        push es
        push di
        push si
        mov ax, ds
        mov es, ax
        mov di, dest
        mov si, src
        mov cx, n_bytes
        cld
        mov dx, cx
        shr cx, 2
        rep movsd
        mov cx, dx
        and cx, 3
        rep movsb
        pop si
        pop di
        pop es
    }
}
#  else
static void fast_memcpy(void *dest, const void *src, unsigned int n_bytes) {
    _asm {
        push es
        push di
        push si
        mov ax, ds
        mov es, ax
        mov di, dest
        mov si, src
        mov cx, n_bytes
        cld
        rep movsb
        pop si
        pop di
        pop es
    }
}
#  endif
#else
static void fast_memcpy(void *dest, const void *src, unsigned int n_bytes) {
    memcpy(dest, src, n_bytes);
}
#endif

/* ===================================================================== */
/* §4  Clash blit to VGA                                                 */
/* ===================================================================== */

/* Where the module's share of the render profile is tallied
 * (write_profile_report in main.c prints these). */
static unsigned long prof_vga_rects = 0;
static unsigned long prof_vga_bytes = 0;

/* The expansion of one (pixel byte, cell attribute) pair into 8 VGA
 * pixels is where colour clash physically happens: the eight pixels can
 * only be this cell's ink or this cell's paper, whatever drew them. The
 * byte is split into two nibbles so a single table lookup covers 4
 * pixels — 2 stores per byte on 386, 4 on 8086.
 *
 * On DOS the inner loop is written out inline in each function below
 * (Watcom's `_asm` uses newlines as statement separators, so it cannot
 * be wrapped in a macro). The host build uses this equivalent, which
 * reads the SAME table — so a test comparing its output against an
 * independent ULA reference is testing the real expansion table, not a
 * reimplementation of it. */
#ifndef __WATCOMC__
#  ifdef BATTY_CPU386
#    define ZXVGA_EMIT_BYTE(dest, b, attr)                                        \
        do {                                                                      \
            unsigned long zhi = vga_attr_nibble_dwords[(attr) & 0x7F][(b) >> 4];   \
            unsigned long zlo = vga_attr_nibble_dwords[(attr) & 0x7F][(b) & 0x0F]; \
            int zi;                                                               \
            for (zi = 0; zi < 4; zi++) *(dest)++ = (unsigned char)(zhi >> (zi * 8)); \
            for (zi = 0; zi < 4; zi++) *(dest)++ = (unsigned char)(zlo >> (zi * 8)); \
        } while (0)
#  else
#    define ZXVGA_EMIT_BYTE(dest, b, attr)                                            \
        do {                                                                          \
            const unsigned short *zhi = vga_attr_nibble_words[(attr) & 0x7F][(b) >> 4];   \
            const unsigned short *zlo = vga_attr_nibble_words[(attr) & 0x7F][(b) & 0x0F]; \
            *(dest)++ = (unsigned char)zhi[0];                                        \
            *(dest)++ = (unsigned char)(zhi[0] >> 8);                                 \
            *(dest)++ = (unsigned char)zhi[1];                                        \
            *(dest)++ = (unsigned char)(zhi[1] >> 8);                                 \
            *(dest)++ = (unsigned char)zlo[0];                                        \
            *(dest)++ = (unsigned char)(zlo[0] >> 8);                                 \
            *(dest)++ = (unsigned char)zlo[1];                                        \
            *(dest)++ = (unsigned char)(zlo[1] >> 8);                                 \
        } while (0)
#  endif
#endif

/* Expand the whole 256x192 playfield. 49152 VGA writes — reserved for
 * screen changes; per-frame updates go through the dirty flush below. */
static void buff_to_vga(void) {
    int y, byte_col;
    for (y = 0; y < PLAYFIELD_H; y++) {
        unsigned char ZXVGA_FAR *dest = vga + (long)(BORDER_Y + y) * SCREEN_W + BORDER_X;
        const unsigned char *scr_row = &scr_buff[y * 32];
        const unsigned char *attr_row = &attr_buff[(y >> 3) * 32];
        for (byte_col = 0; byte_col < 32; byte_col++) {
            unsigned char b = scr_row[byte_col];
            unsigned char attr = attr_row[byte_col];
#ifndef __WATCOMC__
            ZXVGA_EMIT_BYTE(dest, b, attr);
#elif defined(BATTY_CPU386)
            const unsigned long *hi = &vga_attr_nibble_dwords[attr & 0x7F][b >> 4];
            const unsigned long *lo = &vga_attr_nibble_dwords[attr & 0x7F][b & 0x0F];

            _asm {
                les di, dest
                mov si, hi
                mov eax, [si]
                stosd
                mov si, lo
                mov eax, [si]
                stosd

                mov word ptr dest, di
            }
#else
            const unsigned short *hi = vga_attr_nibble_words[attr & 0x7F][b >> 4];
            const unsigned short *lo = vga_attr_nibble_words[attr & 0x7F][b & 0x0F];

            _asm {
                les di, dest
                mov si, hi
                mov ax, [si]
                stosw
                mov ax, [si+2]
                stosw
                mov si, lo
                mov ax, [si]
                stosw
                mov ax, [si+2]
                stosw

                mov word ptr dest, di
            }
#endif
        }
    }
}

/* Same expansion over a byte-column-aligned sub-rectangle: rows
 * [y0, y0+h) and byte columns [byte_lo, byte_hi]. Byte columns are also
 * character-cell columns, so a rect can never split a cell horizontally
 * and the attribute lookup stays valid. */
static void buff_to_vga_rect_bytes(int y0, int h, int byte_lo, int byte_hi) {
    int y, byte_col;
    int y_end = y0 + h;
    if (byte_lo < 0) byte_lo = 0;
    if (byte_hi > 31) byte_hi = 31;
    if (byte_lo > byte_hi) return;
    if (y_end > PLAYFIELD_H) y_end = PLAYFIELD_H;
    if (y0 < 0) y0 = 0;
    if (y0 >= y_end) return;
    prof_vga_rects++;
    prof_vga_bytes += (unsigned long)(y_end - y0) * (unsigned long)(byte_hi - byte_lo + 1) * 8UL;
    for (y = y0; y < y_end; y++) {
        unsigned char ZXVGA_FAR *dest = vga + (long)(BORDER_Y + y) * SCREEN_W
                                  + BORDER_X + byte_lo * 8;
        const unsigned char *scr_row = &scr_buff[y * 32];
        const unsigned char *attr_row = &attr_buff[(y >> 3) * 32];
        for (byte_col = byte_lo; byte_col <= byte_hi; byte_col++) {
            unsigned char b = scr_row[byte_col];
            unsigned char attr = attr_row[byte_col];
#ifndef __WATCOMC__
            ZXVGA_EMIT_BYTE(dest, b, attr);
#elif defined(BATTY_CPU386)
            const unsigned long *hi = &vga_attr_nibble_dwords[attr & 0x7F][b >> 4];
            const unsigned long *lo = &vga_attr_nibble_dwords[attr & 0x7F][b & 0x0F];
            _asm {
                les di, dest
                mov si, hi
                mov eax, [si]
                stosd
                mov si, lo
                mov eax, [si]
                stosd
                mov word ptr dest, di
            }
#else
            const unsigned short *hi = vga_attr_nibble_words[attr & 0x7F][b >> 4];
            const unsigned short *lo = vga_attr_nibble_words[attr & 0x7F][b & 0x0F];
            _asm {
                les di, dest
                mov si, hi
                mov ax, [si]
                stosw
                mov ax, [si+2]
                stosw
                mov si, lo
                mov ax, [si]
                stosw
                mov ax, [si+2]
                stosw
                mov word ptr dest, di
            }
#endif
        }
    }
}

/* ===================================================================== */
/* §5  Dirty rectangles                                                  */
/* ===================================================================== */

static unsigned char dirty_min_byte[DIRTY_SLOTS][PLAYFIELD_H];
static unsigned char dirty_max_byte[DIRTY_SLOTS][PLAYFIELD_H];
/* Pixel-row span of prev_dirty (set by carry_dirty_with_previous), so the
 * per-frame restore scans only the rows touched last frame instead of all
 * PLAYFIELD_H. Init full so the first restore (before any carry) is safe. */
static int prev_dirty_y_lo = 0;
static int prev_dirty_y_hi = PLAYFIELD_H - 1;
/* Pixel-row span of THIS frame's current dirty marks (reset each frame in
 * clear_dirty_ranges, grown by mark_dirty_bytes / mark_all_dirty), so
 * carry_dirty_with_previous can scan only [current ∪ prev] rows. */
static int cur_dirty_y_lo = PLAYFIELD_H;
static int cur_dirty_y_hi = -1;
static unsigned char prev_dirty_min_byte[DIRTY_SLOTS][PLAYFIELD_H];
static unsigned char prev_dirty_max_byte[DIRTY_SLOTS][PLAYFIELD_H];

static void clear_dirty_ranges(unsigned char mins[DIRTY_SLOTS][PLAYFIELD_H],
                               unsigned char maxs[DIRTY_SLOTS][PLAYFIELD_H]) {
    int s;
    for (s = 0; s < DIRTY_SLOTS; s++) {
        memset(mins[s], DIRTY_NONE, PLAYFIELD_H);
        memset(maxs[s], 0, PLAYFIELD_H);
    }
    if (mins == dirty_min_byte) {   /* clearing the CURRENT set: reset its span */
        cur_dirty_y_lo = PLAYFIELD_H;
        cur_dirty_y_hi = -1;
    }
}

/* Add [byte_lo, byte_hi] to row `y`: reuse a free slot, extend a slot it
 * touches or abuts, else merge into whichever slot grows least. */
static void mark_dirty_byte_row(unsigned char mins[DIRTY_SLOTS][PLAYFIELD_H],
                                unsigned char maxs[DIRTY_SLOTS][PLAYFIELD_H],
                                int y, int byte_lo, int byte_hi) {
    int s;
    int best_slot = 0;
    int best_extra = 255;
    for (s = 0; s < DIRTY_SLOTS; s++) {
        if (mins[s][y] == DIRTY_NONE) {
            mins[s][y] = (unsigned char)byte_lo;
            maxs[s][y] = (unsigned char)byte_hi;
            return;
        }
        if (byte_hi + 1 >= mins[s][y] && byte_lo <= maxs[s][y] + 1) {
            if (byte_lo < mins[s][y]) mins[s][y] = (unsigned char)byte_lo;
            if (byte_hi > maxs[s][y]) maxs[s][y] = (unsigned char)byte_hi;
            return;
        }
    }
    for (s = 0; s < DIRTY_SLOTS; s++) {
        int lo = (byte_lo < mins[s][y]) ? byte_lo : mins[s][y];
        int hi = (byte_hi > maxs[s][y]) ? byte_hi : maxs[s][y];
        int extra = (hi - lo + 1) - (maxs[s][y] - mins[s][y] + 1);
        if (extra < best_extra) {
            best_extra = extra;
            best_slot = s;
        }
    }
    if (byte_lo < mins[best_slot][y]) mins[best_slot][y] = (unsigned char)byte_lo;
    if (byte_hi > maxs[best_slot][y]) maxs[best_slot][y] = (unsigned char)byte_hi;
}

static void mark_dirty_bytes(int y_start, int height, int byte_lo, int byte_hi) {
    int y;
    int end = y_start + height;
    if (byte_lo < 0) byte_lo = 0;
    if (byte_hi > 31) byte_hi = 31;
    if (byte_lo > byte_hi) return;
    if (y_start < 0) y_start = 0;
    if (end > PLAYFIELD_H) end = PLAYFIELD_H;
    if (y_start >= end) return;
    if (y_start < cur_dirty_y_lo) cur_dirty_y_lo = y_start;
    if (end - 1 > cur_dirty_y_hi) cur_dirty_y_hi = end - 1;
    for (y = y_start; y < end; y++) {
        mark_dirty_byte_row(dirty_min_byte, dirty_max_byte, y, byte_lo, byte_hi);
    }
}

static void mark_dirty_rect_px(int x_start, int y_start, int width, int height) {
    int x_end = x_start + width;
    int byte_lo;
    int byte_hi;
    if (width <= 0 || height <= 0) return;
    if (x_start < 0) x_start = 0;
    if (x_end > PLAYFIELD_W) x_end = PLAYFIELD_W;
    if (x_start >= x_end) return;
    byte_lo = x_start >> 3;
    byte_hi = (x_end - 1) >> 3;
    mark_dirty_bytes(y_start, height, byte_lo, byte_hi);
}

/* Cell-aligned dirty mark for sprites that also rewrite char-cell ATTRS
 * (the colour-clash attr blit): an attr write recolours the WHOLE 8x8
 * cell, so the flush must cover every pixel row of every touched cell,
 * not just the sprite's own rows. Otherwise the boundary cells' rows
 * outside the sprite keep the clash colour on VGA after the attr is
 * restored — the enemy fly-over residue of known-bugs.md #2.
 * X needs no rounding (dirty ranges are byte == cell granular). */
static void mark_dirty_cell_rect_px(int x, int y, int w, int h) {
    int y0, y1;
    if (h <= 0) return;
    y0 = y & ~7;
    y1 = (y + h - 1) | 7;
    mark_dirty_rect_px(x, y0, w, y1 - y0 + 1);
}

static void mark_all_dirty(void) {
    int y;
    for (y = 0; y < PLAYFIELD_H; y++) {
        dirty_min_byte[0][y] = 0;
        dirty_max_byte[0][y] = 31;
        dirty_min_byte[1][y] = DIRTY_NONE;
        dirty_max_byte[1][y] = 0;
    }
    cur_dirty_y_lo = 0;
    cur_dirty_y_hi = PLAYFIELD_H - 1;
}

/* Walk one slot top-down, coalescing consecutive rows that share a byte
 * span into a single rect flush. */
static void flush_dirty_slot_to_vga(int slot) {
    int y;
    int start_y = -1;
    int byte_lo = 0;
    int byte_hi = 0;
    for (y = 0; y < PLAYFIELD_H; y++) {
        if (dirty_min_byte[slot][y] != DIRTY_NONE) {
            if (start_y == -1) {
                start_y = y;
                byte_lo = dirty_min_byte[slot][y];
                byte_hi = dirty_max_byte[slot][y];
            } else if (dirty_min_byte[slot][y] < byte_lo || dirty_max_byte[slot][y] > byte_hi) {
                buff_to_vga_rect_bytes(start_y, y - start_y, byte_lo, byte_hi);
                start_y = y;
                byte_lo = dirty_min_byte[slot][y];
                byte_hi = dirty_max_byte[slot][y];
            }
        } else {
            if (start_y != -1) {
                buff_to_vga_rect_bytes(start_y, y - start_y, byte_lo, byte_hi);
                start_y = -1;
            }
        }
    }
    if (start_y != -1) {
        buff_to_vga_rect_bytes(start_y, PLAYFIELD_H - start_y, byte_lo, byte_hi);
    }
}

static void flush_dirty_to_vga(void) {
    flush_dirty_slot_to_vga(0);
    flush_dirty_slot_to_vga(1);
}

/* ===================================================================== */
/* §6  Masked blits                                                      */
/* ===================================================================== */

/* Masked blit into the 1-bit scr_buff: per byte,
 *   scr_buff' = (~mask & scr_buff) | (mask & pix)
 * i.e. mask=1 bits take the pix bit (1 = ink, 0 = paper at the
 * buff_to_vga pass), mask=0 bits are PRESERVED (transparent).
 *
 * ATTRIBUTES ARE NOT TOUCHED — that is the point. A sprite drawn this
 * way inherits each cell's existing ink/paper, i.e. it clashes, exactly
 * as the original's print_obj_to_buff ($B82C) does.
 *
 * Encoding note (the full story is in notes/bird-render-parity.md):
 * this operates on the TAPE sprite encoding (assets/sprites.bin). The
 * original blits `(mask | scr) ^ pix` on data its boot-time
 * gfx_inverse pass transformed to pix^mask — the two compositions are
 * bit-identical on mask=1 bits. On mask=0 bits the original would XOR
 * stray pix bits into the background, but NO shipped sprite has pix
 * bits outside its mask (verified exhaustively over all 49 sprites;
 * the single exception is one bit in bird_4's garbage 15th row — see
 * the note), so the preserve-semantics here is equivalent. An earlier
 * version of this comment described `(mask|scr)^pix` outcomes
 * including an "XOR shadow" — that table never matched this code and
 * misled a whole triage; the dotted bat shadow comes from mask=1
 * dither bits, not XOR.
 * Handles non-byte-aligned x by emitting each source byte across two
 * destination bytes with a per-row shift. */
static void blit_masked_to_scr_buff_ptr(const unsigned char *src,
                                         int x_px, int y_px) {
    int w = src[0];
    int h = src[1];
    const unsigned char *p = src + 2;
    int shift     = x_px & 7;
    int start_col = x_px >> 3;
    int row, col_byte;

    if (shift == 0) {
        for (row = 0; row < h; row++) {
            int y = y_px + row;
            if (y < 0 || y >= PLAYFIELD_H) { p += (unsigned)w * 2; continue; }
            unsigned int row_base = (unsigned int)y * 32U;
            for (col_byte = 0; col_byte < w; col_byte++) {
                unsigned char mask = *p++;
                unsigned char pix  = *p++;
                int dst_l = start_col + col_byte;
                if (dst_l >= 0 && dst_l < 32) {
                    unsigned char *d = &scr_buff[row_base + dst_l];
                    *d = (unsigned char)(((unsigned char)(~mask) & *d) | (mask & pix));
                }
            }
        }
    } else {
        int rshift = 8 - shift;
        for (row = 0; row < h; row++) {
            int y = y_px + row;
            if (y < 0 || y >= PLAYFIELD_H) { p += (unsigned)w * 2; continue; }
            unsigned int row_base = (unsigned int)y * 32U;
            for (col_byte = 0; col_byte < w; col_byte++) {
                unsigned char mask = *p++;
                unsigned char pix  = *p++;
                int dst_l = start_col + col_byte;
                int dst_r = dst_l + 1;
                unsigned char m_l = (unsigned char)(mask >> shift);
                unsigned char p_l = (unsigned char)(pix  >> shift);
                unsigned char m_r = (unsigned char)(mask << rshift);
                unsigned char p_r = (unsigned char)(pix  << rshift);

                if (dst_l >= 0 && dst_l < 32) {
                    unsigned char *d = &scr_buff[row_base + dst_l];
                    *d = (unsigned char)(((unsigned char)(~m_l) & *d) | (m_l & p_l));
                }
                if (dst_r >= 0 && dst_r < 32) {
                    unsigned char *d = &scr_buff[row_base + dst_r];
                    *d = (unsigned char)(((unsigned char)(~m_r) & *d) | (m_r & p_r));
                }
            }
        }
    }
}

/* Blit an original-format masked sprite at playfield (x_px, y_px).
 *
 * Sprite layout (verbatim from the program at $7A8C+):
 *   byte 0     = width in bytes
 *   byte 1     = height in rows
 *   then h * w pairs of (mask_byte, pixel_byte) per byte-column.
 *
 * Original blit (sub_94BC's inner loop at byte_put_width_*):
 *   screen' = (mask | screen) ^ pixel
 *   per 8-pixel chunk. For VGA we walk each bit individually:
 *     mask=1, pixel=0  ->  ink     (the sprite's solid body)
 *     mask=1, pixel=1  ->  paper   (the sprite's internal texture)
 *     mask=0           ->  preserve background (sprite transparent)
 *   The XOR-on-mask=0 case (shadow effect) collapses to "preserve"
 *   here because we don't track screen as 1-bit; visually the bat
 *   shadow rows pick up mask=1 bits on their own.
 *
 * This one goes STRAIGHT TO VGA with caller-supplied ink/paper, i.e. it
 * bypasses the attribute plane entirely — used where the caller already
 * knows the cell colours. */
static void blit_masked_sprite_ptr(const unsigned char *src,
                                    int x_px, int y_px,
                                    unsigned char ink, unsigned char paper);

static void blit_masked_sprite_ptr(const unsigned char *src,
                                    int x_px, int y_px,
                                    unsigned char ink, unsigned char paper) {
    int w = src[0];
    int h = src[1];
    const unsigned char *p = src + 2;
    int row, col_byte, bit;
    for (row = 0; row < h; row++) {
        int y = y_px + row;
        for (col_byte = 0; col_byte < w; col_byte++) {
            unsigned char mask = *p++;
            unsigned char pix  = *p++;
            int base_x = x_px + col_byte * 8;
            for (bit = 0; bit < 8; bit++) {
                if (mask & (0x80 >> bit)) {
                    int x = base_x + bit;
                    if (x < 0 || x >= PLAYFIELD_W) continue;
                    if (y < 0 || y >= PLAYFIELD_H) continue;
                    vga[(long)(BORDER_Y + y) * SCREEN_W + BORDER_X + x] =
                        (pix & (0x80 >> bit)) ? paper : ink;
                }
            }
        }
    }
}

/* The ONLY attribute writer in this module (print_sprite_attrib's
 * clipped form). Recolours every 8x8 cell the rect touches — cells are
 * the finest granularity colour has, so a sprite that sets attrs
 * repaints its neighbours' pixels too. Callers that use this must mark
 * dirty with mark_dirty_cell_rect_px, not mark_dirty_rect_px. */
static void blit_sprite_attrs_to_buff_clipped(int x_px, int y_px, int w_px, int h_px,
                                              unsigned char attr,
                                              int clip_left_px, int clip_right_px) {
    int x0 = x_px;
    int x1 = x_px + w_px;
    int col_lo, col_hi, row_lo, row_hi, r, c;
    if (x0 < clip_left_px) x0 = clip_left_px;
    if (x1 > clip_right_px) x1 = clip_right_px;
    if (x1 <= x0) return;
    col_lo = x0 / 8;
    col_hi = (x1 - 1) / 8;
    row_lo = y_px / 8;
    row_hi = (y_px + h_px - 1) / 8;
    if (col_lo < 0) col_lo = 0;
    if (row_lo < 0) row_lo = 0;
    if (col_hi >= ATTR_COLS) col_hi = ATTR_COLS - 1;
    if (row_hi >= ATTR_ROWS) row_hi = ATTR_ROWS - 1;
    for (r = row_lo; r <= row_hi; r++) {
        for (c = col_lo; c <= col_hi; c++) {
            attr_buff[r * 32 + c] = attr;
        }
    }
}
