/* zxvga — ZX Spectrum display emulation on VGA mode 13h.
 *
 * This is the video engine: everything that turns the Spectrum's
 * *attribute* display model into VGA pixels, including its defining
 * artefact, COLOUR CLASH. Game content (bricks, bat, HUD, background
 * tiles, level assets) lives in main.cpp and only talks to this module
 * through the buffers and calls declared here.
 *
 *
 * THE DISPLAY MODEL
 * -----------------
 * The Spectrum's screen is two separate planes:
 *
 *   scr_buff   256x192 pixels, 1 BIT each. A bit is not a colour — it
 *              only selects ink (1) or paper (0). 32 bytes/row.
 *              Pixel (x, y) = scr_buff[y*32 + x/8] bit (7 - x%8).
 *
 *   attr_buff  32x24 ATTRIBUTE bytes, one per 8x8 character cell.
 *              Cell (cx, cy) = attr_buff[cy*32 + cx].
 *              Layout: flash(1) bright(1) paper(3) ink(3)
 *                bit  7      6        5..3      2..0
 *
 * Colour is therefore stored at 1/64th the resolution of the pixels.
 * Every one of the 64 pixels in a cell must be either that cell's ink
 * or that cell's paper — no exceptions. That constraint IS colour
 * clash: draw a sprite over a background of a different colour and the
 * sprite takes the cell's colours, or repaints the whole cell in its
 * own. Batty depends on this everywhere (a bird flying over the brick
 * band is drawn in the BRICKS' colours), so the port reproduces the
 * model exactly rather than rendering sprites in their "own" colour.
 *
 * The consequence for callers, and the invariant worth protecting:
 *
 *   - blit_masked_to_scr_buff() writes PIXELS ONLY. It never touches
 *     attr_buff, so anything drawn with it clashes into whatever
 *     colours its cells already carry. This mirrors the original's
 *     print_obj_to_buff ($B82C) and is what every moving object uses.
 *   - blit_sprite_attrs_to_buff_clipped() is the ONLY function here
 *     that writes attributes, and it recolours whole 8x8 cells because
 *     that is the only granularity the hardware has.
 *
 * (These two are covered by tests/test_zxvga.cpp and by the
 * test-sprite-attr-parity / test-enemy-attr-parity gates.)
 *
 *
 * THE OUTPUT PATH
 * ---------------
 * buff_to_vga() / buff_to_vga_rect_bytes() expand the two planes into
 * mode 13h's 8-bit-per-pixel framebuffer: for each (pixel byte, cell
 * attribute) pair, 8 palette indices, table-driven so a whole nibble is
 * one 32-bit store.
 *
 * FLASH IS NOT EMULATED. The table is indexed by `attr & 0x7F`, which
 * drops bit 7, so a flashing attribute renders as its steady state. The
 * game's own blink effects are done by rewriting attributes.
 *
 * Because a full-screen expansion is 49152 VGA byte writes, the engine
 * tracks dirty spans (mark_dirty_*) and flushes only those.
 *
 *
 * PORTABILITY
 * -----------
 * On DOS `vga` points at 0xA0000; under any other compiler it points at a
 * host buffer, which is what lets tests/test_zxvga.cpp exercise this exact
 * source natively. That one guard is the only target-specific thing here. */

#ifndef ZXVGA_H
#define ZXVGA_H

/* Fixed-width aliases. Watcom's 32-bit `long` is 4 bytes but a 64-bit
 * host's is 8, which silently doubles the width of a store through a
 * cast — use these in anything the host test build also compiles. */
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;      /* 4 bytes on both */

/* Open Watcom's C++ is C++98 plus static_assert; the host build is strict
 * C++98, which has neither. One macro so the assertions read the same in
 * both, and cost nothing in either. */
#if defined(__WATCOMC__)
#  define ZX_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#  define ZX_SA_CAT_(a, b) a##b
#  define ZX_SA_CAT(a, b)  ZX_SA_CAT_(a, b)
#  define ZX_STATIC_ASSERT(cond, msg) \
       typedef char ZX_SA_CAT(zx_static_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

/* --- VGA mode 13h geometry ------------------------------------------- */
const int SCREEN_W    = 320;
const int SCREEN_H    = 200;
const int PLAYFIELD_W = 256;
const int PLAYFIELD_H = 192;
const int BORDER_X    = (SCREEN_W - PLAYFIELD_W) / 2;   /* 32 */
const int BORDER_Y    = (SCREEN_H - PLAYFIELD_H) / 2;   /*  4 */

const u8 COL_BORDER = 0x00;   /* black; the loading screen has its own paper */

/* --- ZX plane geometry ------------------------------------------------ */
const int CELL_PX       = 8;                        /* an 8x8 character cell */
const int ATTR_COLS     = PLAYFIELD_W / CELL_PX;    /* 32 */
const int ATTR_ROWS     = PLAYFIELD_H / CELL_PX;    /* 24 */
const int BYTES_PER_ROW = PLAYFIELD_W / 8;          /* 32 — also ATTR_COLS */
const int SCR_BUFF_SIZE  = BYTES_PER_ROW * PLAYFIELD_H;   /* 6144 */
const int ATTR_BUFF_SIZE = ATTR_COLS * ATTR_ROWS;         /* 768 */

/* --- The attribute byte ----------------------------------------------- */
/* Palette index of a cell's ink / paper. Bit 6 is BRIGHT, which selects
 * the upper half of the 16-entry palette; bit 7 is FLASH and is ignored. */
inline u8 attr_ink(u8 attr)   { return u8((attr & 7) | ((attr & 0x40) >> 3)); }
inline u8 attr_paper(u8 attr) { return u8(((attr >> 3) & 7) | ((attr & 0x40) >> 3)); }

/* --- Sprites ---------------------------------------------------------- */
/* The original's sprite format, verbatim from the program at $7A8C+:
 *   byte 0            width in BYTES (so 8x that in pixels)
 *   byte 1            height in rows
 *   then h*w pairs    (mask_byte, pixel_byte), row-major
 *
 * Constructible straight from a `const u8 *`, so call sites that already
 * hold sprite data need no ceremony. */
class Sprite {
public:
    Sprite(const u8 *data) : d(data) {}
    int width_bytes() const { return d[0]; }
    int width_px()    const { return d[0] * 8; }
    int height()      const { return d[1]; }
    /* (mask, pixel) pairs, width_bytes() of them per row. */
    const u8 *pixels() const { return d + 2; }
private:
    const u8 *d;
};

/* --- Dirty-rectangle tracking ---------------------------------------- */
/* Per pixel row we keep up to DIRTY_SLOTS disjoint byte-column spans, so
 * two far-apart sprites on the same row don't force a flush of
 * everything between them. DIRTY_NONE marks an unused slot. */
const u8  DIRTY_NONE  = 0xFF;
const int DIRTY_SLOTS = 2;

#endif /* ZXVGA_H */
