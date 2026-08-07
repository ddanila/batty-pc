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
 * Compiled into the DOS build via main.cpp, and standalone by
 * tests/test_zxvga.cpp under a host compiler — the __WATCOMC__ guard picks
 * the VGA surface (real hardware vs a plain array). */

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

/* The DOS extender identity-maps the first megabyte, so VGA is an
 * ordinary pointer. */
#ifdef __WATCOMC__
unsigned char *vga = (unsigned char *)0x000A0000;
#else
/* Host builds render into a plain array so the tests can inspect it. */
static unsigned char zxvga_host_fb[SCREEN_W * SCREEN_H];
unsigned char *vga = zxvga_host_fb;
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
static void set_mode(u8 mode) {
    union REGS r;
    r.h.ah = 0x00;
    r.h.al = mode;
    int386(0x10, &r, &r);
}

static void set_palette(const u8 *rgb, int count) {
    outp(0x3C8, 0);
    for (int i = 0; i < count * 3; i++) outp(0x3C9, rgb[i]);
}
#else
/* Host stand-ins: no BIOS, no DAC ports. set_palette records what would
 * have been written so a test can check the uploaded DAC values. */
static unsigned char zxvga_host_dac[256 * 3];
static int zxvga_host_dac_len;

static void set_mode(u8 mode) { (void)mode; }

static void set_palette(const u8 *rgb, int count) {
    for (int i = 0; i < count * 3 && i < (int)sizeof(zxvga_host_dac); i++)
        zxvga_host_dac[i] = rgb[i];
    zxvga_host_dac_len = count * 3;
}
#endif

void fill(int x, int y, int w, int h, u8 c) {
    for (int row = 0; row < h; row++) {
        memset(vga + (long)(y + row) * SCREEN_W + x, c, (size_t)w);
    }
}

/* The playfield is 256x192 centred in the 320x200 mode — paint the
 * surrounding letterbox. */
void clear_playfield_border() {
    fill(0, 0, SCREEN_W, BORDER_Y, COL_BORDER);
    fill(0, BORDER_Y + PLAYFIELD_H, SCREEN_W,
         SCREEN_H - BORDER_Y - PLAYFIELD_H, COL_BORDER);
    fill(0, BORDER_Y, BORDER_X, PLAYFIELD_H, COL_BORDER);
    fill(BORDER_X + PLAYFIELD_W, BORDER_Y,
         SCREEN_W - BORDER_X - PLAYFIELD_W, PLAYFIELD_H, COL_BORDER);
}

static void init_pal_tables();

ZxDisplay::ZxDisplay() {
    set_mode(0x13);
    set_palette(zx_palette, 16);
    init_pal_tables();
}

ZxDisplay::~ZxDisplay() { set_mode(0x03); }

/* ===================================================================== */
/* §2  Attribute model                                                   */
/* ===================================================================== */

/* Ink colour of an attribute whose paper is known to be 0 — the glyph and
 * markup renderers draw on a black cell, so only the ink matters. */
u8 attr_to_palette(u8 attr) { return attr_ink(attr); }

static u8 ink_table[256];
static u8 paper_table[256];

/* The clash expansion table: for every attribute (flash bit dropped — see
 * zxvga.h) and every 4-bit pixel nibble, the 4 VGA palette indices that
 * nibble expands to, packed leftmost-pixel-in-the-low-byte so a whole
 * nibble is one 32-bit store. */
static u32 vga_attr_nibble_dwords[128][16];
ZX_STATIC_ASSERT(sizeof(u32) == 4, "the expansion table stores 4 pixels per entry");

static void init_pal_tables() {
    for (int a = 0; a < 256; a++) {
        ink_table[a]   = attr_ink(u8(a));
        paper_table[a] = attr_paper(u8(a));
    }
    /* Flash (bit 7) is not emulated, so only 128 attributes are distinct. */
    for (int a = 0; a < 128; a++) {
        const u8 ink   = ink_table[a];
        const u8 paper = paper_table[a];
        for (int nibble = 0; nibble < 16; nibble++) {
            u32 packed = 0;
            /* Bit 3 is the leftmost pixel and goes in the LOW byte, so the
             * whole nibble stores little-endian in screen order. */
            for (int bit = 0; bit < 4; bit++) {
                const u8 px = (nibble & (8 >> bit)) ? ink : paper;
                packed |= u32(px) << (bit * 8);
            }
            vga_attr_nibble_dwords[a][nibble] = packed;
        }
    }
}

