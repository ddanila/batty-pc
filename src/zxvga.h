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

#include "types.h"

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

/* --- The video mode --------------------------------------------------- */

/* Owns the video mode for its lifetime: mode 13h and the ZX palette on
 * construction, text mode on destruction. Declaring one in main() means no
 * return path can leave the display in a graphics mode. */
class ZxDisplay {
public:
    ZxDisplay();
    ~ZxDisplay();
private:
    ZxDisplay(const ZxDisplay &);
    ZxDisplay &operator=(const ZxDisplay &);
};

/* The mode 13h framebuffer. Game code writes through this directly only
 * for the few things that bypass the attribute plane (glyphs, whole-screen
 * asset blits); everything else goes through the two planes below. */
extern u8 *vga;

void fill(int x, int y, int w, int h, u8 c);
void clear_playfield_border();

/* Ink colour of an attribute whose paper is 0 — what the glyph and markup
 * renderers draw with. */
u8 attr_to_palette(u8 attr);

/* --- The two planes --------------------------------------------------- */

extern u8 scr_buff[SCR_BUFF_SIZE];
extern u8 attr_buff[ATTR_BUFF_SIZE];

/* Expand both planes to VGA. buff_to_vga is the whole playfield; the rect
 * form takes rows [y0, y0+h) and byte columns [byte_lo, byte_hi], which are
 * also cell columns, so a rect never splits a cell horizontally. */
void buff_to_vga();
void buff_to_vga_rect_bytes(int y0, int h, int byte_lo, int byte_hi);

/* Pixels only — never touches attr_buff, so what it draws clashes into
 * whatever colours its cells already carry. */
void blit_masked_to_scr_buff(const Sprite &sprite, int x_px, int y_px);

/* Straight to VGA in caller-supplied colours, bypassing the attribute
 * plane entirely. */
void blit_masked_sprite(const Sprite &sprite, int x_px, int y_px,
                        u8 ink, u8 paper);

/* The only attribute writer: recolours whole 8x8 cells, because that is
 * the only granularity colour has. Callers must mark dirty with
 * mark_dirty_cell_rect_px, not mark_dirty_rect_px. */
void blit_sprite_attrs_to_buff_clipped(int x_px, int y_px, int w_px, int h_px,
                                       u8 attr,
                                       int clip_left_px, int clip_right_px);

/* --- Dirty-rectangle tracking ---------------------------------------- */
/* Per pixel row we keep up to DIRTY_SLOTS disjoint byte-column spans, so
 * two far-apart sprites on the same row don't force a flush of
 * everything between them. DIRTY_NONE marks an unused slot. */
const u8  DIRTY_NONE  = 0xFF;
const int DIRTY_SLOTS = 2;

extern u8  dirty_min_byte[DIRTY_SLOTS][PLAYFIELD_H];
extern u8  dirty_max_byte[DIRTY_SLOTS][PLAYFIELD_H];
extern u8  prev_dirty_min_byte[DIRTY_SLOTS][PLAYFIELD_H];
extern u8  prev_dirty_max_byte[DIRTY_SLOTS][PLAYFIELD_H];
/* Pixel-row spans of the current and previous frames' marks, so the
 * per-frame restore and carry scan only the rows that matter. */
extern int cur_dirty_y_lo, cur_dirty_y_hi;
extern int prev_dirty_y_lo, prev_dirty_y_hi;

void clear_dirty_ranges(u8 mins[DIRTY_SLOTS][PLAYFIELD_H],
                        u8 maxs[DIRTY_SLOTS][PLAYFIELD_H]);
void mark_dirty_byte_row(u8 mins[DIRTY_SLOTS][PLAYFIELD_H],
                         u8 maxs[DIRTY_SLOTS][PLAYFIELD_H],
                         int y, int byte_lo, int byte_hi);
void mark_dirty_bytes(int y_start, int height, int byte_lo, int byte_hi);
void mark_dirty_rect_px(int x_start, int y_start, int width, int height);
/* Cell-aligned: an attribute write recolours the whole cell, so the flush
 * must cover every pixel row of every touched cell. */
void mark_dirty_cell_rect_px(int x, int y, int w, int h);
void mark_all_dirty();
void flush_dirty_to_vga();

/* Tallied by the blit; write_profile_report prints them. */
extern unsigned long prof_vga_rects, prof_vga_bytes;

#endif /* ZXVGA_H */
