/* zxvga — ZX Spectrum display emulation on VGA mode 13h.
 *
 * This is the video engine: everything that turns the Spectrum's
 * *attribute* display model into VGA pixels, including its defining
 * artefact, COLOUR CLASH. Game content (bricks, bat, HUD, background
 * tiles, level assets) lives in main.c and only talks to this module
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
 *   - blit_masked_to_scr_buff_ptr() writes PIXELS ONLY. It never
 *     touches attr_buff, so anything drawn with it clashes into
 *     whatever colours its cells already carry. This mirrors the
 *     original's print_obj_to_buff ($B82C) and is what every moving
 *     object uses.
 *   - blit_sprite_attrs_to_buff_clipped() is the ONLY function here
 *     that writes attributes, and it recolours whole 8x8 cells because
 *     that is the only granularity the hardware has.
 *
 * (These two are covered by tests/test_zxvga.c and by the
 * test-sprite-attr-parity / test-enemy-attr-parity gates.)
 *
 *
 * THE OUTPUT PATH
 * ---------------
 * buff_to_vga() / buff_to_vga_rect_bytes() expand the two planes into
 * mode 13h's 8-bit-per-pixel framebuffer: for each (pixel byte, cell
 * attribute) pair, 8 palette indices. That expansion is table-driven
 * (see vga_attr_nibble_words) so the inner loop is two or four string
 * stores rather than per-pixel branching.
 *
 * FLASH IS NOT EMULATED. The table is indexed by `attr & 0x7F`, which
 * drops bit 7, so a flashing attribute renders as its steady state.
 * The game's own blink effects are done by rewriting attributes.
 *
 * Because a full-screen expansion is 49152 VGA byte writes, the engine
 * tracks dirty spans (mark_dirty_*) and flushes only those (see the
 * dirty-rectangle section below).
 *
 *
 * PORTABILITY
 * -----------
 * On DOS `vga` points at 0xA0000; under any other compiler it points at a
 * host buffer, which is what lets tests/test_zxvga.c exercise this exact
 * source natively. That one guard is the only target-specific thing here. */

#ifndef ZXVGA_H
#define ZXVGA_H

/* --- VGA mode 13h geometry ------------------------------------------- */
#define SCREEN_W      320
#define SCREEN_H      200
#define PLAYFIELD_W   256
#define PLAYFIELD_H   192
#define BORDER_X      ((SCREEN_W - PLAYFIELD_W) / 2)   /* 32 */
#define BORDER_Y      ((SCREEN_H - PLAYFIELD_H) / 2)   /*  4 */

#define COL_BORDER    0x00   /* black border; the loading screen has its own paper */

/* --- ZX attribute grid ----------------------------------------------- */
#define ATTR_ROWS       24
#define ATTR_COLS       32

/* --- Dirty-rectangle tracking ---------------------------------------- */
/* Per pixel row we keep up to DIRTY_SLOTS disjoint byte-column spans, so
 * two far-apart sprites on the same row don't force a flush of
 * everything between them. DIRTY_NONE marks an unused slot. */
#define DIRTY_NONE  0xFF
#define DIRTY_SLOTS 2

#endif /* ZXVGA_H */