/* ===================================================================== */
/* §3  ZX framebuffer                                                    */
/* ===================================================================== */

/* The two planes, mirroring the original's offscreen buffers at
 * $DA00 / $D700. Address them through the accessors below rather than
 * open-coding the arithmetic. */
u8 scr_buff[SCR_BUFF_SIZE];
u8 attr_buff[ATTR_BUFF_SIZE];

ZX_STATIC_ASSERT(BYTES_PER_ROW == ATTR_COLS,
                 "a byte of pixels and a character cell are both 8 px wide");
ZX_STATIC_ASSERT(sizeof(scr_buff) == 6144,  "scr_buff matches the ZX layout");
ZX_STATIC_ASSERT(sizeof(attr_buff) == 768,  "attr_buff matches the ZX layout");

/* Start of pixel row y, and of the attribute row covering it. Eight pixel
 * rows share one attribute row — that ratio is the whole of colour clash. */
inline u8 *scr_row(int y)  { return &scr_buff[y * BYTES_PER_ROW]; }
inline u8 *attr_row(int y) { return &attr_buff[(y / CELL_PX) * ATTR_COLS]; }

/* Playfield pixel (x, y) in the mode 13h framebuffer. */
inline u8 *vga_at(int x, int y) {
    return vga + (BORDER_Y + y) * SCREEN_W + BORDER_X + x;
}

/* ===================================================================== */
/* §4  Clash blit to VGA                                                 */
/* ===================================================================== */

/* Where the module's share of the render profile is tallied
 * (write_profile_report in main.c prints these). */
unsigned long prof_vga_rects = 0;
unsigned long prof_vga_bytes = 0;

/* One (pixel byte, cell attribute) pair -> 8 VGA pixels, advancing `dest`.
 * This is where colour clash physically happens: the eight pixels can only
 * be this cell's ink or this cell's paper, whatever drew them. */
inline void emit_byte(u8 *&dest, u8 pixels, u8 attr) {
    const u32 *table = vga_attr_nibble_dwords[attr & 0x7F];
    *(u32 *)(dest)     = table[pixels >> 4];
    *(u32 *)(dest + 4) = table[pixels & 0x0F];
    dest += 8;
}

/* Expand the whole 256x192 playfield. 49152 VGA writes — reserved for
 * screen changes; per-frame updates go through the dirty flush below. */
void buff_to_vga() {
    for (int y = 0; y < PLAYFIELD_H; y++) {
        u8 *dest = vga_at(0, y);
        const u8 *pixels = scr_row(y);
        const u8 *attrs  = attr_row(y);
        for (int col = 0; col < BYTES_PER_ROW; col++) {
            emit_byte(dest, pixels[col], attrs[col]);
        }
    }
}

/* Same expansion over a byte-column-aligned sub-rectangle: rows
 * [y0, y0+h) and byte columns [byte_lo, byte_hi]. Byte columns are also
 * character-cell columns, so a rect can never split a cell horizontally
 * and the attribute lookup stays valid. */
void buff_to_vga_rect_bytes(int y0, int h, int byte_lo, int byte_hi) {
    int y_end = y0 + h;
    if (byte_lo < 0) byte_lo = 0;
    if (byte_hi > BYTES_PER_ROW - 1) byte_hi = BYTES_PER_ROW - 1;
    if (byte_lo > byte_hi) return;
    if (y_end > PLAYFIELD_H) y_end = PLAYFIELD_H;
    if (y0 < 0) y0 = 0;
    if (y0 >= y_end) return;

    prof_vga_rects++;
    prof_vga_bytes += (unsigned long)(y_end - y0)
                    * (unsigned long)(byte_hi - byte_lo + 1) * 8UL;

    for (int y = y0; y < y_end; y++) {
        u8 *dest = vga_at(byte_lo * 8, y);
        const u8 *pixels = scr_row(y);
        const u8 *attrs  = attr_row(y);
        for (int col = byte_lo; col <= byte_hi; col++) {
            emit_byte(dest, pixels[col], attrs[col]);
        }
    }
}

/* ===================================================================== */
/* §5  Dirty rectangles                                                  */
/* ===================================================================== */

unsigned char dirty_min_byte[DIRTY_SLOTS][PLAYFIELD_H];
unsigned char dirty_max_byte[DIRTY_SLOTS][PLAYFIELD_H];
/* Pixel-row span of prev_dirty (set by carry_dirty_with_previous), so the
 * per-frame restore scans only the rows touched last frame instead of all
 * PLAYFIELD_H. Init full so the first restore (before any carry) is safe. */
int prev_dirty_y_lo = 0;
int prev_dirty_y_hi = PLAYFIELD_H - 1;
/* Pixel-row span of THIS frame's current dirty marks (reset each frame in
 * clear_dirty_ranges, grown by mark_dirty_bytes / mark_all_dirty), so
 * carry_dirty_with_previous can scan only [current ∪ prev] rows. */
int cur_dirty_y_lo = PLAYFIELD_H;
int cur_dirty_y_hi = -1;
unsigned char prev_dirty_min_byte[DIRTY_SLOTS][PLAYFIELD_H];
unsigned char prev_dirty_max_byte[DIRTY_SLOTS][PLAYFIELD_H];

void clear_dirty_ranges(unsigned char mins[DIRTY_SLOTS][PLAYFIELD_H],
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
void mark_dirty_byte_row(unsigned char mins[DIRTY_SLOTS][PLAYFIELD_H],
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

void mark_dirty_bytes(int y_start, int height, int byte_lo, int byte_hi) {
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

void mark_dirty_rect_px(int x_start, int y_start, int width, int height) {
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
void mark_dirty_cell_rect_px(int x, int y, int w, int h) {
    int y0, y1;
    if (h <= 0) return;
    y0 = y & ~7;
    y1 = (y + h - 1) | 7;
    mark_dirty_rect_px(x, y0, w, y1 - y0 + 1);
}

void mark_all_dirty(void) {
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

void flush_dirty_to_vga(void) {
    flush_dirty_slot_to_vga(0);
    flush_dirty_slot_to_vga(1);
}

/* ===================================================================== */
/* §6  Masked blits                                                      */
/* ===================================================================== */

/* Composite one byte of sprite over one byte of the pixel plane. */
inline void apply_mask(u8 &dest, u8 mask, u8 pixels) {
    dest = u8((~mask & dest) | (mask & pixels));
}

/* Masked blit into the 1-bit scr_buff: per destination byte,
 *   scr_buff' = (~mask & scr_buff) | (mask & pixels)
 * so mask=1 bits take the sprite's bit (which buff_to_vga later resolves
 * to the cell's ink or paper) and mask=0 bits are PRESERVED — transparent.
 *
 * ATTRIBUTES ARE NOT TOUCHED — that is the point. A sprite drawn this way
 * inherits each cell's existing ink/paper, i.e. it clashes, exactly as the
 * original's print_obj_to_buff ($B82C) does.
 *
 * Encoding note (the full story is in notes/bird-render-parity.md): this
 * operates on the TAPE sprite encoding (assets/sprites.bin). The original
 * blits `(mask | scr) ^ pix` on data its boot-time gfx_inverse pass
 * transformed to pix^mask; the two compositions are bit-identical on
 * mask=1 bits. On mask=0 bits the original would XOR stray pix bits into
 * the background, but no shipped sprite has pix bits outside its mask
 * (verified over all 49; the one exception is a bit in bird_4's garbage
 * 15th row), so preserve-semantics is equivalent.
 *
 * x need not be byte-aligned: each source byte then straddles two
 * destination bytes and is shifted into both. */
void blit_masked_to_scr_buff(const Sprite &sprite, int x_px, int y_px) {
    const int w = sprite.width_bytes();
    const int h = sprite.height();
    const int shift     = x_px & 7;
    const int start_col = x_px >> 3;
    const u8 *p = sprite.pixels();

    for (int row = 0; row < h; row++) {
        const int y = y_px + row;
        if (y < 0 || y >= PLAYFIELD_H) { p += w * 2; continue; }
        u8 *dest_row = scr_row(y);

        for (int col = 0; col < w; col++) {
            const u8 mask   = *p++;
            const u8 pixels = *p++;
            const int left  = start_col + col;

            if (shift == 0) {
                if (left >= 0 && left < BYTES_PER_ROW)
                    apply_mask(dest_row[left], mask, pixels);
            } else {
                const int right = left + 1;
                if (left >= 0 && left < BYTES_PER_ROW)
                    apply_mask(dest_row[left], u8(mask >> shift), u8(pixels >> shift));
                if (right >= 0 && right < BYTES_PER_ROW)
                    apply_mask(dest_row[right], u8(mask << (8 - shift)),
                                                u8(pixels << (8 - shift)));
            }
        }
    }
}

/* Blit a masked sprite STRAIGHT TO VGA in caller-supplied colours,
 * bypassing the attribute plane — used where the caller already knows the
 * cell colours and wants no clash.
 *
 *   mask=1, pixel=0  ->  ink    (the sprite's solid body)
 *   mask=1, pixel=1  ->  paper  (its internal texture)
 *   mask=0           ->  background preserved (transparent)
 *
 * Pixel-at-a-time rather than byte-at-a-time because the destination is
 * 8 bits per pixel, so there is nothing to pack. */
void blit_masked_sprite(const Sprite &sprite, int x_px, int y_px,
                               u8 ink, u8 paper) {
    const int w = sprite.width_bytes();
    const int h = sprite.height();
    const u8 *p = sprite.pixels();

    for (int row = 0; row < h; row++) {
        const int y = y_px + row;
        for (int col = 0; col < w; col++) {
            const u8 mask   = *p++;
            const u8 pixels = *p++;
            const int base_x = x_px + col * 8;
            for (int bit = 0; bit < 8; bit++) {
                const u8 select = u8(0x80 >> bit);
                if (!(mask & select)) continue;          /* transparent */
                const int x = base_x + bit;
                if (x < 0 || x >= PLAYFIELD_W)  continue;
                if (y < 0 || y >= PLAYFIELD_H)  continue;
                *vga_at(x, y) = (pixels & select) ? paper : ink;
            }
        }
    }
}

/* The ONLY attribute writer in this module (print_sprite_attrib's
 * clipped form). Recolours every 8x8 cell the rect touches — cells are
 * the finest granularity colour has, so a sprite that sets attrs
 * repaints its neighbours' pixels too. Callers that use this must mark
 * dirty with mark_dirty_cell_rect_px, not mark_dirty_rect_px. */
void blit_sprite_attrs_to_buff_clipped(int x_px, int y_px, int w_px, int h_px,
                                              u8 attr,
                                              int clip_left_px, int clip_right_px) {
    int x0 = x_px;
    int x1 = x_px + w_px;
    if (x0 < clip_left_px)  x0 = clip_left_px;
    if (x1 > clip_right_px) x1 = clip_right_px;
    if (x1 <= x0) return;

    int col_lo = x0 / CELL_PX;
    int col_hi = (x1 - 1) / CELL_PX;
    int row_lo = y_px / CELL_PX;
    int row_hi = (y_px + h_px - 1) / CELL_PX;
    if (col_lo < 0) col_lo = 0;
    if (row_lo < 0) row_lo = 0;
    if (col_hi >= ATTR_COLS) col_hi = ATTR_COLS - 1;
    if (row_hi >= ATTR_ROWS) row_hi = ATTR_ROWS - 1;

    for (int r = row_lo; r <= row_hi; r++)
        for (int c = col_lo; c <= col_hi; c++)
            attr_buff[r * ATTR_COLS + c] = attr;
}
