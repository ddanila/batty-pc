/* batty — title -> static hi-score -> font-rendered hi-score.
 *
 * State machine: any non-ESC key advances; ESC at any point exits.
 *   1. LOADING.BIN    Original ZX loading screen (8bpp 256×192).
 *   2. HISCORE.BIN    Snap1 hi-score screen, byte-for-byte.
 *   3. (font render)  Same screen drawn programmatically through
 *                     FONT.BIN — proves we control the glyph path.
 *
 * Glyph format (extracted via scripts/extract_font.py):
 *   - 36 glyphs × 6 bytes (8 px wide × 6 px tall, top 2 rows of the
 *     8-row char cell are renderer-side blank padding).
 *   - Index = markup char-code: 0..9 = digits, 0x0A..0x23 = A..Z. */

#include <bios.h>
#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The video engine — ZX Spectrum attribute/colour-clash emulation on
 * VGA mode 13h. Everything below draws through its scr_buff / attr_buff
 * and the dirty marks declared there. */
#include "assets.h"
#include "bricks.h"
#include "hud.h"
#include "objects.h"
#include "bonus_codes.h"
#include "enemies.h"
#include "weapons.h"
#include "sound.h"
#include "physics.h"
#include "rng.h"
#include "zxvga.h"

#define SCREEN_CHUNK_ROWS 16
static unsigned char screen_chunk[SCREEN_CHUNK_ROWS * PLAYFIELD_W];

/* Stream a 256x192 8bpp asset straight into VGA. Read 16 scanlines per
 * DOS call to keep floppy/stdio overhead down on XT-class machines
 * while avoiding a full 48 KiB near-data buffer. */
static int blit_screen(const char *path) {
    FILE *f = fopen(path, "rb");
    int y;
    if (!f) return -1;
    for (y = 0; y < PLAYFIELD_H; y += SCREEN_CHUNK_ROWS) {
        int r;
        int rows = SCREEN_CHUNK_ROWS;
        if (y + rows > PLAYFIELD_H) rows = PLAYFIELD_H - y;
        if (fread(screen_chunk, PLAYFIELD_W, rows, f) != (size_t)rows) {
            fclose(f);
            return -2;
        }
        for (r = 0; r < rows; r++) {
            memcpy(vga + (long)(BORDER_Y + y + r) * SCREEN_W + BORDER_X,
                     &screen_chunk[r * PLAYFIELD_W], PLAYFIELD_W);
        }
    }
    fclose(f);
    return 0;
}

/* Repaint the border + blit one named asset. On asset-missing, paint
 * the playfield magenta so the failure is unmistakable in QEMU. */
static void show(const char *path) {
    clear_playfield_border();
    if (blit_screen(path) != 0) {
        fill(BORDER_X, BORDER_Y, PLAYFIELD_W, PLAYFIELD_H, 3 /* magenta */);
    }
}



/* Draw glyph `code` (0..35) at VGA pixel (x, y) using palette index
 * `color`. Bits are OR'd onto whatever's there; pixels with bit 0
 * are left as-is. Glyph is 6 rows × 8 cols. */




/* Record: marker | Y | attr | count | count payload bytes.
 * Payload bytes: 0x00-0x09 = digit, 0x0A-0x23 = letter, 0x24-0x2A =
 * specials (period/comma/space/dash/_/II/=), 0x40-0x4F = in-band
 * colour escape. */


/* 2-pixel-thick frame around the 256×192 playfield — matches the
 * original's drawn-in-pixels frame (not just the attribute gutter).
 * Palette indices: 10 = bright red (hi-score), 11 = bright magenta (menu). */
static void draw_frame(unsigned char colour) {
    int r;
    for (r = 0; r < 2; r++) {
        fill(BORDER_X,                       BORDER_Y + r,                    PLAYFIELD_W, 1, colour);
        fill(BORDER_X,                       BORDER_Y + PLAYFIELD_H - 1 - r,  PLAYFIELD_W, 1, colour);
        fill(BORDER_X + r,                   BORDER_Y,                        1, PLAYFIELD_H, colour);
        fill(BORDER_X + PLAYFIELD_W - 1 - r, BORDER_Y,                        1, PLAYFIELD_H, colour);
    }
}

/* Player input-device state — mirrors the original's bytes at
 * 0xB7EF (A toggles, player 1) and 0xB7F7 (B toggles, player 2).
 * Range 0..3 = KEYBOARD / KEMPSTON / CURSOR / INTERFACE II. */
static unsigned char p1_dev = 0;
static unsigned char p2_dev = 0;

/* Player indicators 32×16 each: P1 at blob 0x92C1, P2 at 0x9303.
 * Stored on disk as one 132 B blob: P1 header+body (66) then P2 (66). */
#define INDICATOR_W_BYTES  4
#define INDICATOR_H        16
#define INDICATOR_ROW_BYTES INDICATOR_W_BYTES
static unsigned char ind_p1[INDICATOR_W_BYTES * INDICATOR_H];
static unsigned char ind_p2[INDICATOR_W_BYTES * INDICATOR_H];


/* The original's row-advancer sub_b56eh moves the destination UP in
 * pixel rows (decrements H, which in ZX VRAM addressing reduces
 * pixel_row_in_char). So bitmap row 0 = bottom visually. We flip
 * here so (x, y) is still the top-left, and bitmap rows draw top to
 * bottom on screen. */
static void draw_indicator(const unsigned char *bitmap, int x, int y,
                           unsigned char colour) {
    int r, c, b;
    for (r = 0; r < INDICATOR_H; r++) {
        int dst_row = y + (INDICATOR_H - 1 - r);
        for (c = 0; c < INDICATOR_W_BYTES; c++) {
            unsigned char byte = bitmap[r * INDICATOR_W_BYTES + c];
            for (b = 0; b < 8; b++) {
                if (byte & (0x80 >> b)) {
                    vga[(long)dst_row * SCREEN_W + x + c * 8 + b] = colour;
                }
            }
        }
    }
}

/* Position state mirrors the original's words at (l92BD) for P1 and
 * (l92BF) for P2: high byte = Y (cycles 0x6C, 0x7C, 0x8C, 0x9C), low
 * byte = fixed X (0x28 P1 = col 5, 0xB8 P2 = col 23). The bitmap top
 * is stored_Y - 15 = 0x6C + state*0x10 - 15. */
#define IND_BASE_Y    0x6C
#define IND_Y_STRIDE  0x10
#define IND_P1_X      0x28
#define IND_P2_X      0xB8

static void draw_player_indicators(void) {
    int p1_y = BORDER_Y + IND_BASE_Y + p1_dev * IND_Y_STRIDE - 15;
    int p2_y = BORDER_Y + IND_BASE_Y + p2_dev * IND_Y_STRIDE - 15;
    draw_indicator(ind_p1, BORDER_X + IND_P1_X, p1_y, 15);
    draw_indicator(ind_p2, BORDER_X + IND_P2_X, p2_y, 15);
}

/* Bottom decorative sprite + arrow combined: 32×13 px each, bright
 * white. Sources from blob 0x938E (P1) and 0x93C4 (P2), 52 B each
 * stored bottom-to-top. The 13-row visual is: 5 rows of decorative
 * sprite, 2 blank rows, 6 rows of small downward arrow. */
#define BOTSPR_W_BYTES 4
#define BOTSPR_H       13
static unsigned char bot_p1[BOTSPR_W_BYTES * BOTSPR_H];
static unsigned char bot_p2[BOTSPR_W_BYTES * BOTSPR_H];


#define HUD_SPRITES_SIZE 0x0128
#define HUD_SPR_1UP      0x0000
#define HUD_SPR_2UP      0x0032
#define HUD_SPR_HI       0x0064
#define HUD_SCORE_DIGITS 0x0086
static unsigned char hud_sprites[HUD_SPRITES_SIZE];


static void draw_bottom_sprite(const unsigned char *bitmap, int x, int y_top,
                                unsigned char colour) {
    int r, c, b;
    for (r = 0; r < BOTSPR_H; r++) {
        /* bitmap stored bottom-to-top: source row 0 = visual row 4. */
        int dst_y = y_top + (BOTSPR_H - 1 - r);
        for (c = 0; c < BOTSPR_W_BYTES; c++) {
            unsigned char byte = bitmap[r * BOTSPR_W_BYTES + c];
            for (b = 0; b < 8; b++) {
                if (byte & (0x80 >> b)) {
                    vga[(long)dst_y * SCREEN_W + x + c * 8 + b] = colour;
                }
            }
        }
    }
}

static void draw_bottom_sprites(void) {
    int y = BORDER_Y + 59;        /* playfield y=59 = top of glyph in char_row 7 */
    draw_bottom_sprite(bot_p1, BORDER_X +  3 * 8, y, 15);
    draw_bottom_sprite(bot_p2, BORDER_X + 25 * 8, y, 15);
}

/* --- Levels + brick blitter -------------------------------------------
 *
 * 15 levels x 180 B (12 rows x 15 cols, 1 B/cell). Per cell: bit 7 or
 * bit 4 set = skip; else low bits index the sprite cache at
 * (cell_value * 16). Each cache chunk is 16 bytes = 2 bytes wide x 8
 * rows, blitted at (col*16, row*8) inside a 240x96 brick region
 * starting at pixel (8, 16) of the playfield (= VRAM 0x4081 in the
 * original — see notes/sprites.md).
 *
 * For Phase B2 we blit everything in bright-white (palette 15). Per-
 * level / per-row colour comes later. */
#define N_LEVELS   15
#define LVL_ROWS   12
#define LVL_COLS   15
#define LVL_CELLS  (LVL_ROWS * LVL_COLS)
ZX_STATIC_ASSERT(LVL_ROWS == FIELD_ROWS && LVL_COLS == FIELD_COLS,
                 "level tables and the physics brick field must agree");
#define LVL_SIZE   (N_LEVELS * LVL_CELLS)
/* Per-level attribute band: FULL 24 char-rows x 32 cols of ZX
 * attribute bytes captured from each level's GT .scr.
 * Lookup: attr = level_attrs[lvl*ATTR_BAND_SIZE + r*32 + col]
 * where r is the char-row index (0..23):
 *    0..1   top HUD
 *    2..13  brick zone (rendered by render_brick_band)
 *   14..21  side-frame interior
 *   22..23  bottom (bat / lives) */
/* ATTR_ROWS / ATTR_COLS (the 32x24 attribute grid) come from zxvga.h. */
#define ATTR_BAND_SIZE  (ATTR_ROWS * ATTR_COLS)
#define ATTR_TOTAL_SIZE (N_LEVELS * ATTR_BAND_SIZE)
#define BRICK_ATTR_ROW_BASE 3     /* brick char-rows start at attr-row 3
                                   * (= y=24..119 / 8 = char rows 3..14) */

static unsigned char levels[LVL_SIZE];

/* Mutable per-game copy of the current level's 180 cells (the
 * original keeps the equivalent at $6100, current_level_copy). Bricks
 * destroyed by the ball get bit 7 set here, making print_briks_c skip
 * them on the next repaint. */
static unsigned char live_level[LVL_CELLS];
static unsigned char level_attrs[ATTR_TOTAL_SIZE];

/* Ported brick compositor (was: shortcut #1 in notes/shortcuts.md).
 * Composites into the video engine's scr_buff / attr_buff (see zxvga.h).
 *
 * `spr_brik_1` and `briks_colors` are pulled verbatim from
 * original/disasm/gfx/briks.asm and original/disasm/batty.asm (label
 * `briks_colors`, 1-indexed by the brick code's low nibble). */

/* spr_bomb at $786A. Lives outside our SPRITES.BIN range ($7A8C+), so
 * embed it directly. 2-byte-wide x 16-row sprite, 66 bytes total. */
static const unsigned char spr_bomb_data[66] = {
    0x02, 0x10,
    0x7C, 0x00, 0x00, 0x00,
    0xFE, 0x5C, 0x00, 0x00,
    0x7C, 0x38, 0x00, 0x00,
    0x38, 0x00, 0x00, 0x00,
    0x7C, 0x38, 0x00, 0x00,
    0xFE, 0x5C, 0x00, 0x00,
    0xFE, 0x5C, 0x00, 0x00,
    0xFE, 0x5C, 0xF8, 0x00,
    0xFE, 0x5C, 0x70, 0x00,
    0x7C, 0x38, 0x00, 0x00,
    0x38, 0x00, 0x70, 0x00,
    0x00, 0x00, 0xF8, 0x00,
    0x00, 0x00, 0xF8, 0x00,
    0x00, 0x00, 0xF8, 0x00,
    0x00, 0x00, 0xF8, 0x00,
    0x00, 0x00, 0x70, 0x00
};

/* spr_magnet_circle_off at $78AC and spr_magnet_circle_on at $7938 —
 * both live outside our SPRITES.BIN range ($7A8C..$8F50). The
 * original's print_magnets paints `off` at (x,y), then on a 50%
 * coin-flip overlays `on` at (x, y+5). For the deterministic state4
 * test we paint both: the GT was captured at a moment when the coin
 * flipped to "on" (verified by the L2 magnet showing up in the
 * 271-px diff at y=44..70 — see notes/magnets-missing.md). */
static const unsigned char spr_magnet_off[140] = {
    0x03, 0x17,
    0x00,0x00,0xFF,0x00,0x80,0x00,
    0x07,0x00,0xFF,0x7F,0xF0,0x00,
    0x1F,0x03,0xFF,0xC1,0xFC,0xE0,
    0x3F,0x0E,0xFF,0x1C,0xFE,0x38,
    0x3F,0x18,0xFF,0x10,0xFE,0x0C,
    0x7F,0x11,0xFF,0x18,0xFF,0x44,
    0x7F,0x32,0xFF,0x90,0xFF,0xA6,
    0x7F,0x24,0xFF,0x41,0xFF,0x52,
    0xFF,0x22,0xFF,0x9C,0xFF,0x02,
    0xFF,0x61,0xFF,0x26,0xFF,0x03,
    0xFF,0x40,0xFF,0x4F,0xFF,0x01,
    0xFF,0x4C,0xFF,0x5F,0xFF,0x19,
    0xFF,0x4C,0xFF,0x5F,0xFF,0x19,
    0xFF,0x40,0xFF,0x7F,0xFF,0x01,
    0xFF,0x60,0xFF,0x3E,0xFF,0x03,
    0xFF,0x21,0xFF,0x1C,0xFF,0x42,
    0x7F,0x22,0xFF,0x80,0xFF,0x82,
    0x7F,0x34,0xFF,0x5D,0xFF,0x46,
    0x7F,0x12,0xFF,0x90,0xFF,0x84,
    0x3F,0x19,0xFF,0x18,0xFF,0x4C,
    0x3F,0x0E,0xFF,0x10,0xFF,0x38,
    0x1F,0x03,0xFF,0xC1,0xFE,0xE0,
    0x07,0x00,0xFF,0x7F,0xF5,0x00
};

static const unsigned char spr_magnet_on[242] = {
    0x04, 0x1E,
    0x00,0x00,0xFF,0x00,0x80,0x00,0x00,0x00,
    0x07,0x00,0xFF,0x7F,0xF0,0x00,0x00,0x00,
    0x1F,0x03,0xFF,0xF7,0xFC,0xE0,0x00,0x00,
    0x3F,0x0F,0xFF,0xF7,0xFE,0xF8,0x00,0x00,
    0x3F,0x1F,0xFF,0xF7,0xFE,0xFC,0x00,0x00,
    0x7F,0x1F,0xFF,0xF7,0xFF,0xFC,0x00,0x00,
    0x7F,0x27,0xFF,0xC1,0xFF,0xF2,0x00,0x00,
    0x7F,0x39,0xFF,0xE3,0xFF,0xCE,0x00,0x00,
    0xFF,0x3E,0xFF,0xB6,0xFF,0xBE,0x80,0x00,
    0xFF,0x7F,0xFF,0x3E,0xFF,0x7F,0x80,0x00,
    0xFF,0x7E,0xFF,0x3E,0xFF,0x3F,0xC0,0x00,
    0xFF,0x7F,0xFF,0xFF,0xFF,0xFF,0xA0,0x00,
    0xFF,0x7F,0xFF,0xFF,0xFF,0xFF,0xD0,0x00,
    0xFF,0x7E,0xFF,0x3E,0xFF,0x3F,0xA8,0x00,
    0xFF,0x7F,0xFF,0x3E,0xFF,0x7F,0xD0,0x00,
    0xFF,0x3E,0xFF,0xB6,0xFF,0xBE,0xA8,0x00,
    0x7F,0x39,0xFF,0xE3,0xFF,0xCE,0x54,0x00,
    0x7F,0x27,0xFF,0xC1,0xFF,0xF2,0xA8,0x00,
    0x7F,0x1F,0xFF,0xF7,0xFF,0xFC,0x54,0x00,
    0x3F,0x1F,0xFF,0xF7,0xFF,0xFC,0xA8,0x00,
    0x3F,0x0F,0xFF,0xF7,0xFF,0xF8,0x54,0x00,
    0x1F,0x03,0xFF,0xF7,0xFE,0xE0,0xA8,0x00,
    0x07,0x00,0xFF,0x7F,0xF5,0x00,0x50,0x00,
    0x00,0x00,0xFF,0x00,0xAA,0x00,0xA8,0x00,
    0x00,0x00,0x55,0x00,0x55,0x00,0x50,0x00,
    0x00,0x00,0xAA,0x00,0xAA,0x00,0xA0,0x00,
    0x00,0x00,0x55,0x00,0x55,0x00,0x50,0x00,
    0x00,0x00,0x2A,0x00,0xAA,0x00,0xA0,0x00,
    0x00,0x00,0x05,0x00,0x55,0x00,0x40,0x00,
    0x00,0x00,0x00,0x00,0xAA,0x00,0x00,0x00
};

/* Per-level magnet positions, ported from magnet_level_NN entries at
 * original/disasm/routines/magnets.asm. Each row: { count, x0, y0,
 * x1, y1, x2, y2, x3, y3 } — max 4 magnets per level. The original's
 * `magnets` lookup-table maps L1 -> L3 data (= empty) so L1 has no
 * magnets despite being level index 0. */
#define MAGNETS_MAX_PER_LEVEL 4
static const unsigned char magnets_per_level[N_LEVELS][1 + 2*MAGNETS_MAX_PER_LEVEL] = {
    { 0 },                                                     /* L1 (= L3 dup) */
    { 1, 0x74,0x2C, 0,0, 0,0, 0,0 },                           /* L2  */
    { 0 },                                                     /* L3  */
    { 1, 0x74,0x7C, 0,0, 0,0, 0,0 },                           /* L4  */
    { 0 },                                                     /* L5  */
    { 3, 0x74,0x10, 0x48,0x73, 0xA0,0x73, 0,0 },               /* L6  */
    { 2, 0x30,0x5C, 0xD8,0x5C, 0,0, 0,0 },                     /* L7  */
    { 3, 0x74,0x18, 0x4C,0x74, 0x9C,0x74, 0,0 },               /* L8  */
    { 4, 0x40,0x3C, 0xA8,0x3C, 0x54,0x6C, 0x94,0x6C },         /* L9  */
    { 1, 0x74,0x44, 0,0, 0,0, 0,0 },                           /* L10 */
    { 2, 0x5C,0x84, 0x8C,0x84, 0,0, 0,0 },                     /* L11 */
    { 3, 0x74,0x08, 0x20,0x44, 0xC8,0x44, 0,0 },               /* L12 */
    { 4, 0x10,0x20, 0xD8,0x20, 0x18,0x6C, 0xD0,0x6C },         /* L13 */
    { 4, 0x8C,0x24, 0xC4,0x24, 0x8C,0x64, 0xC4,0x64 },         /* L14 */
    { 2, 0x4C,0x82, 0x9C,0x82, 0,0, 0,0 }                      /* L15 */
};

/* Runtime magnet state — port of magnets_quantity + magnet_properties
 * ($8DB7/$8DB8). magnet_px/py keep the PAINT origin (x0,y0 from the
 * level table); the original's slot stores (x0+5, y0+5) — the circle's
 * physics box origin — with body size 15x14 px (slot +$0C/+$0D).
 * magnet_on_state mirrors slot +$01: 1 = sprite $06 (ON, bit0 clear),
 * 0 = sprite $07 (OFF). Set once per level entry by magnet_level_init
 * (the print_magnets coin), flipped at random by magnet_random_toggle
 * (print_one_magnet $8E72). */
static unsigned char magnet_count = 0;
static unsigned char magnet_px[MAGNETS_MAX_PER_LEVEL];
static unsigned char magnet_py[MAGNETS_MAX_PER_LEVEL];
static unsigned char magnet_on_state[MAGNETS_MAX_PER_LEVEL];
/* Index of a magnet whose ON/OFF flip still needs its incremental
 * circle redraw applied at the next frame compose (0xFF = none). */
static unsigned char magnet_toggle_pending = 0xFF;
#define MAGNET_BODY_W  15        /* slot +$0C */
#define MAGNET_BODY_H  14        /* slot +$0D */

/* Per-ball magnet capture blocks — port of the 4-byte LA270 (ball 1) /
 * LA274 (ball 2) / LA278 (other) state:
 *   cool:  +0  re-capture cooldown (2 frames after a release)
 *   delta: +1  per-frame dir rotation while captured (+1/-1, 0 = free)
 *   exit:  +2  quantized release direction, recomputed every frame
 *   idx:   +3  capturing magnet's slot index
 * Index by ball object (OBJ_BALL_1/2/3 -> 0/1/2). */
static unsigned char ball_mag_cool[3];
static unsigned char ball_mag_delta[3];
static unsigned char ball_mag_exit[3];
static unsigned char ball_mag_idx[3];

/* PIT-tick duration of the last completed level-intro shimmer pass,
 * exported via PROBE.TXT (brik_anim_ticks=) so the regression test can
 * assert the original's pacing (8 frames x 2 interrupt edges = ~16). */
static unsigned long brik_anim_probe_ticks = 0;


/* Per-cycle bg attribute. The original game's game_screen_draw_to_buffer
 * ($BE6B) fills the playfield via spr_level_textures - 4 sprites whose
 * trailing color byte gives the cycle's attr. Values copied from
 * spr_level_texture_1..4 at $C015 / $8EE8 / $8F10 / $8F38. */
static const unsigned char bg_attr_per_cycle[4] = {
    0x46,    /* texture_1: bright yellow ink, black paper (L1/L5/L9/L13) */
    0x44,    /* texture_2: bright green                  (L2/L6/L10/L14) */
    0x45,    /* texture_3: bright cyan                   (L3/L7/L11/L15) */
    0x47     /* texture_4: bright white                  (L4/L8/L12)     */
};

/* 16x16-pixel hex pattern tile, one per colour cycle. Cycle index:
 *   0  yellow  (L1, L5, L9, L13)
 *   1  green   (L2, L6, L10, L14)
 *   2  cyan    (L3, L7, L11, L15)
 *   3  white   (L4, L8, L12)
 * Within a cycle the bitmap is byte-identical. */
#define BG_TILE_W_PX 16
#define BG_TILE_H_PX 16
#define BG_TILE_SIZE (BG_TILE_H_PX * 2)
#define BG_TILE_CYCLES 4
static unsigned char bg_tile[BG_TILE_CYCLES * BG_TILE_SIZE];

/* Bat + on-bat ball composite: 5 bytes wide x 19 rows = 95 B (40 x 19
 * px). Captures the bat, the ball resting on it, AND the bat's shadow
 * pixels just below. The bat is 32 px wide at the top, 40 at the
 * base; we include the wider edge col so the shadow's right-most
 * pixels are covered (the wider area painted with bg attr just
 * reproduces the hex tile underneath, no harm). */
/* Bat geometry now matches the original spr_bat_normal ($7E38):
 * 4 bytes wide (32 px) * 13 rows. Top 8 rows are the body, last 3 are
 * the dithered shadow drop. */
#define BAT_W_BYTES 4
#define BAT_H_PX    13
#define BAT_Y_PX    0xAD            /* = 173, matches object_bat_1.y_coord */
/* Match the original's clamps: $08..$F8-w_body. For our 32-px normal
 * bat that's 8..216; for the 48-px big bat 8..200. The wider shipped
 * frame strip can clip the bat at the edges, but the user prefers
 * full reach over partial-visibility cosmetics. Proper fix awaits
 * the frame ornament painter port. */
#define BAT_X_MIN     8
#define BAT_X_MAX   216
#define BAT_X_INIT  0x74             /* = 116, matches object_bat_1.x_coord */
/* The bat's authoritative state lives in objects[OBJ_BAT_1] - macros
 * defined after the object table below. */

/* Ball geometry from the original spr_ball_normal ($7B16): 2-byte-wide
 * sprite (16 px) with body in left byte (8 px) and shadow in right
 * byte at lower rows. For collisions we use the BODY size 8x7; the
 * shadow is purely visual. */
#define BALL_W_PX   8          /* body width for collision */
#define BALL_H_PX   7          /* body height */
#define BALL_SPEED  2
#define BALL_X_OFFSET_ON_BAT 16  /* = $84 - $74, matches the original's
                                  * object_ball_1.x_coord - object_bat_1.x_coord */
#define BALL_Y_TOP     8
#define BALL_X_MIN     8
#define BALL_X_MAX   240        /* 256 - 8 - body 8 */
/* Ball state - x/y now live in objects[OBJ_BALL_1].x_coord/y_coord
 * (the descriptor is the source of truth, mirroring the original's
 * IX-relative access). The primary ball uses the descriptor's
 * direction/speed plus the +03/+05 fractional bytes for movement;
 * the legacy integer deltas remain for the two extra balls. */
static int ball_dx     = +BALL_SPEED;
static int ball_dy     = -BALL_SPEED;
static unsigned char ball_stuck   = 1;
static unsigned char last_primary_launch_valid = 0;
static unsigned char last_primary_launch_x = 0;
static unsigned char last_primary_launch_y = 0;
static unsigned char last_primary_launch_dir = 0;
static unsigned char last_primary_launch_speed = 0;
static unsigned int  launch_probe_frames = 0;
static unsigned int  launch_probe_countdown = 0;
static unsigned char launch_probe_active = 0;
static unsigned int  frame_probe_frames = 0;
static unsigned int  frame_probe_countdown = 0;
static unsigned char frame_probe_active = 0;
/* 0 while the level-init PROBE.TXT write (the seeded pre-gameplay state)
 * is emitted; set to 1 once the gameplay main loop is entered. The harness
 * reads `probe_phase` to tell a real checkpoint write apart from the init
 * write it would see if a BATTY_REPLAY_WAIT_KEY wake key was missed (a slow
 * boot host-timing race), so it can re-boot instead of trusting stale seed
 * state. See scripts/test_visual.py read_gameplay_probe(). */
static unsigned char probe_from_gameplay = 0;
/* Deterministic mid-game capture checkpoints. BATTY_VISUAL_PROBE_FRAMES
 * is a comma-separated list of ascending absolute frame indices; the
 * port runs to each in turn, halts (so the harness can grab a drift-free
 * capture), resumes on a key, and quits after the last. A single value
 * reproduces the original single-shot behaviour. This is the port side
 * of the frame-step parity sweep (see notes/replay-harness.md). */
#define VISUAL_PROBE_MAX 16
static unsigned int  visual_probe_list[VISUAL_PROBE_MAX];
static unsigned char visual_probe_count = 0;
static unsigned char visual_probe_index = 0;
static unsigned int  visual_probe_countdown = 0;
static unsigned char visual_probe_active = 0;
/* Offset from BAT_X where the ball sits while stuck. Defaults to
 * BALL_X_OFFSET_ON_BAT for the standard "ball respawns at bat
 * centre" cases (level entry, life lost). The CATCH bonus rewrites
 * this when the ball hits the bat so the ball sticks at the
 * actual catch position and rides the bat from there until SPACE. */
static int stuck_offset_x = BALL_X_OFFSET_ON_BAT;

/* Laser bullet state — single bullet at a time, fired from the bat
 * top centre while the LASER bonus is active (BAT+\$14 = \$01).
 * Moves up each frame, deactivates on first brick / alien hit or
 * after leaving the playfield top. */
#define BULLET_W_PX     8    /* sprite width incl. transparent column */
#define BULLET_H_PX     8
/* Bat laser-fire animation: ticks down from 8 to 0; while non-zero
 * render_bat picks spr_bat_gun_1..4 based on the count so the bat's
 * cannon visibly flashes when SPACE fires a bullet. */
static unsigned char bat_fire_anim_ticks = 0;
/* Laser fire cooldown — port of the `bullet` counter at $A160. Original
 * sets it to ~\$16 (=22) on each fire, then `SUB \$02` per frame; SPACE
 * is ignored until the counter underflows. Net effect: ~11 frames
 * between shots regardless of how fast SPACE is mashed. */
static unsigned char bullet_cooldown = 0;
/* Total laser shots fired this level (diagnostic, exposed via the probe so
 * the fire-cadence gate can assert the 12-frame period). Reset at level
 * entry. */
static unsigned int  dbg_shots_fired = 0;
/* Test hook: when set (BATTY_AUTO_FIRE), the laser fires every frame the
 * cooldown permits, simulating held SPACE so the cadence is gate-checkable
 * without driving keyboard input through the capture harness. */
static unsigned char auto_fire = 0;

/* Second ball for the MULTI_BALL ($02 = triple_ball) bonus. We only
 * spawn TWO extras for a total of three balls — port of the LA67B_8
 * triple-ball block at $A67B which spawns ball2 + ball3 with
 * directions derived from ball1's. Position for each lives in
 * objects[OBJ_BALL_2/3].x_coord/y_coord; velocity + active flag are
 * dedicated state. Falling past the bat just deactivates the extra
 * ball without decrementing lives (= the lives counter only tracks
 * the primary ball; multi-ball is a bonus pile of destruction). */
static unsigned char ball2_active = 0;
static int           ball2_dx     = +BALL_SPEED;
static int           ball2_dy     = -BALL_SPEED;
static unsigned char ball3_active = 0;
static int           ball3_dx     = -BALL_SPEED;
static int           ball3_dy     = -BALL_SPEED;

/* Original random_generate walks $8000..$9FFF and folds those bytes
 * into the two random_number bytes. Ship that 8 KB source window from
 * the original program so bonuses / bombs / enemies consume the same
 * byte-stream shape as the Spectrum game. */
static unsigned char random_rom[RANDOM_ROM_SIZE];



/* Brick destruction dirty marker. The original remove path restores
 * background/window data; it does not paint a bright-white replacement
 * block. Keep the most recent cell live for a couple of ticks only so
 * dirty redraw includes print_one_brik_buf's wider 18x10 footprint. */
/* MUST stay 2 for frame-step parity. The perf idea (cut to 1, let
 * carry_dirty_with_previous re-flush the cell on the next ball-only/object
 * frame) holds for the single-buffer ERASE (test-brick-flash passes), but
 * the carry re-flush does NOT reproduce a full-dynamic band rebuild of the
 * destruction transient: cutting to 1 regressed the L3 capture-timeline
 * residual from 4px to 88-134px at the destroy frame (f5). Two full-dynamic
 * frames are required to match the original's destroy render. See
 * notes/metal-shimmer.md (BRICK_FLASH_TICKS regression). */
#define BRICK_FLASH_TICKS 2
static unsigned char brick_flash_ticks = 0;
static int           brick_flash_x     = 0;
static int           brick_flash_y     = 0;
static void render_brick_flash_to_buff(void);    /* forward decl — defined alongside brick_collision */

/* Original briks_data: up to five simultaneous hard-brick shimmer
 * animations after a non-destroying hit. The slot counter mirrors the
 * original's `(c+1) & $0F`: ticks 1..15 (anim_brik's 8 frames, ~2 ticks
 * each), then the wrap to 0 frees the slot — one pass, not a loop. */
#define BRICK_HIT_ANIM_SLOTS 5
static unsigned char brick_hit_anim_ticks[BRICK_HIT_ANIM_SLOTS];
static unsigned char brick_hit_anim_col[BRICK_HIT_ANIM_SLOTS];
static unsigned char brick_hit_anim_row[BRICK_HIT_ANIM_SLOTS];
static void brick_hit_anim_spawn(int col, int row);
static void step_brick_hit_anim(void);
static int any_brick_hit_anim(void);
static void reset_brick_hit_anim(void);
static void render_brick_hit_anim_to_buff(void);

/* Rocket bonus animation. The original uses spr_bonus_rocket_1/2 as a
 * 3-byte-wide, 27/28-row sprite; get_rocket also patches frame 1's
 * height to $1B. Use that visible footprint for the brick sweep rather
 * than the narrower placeholder box the earlier port used. */
#define ROCKET_W_PX     24
#define ROCKET_H_PX     27
#define ROCKET_BONUS_H_PX 0x0C
static unsigned char rocket_active = 0;
static int           rocket_x      = 0;
static int           rocket_y      = 0;
static unsigned int  rocket_acc    = 0;
static unsigned char rocket_frac   = 0;
static unsigned char rocket_counter = 0;
static unsigned char rocket_clear_completed = 0;
/* Stuck-on-bat dwell counter. While ball_stuck, the ball rides the
 * bat; SPACE detaches immediately; after STUCK_TIMEOUT ticks the ball
 * auto-launches. ~5 sec at 50 Hz. */
/* Mirror of ball.bonus_applied = $C0 at all_var_init's level entry: the
 * original counts down from 192 ticks (= 3.84 s at 50 Hz) before auto-
 * releasing a stuck ball. We were waiting ~25 % longer at 5 s. */
#define STUCK_TIMEOUT 192
static unsigned int stuck_ticks = 0;

/* Original handling_bonus drives Y through the shared LA55A_0
 * fixed-point accelerator. Falling bonuses and bombs use DE=$0008,
 * B=$02 (accelerate to 2 px/frame); the +400 marker uses DE=$0028,
 * B=$80 and dies when it reaches y=$C0. */
typedef struct {
    unsigned int  acc;
    unsigned char frac;
} motion_acc_t;

static int motion_accel_step(motion_acc_t *m, unsigned int de,
                             unsigned char cap_hi) {
    unsigned int acc = (unsigned int)(m->acc + de);
    unsigned int sum;
    if ((unsigned char)(acc >> 8) == cap_hi) acc = (unsigned int)cap_hi << 8;
    m->acc = acc;
    sum = acc + m->frac;
    m->frac = (unsigned char)sum;
    return (int)((signed char)(sum >> 8));
}

/* Bomb state - port of bomb_appear at $A977. UFOs (and birds) drop a
 * single bomb that falls toward the bat. Mutually exclusive with a
 * regular bonus in the original since they share object_bonus; here
 * we keep separate side state. Bat collision = lose life like a
 * ball drop. */
static unsigned char bomb_active = 0;
static int           bomb_x = 0;
static int           bomb_y = 0;
static motion_acc_t  bomb_motion = {0, 0};
#define BOMB_W_PX       8
#define BOMB_H_PX       12
/* ball_visible is encoded in objects[OBJ_BALL_1].sprite_set bit 7:
 *   sprite_set == 0x02  -> active, drawn
 *   sprite_set == 0x82  -> inactive, not drawn (BIT 7 set per
 *                          original obj_processing convention) */

/* Game-loop state. score is a plain integer; the original uses a
 * 3-byte BCD-ish representation across current_score_1up + the in-game
 * digits at score_1up_in_game. lives starts at 3 per original
 * game_restart at $B9A0 (LD A,$03 / LD (lives_1up),A). */

/* Per-brick scoring table from points_table at $AFE4 (BCD source,
 * decimal here). Indexed by brik_value+$01 in the original - that
 * byte tracks the ball's row position so top-row bricks score more
 * than bottom-row. We approximate with the brick's row index, which
 * matches "higher up = more points" exactly. Metal bricks (cell low
 * nibble >= 6) get DOUBLE per the JP C, add_points_to_score test at
 * $AFD6. */
static const unsigned int points_table[12] = {
    120, 110, 100, 90, 80, 70, 60, 50, 40, 30, 20, 10
};
#define LIVES_INIT          3
static unsigned long score      = 0;
static int           lives      = LIVES_INIT;
static unsigned long high_score = 0;

/* Score milestones at which an extra life is awarded — port of
 * live_add_steps at $0395. Original stores BCD high-byte thresholds
 * ($03, $06, $10, ...), which interpreted as 6-digit scores are
 * 30000, 60000, 100000, etc. */
static const unsigned long live_add_thresholds[] = {
    30000UL, 60000UL, 100000UL, 150000UL,
    200000UL, 250000UL, 500000UL, 750000UL
};
#define LIVE_ADD_COUNT (sizeof(live_add_thresholds)/sizeof(live_add_thresholds[0]))
static unsigned char live_adds_awarded = 0;

/* Mirror of flag_extra_life — set when the player catches a LIFE bonus
 * in the current round, prevents another LIFE drop until the round
 * ends. Reset at each new round entry in run_level. */
static unsigned char life_dropped_this_round = 0;
static unsigned char high_score_beaten_this_game = 0;
/* Three-letter initials saved with the high score. Stored as glyph
 * codes (A = 0x0A .. Z = 0x23). Default "AAA" when no save exists. */
static unsigned char high_score_name[3] = { 0x0A, 0x0A, 0x0A };

/* Persist the best score across runs by reading / writing 4 little-
 * endian bytes to A:\HISCORE.DAT (the floppy image). DOS floppy is
 * read/write under QEMU's if=floppy mode, so writes survive a reboot
 * as long as the disk image isn't rebuilt by `make floppy`. */

/* Power-up state: a single falling bonus on screen at a time. The
 * original (notes/plan-gameplay.md Phase H) drives this via
 * bonus_table_first/second + generate_new_bonus + set_bonus at $9866;
 * we hold a simpler 1-slot version until the object descriptor port
 * (M3 proper) lands. The slow-ball effect uses a tick countdown that
 * runs at the PIT frame rate (50 Hz). */
/* Catch hit-box. The visible bonus sprite is 24 px wide x 13 rows
 * tall (with a drop-shadow band), but we run the bat-collision
 * against the central body region only — 16 wide x 8 tall — so a
 * marginal "shadow touched the bat" doesn't register as a catch. */
#define BONUS_W_PX        16
#define BONUS_H_PX        8
/* Original game bonus codes from set_bonus / bonus_table_* at $9E4A:
 *   $01 gun        (deferred - needs bullet system)
 *   $02 triple_ball (deferred - needs multi-ball)
 *   $03 ?
 *   $04 slow_ball  -> our SLOW
 *   $05 extra_life -> our LIFE
 *   $06 rocket     (deferred - needs handling_rocket)
 *   $07 smash      -> our BIG_BALL
 *   $08 ?          -> mapped to BIG_BAT (placeholder)
 *   $09 kill_aliens (deferred)
 * map_orig_to_our_bonus translates a table draw to one of our 4
 * supported effects; unsupported codes get rolled away in pick. */

/* bonus_table_first / bonus_table_second - byte-exact copies of the
 * 32-byte tables at $9E5A / $9E6A. The lower 4 bits of random_number
 * index the active table; the original walks rows 0..15 and rows
 * 16..31 are an extension (the original code only ANDs $0F so it
 * picks from 0..15 - the duplicate 16..31 region appears to be
 * vestigial in the disasm but we keep it for byte-fidelity). */
static const unsigned char bonus_table_first[32] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x00, 0x04, 0x00, 0x03, 0x01, 0x02,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x02,
    0x01, 0x03, 0x00, 0x04, 0x00, 0x03, 0x01, 0x02
};
static const unsigned char bonus_table_second[32] = {
    0x00, 0x01, 0x02, 0x03, 0x02, 0x00, 0x06, 0x07,
    0x08, 0x09, 0x00, 0x03, 0x00, 0x02, 0x01, 0x03,
    0x00, 0x01, 0x02, 0x03, 0x02, 0x00, 0x06, 0x02,
    0x01, 0x03, 0x00, 0x03, 0x00, 0x02, 0x01, 0x03
};


/* Forward decls - defined below in the enemy section. */
static unsigned int next_random(void);
static unsigned int rng_sample(void);   /* defined later; used by enemy steering */
/* Global per-frame counter (PIT IRQ; = the original's counter_misc).
 * Forward declaration: the definition (with init) is near the IRQ setup;
 * the enemy steering reads it for its global 4-frame turn cadence. */
static volatile unsigned long pit_frame_counter;
static unsigned char random_hi(unsigned int r) { return (unsigned char)(r >> 8); }
static unsigned char random_lo(unsigned int r) { return (unsigned char)r; }
extern unsigned char round_number;
/* Ball speed model (handling_ball LA27E_22 / get_bonus LA67B_7):
 * the original ball ACCELERATES over a level. A per-ball counter
 * (object+$13) increments when (counter_misc & 7) == 0 — every 8
 * frames — and when it reaches $94 (148) it resets and the speed byte
 * (+$07) increments, capped at $06. So speed climbs $02 -> $06, one
 * step per ~1184 frames (~24 s). SLOW ($04) just sets all ball speeds
 * back to $02 (it does NOT touch the ramp counter), so it naturally
 * wears off as the speed ramps back up. We model this with the shared
 * ball_speed_ramp counter + the per-frame ball_speed_ramp_tick(), and
 * SLOW resets objects[].speed to BALL_SPEED. (Earlier port used a
 * fixed speed + a permanent slow_ticks frame-skip — the ball never
 * sped up and SLOW lasted the whole life.) */
/* BIG_BAT is permanent in the original — handling_bat_no_transform
 * keys off bat.bonus_applied == \$00, with no auto-expire. The bat
 * stays wide until another bonus is caught or the ball is lost.
 * Setting to UINT_MAX-ish so the timer-based expiration never fires;
 * big_bat_active() AND with bat.bonus_applied does the real gating. */
#define BIG_BAT_DURATION  0xFFFFu
/* Original smash_counter expires at $F8 and advances only on every
 * other counter_misc value while the big-ball sprite is being printed. */
#define BIG_BALL_DURATION 0xF8u
#define BAT_BIG_EXTRA_PX    8     /* width added on each side in big mode */
static int           bonus_x = 0;
static int           bonus_y = 0;
static unsigned char bonus_type   = 0;
static unsigned char bonus_active = 0;
static motion_acc_t  bonus_motion = {0, 0};
/* Shared ball speed-up ramp counter (= the original's per-ball
 * object+$13). Bumps every active ball's speed at $94; see the model
 * comment above and ball_speed_ramp_tick(). */
static unsigned int  ball_speed_ramp = 0;
static unsigned int  big_bat_ticks   = 0;
static unsigned int  big_ball_ticks  = 0;
static int big_ball_active(void);    /* forward — defined below */
static int big_bat_active(void);     /* forward — defined below */
/* Bat resize animation - bat_extra_px ramps 0..8 toward bat_extra_tgt
 * (port of bat_resize at $9D2C). Width grows / shrinks 1 px / 50 Hz
 * tick = ~6 px / 100 ms which roughly matches the original's 2-px-
 * every-other-frame from $9D45's `RR E` gating. */
static int           bat_extra_px    = 0;
static int           bat_extra_tgt   = 0;
static int           bat_draw_extra_px = 0;
static int           bat_draw_y = BAT_Y_PX;
static unsigned char bat_draw_bonus_applied = 0xFF;
static unsigned char bat_draw_fire_ticks = 0;

/* "+400" floating-marker state spawned on bonus catch (port of
 * sprite_set $0B transition at $A6BA + handling_400pts at $A58D).
 * The original puts the marker in the same slot the bonus occupied;
 * we use side state for now since the bonus state is also side. */
static int           pts_400_x = 0;
static int           pts_400_y = 0;
static unsigned char pts_400_active = 0;
static motion_acc_t  pts_400_motion = {0, 0};
/* X drift per frame, mirror of the SMC at \$A590 in handling_400pts.
 * Original chooses from {-2, -1, +1, +2} based on random_number bits at
 * spawn (LA67B_3 area at \$3030-\$3038). Picked once per +400 spawn. */
static int           pts_400_dx = 0;
/* Sprite for the floating points marker. Defaults to the universal
 * "+400" reward; SCORE_5K bonus catches override to the larger
 * "+5000" sprite so the unusual reward has its own visible cue. */
static unsigned int  pts_marker_spr = 0;  /* set on catch */


/* Position of the leftmost dynamic life indicator; we paint
 * spr_lives_indicator (16x6 px sprite at $7AFC) here. Port of
 * LBE8B_7's `LD A,$08` initial X and object_lives_indicator's
 * `DEFB ... $B9 ...` y_coord — bats grow rightwards at +16 px. */
#define LIVES_X_PX    8
#define LIVES_Y_PX    0xB9       /* = 185 */

/* The original game's sprite block, extracted verbatim from the
 * program at $7A8C..$8D46 (offset 0x128c..0x2546 within
 * 03_DATA_headless.dat.bin). Format per sprite:
 *   byte 0  -- width in bytes
 *   byte 1  -- height in rows
 *   then h rows of w (mask, pixel) pairs - blit semantics described
 *   in blit_masked_sprite below.
 * The constants below are offsets WITHIN sprites_blob. */
#define SPRITES_BLOB_SIZE 0x12BA
static unsigned char sprites_blob[SPRITES_BLOB_SIZE];
#define SPR_BIG_BALL     (0x7A8C - 0x7A8C)   /* = 0x000 */
#define SPR_LIVES        (0x7AFC - 0x7A8C)   /* = 0x070 */
#define SPR_BALL_NORMAL  (0x7B16 - 0x7A8C)   /* = 0x08a */
#define SPR_BAT_NORMAL   (0x7E38 - 0x7A8C)   /* = 0x3ac */
#define SPR_BAT_BIG      (0x7F42 - 0x7A8C)   /* = 0x4b6 */
#define SPR_BAT_GUN      (0x8188 - 0x7A8C)   /* laser-cannon bat (resting) */
#define SPR_BAT_GUN_1    (0x7FE0 - 0x7A8C)   /* firing anim frame 1 */
#define SPR_BAT_GUN_2    (0x804A - 0x7A8C)   /* firing anim frame 2 */
#define SPR_BAT_GUN_3    (0x80B4 - 0x7A8C)   /* firing anim frame 3 */
#define SPR_BAT_GUN_4    (0x811E - 0x7A8C)   /* firing anim frame 4 */

/* Laser bullet — 1 byte (8 px) wide x 8 rows tall, two animation
 * frames in the original sprite blob. */
#define SPR_BULLET_1     (0x7DD2 - 0x7A8C)
#define SPR_BULLET_2     (0x7DE4 - 0x7A8C)
/* Bullet-blast 4-frame animation, played at the hit point when the
 * laser bullet stops against a brick or alien. */
#define SPR_BULLET_BLAST_1 (0x7DF6 - 0x7A8C)
#define SPR_BULLET_BLAST_2 (0x7E04 - 0x7A8C)
#define SPR_BULLET_BLAST_3 (0x7E14 - 0x7A8C)
#define SPR_BULLET_BLAST_4 (0x7E26 - 0x7A8C)
#define SPR_UFO_1        (0x83B0 - 0x7A8C)   /* = 0x924 */
#define SPR_UFO_2        (0x8406 - 0x7A8C)   /* = 0x97a */
#define SPR_UFO_3        (0x8462 - 0x7A8C)   /* = 0x9d6 */
#define SPR_UFO_4        (0x84C4 - 0x7A8C)   /* = 0xa38 */
#define SPR_UFO_5        (0x852C - 0x7A8C)   /* = 0xaa0 */
#define SPR_UFO_6        (0x859A - 0x7A8C)   /* = 0xb0e */
#define SPR_BIRD_1       (0x860E - 0x7A8C)   /* = 0xb82 */
#define SPR_BIRD_2       (0x866A - 0x7A8C)   /* = 0xbde */
#define SPR_BIRD_3       (0x86C6 - 0x7A8C)   /* = 0xc3a */
#define SPR_BIRD_4       (0x8722 - 0x7A8C)   /* = 0xc96 */
#define SPR_BIRD_5       (0x8778 - 0x7A8C)   /* = 0xcec (frame-7 pose; h=17 at
                                              * runtime — the gfx_inverse overrun
                                              * patch in the Makefile asset rule) */

/* Frame tables for alien animation. The original advances object+$01
 * (sprite_num) +1 every 4 frames over an 8-step cycle (IX+$13=$70 high
 * nibble 7) and indexes anim_bird ($7896) — an 8-entry PING-PONG using all
 * 5 bird sprites: 1,2,3,4,3,2,1,5 (GT-confirmed sprite_num walk
 * f8=0..f32=6, see notes/enemy-movement.md). We encode that ping-pong in
 * the table and index it with (misc_12 >> 2) & 7. (The original's LAA02
 * also mirrors the sprite by flight direction — not yet ported; the
 * wing-flap sequence here is the visible animation regardless.) */
static const unsigned int spr_bird_frames[8] = {
    SPR_BIRD_1, SPR_BIRD_2, SPR_BIRD_3, SPR_BIRD_4,
    SPR_BIRD_3, SPR_BIRD_2, SPR_BIRD_1, SPR_BIRD_5
};
/* anim_ufo ($789E) — TEN entries, a full ping-pong over the 6 UFO
 * sprites: 1,2,3,4,5,6,5,4,3,2. The UFO's +$13 = $90 walks sprite_num
 * 0..9 through all ten (the table was previously truncated to 8 and
 * indexed `& 7`, skipping the 3,2 tail — notes/bird-render-parity.md).
 * Indexed by the LAAD2-stepped sprite_num. */
static const unsigned int spr_ufo_frames[10]  = {
    SPR_UFO_1, SPR_UFO_2, SPR_UFO_3, SPR_UFO_4, SPR_UFO_5,
    SPR_UFO_6, SPR_UFO_5, SPR_UFO_4, SPR_UFO_3, SPR_UFO_2
};

#define SPR_400_POINTS   (0x7ABE - 0x7A8C)   /* = 0x032 */
#define SPR_BLAST_1      (0x87E6 - 0x7A8C)   /* = 0xd5a */
#define SPR_BLAST_2      (0x881C - 0x7A8C)   /* = 0xd90 */
#define SPR_BLAST_3      (0x8852 - 0x7A8C)   /* = 0xdc6 */
#define SPR_BLAST_4      (0x888C - 0x7A8C)   /* = 0xe00 */
#define SPR_BLAST_5      (0x88CE - 0x7A8C)   /* = 0xe42 */

/* L4's spark enemy — 5 decaying frames at $8342..$83A6. Same
 * mask+pix layout as the alien/blast sprites. */
#define SPR_SPARK_1      (0x8342 - 0x7A8C)
#define SPR_SPARK_2      (0x8370 - 0x7A8C)
#define SPR_SPARK_3      (0x8386 - 0x7A8C)
#define SPR_SPARK_4      (0x8398 - 0x7A8C)
#define SPR_SPARK_5      (0x83A6 - 0x7A8C)

/* Bonus sprites. Offsets from $7A8C. */
#define SPR_BONUS_ROCKET_1    (0x891C - 0x7A8C)   /* code $06 — rocket frame 1 */
#define SPR_BONUS_ROCKET_2    (0x89C0 - 0x7A8C)   /* rocket frame 2 (animation) */
#define SPR_BONUS_SMASH       (0x8A6A - 0x7A8C)   /* code $07 — big-ball */
#define SPR_BONUS_KILL_ALIENS (0x8AC6 - 0x7A8C)   /* code $09 */
#define SPR_BONUS_HAND        (0x8B22 - 0x7A8C)   /* code $03 — catch */
#define SPR_BONUS_SIZE        (0x8B6C - 0x7A8C)   /* code $00 — big-bat */
#define SPR_BONUS_SLOW        (0x8BB0 - 0x7A8C)   /* code $04 */
#define SPR_BONUS_GUN         (0x8C0C - 0x7A8C)   /* code $01 — laser */
#define SPR_BONUS_EXTRA_LIFE  (0x8C44 - 0x7A8C)   /* code $05 */
#define SPR_BONUS_5000_POINTS (0x8C94 - 0x7A8C)   /* code $08 */
#define SPR_BONUS_TRIPLE_BALL (0x8CEA - 0x7A8C)   /* code $02 */
/* anim_alien_blast ($788A) is a 10-entry PING-PONG over the 5 blast
 * sprites: 1,2,3,4,5,4,3,2,1,1 (GT-confirmed: poking the enemy to the
 * blast state and stepping shows sprite_num 0..9 advancing +1 every 2
 * frames — misc_12 toggles $50<->$10 — then handling_blast deactivates at
 * frame 9). The port had only the 5-frame expand (1..5) at 3 ticks/frame. */
static const unsigned int spr_blast_frames[10] = {
    SPR_BLAST_1, SPR_BLAST_2, SPR_BLAST_3, SPR_BLAST_4, SPR_BLAST_5,
    SPR_BLAST_4, SPR_BLAST_3, SPR_BLAST_2, SPR_BLAST_1, SPR_BLAST_1
};
static const unsigned int spr_spark_frames[5] = {
    SPR_SPARK_1, SPR_SPARK_2, SPR_SPARK_3, SPR_SPARK_4, SPR_SPARK_5
};
#define BLAST_FRAMES 10
/* (blast cadence now comes from LAAD2 / step_obj_anim: the kill sites
 * seed +$12=$50 -> one anim_alien_blast step every 2 frames) */
/* Spark: each frame decays in duration (rough port of the original's
 * "halve the timer each frame" mechanic). 8/4/2/1/1 ticks total. */
#define SPARK_FRAMES 5

/* Perimeter frame (top + left + right, no bottom). Side strip is 1 col
 * wide — only the actual ornament byte_x=0 / byte_x=31. The original
 * draws no shadow column at byte_x=1, 2; those positions are part of
 * the playfield interior, filled by paint_bg + render_brick_band +
 * print_briks_c. (Wider side strip would overlap bricks at lvl_col=0 /
 * lvl_col=14 with the captured-from-L1 frame data.)
 *   top  pixels: 32 cols x 16 rows  = 512 B
 *   top  attrs : 32 cols x  2 rows  =  64 B
 *   left pixels:  1 col  x 176 rows = 176 B
 *   left attrs :  1 col  x  22 rows =  22 B
 *   right pixels: 1 col  x 176 rows = 176 B
 *   right attrs : 1 col  x  22 rows =  22 B
 * Total: 972 B. */
#define FRAME_SIDE_W     1
#define FRAME_TOP_H_PX   24        /* HUD is 24 px tall: y=0..7 ornament,
                                    * y=8..15 labels, y=16..23 scores */
#define FRAME_SIDE_H_PX  168       /* y=24..191 below the HUD */
#define FRAME_TOP_PX     (32 * FRAME_TOP_H_PX)
#define FRAME_TOP_ATTRS  (32 * (FRAME_TOP_H_PX / 8))
#define FRAME_SIDE_PX    (FRAME_SIDE_W * FRAME_SIDE_H_PX)
#define FRAME_SIDE_ATTRS (FRAME_SIDE_W * (FRAME_SIDE_H_PX / 8))
#define FRAME_SIZE  (FRAME_TOP_PX + FRAME_TOP_ATTRS + \
                     2 * (FRAME_SIDE_PX + FRAME_SIDE_ATTRS))
#define FRAME_CYCLES 4
static unsigned char frame_l1[FRAME_CYCLES * FRAME_SIZE];







static unsigned long prof_bg_pit = 0;
static unsigned long prof_frame_pit = 0;
static unsigned long prof_hud_pit = 0;
static unsigned long prof_bricks_pit = 0;
static unsigned long prof_vga_pit = 0;
static unsigned long prof_frames_count = 0;
/* prof_vga_rects / prof_vga_bytes are tallied by the video engine (zxvga.c). */
static unsigned long prof_static_rebuilds = 0;
/* Brick-band cache rebuilds (build_static_brick_band_cache): count, total
 * char-rows re-composited, and PIT ticks spent. rows/rebuilds shows the
 * incremental win directly (full = 14, scoped ~= 3). */
static unsigned long prof_band_rebuilds = 0;
static unsigned long prof_band_rows = 0;
static unsigned long prof_band_pit = 0;
/* Force the whole-band rebuild path (A/B baseline for the incremental
 * scoping). Set by BATTY_FULL_BAND_REBUILD. */
static unsigned char force_full_band_rebuild = 0;
static unsigned long prof_full_dynamic_frames = 0;
static unsigned long prof_ball_only_frames = 0;
static unsigned long prof_ball_object_frames = 0;
static unsigned long prof_ball_dirty_block_bat = 0;
static unsigned long prof_ball_dirty_block_static = 0;
static unsigned long prof_ball_dirty_block_hud = 0;
static unsigned long prof_ball_dirty_block_objects = 0;
static unsigned long prof_ball_dirty_block_bricks = 0;
static unsigned long prof_ball_dirty_block_balls = 0;
static unsigned long prof_ball_dirty_block_bat_fx = 0;
static unsigned long profile_auto_frames = 0;
/* While set the frame body does no physics; only P (toggle), ESC
 * (quit) and ENTER (advance) are acted on. */
static unsigned char paused = 0;
static unsigned char force_bat_full_redraw = 0;
static unsigned char force_ball_full_redraw = 0;
static unsigned char force_full_flush_each_frame = 0;
/* LAFFC brick collision is now the DEFAULT for the primary ball: it is
 * byte-exact vs the Spectrum over L3's full 150-frame trajectory (dozens
 * of bounces / cell configs, gated by test-laffc-ball-frame1) and falls
 * back to brick_collision when it reports no hit, so it can never pass a
 * brick through. BATTY_LEGACY_COLLISION=1 reverts to the old
 * brick_collision path. (Multi-ball secondaries still use brick_collision.) */
static unsigned char use_laffc = 1;
/* RNG-model alignment (see notes/rng-model.md). OFF by default: the port
 * advances the RNG on demand at each consumer. ON (BATTY_RNG_PERFRAME=1):
 * tick the RNG once per frame at the play-loop top (mirroring the
 * original's per-frame `CALL random_generate` at LB9E8_2) and let
 * read-current consumers sample without advancing — the original's model.
 *
 * Flag ON is now the VALIDATED-CORRECT model: random_number lives at the
 * original's $8D48 (init $8E17, = the port's init) and ticks per frame;
 * seeded to the original's frame-0 state, the port's next_random walk
 * reproduces the original's $8D48 sequence EXACTLY (offset by the one
 * frame the original doesn't tick at its snapshot start); the byte-exact
 * Now the DEFAULT (2026-06-05). The per-frame tick is the original's model
 * (random_generate once per frame at the main-loop top; consumers read
 * without advancing). It is byte-exact: `make test-rng-walk` proves the
 * port's random_number walk equals the original's f0..f4 from the L3 seed,
 * and that makes the enemy steering match the GT (dir 0x11->0x12->0x13).
 * The earlier flag-OFF (advance-on-read) consumed the RNG faster than the
 * original. `BATTY_RNG_PERFRAME=0` reverts to the old behaviour (the
 * BATTY_LAFFC fallback pattern); the RNG-independent gates (ball, bat,
 * enemy-descend, visual states) stay green either way. */
static unsigned char rng_perframe = 1;
static unsigned char suppress_no_ball_death = 0;

static unsigned short last_prof_tick = 0;

static unsigned short pit_current_ticks(void) {
    unsigned char low, high;
    unsigned short val;
    _disable();
    outp(0x43, 0x00);
    low  = (unsigned char)inp(0x40);
    high = (unsigned char)inp(0x40);
    _enable();
    val = (unsigned short)(((unsigned short)high << 8) | low);
    return val;
}

static void prof_start(void) {
    last_prof_tick = pit_current_ticks();
}

static unsigned short prof_elapsed(void) {
    unsigned short now = pit_current_ticks();
    unsigned short diff;
    if (now <= last_prof_tick) {
        diff = last_prof_tick - now;
    } else {
        diff = (unsigned short)((last_prof_tick - now) + 23864u);
    }
    last_prof_tick = now;
    return diff;
}

static void write_profile_report(void) {
    FILE *f = fopen("A:\\PROFILE.TXT", "w");
    if (!f) f = fopen("PROFILE.TXT", "w");
    if (f) {
        unsigned long total = prof_bg_pit + prof_frame_pit + prof_hud_pit + prof_bricks_pit + prof_vga_pit;
        fprintf(f, "Profiling Report over %lu frames:\n", prof_frames_count);
        if (total > 0) {
            fprintf(f, "  paint_bg_to_buff:     %lu (%u%%)\n", prof_bg_pit, (unsigned)((prof_bg_pit * 100) / total));
            fprintf(f, "  paint_frame_to_buff:  %lu (%u%%)\n", prof_frame_pit, (unsigned)((prof_frame_pit * 100) / total));
            fprintf(f, "  HUD / Lives:          %lu (%u%%)\n", prof_hud_pit, (unsigned)((prof_hud_pit * 100) / total));
            fprintf(f, "  render_brick_band:    %lu (%u%%)\n", prof_bricks_pit, (unsigned)((prof_bricks_pit * 100) / total));
            fprintf(f, "  buff_to_vga:          %lu (%u%%)\n", prof_vga_pit, (unsigned)((prof_vga_pit * 100) / total));
        }
        fprintf(f, "  static rebuilds:      %lu\n", prof_static_rebuilds);
        fprintf(f, "  band rebuilds:        %lu\n", prof_band_rebuilds);
        fprintf(f, "  band rows rebuilt:    %lu\n", prof_band_rows);
        fprintf(f, "  band rebuild PIT:     %lu\n", prof_band_pit);
        fprintf(f, "  full dynamic frames:  %lu\n", prof_full_dynamic_frames);
        fprintf(f, "  ball-only frames:     %lu\n", prof_ball_only_frames);
        fprintf(f, "  ball-object frames:   %lu\n", prof_ball_object_frames);
        fprintf(f, "  ball block bat:       %lu\n", prof_ball_dirty_block_bat);
        fprintf(f, "  ball block static:    %lu\n", prof_ball_dirty_block_static);
        fprintf(f, "  ball block HUD:       %lu\n", prof_ball_dirty_block_hud);
        fprintf(f, "  ball block objects:   %lu\n", prof_ball_dirty_block_objects);
        fprintf(f, "  ball block bricks:    %lu\n", prof_ball_dirty_block_bricks);
        fprintf(f, "  ball block balls:     %lu\n", prof_ball_dirty_block_balls);
        fprintf(f, "  ball block bat FX:    %lu\n", prof_ball_dirty_block_bat_fx);
        fprintf(f, "  VGA rect flushes:     %lu\n", prof_vga_rects);
        fprintf(f, "  VGA bytes written:    %lu\n", prof_vga_bytes);
        fprintf(f, "  sound disabled:       %u\n", (unsigned)(sound_is_enabled() ? 0 : 1));
        fprintf(f, "  Total PIT ticks sum:  %lu\n", total);
        fclose(f);
    }
}

/* Buffer-pipeline variant of paint_strip: write the strip's pixel
 * data into scr_buff (overwriting the bg pattern that paint_bg_to_buff
 * left there) and the attrs into attr_buff. Called before bat / ball
 * / enemy / etc blits so those sprites can OR-merge into the frame
 * via the original (mask | screen) ^ pixel semantics — exactly the
 * way the original game's frame and bat coexist in the side strips. */
static void paint_strip_to_buff(const unsigned char *pixels,
                                 const unsigned char *attrs, int attr_stride,
                                 int cols_bytes, int rows_px,
                                 int x0_px, int y0_px) {
    int char_row, char_col, pix_row;
    int char_rows = rows_px / 8;
    int byte_col_off = x0_px / 8;
    int char_row_off = y0_px / 8;
    if (x0_px >= 0 && y0_px >= 0
        && byte_col_off + cols_bytes <= 32
        && y0_px + rows_px <= PLAYFIELD_H
        && char_row_off + char_rows <= ATTR_ROWS) {
        int y;
        for (y = 0; y < rows_px; y++) {
            memcpy(&scr_buff[(y0_px + y) * 32 + byte_col_off],
                        &pixels[y * cols_bytes],
                        (unsigned int)cols_bytes);
        }
        for (char_row = 0; char_row < char_rows; char_row++) {
            memcpy(&attr_buff[(char_row_off + char_row) * 32 + byte_col_off],
                        &attrs[char_row * attr_stride],
                        (unsigned int)cols_bytes);
        }
        return;
    }
    for (char_row = 0; char_row < char_rows; char_row++) {
        for (char_col = 0; char_col < cols_bytes; char_col++) {
            int abs_col = byte_col_off + char_col;
            int abs_char_row = char_row_off + char_row;
            if (abs_col < 0 || abs_col >= 32) continue;
            if (abs_char_row < 0 || abs_char_row >= ATTR_ROWS) continue;
            attr_buff[abs_char_row * 32 + abs_col] =
                attrs[char_row * attr_stride + char_col];
            for (pix_row = 0; pix_row < 8; pix_row++) {
                int y = y0_px + char_row * 8 + pix_row;
                if (y < 0 || y >= PLAYFIELD_H) continue;
                scr_buff[y * 32 + abs_col] =
                    pixels[(char_row * 8 + pix_row) * cols_bytes + char_col];
            }
        }
    }
}

/* Paint the frame top + sides into scr_buff / attr_buff using the
 * per-level attrs from level_attrs. Called BEFORE bat / ball / etc
 * blits so the frame's pixels participate in the OR-blit — fixes
 * "bat invisible at extremes" where direct-VGA frame painting
 * overwrote the side-strip half of the bat sprite. */
static void paint_frame_to_buff(unsigned char cycle, unsigned char level_idx) {
    const unsigned char *base     = frame_l1 + (unsigned int)cycle * FRAME_SIZE;
    const unsigned char *top_px   = base;
    const unsigned char *left_px  = top_px  + FRAME_TOP_PX  + FRAME_TOP_ATTRS;
    const unsigned char *right_px = left_px + FRAME_SIDE_PX + FRAME_SIDE_ATTRS;
    const unsigned char *lattr = level_attrs + (unsigned int)level_idx * ATTR_BAND_SIZE;
    int right_col = 32 - FRAME_SIDE_W;
    paint_strip_to_buff(top_px,   lattr, 32, 32, FRAME_TOP_H_PX,
                        0, 0);
    paint_strip_to_buff(left_px,  lattr + (FRAME_TOP_H_PX / 8) * ATTR_COLS, 32,
                        FRAME_SIDE_W, FRAME_SIDE_H_PX,
                        0, FRAME_TOP_H_PX);
    paint_strip_to_buff(right_px, lattr + (FRAME_TOP_H_PX / 8) * ATTR_COLS + right_col, 32,
                        FRAME_SIDE_W, FRAME_SIDE_H_PX,
                        right_col * 8, FRAME_TOP_H_PX);
}

static void restore_top_frame_center(unsigned char cycle, unsigned char level_idx) {
    const unsigned char *base = frame_l1 + (unsigned int)cycle * FRAME_SIZE;
    const unsigned char *top_px = base;
    const unsigned char *lattr = level_attrs + (unsigned int)level_idx * ATTR_BAND_SIZE;
    int y, cr;
    for (y = 0; y < FRAME_TOP_H_PX; y++) {
        memcpy(&scr_buff[y * 32 + 8], &top_px[y * 32 + 8], 3);
    }
    for (cr = 0; cr < FRAME_TOP_H_PX / 8; cr++) {
        memcpy(&attr_buff[cr * 32 + 8], &lattr[cr * 32 + 8], 3);
    }
}

/* sprites.bin-relative wrappers over the video engine's blits
 * (zxvga.c §6): resolve a sprite offset in the loaded blob, then hand
 * the raw sprite pointer to the engine. */
static void blit_masked_sprite(unsigned int sprite_off, int x_px, int y_px,
                               unsigned char ink, unsigned char paper) {
    blit_masked_sprite(sprites_blob + sprite_off, x_px, y_px, ink, paper);
}

static void blit_masked_to_scr_buff(unsigned int sprite_off,
                                     int x_px, int y_px) {
    blit_masked_to_scr_buff(sprites_blob + sprite_off, x_px, y_px);
}

/* Sprite width/height live in the blob's first two bytes (bytes, rows). */
static void mark_dirty_sprite_rect(unsigned int spr, int x, int y) {
    mark_dirty_rect_px(x, y, (int)sprites_blob[spr] * 8, sprites_blob[spr + 1]);
}

/* Bat ink + paper come from the BG attr at the bat's position - same
 * palette as the surrounding hex pattern, as in the original. The bat
 * texture-detail (mask=1, pixel=1 pixels) renders as paper colour;
 * body pixels (mask=1, pixel=0) render as ink. */


/* Initial values are byte-exact copies of the DEFB blocks at $9AD0+. */
Object objects[N_OBJECTS] = {
    /* OBJ_BALL_1   @ $9AD0 */
    { 0x02,0x00, 0x84,0x00, 0xA0,0x00, 0x38, 0x02,
      0x02, 0x0C, 0x00,0x00, 0x08, 0x07, 0x00,0x00,
      0x00,0x00, 0x00, 0x00, 0x00, 0x80 },
    /* OBJ_BALL_2   @ $9AE6 */
    { 0x00,0x00, 0x84,0x00, 0xA0,0x00, 0x38, 0x02,
      0x02, 0x0C, 0x00,0x00, 0x08, 0x07, 0x00,0x00,
      0x00,0x00, 0x00, 0x00, 0x00, 0x80 },
    /* OBJ_BALL_3   @ $9AFC */
    { 0x00,0x00, 0x84,0x00, 0xA0,0x00, 0x38, 0x02,
      0x02, 0x0C, 0x00,0x00, 0x08, 0x07, 0x00,0x00,
      0x00,0x00, 0x00, 0x00, 0x00, 0x80 },
    /* OBJ_BULLET_1 @ $9B12 */
    { 0x00,0x00, 0x84,0x00, 0xA0,0x00, 0x30, 0x01,
      0x01, 0x08, 0x00,0x00, 0x04, 0x08, 0x00,0x00,
      0x00,0x00, 0x00, 0x00, 0x00, 0x80 },
    /* OBJ_BULLET_2 @ $9B28 */
    { 0x00,0x00, 0x84,0x00, 0xA0,0x00, 0x30, 0x01,
      0x01, 0x08, 0x00,0x00, 0x04, 0x08, 0x00,0x00,
      0x00,0x00, 0x00, 0x00, 0x00, 0x80 },
    /* OBJ_BAT_2    @ $9B3E */
    { 0x00,0x00, 0x74,0x00, 0xAD,0x00, 0x00, 0x00,
      0x04, 0x0D, 0x00,0x00, 0x1C, 0x0A, 0x00,0x00,
      0x00,0x00, 0xF0, 0x00, 0xFF, 0x80 },
    /* OBJ_BAT_1    @ $9B54 */
    { 0x01,0x00, 0x74,0x00, 0xAD,0x00, 0x00, 0x00,
      0x04, 0x0D, 0x00,0x00, 0x1C, 0x0A, 0x00,0x00,
      0x00,0x00, 0xF0, 0x00, 0x00, 0x80 },
    /* OBJ_BAT_TEMP @ $9B6A */
    { 0x00,0x03, 0x84,0x00, 0xAD,0x00, 0x00, 0x00,
      0x03, 0x0D, 0x00,0x00, 0x1B, 0x0A, 0x00,0x00,
      0x00,0x00, 0x00, 0x00, 0x00, 0x00 },
    /* OBJ_BONUS    @ $9B80 */
    { 0x00,0x00, 0x28,0x00, 0x9F,0x00, 0x00, 0x00,
      0x00, 0x00, 0x00,0x00, 0x00, 0x00, 0x00,0x00,
      0x00,0x00, 0xF0, 0x60, 0x00, 0x00 },
    /* OBJ_ENEMY    @ $9B96 */
    { 0x00,0x01, 0x78,0x00, 0x88,0x00, 0x00, 0x00,
      0x03, 0x18, 0x00,0x00, 0x18, 0x18, 0x00,0x00,
      0x00,0x00, 0x50, 0x44, 0x00, 0x00 },
    /* OBJ_ROCKET   @ $9BAC */
    { 0x00,0x00, 0xF8,0x00, 0xA8,0x00, 0x00, 0x00,
      0x03, 0x1C, 0x00,0x00, 0x00, 0x00, 0x00,0x00,
      0x00,0x00, 0x00, 0x00, 0x00, 0x00 }
};

/* The 3 special slots from the original (lives_indicator, score_
 * indicator, separator at $9BC2, $9BD4, $9BDC) are not part of the
 * 11-slot iteration. We'll add them when handling_object work makes
 * them load-bearing - for now they're rendered by the legacy paths. */

/* Sugar over the bat's descriptor fields so gameplay code reads
 * naturally. The bat is anchored by object_bat_1 (the original game's
 * 1P bat); object_bat_2 covers 2P mode. */
#define BAT_X       (objects[OBJ_BAT_1].x_coord)
#define BAT_Y       (objects[OBJ_BAT_1].y_coord)
#define BAT_PREV_X  (objects[OBJ_BAT_1].prev_x)
#define BALL_X      (objects[OBJ_BALL_1].x_coord)
#define BALL_Y      (objects[OBJ_BALL_1].y_coord)
/* BIT 7 of sprite_set marks the object inactive in the original. We
 * use the same convention for "ball not yet released / hidden". */
#define BALL_VISIBLE     ((objects[OBJ_BALL_1].sprite_set & 0x80) == 0)
#define BALL_SHOW()      (objects[OBJ_BALL_1].sprite_set = 0x02)
#define BALL_HIDE()      (objects[OBJ_BALL_1].sprite_set = 0x82)


static void primary_ball_set_velocity(int dx, int dy) {
    ball_dx = dx;
    ball_dy = dy;
    objects[OBJ_BALL_1].speed = BALL_SPEED;
    objects[OBJ_BALL_1].x_coord_hi = 0;
    objects[OBJ_BALL_1].y_coord_hi = 0;
    if (dy < 0) {
        if (dx < 0)      objects[OBJ_BALL_1].dir = 0x24;
        else if (dx > 0) objects[OBJ_BALL_1].dir = 0x1B;
        else             objects[OBJ_BALL_1].dir = 0x20;
    } else if (dy > 0) {
        if (dx < 0)      objects[OBJ_BALL_1].dir = 0x3B;
        else if (dx > 0) objects[OBJ_BALL_1].dir = 0x04;
        else             objects[OBJ_BALL_1].dir = 0x00;
    }
}

static void primary_ball_launch_from_bat(void) {
    unsigned char dir;
    int launch_offset = stuck_offset_x - 4;
    if (launch_offset < 0) launch_offset = 0;
    /* Original LA27E_15 derives the release direction from the stuck
     * bat offset, with $30 remapped to $34. The first movement step can
     * still resolve against the bat and produce the actual upward
     * trajectory; skipping that step was what made launch drift from
     * the Spectrum behavior. */
    dir = (unsigned char)((launch_offset + 0x24) & 0x3F);
    if (dir == 0x30) dir = 0x34;
    dir_to_dxdy(dir, BALL_SPEED, &ball_dx, &ball_dy);
    ball_dx = (ball_dx < 0) ? -1 : (ball_dx > 0 ? 1 : 0);
    ball_dy = (ball_dy < 0) ? -1 : (ball_dy > 0 ? 1 : 0);
    objects[OBJ_BALL_1].dir = dir;
    objects[OBJ_BALL_1].speed = BALL_SPEED;
    objects[OBJ_BALL_1].x_coord_hi = 0;
    objects[OBJ_BALL_1].y_coord_hi = 0;
}

static void record_primary_launch(void) {
    last_primary_launch_valid = 1;
    last_primary_launch_x = BALL_X;
    last_primary_launch_y = BALL_Y;
    last_primary_launch_dir = objects[OBJ_BALL_1].dir;
    last_primary_launch_speed = objects[OBJ_BALL_1].speed;
    if (launch_probe_frames != 0) {
        launch_probe_countdown = launch_probe_frames;
        launch_probe_active = 1;
    }
}

/* --- Per-object handler dispatch (handling_object @ $9F54) ------------ */

typedef void (*obj_handler_t)(Object *obj);

static void handling_bat_stub(Object *o)  { (void)o; }
static void handling_ball_obj(Object *o)  { (void)o; }
static void handling_bonus_obj(Object *o) { (void)o; }
static void handling_bullet_obj(Object *o){ (void)o; }
static void handling_rocket_obj(Object *o){ (void)o; }
/* L4's spark enemy: short-lived bouncing dot that decays through
 * 5 sprite frames before vanishing. dir bit 0 = X heading (0=right,
 * 1=left); bit 1 = Y heading (0=down, 1=up). Frame index ramps up
 * with misc_12 against a decay threshold table — rough port of the
 * original's halving-timer mechanic, simpler to reason about. */
static const unsigned char spark_frame_threshold[SPARK_FRAMES] = {
    16, 24, 28, 30, 31
};
static void handling_spark_obj(Object *o) {
    int dx = (o->dir & 1) ? -(int)o->speed : (int)o->speed;
    int dy = (o->dir & 2) ? -(int)o->speed : (int)o->speed;
    int nx = (int)o->x_coord + dx;
    int ny = (int)o->y_coord + dy;
    unsigned char f;
    o->misc_12++;
    for (f = 0; f < SPARK_FRAMES; f++) {
        if (o->misc_12 < spark_frame_threshold[f]) {
            o->sprite_num = f;
            break;
        }
    }
    if (f >= SPARK_FRAMES) {
        o->sprite_set |= 0x80;
        return;
    }
    if (nx < 8) { nx = 8; o->dir &= (unsigned char)~1; }
    else if (nx >= PLAYFIELD_W - 8 - (int)o->w_body_px) {
        nx = PLAYFIELD_W - 8 - (int)o->w_body_px - 1;
        o->dir |= 1;
    }
    if (ny < 0) { ny = 0; o->dir &= (unsigned char)~2; }
    else if (ny >= PLAYFIELD_H - (int)o->h_body_px) {
        o->sprite_set |= 0x80;
        return;
    }
    o->x_coord = (unsigned char)nx;
    o->y_coord = (unsigned char)ny;
}
/* Literal port of LAAD2 ($AAD2) — the original's shared per-object
 * sprite-animation stepper (bird, ufo, blast; the bat/bullet use it
 * too but keep their own port approximations for now). +$12 (misc_12)
 * is a cadence counter: while >= $40 it just loses $40 per call; on
 * underflow the sprite frame +$01 advances LINEARLY and wraps via
 * +$13's (misc_13) nibbles — HIGH nibble = last frame, LOW = first —
 * and the counter reloads as ((res<<2)&$C0)|res. Seeds: bird
 * $F0/$70 -> frames 0..7 every 4 frames; UFO $60/$90 -> frames 0..9
 * every 3 frames (after one 2-call lead-in); blast $50/$90 -> frames
 * 0..9 every 2 frames. (The original also re-derives the sprite's
 * w/h via calc_write_spr_addr here; the port reads dims at render
 * time, so that part is unnecessary.) See notes/bird-render-parity.md
 * for the decode + the dead facing-mirror block this replaces. */

/* Port of handling_blast at $AA30: force +$13=$90 (frames 0..9), step
 * LAAD2, free the slot the moment sprite_num reaches 9 (`CP $09 /
 * RET NZ / SET 7`). Kill sites seed +$12=$50 / sprite_num=0 like the
 * original kill_enemy ($A4C4), giving the 2-frames-per-step cadence. */
static void handling_blast_obj(Object *o) {
    o->misc_13 = 0x90;
    object_step_animation(*(o));
    if ((o->sprite_num & 0x3F) == 9) {
        /* Clear sprite_set to 0 (= empty slot) so enemy_prepare can
         * spawn a fresh alien next time the spawn conditions hit. The
         * original "SET 7,(IX+\$00)" leaves sprite_set as \$8A; combined
         * with our enemy_prepare check of "sprite_set != 0" that would
         * permanently block respawns for the rest of the level. */
        o->sprite_set = 0;
    }
}
static void handling_400pts_obj(Object *o){ (void)o; }

/* (Removed enemy_dir_delta_q8 + direction_table_q8: the enemy now moves
 * with the exact dir_to_dxdy / hl_bc_calc_direction, like the ball and the
 * original's LAD69. The old routine had the X/Y components swapped per
 * quadrant, flying the bird on the wrong axis.) */


/* Wall reflect for ANY ball object: flip the 6-bit dir about the X and/or
 * Y axis (the original's wall-bounce dir mapping). Shared by the primary
 * and (once unified) the multi-ball secondaries. */
static void ball_reflect_descriptor(int flip_x, int flip_y) {
    int dx_q8, dy_q8;
    object_reflect(*(&objects[OBJ_BALL_1]), flip_x, flip_y);
    dir_to_dxdy(objects[OBJ_BALL_1].dir, objects[OBJ_BALL_1].speed,
                      &dx_q8, &dy_q8);
    ball_dx = (dx_q8 < 0) ? -1 : (dx_q8 > 0 ? 1 : 0);
    ball_dy = (dy_q8 < 0) ? -1 : (dy_q8 > 0 ? 1 : 0);
}

/* Port of LAA7D (called every 4 frames from handling_bird/ufo with the
 * turn step B=1): turn the current dir one 6-bit step toward the target
 * dir; the bit-5 test of (dir - target) picks the shorter way round. When
 * dir == target (LAA7D_1), pick a NEW random target = random_number & $3F.
 * The original refreshes the target ONLY on arrival like this, not on a
 * fixed timer, and reads the current random_number low byte WITHOUT
 * advancing the RNG. (A byte-exact target match additionally needs the
 * original's per-frame RNG tick, which the port advances on demand — see
 * notes/enemy-movement.md.) */



/* Birds/UFOs use a reduced port of the original 6-bit direction-table
 * movement. The original also uses LAA7B target steering and collision
 * reactions; here we keep the same q8.8 motion shape and periodically
 * steer to a new target so enemies roam through the playfield instead
 * of patrolling only along the top edge. */
static void bomb_appear(Object *o);     /* forward decl */
static void handling_bird_obj(Object *o) {
    int dx_q8, dy_q8;
    long nx_q8, ny_q8;
    int nx, ny;
    /* Port of the entry slide at $A9BC line 3429-3433:
     *   LD A,(IX+$04); CP $08; JR NC,LA9BC_0; INC (IX+$04); RET
     * Alien spawns at Y=0 (= top of playfield) and slides down 8 px
     * before starting its horizontal traverse. Earlier port skipped
     * this — alien was visible at Y=0, one char-row above the band
     * the original places it at. */
    if (o->y_coord < 8) {
        o->y_coord++;
        return;
    }
    /* Sprite animation: the literal LAAD2 stepper (per-object +$12
     * cadence, +$13 nibble frame range). The bird's $F0/$70 seed gives
     * the same 4-frame walk as the old `misc_12++ / &3` approximation
     * (test-enemy-anim's pinned f8..f24 walk is unchanged), but the
     * UFO's $60/$90 seed animates every 3 frames over TEN anim_ufo
     * entries — the old code ran it at 4 frames over 8. The original
     * calls LAAD2 at the handler tail (LA902_3/LA9BC_3); within-frame
     * position doesn't matter since rendering happens after all
     * handlers. */
    object_step_animation(*(o));
    bomb_appear(o);
    /* Steer every 4 frames. The original gates on the GLOBAL counter_misc
     * (`LD A,(counter_misc); AND $03; CALL Z,LAA7D`), not a per-object
     * counter, so all enemies turn on the same global 4-frame phase. Use
     * the port's per-frame counter (pit_frame_counter, = the original's
     * counter_misc) instead of o->misc_12, whose phase was spawn-relative
     * (and whose object byte +$12 is the sprite address in the original,
     * not a turn counter). LAA7D also refreshes the target on arrival, so
     * there is no separate timer-based random re-target. */
    if (((unsigned long)pit_frame_counter & 0x03UL) == 0)
        enemy_turn_towards_target(*o);
    /* Move with the EXACT hl_bc_calc_direction (dir_to_dxdy) — the
     * original handling_bird calls LAD69, the same motion routine as the
     * ball. The old enemy_dir_delta_q8 had the X/Y components swapped per
     * quadrant (dir $10 came out moving RIGHT instead of straight DOWN),
     * so the bird flew the wrong axis. */
    dir_to_dxdy(o->dir, o->speed, &dx_q8, &dy_q8);
    nx_q8 = ((long)o->x_coord << 8) + o->x_coord_hi + dx_q8;
    ny_q8 = ((long)o->y_coord << 8) + o->y_coord_hi + dy_q8;
    nx = (int)(nx_q8 >> 8);
    ny = (int)(ny_q8 >> 8);
    if (nx < 8) {
        nx = 8; nx_q8 = (long)nx << 8;
        o->dir = (unsigned char)((0x20 - o->dir) & 0x3F);
        o->x_coord = (unsigned char)nx;
        o->y_coord = (unsigned char)ny;
        enemy_target_away_from_margins(*o);
    } else if (nx >= PLAYFIELD_W - 8 - (int)o->w_body_px) {
        nx = PLAYFIELD_W - 8 - (int)o->w_body_px; nx_q8 = (long)nx << 8;
        o->dir = (unsigned char)((0x20 - o->dir) & 0x3F);
        o->x_coord = (unsigned char)nx;
        o->y_coord = (unsigned char)ny;
        enemy_target_away_from_margins(*o);
    }
    if (ny < 8) {
        ny = 8; ny_q8 = (long)ny << 8;
        o->dir = (unsigned char)((0x40 - o->dir) & 0x3F);
        o->x_coord = (unsigned char)nx;
        o->y_coord = (unsigned char)ny;
        enemy_target_away_from_margins(*o);
    } else if (ny >= PLAYFIELD_H) {
        o->sprite_set |= 0x80;
        return;
    }
    o->x_coord = (unsigned char)nx;
    o->x_coord_hi = (unsigned char)(nx_q8 & 0xFF);
    o->y_coord = (unsigned char)ny;
    o->y_coord_hi = (unsigned char)(ny_q8 & 0xFF);
}
static void handling_ufo_obj(Object *o) { handling_bird_obj(o); }

/* Indexed by sprite_set (the original's table starts at index 1; we
 * leave slot 0 NULL since sprite_set=0 means "inactive"). */
static const obj_handler_t handling_table_routines[] = {
    NULL,                /* 0  - inactive */
    handling_bat_stub,   /* 1  gfx_bat - real handler dispatched manually */
    handling_ball_obj,   /* 2  gfx_ball */
    handling_ball_obj,   /* 3  gfx_screen_elements (shares handler) */
    handling_bonus_obj,  /* 4  gfx_bonuses */
    handling_bullet_obj, /* 5  gfx_bullet */
    handling_rocket_obj, /* 6  anim_rocket */
    handling_spark_obj,  /* 7  anim_spark */
    handling_ufo_obj,    /* 8  anim_ufo */
    handling_bird_obj,   /* 9  anim_bird */
    handling_blast_obj,  /* A  anim_alien_blast */
    handling_400pts_obj  /* B  gfx_last_sprite (the 400-points marker) */
};

/* Port of handling_object at $9F54. Dispatches via sprite_set; skips
 * inactive slots (sprite_set == 0 or BIT7 set). */
static void handling_object(Object *obj) {
    unsigned char id = obj->sprite_set;
    if (id == 0 || (id & 0x80)) return;
    id &= 0x7F;
    if (id >= sizeof(handling_table_routines) / sizeof(handling_table_routines[0]))
        return;
    if (handling_table_routines[id]) handling_table_routines[id](obj);
}

/* Port of call_hl_for_all_obj at $B66A. Iterates the 11-slot table,
 * calls fn(slot) for each, skipping slots with sprite_set == 0
 * (matches the original's ADD A,A / CALL NZ guard). */
/* call_for_all_obj hands each slot by pointer, so the module's
 * reference-taking helper needs a shim. */
static void refresh_buffer_offset(Object *o) { object_update_buffer_offset(*o); }

static void call_for_all_obj(obj_handler_t fn) {
    int i;
    for (i = 0; i < N_OBJECTS; i++) {
        if (objects[i].sprite_set != 0) fn(&objects[i]);
    }
}

/* Bat sprite layout (both spr_bat_normal and spr_bat_big):
 *   rows 0..9  - body (mask=1 = bat colour, pixel=1 = paper for
 *                internal texture).
 *   rows 10..12 - shadow drop (mask=0 dotted pattern with pix=1 -
 *                  the original OR-blit's (mask|screen)^pix flips
 *                  bg bits at those positions, producing the
 *                  textured shadow band below the bat).
 * One blit into scr_buff covers the whole sprite. */
static void render_bat(unsigned char cycle, unsigned char attr) {
    unsigned int spr;
    int x, y, sprite_w;
    (void)cycle;
    if (bat_extra_px >= BAT_BIG_EXTRA_PX) {
        spr = SPR_BAT_BIG;
        x   = BAT_X - BAT_BIG_EXTRA_PX;
        sprite_w = BAT_W_BYTES * 8 + 2 * BAT_BIG_EXTRA_PX;
    } else {
        /* Laser-carrying bat shows the gun-mounted sprite. While the
         * fire-animation counter is non-zero, cycle through the four
         * spr_bat_gun_1..4 frames (2 ticks per frame, picked off the
         * countdown). Same 32 x 13 footprint as spr_bat_normal. */
        if (objects[OBJ_BAT_1].bonus_applied == 0x01) {
            if (bat_fire_anim_ticks >= 7)      spr = SPR_BAT_GUN_1;
            else if (bat_fire_anim_ticks >= 5) spr = SPR_BAT_GUN_2;
            else if (bat_fire_anim_ticks >= 3) spr = SPR_BAT_GUN_3;
            else if (bat_fire_anim_ticks >= 1) spr = SPR_BAT_GUN_4;
            else                               spr = SPR_BAT_GUN;
        } else {
            spr = SPR_BAT_NORMAL;
        }
        x   = BAT_X;
        sprite_w = BAT_W_BYTES * 8 + 2 * bat_extra_px;
        if (bat_extra_px > 0) {
            /* Resize ramp side-fillers: stuff solid bits into scr_buff
             * so buff_to_vga lights them with bg's ink. */
            int side_w = bat_extra_px;
            int row;
            for (row = 0; row < 8; row++) {
                int yy = BAT_Y + 1 + row;
                int bx;
                if (yy < 0 || yy >= PLAYFIELD_H) continue;
                for (bx = BAT_X - side_w; bx < BAT_X; bx++) {
                    if (bx >= 0 && bx < PLAYFIELD_W) {
                        scr_buff[yy * 32 + (bx >> 3)] |= (unsigned char)(0x80 >> (bx & 7));
                    }
                }
                for (bx = BAT_X + BAT_W_BYTES * 8; bx < BAT_X + BAT_W_BYTES * 8 + side_w; bx++) {
                    if (bx >= 0 && bx < PLAYFIELD_W) {
                        scr_buff[yy * 32 + (bx >> 3)] |= (unsigned char)(0x80 >> (bx & 7));
                    }
                }
            }
        }
    }
    y = BAT_Y;
    /* Force bg attr on the interior playfield cells the bat covers, but
     * leave the side-frame attr cells alone. The sprite pixels still
     * OR into the frame at the extremes; only the static tube colour
     * must stay owned by paint_frame_to_buff. */
    blit_sprite_attrs_to_buff_clipped(x - bat_extra_px, y,
                                      sprite_w, 13, attr,
                                      8, PLAYFIELD_W - 8);
    blit_masked_to_scr_buff(spr, x, y);
}

static int bat_draw_extra_for_bounds(int extra) {
    return (extra >= BAT_BIG_EXTRA_PX) ? BAT_BIG_EXTRA_PX : extra;
}

static void bat_sprite_bounds(int x, int extra, int *x0, int *x1) {
    int e = bat_draw_extra_for_bounds(extra);
    *x0 = x - e;
    *x1 = x + BAT_W_BYTES * 8 + e;
}

static void remember_bat_draw_state(void) {
    BAT_PREV_X = BAT_X;
    bat_draw_extra_px = bat_extra_px;
    bat_draw_y = BAT_Y;
    bat_draw_bonus_applied = objects[OBJ_BAT_1].bonus_applied;
    bat_draw_fire_ticks = bat_fire_anim_ticks;
}

/* Two pixels that walk back and forth along the bat — the original's
 * running_dot at $B8E6. Drawn into scr_buff at offset $1660 (row 179,
 * = 6 rows into the 13-row bat sprite) by ANDing one bit per dot via
 * running_dot_mask, "punching" a dark pixel through the otherwise
 * solid bat ink. Frame counter advances by 1 per frame, reverses
 * direction at the inner edges (9 from the left, 10 from the right).
 *
 * Frame counter layout: bit 7 = direction (1 = walking toward smaller
 * frame values, 0 = larger), bits 6..0 = current frame in [9, w-10]. */
#define RUN_DOT_ROW_OFF  0x1660     /* scr_buff row 179, col 0 */
static unsigned char run_dot_frame = 0x0E;   /* matches running_dot_frame_1up DEFB $0E */
static const unsigned char run_dot_mask[8] = {
    0x7F, 0xBF, 0xDF, 0xEF, 0xF7, 0xFB, 0xFD, 0xFE
};

static void run_dot_punch(int abs_x) {
    int byte_col = (abs_x >> 3) & 0x1F;
    unsigned int off = RUN_DOT_ROW_OFF + byte_col;
    scr_buff[off] &= run_dot_mask[abs_x & 7];
}

static int test_mode_pin_blink;   /* set by BATTYALL env */

static void render_running_dot(void) {
    int bat_w, bat_left;
    int frame, dir, span;
    /* Test-mode determinism: the GT was captured after exactly one
     * gameplay-loop iter (PC=0xBB61), so the original's running_dot
     * punched at frame=0x0E. Our test reaches the screendump after many
     * iters during which run_dot_frame would have advanced and the dots
     * would land elsewhere. Pin it so the dots stay where the GT has
     * them. Same trick as test_mode_pin_blink. */
    if (test_mode_pin_blink) run_dot_frame = 0x0E;
    /* Bat "logical width" per object_bat_1+$0C = $1C = 28 px (and
     * object_bat_temp+$0C = $1B = 27 px for big-bat). The bat sprite
     * is 32 px wide but the running_dot uses this narrower W for the
     * mirror-position calc, so the dots land inside the body cap
     * rather than at the tapered sprite edges. Earlier port used the
     * sprite width (32), which placed the second dot 2 px too far
     * right against the GT. */
    if (bat_extra_px >= BAT_BIG_EXTRA_PX) {
        bat_w    = 28 + 2 * BAT_BIG_EXTRA_PX;   /* 44 px in big-bat mode */
        bat_left = BAT_X - BAT_BIG_EXTRA_PX;
    } else {
        bat_w    = 28 + 2 * bat_extra_px;
        bat_left = BAT_X - bat_extra_px;
    }
    /* Original's recovery branch: if the bat shrank into the current
     * frame counter (bat_w - frame < 9), reset frame to bat_w - 11
     * keeping the direction bit. */
    frame = run_dot_frame & 0x7F;
    if (bat_w - frame < 9) {
        frame = bat_w - 11;
        if (frame < 9) frame = 9;
        run_dot_frame = (unsigned char)((run_dot_frame & 0x80) | frame);
    }
    /* Punch both dots: one at frame from the left, one mirrored. */
    run_dot_punch(bat_left + frame);
    run_dot_punch(bat_left + bat_w - frame - 1);
    /* Advance the counter. Direction bit set = decreasing; clear =
     * increasing. Floor at 9, ceiling at bat_w - 10. */
    dir = run_dot_frame & 0x80;
    if (dir) {
        frame--;
        if (frame <= 9) {
            frame = 9;
            run_dot_frame = (unsigned char)frame;     /* flip to increasing */
        } else {
            run_dot_frame = (unsigned char)(0x80 | frame);
        }
    } else {
        frame++;
        span = bat_w - 10;
        if (frame >= span) {
            frame = span;
            run_dot_frame = (unsigned char)(0x80 | frame);   /* flip to decreasing */
        } else {
            run_dot_frame = (unsigned char)frame;
        }
    }
}

/* Display (lives - 2) right-side indicators next to the left one
 * baked into the frame strip. Cap at 4 to fit. */
/* Port of LBE8B_8's `ADD A,$10; CP $E9; JR NC` cap: the indicator
 * stops advancing X once it would land at $E9 (= 233), giving at
 * most 14 distinct sprite slots between $08 and $D8. Earlier port
 * capped at 4 — fine for normal play but truncated the meter
 * for high-life scores. */
#define LIVES_DYNAMIC_MAX 14
static void render_lives(unsigned char cycle, unsigned char attr) {
    /* Port of LBE8B_7's `LD A,(lives_1up); DEC A; JR Z,skip`:
     * draw (lives - 1) indicator bats. lives=3 → 2 sprites,
     * lives=2 → 1, lives=1 → 0 (on the player's last life the
     * meter is empty). Earlier port used (lives - 2) — off by one. */
    int show = lives - 1;
    int i;
    (void)cycle;
    (void)attr;
    if (show < 0) show = 0;
    if (show > LIVES_DYNAMIC_MAX) show = LIVES_DYNAMIC_MAX;
    for (i = 0; i < show; i++) {
        blit_masked_to_scr_buff(SPR_LIVES,
                                LIVES_X_PX + i * 16,
                                LIVES_Y_PX);
    }
}








/* Brick compositor stage. Writes brick bricks/edges into scr_buff and
 * per-cell attrs (brick + shadow) into attr_buff for char rows 3..16.
 * Does NOT touch VGA — buff_to_vga handles the final pass. Assumes
 * paint_bg_to_buff already pre-filled the rest of the buffers. */
static void print_border_shadow_c(void);
static void render_brick_band(unsigned char level_idx) {
    int lvl_row, lvl_col;
    const unsigned char *cells = live_level;
    const unsigned char *lattr = &level_attrs[(int)level_idx * ATTR_BAND_SIZE];
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];

    if (level_idx >= N_LEVELS) return;

    /* Copy the per-level attrs into char rows 3..16 (the brick band,
     * including frame side strips and pre-dimmed shadow rows). */
    memcpy(&attr_buff[3 * 32], &lattr[3 * ATTR_COLS], 14 * 32);

    /* level_attrs.bin was captured with all bricks alive, so it carries
     * the brick colour in every brick cell. For cells whose brick is
     * destroyed (bit 7), reset the body attr to bg_attr — otherwise
     * destroyed bricks keep showing brick colour even though
     * print_briks_c skips the body pixels. Also clear the shadow row
     * (the char row below): if a live brick is below, print_briks_c will
     * repaint its body attr after this cleanup; if not, the stale dimmed
     * shadow attr disappears with the destroyed brick. */
    {
        for (lvl_row = 0; lvl_row < LVL_ROWS; lvl_row++) {
            for (lvl_col = 0; lvl_col < LVL_COLS; lvl_col++) {
                unsigned char cell = cells[lvl_row * LVL_COLS + lvl_col];
                /* Only RUNTIME-destroyed cells reset to bg_attr — those
                 * have bit 7 set and bit 6 clear. The empty-cell sentinel
                 * $C0 has BOTH bits 7+6 set and must keep its level_attrs
                 * value (the captured per-level attrs include per-side-
                 * strip cell colours we want to preserve). */
                if ((cell & 0xC0) != 0x80) continue;
                {
                    int cr  = 4 + lvl_row;
                    int cc1 = 1 + 2 * lvl_col;
                    int cc2 = cc1 + 1;
                    /* A destroyed cell reveals the brick-band bg. The
                     * original casts an inter-brick shadow from a live
                     * brick onto the LEFT char of the cell to its right, so
                     * a destroyed cell shows a non-bright left char ONLY
                     * when its LEFT NEIGHBOUR is still a live brick (GT:
                     * destroyed col 6 with live col 5 -> left char $05; a
                     * destroyed cell whose left neighbour is also gone
                     * keeps the bright $45). Right char is always bg_attr. */
                    int left_live = (lvl_col > 0) &&
                        !(cells[lvl_row * LVL_COLS + lvl_col - 1] & 0x80);
                    unsigned char latt = left_live
                        ? (unsigned char)(bg_attr & 0xBF) : bg_attr;
                    attr_buff[cr * 32 + cc1] = latt;
                    attr_buff[cr * 32 + cc2] = bg_attr;
                    attr_buff[(cr + 1) * 32 + cc1] = latt;
                    attr_buff[(cr + 1) * 32 + cc2] = bg_attr;
                }
            }
        }
    }

    paint_bricks(cells);
    print_border_shadow_c();
}

/* Row-scoped variant of print_briks_c: re-composite brick rows [r0, r1]
 * into char rows [cr0, cr1] (= [4+r0, 5+r1]: cell rows + the last row's
 * shadow row). Used by the incremental band-cache rebuild.
 *
 * Boundary care (the source of the destroyed-brick leftovers,
 * known-bugs.md #1/#2): char row cr0 doubles as row r0-1's SHADOW row,
 * and cr1 doubles as row r1+1's CELL row. The level_attrs base copy
 * resurrects the captured LIVE look for both, so the destroyed-cell
 * reset loop must extend one brick row beyond [r0, r1] on each side —
 * with each attr write guarded to the base-copied range (rows outside
 * it keep their current, already-correct values). Pixel edges interlock
 * the same way (a brick writes its top/bottom edge one pixel row into
 * the neighbouring cell row); the caller's window and the two edge
 * fix-ups below keep those rows canonical. */
static void render_brick_band_rows(unsigned char level_idx,
                                   int r0, int r1, int cr0, int cr1) {
    int lvl_row, lvl_col, cr;
    const unsigned char *cells = live_level;
    const unsigned char *lattr = &level_attrs[(int)level_idx * ATTR_BAND_SIZE];
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];

    if (level_idx >= N_LEVELS) return;

    /* Base attrs for the recomposited char rows (ATTR_COLS == 32). */
    memcpy(&attr_buff[cr0 * 32], &lattr[cr0 * ATTR_COLS],
                (unsigned int)((cr1 - cr0 + 1) * 32));

    for (lvl_row = r0 - 1; lvl_row <= r1 + 1; lvl_row++) {
        if (lvl_row < 0 || lvl_row >= LVL_ROWS) continue;
        for (lvl_col = 0; lvl_col < LVL_COLS; lvl_col++) {
            unsigned char cell = cells[lvl_row * LVL_COLS + lvl_col];
            if ((cell & 0xC0) != 0x80) continue;
            {
                int cc1 = 1 + 2 * lvl_col;
                int cc2 = cc1 + 1;
                int crr = 4 + lvl_row;
                int left_live = (lvl_col > 0) &&
                    !(cells[lvl_row * LVL_COLS + lvl_col - 1] & 0x80);
                unsigned char latt = left_live
                    ? (unsigned char)(bg_attr & 0xBF) : bg_attr;
                if (crr >= cr0 && crr <= cr1) {
                    attr_buff[crr * 32 + cc1] = latt;
                    attr_buff[crr * 32 + cc2] = bg_attr;
                }
                if (crr + 1 >= cr0 && crr + 1 <= cr1) {
                    attr_buff[(crr + 1) * 32 + cc1] = latt;
                    attr_buff[(crr + 1) * 32 + cc2] = bg_attr;
                }
            }
        }
    }

    paint_brick_rows(cells, r0, r1);

    /* Edge fix-up 1: row r1's print zeroed its bottom-edge row, which in
     * the full ascending paint is overwritten by row r1+1's body row 0
     * where that brick is live — re-paint those two bytes plus the
     * side-edge bit clears print_one_brik would apply on that row. */
    if (r1 + 1 < LVL_ROWS) repaint_row_body_top(cells, r1 + 1);
    /* Edge fix-up 4: row r1's body row 7 (pixel row 39+8*r1, bg-erased
     * and re-painted above) is canonically overwritten by row r1+1's
     * TOP-edge zeros where that brick is live — re-apply them. */
    if (r1 + 1 < LVL_ROWS) repaint_row_top_edge(cells, r1 + 1);
    /* Edge fix-up 3: print's brik_shadow_c(r1) dimmed char row 5+r1,
     * which is row r1+1's CELL row — in the full ascending paint, row
     * r1+1's own print re-brightens its live cells' attrs right after.
     * Re-apply that write since r1+1 isn't printed here. */
    if (r1 + 1 < LVL_ROWS) repaint_row_attrs(cells, r1 + 1);
    /* Edge fix-up 2: a destroyed cell in row r0 sits under row r0-1's
     * bottom-edge zeros; the caller's bg repaint erased them — restore
     * where the brick above is live. */
    if (r0 > 0) {
        unsigned int hl = 0x401u + (unsigned int)r0 * 0x100u;
        for (lvl_col = 0; lvl_col < LVL_COLS; lvl_col++) {
            if ((cells[r0 * LVL_COLS + lvl_col] & 0x80)
                && !(cells[(r0 - 1) * LVL_COLS + lvl_col] & 0x80)) {
                scr_buff[hl]     = 0;
                scr_buff[hl + 1] = 0;
            }
            hl += 2;
        }
    }

    /* Border-shadow (left col-1 dim) for the recomposited char rows. */
    for (cr = cr0; cr <= cr1; cr++) attr_buff[cr * 32 + 1] &= 0xBF;
}

/* Port of the inner-border-line routine at LBE8B_2 ($BE99), adjusted to
 * the port's combined frame asset. The original clears y=0..21 before
 * it draws the top border, so those top-frame pixels are restored later.
 * Since paint_frame_to_buff() already includes the top border, the net
 * visible effect here starts at the lower three bands only. */
static void inner_border_line_c(void) {
    int pass;
    int y;
    static const int starts[3] = { 50, 106, 162 };
    for (pass = 0; pass < 3; pass++) {
        int start = starts[pass];
        for (y = start; y < start + 28; y++) {
            if (y < 0 || y >= PLAYFIELD_H) continue;
            scr_buff[y * 32 + 1]  &= 0x7F;   /* clear leftmost bit  -> x=8   black */
            scr_buff[y * 32 + 30] &= 0xFE;   /* clear rightmost bit -> x=247 black */
        }
    }
}

/* Port of print_border_shadow ($BFCF) — finalization after print_briks.
 * Clears bit 6 (the bright attr bit) at:
 *   - cc 1 of cr 1..23 (left dim strip column, all playfield rows)
 *   - cr 1 cc 2..30 (HUD label-row dimming)
 * Without this, bricks at lvl_col=0 (col_byte=1) leak their bright
 * brick color into cc 1, painting the left dim strip with the brick
 * colour instead of the non-bright side-strip attr the level expects
 * (e.g. L1 cr 7 cc 1: should be $1F, our print_briks_c writes $5F). */
static void print_border_shadow_c(void) {
    int cr;
    int cc;
    for (cr = 1; cr <= 23; cr++) {
        attr_buff[cr * 32 + 1] &= 0xBF;
    }
    for (cc = 2; cc <= 30; cc++) {
        attr_buff[1 * 32 + cc] &= 0xBF;
    }
}

/* Pre-fill scr_buff with the hex tile and attr_buff uniformly with
 * the level's bg_attr, across the WHOLE 256x192 playfield. This
 * replaces the prior direct-to-VGA paint_hex_bg path. buff_to_vga
 * does the final pixel expansion using the (possibly overwritten)
 * attr_buff. */
static void paint_bg_to_buff(unsigned char attr, unsigned char cycle) {
    const unsigned char *tile = bg_tile + (int)cycle * BG_TILE_SIZE;
    int y;
    for (y = 0; y < PLAYFIELD_H; y++) {
        int ty = y & 15;
        unsigned char t0 = tile[ty * 2];
        unsigned char t1 = tile[ty * 2 + 1];
        unsigned short pattern = t0 | ((unsigned short)t1 << 8);
        unsigned short *dest = (unsigned short *)&scr_buff[y * 32];
        int i;
        for (i = 0; i < 16; i++) {
            dest[i] = pattern;
        }
    }
    memset(attr_buff, attr, sizeof(attr_buff));
}

/* Single-pass buffer-to-VGA conversion: walk scr_buff bits and emit
 * each pixel via the surrounding char cell's attr_buff entry. Mirrors
 * the original game's final-frame paint (game_screen_draw_to_buffer
 * at $BE6B followed by buffer-to-screen copy). */
static void paint_bg_window_to_buff(unsigned char attr, unsigned char cycle,
                                    int y0, int h, int byte_lo, int byte_hi);

static unsigned char bg_scr_buff[6144];
static unsigned char bg_attr_buff[768];
static int static_bg_dirty = 1;
static int static_bg_cache_dirty = 0;
/* Dirty brick-row range [lo, hi] accumulated since the last band-cache
 * build, so the rebuild can re-composite only the changed rows instead of
 * the whole band. Default = whole band (a full rebuild). */
static int brick_dirty_lo = 0;
static int brick_dirty_hi = LVL_ROWS - 1;
static int force_full_flush = 1;

static unsigned long prev_score = 0xFFFFFFFFUL;
static unsigned long prev_high_score = 0xFFFFFFFFUL;
static int prev_lives = -1;

/* Set up the level's runtime magnet state — the state half of
 * print_magnets ($8D4C). Coordinates from magnets_per_level; the
 * initial ON/OFF coin is the original's per-magnet
 * `CALL random_generate / LD A,(random_number) / RRA / JR C,stay-ON`:
 * ADVANCES the RNG once per magnet, keeps the magnet ON when bit0==1.
 * (An earlier render-time coin sampled without advancing — so every
 * magnet on the level shared one coin — and drew OFF on bit0==1,
 * inverted. Now the coin is rolled once here, and render_magnets /
 * the toggle just read the state.)
 *
 * Test-mode pin (BATTYALL): slots 0/1 ON, 2/3 OFF, no RNG consumed —
 * keeps the state4 level-entry captures deterministic. */
static void magnet_level_init(unsigned char level_idx) {
    const unsigned char *rec;
    int i;
    magnet_count = 0;
    magnet_toggle_pending = 0xFF;
    for (i = 0; i < 3; i++) {
        ball_mag_cool[i] = 0;
        ball_mag_delta[i] = 0;
        ball_mag_exit[i] = 0;
        ball_mag_idx[i] = 0;
    }
    for (i = 0; i < MAGNETS_MAX_PER_LEVEL; i++) magnet_on_state[i] = 0;
    if (level_idx >= N_LEVELS) return;
    rec = magnets_per_level[level_idx];
    magnet_count = rec[0];
    for (i = 0; i < magnet_count; i++) {
        magnet_px[i] = rec[1 + 2*i];
        magnet_py[i] = rec[1 + 2*i + 1];
        magnet_on_state[i] = test_mode_pin_blink
            ? (unsigned char)(i < 2)
            : (unsigned char)(random_lo(next_random()) & 1);
    }
    /* Replay hook: force the initial ON/OFF pattern (low 4 bits = slots
     * 0..3) so the magnet physics tests get a deterministic field. */
    {
        const char *p = getenv("BATTY_REPLAY_MAGNET");
        if (p != NULL && *p != '\0') {
            char *endp;
            unsigned long v = strtoul(p, &endp, 16);
            if (*endp == '\0') {
                for (i = 0; i < magnet_count; i++)
                    magnet_on_state[i] = (unsigned char)((v >> i) & 1);
            }
        }
    }
}

/* Port of print_magnets ($8D4C), render half — paints each magnet from
 * the runtime state set by magnet_level_init. Inherits each cell's attr
 * — no override, same monochrome rule as other moving sprites. */
static void render_magnets(unsigned char level_idx) {
    int i;
    (void)level_idx;
    for (i = 0; i < magnet_count; i++) {
        int x = magnet_px[i];
        int y = magnet_py[i];
        /* Draw order matches the original's print_magnets ($8D4C):
         *   sprite_num $06 = spr_magnet_circle_ON (lightning, w=4, h=30
         *                    with SMC) — drawn UNCONDITIONALLY first.
         *   sprite_num $07 = spr_magnet_circle_OFF (bare outline, w=3,
         *                    h=23) — drawn CONDITIONALLY for an OFF slot.
         * (Iter 21 had this backwards, treating $06 as the "off state"
         * and $07 as the "on overlay"; gfx_screen_elements actually
         * maps $06 → spr_magnet_circle_on and $07 → spr_magnet_circle_off.)
         *
         * Both blits use the SAME (x, y) — original's `ADD A,$05` to
         * (IX+$04) between calls is dead state since ix_buf_addr_calc
         * only runs once. Note this means the ON sprite's bottom "spark"
         * rows (23..29) are painted under BOTH states and persist
         * regardless of later toggles, exactly like the original (the
         * toggle redraw is 23 rows tall — circle only). */
        blit_masked_to_scr_buff(spr_magnet_on, x, y);
        if (!magnet_on_state[i]) {
            blit_masked_to_scr_buff(spr_magnet_off, x, y);
        }
    }
}

/* Port of print_one_magnet ($8E72), called from the main-loop top when
 * the read-current `random_number+$01 == $99` gate fires (LB9E8_2,
 * ~1/256 per frame): pick a uniform random magnet (rejection over &3 —
 * each retry ADVANCES the RNG like the original's CALL random_generate),
 * flip its ON/OFF state, queue the sweep sound, and leave the circle
 * redraw for the next frame compose. Count==0 returns before any RNG
 * use (RET Z) — so non-magnet levels never perturb the RNG walk. */

static void magnet_random_toggle(void) {
    unsigned char a, b;
    if (magnet_count == 0) return;
    b = (unsigned char)(magnet_count - 1);
    do {
        a = (unsigned char)(random_lo(next_random()) & 0x03);
    } while (a > b);
    magnet_on_state[a] ^= 1;
    magnet_toggle_pending = a;
    sound_queue(SND_MAGNET);
}

/* Apply a pending toggle's incremental redraw — the visual half of
 * print_one_magnet + the restore window in restore_objs_and_magnet
 * ($987A: 4 chars x $17 rows at the paint origin). The toggle redraws
 * the CIRCLE only: sprite $07 (OFF) is natively 23 rows; for ON the
 * original's spr_magnet_circle_on height byte is $17=23 outside
 * print_magnets' temporary $1E SMC, so the bottom spark rows (23..29)
 * are never repainted (they persist from level paint regardless of
 * state). Must run while scr_buff holds clean background in the window
 * (frame-compose top, after the prev-dirty restore, before objects are
 * drawn) because the result is baked into the static bg cache. */
static void apply_magnet_toggle_visual(void) {
    static unsigned char spr_magnet_on_h23[242];
    unsigned char i = magnet_toggle_pending;
    int x, y, yy;
    int byte_lo, byte_hi;
    if (i == 0xFF) return;
    magnet_toggle_pending = 0xFF;
    if (i >= magnet_count) return;
    x = magnet_px[i];
    y = magnet_py[i];
    if (magnet_on_state[i]) {
        if (spr_magnet_on_h23[0] == 0) {
            memcpy(spr_magnet_on_h23, spr_magnet_on,
                        sizeof(spr_magnet_on_h23));
            spr_magnet_on_h23[1] = 0x17;     /* 30 -> 23 rows */
        }
        blit_masked_to_scr_buff(spr_magnet_on_h23, x, y);
    } else {
        blit_masked_to_scr_buff(spr_magnet_off, x, y);
    }
    /* Bake the window into the static bg cache (the magnet is part of
     * the cached background) and mark it for the VGA flush. 5 bytes
     * covers the 4-byte sprite at any sub-byte shift. */
    byte_lo = x >> 3;
    byte_hi = byte_lo + 4;
    if (byte_hi > 31) byte_hi = 31;
    for (yy = y; yy < y + 0x17 && yy < PLAYFIELD_H; yy++) {
        memcpy(&bg_scr_buff[(yy << 5) + byte_lo],
                    &scr_buff[(yy << 5) + byte_lo],
                    (unsigned int)(byte_hi - byte_lo + 1));
    }
    mark_dirty_bytes(y, 0x17, byte_lo, byte_hi);
}

static void render_hud_to_buff(void);

static void render_level_screen_static(unsigned char level_idx) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle   = (unsigned char)(level_idx & 3);
    paint_bg_to_buff(bg_attr, cycle);
    paint_frame_to_buff(cycle, level_idx);
    render_lives(cycle, bg_attr);
    render_hud_to_buff();
    render_magnets(level_idx);
    inner_border_line_c();
    render_brick_band(level_idx);
    restore_top_frame_center(cycle, level_idx);
}

static void build_static_background(unsigned char level_idx) {
    prof_static_rebuilds++;
    render_level_screen_static(level_idx);
    memcpy(bg_scr_buff, scr_buff, sizeof(bg_scr_buff));
    memcpy(bg_attr_buff, attr_buff, sizeof(bg_attr_buff));
    static_bg_cache_dirty = 0;
}

static void build_static_brick_band_cache(unsigned char level_idx) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle   = (unsigned char)(level_idx & 3);
    int y;
    int cr;
    int lo = brick_dirty_lo;
    int hi = brick_dirty_hi;
    unsigned short t0 = pit_current_ticks();
    unsigned short t1;

    if (force_full_band_rebuild || (lo <= 0 && hi >= LVL_ROWS - 1)) {
        /* Whole band dirty: the proven full path. */
        paint_bg_window_to_buff(bg_attr, cycle,
                                BRICK_BAND_Y_TOP,
                                BRICK_BAND_Y_BOT - BRICK_BAND_Y_TOP + 1,
                                1, 30);
        render_brick_band(level_idx);
        for (y = BRICK_BAND_Y_TOP; y <= BRICK_BAND_Y_BOT; y++) {
            memcpy(&bg_scr_buff[(y << 5) + 1], &scr_buff[(y << 5) + 1], 30);
        }
        for (cr = 3; cr <= 16; cr++) {
            memcpy(&bg_attr_buff[cr << 5], &attr_buff[cr << 5], 32);
        }
        /* The rebuild rewrote scr_buff/attr_buff well beyond the brick
         * flash's small dirty rect (whole rows, shadow attrs on the row
         * below, the 32-byte attr rows). Flush every pixel row of every
         * touched attr cell, or the parts outside the flash rect go stale
         * on VGA — the post-destroy leftovers of known-bugs.md #1. */
        mark_dirty_bytes(3 * 8, (16 - 3 + 1) * 8, 0, 31);
        prof_band_rows += 14;
    } else {
        /* Incremental: re-composite [R0, R1] = the dirty brick rows
         * widened by one row each side, so every attr/pixel the window
         * inherits from a neighbour row is re-derived rather than left
         * stale (see render_brick_band_rows' boundary notes). Pixel
         * window = the rows' bodies + R1's bottom-edge row; the shared
         * top-edge row (31 + 8*R0) is not bg-erased — print re-zeros it
         * only under live R0 bricks, which is the canonical content. */
        int R0  = (lo > 0) ? lo - 1 : 0;
        int R1  = (hi + 1 < LVL_ROWS) ? hi + 1 : LVL_ROWS - 1;
        int py0 = 32 + R0 * 8;
        int py1 = 40 + R1 * 8;
        int cr0 = 4 + R0;
        int cr1 = 5 + R1;
        paint_bg_window_to_buff(bg_attr, cycle, py0, py1 - py0 + 1, 1, 30);
        /* Re-draw the inner border line columns the bg repaint erased
         * (canonical order: the line is drawn before the bricks, which
         * then overwrite it — mirror inner_border_line_c's bands). */
        for (y = py0; y <= py1; y++) {
            if ((y >= 50 && y < 78) || (y >= 106 && y < 134)) {
                scr_buff[y * 32 + 1]  &= 0x7F;
                scr_buff[y * 32 + 30] &= 0xFE;
            }
        }
        render_brick_band_rows(level_idx, R0, R1, cr0, cr1);
        /* Capture from the shared top-edge row down (print touches it). */
        for (y = py0 - 1; y <= py1; y++) {
            memcpy(&bg_scr_buff[(y << 5) + 1], &scr_buff[(y << 5) + 1], 30);
        }
        for (cr = cr0; cr <= cr1; cr++) {
            memcpy(&bg_attr_buff[cr << 5], &attr_buff[cr << 5], 32);
        }
        /* Flush every pixel row of every recomposited attr cell, plus
         * the shared top-edge pixel row (same rule as the full branch). */
        mark_dirty_bytes(py0 - 1, (cr1 * 8 + 7) - (py0 - 1) + 1, 0, 31);
        prof_band_rows += (unsigned long)(cr1 - cr0 + 1);
    }
    t1 = pit_current_ticks();
    prof_band_pit += (t1 <= t0) ? (unsigned long)(t0 - t1)
                                : (unsigned long)((t0 - t1) + 23864u);
    prof_band_rebuilds++;
    static_bg_cache_dirty = 0;
}

static void mark_static_bg_cache_dirty(void) {
    /* Whole-band dirty (level entry, rocket clear, multi-cell changes). */
    brick_dirty_lo = 0;
    brick_dirty_hi = LVL_ROWS - 1;
    static_bg_cache_dirty = 1;
}

/* Mark a single brick row dirty, unioning into the pending range. Lets the
 * band-cache rebuild scope to just the rows a brick hit touched. */
static void mark_brick_row_dirty(int row) {
    if (!static_bg_cache_dirty) {
        brick_dirty_lo = row;
        brick_dirty_hi = row;
    } else {
        if (row < brick_dirty_lo) brick_dirty_lo = row;
        if (row > brick_dirty_hi) brick_dirty_hi = row;
    }
    static_bg_cache_dirty = 1;
}

static void restore_prev_dirty_from_static_cache(void) {
    int y;
    int cr;
    /* Restore only the byte ranges touched by moving sprites last
     * frame. Untouched rows and columns retain the cached static
     * background. */
    for (y = prev_dirty_y_lo; y <= prev_dirty_y_hi; y++) {
        int s;
        for (s = 0; s < DIRTY_SLOTS; s++) {
            if (prev_dirty_min_byte[s][y] != DIRTY_NONE) {
                unsigned char lo = prev_dirty_min_byte[s][y];
                unsigned char hi = prev_dirty_max_byte[s][y];
                memcpy(&scr_buff[(y << 5) + lo],
                            &bg_scr_buff[(y << 5) + lo],
                            (unsigned int)(hi - lo + 1));
            }
        }
    }
    for (cr = 0; cr < 24; cr++) {
        int byte_lo = 32;
        int byte_hi = -1;
        int r;
        for (r = 0; r < 8; r++) {
            int yy = (cr << 3) + r;
            int s;
            for (s = 0; s < DIRTY_SLOTS; s++) {
                if (prev_dirty_min_byte[s][yy] != DIRTY_NONE) {
                    if (prev_dirty_min_byte[s][yy] < byte_lo)
                        byte_lo = prev_dirty_min_byte[s][yy];
                    if (prev_dirty_max_byte[s][yy] > byte_hi)
                        byte_hi = prev_dirty_max_byte[s][yy];
                }
            }
        }
        if (byte_hi >= byte_lo) {
            memcpy(&attr_buff[(cr << 5) + byte_lo],
                        &bg_attr_buff[(cr << 5) + byte_lo],
                        (unsigned int)(byte_hi - byte_lo + 1));
        }
    }
}

static void restore_static_cache_rect_bytes(int y_start, int height,
                                            int byte_lo, int byte_hi) {
    int y;
    int cr;
    int y_end = y_start + height;
    if (byte_lo < 0) byte_lo = 0;
    if (byte_hi > 31) byte_hi = 31;
    if (byte_lo > byte_hi) return;
    if (y_start < 0) y_start = 0;
    if (y_end > PLAYFIELD_H) y_end = PLAYFIELD_H;
    if (y_start >= y_end) return;
    for (y = y_start; y < y_end; y++) {
        memcpy(&scr_buff[(y << 5) + byte_lo],
                    &bg_scr_buff[(y << 5) + byte_lo],
                    (unsigned int)(byte_hi - byte_lo + 1));
    }
    for (cr = y_start >> 3; cr <= (y_end - 1) >> 3; cr++) {
        memcpy(&attr_buff[(cr << 5) + byte_lo],
                    &bg_attr_buff[(cr << 5) + byte_lo],
                    (unsigned int)(byte_hi - byte_lo + 1));
    }
}

static void update_static_hud_top(unsigned char level_idx) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle   = (unsigned char)(level_idx & 3);
    int y;
    int cr;
    (void)bg_attr;
    paint_frame_to_buff(cycle, level_idx);
    render_hud_to_buff();
    restore_top_frame_center(cycle, level_idx);
    for (y = 0; y < FRAME_TOP_H_PX; y++) {
        memcpy(&bg_scr_buff[y << 5], &scr_buff[y << 5], 32);
    }
    for (cr = 0; cr < FRAME_TOP_H_PX / 8; cr++) {
        memcpy(&bg_attr_buff[cr << 5], &attr_buff[cr << 5], 32);
    }
}

static void render_level_screen(unsigned char level_idx) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle   = (unsigned char)(level_idx & 3);

    static_bg_dirty = 1;
    static_bg_cache_dirty = 0;
    clear_dirty_ranges(prev_dirty_min_byte, prev_dirty_max_byte);
    prev_score = 0xFFFFFFFFUL;
    prev_high_score = 0xFFFFFFFFUL;
    prev_lives = -1;

    fill(0, 0, SCREEN_W, SCREEN_H, COL_BORDER);
    draw_frame(10);              /* bright red — placeholder */
    paint_bg_to_buff(bg_attr, cycle);
    paint_frame_to_buff(cycle, level_idx);
    render_bat(cycle, bg_attr);
    render_lives(cycle, bg_attr);
    remember_bat_draw_state();
    render_hud_to_buff();
    /* Original LBE8B_11 draws score labels/digits immediately before
     * magnets and bricks. This matters on levels whose magnets overlap
     * HUD rows: magnets can overwrite the score area, not vice versa.
     *
     * Magnets blit BEFORE bricks (original at $BE8B does
     * CALL print_magnets; CALL print_briks). The brick top row
     * overwrites the magnet's lower shadow rows where they overlap;
     * inverting the order leaves the shadow rows punching through
     * the brick top. */
    render_magnets(level_idx);
    inner_border_line_c();
    render_brick_band(level_idx);
    restore_top_frame_center(cycle, level_idx);
    render_brick_flash_to_buff();
    render_brick_hit_anim_to_buff();
    buff_to_vga();
}


static unsigned long bios_ticks(void);

/* Currently selected game mode. 1, 2, 3 = "1 PLAYER" / "2 PLAYERS" /
 * "DOUBLE PLAY" — option line at the corresponding Y blinks when set.
 * Boot default is 1: in the original game "1 PLAYER" is already
 * selected on menu entry and starts blinking before any key is
 * pressed (snap2's selected-row attrs are 0x00 = the BLACK half of
 * that initial blink). */
static unsigned char selected_mode = 1;

static int option_y_pix_for_mode(unsigned char mode) {
    switch (mode) {
        case 1: return 0x2F;     /* "1 - 1 PLAYER"    */
        case 2: return 0x3F;     /* "2 - 2 PLAYERS"   */
        case 3: return 0x4F;     /* "3 - DOUBLE PLAY" */
        default: return -1;
    }
}

/* sub_961c writes a fixed 11 attribute bytes at cols 14..24 — that's
 * wider than "1 PLAYER" / "2 PLAYERS" payload, just covering the
 * full "DOUBLE PLAY" extent. The off-text cells were already black
 * paper so the over-blank is invisible; we mirror the same constant. */
#define BLINK_CELLS  11

static int blink_phase(void);

/* Selected option's TEXT portion (payload indices 4+, after "N - ")
 * strobes WHITE ↔ INVISIBLE. Original mechanism (sub_961c at 0x961C):
 * always writes one of 11 attribute bytes to screen-attr area 0x5800+
 * for the option's row, picked from the 16-entry table at 0x9643 =
 * {0x00 ×8, 0x47 ×8}. Half the cycle the cells are attr=0x00 (black
 * ink on black paper = invisible), the other half attr=0x47 (bright
 * white). The default green from the markup attr is OVERWRITTEN every
 * frame — never visible.
 *
 * We emulate that on VGA: always black-out the text cells (= erase
 * the green pixels the markup just drew); on the WHITE phase also
 * re-draw the glyphs in bright white. */
static void apply_option_blink(void) {
    int y_pix = option_y_pix_for_mode(selected_mode);
    int y, p, i, count, x_text;
    unsigned char marker, c;
    if (y_pix < 0) return;
    p = 0;
    while (p < markup_len) {
        if (!is_row_marker(markup[p])) { p++; continue; }
        if (markup[p + 1] != (unsigned char)y_pix) {
            p += 4 + markup[p + 3]; continue;
        }
        marker = markup[p];
        count  = markup[p + 3];
        x_text = BORDER_X + (int)(marker / 8) * 8 + 4 * 8;
        y      = BORDER_Y + y_pix - 5;
        /* Erase the green pixels the markup just painted across all
         * 11 cells the original would have attr=0x00'd. */
        fill(x_text, y, BLINK_CELLS * 8, FONT_ROWS, 0);
        /* WHITE phase: redraw glyphs from index 4+ in bright white. */
        if (blink_phase()) {
            for (i = 4; i < count; i++) {
                c = markup[p + 4 + i];
                if (c != 0x26 && c <= 0x2A) {
                    draw_glyph(x_text + (i - 4) * 8, y, 15, c);
                }
            }
        }
        return;
    }
}

static void render_menu_screen(void) {
    fill(0, 0, SCREEN_W, SCREEN_H, COL_BORDER);
    draw_frame(11);              /* bright magenta */
    render_markup();
    draw_player_indicators();
    draw_bottom_sprites();
    apply_option_blink();
}

/* BIOS tick counter @ ~18.2 Hz. INT 1Ah AH=0 returns CX:DX. One unit
 * ≈ 55 ms — coarse but plenty for attract-mode timing. */
static unsigned long bios_ticks(void) {
    union REGS r;
    r.h.ah = 0;
    int386(0x1A, &r, &r);
    return ((unsigned long)r.w.cx << 16) | r.w.dx;
}

/* --- 50 Hz timer harness (M4) ------------------------------------------
 *
 * Reprograms PIT timer 0 to fire at 1.193182 MHz / 23864 = ~50 Hz and
 * installs a __interrupt handler that increments pit_frame_counter.
 * To keep the BIOS time-of-day at $0040:$006C ticking at 18.2 Hz, we
 * chain the original INT 8 on every 16-bit overflow of an accumulator
 * we advance by 23864 per tick (since 65536 / 23864 ~= 2.747, chaining
 * fires every 3rd tick on average, matching the BIOS rate).
 *
 * When chained, the BIOS handler acks the IRQ (sends EOI to the PIC);
 * when not chained, we send the EOI ourselves. */
#define PIT_DIV_50HZ 23864
static void (__interrupt *prev_int8)(void) = NULL;
static volatile unsigned int  bios_acc          = 0;

static void __interrupt new_int8(void) {
    unsigned int old;
    pit_frame_counter++;
    old = bios_acc;
    bios_acc += PIT_DIV_50HZ;
    if (bios_acc < old) {
        /* 16-bit overflow -> chain BIOS so its tick counter + EOI fire. */
        _chain_intr(prev_int8);
        /* _chain_intr does not return. */
    }
    outp(0x20, 0x20);   /* EOI: end-of-interrupt for IRQ 0 */
}

static void timer_install(void) {
    if (prev_int8) return;
    prev_int8 = _dos_getvect(0x08);
    _disable();
    /* PIT control word: counter 0, lo+hi byte, mode 3 (square wave), binary. */
    outp(0x43, 0x36);
    outp(0x40, PIT_DIV_50HZ & 0xFF);
    outp(0x40, (PIT_DIV_50HZ >> 8) & 0xFF);
    _dos_setvect(0x08, new_int8);
    _enable();
}

static void timer_restore(void) {
    if (!prev_int8) return;
    _disable();
    /* Silence the speaker so DOS doesn't return to a buzzing prompt. */
    outp(0x61, (unsigned char)(inp(0x61) & 0xFC));
    /* Restore default 18.2 Hz (divisor 0 = 65536). */
    outp(0x43, 0x36);
    outp(0x40, 0);
    outp(0x40, 0);
    _dos_setvect(0x08, prev_int8);
    _enable();
    prev_int8 = NULL;
}

/* Atomic read of the 32-bit frame counter from main code. */
static unsigned long pit_ticks(void) {
    unsigned long v;
    _disable();
    v = pit_frame_counter;
    _enable();
    return v;
}

/* --- Keyboard polling (INT 9 hook) -------------------------------------
 *
 * The original game polls keyboard state every frame
 * (get_left_player_ctrl_state at $A161 / get_right_player_ctrl_state
 * at $A19E). The Z80 reads the keyboard half-rows via IN A,($FE) and
 * builds the ctrl_btns_pressed byte.
 *
 * On a PC we install a __interrupt handler on INT 9 (= IRQ 1) that
 * latches each scan code into key_state[] before chaining to BIOS,
 * so BIOS's standard keyboard buffer still works for getch (= our
 * menu navigation) while gameplay can read the live held state at
 * 50 Hz.
 *
 * Scan codes follow the PC AT set 1 (= XT-compatible). Extended
 * keys (gray arrows) send an 0xE0 prefix before the regular code -
 * we ignore the prefix and just track the resulting code so the
 * keypad arrows and the gray arrows both work. */
static void (__interrupt *prev_int9)(void) = NULL;
static volatile unsigned char key_state[128];

#define SC_ESC      0x01
#define SC_P        0x19
#define SC_ENTER    0x1C
#define SC_SPACE    0x39
#define SC_LEFT     0x4B    /* arrow / keypad 4 */
#define SC_RIGHT    0x4D    /* arrow / keypad 6 */

static void __interrupt new_int9(void) {
    unsigned char sc = (unsigned char)inp(0x60);
    if (sc != 0xE0) {                       /* skip the extended-key prefix */
        if (sc & 0x80) key_state[sc & 0x7F] = 0;
        else           key_state[sc & 0x7F] = 1;
    }
    _chain_intr(prev_int9);                 /* BIOS reads 0x60 again + EOIs */
}

static void kbd_install(void) {
    if (prev_int9) return;
    prev_int9 = _dos_getvect(0x09);
    _dos_setvect(0x09, new_int9);
}

static void kbd_restore(void) {
    if (!prev_int9) return;
    _dos_setvect(0x09, prev_int9);
    prev_int9 = NULL;
}


/* Attract-mode state machine — same shape as the original game:
 *   TITLE   one-shot loading screen on boot, timeout -> MENU
 *   MENU    interactive (A/B cycle device, any other key advances),
 *           idle timeout -> HI_SCORE
 *   HI_SCORE static-ish, any key or timeout -> MENU
 *   ESC quits anywhere. */
typedef enum { ST_TITLE, ST_MENU, ST_HISCORE, ST_LEVEL, ST_QUIT } state_t;

#define TITLE_TIMEOUT_TICKS   60    /* ~3.3 s */
#define MENU_TIMEOUT_TICKS   200    /* ~11 s  */
#define HISCORE_TIMEOUT_TICKS 120   /* ~6.6 s */
#define LEVEL_TIMEOUT_TICKS   40    /* ~2.2 s per level in the cycle */

/* Attract-mode auto-cycle through TITLE / MENU / HISCORE states for
 * the no-input demo loop. Defaults to OFF — matches the original
 * game, where the player drives transitions. The legacy BATTYALL=1
 * env-var still forces it OFF for the test floppy (no-op now, but
 * kept so the autoexec.bat doesn't need editing). */
static int auto_advance = 0;
#define TIMED_OUT(start, ticks) (auto_advance && (bios_ticks() - (start) > (ticks)))

/* Blink phase for the selected option's text on the MENU. Test mode
 * pins it to 0 (BLACK / invisible) so the screendump matches snap2's
 * captured BLACK half deterministically. `make run` uses real-time
 * bios_ticks so the user sees the actual blink. */
static int blink_phase(void) {
    if (test_mode_pin_blink) return 0;
    return (int)((bios_ticks() >> 1) & 1);   /* ~4.5 Hz half-period */
}
static void render_hiscore_screen(void) {
    asset_load_variable("MARKUP.BIN", markup, MARKUP_MAX, &markup_len);
    fill(0, 0, SCREEN_W, SCREEN_H, COL_BORDER);
    draw_frame(10);                  /* bright red */
    render_markup();
}

static state_t run_title(void) {
    unsigned long start;
    show("LOADING.BIN");
    start = bios_ticks();
    for (;;) {
        if (kbhit()) {
            int k = getch();
            return (k == 27) ? ST_QUIT : ST_MENU;
        }
        if (TIMED_OUT(start, TITLE_TIMEOUT_TICKS)) return ST_MENU;
    }
}

static state_t run_menu(void) {
    unsigned long last_input;
    int last_blink_phase = -1;
    asset_load_variable("MENUMARK.BIN", markup, MARKUP_MAX, &markup_len);
    render_menu_screen();
    last_input = bios_ticks();
    for (;;) {
        if (kbhit()) {
            int k = getch();
            if (k == 27) return ST_QUIT;
            if (k == 'a' || k == 'A') {
                p1_dev = (p1_dev + 1) & 3;
                render_menu_screen();
                last_input = bios_ticks();
                continue;
            }
            if (k == 'b' || k == 'B') {
                p2_dev = (p2_dev + 1) & 3;
                render_menu_screen();
                last_input = bios_ticks();
                continue;
            }
            if (k >= '1' && k <= '3') {
                selected_mode = (unsigned char)(k - '0');
                render_menu_screen();
                last_input = bios_ticks();
                last_blink_phase = -1;       /* force redraw next phase tick */
                continue;
            }
            /* 0 / ENTER / other — would start a game; advance for now. */
            return ST_HISCORE;
        }
        /* Re-render when blink phase flips. Shares blink_phase() with
         * apply_option_blink so we always render at the right phase. */
        if (selected_mode != 0) {
            int phase = blink_phase();
            if (phase != last_blink_phase) {
                render_menu_screen();
                last_blink_phase = phase;
            }
        }
        if (TIMED_OUT(last_input, MENU_TIMEOUT_TICKS)) return ST_HISCORE;
    }
}

static state_t run_hiscore(void) {
    unsigned long start;
    render_hiscore_screen();
    start = bios_ticks();
    for (;;) {
        if (kbhit()) {
            int k = getch();
            return (k == 27) ? ST_QUIT : ST_LEVEL;
        }
        if (TIMED_OUT(start, HISCORE_TIMEOUT_TICKS)) return ST_LEVEL;
    }
}

/* Restore a byte-column window of scr_buff to the level's hex bg tile
 * and attr_buff to bg_attr. Used to wipe a sprite's previous position
 * before re-blitting. */
static void paint_bg_window_to_buff(unsigned char attr, unsigned char cycle,
                                    int y0, int h, int byte_lo, int byte_hi) {
    const unsigned char *tile = bg_tile + (int)cycle * BG_TILE_SIZE;
    int y, char_row;
    int y_end = y0 + h;
    if (byte_lo < 0) byte_lo = 0;
    if (byte_hi > 31) byte_hi = 31;
    if (y0 < 0) y0 = 0;
    if (y_end > PLAYFIELD_H) y_end = PLAYFIELD_H;
    if (byte_lo > byte_hi || y0 >= y_end) return;
    for (y = y0; y < y_end; y++) {
        int ty = y & 15;
        unsigned char t0 = tile[ty * 2];
        unsigned char t1 = tile[ty * 2 + 1];
        int byte_col;
        for (byte_col = byte_lo; byte_col <= byte_hi; byte_col++) {
            scr_buff[y * 32 + byte_col] = (byte_col & 1) ? t1 : t0;
        }
    }
    {
        int char_row_lo = y0 / 8;
        int char_row_hi = (y_end - 1) / 8;
        for (char_row = char_row_lo; char_row <= char_row_hi && char_row < ATTR_ROWS; char_row++) {
            memset(&attr_buff[char_row * 32 + byte_lo], attr,
                   (unsigned int)(byte_hi - byte_lo + 1));
        }
    }
}

/* DOS extended-key scancodes (after a leading 0 byte from getch). */
#define KEY_EXT_PREFIX 0
#define KEY_LEFT  75
#define KEY_RIGHT 77
#define KEY_ENTER 13
#define KEY_ESC   27
#define KEY_SPACE 32
#define KEY_P_LOWER 'p'
#define KEY_P_UPPER 'P'

/* Paint the ball at (x, y) into scr_buff via the original masked
 * OR-blit. Cell attrs are left at bg_attr (we don't override), so the
 * surrounding bg pattern inside the ball's char cells stays identical
 * to neighbouring cells — no colour-clash halo around the ball. The
 * ball's solid body bits (mask=1, pix=0) render as bg ink (yellow on
 * L1) and the texture/shadow bits (mask=1, pix=1) render as bg paper
 * (black) — the same effect the original ZX game produces. */
static void render_ball_to_buff(int x, int y, unsigned char bg) {
    unsigned int spr = big_ball_active() ? SPR_BIG_BALL : SPR_BALL_NORMAL;
    (void)bg;
    blit_masked_to_scr_buff(spr, x, y);
}

/* Paint each active laser bullet via the original masked OR-blit, no
 * per-cell attr override. The original's print_obj_to_buff writes
 * pixels only — the bullet renders in whichever attr each cell
 * already has (= bg attr in the empty playfield). User's spec: the
 * game field is monochrome except blocks. Two slots = up to two
 * bullets in flight. */
static void render_bullet_to_buff(void) {
    static unsigned char bullet_anim_tick = 0;
    unsigned int spr;
    int i;
    bullet_anim_tick++;
    spr = (bullet_anim_tick & 1) ? SPR_BULLET_2 : SPR_BULLET_1;
    for (i = 0; i < N_BULLETS; i++) {
        if (!bullet_active[i]) continue;
        blit_masked_to_scr_buff(spr, bullet_x[i], bullet_y[i]);
    }
}

/* Paint the rocket into scr_buff. Alternates between the two
 * spr_bonus_rocket_* frames based on a per-tick counter for a
 * crude flame-flicker effect. No per-cell attr override — same
 * "monochrome except blocks" rule as bullets / bonus. */
static void render_rocket_to_buff(void) {
    unsigned int spr;
    if (!rocket_active) return;
    /* Original handling_rocket at \$A89A toggles sprite each frame:
     *   LD A,(counter_misc); AND \$01; LD (IX+\$01),A
     * Was masking \& 2 which only flipped every 2 ticks — half the
     * original's flame flicker rate. */
    spr = (rocket_counter & 1) ? SPR_BONUS_ROCKET_2 : SPR_BONUS_ROCKET_1;
    blit_masked_to_scr_buff(spr, rocket_x, rocket_y);
}

static unsigned int current_rocket_spr(void) {
    return (rocket_counter & 1) ? SPR_BONUS_ROCKET_2 : SPR_BONUS_ROCKET_1;
}

/* Direct-VGA fallback kept for the bat-only-move path (redraw_bat
 * extra slot at line 2697 below), which only refreshes the bat strip
 * and can't carry over the ball's scr_buff write. */
static void render_ball(int x, int y, unsigned char attr) {
    unsigned int spr = big_ball_active() ? SPR_BIG_BALL : SPR_BALL_NORMAL;
    blit_masked_sprite(spr, x, y, attr_ink(attr), attr_paper(attr));
}

/* Map our BONUS_TYPE_* enum to the original spr_bonus_* sprite offset. */
static unsigned int spr_for_bonus(unsigned char t) {
    switch (t) {
        case BONUS_TYPE_LIFE:        return SPR_BONUS_EXTRA_LIFE;
        case BONUS_TYPE_SLOW:        return SPR_BONUS_SLOW;
        case BONUS_TYPE_BIG_BAT:     return SPR_BONUS_SIZE;
        case BONUS_TYPE_BIG_BALL:    return SPR_BONUS_SMASH;
        case BONUS_TYPE_KILL_ALIENS: return SPR_BONUS_KILL_ALIENS;
        case BONUS_TYPE_CATCH:       return SPR_BONUS_HAND;
        case BONUS_TYPE_ROCKET:      return SPR_BONUS_ROCKET_1;
        case BONUS_TYPE_SCORE_5K:    return SPR_BONUS_5000_POINTS;
        case BONUS_TYPE_LASER:       return SPR_BONUS_GUN;
        case BONUS_TYPE_MULTI_BALL:  return SPR_BONUS_TRIPLE_BALL;
        default:                     return SPR_BONUS_SIZE;
    }
}

/* Paint the bonus into scr_buff only. The original set_bonus selects an
 * entry in gfx_bonuses, and print_obj_to_buff writes only object pixels;
 * it does not call print_sprite_attrib for falling bonuses. Colours must
 * therefore come from the existing attr_buff cells underneath. */
static void render_bonus_to_buff(unsigned char bg) {
    unsigned int spr = spr_for_bonus(bonus_type);
    (void)bg;
    blit_masked_to_scr_buff(spr, bonus_x, bonus_y);
}

static void set_rocket_bonus_sprite_height(unsigned char height) {
    /* Original startup/all_var_init stores $0C into spr_bonus_rocket_1+1
     * so the falling "next level" bonus is just the compact rocket pack.
     * get_rocket patches the same byte to $1B when it attaches to the
     * bat for the level-clear flight. */
    sprites_blob[SPR_BONUS_ROCKET_1 + 1] = height;
}

/* Map our BONUS_TYPE_* back to the original $00..$09 code used by
 * bat.bonus_applied. Inverse of map_orig_to_our_bonus. */

/* Apply the effect that comes with `type`. Catching the same type
 * while already active extends the duration. */
static void bonus_apply(unsigned char type) {
    /* Original get_bonus at $A67B: every catch awards 400 points and
     * plays a sound — sound_live_add ($07) for the LIFE bonus, the
     * resize-2 beep ($0C) for everything else (push_resize_sound at
     * $A645, gated by `CP $05; CALL NZ,push_resize_sound`). Our port
     * had been routing every catch through SND_LIVE_ADD. */
    sound_queue(type == BONUS_TYPE_LIFE ? SND_LIVE_ADD : SND_BAT_RESIZE_2);
    /* Original LA67B_3 at \$A6FC writes the bonus type code into
     * bat.bonus_applied for every catch except ROCKET (which jumps
     * out earlier to get_rocket). Catching a new bonus thus REPLACES
     * any previous bat-side effect — e.g. catching BIG_BAT after
     * LASER clears the LASER state. */
    if (type != BONUS_TYPE_ROCKET) {
        unsigned char orig_code = bonus_to_original(type);
        objects[OBJ_BAT_1].bonus_applied = orig_code;
        objects[OBJ_BAT_2].bonus_applied = orig_code;
    }
    switch (type) {
        case BONUS_TYPE_LIFE:     lives++; life_dropped_this_round = 1; break;
        case BONUS_TYPE_SLOW:
            /* Original at line 3108 (SLOW path): `LD (IY+\$14),\$FF` —
             * overwrites bat.bonus_applied to \$FF (= no bat-side state)
             * after the universal assignment. SLOW is ball-side; the
             * bat doesn't track it. */
            objects[OBJ_BAT_1].bonus_applied = 0xFF;
            objects[OBJ_BAT_2].bonus_applied = 0xFF;
            /* Original LA67B_7: SLOW sets ALL ball speeds to $02 (the
             * minimum) — `LD A,$02; LD (object_ball_1+$07),A` etc. It
             * does NOT reset the speed-up ramp counter, so the ball
             * starts climbing back toward $06 from the next $94 tick
             * (SLOW wears off). */
            objects[OBJ_BALL_1].speed = BALL_SPEED;
            objects[OBJ_BALL_2].speed = BALL_SPEED;
            objects[OBJ_BALL_3].speed = BALL_SPEED;
            break;
        case BONUS_TYPE_BIG_BAT:  big_bat_ticks  = BIG_BAT_DURATION;
                                  bat_extra_tgt  = BAT_BIG_EXTRA_PX;
                                  sound_queue(SND_BAT_RESIZE_1);
                                  break;
        case BONUS_TYPE_BIG_BALL: big_ball_ticks = BIG_BALL_DURATION; break;
        case BONUS_TYPE_KILL_ALIENS:
            /* bat.bonus_applied = \$09 has already been set above —
             * enemy_prepare reads that to skip further alien spawns.
             * Also clear any currently active alien for immediate
             * visible effect. */
            {
                Object *e = &objects[OBJ_ENEMY];
                if ((e->sprite_set & 0x7F) != 0
                    && !(e->sprite_set & 0x80)
                    && (e->sprite_set & 0x7F) != 0x0A) {
                    /* Centre 16x13 blast over alien (mirror of \$A4D2). */
                    e->x_coord = (unsigned char)(e->x_coord + (int)e->w_body_px / 2 - 8);
                    e->y_coord = (unsigned char)(e->y_coord + 4);
                    e->w_body_px = 16;
                    e->h_body_px = 13;
                    e->sprite_set = 0x0A;
                    e->sprite_num = 0;
                    e->misc_12 = 0x50;   /* kill_enemy $A4C4 seed */
                    score += 350;
                    sound_queue(SND_ALIEN_BLAST);
                }
            }
            break;
        case BONUS_TYPE_CATCH:
            /* bat.bonus_applied = \$03 has already been set above —
             * step_ball reads it on bat-bounce to decide whether to
             * stick the ball. */
            break;
        case BONUS_TYPE_ROCKET:
            /* Spawn a rocket flying up from the bat. step_rocket
             * destroys every destructible cell it passes through,
             * giving the player a visible "rocket cleared the level"
             * moment instead of the level just dissolving. */
            if (!rocket_active) {
                rocket_active = 1;
                rocket_clear_completed = 0;
                set_rocket_bonus_sprite_height(ROCKET_H_PX);
                /* Original LBAED_6 hides every object while the rocket
                 * clear loop runs, then keeps the caught bat + rocket
                 * alive. Mirror the visible result: the ball, bullets,
                 * alien, bomb, and any marker vanish for the rocket
                 * sequence instead of continuing to play underneath. */
                BALL_HIDE();
                ball_stuck = 0;
                ball2_active = 0;
                ball3_active = 0;
                objects[OBJ_BALL_2].sprite_set = 0x82;
                objects[OBJ_BALL_3].sprite_set = 0x82;
                bomb_active = 0;
                bullet_active[0] = 0;
                bullet_active[1] = 0;
                bullet_blast_ticks[0] = 0;
                bullet_blast_ticks[1] = 0;
                pts_400_active = 0;
                objects[OBJ_ENEMY].sprite_set = 0;
                /* Original get_rocket at $AA9D:
                 *   rocket_x = bat_x + 4 (normal) or +12 (big)
                 *   rocket_y = bat_y + 6 (inside the bat body)
                 * Both put the rocket on the bat's left half emerging
                 * up from inside it. Our sprite is masked so the bat
                 * pixels stay visible through the transparent regions. */
                rocket_x = BAT_X + 4;
                if (bat_extra_px >= BAT_BIG_EXTRA_PX) rocket_x += 8;
                rocket_y = BAT_Y + 6;
                rocket_acc = 0;
                rocket_frac = 0;
                rocket_counter = 0;
                /* No catch sound — get_rocket at $AA9D pushes none. */
                /* INC (IY+\$14) at $AA9D:\$AA72: ROCKET catch bumps
                 * bat.bonus_applied by 1, which silently cancels any
                 * prior bat-side bonus (e.g. CATCH \$03 → \$04 = no
                 * effect; LASER \$01 → \$02 = no effect). The
                 * universal bonus_apply set we did at the top of this
                 * function intentionally skips ROCKET, so we apply
                 * the INC here instead. */
                objects[OBJ_BAT_1].bonus_applied++;
                objects[OBJ_BAT_2].bonus_applied++;
            }
            break;
        case BONUS_TYPE_SCORE_5K:
            /* Pure score bonus. 5000 in BCD-equivalent decimal. */
            score += 5000;
            break;
        case BONUS_TYPE_LASER:
            /* bat.bonus_applied = \$01 has already been set above —
             * the inner-loop SPACE handler reads it to enable laser
             * fires. Cleared automatically when another bonus is
             * caught (since bat.bonus_applied is rewritten). */
            break;
        case BONUS_TYPE_MULTI_BALL:
            /* Spawn two extra balls at the primary's current position
             * for a 3-ball total (port of LA67B_8 / "triple ball" at
             * $A67B). Directions come from the original low-nibble
             * split: primary d=$04 -> extras $0C/$08, primary d=$08
             * -> $0C/$04, otherwise -> $08/$04, preserving quadrant. */
            /* Original at LA67B_8 (\$3074): `LD (IY+\$14),\$FF` after
             * setting balls_quantity = 3 — overwrites bat.bonus_applied
             * to \$FF (TRIPLE_BALL is ball-side, not bat-side). */
            objects[OBJ_BAT_1].bonus_applied = 0xFF;
            objects[OBJ_BAT_2].bonus_applied = 0xFF;
            if (!ball2_active && !ball3_active) {
                unsigned char base_dir = delta_to_dir(ball_dx, ball_dy);
                unsigned char q = (unsigned char)(base_dir & 0x30);
                unsigned char d = (unsigned char)(base_dir & 0x0F);
                unsigned char ball2_dir, ball3_dir;
                if (d == 0x04) {
                    ball2_dir = (unsigned char)(q | 0x0C);
                    ball3_dir = (unsigned char)(q | 0x08);
                } else if (d == 0x08) {
                    ball2_dir = (unsigned char)(q | 0x0C);
                    ball3_dir = (unsigned char)(q | 0x04);
                } else {
                    ball2_dir = (unsigned char)(q | 0x08);
                    ball3_dir = (unsigned char)(q | 0x04);
                }
                ball2_active = 1;
                objects[OBJ_BALL_2].sprite_set = 0x02;
                objects[OBJ_BALL_2].x_coord = BALL_X;
                objects[OBJ_BALL_2].y_coord = BALL_Y;
                /* Unified extras read dir/speed/q8.8 from the object (the
                 * original copies ball_1's speed + a derived dir). */
                objects[OBJ_BALL_2].dir = ball2_dir;
                objects[OBJ_BALL_2].speed = objects[OBJ_BALL_1].speed;
                objects[OBJ_BALL_2].x_coord_hi = 0;
                objects[OBJ_BALL_2].y_coord_hi = 0;
                dir_to_delta(ball2_dir, &ball2_dx, &ball2_dy);
                ball3_active = 1;
                objects[OBJ_BALL_3].sprite_set = 0x02;
                objects[OBJ_BALL_3].x_coord = BALL_X;
                objects[OBJ_BALL_3].y_coord = BALL_Y;
                objects[OBJ_BALL_3].dir = ball3_dir;
                objects[OBJ_BALL_3].speed = objects[OBJ_BALL_1].speed;
                objects[OBJ_BALL_3].x_coord_hi = 0;
                objects[OBJ_BALL_3].y_coord_hi = 0;
                dir_to_delta(ball3_dir, &ball3_dx, &ball3_dy);
                sound_queue(SND_TRIPLE_BALL);
            }
            break;
        default: break;
    }
}

/* Current effective bat geometry (varies with big_bat_ticks). */
/* spr_bat_big is 48 px wide (6 bytes) vs spr_bat_normal's 32 px (4
 * bytes). Keep the bat visually centred on BAT_X by rendering big
 * bat 8 px further left; hitbox widens correspondingly. */
/* Collision uses the bat BODY (28 px wide per object_bat_1's
 * w_body_px = \$1C), not the full sprite (32 px = body + shadow).
 * The sprite's last 4 px (rows 2+ have mask \$F0 in byte 3) are
 * transparent shadow, not visible bat surface — ball passing through
 * those pixels shouldn't register a hit. */
/* BAT_BODY_W comes from physics.h. */
static int eff_bat_left(void)  { return BAT_X - bat_extra_px; }
static int eff_bat_right(void) { return BAT_X + BAT_BODY_W + bat_extra_px; }

/* Current effective ball body size. spr_ball_normal body is 8x7;
 * spr_big_ball body fills the full 2-byte * 12 row sprite at its
 * widest = ~12 px in the middle rows. We approximate as 12. */
/* SMASH (BIG_BALL) is active in the original iff bat.bonus_applied == \$07
 * (see line 919-920 at \$0397: `CP \$07; JR NZ,obj_processing`). Catching
 * another bonus rewrites bat.bonus_applied and the ball reverts on the
 * very next frame. We add a timer (~10 s, mirrors smash_counter wrap at
 * \$F8) as an OR with the bat state so the effect ends either way. */
static int big_ball_active(void) {
    return big_ball_ticks > 0
        && objects[OBJ_BAT_1].bonus_applied == 0x07;
}
/* BIG_BAT is active iff bat.bonus_applied == \$00 in the original — the
 * bat-resize state machine in handling_bat_no_transform reads the byte
 * each frame. Catching another bonus immediately ends the wide-bat
 * state via the bat_extra_tgt = 0 target below in step_bonus. */
static int big_bat_active(void) {
    return big_bat_ticks > 0
        && objects[OBJ_BAT_1].bonus_applied == 0x00;
}
/* Collision body stays 8x7 even with BIG_BALL active — original at
 * $9D5A_1 sets bat.bonus_applied=$07 and swaps the sprite to
 * spr_big_ball, but never touches the ball's (IX+$0C, IX+$0D) body
 * dimensions. The bigger sprite is purely cosmetic; the hitbox the
 * brick / bat / wall collision uses is the same as the normal ball.
 * Earlier port grew the collision to match the sprite (12 px),
 * making it artificially easier to catch / hit during SMASH. */
static int eff_ball_size(void) { return BALL_W_PX; }

/* Advance the falling bonus, check for catch on the bat, and tick down
 * any active effect timers. */
static void step_bonus(void) {
    int bat_left, bat_right;
    if (big_bat_ticks > 0) {
        big_bat_ticks--;
        if (big_bat_ticks == 0 || !big_bat_active()) {
            /* Timer expired OR bat.bonus_applied was changed by
             * another catch — either way, target the shrink. The
             * original's bat_decrease_size at \$9DE0 runs silently;
             * the SND_BAT_RESIZE_2 cue plays from push_resize_sound
             * at the bonus catch that replaced BIG_BAT, not from
             * the shrink animation itself. */
            bat_extra_tgt = 0;
            big_bat_ticks = 0;                        /* keep the two in sync */
        }
    }
    if (big_ball_ticks > 0 && ((pit_ticks() & 1UL) == 0)) {
        big_ball_ticks--;
        if (big_ball_ticks == 0
            && objects[OBJ_BAT_1].bonus_applied == 0x07) {
            /* Port of the smash_counter \$F8 expire at \$03B0: clear
             * bat.bonus_applied to \$FF so future BIG_BALL bonus drops
             * aren't blocked by the duplicate-exclusion check. */
            objects[OBJ_BAT_1].bonus_applied = 0xFF;
            objects[OBJ_BAT_2].bonus_applied = 0xFF;
        }
    }
    /* Animate bat width toward target — gated every other tick.
     * Original bat_resize at \$9DE0 combines counter_misc bit 0 +
     * bit 1 over a 4-frame cycle to grow body 1 px / frame. Each
     * step of bat_extra_px in our centred BIG_BAT changes body
     * width by 2 px, so we ramp every 2 ticks → 1 px / frame body
     * change = matches original's ~16-frame full grow.
     *
     * Was 1 px / tick (= 2 px / tick body), making BIG_BAT grow
     * twice as fast as the disasm prescribes. */
    {
        static unsigned char resize_gate = 0;
        resize_gate++;
        if ((resize_gate & 1) == 0) {
            if (bat_extra_px < bat_extra_tgt) bat_extra_px++;
            else if (bat_extra_px > bat_extra_tgt) bat_extra_px--;
        }
    }
    if (!bonus_active) return;
    bonus_y += motion_accel_step(&bonus_motion, 0x0008, 0x02);
    bat_left  = eff_bat_left();
    bat_right = eff_bat_right();
    /* Catch test uses bat body (10 px) not full sprite (13 px =
     * body + shadow). obj_compare_2pix at \$94BC reads (IY+\$0C, IY+\$0D)
     * which are body dimensions on object_bat_1. Shadow rows aren't
     * a catch surface. */
    if (bonus_y + BONUS_H_PX >= BAT_Y
        && bonus_y < BAT_Y + 10
        && bonus_x + BONUS_W_PX > bat_left
        && bonus_x < bat_right) {
        unsigned char caught_type = bonus_type;
        bonus_apply(bonus_type);                  /* applies effect + pushes catch sound */
        bonus_active = 0;
        score += 400;                         /* matches LD BC,$0400 / add_points_to_score at $A67D */
        /* Spawn the floating reward marker. Original LA67B_3 at \$A6FC
         * sets sprite_num = \$00 (= +400) for EVERY catch including
         * SCORE_5K, before the type dispatch — so the +5000 sprite is
         * only ever used as a falling-bonus glyph, not a marker. Match
         * by always using +400 here. */
        (void)caught_type;
        pts_marker_spr = SPR_400_POINTS;
        pts_400_x = bonus_x;
        pts_400_y = bonus_y;
        pts_400_active = 1;
        pts_400_motion.acc = (unsigned int)(((pit_ticks() & 1UL) ? 0xFEu : 0xFFu) << 8);
        pts_400_motion.frac = 0;
        /* Pick X drift in {-2, -1, +1, +2} — port of \$3030's
         * `AND \$01 / INC A / RL B / JR C / NEG` sequence:
         *   bit 0 of random → +1 or +2 magnitude
         *   bit 7 of random → keep positive or negate */
        {
            /* Read-current (rng_sample): the original reads random_number
             * (low byte) here without advancing. */
            unsigned int r = rng_sample();
            int mag = (int)((r & 1) + 1);
            pts_400_dx = (r & 0x80) ? mag : -mag;
        }
        return;
    }
    if (bonus_y > PLAYFIELD_H) bonus_active = 0;
}

/* Advance the +400 floating marker each tick. Port of handling_400pts
 * at \$A58D + the shared LA55A_0 advance with DE=\$0028, B=\$80.
 *
 * Original moves the marker DOWN (Y increases), accelerating as the
 * accumulator grows; dies when Y >= \$C0 (= 192 = bottom of playfield).
 * Earlier port had it floating UP — counterintuitive but seemed nicer.
 * Switched back to match the disasm: marker falls off the bottom. */
static void step_pts_400(void) {
    if (!pts_400_active) return;
    pts_400_y += motion_accel_step(&pts_400_motion, 0x0028, 0x80);
    /* Apply the X drift each frame (port of LA590's ADD A,SMC). Clamp
     * to playfield via the original's check_left/right_margin pattern. */
    pts_400_x += pts_400_dx;
    if (pts_400_x < 8) pts_400_x = 8;
    if (pts_400_x > PLAYFIELD_W - 16) pts_400_x = PLAYFIELD_W - 16;
    if (pts_400_y >= PLAYFIELD_H) pts_400_active = 0;
}

/* Brick band geometry: 12 rows * 8 px starting at y=32, 15 cols * 16 px
 * starting at x=8. Determines whether the ball's new center overlaps a
 * live brick and, if so, marks the brick destroyed and returns which
 * axis to reverse:
 *   0 = no hit
 *   1 = vertical hit (entered from top or bottom)  -> caller flips dy
 *   2 = horizontal hit (entered from a side)       -> caller flips dx
 * The previous ball position is needed to disambiguate corner cases. */

/* Try to drop a bonus at (col, row). Called from every brick-
 * destruction site (ball collision, laser bullet, rocket sweep)
 * so the cadence is the same regardless of who destroyed it.
 * Mirrors set_bonus's selection logic at $9D5A: random index into
 * bonus_table_current (_first for rounds 0..5, _second for 6+),
 * retry up to 16 times if the picked code maps to an unsupported
 * effect. No-op if a bonus is already in flight or the cadence
 * counter isn't at a spawn-multiple. */
static void try_spawn_bonus(int col, int row) {
    int tries;
    const unsigned char *tbl;
    if (bonus_active) return;
    /* The original shares object_bonus between falling bonus and bomb,
     * so a bomb in flight blocks new bonus spawns. We keep separate
     * state but mirror the mutual exclusion here. */
    if (bomb_active) return;
    /* Mirror the test at $A2CC: drop a bonus iff (random & 0x0F) < 5,
     * i.e. 5/16 ≈ 31% per destroyed brick. (Earlier port used a
     * deterministic every-Nth counter — close in average rate but
     * obvious as a pattern to the player.) */
    /* Drop chance: the original reads random_number+$01 WITHOUT advancing
     * (read-current; brik_value: LD A,(random_number+$01) / CP $05 /
     * CALL C,set_bonus) -> rng_sample. The bonus TYPE pick below keeps
     * next_random(): generate_new_bonus re-CALLs random_generate each
     * retry, so each iteration advances. */
    if ((random_hi(rng_sample()) & 0x0F) >= 5) return;
    tbl = (round_number >= 6) ? bonus_table_second : bonus_table_first;
    for (tries = 0; tries < 16; tries++) {
        unsigned int rnd = next_random();
        unsigned char idx = (unsigned char)(random_hi(rnd) & 0x0F);
        unsigned char code = tbl[idx];
        unsigned char mapped;
        /* Original generate_new_bonus re-rolls when the picked bonus
         * equals `current_bonus`. The setup at $9D5A sets
         * `current_bonus = (object_bat_1+$14)` (= bat.bonus_applied) just
         * before generating (the 2-player path uses object_bat_2+$14), so
         * comparing to bat.bonus_applied here is byte-faithful, not an
         * approximation. Prevents back-to-back duplicates of the same bat
         * effect. */
        if (code == objects[OBJ_BAT_1].bonus_applied) continue;
        /* Per-type exclusions from L9D5A_2..L9D5A_9:
         *   $02 (TRIPLE_BALL): skip if extra balls already in play
         *   $04 (SLOW): skip if SLOW already active
         *   $05 (LIFE): skip if already dropped this round
         *   $06 (ROCKET): skip if rocket in flight
         * From round 6 onwards, rocket gets an extra (random & $C0)
         * re-roll: ~3/4 of would-be rockets get rejected, making the
         * bonus ~4x rarer in late levels (port of $9D6F's CP $06 /
         * JR C / AND $C0 / JR NZ chain). */
        if (code == 0x02 && (ball2_active || ball3_active)) continue;
        /* Original generate_new_bonus re-rolls SLOW if a ball is already
         * at the minimum speed $02 (it checks object_ball_N+$07 == $02).
         * With the speed-ramp model, that's `primary ball speed <= base`. */
        if (code == 0x04 && objects[OBJ_BALL_1].speed <= BALL_SPEED) continue;
        if (code == 0x05 && life_dropped_this_round) continue;
        if (code == 0x06 && rocket_active) continue;
        if (code == 0x06 && round_number >= 6
            && (random_lo(rnd) & 0xC0) != 0) continue;
        mapped = bonus_from_original(code);
        if (mapped != BONUS_TYPE_UNSUPPORTED) {
            bonus_active = 1;
            bonus_x = 8 + col * 16 + (16 - BONUS_W_PX) / 2;
            bonus_y = 32 + row * 8;
            bonus_type = mapped;
            bonus_motion.acc = 0;
            bonus_motion.frac = 0;
            return;
        }
    }
}

/* Track the destroyed brick cell long enough to dirty its full original
 * blit footprint. print_one_brik_buf writes one row above, one row below,
 * and one pixel into neighbouring byte columns. */
static void brick_flash_spawn(int col, int row) {
    brick_flash_x = 8 + col * 16;
    brick_flash_y = 32 + row * 8;
    brick_flash_ticks = BRICK_FLASH_TICKS;
}

static void step_brick_flash(void) {
    if (brick_flash_ticks) brick_flash_ticks--;
}

/* No visual flash here. The original destruction path leaves the brick
 * absent after background recovery; this marker exists only for dirty
 * rectangle scheduling. */
static void render_brick_flash_to_buff(void) {
    (void)brick_flash_x;
    (void)brick_flash_y;
}

static int brick_hit_resolve(int col, int row, int axis);
static int laffc_collision(Object *o, int prev_x, int prev_y, int new_x, int new_y);

static int brick_hit_resolve(int col, int row, int axis) {
    unsigned char *cell = &live_level[row * LVL_COLS + col];
    /* BIT 5 = undestructible: bounce, never destroy.
     * BIT 4 = "this hit destroys" (1-hit brick OR multi-hit's final
     *          hit registered by an earlier collision).
     * Otherwise (bit 4 + bit 5 both clear) = multi-hit brick: this is
     *          the FIRST collision, so SET BIT 4 and bounce; the next
     *          hit will hit the BIT 4 branch above and destroy. */
    if (*cell & 0x20) {
        /* Undestructible: bounce off with the same brick-tick sound
         * as a destruction (the original plays SND_NORMAL_BRIK on
         * every brick collision regardless of survival). It also
         * schedules the same briks_data shimmer slot used by hard
         * bricks, because bit 5 jumps to LAFFC_34. */
        brick_hit_anim_spawn(col, row);
        sound_queue(SND_NORMAL_BRIK);
        return axis;
    }
    /* SMASH (BIG_BALL) bypasses the multi-hit half-state — port of
     * LAFFC's `CP \$07; JR Z,LAFFC_38` test that jumps directly to
     * the destroy path. Without this, multi-hit bricks still need
     * two hits even with SMASH active. */
    if (!big_ball_active() && !(*cell & 0x10)) {
        *cell |= 0x10;
        brick_hit_anim_spawn(col, row);
        sound_queue(SND_NORMAL_BRIK);
        return axis;
    }

    /* BIT 4 set: destroy on this hit. */
    {
        unsigned char cell_val = *cell;
        unsigned int idx = (unsigned int)((row < 12) ? row : 11);
        unsigned int pts = points_table[idx];
        if ((cell_val & 0x0F) >= 6) pts *= 2;     /* metal -> double */
        score += pts;
    }
    *cell |= 0x80;
    mark_brick_row_dirty(row);
    sound_queue(SND_NORMAL_BRIK);            /* brick-break click */
    /* BIG_BALL (smash) bonus: ball ploughs through bricks rather
     * than bouncing — keep the bonus-spawn check below intact but
     * stash the "no bounce" intent. */
    if (big_ball_active()) axis = 0;
    brick_flash_spawn(col, row);
    try_spawn_bonus(col, row);
    return axis;
}

static int brick_collision(int prev_x, int prev_y, int new_x, int new_y) {
    const BrickHit hit = brick_sweep(BrickField(live_level),
                                     eff_ball_size(), BALL_H_PX,
                                     prev_x, prev_y, new_x, new_y);
    if (!hit.hit) return 0;
    return brick_hit_resolve(hit.col, hit.row, hit.axis);
}

static int laffc_collision(Object *o, int prev_x, int prev_y, int new_x, int new_y) {
    (void)prev_x; (void)prev_y;
    const LaffcHit hit = laffc_sweep(BrickField(live_level), o->dir,
                                     o->w_body_px, o->h_body_px, new_x, new_y);
    if (!hit.hit) return 0;

    /* SMASH (big ball) ploughs through: the cell is destroyed and there is
     * no bounce to apply. */
    if (brick_hit_resolve(hit.col, hit.row, 1) == 0) return 0;

    const BallBounce bounce = laffc_bounce(hit, o->dir,
                                           o->w_body_px, o->h_body_px,
                                           new_x, new_y);
    o->x_coord = bounce.x;
    o->y_coord = bounce.y;
    o->dir     = bounce.dir;

    int dx_q8, dy_q8;
    dir_to_dxdy(o->dir, o->speed, &dx_q8, &dy_q8);
    ball_dx = (dx_q8 < 0) ? -1 : (dx_q8 > 0 ? 1 : 0);
    ball_dy = (dy_q8 < 0) ? -1 : (dy_q8 > 0 ? 1 : 0);
    return 3;   /* reflected and snapped; step_ball must not re-reflect */
}

/* Count remaining destructible bricks: bit 7 clear (still present)
 * AND bit 5 clear (not undestructible). bit 4 is the multi-hit
 * "next hit destroys" marker — those still count as destructible. */
static int live_bricks_remaining(void) {
    int i, n = 0;
    for (i = 0; i < LVL_CELLS; i++) {
        if (!(live_level[i] & 0xA0)) n++;
    }
    return n;
}

/* --- Enemy preparation (port of enemy_prepare @ $9EAA) -----------------
 *
 * Activates object_enemy at the playfield edge under the original's
 * spawn conditions:
 *   - current_level_number_1up != 4 (= no aliens on L5 in original)
 *   - applied bonus on bat_1 / bat_2 != $09 (kill-aliens bonus)
 *   - briks_quantity_1up < 44 (=  enough bricks down for late-level alien)
 *   - object_enemy currently empty
 * Picks bird (sprite_set $09) on odd rounds, UFO ($08) on even. X
 * coord from prop_x_coord[random & 3] = {$40, $A8, $40, $A8}. Speed
 * from per-round prop table. */
static unsigned char ctrl_btns_pressed_value(void) {
    unsigned char v = 0;
    if (key_state[SC_RIGHT]) v |= 0x01;
    if (key_state[SC_LEFT])  v |= 0x02;
    if (key_state[SC_SPACE]) v |= 0x10;
    return v;
}

static unsigned int next_random(void) { return rng_next(ctrl_btns_pressed_value()); }

/* Sample the RNG for a "read-current" consumer (the original's
 * `LD A,(random_number)` without a preceding `CALL random_generate`).
 * With rng_perframe OFF this is identical to next_random() (advance on
 * read) so behaviour and all gates are byte-unchanged; with it ON it
 * returns the current random_number WITHOUT advancing, because the
 * per-frame tick (added in the play loop) is the only advance — matching
 * the original. Consumers the original advances-then-reads (bonus
 * generation) keep calling next_random() directly. */
/* The noise envelope's random source. orig: the magnet zip reads
 * random_number like any other consumer. */
static u8 sound_random_byte(void) { return rng_low(u16(next_random())); }

/* The enemy's two reads, kept distinct on purpose: arrival looks at the
 * current number, target-picking goes through the model's sampler.
 * See notes/rng-model.md. */
static u8 enemy_random_current(void) { return rng_low(rng_current()); }
static u8 enemy_random_sample(void)  { return rng_low(u16(rng_sample())); }

static unsigned int rng_sample(void) {
    return rng_perframe ? rng_current() : next_random();
}

static void apply_replay_random_override(void) {
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

static int replay_hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int replay_parse_hex_bytes(const char *p, unsigned char *out, int n) {
    int i;
    if (p == NULL) return -1;
    for (i = 0; i < n; i++) {
        int hi = replay_hex_nibble(p[i * 2]);
        int lo = replay_hex_nibble(p[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    if (p[n * 2] != '\0') return -1;
    return 0;
}

static void apply_replay_bat_object_override(void) {
    unsigned char bytes[sizeof(Object)];
    if (replay_parse_hex_bytes(getenv("BATTY_REPLAY_BAT_OBJECT"),
                               bytes, (int)sizeof(bytes)) != 0) return;
    memcpy(&objects[OBJ_BAT_1], bytes, sizeof(bytes));
}

static void apply_replay_ball_object_override(void) {
    unsigned char bytes[sizeof(Object)];
    if (replay_parse_hex_bytes(getenv("BATTY_REPLAY_BALL_OBJECT"),
                               bytes, (int)sizeof(bytes)) != 0) return;
    memcpy(&objects[OBJ_BALL_1], bytes, sizeof(bytes));
}

static void apply_replay_ball_motion_override(void) {
    const char *stuck = getenv("BATTY_REPLAY_BALL_STUCK");
    const char *vel = getenv("BATTY_REPLAY_BALL_VEL");
    if (stuck != NULL) {
        ball_stuck = (unsigned char)(atoi(stuck) != 0);
    }
    if (vel != NULL) {
        char *endp;
        long dx = strtol(vel, &endp, 0);
        if (endp != vel && *endp == ',') {
            char *endp2;
            long dy = strtol(endp + 1, &endp2, 0);
            if (endp2 != endp + 1) {
                primary_ball_set_velocity((int)dx, (int)dy);
            }
        }
    }
    if (getenv("BATTY_HIDE_BALL") != NULL) {
        BALL_HIDE();
        ball_stuck = 0;
    }
}

static void apply_replay_enemy_object_override(void) {
    unsigned char bytes[sizeof(Object)];
    if (replay_parse_hex_bytes(getenv("BATTY_REPLAY_ENEMY_OBJECT"),
                               bytes, (int)sizeof(bytes)) != 0) return;
    memcpy(&objects[OBJ_ENEMY], bytes, sizeof(bytes));
}

/* Bake a falling bonus for the falling-object regression gate.
 * BATTY_REPLAY_BONUS = "type,x,y" (decimal or 0x-hex per field). Starts a
 * fresh fall (bonus_motion zeroed) so the accel progression
 * motion_accel_step(&bonus_motion, 0x0008, 0x02) is deterministic from y.
 * Put x clear of the bat to test pure fall (no catch). */
static void apply_replay_bonus_override(void) {
    const char *spec = getenv("BATTY_REPLAY_BONUS");
    char *endp, *endp2, *endp3;
    long type, x, y;
    if (spec == NULL) return;
    type = strtol(spec, &endp, 0);
    if (endp == spec || *endp != ',') return;
    x = strtol(endp + 1, &endp2, 0);
    if (endp2 == endp + 1 || *endp2 != ',') return;
    y = strtol(endp2 + 1, &endp3, 0);
    if (endp3 == endp2 + 1) return;
    bonus_active = 1;
    bonus_type = (unsigned char)type;
    bonus_x = (int)x;
    bonus_y = (int)y;
    bonus_motion.acc = 0;
    bonus_motion.frac = 0;
}

/* Bake a falling enemy bomb for the bomb-fall regression gate.
 * BATTY_REPLAY_BOMB = "x,y". Same accel family as the bonus
 * (motion_accel_step(&bomb_motion, 0x0008, 0x02)); put x clear of the bat
 * to test pure fall (no bat-kill). */
static void apply_replay_bomb_override(void) {
    const char *spec = getenv("BATTY_REPLAY_BOMB");
    char *endp, *endp2;
    long x, y;
    if (spec == NULL) return;
    x = strtol(spec, &endp, 0);
    if (endp == spec || *endp != ',') return;
    y = strtol(endp + 1, &endp2, 0);
    if (endp2 == endp + 1) return;
    bomb_active = 1;
    bomb_x = (int)x;
    bomb_y = (int)y;
    bomb_motion.acc = 0;
    bomb_motion.frac = 0;
}

/* Bake a rising/falling +400 score popup for the pts-400-fall gate.
 * BATTY_REPLAY_PTS400 = "x,y". Uses motion_accel_step(&pts_400_motion,
 * 0x0028, 0x80) — a DIFFERENT accel constant pair than bonus/bomb, so this
 * exercises a faster-grow path. dx is zeroed so the y progression is pure. */
static void apply_replay_pts400_override(void) {
    const char *spec = getenv("BATTY_REPLAY_PTS400");
    char *endp, *endp2;
    long x, y;
    if (spec == NULL) return;
    x = strtol(spec, &endp, 0);
    if (endp == spec || *endp != ',') return;
    y = strtol(endp + 1, &endp2, 0);
    if (endp2 == endp + 1) return;
    pts_400_active = 1;
    pts_400_x = (int)x;
    pts_400_y = (int)y;
    pts_400_dx = 0;
    pts_400_motion.acc = 0;
    pts_400_motion.frac = 0;
}

/* Bake an in-flight laser bullet for the bullet-motion gate.
 * BATTY_REPLAY_BULLET = "x,y" into slot 0. The bullet rises at a constant
 * BULLET_SPEED (6 px/frame) in step_bullet_one; probe it in the window
 * below the brick field (y > 128) so it travels without blasting. */
static void apply_replay_bullet_override(void) {
    const char *spec = getenv("BATTY_REPLAY_BULLET");
    char *endp, *endp2;
    long x, y;
    if (spec == NULL) return;
    x = strtol(spec, &endp, 0);
    if (endp == spec || *endp != ',') return;
    y = strtol(endp + 1, &endp2, 0);
    if (endp2 == endp + 1) return;
    bullet_active[0] = 1;
    bullet_x[0] = (int)x;
    bullet_y[0] = (int)y;
}

/* Activate big-ball (SMASH) for the deterministic big-ball dirty-tier gate.
 * big_ball_active() needs big_ball_ticks>0 AND bat.bonus_applied==0x07. */
static void apply_replay_bigball(void) {
    if (getenv("BATTY_REPLAY_BIGBALL") == NULL) return;
    big_ball_ticks = BIG_BALL_DURATION;
    objects[OBJ_BAT_1].bonus_applied = 0x07;
    objects[OBJ_BAT_2].bonus_applied = 0x07;
}

/* Bake two extra balls (multi-ball) for the deterministic extra-ball dirty
 * tier gate — WITHOUT a bonus catch (so no +400 popup) and placed BELOW the
 * brick band (y=150, clear of bricks at y<128 and the bat at y>=173), so a
 * few-frame probe stays clear of any emergent brick hit regardless of
 * direction. Dirs/speed copied from the primary. */
static void apply_replay_multiball(void) {
    if (getenv("BATTY_REPLAY_MULTIBALL") == NULL) return;
    if (ball2_active || ball3_active) return;
    ball2_active = 1;
    objects[OBJ_BALL_2].sprite_set = 0x02;
    objects[OBJ_BALL_2].x_coord = 96;
    objects[OBJ_BALL_2].y_coord = 150;
    objects[OBJ_BALL_2].dir = objects[OBJ_BALL_1].dir;
    objects[OBJ_BALL_2].speed = objects[OBJ_BALL_1].speed;
    objects[OBJ_BALL_2].x_coord_hi = 0;
    objects[OBJ_BALL_2].y_coord_hi = 0;
    dir_to_delta(objects[OBJ_BALL_2].dir, &ball2_dx, &ball2_dy);
    ball3_active = 1;
    objects[OBJ_BALL_3].sprite_set = 0x02;
    objects[OBJ_BALL_3].x_coord = 160;
    objects[OBJ_BALL_3].y_coord = 150;
    objects[OBJ_BALL_3].dir = objects[OBJ_BALL_1].dir;
    objects[OBJ_BALL_3].speed = objects[OBJ_BALL_1].speed;
    objects[OBJ_BALL_3].x_coord_hi = 0;
    objects[OBJ_BALL_3].y_coord_hi = 0;
    dir_to_delta(objects[OBJ_BALL_3].dir, &ball3_dx, &ball3_dy);
}

/* Force one bonus-drop roll at level entry for the drop-economy gate.
 * BATTY_FORCE_SPAWN_BONUS = "1" (or "col,row") calls try_spawn_bonus once
 * with the freshly-baked RNG (no frames elapsed yet), isolating the drop
 * decision: with rng_perframe ON, the gate is (rng high & 0x0F) < 5, so a
 * baked BATTY_REPLAY_RANDOM directly controls whether a bonus drops. */
static void apply_replay_force_bonus(void) {
    const char *spec = getenv("BATTY_FORCE_SPAWN_BONUS");
    int col = 5, row = 5;
    char *endp;
    if (spec == NULL) return;
    if (*spec) {
        long c = strtol(spec, &endp, 0);
        if (endp != spec && *endp == ',') {
            col = (int)c;
            row = (int)strtol(endp + 1, NULL, 0);
        }
    }
    try_spawn_bonus(col, row);
}

/* Seed the ball speed-up ramp counter for the speed-ramp gate.
 * BATTY_REPLAY_BALL_RAMP = value. The ramp bumps every active ball's speed
 * (cap 6) when ball_speed_ramp reaches 0x94, ticking once per 8 frames
 * (ball_speed_ramp_tick). Seeding it near 0x94 lets the gate observe a bump
 * in a few frames instead of the full ~1184-frame climb. */
static void apply_replay_ball_ramp(void) {
    const char *spec = getenv("BATTY_REPLAY_BALL_RAMP");
    if (spec == NULL || !*spec) return;
    ball_speed_ramp = (unsigned int)strtol(spec, NULL, 0);
}

/* Plant a known brick for the per-row scoring gate.
 * BATTY_FORCE_BRICK = "col,row,value" overwrites live_level[row*COLS+col]
 * after the level load, so a bullet/ball can destroy a brick of a known row
 * + colour and the points_table[row] (x2 for colour nibble >= 6) award is
 * checkable. value 0x1X = single-hit (bit4 set) colour-X brick. */
static void apply_replay_force_brick(void) {
    const char *spec = getenv("BATTY_FORCE_BRICK");
    char *e1, *e2;
    long col, row, val;
    if (spec == NULL) return;
    col = strtol(spec, &e1, 0);
    if (e1 == spec || *e1 != ',') return;
    row = strtol(e1 + 1, &e2, 0);
    if (e2 == e1 + 1 || *e2 != ',') return;
    val = strtol(e2 + 1, NULL, 0);
    if (col < 0 || col >= LVL_COLS || row < 0 || row >= LVL_ROWS) return;
    live_level[row * LVL_COLS + col] = (unsigned char)val;
}

/* Bake a bullet-impact blast for the blast-animation gate.
 * BATTY_REPLAY_BLAST = "x,y" arms slot 0 at full duration; step_bullet_blast
 * decrements bullet_blast_ticks 1/frame (8 -> 0 over 8 frames = 4 frames x
 * BULLET_BLAST_TICKS_PER_FRAME), render frame = (ticks-1)/2. */
static void apply_replay_blast_override(void) {
    const char *spec = getenv("BATTY_REPLAY_BLAST");
    char *endp, *endp2;
    long x, y;
    if (spec == NULL) return;
    x = strtol(spec, &endp, 0);
    if (endp == spec || *endp != ',') return;
    y = strtol(endp + 1, &endp2, 0);
    if (endp2 == endp + 1) return;
    bullet_blast_ticks[0] = BULLET_BLAST_FRAMES * BULLET_BLAST_TICKS_PER_FRAME;
    bullet_blast_x[0] = (int)x;
    bullet_blast_y[0] = (int)y;
}

static void apply_replay_rocket_override(void) {
    if (getenv("BATTY_REPLAY_ROCKET_ACTIVE") == NULL) return;
    rocket_active = 1;
    rocket_clear_completed = 0;
    set_rocket_bonus_sprite_height(ROCKET_H_PX);
    rocket_x = BAT_X + 4;
    if (bat_extra_px >= BAT_BIG_EXTRA_PX) rocket_x += 8;
    rocket_y = BAT_Y + 6;
    rocket_acc = 0;
    rocket_frac = 0;
    rocket_counter = 0;
    BALL_HIDE();
    ball_stuck = 0;
}

/* prop_uneven / prop_even / prop_x_coord from $9F27. Fields:
 *   +0 type ($09=bird, $08=UFO)
 *   +1 misc_12
 *   +2 misc_13
 *   +3 width body
 *   +4 height body
 *   +5 speed */
static const unsigned char prop_uneven[6] = { 0x09, 0xF0, 0x70, 0x18, 0x0C, 0x01 };
/* prop_even byte-exact per $9F2D — was hand-tuned earlier with extra
 * speed and a smaller height; the original UFO is 16 px tall and moves
 * at speed 1, same as the bird. */
static const unsigned char prop_even[6]   = { 0x08, 0x60, 0x90, 0x18, 0x10, 0x01 };
static const unsigned char prop_x_coord[4]= { 0x40, 0xA8, 0x40, 0xA8 };

unsigned char round_number = 0;              /* current round counter */
static void play_bat_explosion(unsigned char level_idx);   /* forward */
static void respawn_primary_ball(void);                     /* forward */
static unsigned char current_level_idx_var;  /* set by run_level so
                                              * enemy_prepare can read it */

static unsigned int replay_probe_screen_addr_for_brick(int col, int row) {
    unsigned int x = 8u + (unsigned int)col * 16u;
    unsigned int y = 32u + (unsigned int)row * 8u;
    return 0x4000u
         + ((y & 0xC0u) << 5)
         + ((y & 0x07u) << 8)
         + ((y & 0x38u) << 2)
         + (x >> 3);
}

static void write_replay_briks_data(FILE *f) {
    int i;
    fprintf(f, "\nbriks_data=");
    for (i = 0; i < BRICK_HIT_ANIM_SLOTS; i++) {
        unsigned char tick = brick_hit_anim_ticks[i];
        unsigned char col = brick_hit_anim_col[i];
        unsigned char row = brick_hit_anim_row[i];
        unsigned int screen_addr = tick
                                 ? replay_probe_screen_addr_for_brick(col, row)
                                 : 0;
        unsigned int scr_buff_addr = tick ? 0xDA00u + 0x401u
                                  + (unsigned int)row * 0x100u
                                  + (unsigned int)col * 2u : 0;
        unsigned int level_addr = tick ? 0x6100u
                                + (unsigned int)row * LVL_COLS
                                + (unsigned int)col : 0;
        fprintf(f, "%02X%02X%02X%02X%02X%02X%02X",
                tick,
                screen_addr & 0xFFu, screen_addr >> 8,
                scr_buff_addr & 0xFFu, scr_buff_addr >> 8,
                level_addr & 0xFFu, level_addr >> 8);
    }
}

/* Tell the harness, via COM1, that the port has REACHED a probe checkpoint
 * (frame N) and written PROBE.TXT — so it can capture/quit deterministically
 * instead of guessing with a wall-clock sleep. QEMU `-serial file:...`
 * captures these bytes live (flushed per write), which is robust to slow/TCG
 * emulation where wall-clock frame waits land on the wrong frame. COM1 THR @
 * 0x3F8; bounded spin on the LSR THR-empty bit (0x20 @ 0x3FD) so it never
 * hangs and is a harmless no-op when no serial backend is attached. Opt-in
 * via BATTY_SERIAL_PROBE so existing wall-clock callers are unaffected. */
static unsigned char serial_probe_enabled = 0;

static void serial_probe_signal(void) {
    static const char msg[] = "PROBE\r\n";
    int k;
    if (!serial_probe_enabled) return;
    for (k = 0; msg[k] != '\0'; k++) {
        unsigned int spin = 0;
        while (!(inp(0x3FD) & 0x20) && ++spin < 60000u) { }
        outp(0x3F8, (unsigned char)msg[k]);
    }
}

static void write_replay_probe(void) {
    FILE *f;
    int i;
    if (getenv("BATTY_REPLAY_PROBE") == NULL) return;
    f = fopen("PROBE.TXT", "wt");
    if (!f) return;
    fprintf(f, "probe_phase=%s\n", probe_from_gameplay ? "play" : "init");
    fprintf(f, "round_number=%02X\n", (unsigned)round_number);
    fprintf(f, "current_level=%02X\n", (unsigned)current_level_idx_var);
    fprintf(f, "bricks_quantity=%02X\n", (unsigned)live_bricks_remaining());
    fprintf(f, "score=%06lu\n", score);
    fprintf(f, "random_number=%04X\n", (unsigned)rng_current());
    fprintf(f, "random_seed=%04X\n", (unsigned)rng_seed_addr());
    fprintf(f, "enemy_repicks=arrival%u_margin%u_turns%u\n",
            enemy_arrival_repicks, enemy_margin_repicks,
            enemy_turn_calls);
    fprintf(f, "brik_anim_ticks=%lu\n", brik_anim_probe_ticks);
    fprintf(f, "magnet_state=count%02X_on%02X%02X%02X%02X_ball0_c%02X_d%02X_e%02X_i%02X\n",
            (unsigned)magnet_count,
            (unsigned)magnet_on_state[0], (unsigned)magnet_on_state[1],
            (unsigned)magnet_on_state[2], (unsigned)magnet_on_state[3],
            (unsigned)ball_mag_cool[0], (unsigned)ball_mag_delta[0],
            (unsigned)ball_mag_exit[0], (unsigned)ball_mag_idx[0]);
    fprintf(f, "object_ball_1=");
    for (i = 0; i < (int)sizeof(Object); i++) {
        fprintf(f, "%02X", ((unsigned char *)&objects[OBJ_BALL_1])[i]);
    }
    fprintf(f, "\nobject_bat_1=");
    for (i = 0; i < (int)sizeof(Object); i++) {
        fprintf(f, "%02X", ((unsigned char *)&objects[OBJ_BAT_1])[i]);
    }
    fprintf(f, "\nobject_enemy=");
    for (i = 0; i < (int)sizeof(Object); i++) {
        fprintf(f, "%02X", ((unsigned char *)&objects[OBJ_ENEMY])[i]);
    }
    /* Extra balls (multiball) — so the collision-invariant sweep can probe
     * step_extra_ball's path (no-tunnel) the same way it probes the primary. */
    fprintf(f, "\nobject_ball_2=");
    for (i = 0; i < (int)sizeof(Object); i++) {
        fprintf(f, "%02X", ((unsigned char *)&objects[OBJ_BALL_2])[i]);
    }
    fprintf(f, "\nobject_ball_3=");
    for (i = 0; i < (int)sizeof(Object); i++) {
        fprintf(f, "%02X", ((unsigned char *)&objects[OBJ_BALL_3])[i]);
    }
    /* Bonus/bomb state (the original shares object_bonus $9B80 for both).
     * Used to verify RNG-dependent drops match the original (e.g. that the
     * RNG-perframe flip + seed do not spawn a spurious bonus). */
    fprintf(f, "\nbonus_state=active%02X_type%02X_x%02X_y%02X_bomb%02X",
            (unsigned)bonus_active, (unsigned)bonus_type,
            (unsigned)(bonus_x & 0xFF), (unsigned)(bonus_y & 0xFF),
            (unsigned)bomb_active);
    fprintf(f, "\nbomb_state=active%02X_x%02X_y%02X",
            (unsigned)bomb_active, (unsigned)(bomb_x & 0xFF),
            (unsigned)(bomb_y & 0xFF));
    fprintf(f, "\npts400_state=active%02X_x%02X_y%02X",
            (unsigned)pts_400_active, (unsigned)(pts_400_x & 0xFF),
            (unsigned)(pts_400_y & 0xFF));
    fprintf(f, "\nbullet_state=active%02X_x%02X_y%02X",
            (unsigned)bullet_active[0], (unsigned)(bullet_x[0] & 0xFF),
            (unsigned)(bullet_y[0] & 0xFF));
    fprintf(f, "\nlaser_fire_state=shots%04X_cd%02X",
            (unsigned)dbg_shots_fired, (unsigned)bullet_cooldown);
    fprintf(f, "\nspeed_ramp_state=ramp%04X_spd%02X",
            (unsigned)ball_speed_ramp, (unsigned)objects[OBJ_BALL_1].speed);
    fprintf(f, "\nblast_state=ticks%02X_frame%02X",
            (unsigned)bullet_blast_ticks[0],
            (unsigned)(bullet_blast_ticks[0]
                       ? (bullet_blast_ticks[0] - 1) / BULLET_BLAST_TICKS_PER_FRAME
                       : 0xFF));
    fprintf(f, "\neffects_state=b2%02X_b3%02X_xtgt%02X_bball%02X_lives%02X",
            (unsigned)ball2_active, (unsigned)ball3_active,
            (unsigned)(bat_extra_tgt & 0xFF),
            (unsigned)(big_ball_ticks != 0),
            (unsigned)(lives & 0xFF));
    fprintf(f, "\nnormal_launch_state=%02X%02X%02X%02X%02X",
            (unsigned)last_primary_launch_valid,
            (unsigned)last_primary_launch_x,
            (unsigned)last_primary_launch_y,
            (unsigned)last_primary_launch_dir,
            (unsigned)last_primary_launch_speed);
    fprintf(f, "\nlaunch_probe_state=%04X%04X%02X",
            (unsigned)launch_probe_frames,
            (unsigned)launch_probe_countdown,
            (unsigned)launch_probe_active);
    fprintf(f, "\nframe_probe_state=%04X%04X%02X",
            (unsigned)frame_probe_frames,
            (unsigned)frame_probe_countdown,
            (unsigned)frame_probe_active);
    fprintf(f, "\nbonus_state=%02X%02X%02X%02X%02X%04X",
            (unsigned)bonus_active,
            (unsigned)bonus_type,
            (unsigned)(bonus_x & 0xFF),
            (unsigned)(bonus_y & 0xFF),
            (unsigned)pts_400_active,
            (unsigned)(pts_marker_spr & 0xFFFFu));
    write_replay_briks_data(f);
    fprintf(f, "\ncurrent_level_copy=");
    for (i = 0; i < LVL_CELLS; i++) fprintf(f, "%02X", live_level[i]);
    /* Full 32x24 attr buffer, plus the static-background attr snapshot
     * (bg_attr_buff, set after the level's first full paint). A moving
     * object must NOT alter any cell's attr vs the original — moving
     * sprites only blit pixels (print_obj_to_buff $B82C), they never call
     * print_sprite_attrib. test-enemy-attr-parity asserts attr_buff equals
     * bg_attr_buff under a flying enemy (= the bird shows ZX colour-clash
     * in the underlying brick/bg colour, not recoloured to bg_attr). */
    fprintf(f, "\nattr_buff=");
    for (i = 0; i < 768; i++) fprintf(f, "%02X", attr_buff[i]);
    fprintf(f, "\nbg_attr_buff=");
    for (i = 0; i < 768; i++) fprintf(f, "%02X", bg_attr_buff[i]);
    fprintf(f, "\n");
    fclose(f);
}

static void enemy_prepare(void) {
    Object *e = &objects[OBJ_ENEMY];
    const unsigned char *prop;
    unsigned char r;
    /* Test-mode pin (BATTYALL): no NATURAL alien spawns — the same
     * determinism trick as the menu-blink / running-dot / magnet-toggle
     * pins. On levels whose starting brick count is already below the
     * 0x2C spawn gate (L3: 26, L9: 7 — L5 is exempted below), an alien
     * spawns within the first gameplay frame and the wall-clock state4
     * screendump races its descent — the L3/L9 "186 px drift"
     * (notes/per-level-profile.md, 2026-06-11). The GT is alien-free by
     * construction. Tests that need an alien seed one explicitly via
     * BATTY_REPLAY_ENEMY_OBJECT, which bypasses this spawner. */
    if (test_mode_pin_blink) return;
    /* Original $9EAA returns immediately when current_level == 4 —
     * L4 has no enemies at all. Earlier port added a bouncing spark
     * here as extra challenge; removed for byte-exact parity. */
    if (current_level_idx_var == 4) return;
    /* Skip if bat carries the kill-aliens bonus. */
    if (objects[OBJ_BAT_1].bonus_applied == 0x09) return;
    if (objects[OBJ_BAT_2].bonus_applied == 0x09) return;
    /* Skip while bricks remaining >= 44. */
    if (live_bricks_remaining() >= 0x2C) return;
    /* Skip if alien already active. */
    if (e->sprite_set != 0) return;
    /* clear_hl_buff16 equivalent. */
    {
        unsigned int i;
        unsigned char *p = (unsigned char *)e;
        for (i = 0; i < sizeof(*e); i++) p[i] = 0;
    }
    prop = (round_number & 1) ? prop_even : prop_uneven;
    e->sprite_set = prop[0];
    e->prev_h_shadow = 0;
    e->misc_12 = prop[1];
    e->misc_13 = prop[2];
    e->w_body_px = prop[3];
    e->h_body_px = prop[4];
    e->speed = prop[5];
    e->sprite_num = 0;
    e->y_coord = 0;
    /* Original enemy_prepare ($9EAA): x = prop_x_coord[random_number & 3]
     * (read-current, no advance -> rng_sample), then dir = $10 and
     * target (+$14) = $10 UNCONDITIONALLY. The enemy spawns heading
     * straight down and steers from there (ground truth: frame-0 dir =
     * 0x10). The port had derived dir from `r` (0x38/0x08), which spawned
     * it on a diagonal it never has on the Spectrum. */
    r = (unsigned char)(random_lo(rng_sample()) & 3);
    e->x_coord = prop_x_coord[r];
    e->x_coord_hi = 0;
    e->y_coord_hi = 0;
    e->dir = 0x10;             /* LD (IX+$06),$10 */
    e->bonus_applied = 0x10;   /* LD (IX+$14),$10 — initial target */
}

/* Port of kill_enemy_by_bat at $A4B8 / kill_enemy at $A4C4. AABB check
 * between the alien body rect and the bat; on overlap, deactivate the
 * alien, award 350 BCD points (LD BC, $0350 at $A4E0), and push the
 * alien-blast sound. Full blast animation via sprite_set = $0A and
 * handling_blast is deferred - we just mark inactive for now. */
static void kill_enemy_by_bat(void) {
    Object *e = &objects[OBJ_ENEMY];
    int ex_l, ex_r, ey_t, ey_b;
    int bx_l, bx_r, by_t, by_b;
    if ((e->sprite_set & 0x7F) == 0) return;        /* slot empty */
    if (e->sprite_set & 0x80)       return;        /* inactive */
    if ((e->sprite_set & 0x7F) == 0x0A) return;    /* already exploding */
    ex_l = e->x_coord;
    ex_r = e->x_coord + e->w_body_px;
    ey_t = e->y_coord;
    ey_b = e->y_coord + e->h_body_px;
    /* Use effective bat extents so BIG_BAT widens the kill zone too —
     * the original uses obj_compare_2pix with (IY+\$0C) = current bat
     * body width, which grows with the BIG_BAT bonus. */
    bx_l = eff_bat_left();
    bx_r = eff_bat_right();
    by_t = BAT_Y;
    by_b = BAT_Y + 10;                /* h_body_px = \$0A per object_bat_1 init */
    if (ex_r <= bx_l || ex_l >= bx_r) return;
    if (ey_b <= by_t || ey_t >= by_b) return;
    /* Hit. Transition to blast state (per $A4C4): sprite_set = $0A
     * so handling_table_routines dispatches to handling_blast_obj
     * which animates the 5-frame explosion then deactivates.
     *
     * Centre the blast sprite (16x13) over the alien before swapping
     * — mirror of the $A4D2 position adjustment which adds
     * (old_w_shadow - 2) * 4 to X and 4 to Y so the explosion is
     * roughly where the alien's centre was. */
    e->x_coord = (unsigned char)(e->x_coord + (int)e->w_body_px / 2 - 8);
    e->y_coord = (unsigned char)(e->y_coord + 4);
    e->w_body_px = 16;
    e->h_body_px = 13;
    e->sprite_set = 0x0A;
    e->sprite_num = 0;
    e->misc_12 = 0x50;          /* kill_enemy $A4C4: LD (IY+$12),$50 */
    score += 350;                                   /* $0350 BCD */
    sound_queue(SND_ALIEN_BLAST);                    /* port of $C1A8 */
}

/* Mirror of kill_enemy_by_bat for the ball — original
 * kill_enemy_by_bat at $A4B8 is called from BOTH handling_bat AND
 * handling_ball (see the cross-reference at line 2745 of the disasm),
 * so a ball plunking down on an alien destroys it the same way a bat
 * crashing into one does. AABB between the ball body (8x7) and the
 * alien body. */
static void kill_enemy_by_ball_rect(int bx_l, int by_t, int bw, int bh) {
    Object *e = &objects[OBJ_ENEMY];
    int ex_l, ex_r, ey_t, ey_b;
    int bx_r = bx_l + bw;
    int by_b = by_t + bh;
    if ((e->sprite_set & 0x7F) == 0) return;
    if (e->sprite_set & 0x80)       return;
    if ((e->sprite_set & 0x7F) == 0x0A) return;
    ex_l = e->x_coord;
    ex_r = e->x_coord + e->w_body_px;
    ey_t = e->y_coord;
    ey_b = e->y_coord + e->h_body_px;
    if (ex_r <= bx_l || ex_l >= bx_r) return;
    if (ey_b <= by_t || ey_t >= by_b) return;
    /* Same blast transition as kill_enemy_by_bat. */
    e->x_coord = (unsigned char)(e->x_coord + (int)e->w_body_px / 2 - 8);
    e->y_coord = (unsigned char)(e->y_coord + 4);
    e->w_body_px = 16;
    e->h_body_px = 13;
    e->sprite_set = 0x0A;
    e->sprite_num = 0;
    e->misc_12 = 0x50;          /* kill_enemy $A4C4 seed */
    score += 350;
    sound_queue(SND_ALIEN_BLAST);
}

/* Port of bomb_appear at $A977 - called per alien tick. Probability
 * (random + random+1) & $3F == 0 = ~1/64 chance per call. Bomb
 * shares the bonus slot in the original; we keep separate state. */
static void bomb_appear(Object *o) {
    unsigned int r;
    if (bomb_active) return;
    if (bonus_active) return;
    /* Read-current (rng_sample): $A989 reads both random_number bytes
     * without advancing. bomb_appear runs every alien frame, so leaving it
     * on next_random() advances the shared RNG every frame and is the main
     * polluter of the enemy target sequence (see notes/rng-model.md). */
    r = rng_sample();
    /* Original at $A989: `LD A,(random_number); LD B,A;
     * LD A,(random_number+$01); ADD A,B; AND $3F; RET NZ`. ADD
     * not XOR — the byte distributions are subtly different,
     * even though both gate at $3F = 1/64. */
    if ((unsigned char)((random_hi(r) + random_lo(r)) & 0x3F) != 0) return;
    /* Only spawn while alien still in upper half (y < $C0 = 192). */
    if (o->y_coord + 8 >= 0xC0) return;
    bomb_active = 1;
    bomb_x = (int)o->x_coord + 8;
    bomb_y = (int)o->y_coord + 8;
    bomb_motion.acc = 0;
    bomb_motion.frac = 0;
}

/* Step the bomb each frame: fall, check bat collision, deactivate
 * past the bottom. Bat hit costs a life and respawns the ball. */
static void step_bomb(void) {
    int bx_l, bx_r, by_t, by_b;
    if (!bomb_active) return;
    bomb_y += motion_accel_step(&bomb_motion, 0x0008, 0x02);
    /* Original bomb_appear at $A977 sets (object_bonus+$0C, +$0D) =
     * $08, $08 — bomb body is 8x8 starting at (bomb_x, bomb_y), not
     * the full 8x12 sprite extent. Earlier port used only the last
     * 4 px of the sprite for collision, which triggered slightly
     * earlier (when bomb-bottom reached bat-top) than the original
     * (when bomb-body overlapped bat-body). */
    bx_l = bomb_x; bx_r = bomb_x + BOMB_W_PX;
    by_t = bomb_y;            by_b = bomb_y + 8;
    if (by_b >= BAT_Y && by_t < BAT_Y + 10
        && bx_r > eff_bat_left() && bx_l < eff_bat_right()) {
        /* h = 10 matches object_bat_1.h_body_px = \$0A — same as
         * kill_enemy_by_bat's body band. */
        /* Bomb hit the bat. Original at $A69D zeroes balls_quantity
         * which triggers LBC10's bat-explosion + lives-- branch on
         * the next frame — i.e. ALL balls die, not just the primary.
         * Mirror that here so multi-ball play can't soak bomb hits. */
        bomb_active = 0;
        ball2_active = 0;
        objects[OBJ_BALL_2].sprite_set = 0x82;
        ball3_active = 0;
        objects[OBJ_BALL_3].sprite_set = 0x82;
        play_bat_explosion(current_level_idx_var);
        if (lives > 0) lives--;
        if (lives > 0) respawn_primary_ball();    /* else game-over fires next frame */
        return;
    }
    if (bomb_y > PLAYFIELD_H) bomb_active = 0;
}

/* Step one laser bullet slot (slot index in `b`). Move up; on first
 * brick / alien hit deactivate (and destroy / damage the target). Off
 * the top of the playfield also deactivates. Treats bullet as a point;
 * collision uses the same brick grid lookup as the ball but without
 * bounce-axis logic — the bullet stops dead on contact regardless of
 * brick type. */

static void step_bullet_one(int b) {
    Object *enemy = &objects[OBJ_ENEMY];
    const BulletHit hit = bullet_advance(b, *enemy, BrickField(live_level));

    if (hit.what == BulletHit::ENEMY) {
        /* The alien becomes its own 5-frame blast, centred on itself.
         * orig: kill_enemy $A4C4 / $A4D2 */
        enemy->x_coord   = u8(enemy->x_coord + int(enemy->w_body_px) / 2 - 8);
        enemy->y_coord   = u8(enemy->y_coord + 4);
        enemy->w_body_px = 16;
        enemy->h_body_px = 13;
        enemy->sprite_set = 0x0A;
        enemy->sprite_num = 0;
        enemy->misc_12    = 0x50;
        score += 350;
        sound_queue(SND_ALIEN_BLAST);
        return;
    }

    if (hit.what == BulletHit::BRICK) {
        /* Same damage rules as a ball hit, minus the click: the original
         * skips sound_normall_brik when the colliding object is a bullet
         * (sprite_set $05), because the impact blast is the feedback. */
        unsigned char *cell = &live_level[hit.row * LVL_COLS + hit.col];
        if (*cell & 0x20) {                       /* undestructible */
            brick_hit_anim_spawn(hit.col, hit.row);
        } else if (!(*cell & 0x10)) {             /* multi-hit, first hit */
            *cell |= 0x10;
            brick_hit_anim_spawn(hit.col, hit.row);
        } else {
            unsigned int idx = (unsigned int)((hit.row < 12) ? hit.row : 11);
            unsigned int pts = points_table[idx];
            if ((*cell & 0x0F) >= 6) pts *= 2;    /* metal scores double */
            score += pts;
            *cell |= 0x80;
            mark_brick_row_dirty(hit.row);
            brick_flash_spawn(hit.col, hit.row);
            try_spawn_bonus(hit.col, hit.row);
        }
    }
}

static void step_bullet(void) {
    int i;
    for (i = 0; i < N_BULLETS; i++) step_bullet_one(i);
}

/* Fire one laser bullet if the bat carries the LASER bonus, the cooldown
 * has expired, and a slot is free. Port of free_bullet_2 ($A14C) + the
 * $A12C cooldown gate. Called from the SPACE handler and (under
 * BATTY_AUTO_FIRE) once per frame. The cooldown == 0 check is BEFORE the
 * end-of-frame `-= 2`, so the 0x18 reset yields the original's 12-frame
 * cadence (see the long note at the old call site / notes/laser.md). */
static void try_fire_laser(void) {
    int free_slot = -1;
    int j;
    if (rocket_active
        || objects[OBJ_BAT_1].bonus_applied != 0x01
        || bullet_cooldown != 0) return;
    for (j = 0; j < N_BULLETS; j++) {
        if (!bullet_active[j]) { free_slot = j; break; }
    }
    if (free_slot < 0) return;
    bullet_active[free_slot] = 1;
    bullet_x[free_slot] = BAT_X + 12;
    bullet_y[free_slot] = BAT_Y - 1;
    bat_fire_anim_ticks = 8;
    bullet_cooldown = 0x18;          /* 12 frames @ -2 / frame */
    dbg_shots_fired++;
    sound_queue(SND_SHOT);
}

/* Step the bullet-impact blasts one tick each. Per-slot countdown
 * matches the per-slot bullet that spawned each blast. */

/* Paint each active bullet-blast frame at its recorded impact point.
 * Frame index = (BULLET_BLAST_FRAMES - 1) - (ticks / ticks_per_frame)
 * so the animation plays start -> end as the counter winds down. */
static void render_bullet_blast_to_buff(void) {
    static const unsigned int frames[BULLET_BLAST_FRAMES] = {
        SPR_BULLET_BLAST_1, SPR_BULLET_BLAST_2,
        SPR_BULLET_BLAST_3, SPR_BULLET_BLAST_4
    };
    int i;
    /* No per-cell attr override — same "monochrome except blocks"
     * rule as bullets / bonus / rocket. */
    for (i = 0; i < N_BULLETS; i++) {
        unsigned char frame;
        if (!bullet_blast_ticks[i]) continue;
        frame = (unsigned char)((bullet_blast_ticks[i] - 1) / BULLET_BLAST_TICKS_PER_FRAME);
        if (frame >= BULLET_BLAST_FRAMES) frame = BULLET_BLAST_FRAMES - 1;
        frame = (unsigned char)((BULLET_BLAST_FRAMES - 1) - frame);
        blit_masked_to_scr_buff(frames[frame], bullet_blast_x[i], bullet_blast_y[i]);
    }
}

/* True if any bullet-blast slot is still rendering an animation. */

/* True if any bullet slot is in flight — used by the inner loop to
 * decide whether to redraw and to expose firing capacity to SPACE. */

/* Step the rocket one frame: move up (the original handling_rocket accel),
 * lifting the bat. The original (LBB97 flight loop) does NO brick
 * destruction — the rocket flies over the INTACT brick field; the bricks
 * are awarded + cleared at fly-off by play_rocket_award_tally. The dirty
 * redraw restores the bricks behind the rocket from scr_buff each frame.
 * Deactivate when the rocket leaves the top of the playfield. */
/* End-of-level brick-points tally (port of add_points_for_left_briks
 * $AF0D): tick the remaining bricks' points up one-by-one with the scene
 * + score on screen (bricks stay visible), then clear them so
 * live_bricks_remaining()==0 advances the level. Defined after the
 * scene-redraw helpers; forward-declared here for step_rocket. */
static void play_rocket_award_tally(unsigned char level_idx);

static void step_rocket(void) {
    if (!rocket_active) return;
    /* Port of handling_rocket at $A89A:
     *   HL = LA8CF - $20
     *   if counter_misc >= $38, persist HL back to LA8CF
     *   y/fract = HL + (current_y:LA8D1)
     * This gives a slow initial lift followed by acceleration instead
     * of a constant-pixel rocket climb. */
    {
        unsigned int hl;
        unsigned int sum;
        rocket_counter++;
        hl = (unsigned int)(rocket_acc - 0x0020u);
        if (rocket_counter >= 0x38) rocket_acc = hl;
        sum = (unsigned int)(hl + (((unsigned int)(unsigned char)rocket_y) << 8) + rocket_frac);
        rocket_frac = (unsigned char)sum;
        rocket_y = (int)(unsigned char)(sum >> 8);
        /* handling_rocket writes rocket_y - 6 into both bat objects,
         * so the rocket pack stays attached and lifts the bat. */
        BAT_Y = (unsigned char)(rocket_y - 6);
        objects[OBJ_BAT_2].y_coord = BAT_Y;
    }
    if (rocket_y >= PLAYFIELD_H || rocket_y + ROCKET_H_PX < 0) {
        rocket_active = 0;
        rocket_clear_completed = 1;
        /* Mirror LBB97 → LBBFB → add_points_for_left_briks: tick up the
         * points for every remaining brick (bricks stay on screen), then
         * clear them to end the level. */
        play_rocket_award_tally(current_level_idx_var);
        return;
    }
    /* No brick destruction during flight (port of the destruction-free
     * LBB97 loop): the rocket flies over the intact bricks, which the
     * dirty redraw restores behind it. They are awarded + cleared at
     * fly-off above. (Was a bbox sweep that carved a tunnel — a port-ism
     * the original does not have; see notes/rocket-flight.md.) */
}

/* --- Exact bat deflection (port of LAB1F @ $AB1F) ---------------------
 *
 * Replaces the old 5-zone approximation. The original snaps the ball to
 * the bat top, computes offset = ball_x + 3 - bat_x (IY+$02), walks a
 * (threshold,zone) table, optionally reflects the direction, then looks
 * up the outgoing dir in LAC0A indexed by (zone&3) and the incoming dir.
 * Full decode + captured ground truth: notes/bat-deflection.md. The
 * offline check in that note confirms this reproduces every captured
 * datapoint (incoming 0x0C: offset -3->0x28, 5->0x2C, 13->0x34,
 * 21->0x38, 29->0x38). */



/* LAB1F_11: index of a downward dir within {04,08,0C,14,18,1C} (A starts
 * at 4, +4 each step, skipping 0x10). Returns 0..5, or -1 for a dir not
 * in the set — only pure-vertical 0x10 / non-multiple-of-4 dirs, which
 * the original assumes never reach the bat (it would loop forever). The
 * caller treats -1 as a plain vertical reflect so the port never hangs. */

/* Port of LAB1F_4..LAB1F_12: outgoing dir for a normal (non-catch) bat
 * bounce. big_bat picks the LABFC threshold table. */

/* Step the ball one frame: handle wall + bat collisions. If the ball
 * exits the bottom of the playfield it respawns stuck on the bat. */
/* Port of handling_ball LA27E_22 ($A6F2): advance the ball speed-up
 * ramp once per frame. With C = counter_misc, object+$13 increments
 * when (counter_misc & 7) == 0 (the block is only reached when
 * counter_misc & 3 == 0), and at $94 it resets and speed (+$07)
 * increments, capped at $06. We use one shared counter that bumps
 * every active ball — all balls share the counter_misc phase and the
 * TRIPLE_BALL spawn copies the primary's (already-ramped) speed, so
 * they stay in step. Called once per frame during active play. */
static void ball_speed_ramp_tick(void) {
    if ((pit_frame_counter & 7UL) != 0) return;
    if (++ball_speed_ramp != 0x94) return;
    ball_speed_ramp = 0;
    if (objects[OBJ_BALL_1].speed < 6) objects[OBJ_BALL_1].speed++;
    if (ball2_active && objects[OBJ_BALL_2].speed < 6) objects[OBJ_BALL_2].speed++;
    if (ball3_active && objects[OBJ_BALL_3].speed < 6) objects[OBJ_BALL_3].speed++;
}

/* ---- Magnet ball physics — port of handling_ball's LA27E_0..11 ------- */

/* obj_compare ($AC22) against magnet slot i: the slot's stored origin is
 * the paint origin +5 on both axes (print_magnets' post-draw ADD $05s),
 * body 15x14 px (slot +$0C/+$0D). Carry = overlap. Note the original's
 * asymmetry: strict `<` against the ball's body when the magnet is to
 * the right/below, `<=` against the magnet's body otherwise. */
static int magnet_ball_overlap(const Object *o, unsigned char i) {
    unsigned char mx = (unsigned char)(magnet_px[i] + 5);
    unsigned char my = (unsigned char)(magnet_py[i] + 5);
    if (mx >= o->x_coord) {
        if ((unsigned char)(mx - o->x_coord) >= o->w_body_px) return 0;
    } else {
        if ((unsigned char)(o->x_coord - mx) > MAGNET_BODY_W) return 0;
    }
    if (my >= o->y_coord) {
        if ((unsigned char)(my - o->y_coord) >= o->h_body_px) return 0;
    } else {
        if ((unsigned char)(o->y_coord - my) > MAGNET_BODY_H) return 0;
    }
    return 1;
}

/* Captured-frame move (the LA27E_4 keep-captured path): move the ball
 * with the CURVED dir (LAD69) and clamp to the margins WITHOUT
 * reflecting (check_margins $AC6C — a captured ball hugs the wall, it
 * doesn't bounce), then run the brick collision as if the dir were the
 * quantized EXIT dir (the original temporarily swaps +$06, CALLs
 * LA27E_24, and keeps the collision's dir only if it changed). The
 * LA27E_24 bat-contact and bottom-exit checks are unreachable while
 * captured (the deepest magnet box ends at y~165 < the y>=167 bat
 * contact and far above y>=192) and are omitted; ball-vs-enemy contact
 * runs in the main loop regardless of this path. */
static void magnet_captured_move(Object *o, unsigned char exit_dir) {
    int dx_q8, dy_q8, next_x, next_y, hit;
    long nx_q8, ny_q8;
    unsigned char curved = o->dir;
    int x_max = 0xF8 - (int)o->w_body_px;   /* check_right_margin */
    dir_to_dxdy(o->dir, o->speed, &dx_q8, &dy_q8);
    nx_q8 = ((long)o->x_coord << 8) + o->x_coord_hi + dx_q8;
    ny_q8 = ((long)o->y_coord << 8) + o->y_coord_hi + dy_q8;
    next_x = (int)(nx_q8 >> 8);
    next_y = (int)(ny_q8 >> 8);
    if (next_x < BALL_X_MIN)  { next_x = BALL_X_MIN;  nx_q8 = (long)next_x << 8; }
    else if (next_x > x_max)  { next_x = x_max;       nx_q8 = (long)next_x << 8; }
    if (next_y < BALL_Y_TOP)  { next_y = BALL_Y_TOP;  ny_q8 = (long)next_y << 8; }
    o->dir = exit_dir;
    hit = laffc_collision(o, o->x_coord, o->y_coord, next_x, next_y);
    if (hit == 0) hit = brick_collision(o->x_coord, o->y_coord, next_x, next_y);
    if (hit == 3) {
        nx_q8 = ((long)o->x_coord << 8) | (nx_q8 & 0xFF);
        ny_q8 = ((long)o->y_coord << 8) | (ny_q8 & 0xFF);
        next_x = o->x_coord; next_y = o->y_coord;
    } else if (hit == 1) {
        object_reflect(*(o), 0, 1);
        next_y = o->y_coord;
        ny_q8 = ((long)o->y_coord << 8) + o->y_coord_hi;
    } else if (hit == 2) {
        object_reflect(*(o), 1, 0);
        next_x = o->x_coord;
        nx_q8 = ((long)o->x_coord << 8) + o->x_coord_hi;
    }
    if (o->dir == exit_dir) o->dir = curved;   /* no collision: stay curved */
    o->x_coord = (unsigned char)next_x;
    o->y_coord = (unsigned char)next_y;
    o->x_coord_hi = (unsigned char)(nx_q8 & 0xFF);
    o->y_coord_hi = (unsigned char)(ny_q8 & 0xFF);
}

/* Per-frame magnet state machine for one ball — the block at the top of
 * handling_ball (LA27E_0..11). si = 0/1/2 for OBJ_BALL_1/2/3 (the
 * original's LA270/LA274/LA278 blocks). Returns 1 when the frame was
 * fully handled (ball captured: curved move done — skip the normal
 * step), 0 to proceed with the normal step (a release rewrites dir to
 * the quantized exit first; a fresh capture only registers state — the
 * curving starts NEXT frame, like the original). */
static int magnet_ball_frame(Object *o, unsigned char si) {
    if (ball_mag_cool[si]) {           /* post-release re-capture cooldown */
        ball_mag_cool[si]--;
        return 0;
    }
    if (ball_mag_delta[si]) {          /* captured: curve the trajectory */
        unsigned char dir = (unsigned char)((o->dir + ball_mag_delta[si]) & 0x3F);
        unsigned char ex;
        unsigned char mi = ball_mag_idx[si];
        o->dir = dir;
        /* Quantized exit dir, recomputed every captured frame:
         * (dir+2) & $3C, nudged ±4 off the pure up/down/left/right
         * codes (LA27E_1..3). Always a multiple of 4. */
        ex = (unsigned char)((dir + 2) & 0x3C);
        if ((ex & 0x0F) == 0) {
            if (dir & 0x0C) ex = (unsigned char)((ex - 4) & 0x3F);
            else            ex = (unsigned char)((ex + 4) & 0x3F);
        }
        ball_mag_exit[si] = ex;
        if (!magnet_on_state[mi] || !magnet_ball_overlap(o, mi)) {
            /* LA27E_5: release — exit dir, 2-frame cooldown, then the
             * NORMAL move/collision path runs this frame (LA27E_23). */
            ball_mag_cool[si]  = 2;
            ball_mag_delta[si] = 0;
            o->dir = ex;
            return 0;
        }
        magnet_captured_move(o, ex);
        return 1;
    }
    /* Free: scan ON magnets for a capture (LA27E_6..11). First overlap
     * wins; delta = ±1 from the dir quadrant XOR the ball-above-centre
     * test ((IY+$04)+4 vs ball y). */
    {
        unsigned char i;
        for (i = 0; i < magnet_count; i++) {
            unsigned char b = 0;
            if (!magnet_on_state[i]) continue;       /* BIT 0,(IY+$01) */
            if (!magnet_ball_overlap(o, i)) continue;
            if (((o->dir + 0x10) & 0x3F) >= 0x20) b = 0xFE;
            if ((unsigned char)(magnet_py[i] + 5 + 4) >= o->y_coord) b ^= 0xFE;
            ball_mag_delta[si] = (unsigned char)(0xFF ^ b);  /* $FF or $01 */
            ball_mag_idx[si]   = i;
            break;
        }
    }
    return 0;
}

static void magnet_ball_state_clear(unsigned char si) {
    /* LA27E_25 bottom-exit / LBC10: zero the cooldown + delta bytes. */
    ball_mag_cool[si]  = 0;
    ball_mag_delta[si] = 0;
}

static void step_ball(void) {
    int next_x, next_y;
    int dx_q8, dy_q8;
    long next_x_q8, next_y_q8;
    int bat_left  = eff_bat_left();
    int bat_right = eff_bat_right();
    int bat_top   = BAT_Y;
    int ball_sz   = eff_ball_size();
    if (ball_stuck) {
        BALL_X = BAT_X + stuck_offset_x;
        /* Rest the ball ON the bat: bottom row touches the bat top, i.e.
         * BAT_Y - BALL_H_PX = 166 = $A6 (matches the original's launch
         * rest at LA27E_15 and respawn_primary_ball). Using ball_sz (= 8
         * width) here put it 1px high (165) every frame, silently
         * clobbering respawn_primary_ball's correct $A6. A ball HELD by
         * the MAGNET bonus rests 1px lower at $A7 = 167 (LAB1F_3 uses $A7
         * for a caught ball vs $A6 for the launch rest); the bat's active
         * bonus ($03 == MAGNET, the original's IY+$14) distinguishes the
         * two without a separate caught-state flag. */
        BALL_Y = BAT_Y - BALL_H_PX +
                 (objects[OBJ_BAT_1].bonus_applied == 0x03 ? 1 : 0);
        objects[OBJ_BALL_1].x_coord_hi = 0;
        objects[OBJ_BALL_1].y_coord_hi = 0;
        return;
    }
    /* Magnet capture/curve/release (LA27E_0..11) — runs before the
     * normal move, may rewrite dir (release) or fully handle the frame
     * (captured). A stuck ball can't overlap a magnet box (boxes end
     * far above the bat), so running this after the stuck early-out
     * matches the original's effective behaviour. */
    if (magnet_ball_frame(&objects[OBJ_BALL_1], 0)) return;
    dir_to_dxdy(objects[OBJ_BALL_1].dir, objects[OBJ_BALL_1].speed,
                      &dx_q8, &dy_q8);
    next_x_q8 = ((long)BALL_X << 8) + objects[OBJ_BALL_1].x_coord_hi + dx_q8;
    next_y_q8 = ((long)BALL_Y << 8) + objects[OBJ_BALL_1].y_coord_hi + dy_q8;
    next_x = (int)(next_x_q8 >> 8);
    next_y = (int)(next_y_q8 >> 8);
    ball_dx = (dx_q8 < 0) ? -1 : (dx_q8 > 0 ? 1 : 0);
    ball_dy = (dy_q8 < 0) ? -1 : (dy_q8 > 0 ? 1 : 0);
    /* Side walls: port the original change_direction masks. */
    {
        int x_max = PLAYFIELD_W - 8 - ball_sz;   /* 244 normal, 240 big */
        if (next_x < BALL_X_MIN) {
            next_x = BALL_X_MIN;
            next_x_q8 = (long)next_x << 8;
            ball_reflect_descriptor(1, 0);
        } else if (next_x > x_max) {
            next_x = x_max;
            next_x_q8 = (long)next_x << 8;
            ball_reflect_descriptor(1, 0);
        }
    }
    if (next_y < BALL_Y_TOP) {
        next_y = BALL_Y_TOP;
        next_y_q8 = (long)next_y << 8;
        ball_reflect_descriptor(0, 1);
    }
    /* Bat top: ball moving down, ball overlaps bat in X. Use a 5-zone
     * deflection so the ball gains horizontal control from where the
     * player intercepts it - the classic brick-breaker mechanic. */
    if (ball_dy > 0
        && next_y + BALL_H_PX > bat_top
        && next_y < bat_top
        && next_x + ball_sz > bat_left
        && next_x < bat_right) {
        int hit_x = (next_x + ball_sz / 2) - bat_left;
        int span  = bat_right - bat_left;
        /* Y-dimension uses the ball HEIGHT (7), not the width (eff_ball_size
         * = 8), and a STRICT `>`: the original fires LAB1F when obj_compare
         * reports Y overlap, which (LAC22: 166 - ball_y borrows) is exactly
         * ball_y >= 167, i.e. next_y + 7 > bat_top(173). Matching the fire
         * frame is what makes the ball x - hence the deflection zone -
         * match the Spectrum. Firing at >= (y=166) or using the width fired
         * one frame early at a smaller x, shifting the zone on shallow
         * descents (e.g. dir 0x08 left of centre gave 0x24 not 0x28). The
         * resting ball then snaps to $A6 = bat_top - 7 = 166. See
         * notes/bat-deflection.md. */
        next_y  = bat_top - BALL_H_PX;
        /* MAGNET/CATCH bonus (original BAT+$14 == $03, LAB1F_1..3): the
         * ball sticks on contact and waits for FIRE to release. Only a
         * NORMAL-width bat catches (the original gates on width $1C; a
         * big bat falls through to the normal deflection). The caught
         * offset is QUANTIZED: offset = ball_x - bat_x, clamped >=0, then
         * `& 0xFC` (multiple of 4) and clamped to 0x18 - so the rest x
         * (= bat_x + offset) and the launch direction derived from it
         * match the Spectrum (probed: ball_x 133 -> offset 0x10 -> rest
         * x 132). The original then snaps the ball to y=$A7=167. */
        if (objects[OBJ_BAT_1].bonus_applied == 0x03 && bat_extra_px == 0) {
            int off = next_x - BAT_X;
            if (off < 0) off = 0;
            off &= 0xFC;
            if (off >= 0x19) off = 0x18;
            stuck_offset_x  = off;
            ball_stuck      = 1;
            stuck_ticks     = 0;
            ball_dy         = -BALL_SPEED;
            objects[OBJ_BALL_1].dir = 0x20;
            BALL_X          = BAT_X + off;
            /* A MAGNET-caught ball rests 1px lower than the launch rest:
             * LAB1F_3 sets y = $A7 = 167 (= bat_top - BALL_H_PX + 1),
             * vs $A6 = 166 for the level-start / launch rest. */
            BALL_Y          = bat_top - BALL_H_PX + 1;
            objects[OBJ_BALL_1].x_coord_hi = 0;
            objects[OBJ_BALL_1].y_coord_hi = 0;
            sound_queue(SND_BAT_BEAT);
            return;
        }
        ball_dy = -BALL_SPEED;
        /* Exact LAB1F deflection (replaces the 5-zone approximation).
         * offset = ball_x + 3 - bat_x (the bat object's left edge BAT_X,
         * = original IY+$02); an enlarged bat selects the LABFC table.
         * See notes/bat-deflection.md (validated vs captured ground
         * truth). hit_x/span retained only for the catch branch above. */
        (void)hit_x; (void)span;
        objects[OBJ_BALL_1].dir =
            bat_deflect_dir(objects[OBJ_BALL_1].dir,
                            next_x + 3 - BAT_X, bat_extra_px != 0);
        dir_to_dxdy(objects[OBJ_BALL_1].dir, objects[OBJ_BALL_1].speed,
                          &dx_q8, &dy_q8);
        ball_dx = (dx_q8 < 0) ? -1 : (dx_q8 > 0 ? 1 : 0);
        ball_dy = (dy_q8 < 0) ? -1 : (dy_q8 > 0 ? 1 : 0);
        sound_queue(SND_BAT_BEAT);            /* ball-on-bat */
    }
    /* Past the bat (= primary ball lost). Original at LA27E_25 ($A4xx)
     * checks Y >= $C0 (= 192). It deactivates the ball and decrements
     * balls_quantity; only when balls_quantity reaches 0 next frame
     * does LBC10 fire the bat-explosion + lives--. Multi-ball thus
     * lets the player survive the primary ball's fall as long as an
     * extra is still in flight.
     *
     * Mirror: if any extra ball is active, just hide the primary and
     * keep playing. Otherwise run the death animation, decrement
     * lives, and respawn primary stuck on the bat. */
    if (next_y >= PLAYFIELD_H) {
        magnet_ball_state_clear(0);          /* LA27E_25 zeroes the LA270 pair */
        if (ball2_active || ball3_active) {
            BALL_HIDE();
            return;
        }
        play_bat_explosion(current_level_idx_var);
        if (lives > 0) lives--;
        if (lives > 0) respawn_primary_ball();
        return;
    }
    /* Brick collision: side-aware. brick_collision tells us which axis
     * the ball entered through; we reverse + unwind that axis. */
    {
        int hit;
        if (use_laffc) {
            /* LAFFC-exact bounce where it fires (returns 3 = handled, or 0
             * = no hit). Fall back to the proven brick_collision when LAFFC
             * reports no hit, so the byte-exact path can never pass through
             * a brick it failed to resolve (e.g. an unported two-cell
             * straddle on a layout other than L3). On L3 LAFFC handles the
             * hit, so the fallback never triggers and parity is unchanged. */
            hit = laffc_collision(&objects[OBJ_BALL_1], BALL_X, BALL_Y, next_x, next_y);
            if (hit == 0) hit = brick_collision(BALL_X, BALL_Y, next_x, next_y);
        } else {
            hit = brick_collision(BALL_X, BALL_Y, next_x, next_y);
        }
        if (hit == 3) {
            /* LAFFC path already reflected the direction and snapped the
             * ball to the cell edge (in BALL_X/BALL_Y). LAFFC_26-29 set
             * only the pixel byte (IX+$02 / IX+$04) and LEAVE the q8.8
             * fraction from the move untouched, so keep the moved low
             * byte rather than zeroing it — matching the original's
             * sub-pixel accumulation (probed: x frac 9, y frac 72). */
            next_x_q8 = ((long)BALL_X << 8) | (next_x_q8 & 0xFF);
            next_y_q8 = ((long)BALL_Y << 8) | (next_y_q8 & 0xFF);
            next_x = BALL_X;
            next_y = BALL_Y;
        } else if (hit == 1) {
            ball_reflect_descriptor(0, 1);
            next_y = BALL_Y;
            next_y_q8 = ((long)BALL_Y << 8) + objects[OBJ_BALL_1].y_coord_hi;
        } else if (hit == 2) {
            ball_reflect_descriptor(1, 0);
            next_x = BALL_X;
            next_x_q8 = ((long)BALL_X << 8) + objects[OBJ_BALL_1].x_coord_hi;
        }
    }
    BALL_X = next_x;
    BALL_Y = next_y;
    objects[OBJ_BALL_1].x_coord_hi = (unsigned char)(next_x_q8 & 0xFF);
    objects[OBJ_BALL_1].y_coord_hi = (unsigned char)(next_y_q8 & 0xFF);
}

/* Step the secondary (multi-ball) one frame. Simpler than step_ball:
 * no stuck phase, no life decrement on bottom-exit — the slot just
 * deactivates. Bat bounce reuses the same 5-zone deflection. Wall +
 * brick collision shared with step_ball semantics. */
/* Step an extra (TRIPLE_BALL) ball. UNIFIED with the primary: the
 * original runs ONE handling_ball for every ball, so the extras now use
 * the exact same q8.8 + dir motion (dir_to_dxdy), wall reflect
 * (reflect_obj_dir), brick collision (LAFFC), and bat deflection (LAB1F /
 * bat_deflect_dir) as step_ball — reading dir/q8.8 from the object table.
 * Only the primary's stuck/catch and life-decrement paths are omitted (an
 * extra just deactivates off the bottom). The legacy integer in_dx/in_dy
 * are no longer used. Correct by construction (reuses the byte-exact
 * primary path); validated by the liveness sweep + the primary ball gate
 * (unaffected). */
static void step_extra_ball(unsigned char *in_active,
                             int *in_dx, int *in_dy,
                             unsigned char obj_idx) {
    Object *o = &objects[obj_idx];
    int next_x, next_y, dx_q8, dy_q8, hit;
    long next_x_q8, next_y_q8;
    int bat_left  = eff_bat_left();
    int bat_right = eff_bat_right();
    int bat_top   = BAT_Y;
    int ball_sz   = eff_ball_size();
    int x_max     = PLAYFIELD_W - 8 - ball_sz;
    (void)in_dx; (void)in_dy;          /* legacy integer deltas unused now */
    if (!*in_active) return;
    /* Magnet capture/curve/release — the original runs ONE handling_ball
     * per ball, so extras share the exact same LA27E_0..11 block (their
     * own LA274/LA278 state). */
    if (magnet_ball_frame(o, (unsigned char)(obj_idx == OBJ_BALL_2 ? 1 : 2)))
        return;
    dir_to_dxdy(o->dir, o->speed, &dx_q8, &dy_q8);
    next_x_q8 = ((long)o->x_coord << 8) + o->x_coord_hi + dx_q8;
    next_y_q8 = ((long)o->y_coord << 8) + o->y_coord_hi + dy_q8;
    next_x = (int)(next_x_q8 >> 8);
    next_y = (int)(next_y_q8 >> 8);
    if (next_x < BALL_X_MIN)  { next_x = BALL_X_MIN; next_x_q8 = (long)next_x << 8; object_reflect(*(o), 1, 0); }
    else if (next_x > x_max)  { next_x = x_max;      next_x_q8 = (long)next_x << 8; object_reflect(*(o), 1, 0); }
    if (next_y < BALL_Y_TOP)  { next_y = BALL_Y_TOP; next_y_q8 = (long)next_y << 8; object_reflect(*(o), 0, 1); }
    /* Bat: LAB1F contact (ball_y >= 167) + exact deflection. No catch. */
    if (dy_q8 > 0
        && next_y + BALL_H_PX > bat_top
        && next_y < bat_top
        && next_x + ball_sz > bat_left
        && next_x < bat_right) {
        next_y = bat_top - BALL_H_PX;
        o->dir = bat_deflect_dir(o->dir, next_x + 3 - BAT_X, bat_extra_px != 0);
        sound_queue(SND_BAT_BEAT);
    }
    if (next_y >= PLAYFIELD_H) {        /* off the bottom: deactivate */
        magnet_ball_state_clear((unsigned char)(obj_idx == OBJ_BALL_2 ? 1 : 2));
        *in_active = 0;
        o->sprite_set = 0x82;
        return;
    }
    hit = laffc_collision(o, o->x_coord, o->y_coord, next_x, next_y);
    if (hit == 0) hit = brick_collision(o->x_coord, o->y_coord, next_x, next_y);
    if (hit == 3) {
        next_x_q8 = ((long)o->x_coord << 8) | (next_x_q8 & 0xFF);
        next_y_q8 = ((long)o->y_coord << 8) | (next_y_q8 & 0xFF);
        next_x = o->x_coord; next_y = o->y_coord;
    } else if (hit == 1) {
        object_reflect(*(o), 0, 1);
        next_y = o->y_coord;
        next_y_q8 = ((long)o->y_coord << 8) + o->y_coord_hi;
    } else if (hit == 2) {
        object_reflect(*(o), 1, 0);
        next_x = o->x_coord;
        next_x_q8 = ((long)o->x_coord << 8) + o->x_coord_hi;
    }
    o->x_coord = (unsigned char)next_x;
    o->y_coord = (unsigned char)next_y;
    o->x_coord_hi = (unsigned char)(next_x_q8 & 0xFF);
    o->y_coord_hi = (unsigned char)(next_y_q8 & 0xFF);
}

static void step_ball2(void) {
    step_extra_ball(&ball2_active, &ball2_dx, &ball2_dy, OBJ_BALL_2);
}

static void step_ball3(void) {
    step_extra_ball(&ball3_active, &ball3_dx, &ball3_dy, OBJ_BALL_3);
}

/* M3 minimal play loop. For each level: full render once, then poll
 * arrows for bat motion (LEFT/RIGHT = +-4 px, matching the original's
 * handling_bat step). Any non-arrow key advances; ESC quits.
 *
 * In auto-advance mode (the default attract loop) the per-level
 * timeout still trips, so the cycle keeps moving even with no input.
 * Under BATTYALL=1 (test floppy) auto-advance is off and the test
 * orchestrator drives every transition via sendkey. */
/* Redraw the bat's object window only. Original print_obj_from_buf_to_scr
 * computes a byte-aligned union of previous/current object bounds, then
 * recovers that window from scr_buff instead of repainting the whole
 * row band. Keep the same shape here: restore the old/current bat
 * footprint, re-blit the bat/running dot, and flush only those bytes.
 * Side-frame bytes are therefore touched only when the bat actually
 * overlaps them. */
static void redraw_bat(unsigned char cycle, unsigned char bg_attr) {
    int old_x0, old_x1, new_x0, new_x1;
    int byte_lo, byte_hi;
    int y;
    bat_sprite_bounds(BAT_PREV_X, bat_draw_extra_px, &old_x0, &old_x1);
    bat_sprite_bounds(BAT_X, bat_extra_px, &new_x0, &new_x1);
    if (new_x0 < old_x0) old_x0 = new_x0;
    if (new_x1 > old_x1) old_x1 = new_x1;
    byte_lo = old_x0 >> 3;
    byte_hi = (old_x1 + 7) >> 3;
    byte_lo--;
    byte_hi++;
    if (byte_lo < 0) byte_lo = 0;
    if (byte_hi > 32) byte_hi = 32;
    if (byte_lo >= byte_hi) return;

    paint_bg_window_to_buff(bg_attr, cycle, BAT_Y, BAT_H_PX,
                            byte_lo, byte_hi - 1);
    paint_frame_to_buff(cycle, current_level_idx_var);
    render_bat(cycle, bg_attr);
    render_running_dot();
    render_lives(cycle, bg_attr);
    buff_to_vga_rect_bytes(BAT_Y, BAT_H_PX, byte_lo, byte_hi - 1);
    for (y = BAT_Y; y < BAT_Y + BAT_H_PX; y++) {
        prev_dirty_min_byte[0][y] = (unsigned char)byte_lo;
        prev_dirty_max_byte[0][y] = (unsigned char)(byte_hi - 1);
        prev_dirty_min_byte[1][y] = DIRTY_NONE;
        prev_dirty_max_byte[1][y] = 0;
    }
    remember_bat_draw_state();
}

static void render_hud_to_buff(void);

static void carry_dirty_with_previous(void) {
    int y;
    /* The new prev = THIS frame's marks (current_*), so track their pixel-row
     * span for next frame's restore to scan only those rows, not all 192. */
    int y_lo = PLAYFIELD_H;
    int y_hi = -1;
    /* Only rows where the current OR last-prev set is dirty need carrying;
     * everything else is already NONE in both. Scan that union, not all 192. */
    int scan_lo = (cur_dirty_y_lo < prev_dirty_y_lo) ? cur_dirty_y_lo : prev_dirty_y_lo;
    int scan_hi = (cur_dirty_y_hi > prev_dirty_y_hi) ? cur_dirty_y_hi : prev_dirty_y_hi;
    if (scan_lo < 0) scan_lo = 0;
    if (scan_hi > PLAYFIELD_H - 1) scan_hi = PLAYFIELD_H - 1;
    for (y = scan_lo; y <= scan_hi; y++) {
        unsigned char current_min0 = dirty_min_byte[0][y];
        unsigned char current_max0 = dirty_max_byte[0][y];
        unsigned char current_min1 = dirty_min_byte[1][y];
        unsigned char current_max1 = dirty_max_byte[1][y];
        int s;
        for (s = 0; s < DIRTY_SLOTS; s++) {
            if (prev_dirty_min_byte[s][y] != DIRTY_NONE) {
                mark_dirty_byte_row(dirty_min_byte, dirty_max_byte, y,
                                    prev_dirty_min_byte[s][y],
                                    prev_dirty_max_byte[s][y]);
            }
        }
        prev_dirty_min_byte[0][y] = current_min0;
        prev_dirty_max_byte[0][y] = current_max0;
        prev_dirty_min_byte[1][y] = current_min1;
        prev_dirty_max_byte[1][y] = current_max1;
        if (current_min0 != DIRTY_NONE || current_min1 != DIRTY_NONE) {
            if (y < y_lo) y_lo = y;
            if (y > y_hi) y_hi = y;
        }
    }
    prev_dirty_y_lo = y_lo;
    prev_dirty_y_hi = y_hi;   /* hi < lo when nothing dirty -> restore scans none */
}

/* Full-frame compose. Walks the same scr_buff -> attr_buff -> VGA
 * path as the original (game_screen_draw_to_buffer @ $BE6B):
 *   - paint bg + bricks + bat + lives into scr_buff/attr_buff
 *   - paint ball, bomb, 400pts, alien into scr_buff (each picks up
 *     its surrounding char cell's bg attr at buff_to_vga time)
 *   - HUD labels/scores join scr_buff before the same buff_to_vga pass. */
static void redraw_full_with_ball(unsigned char level_idx) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle   = (unsigned char)(level_idx & 3);
    Object *enemy = &objects[OBJ_ENEMY];
    int y, i;
    int score_dirty;
    int lives_dirty;
    int can_local_hud;
    int bat_full_dirty;

    prof_start();

    score_dirty = (score != prev_score || high_score != prev_high_score);
    lives_dirty = (lives != prev_lives);
    can_local_hud = (magnets_per_level[level_idx][0] == 0);
    bat_full_dirty = (BAT_X != BAT_PREV_X)
                  || (BAT_Y != bat_draw_y)
                  || (bat_extra_px != bat_draw_extra_px)
                  || (objects[OBJ_BAT_1].bonus_applied != bat_draw_bonus_applied)
                  || (bat_fire_anim_ticks != bat_draw_fire_ticks);
    if (force_full_flush || lives_dirty || (score_dirty && !can_local_hud)) {
        static_bg_dirty = 1;
    }

    /* Clear BEFORE the static branch so build_static_brick_band_cache's
     * window mark survives into this frame's flush. */
    clear_dirty_ranges(dirty_min_byte, dirty_max_byte);
    if (static_bg_dirty) {
        build_static_background(level_idx);
        static_bg_dirty = 0;
        prev_score = score;
        prev_high_score = high_score;
        prev_lives = lives;
        force_full_flush = 1;
    } else if (static_bg_cache_dirty) {
        build_static_brick_band_cache(level_idx);
        restore_prev_dirty_from_static_cache();
    } else {
        restore_prev_dirty_from_static_cache();
    }
    if (!static_bg_dirty && score_dirty && can_local_hud) {
        update_static_hud_top(level_idx);
        prev_score = score;
        prev_high_score = high_score;
        mark_dirty_bytes(0, FRAME_TOP_H_PX, 0, 31);
    }
    /* A magnet toggled this frame: redraw its circle now, while scr_buff
     * holds clean background in the window (objects not yet drawn), and
     * bake it into the static bg cache. After a full static rebuild the
     * blit is redundant (render_magnets painted from state) but
     * harmless — and the dirty mark is still needed. */
    apply_magnet_toggle_visual();
    /* Repair the top-frame centre (bytes 8..10, rows 0..23) BEFORE any
     * moving object is composed. This call used to sit at the END of the
     * compose (after the enemy/balls), where it ERASED the slice of any
     * sprite overlapping the frame centre — an alien or ball transiting
     * x 64..87 / y < 24 flickered out on every full-path frame (found
     * 2026-06-12 via the fly-over A/B harness, frame-12 247px diff vs
     * the dirty path, which correctly draws sprites over the frame). */
    restore_top_frame_center(cycle, level_idx);
    prof_bg_pit += prof_elapsed();

    /* The frame itself is static and baked into bg_scr_buff/bg_attr_buff. */
    prof_frame_pit += prof_elapsed();

    if (BALL_VISIBLE) {
        render_ball_to_buff(BALL_X, BALL_Y, bg_attr);
        mark_dirty_rect_px(BALL_X, BALL_Y, 16, 12);
    }
    if (bat_full_dirty) {
        int old_x0, old_x1;
        int byte_lo, byte_hi;
        bat_sprite_bounds(BAT_PREV_X, bat_draw_extra_px, &old_x0, &old_x1);
        byte_lo = old_x0 >> 3;
        byte_hi = (old_x1 - 1) >> 3;
        restore_static_cache_rect_bytes(bat_draw_y, 13, byte_lo, byte_hi);
        mark_dirty_rect_px(old_x0, bat_draw_y, old_x1 - old_x0, 13);
    }
    render_bat(cycle, bg_attr);
    render_running_dot();
    {
        int bat_x0, bat_x1, old_x0, old_x1;
        bat_sprite_bounds(BAT_X, bat_extra_px, &bat_x0, &bat_x1);
        if (bat_full_dirty) {
            bat_sprite_bounds(BAT_PREV_X, bat_draw_extra_px, &old_x0, &old_x1);
            if (old_x0 < bat_x0) bat_x0 = old_x0;
            if (old_x1 > bat_x1) bat_x1 = old_x1;
            mark_dirty_rect_px(bat_x0, BAT_Y, bat_x1 - bat_x0, 13);
        } else {
            mark_dirty_rect_px(bat_x0, BAT_Y + 6, bat_x1 - bat_x0, 1);
        }
    }
    remember_bat_draw_state();

    /* Lives and HUD are static in the cached background and are rebuilt
     * only when score/lives/brick-animation invalidation requires it. */
    prof_hud_pit += prof_elapsed();

    render_brick_flash_to_buff();
    if (brick_flash_ticks) {
        mark_dirty_rect_px(brick_flash_x - 1, brick_flash_y - 1, 18, 10);
    }
    render_brick_hit_anim_to_buff();
    if (any_brick_hit_anim()) {
        for (i = 0; i < BRICK_HIT_ANIM_SLOTS; i++) {
            if (brick_hit_anim_ticks[i]) {
                mark_dirty_rect_px(8 + (int)brick_hit_anim_col[i] * 16,
                                   32 + (int)brick_hit_anim_row[i] * 8,
                                   16, 8);
            }
        }
    }

    /* Moving objects in the ORIGINAL's slot-paint order ($9AD0 table,
     * call_for_all_obj walks it low->high, so later slots paint ON TOP):
     *   balls 1-3 < bullets < bats < bonus/bomb/pts400 (shared $9B80
     *   slot) < ENEMY ($9B96) < rocket ($9BAC).
     * The two compose paths used to disagree (simple drew enemy before
     * the bomb, full after) — with a fresh bomb still overlapping its
     * parent UFO the paths rendered different pixels (the f50 21px A/B
     * delta, notes/bird-render-parity.md). The enemy paints OVER the
     * bomb/bonus; the rocket paints over everything. */
    if (ball2_active) {
        render_ball_to_buff(objects[OBJ_BALL_2].x_coord,
                            objects[OBJ_BALL_2].y_coord, bg_attr);
        mark_dirty_rect_px(objects[OBJ_BALL_2].x_coord,
                           objects[OBJ_BALL_2].y_coord, 16, 12);
    }
    if (ball3_active) {
        render_ball_to_buff(objects[OBJ_BALL_3].x_coord,
                            objects[OBJ_BALL_3].y_coord, bg_attr);
        mark_dirty_rect_px(objects[OBJ_BALL_3].x_coord,
                           objects[OBJ_BALL_3].y_coord, 16, 12);
    }
    render_bullet_to_buff();
    for (i = 0; i < N_BULLETS; i++) {
        if (bullet_active[i]) {
            mark_dirty_rect_px(bullet_x[i], bullet_y[i], 8, 8);
        }
    }
    if (any_bullet_blast()) {
        render_bullet_blast_to_buff();
        for (i = 0; i < N_BULLETS; i++) {
            if (bullet_blast_ticks[i]) {
                mark_dirty_rect_px(bullet_blast_x[i], bullet_blast_y[i], 16, 8);
            }
        }
    }
    if (bomb_active) {
        blit_masked_to_scr_buff(spr_bomb_data, bomb_x, bomb_y);
        mark_dirty_rect_px(bomb_x, bomb_y, 16, 16);
    }
    if (pts_400_active) {
        blit_masked_to_scr_buff(pts_marker_spr, pts_400_x, pts_400_y);
        mark_dirty_sprite_rect(pts_marker_spr, pts_400_x, pts_400_y);
    }
    if (bonus_active) {
        unsigned int spr = spr_for_bonus(bonus_type);
        render_bonus_to_buff(bg_attr);
        mark_dirty_sprite_rect(spr, bonus_x, bonus_y);
    }
    if ((enemy->sprite_set & 0x7F) != 0 && !(enemy->sprite_set & 0x80)) {
        unsigned int spr;
        int spr_w_px, spr_h_px;
        if ((enemy->sprite_set & 0x7F) == 0x0A) {
            unsigned char frame = enemy->sprite_num;
            if (frame >= BLAST_FRAMES) frame = BLAST_FRAMES - 1;
            spr = spr_blast_frames[frame];
        } else {
            spr = (enemy->sprite_set == 0x09)
                ? spr_bird_frames[enemy->sprite_num & 7]    /* anim_bird 0..7 */
                : spr_ufo_frames[enemy->sprite_num % 10];  /* anim_ufo 0..9 */
        }
        spr_w_px = sprites_blob[spr]     * 8;
        spr_h_px = sprites_blob[spr + 1];
        /* The original draws the enemy with print_obj_to_buff ($B82C):
         * sprite PIXELS only, never print_sprite_attrib. So the bird/UFO
         * keeps each cell's underlying attr (bg over open texture, the
         * BRICK's attr over bricks) and renders in ZX colour-clash — the
         * bird over a red brick shows red, not the playfield background.
         * Recolouring to bg_attr here (known-bugs #7) wrongly repainted the
         * brick cells the bird flew over; verified vs the ZEsarUX oracle
         * (build/orig_flyover: bird cells == static GT). Gate:
         * test-enemy-attr-parity. (void) keeps the slot-order bg_attr arg. */
        (void)bg_attr;
        blit_masked_to_scr_buff(spr, enemy->x_coord, enemy->y_coord);
        mark_dirty_cell_rect_px(enemy->x_coord, enemy->y_coord,
                                spr_w_px, spr_h_px);
    }
    if (rocket_active) {
        unsigned int spr = current_rocket_spr();
        render_rocket_to_buff();
        mark_dirty_sprite_rect(spr, rocket_x, rocket_y);
    }
    /* (restore_top_frame_center used to run here — moved BEFORE the
     * object compose so it can't erase sprites overlapping the frame
     * centre; see the comment at the new call site above.) */
    prof_bricks_pit += prof_elapsed();

    if (force_full_flush) {
        for (y = 0; y < PLAYFIELD_H; y++) {
            int s;
            for (s = 0; s < DIRTY_SLOTS; s++) {
                prev_dirty_min_byte[s][y] = dirty_min_byte[s][y];
                prev_dirty_max_byte[s][y] = dirty_max_byte[s][y];
            }
        }
        mark_all_dirty();
        force_full_flush = 0;
    } else {
        /* Flush both the old sprite positions and the new sprite positions. */
        carry_dirty_with_previous();
    }
    flush_dirty_to_vga();
    prof_vga_pit += prof_elapsed();

    prof_frames_count++;
    prof_full_dynamic_frames++;
}

#define BALL_DIRTY_BLOCK_BAT      0x0001
#define BALL_DIRTY_BLOCK_STATIC   0x0002
#define BALL_DIRTY_BLOCK_HUD      0x0004
#define BALL_DIRTY_BLOCK_OBJECTS  0x0008
#define BALL_DIRTY_BLOCK_BRICKS   0x0010
#define BALL_DIRTY_BLOCK_BALLS    0x0020
#define BALL_DIRTY_BLOCK_BAT_FX   0x0040
#define BALL_DIRTY_BLOCK_FORCED   0x0080

static unsigned int ball_dirty_blockers(int bat_moved) {
    unsigned int blockers = 0;
    if (force_ball_full_redraw || force_bat_full_redraw) blockers |= BALL_DIRTY_BLOCK_FORCED;
    if (bat_moved) blockers |= BALL_DIRTY_BLOCK_BAT;
    /* A hidden primary can't be redrawn (nothing to draw) -> full path. A
     * STUCK ball, though, is visible and rides the bat at a known position
     * (BALL_X/Y set each frame in the stuck handler), so it redraws fine on
     * the dirty path like a moving ball — no need to force full (helps the
     * MAGNET-hold + pre-launch states). */
    if (!BALL_VISIBLE) blockers |= BALL_DIRTY_BLOCK_BALLS;
    if (static_bg_dirty || static_bg_cache_dirty || force_full_flush) blockers |= BALL_DIRTY_BLOCK_STATIC;
    if (score != prev_score || high_score != prev_high_score || lives != prev_lives) blockers |= BALL_DIRTY_BLOCK_HUD;
    if (bonus_active || pts_400_active || bomb_active || rocket_active) blockers |= BALL_DIRTY_BLOCK_OBJECTS;
    if (objects[OBJ_ENEMY].sprite_set != 0) blockers |= BALL_DIRTY_BLOCK_OBJECTS;
    if (any_bullet_active() || any_bullet_blast()) blockers |= BALL_DIRTY_BLOCK_OBJECTS;
    if (brick_flash_ticks || any_brick_hit_anim()) blockers |= BALL_DIRTY_BLOCK_BRICKS;
    /* Extra balls (multi-ball) are full moving sprites like the primary —
     * route them to the simple-object dirty tier (OBJECTS), not a full
     * recompose. big-ball is the PRIMARY ball with a different sprite of the
     * SAME 16×12 footprint (verified: SPR_BIG_BALL/SPR_BALL_NORMAL both
     * 2 bytes × 12 rows), already drawn by render_ball_to_buff + covered by
     * the primary's 16×12 dirty mark — so it needs no blocker at all. */
    if (ball2_active || ball3_active) blockers |= BALL_DIRTY_BLOCK_OBJECTS;
    /* Resize transitions still force a full frame (the bat changes width,
     * needing the vacated-area restore); the laser fire-anim is now handled
     * on the dirty path by redraw_bat_dirty, so it is no longer a blocker. */
    if (bat_extra_px != bat_extra_tgt) blockers |= BALL_DIRTY_BLOCK_BAT_FX;
    return blockers;
}

static void prof_note_ball_dirty_blockers(unsigned int blockers) {
    if (blockers & (BALL_DIRTY_BLOCK_BAT | BALL_DIRTY_BLOCK_FORCED))
        prof_ball_dirty_block_bat++;
    if (blockers & BALL_DIRTY_BLOCK_STATIC)
        prof_ball_dirty_block_static++;
    if (blockers & BALL_DIRTY_BLOCK_HUD)
        prof_ball_dirty_block_hud++;
    if (blockers & BALL_DIRTY_BLOCK_OBJECTS)
        prof_ball_dirty_block_objects++;
    if (blockers & BALL_DIRTY_BLOCK_BRICKS)
        prof_ball_dirty_block_bricks++;
    if (blockers & BALL_DIRTY_BLOCK_BALLS)
        prof_ball_dirty_block_balls++;
    if (blockers & BALL_DIRTY_BLOCK_BAT_FX)
        prof_ball_dirty_block_bat_fx++;
}

static int can_redraw_ball_with_simple_objects(unsigned int blockers) {
    if ((blockers & ~BALL_DIRTY_BLOCK_OBJECTS) != 0) return 0;
    if (!bonus_active && !pts_400_active && objects[OBJ_ENEMY].sprite_set == 0
        && !any_bullet_active() && !any_bullet_blast() && !bomb_active
        && !ball2_active && !ball3_active) return 0;
    if (rocket_active) return 0;
    /* The +400 catch popup renders correctly only via the full path; in the
     * simple tier it leaves a trail (drift + catch-frame transition — a
     * pre-existing latent issue). It is brief + low-frequency, so route its
     * frames to the full path. This also keeps the multi-ball CATCH frames
     * (which spawn the popup) on the full path; steady-state multi-ball
     * (after the popup falls off) still tiers. */
    if (pts_400_active) return 0;
    return 1;
}

static void render_enemy_to_buff_and_mark(unsigned char bg_attr) {
    Object *enemy = &objects[OBJ_ENEMY];
    unsigned int spr;
    int spr_w_px, spr_h_px;
    if ((enemy->sprite_set & 0x7F) == 0 || (enemy->sprite_set & 0x80)) return;
    if ((enemy->sprite_set & 0x7F) == 0x0A) {
        unsigned char frame = enemy->sprite_num;
        if (frame >= BLAST_FRAMES) frame = BLAST_FRAMES - 1;
        spr = spr_blast_frames[frame];
    } else {
        spr = (enemy->sprite_set == 0x09)
            ? spr_bird_frames[enemy->sprite_num & 7]    /* anim_bird 0..7 */
            : spr_ufo_frames[enemy->sprite_num % 10];  /* anim_ufo 0..9 */
    }
    spr_w_px = sprites_blob[spr] * 8;
    spr_h_px = sprites_blob[spr + 1];
    /* See the matching note in the full-compose path: the original blits
     * enemy PIXELS only (print_obj_to_buff), never recolours its cells, so
     * the sprite shows colour-clash in the underlying brick/bg attr
     * (known-bugs #7, gate test-enemy-attr-parity). */
    (void)bg_attr;
    blit_masked_to_scr_buff(spr, enemy->x_coord, enemy->y_coord);
    mark_dirty_cell_rect_px(enemy->x_coord, enemy->y_coord,
                            spr_w_px, spr_h_px);
}

static void render_simple_objects_to_buff_and_mark(unsigned char bg_attr) {
    /* Same slot-paint ORDER as redraw_full_with_ball and the original's
     * $9AD0 object table (later slots paint on top): balls < bullets <
     * bomb/bonus/pts400 (shared $9B80 slot) < ENEMY. The paths used to
     * disagree (this one drew the enemy FIRST), so a fresh bomb still
     * overlapping its parent UFO rendered differently on the dirty path
     * vs the full path — the f50 21px A/B delta
     * (notes/bird-render-parity.md). */
    /* Multi-ball extras: full 16×12 moving balls, same dirty treatment as
     * the primary (the carry erases last frame's position). */
    if (ball2_active) {
        render_ball_to_buff(objects[OBJ_BALL_2].x_coord,
                            objects[OBJ_BALL_2].y_coord, bg_attr);
        mark_dirty_rect_px(objects[OBJ_BALL_2].x_coord,
                           objects[OBJ_BALL_2].y_coord, 16, 12);
    }
    if (ball3_active) {
        render_ball_to_buff(objects[OBJ_BALL_3].x_coord,
                            objects[OBJ_BALL_3].y_coord, bg_attr);
        mark_dirty_rect_px(objects[OBJ_BALL_3].x_coord,
                           objects[OBJ_BALL_3].y_coord, 16, 12);
    }
    /* Laser bullets + impact blasts: small fast sprites, redrawable on the
     * dirty path like the ball. render_*_to_buff blit into scr_buff; mark
     * each live slot's rect (the carry restores last frame's position, so
     * the bullet's fast upward travel leaves no trail). */
    if (any_bullet_active()) {
        int i;
        render_bullet_to_buff();
        for (i = 0; i < N_BULLETS; i++) {
            if (bullet_active[i])
                mark_dirty_rect_px(bullet_x[i], bullet_y[i],
                                   BULLET_W_PX, BULLET_H_PX);
        }
    }
    if (any_bullet_blast()) {
        int i;
        render_bullet_blast_to_buff();
        for (i = 0; i < N_BULLETS; i++) {
            if (bullet_blast_ticks[i])
                mark_dirty_rect_px(bullet_blast_x[i], bullet_blast_y[i], 16, 12);
        }
    }
    /* Enemy bomb: a single falling sprite, same dirty treatment as a bonus
     * (the bat-collision kill is handled in step_bomb, not here). */
    if (bomb_active) {
        blit_masked_to_scr_buff(spr_bomb_data, bomb_x, bomb_y);
        mark_dirty_rect_px(bomb_x, bomb_y, 16, 16);
    }
    if (pts_400_active) {
        blit_masked_to_scr_buff(pts_marker_spr, pts_400_x, pts_400_y);
        mark_dirty_sprite_rect(pts_marker_spr, pts_400_x, pts_400_y);
    }
    if (bonus_active) {
        unsigned int spr = spr_for_bonus(bonus_type);
        render_bonus_to_buff(bg_attr);
        mark_dirty_sprite_rect(spr, bonus_x, bonus_y);
    }
    render_enemy_to_buff_and_mark(bg_attr);
}

/* Repaint + flush the (non-moving) bat on a dirty-redraw frame. Normally
 * only the 1px running-dot row needs refreshing (the bat body is static +
 * cached); but while the laser cannon fire-animation is playing the body
 * sprite changes each frame, so refresh + flush the whole 13px body. This
 * lets a fire-anim frame stay on the dirty path instead of forcing a full
 * recompose. (Bat MOVEMENT and resize transitions still take the full path
 * via the BAT / BAT_FX blockers.) */
static void redraw_bat_dirty(unsigned char cycle, unsigned char bg_attr) {
    int bat_x0, bat_x1;
    bat_sprite_bounds(BAT_X, bat_extra_px, &bat_x0, &bat_x1);
    if (bat_fire_anim_ticks) {
        paint_bg_window_to_buff(bg_attr, cycle, BAT_Y, BAT_H_PX,
                                bat_x0 >> 3, (bat_x1 - 1) >> 3);
        render_bat(cycle, bg_attr);
        render_running_dot();
        mark_dirty_rect_px(bat_x0, BAT_Y, bat_x1 - bat_x0, BAT_H_PX);
    } else {
        paint_bg_window_to_buff(bg_attr, cycle, BAT_Y + 6, 1,
                                bat_x0 >> 3, (bat_x1 - 1) >> 3);
        render_bat(cycle, bg_attr);
        render_running_dot();
        mark_dirty_rect_px(bat_x0, BAT_Y + 6, bat_x1 - bat_x0, 1);
    }
}

static void redraw_ball_only(unsigned char level_idx) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle = (unsigned char)(level_idx & 3);

    prof_start();
    restore_prev_dirty_from_static_cache();
    clear_dirty_ranges(dirty_min_byte, dirty_max_byte);
    prof_bg_pit += prof_elapsed();

    render_ball_to_buff(BALL_X, BALL_Y, bg_attr);
    mark_dirty_rect_px(BALL_X, BALL_Y, 16, 12);
    redraw_bat_dirty(cycle, bg_attr);
    prof_bricks_pit += prof_elapsed();

    carry_dirty_with_previous();
    flush_dirty_to_vga();
    prof_vga_pit += prof_elapsed();

    prof_frames_count++;
    prof_ball_only_frames++;
}

static void redraw_ball_with_simple_objects(unsigned char level_idx) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle = (unsigned char)(level_idx & 3);

    prof_start();
    restore_prev_dirty_from_static_cache();
    clear_dirty_ranges(dirty_min_byte, dirty_max_byte);
    prof_bg_pit += prof_elapsed();

    render_ball_to_buff(BALL_X, BALL_Y, bg_attr);
    mark_dirty_rect_px(BALL_X, BALL_Y, 16, 12);
    redraw_bat_dirty(cycle, bg_attr);
    render_simple_objects_to_buff_and_mark(bg_attr);
    prof_bricks_pit += prof_elapsed();

    carry_dirty_with_previous();
    flush_dirty_to_vga();
    prof_vga_pit += prof_elapsed();

    prof_frames_count++;
    prof_ball_object_frames++;
}

/* Render a short string of N character codes via draw_glyph, anchored
 * top-left at screen (x, y). `codes` follow the markup encoding:
 * 0..9 = digits, 0x0A..0x23 = A..Z (see notes/encoding.md). */
static void draw_text(int x, int y, unsigned char colour,
                      const unsigned char *codes, int n) {
    int i;
    for (i = 0; i < n; i++) draw_glyph(x + i * 8, y, colour, codes[i]);
}

/* Encode an unsigned long as 6 digit-codes (most significant first). */

#ifndef BATTY_SCORELESS_HUD
static void draw_score_digits_original(int x, int y, unsigned long value) {
    unsigned char digits[6];
    int i;
    score_to_digits(value, digits);
    for (i = 0; i < 6; i++) {
        const unsigned char *digit = hud_sprites + HUD_SCORE_DIGITS + 2 + digits[i] * 16;
        int row;
        for (row = 0; row < 8; row++) {
            int py = y + row;
            unsigned char mask = digit[row * 2];
            unsigned char pix = digit[row * 2 + 1];
            int bx = (x >> 3) + i;
            if (py < 0 || py >= PLAYFIELD_H || bx < 0 || bx >= 32) continue;
            scr_buff[py * 32 + bx] =
                (unsigned char)(((unsigned char)(~mask) & scr_buff[py * 32 + bx])
                                | (mask & pix));
        }
    }
}

static void render_hud_to_buff(void) {
    blit_masked_to_scr_buff(hud_sprites + HUD_SPR_1UP, 0x1C, 0x0C);
    blit_masked_to_scr_buff(hud_sprites + HUD_SPR_2UP, 0xCC, 0x0C);
    blit_masked_to_scr_buff(hud_sprites + HUD_SPR_HI,  0x78, 0x0C);
    draw_score_digits_original(0x10, 0x15, score);
    draw_score_digits_original(0x68, 0x15, high_score);
    draw_score_digits_original(0xC0, 0x15, 0);
}
#else
static void render_hud_to_buff(void) {
}
#endif
/* Show a "GAME OVER" screen with the final score + high score, hold
 * ~3 seconds. When the player just beat the high score, a "NEW HIGH
 * SCORE!" banner appears between the labels. */
static void render_game_over(void) {
    static const unsigned char go[]      = { 0x10, 0x0A, 0x16, 0x0E, 0x26,
                                             0x18, 0x1F, 0x0E, 0x1B };  /* GAME OVER */
    static const unsigned char sc_lbl[]  = { 0x1C, 0x0C, 0x18, 0x1B, 0x0E, 0x26 }; /* SCORE_ */
    static const unsigned char hi_lbl[]  = { 0x11, 0x12, 0x10, 0x11, 0x26, 0x26 }; /* HIGH__ */
    static const unsigned char new_lbl[] = { 0x17, 0x0E, 0x21, 0x26, 0x11, 0x12,
                                             0x10, 0x11 }; /* NEW HIGH */
    unsigned char digits[6];
    fill(0, 0, SCREEN_W, SCREEN_H, 0);
    draw_text(BORDER_X + 4 * 8 + 4, BORDER_Y + 70, 15, go, (int)sizeof(go));
    score_to_digits(score, digits);
    draw_text(BORDER_X + 3 * 8,        BORDER_Y +  95, 15, sc_lbl, (int)sizeof(sc_lbl));
    draw_text(BORDER_X + 3 * 8 + 6*8,  BORDER_Y +  95, 15, digits, 6);
    score_to_digits(high_score, digits);
    draw_text(BORDER_X + 3 * 8,        BORDER_Y + 110, 15, hi_lbl, (int)sizeof(hi_lbl));
    draw_text(BORDER_X + 3 * 8 + 6*8,  BORDER_Y + 110, 15, digits, 6);
    /* Saved initials, painted to the right of the HI score line.
     * Shows whatever was recorded with the most recent high score
     * (AAA on first run, the player's choice on subsequent runs). */
    draw_text(BORDER_X + 3 * 8 + 13*8, BORDER_Y + 110, 14,
              high_score_name, 3);
    if (high_score_beaten_this_game) {
        draw_text(BORDER_X + 5 * 8,    BORDER_Y + 130, 14 /* yellow */,
                  new_lbl, (int)sizeof(new_lbl));
    }
}

/* Three-letter initials entry screen — shown after game-over when
 * the player has beaten the previous high score. LEFT/RIGHT cycle
 * the current letter through A..Z; ENTER (or SPACE) confirms it
 * and advances to the next slot; ESC bails (saves whatever's been
 * entered so far + AAA defaults for the rest).
 *
 * The current slot blinks via blink_phase() so the player can see
 * which one they're editing. */
static void input_new_record_name(void) {
    static const unsigned char title[] = {
        0x17, 0x0E, 0x21, 0x26,                 /* NEW_ */
        0x11, 0x12, 0x10, 0x11, 0x26,           /* HIGH_ */
        0x1C, 0x0C, 0x18, 0x1B, 0x0E            /* SCORE */
    };
    static const unsigned char prompt[] = {
        0x0E, 0x17, 0x1D, 0x0E, 0x1B, 0x26,     /* ENTER_ */
        0x22, 0x18, 0x1E, 0x1B, 0x26,           /* YOUR_ */
        0x17, 0x0A, 0x16, 0x0E                  /* NAME */
    };
    static const unsigned char hint[] = {
        0x15, 0x26, 0x1B, 0x26,                 /* L_R_ */
        0x1C, 0x0E, 0x15, 0x0E, 0x0C, 0x1D, 0x26, /* SELECT_ */
        0x0E, 0x17, 0x1D, 0x0E, 0x1B            /* ENTER */
    };
    int pos = 0;
    int name_x = BORDER_X + 14 * 8;
    int name_y = BORDER_Y + 90;
    /* Fresh canvas. */
    fill(0, 0, SCREEN_W, SCREEN_H, 0);
    draw_text(BORDER_X + 7 * 8,  BORDER_Y + 50, 14, title,  (int)sizeof(title));
    draw_text(BORDER_X + 6 * 8,  BORDER_Y + 70, 15, prompt, (int)sizeof(prompt));
    draw_text(BORDER_X + 4 * 8,  BORDER_Y + 130, 13, hint,  (int)sizeof(hint));
    high_score_name[0] = 0x0A;
    high_score_name[1] = 0x0A;
    high_score_name[2] = 0x0A;
    for (;;) {
        /* Repaint the 3-letter row each pass. Current slot blinks. */
        unsigned char blink = (unsigned char)((blink_phase() == 0) ? 0 : 1);
        int i;
        fill(name_x - 2, name_y - 2, 3 * 16 + 4, 12, 0);
        for (i = 0; i < 3; i++) {
            unsigned char code = high_score_name[i];
            unsigned char colour = (i == pos && blink) ? 8 /* dim */ : 15;
            draw_glyph(name_x + i * 16, name_y, colour, code);
        }
        if (kbhit()) {
            int k = getch();
            if (k == KEY_ESC) return;
            if (k == KEY_EXT_PREFIX) {
                int ext = getch();
                if (ext == KEY_LEFT) {
                    high_score_name[pos] = (high_score_name[pos] == 0x0A)
                                           ? 0x23 : (unsigned char)(high_score_name[pos] - 1);
                } else if (ext == KEY_RIGHT) {
                    high_score_name[pos] = (high_score_name[pos] == 0x23)
                                           ? 0x0A : (unsigned char)(high_score_name[pos] + 1);
                }
                continue;
            }
            if (k == KEY_ENTER || k == KEY_SPACE) {
                pos++;
                if (pos >= 3) return;
            }
        }
    }
}

/* Level-intro brick "shimmer" animation. Port of
 * all_metal_briks_animation_snd at $B765 + all_metal_briks_frame at
 * $AD8F: every live brick gets each of 8 animation frames in the
 * order (brik_2, 6, 3, 7, 4, 5, 5, 1), with a 2-tick pause between
 * frames. The sequence ends on frame 1 — same sprite the normal
 * brick rendering uses — so no post-anim restore is needed.
 *
 * Sprite data is brik_1..7 at $AEFF (notes/sprites.md format: raw
 * 16x8 bitmap, no mask). */
static const unsigned char brik_anim_sprites[7][16] = {
    /* spr_brik_1 (default outlined frame, also used by normal render) */
    { 0xFF,0xFE, 0x80,0x00, 0x80,0x00, 0x80,0x00,
      0x80,0x00, 0x80,0x00, 0x80,0x00, 0x00,0x00 },
    /* spr_brik_2 */
    { 0x00,0x02, 0x00,0x02, 0x00,0x02, 0x00,0x02,
      0x00,0x02, 0x00,0x06, 0x00,0xFE, 0x00,0x00 },
    /* spr_brik_3 */
    { 0x00,0x02, 0x00,0x02, 0x00,0x06, 0x00,0x06,
      0x00,0x06, 0x00,0x0E, 0x0F,0xFE, 0x00,0x00 },
    /* spr_brik_4 */
    { 0x00,0x02, 0x00,0x02, 0x00,0x06, 0x00,0x06,
      0x00,0x0E, 0x00,0x3E, 0xFF,0xFE, 0x00,0x00 },
    /* spr_brik_5 (solid) */
    { 0xFF,0xFE, 0xFF,0xFE, 0xFF,0xFE, 0xFF,0xFE,
      0xFF,0xFE, 0xFF,0xFE, 0xFF,0xFE, 0x00,0x00 },
    /* spr_brik_6 */
    { 0x00,0x02, 0x00,0x02, 0x00,0x02, 0x00,0x06,
      0x00,0x06, 0x00,0x0E, 0x01,0xFE, 0x00,0x00 },
    /* spr_brik_7 */
    { 0x00,0x02, 0x00,0x02, 0x00,0x06, 0x00,0x06,
      0x00,0x06, 0x00,0x0E, 0xFF,0xFE, 0x00,0x00 }
};

/* anim_brik order at $AF6F: sprites 2,6,3,7,4,5,5,1 (0-indexed) */
static const unsigned char brik_anim_order[8] = { 1, 5, 2, 6, 3, 4, 4, 0 };

static void reset_brick_hit_anim(void) {
    int i;
    for (i = 0; i < BRICK_HIT_ANIM_SLOTS; i++) {
        brick_hit_anim_ticks[i] = 0;
    }
}

/* Mirror of LAFFC_35/36: take the FIRST free briks_data slot (counter==0),
 * start its counter at 1. No per-brick dedupe — re-hitting a brick mid-anim
 * occupies a second slot, exactly like the original. */
static void brick_hit_anim_spawn(int col, int row) {
    int i;
    for (i = 0; i < BRICK_HIT_ANIM_SLOTS; i++) {
        if (!brick_hit_anim_ticks[i]) {
            brick_hit_anim_ticks[i] = 1;
            brick_hit_anim_col[i] = (unsigned char)col;
            brick_hit_anim_row[i] = (unsigned char)row;
            return;
        }
    }
}

static void step_brick_hit_anim(void) {
    int i;
    for (i = 0; i < BRICK_HIT_ANIM_SLOTS; i++) {
        if (!brick_hit_anim_ticks[i]) continue;
        /* Destroyed brick (cell bit 7) frees the slot — metal_brik_anim's
         * `BIT 7,(HL) -> mark slot free`. That is an EARLY-out only: in
         * the original the anim also ends on its own after one pass (see
         * the counter note below). */
        {
            int col = brick_hit_anim_col[i];
            int row = brick_hit_anim_row[i];
            if (row >= LVL_ROWS || col >= LVL_COLS
                || (live_level[row * LVL_COLS + col] & 0x80)) {
                brick_hit_anim_ticks[i] = 0;
                continue;
            }
        }
        /* metal_brik_anim's `INC A / AND $0F`: the wrap to 0 marks the slot
         * FREE (fill_briks_data skips counter==0), so the anim plays ONE
         * ~15-tick pass (8 frames, last = spr_brik_1, the normal brick
         * look) and stops — metal and multi-hit bricks alike. */
        brick_hit_anim_ticks[i] = (unsigned char)((brick_hit_anim_ticks[i] + 1) & 0x0F);
    }
}

static int any_brick_hit_anim(void) {
    int i;
    for (i = 0; i < BRICK_HIT_ANIM_SLOTS; i++) {
        if (brick_hit_anim_ticks[i]) return 1;
    }
    return 0;
}

/* Port of briks_data / metal_brik_anim ($B6A9): active slots overlay
 * anim_brik frames directly into the screen buffer. Attributes are left
 * alone, matching the original routine's pixel-only writes. */
static void render_brick_hit_anim_to_buff(void) {
    int i, r;
    for (i = 0; i < BRICK_HIT_ANIM_SLOTS; i++) {
        unsigned char tick = brick_hit_anim_ticks[i];
        unsigned char frame_idx;
        const unsigned char *spr;
        unsigned int hl;
        int col, row;
        if (!tick) continue;
        col = brick_hit_anim_col[i];
        row = brick_hit_anim_row[i];
        if (row >= LVL_ROWS || col >= LVL_COLS
            || (live_level[row * LVL_COLS + col] & 0x80)) {
            brick_hit_anim_ticks[i] = 0;
            continue;
        }
        frame_idx = (unsigned char)((tick - 1) >> 1);
        if (frame_idx >= 8) frame_idx = 7;
        spr = brik_anim_sprites[brik_anim_order[frame_idx]];
        hl = 0x401 + (unsigned int)row * 0x100 + (unsigned int)col * 2;
        for (r = 0; r < 8; r++) {
            scr_buff[hl]     = spr[r * 2];
            scr_buff[hl + 1] = spr[r * 2 + 1];
            hl += 32;
        }
    }
}

static void brik_anim_apply_frame(unsigned char frame_idx) {
    const unsigned char *spr = brik_anim_sprites[frame_idx];
    int row, col, r;
    unsigned int hl;
    for (row = 0; row < LVL_ROWS; row++) {
        unsigned int row_base = 0x401 + (unsigned int)row * 0x100;
        const unsigned char *cell_row = &live_level[row * LVL_COLS];
        for (col = 0; col < LVL_COLS; col++) {
            if (cell_row[col] & 0x90) continue;       /* no brick / skip cell */
            hl = row_base + (unsigned int)col * 2;
            for (r = 0; r < 8; r++) {
                scr_buff[hl]     = spr[r * 2];
                scr_buff[hl + 1] = spr[r * 2 + 1];
                hl += 32;
            }
        }
    }
}

/* Run the 8-frame brick-shimmer pass over the current level's bricks.
 * Port of all_metal_briks_animation_snd ($B765): each anim frame is
 * preceded by EXACTLY two 50Hz interrupts (`EI/HALT/EI/HALT/DI`), then
 * drawn — 16 ticks = ~0.32 s total. The "solid" frame (spr_brik_5,
 * index 4) emits a quick metallic beep — port of the
 * play_sound_metal_brik call gated on the "any-metal-brick" check at
 * $B73F.
 *
 * The original is NOT interruptible by input. An earlier port version
 * aborted on any buffered key, which let a held/typematic-repeating key
 * at level entry (moving the bat, pressing FIRE) skip the animation
 * almost entirely (known-bugs #4 "initial shimmer very fast"). Now only
 * ESC reacts (quit, a port convention); other keys are left IN the BIOS
 * buffer for the main loop. The same version also waited
 * `pit_ticks()-t < 2` from a mid-tick sample = 1..2 ticks per frame;
 * the two do{}while edge-waits below are the HALT equivalent: always
 * two full interrupt edges. */
static int play_brik_anim(void) {
    int step;
    int ping_played = 0;    /* SMC trick in original: one_play_sound_metal_brik
                             * rewrites itself to RET after the first call, so
                             * the ping fires once even though spr_brik_5 appears
                             * twice in anim_brik. */
    brik_anim_probe_ticks = pit_ticks();
    for (step = 0; step < 8; step++) {
        unsigned long t;
        unsigned char frame = brik_anim_order[step];
        int two;
        for (two = 0; two < 2; two++) {         /* EI/HALT twice */
            t = pit_ticks();
            do {
                sound_tick();
                /* Peek (no consume) so a buffered gameplay key survives
                 * to the main loop; only ESC is taken, to quit. */
                if ((_bios_keybrd(_KEYBRD_READY) & 0xFF) == 27) {
                    (void)getch();
                    sound_silence();
                    return 1;
                }
            } while (pit_ticks() == t);
        }
        brik_anim_apply_frame(frame);
        buff_to_vga_rect_bytes(32, 96, 1, 30);
        if (frame == 4 && !ping_played) {
            sound_play_metal_brik();
            ping_played = 1;
        }
    }
    brik_anim_probe_ticks = pit_ticks() - brik_anim_probe_ticks;
    sound_silence();
    return 0;
}

/* "PLAYER 1" + "ROUND XX" intro banner — port of show_window_round_number
 * at $8F60 + the pause_long B=4 wait at LB9E8_1. Draws an 80x32 black
 * panel centred between the brick zone and the bat. Original puts
 * PLAYER X at Y=$8F (= 143) and ROUND XX at Y=$9E (= 158). Holds for
 * ~1.2 s (60 PIT ticks) or until a key is pressed. ESC during the
 * wait returns 1 so the caller can quit. */
static int show_round_banner(unsigned int round_num_display) {
    int round_num = (int)round_num_display;
    int banner_x = BORDER_X + 88;
    int banner_y = BORDER_Y + 133;
    int text_x   = BORDER_X + 96;
    unsigned long start;
    unsigned char round_codes[8];
    static const unsigned char player_codes[7] = {
        0x19, 0x15, 0x0A, 0x22, 0x0E, 0x1B, 0x26    /* P L A Y E R _ */
    };
    round_codes[0] = 0x1B;  /* R */
    round_codes[1] = 0x18;  /* O */
    round_codes[2] = 0x1E;  /* U */
    round_codes[3] = 0x17;  /* N */
    round_codes[4] = 0x0D;  /* D */
    round_codes[5] = 0x26;  /* space */
    round_codes[6] = (unsigned char)((round_num / 10) % 10);
    round_codes[7] = (unsigned char)(round_num % 10);

    fill(banner_x, banner_y, 80, 32, 0);
    /* PLAYER 1 / ROUND XX — vertically centred in the 32px window. The
     * original's text coords ($8F=143 PLAYER, $9E=158 ROUND in
     * txt_player_x / txt_round_xx) are BOTTOM-anchored: screen_addr_calc
     * takes them as the glyph's lowest row and print_line draws upward,
     * exactly like the window itself (anchored at $A4=164, drawn up to
     * y=133). The 6px-tall ink therefore lands at byte-5..byte, i.e. the
     * PLAYER ink top is 138 and ROUND ink top is 153 — 5px above the raw
     * byte. An earlier port used the raw bytes (143/158) as top-Y, which
     * jammed both lines against the box bottom (1px gap below vs 6 in the
     * original). Single-player hardcodes the digit; once 2-player wiring
     * lands, swap the trailing $01 for the active player number. */
    draw_text(text_x, BORDER_Y + 138, 15, player_codes, 7);
    {
        unsigned char one = 0x01;
        draw_text(text_x + 7 * 8, BORDER_Y + 138, 15, &one, 1);
    }
    draw_text(text_x, BORDER_Y + 153, 15, round_codes, 8);

    if (getenv("BATTY_HOLD_ROUND_BANNER") != NULL) {
        while (!kbhit()) sound_tick();
        if (getch() == 27) return 1;
        return 0;
    }

    start = pit_ticks();
    while (pit_ticks() - start < 60UL) {
        sound_tick();
        if (kbhit()) {
            int k = getch();
            if (k == 27) return 1;
            break;
        }
    }
    return 0;
}

/* --- "Bat explodes" death animation -----------------------------------
 *
 * When the ball is lost the original spawns 10 sparks at the bat
 * position (LBC10 at $BC10) which fan out for ~46 frames then die. It
 * runs the per-frame loop until all sparks expire, then decrements
 * lives and respawns. Our port plays this as a self-contained sub-loop
 * driven by PIT ticks — the outer run_level enters it from the two
 * ball-lost sites (bomb-on-bat, ball-past-bat). */


/* Port of hl_bc_calc_direction at $AD22 + the LAD13 speed multiply.
 * LAD13 treats the direction-table byte as a magnitude, multiplies by
 * speed, then two's-complement negates the product when the component's
 * sign byte is $FF. Negative components are therefore `-magnitude`, not
 * `magnitude - 256`. Returns signed 8.8 fixed-point displacement. */

#define DEATH_SPARK_COUNT 10
#define DEATH_SPARK_BODY_W 0x08
typedef struct {
    int           active;          /* nonzero while still on screen */
    long          x_q88;           /* 24.8 fixed-point X */
    long          y_q88;           /* 24.8 fixed-point Y */
    unsigned char dir;             /* 6-bit angle */
    unsigned char speed;           /* motion velocity, constant for life */
    unsigned char sprite_num;      /* 0..4 */
    unsigned char frame_ticks;     /* down-counter to next frame */
    unsigned char duration_base;   /* halves at each frame advance —
                                     * mirror of (IX+\$14) in handling_spark
                                     * at \$A8BD. Next frame_ticks = base + 1. */
} death_spark_t;
static death_spark_t death_sparks[DEATH_SPARK_COUNT];

/* Single-frame render of the level scene + the active death sparks.
 * Same compose as redraw_full_with_ball but with the bat / ball / multi-
 * ball hidden (the sparks ARE the bat at this moment). */
static void redraw_with_death_sparks(unsigned char level_idx) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle   = (unsigned char)(level_idx & 3);
    int i;
    paint_bg_to_buff(bg_attr, cycle);
    paint_frame_to_buff(cycle, level_idx);
    render_lives(cycle, bg_attr);
    render_hud_to_buff();
    inner_border_line_c();
    render_brick_band(level_idx);
    render_brick_flash_to_buff();
    render_brick_hit_anim_to_buff();
    for (i = 0; i < DEATH_SPARK_COUNT; i++) {
        int xp, yp;
        if (!death_sparks[i].active) continue;
        xp = (int)(death_sparks[i].x_q88 >> 8);
        yp = (int)(death_sparks[i].y_q88 >> 8);
        if (xp < 0 || xp >= PLAYFIELD_W || yp < 0 || yp >= PLAYFIELD_H) continue;
        blit_masked_to_scr_buff(spr_spark_frames[death_sparks[i].sprite_num],
                                 xp, yp);
    }
    buff_to_vga();
}

/* Render the level scene (no death sparks) for the rocket-clear tally. */
static void redraw_level_scene(unsigned char level_idx) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle   = (unsigned char)(level_idx & 3);
    paint_bg_to_buff(bg_attr, cycle);
    paint_frame_to_buff(cycle, level_idx);
    render_lives(cycle, bg_attr);
    render_hud_to_buff();
    inner_border_line_c();
    render_brick_band(level_idx);
    render_brick_flash_to_buff();
    render_brick_hit_anim_to_buff();
    buff_to_vga();
}

/* Port of add_points_for_left_briks ($AF0D): the rocket-clear score tally.
 * Sweep the grid row-major and, for every remaining live brick, add its
 * points (points_table[row], ×2 for colour >= 6) one PIT tick at a time
 * with the scene + counting score on screen and the BRICKS STILL VISIBLE.
 * Then clear them so live_bricks_remaining()==0 advances the level. The
 * original's per-brick `pause_short` is a CPU busy-wait; we pace one
 * brick per PIT tick instead — the same visible count-up, timing not
 * byte-exact (the busy-wait is Z80-clock-bound and unreproducible). */
static void play_rocket_award_tally(unsigned char level_idx) {
    int row, col;
    unsigned long last = pit_ticks();
    for (row = 0; row < LVL_ROWS; row++) {
        for (col = 0; col < LVL_COLS; col++) {
            unsigned char *cell = &live_level[row * LVL_COLS + col];
            unsigned int pts, idx;
            if (*cell & 0xA0) continue;          /* destroyed or undestructible */
            idx = (unsigned int)((row < 12) ? row : 11);
            pts = points_table[idx];
            if ((*cell & 0x0F) >= 6) pts *= 2;
            score += pts;
            sound_queue(SND_NORMAL_BRIK);          /* per-brick tick */
            /* Pace one brick per PIT tick (sound playing meanwhile). */
            do { sound_tick(); } while (pit_ticks() == last);
            last = pit_ticks();
            sound_frame();
            redraw_level_scene(level_idx);        /* bricks intact + new score */
        }
    }
    /* Clear all the tallied bricks so the level advances (the original's
     * next-round load is what wipes them; here we mark them destroyed). */
    for (row = 0; row < LVL_ROWS; row++) {
        for (col = 0; col < LVL_COLS; col++) {
            unsigned char *cell = &live_level[row * LVL_COLS + col];
            if (!(*cell & 0xA0)) *cell |= 0x80;
        }
    }
    mark_static_bg_cache_dirty();
}

/* Block in a self-contained PIT-paced loop while the bat explodes —
 * the original's per-frame LBAED keeps running through LBC10's spark
 * lifetime; we play it as a separate phase, then return to the outer
 * run_level for the lives-- + respawn step. Mirrors the LBC10_3 spawn
 * loop: 10 sparks, dir = $1B + 5*i (mod 64), speed 2, starting at
 * (bat_x + bat_body_width/2 - 12 + 3*i, $AE), 5-frame decay with
 * halving speed. */
/* Reset the primary ball + bat to fresh-life state. Mirror of
 * all_var_init at \$B7F8 (called from LB9E8_1 on each life-start in
 * the original): ball stuck on bat, bat.bonus_applied = \$FF (= no
 * bonus), all timer-based bat / ball effects cleared. Called from
 * each of the 3 post-explosion respawn sites. */
static void respawn_primary_ball(void) {
    /* Port of all_var_init's LDIR — BAT_X resets to its default $74
     * on each life-start (and on each level entry, handled at the
     * outer loop). Without this, a death mid-screen leaves the bat
     * stranded wherever it was rather than re-centred. */
    BAT_X      = BAT_X_INIT;
    BAT_Y      = BAT_Y_PX;
    objects[OBJ_BAT_2].y_coord = BAT_Y_PX;
    BAT_PREV_X = BAT_X_INIT;
    /* Original all_var_init also clears the sounds_queue at $B842
     * (LD B,$23 / clear_hl_buff). Any stale beep from the just-died
     * frame would otherwise carry over into the new life. */
    sound_stop_all();
    magnet_ball_state_clear(0);
    ball_stuck     = 1;
    stuck_ticks    = 0;
    stuck_offset_x = BALL_X_OFFSET_ON_BAT;
    BALL_SHOW();
    BALL_X = BAT_X + BALL_X_OFFSET_ON_BAT;
    /* Ball sits at BAT_Y_PX - BALL_H_PX = 166 (= $A6) so its bottom
     * row touches the bat's top row. Matches LA27E_15's `LD (IX+$04),
     * $A6` at the FIRE-launch path. Using eff_ball_size (= 8 width)
     * for the Y offset was 1 px too high. */
    BALL_Y = BAT_Y - BALL_H_PX;
    primary_ball_set_velocity(+1, -BALL_SPEED);
    objects[OBJ_BAT_1].bonus_applied = 0xFF;
    objects[OBJ_BAT_2].bonus_applied = 0xFF;
    big_bat_ticks  = 0;
    big_ball_ticks = 0;
    ball_speed_ramp = 0;     /* fresh life: ball restarts at base speed */
    bat_extra_tgt  = 0;
    bullet_cooldown = 0;       /* fresh life — no stale fire cooldown */
    /* Original LBC10 clears flag_extra_life on every life-loss (line
     * \$6411), so another LIFE bonus can drop on the next life within
     * the same round. Our port was only resetting per-level. */
    life_dropped_this_round = 0;
}

static void play_bat_explosion(unsigned char level_idx) {
    int bat_center = BAT_X + (int)objects[OBJ_BAT_1].w_body_px / 2;
    int x_start = bat_center - 12;
    unsigned char dir = 0x1B;
    unsigned long last;
    unsigned long death_pause_start;
    int alive;
    int i;
    /* Mirror LBC10's `SET 7,(IX+\$00)` sweep over all 11 object slots —
     * deactivates bomb, bonus, rocket, enemy, bullets, etc. before the
     * spark animation. Without this, a bomb in flight or alien on
     * screen would persist through the explosion and reappear when
     * the player respawns. */
    bomb_active = 0;
    bonus_active = 0;
    rocket_active = 0;
    rocket_clear_completed = 0;
    set_rocket_bonus_sprite_height(ROCKET_BONUS_H_PX);
    pts_400_active = 0;
    bullet_active[0] = 0;
    bullet_active[1] = 0;
    bullet_blast_ticks[0] = 0;
    bullet_blast_ticks[1] = 0;
    brick_flash_ticks = 0;
    reset_brick_hit_anim();
    objects[OBJ_ENEMY].sprite_set = 0;
    /* Extras may still be inactive but defensive-clear in case a new
     * call site is added that forgets to deactivate them upstream. */
    ball2_active = 0;
    objects[OBJ_BALL_2].sprite_set = 0x82;
    ball3_active = 0;
    objects[OBJ_BALL_3].sprite_set = 0x82;
    for (i = 0; i < DEATH_SPARK_COUNT; i++) {
        death_sparks[i].active        = 1;
        death_sparks[i].x_q88         = (long)(x_start + i * 3) << 8;
        death_sparks[i].y_q88         = (long)0xAE << 8;
        death_sparks[i].dir           = dir;
        death_sparks[i].speed         = 2;        /* matches (IX+\$07) at spawn */
        death_sparks[i].sprite_num    = 0;
        death_sparks[i].frame_ticks   = 0x18;     /* (IX+\$15) at spawn */
        death_sparks[i].duration_base = 0x18;     /* (IX+\$14) at spawn */
        dir = (unsigned char)((dir + 5) & 0x3F);
    }
    /* Push sound $08 — LBC10 seeds sounds_queue with id=$08,state=$3D
     * before the spark fanout loop. */
    sound_queue(SND_SPARK_FANOUT);
    last = pit_ticks();
    do {
        unsigned long now;
        do {
            sound_tick();
            now = pit_ticks();
        } while (now == last);
        last = now;
        sound_frame();
        alive = 0;
        for (i = 0; i < DEATH_SPARK_COUNT; i++) {
            int dx_q88, dy_q88;
            int xp, yp;
            int right_x;
            if (!death_sparks[i].active) continue;
            dir_to_dxdy(death_sparks[i].dir, death_sparks[i].speed,
                         &dx_q88, &dy_q88);
            death_sparks[i].x_q88 += dx_q88;
            death_sparks[i].y_q88 += dy_q88;
            xp = (int)(death_sparks[i].x_q88 >> 8);
            yp = (int)(death_sparks[i].y_q88 >> 8);
            /* Off the bottom = dead. Sides clamp the position so the
             * spark can keep ticking until its frame counter expires. */
            if (yp >= PLAYFIELD_H) { death_sparks[i].active = 0; continue; }
            right_x = 0xF8 - DEATH_SPARK_BODY_W;
            if (xp < 8) {
                death_sparks[i].x_q88 = 8L << 8;
                death_sparks[i].dir = (unsigned char)(((death_sparks[i].dir ^ 0x1F) + 1) & 0x3F);
            } else if (xp > right_x) {
                death_sparks[i].x_q88 = (long)right_x << 8;
                death_sparks[i].dir = (unsigned char)(((death_sparks[i].dir ^ 0x1F) + 1) & 0x3F);
            }
            if (yp < 8) {
                death_sparks[i].y_q88 = 8L << 8;
                death_sparks[i].dir = (unsigned char)(((death_sparks[i].dir ^ 0x3F) + 1) & 0x3F);
            }
            if (--death_sparks[i].frame_ticks == 0) {
                if (death_sparks[i].sprite_num >= SPARK_FRAMES - 1) {
                    death_sparks[i].active = 0;
                    continue;
                }
                death_sparks[i].sprite_num++;
                /* Original handling_spark at $A8BD: SRL (IX+$14);
                 * INC A; LD (IX+$15),A. Halve the duration base, set
                 * tick counter to base+1. (IX+$07) = speed stays
                 * constant for the spark's whole life. */
                death_sparks[i].duration_base =
                    (unsigned char)(death_sparks[i].duration_base >> 1);
                death_sparks[i].frame_ticks =
                    (unsigned char)(death_sparks[i].duration_base + 1);
            }
            alive = 1;
        }
        redraw_with_death_sparks(level_idx);
    } while (alive);
    /* Original LBC10 waits `pause_long B=$03` after the last spark
     * object disappears, before lives-- and respawn/game-over. Since
     * B=4 is 60 PIT ticks at the round banner, B=3 is 45 ticks. */
    death_pause_start = pit_ticks();
    while (pit_ticks() - death_pause_start < 45UL) {
        sound_tick();
        if (kbhit()) {
            int k = getch();
            if (k == KEY_ESC) break;
        }
    }
    sound_stop_all();
}


/* --- Level entry ------------------------------------------------------
 * Everything between arriving at a level and the first frame of play. */

/* Reset the per-level state and load the level's bricks. orig: the LDIR
 * in all_var_init, which restores a template over the whole block. */
static void reset_level_state(unsigned char lvl_idx) {
    objects[OBJ_ENEMY].sprite_set = 0;     /* alien cleared on level entry */
    /* Port of all_var_init's LDIR — BAT_X resets to the default
     * $74 at every level entry. Earlier port kept the bat where
     * the player left it; original re-centres on each level. */
    BAT_X         = BAT_X_INIT;
    BAT_Y         = BAT_Y_PX;
    objects[OBJ_BAT_2].y_coord = BAT_Y_PX;
    BAT_PREV_X    = BAT_X_INIT;
    ball_stuck    = 1;
    stuck_offset_x = BALL_X_OFFSET_ON_BAT;
    BALL_SHOW();                      /* visible from level entry; sits on the bat */
    BALL_X        = BAT_X + BALL_X_OFFSET_ON_BAT;
    BALL_Y        = BAT_Y - BALL_H_PX;
    stuck_ticks   = 0;                /* counts up while waiting for launch */
    primary_ball_set_velocity(+1, -BALL_SPEED);
    bonus_active   = 0;
    bomb_active        = 0;
    bullet_active[0]      = 0;
    bullet_active[1]      = 0;
    bullet_blast_ticks[0] = 0;
    bullet_blast_ticks[1] = 0;
    bullet_cooldown       = 0;
    dbg_shots_fired       = 0;
    rocket_active      = 0;
    rocket_clear_completed = 0;
    set_rocket_bonus_sprite_height(ROCKET_BONUS_H_PX);
    brick_flash_ticks  = 0;
    reset_brick_hit_anim();
    ball2_active   = 0;
    objects[OBJ_BALL_2].sprite_set = 0x82;
    ball3_active   = 0;
    objects[OBJ_BALL_3].sprite_set = 0x82;
    pts_400_active = 0;
    ball_speed_ramp = 0;
    big_bat_ticks  = 0;
    big_ball_ticks = 0;
    /* flag_extra_life is NOT cleared at level entry in original —
     * only LBC10 (death path) clears it. So a LIFE bonus catch
     * blocks future LIFE drops for the rest of the player's life,
     * across levels. Earlier port reset on level entry too,
     * making LIFE bonuses re-available per round. */
    run_dot_frame = 0x0E;               /* matches running_dot_frame_1up reset */
    bat_extra_px   = 0;
    bat_extra_tgt  = 0;
    objects[OBJ_BAT_1].bonus_applied = 0xFF;
    objects[OBJ_BAT_2].bonus_applied = 0xFF;
    /* Mirror all_var_init's `clear_hl_buff` of sounds_queue at line
     * 5984 — sounds in-flight at level entry shouldn't bleed into
     * the new round. */
    sound_stop_all();
    memcpy(live_level, &levels[(int)lvl_idx * LVL_CELLS], LVL_CELLS);
}

/* Seeded overrides from the replay harness. Applied after the level's
 * bricks are loaded and before anything reads them, so a seeded run and
 * the original start from the same state. */
static void apply_replay_overrides(void) {
    apply_replay_random_override();
    apply_replay_bat_object_override();
    apply_replay_ball_object_override();
    apply_replay_ball_motion_override();
    apply_replay_enemy_object_override();
    apply_replay_bonus_override();
    apply_replay_bomb_override();
    apply_replay_pts400_override();
    apply_replay_force_brick();
    apply_replay_ball_ramp();
    apply_replay_bullet_override();
    apply_replay_blast_override();
    apply_replay_force_bonus();
    apply_replay_multiball();
    apply_replay_bigball();
    apply_replay_rocket_override();
}

/* The round banner and the all-metal shimmer, plus the harness hooks that
 * hang off them. Returns false if the player quit. */
static int show_level_intro(unsigned int round) {
    if (show_round_banner(round + 1)) return 0;
    render_level_screen(current_level_idx_var);   /* clear the banner */
    /* Test hook: stuff one ENTER into the BIOS keyboard buffer right
     * before the intro shimmer. The original animation is NOT
     * interruptible by input; the regression test asserts the key
     * neither aborts nor is eaten by the animation (it must survive
     * to release BATTY_REPLAY_WAIT_KEY below). With the old abort-on-
     * any-key behaviour the key is consumed, WAIT_KEY blocks forever
     * and the test times out with no probe. */
    if (getenv("BATTY_TEST_KEY_BEFORE_ANIM") != NULL) {
        union REGS r;
        r.h.ah = 0x05;              /* INT 16h: store keystroke */
        r.w.cx = 0x1C0D;            /* scancode $1C, ascii CR (ENTER) */
        int386(0x16, &r, &r);
    }
    if (play_brik_anim()) return 0;
    /* Replay parity hook: block here until the harness sends a key,
     * giving the original side a matching breakpoint to halt at and
     * letting both runners capture the static L3-entry screen with
     * no wall-clock drift. The wake key is consumed below so it
     * doesn't double as the next main-loop input. */
    if (getenv("BATTY_REPLAY_WAIT_KEY") != NULL) {
        serial_probe_signal();   /* boot+intro done, AT the $BA83 pause */
        while (!kbhit()) {
            sound_tick();
        }
        (void)getch();
    }
    return 1;
}

/* Replay determinism: pin the free-running frame counter at the aligned
 * start, so every counter-phase decision is the same run to run. */
static void pin_replay_frame_counter(void) {
    /* Replay determinism hook: pin the global frame counter (= the
     * original's counter_misc) at the aligned start. The counter has
     * been ticking since boot, so its low-bit PHASE at this point is
     * wall-clock roulette — and the enemy steer (&3), the ball speed
     * ramp (&7), and other counter_misc-gated cadences all key off it.
     * Un-pinned, a 4-frame steer turn can slide across a probe frame
     * run-to-run (the test-enemy-steer flake). Hex value; pinned AFTER
     * the WAIT_KEY release so frame 1 sees counter == pin+1, making
     * every counter_misc-phase decision deterministic per seed. */
    {
        const char *pc = getenv("BATTY_REPLAY_COUNTER");
        if (pc != NULL && *pc != '\0') {
            char *endp;
            unsigned long v = strtoul(pc, &endp, 16);
            if (*endp == '\0') {
                _disable();
                pit_frame_counter = v;
                _enable();
            }
        }
    }
}


/* What the keyboard asked for this frame. Arrow keys are not here: they
 * are polled from key_state[] in the frame body, so holding one steers
 * continuously instead of repeating at the BIOS rate. */
enum InputAction {
    INPUT_NONE,             /* nothing that changes the frame's course */
    INPUT_QUIT,
    INPUT_SKIP_FRAME,       /* pause toggled, or input swallowed while paused */
    INPUT_ADVANCE_LEVEL     /* ENTER on the pause overlay */
};

static InputAction handle_input(int &ball_moved, int &bat_moved,
                            unsigned long &start) {
    if (!kbhit()) return INPUT_NONE;
    {
        const int k = getch();
        if (k == KEY_ESC) {
            return INPUT_QUIT;
        }
        if (k == KEY_P_LOWER || k == KEY_P_UPPER
            || k == '1' || k == '2' || k == '3' || k == '4') {
            paused = !paused;
            sound_silence();
            if (paused) {
                /* Paint a "PAUSED" banner over the current frame. */
                static const unsigned char paused_codes[] = {
                    0x19, 0x0A, 0x1D, 0x1C, 0x0E, 0x0D  /* P A U S E D */
                };
                draw_text(BORDER_X + 13 * 8, BORDER_Y + 90, 15,
                          paused_codes, 6);
            } else {
                /* Resuming: schedule a full redraw to erase banner. */
                bat_moved = 1;
                ball_moved = 1;
                force_full_flush = 1;
            }
            return INPUT_SKIP_FRAME;
        }
        if (paused) {
            if (k == KEY_ENTER) return INPUT_ADVANCE_LEVEL;
            return INPUT_SKIP_FRAME;                          /* swallow other input */
        }
        if (k == KEY_EXT_PREFIX) {
            /* Discard the scancode following 0 - arrows are
             * handled by the per-frame key_state[] polling
             * below; this just keeps the buffer drained. */
            getch();
            start = bios_ticks();
        } else if (k == KEY_SPACE) {
            /* Launch the stuck ball — only fire the launch
             * trajectory if the ball is actually waiting on the
             * bat. Without this guard, hammering SPACE while
             * the ball is in flight (e.g. trying to fire the
             * laser repeatedly) would teleport the ball back to
             * its launch dx/dy, breaking the bounce. */
            if (ball_stuck) {
                BALL_SHOW();
                ball_stuck   = 0;
                stuck_ticks  = 0;
                sound_queue(SND_BALL_START); /* descending launch blip */
                primary_ball_launch_from_bat();
                record_primary_launch();
            }
            /* If the bat carries the LASER bonus and a free
             * bullet slot exists, also fire one from the bat
             * top centre. Two slots = up to two in flight at
             * once (port of object_bullet_1 / _2 at $A0FA).
             * Independent of ball state — SPACE can refire
             * the laser while the ball is in play. */
            /* free_bullet_2 ($A14C): bullet emerges from the bat
             * surface (bat_x+12, y=172), 0x18 reset = 12-frame
             * cadence. Extracted to try_fire_laser so the auto-fire
             * test hook can drive the same path. Independent of ball
             * state — SPACE refires the laser while the ball flies. */
            try_fire_laser();
            start = bios_ticks();
        }
        /* Mirror the original: no level-skip key. ENTER while
         * playing does nothing (only the pause overlay above
         * consumes ENTER to dismiss). The level holds until
         * the player clears it or loses all lives. */
    }
    return INPUT_NONE;
}

static state_t run_level(void) {
    unsigned char i;
    unsigned long start;
    unsigned long last_tick;
    unsigned char cycle;
    unsigned char bg_attr;

    /* New game: reset score + lives + bonus state. Score/lives carry
     * across levels within one game; they reset only when re-entering
     * run_level from ST_HISCORE. */
    score = 0;
    lives = LIVES_INIT;
    live_adds_awarded = 0;
    bonus_active = 0;
    ball_speed_ramp = 0;
    big_bat_ticks = 0;
    big_ball_ticks = 0;
    bat_extra_px = 0;
    bat_extra_tgt = 0;
    paused = 0;
    high_score_beaten_this_game = 0;
    {
        const char *p = getenv("BATTY_LAUNCH_FRAMES");
        launch_probe_frames = (p && *p) ? (unsigned int)atoi(p) : 0;
        launch_probe_countdown = 0;
        launch_probe_active = 0;
        p = getenv("BATTY_FRAME_PROBE");
        frame_probe_frames = (p && *p) ? (unsigned int)atoi(p) : 0;
        frame_probe_countdown = frame_probe_frames;
        frame_probe_active = (frame_probe_frames != 0) ? 1 : 0;
        p = getenv("BATTY_VISUAL_PROBE_FRAMES");
        visual_probe_count = 0;
        visual_probe_index = 0;
        if (p && *p) {
            /* Comma-separated ascending absolute frame indices. Values
             * not strictly greater than the previous one are dropped so
             * the per-checkpoint countdown deltas stay positive. */
            unsigned int prev = 0;
            while (*p && visual_probe_count < VISUAL_PROBE_MAX) {
                unsigned int v;
                while (*p == ',' || *p == ' ') p++;
                if (*p == '\0') break;
                v = (unsigned int)atoi(p);
                if (v > prev) {
                    visual_probe_list[visual_probe_count++] = v;
                    prev = v;
                }
                while (*p && *p != ',') p++;
            }
        }
        visual_probe_active = (visual_probe_count != 0) ? 1 : 0;
        visual_probe_countdown = visual_probe_active ? visual_probe_list[0] : 0;
    }

    /* The original loops levels forever (increment_round_number at
     * $BBE0 wraps current_level_number_1up at 15 → 0 while
     * round_number_1up keeps bumping). Game only ends on lives == 0. */
    /* Test/debug override: BATTY_LEVEL=N (1..15) starts at level N
     * instead of level 1. Lets the visual regression suite (or a
     * person investigating a per-level rendering bug) capture each
     * cycle's level entry without playing through. round_number is
     * 0-based internally, so subtract 1 from the env value. */
    {
        const char *p = getenv("BATTY_LEVEL");
        round_number = 0;
        if (p && *p) {
            int n = atoi(p);
            if (n >= 1 && n <= N_LEVELS) round_number = (unsigned char)(n - 1);
        }
    }
    /* BAT_X resets to BAT_X_INIT at NEW GAME. Original also resets it
     * at every life/level entry via all_var_init's LDIR from
     * objects_buff_2 — handled inline at the inner loop's level-init
     * block and in respawn_primary_ball below. */
    BAT_X      = BAT_X_INIT;
    BAT_Y      = BAT_Y_PX;
    objects[OBJ_BAT_2].y_coord = BAT_Y_PX;
    BAT_PREV_X = BAT_X_INIT;
    for (;;) {
        unsigned char lvl_idx = (unsigned char)(round_number % N_LEVELS);
        current_level_idx_var = lvl_idx;
        i = lvl_idx;                     /* the cycle / bg_attr code below reads `i` */

        reset_level_state(lvl_idx);
        apply_replay_overrides();
        /* After the RNG seed override, so the magnets' ON/OFF coins
         * consume the seeded walk exactly as print_magnets does; before
         * render_level_screen, which paints from this state. */
        magnet_level_init(lvl_idx);

        probe_from_gameplay = 0;         /* this PROBE write is the pre-gameplay seed */
        write_replay_probe();
        render_level_screen(lvl_idx);
        if (!show_level_intro((unsigned int)round_number)) return ST_QUIT;
        pin_replay_frame_counter();

        cycle     = (unsigned char)(i & 3);
        bg_attr   = bg_attr_per_cycle[i & 3];
        probe_from_gameplay = 1;         /* PROBE writes below are checkpoints */
        start     = bios_ticks();
        last_tick = pit_ticks();
        for (;;) {
            unsigned long now;
            int ball_moved = 0;
            int bat_moved  = 0;
            int frame_ticked = 0;

            {
                const InputAction action = handle_input(ball_moved, bat_moved, start);
                if (action == INPUT_QUIT) { write_replay_probe(); return ST_QUIT; }
                if (action == INPUT_ADVANCE_LEVEL) break;
                if (action == INPUT_SKIP_FRAME)    continue;
            }

            /* Frame tick at 50 Hz from our PIT IRQ. */
            now = pit_ticks();
            if (paused) {
                last_tick = now;                       /* stop ball motion */
                continue;
            }
            if (now != last_tick) {
                last_tick = now;
                frame_ticked = 1;
                /* Magnet random toggle (LB9E8_2 top: read-current
                 * `random_number+$01 == $99` -> print_one_magnet). Must
                 * sample BEFORE this frame's per-frame RNG tick, like
                 * the original (the check reads last frame's value).
                 * Pinned off in test mode (BATTYALL) so the level-entry
                 * visual captures stay deterministic — same trick as
                 * the menu blink / running-dot pins. */
                if (!test_mode_pin_blink && rng_high(rng_current()) == 0x99)
                    magnet_random_toggle();
                /* Per-frame RNG tick (original LB9E8_2: one `CALL
                 * random_generate` per main-loop pass). Gated so the
                 * default on-demand model is byte-unchanged. */
                if (rng_perframe) next_random();
                /* Steering. The arrows are polled from key_state[] rather
                 * than read from the BIOS buffer, so holding one steers
                 * continuously at 4 px per 50 Hz tick = 200 px/s, as the
                 * original's get_left_player_ctrl_state does. A rocket in
                 * flight carries the bat, so the player cannot steer. */
                BAT_X = (unsigned char)bat_step_x(
                    BAT_X, bat_extra_px,
                    !rocket_active && key_state[SC_LEFT],
                    !rocket_active && key_state[SC_RIGHT]);
                if (ball_stuck) {
                    /* Ball rides the bat at the catch offset (= where it
                     * hit, when the CATCH bonus stuck it; otherwise the
                     * default BALL_X_OFFSET_ON_BAT) until SPACE or
                     * timeout. */
                    BALL_X = BAT_X + stuck_offset_x;
                    /* $A6 = 166 for the launch rest (LA27E_15); a ball
                     * held by the MAGNET bonus rests 1px lower at $A7 =
                     * 167 (LAB1F_3). The bat's active bonus ($03 = MAGNET,
                     * the original's IY+$14) selects which. */
                    BALL_Y = BAT_Y - BALL_H_PX +
                             (objects[OBJ_BAT_1].bonus_applied == 0x03 ? 1 : 0);
                    ball_moved = 1;
                    stuck_ticks++;
                    if (stuck_ticks >= STUCK_TIMEOUT) {
                        ball_stuck = 0;          /* auto-launch */
                        sound_queue(SND_BALL_START);
                        primary_ball_launch_from_bat();
                        record_primary_launch();
                    }
                } else if (BALL_VISIBLE) {
                    /* SLOW is now a ball-speed reset ($02), not a
                     * frame-skip — handling_ball runs every frame in the
                     * original; the speed byte (via dir_to_dxdy magnitude)
                     * is what changes. So the ball always steps here. */
                    ball_speed_ramp_tick();
                    step_ball();
                    ball_moved = 1;
                    if (launch_probe_active) {
                        if (launch_probe_countdown > 0) launch_probe_countdown--;
                        if (launch_probe_countdown == 0) {
                            write_replay_probe();
                            serial_probe_signal();
                            return ST_QUIT;
                        }
                    }
                    if (frame_probe_active) {
                        if (frame_probe_countdown > 0) frame_probe_countdown--;
                        if (frame_probe_countdown == 0) {
                            write_replay_probe();
                            serial_probe_signal();
                            return ST_QUIT;
                        }
                    }
                }
                if (auto_fire) try_fire_laser();   /* held-SPACE sim (test) */
                step_bonus();
                step_pts_400();
                step_bomb();
                step_bullet();
                bullet_blasts_tick();
                step_rocket();
                step_brick_flash();
                step_brick_hit_anim();
                if (bat_fire_anim_ticks) bat_fire_anim_ticks--;
                if (bullet_cooldown >= 2) bullet_cooldown -= 2;     /* SUB \$02 / frame */
                else bullet_cooldown = 0;
                /* SLOW affects ALL balls in the original (sets the
                 * ball_1/2/3 speed bytes to $02 simultaneously); now
                 * modeled via the speed byte, so the extras simply step
                 * every frame too (their speed magnitude reflects SLOW). */
                step_ball2();
                step_ball3();
                /* Mirror LBAED's ordering: object_rocket is checked
                 * before balls_quantity, and an active rocket jumps to
                 * the rocket loop instead of LBC10's bat-explosion
                 * path. The rocket catch hides all balls while the
                 * level-clear sequence runs, so that temporary no-ball
                 * state must not cost a life. */
                if (!rocket_active
                    && !rocket_clear_completed
                    && !suppress_no_ball_death
                    && !BALL_VISIBLE
                    && !ball2_active
                    && !ball3_active) {
                    play_bat_explosion(current_level_idx_var);
                    if (lives > 0) lives--;
                    if (lives > 0) respawn_primary_ball();
                }
                /* Mirror of LB9E8_2..LB9E8_3 ($BA83..$BAD9):
                 *   enemy_prepare    -- maybe spawn alien
                 *   handling_bat     -- bat motion (here via key_state)
                 *   call_for_all_obj(handling_object)
                 *   call_for_all_obj(ix_buf_addr_calc) */
                enemy_prepare();
                call_for_all_obj(handling_object);
                kill_enemy_by_bat();             /* called from handling_bat
                                                  * in the original at $98A8;
                                                  * here outside since our
                                                  * bat handler doesn't exist */
                /* Ball-vs-alien: the original's kill_enemy_by_bat at
                 * $A4B8 is invoked from BOTH handling_bat and
                 * handling_ball — any ball plunking down on an alien
                 * destroys it. Earlier port only checked the bat. */
                if (BALL_VISIBLE)
                    kill_enemy_by_ball_rect(BALL_X, BALL_Y, BALL_W_PX, BALL_H_PX);
                if (ball2_active)
                    kill_enemy_by_ball_rect((int)objects[OBJ_BALL_2].x_coord,
                                             (int)objects[OBJ_BALL_2].y_coord,
                                             BALL_W_PX, BALL_H_PX);
                if (ball3_active)
                    kill_enemy_by_ball_rect((int)objects[OBJ_BALL_3].x_coord,
                                             (int)objects[OBJ_BALL_3].y_coord,
                                             BALL_W_PX, BALL_H_PX);
                call_for_all_obj(refresh_buffer_offset);
                sound_frame();
                sound_tick();
                /* Score-milestone extra life — port of score_update_3
                 * at $0395. Each crossed threshold in live_add_thresholds
                 * awards one extra life and pushes the live-add sound. */
                while (live_adds_awarded < LIVE_ADD_COUNT
                       && score >= live_add_thresholds[live_adds_awarded]) {
                    lives++;
                    sound_queue(SND_LIVE_ADD);
                    live_adds_awarded++;
                }
                /* Roll the displayed HI score forward the moment it's
                 * passed, instead of waiting for game-over. The disk
                 * save still happens at game-over via save_high_score. */
                if (score > high_score) {
                    high_score = score;
                    high_score_beaten_this_game = 1;
                }
                if (bonus_active) ball_moved = 1;
                if (pts_400_active) ball_moved = 1;
                if (bat_extra_px != bat_extra_tgt) bat_moved = 1;
                if (objects[OBJ_ENEMY].sprite_set != 0) ball_moved = 1;
                if (bomb_active) ball_moved = 1;
                if (any_bullet_active()) ball_moved = 1;
                if (any_bullet_blast()) ball_moved = 1;
                if (rocket_active) ball_moved = 1;
                if (brick_flash_ticks) ball_moved = 1;
                if (any_brick_hit_anim()) ball_moved = 1;
                if (ball2_active) ball_moved = 1;
                if (ball3_active) ball_moved = 1;
            }

            if (BAT_X != BAT_PREV_X) {
                bat_moved = 1;
            }
            if (force_bat_full_redraw && bat_moved) {
                ball_moved = 1;
            }
            if (force_full_flush_each_frame && ball_moved) {
                force_full_flush = 1;
            }

            if (ball_moved) {
                unsigned int blockers = ball_dirty_blockers(bat_moved);
                if (blockers == 0) {
                    redraw_ball_only(i);
                } else if (can_redraw_ball_with_simple_objects(blockers)) {
                    redraw_ball_with_simple_objects(i);
                } else {
                    prof_note_ball_dirty_blockers(blockers);
                    redraw_full_with_ball(i);
                }
            } else if (bat_moved) {
                redraw_bat(cycle, bg_attr);
                if (BALL_VISIBLE && ball_stuck) {
                    BALL_X = BAT_X + BALL_X_OFFSET_ON_BAT;
                    render_ball(BALL_X, BALL_Y, bg_attr);
                }
            }

            if (visual_probe_active && frame_ticked) {
                if (visual_probe_countdown > 0) visual_probe_countdown--;
                if (visual_probe_countdown == 0) {
                    /* Reached a checkpoint: write the probe, then halt so
                     * the harness can grab a deterministic capture. The
                     * wake key resumes play toward the next checkpoint;
                     * after the final one we quit so QEMU exits cleanly.
                     * A single-value probe has count==1 and quits here,
                     * exactly as the original single-shot path did. */
                    write_replay_probe();
                    serial_probe_signal();   /* reached frame N -> tell harness */
                    while (!kbhit()) {
                        sound_tick();
                    }
                    (void)getch();
                    visual_probe_index++;
                    if (visual_probe_index >= visual_probe_count) {
                        return ST_QUIT;
                    }
                    visual_probe_countdown =
                        visual_probe_list[visual_probe_index]
                        - visual_probe_list[visual_probe_index - 1];
                }
            }

            if (profile_auto_frames != 0
                && prof_frames_count >= profile_auto_frames) {
                write_replay_probe();
                return ST_QUIT;
            }

            /* End-of-life conditions. */
            if (lives == 0) {
                if (score > high_score) {
                    high_score = score;
                    high_score_beaten_this_game = 1;
                    /* Save deferred until after the name-entry screen
                     * so the file holds the new high + the player's
                     * initials together. */
                }
                sound_stop_all();
                /* Original LBC10_6 plays no game-over sound — the
                 * preceding pause_clear_screen_attrib just drains the
                 * sound queue while the screen clears. Dropping our
                 * 100 Hz drone for byte-exact silence here. */
                render_game_over();
                /* Hold GAME OVER for ~3.6 s — matches LBC10_6's
                 * `pause_long B=\$0C` (= 12 * 0.3 s). 18.2 Hz BIOS ticks
                 * × 3.6 s ≈ 65. Not gated on auto_advance — the
                 * original's pause_clear_screen_attrib waits regardless,
                 * and TIMED_OUT would be a no-op with auto_advance=0,
                 * leaving the player stuck on the screen. */
                start = bios_ticks();
                while (bios_ticks() - start < 65UL) {
                    sound_tick();
                    if (kbhit()) { getch(); break; }
                }
                if (high_score_beaten_this_game) {
                    input_new_record_name();
                    high_score_save(high_score, high_score_name);
                }
                sound_silence();
                return ST_TITLE;
            }
            /* Mirror LBAED_0's exit conditions:
             *   balls_quantity == 0  -> game over (handled above)
             *   briks_quantity_1up == 0 -> level cleared, advance.
             * No timeout, no key-driven skip — the level holds the
             * player until the bricks are gone. */
            if (live_bricks_remaining() == 0) {
                rocket_clear_completed = 0;
                BALL_HIDE();
                ball2_active = 0;
                ball3_active = 0;
                force_full_flush = 1;
                redraw_full_with_ball(i);
                if (getenv("BATTY_HOLD_ROCKET_CLEAR") != NULL) {
                    while (!kbhit()) sound_tick();
                    if (getch() == KEY_ESC) return ST_QUIT;
                }
                /* Original's LBBFB_0 pauses ~0.6 s (pause_long B=2)
                 * before the next level's setup — let the player see
                 * the cleared brick zone briefly. Just `CALL Z,
                 * play_sounds_queue` to drain any queued sounds (e.g.
                 * the SND_NORMAL_BRIK click from the last brick) —
                 * no dedicated level-clear sound. Earlier port added
                 * a 700 Hz beep that overrode the brick click. */
                unsigned long t = pit_ticks();
                while (pit_ticks() - t < 30UL) {
                    sound_tick();
                    if (kbhit()) {
                        int k = getch();
                        if (k == KEY_ESC) return ST_QUIT;
                        break;
                    }
                }
                sound_silence();
                break;
            }
        }
        round_number++;       /* increment_round_number at $BBE0 */
    }
}

int main(void) {
    state_t state = ST_TITLE;
    /* BATTYALL=1 (test floppy AUTOEXEC) pins the menu blink phase to
     * 0 (BLACK half) so the state2_menu screendump is deterministic
     * against snap2. Plain `make run` floppy leaves it off and the
     * user sees the natural ~4.5 Hz menu blink. */
    if (getenv("BATTYALL") != NULL) test_mode_pin_blink = 1;
    if (getenv("BATTY_FORCE_BAT_FULL_REDRAW") != NULL) force_bat_full_redraw = 1;
    if (getenv("BATTY_FORCE_BALL_FULL_REDRAW") != NULL) force_ball_full_redraw = 1;
    if (getenv("BATTY_LAFFC") != NULL) use_laffc = 1;
    if (getenv("BATTY_LEGACY_COLLISION") != NULL) use_laffc = 0;
    {   /* Default ON (per-frame tick = the original's model). The env can
         * force either state: BATTY_RNG_PERFRAME=0 reverts to advance-on-
         * read (the old default); any other value keeps it on. */
        const char *e = getenv("BATTY_RNG_PERFRAME");
        if (e != NULL) rng_perframe = (e[0] == '0' && e[1] == '\0') ? 0 : 1;
    }
    if (getenv("BATTY_FORCE_FULL_FLUSH_EACH_FRAME") != NULL) force_full_flush_each_frame = 1;
    if (getenv("BATTY_SUPPRESS_NO_BALL_DEATH") != NULL) suppress_no_ball_death = 1;
    if (getenv("BATTY_SERIAL_PROBE") != NULL) serial_probe_enabled = 1;
    if (getenv("BATTY_AUTO_FIRE") != NULL) auto_fire = 1;
    if (getenv("BATTY_FULL_BAND_REBUILD") != NULL) force_full_band_rebuild = 1;
    {
        const char *p = getenv("BATTY_PROFILE_AUTO_FRAMES");
        if (p != NULL && *p != '\0') {
            profile_auto_frames = strtoul(p, NULL, 10);
            if (profile_auto_frames != 0) state = ST_LEVEL;
        }
    }
    if (getenv("BATTY_START_LEVEL") != NULL) state = ST_LEVEL;
    if (getenv("BATTY_NOSOUND") != NULL || getenv("BATTY_SOUND_OFF") != NULL
        || getenv("BATTY_RENDER_PROFILE") != NULL)
        sound_set_enabled(false);
    /* Mode 13h + the ZX palette until main() returns. */
    ZxDisplay display;

    sound_set_clock(pit_ticks);
    sound_set_random(sound_random_byte);
    enemy_set_random(enemy_random_current, enemy_random_sample);

    /* INDICAT.BIN and BOTSPR.BIN each pack two player bitmaps back to
     * back; the indicator's are preceded by a 2-byte header apiece. */
    unsigned char indicator_file[2 + sizeof(ind_p1) + 2 + sizeof(ind_p2)];
    unsigned char bottom_file[sizeof(bot_p1) + sizeof(bot_p2)];

    const bool assets_loaded =
        asset_load("FONT.BIN",    font,        sizeof(font)) &&
        asset_load("HUDSPR.BIN",  hud_sprites, sizeof(hud_sprites)) &&
        asset_load("LEVELS.BIN",  levels,      sizeof(levels)) &&
        asset_load("LVLATTR.BIN", level_attrs, sizeof(level_attrs)) &&
        asset_load("BGTILE.BIN",  bg_tile,     sizeof(bg_tile)) &&
        asset_load("FRAMEL1.BIN", frame_l1,    sizeof(frame_l1)) &&
        asset_load("SPRITES.BIN", sprites_blob, sizeof(sprites_blob)) &&
        asset_load("INDICAT.BIN", indicator_file, sizeof(indicator_file)) &&
        asset_load("BOTSPR.BIN",  bottom_file,    sizeof(bottom_file)) &&
        asset_load_chunked("RANDOM.BIN", random_rom, RANDOM_ROM_SIZE,
                           screen_chunk, sizeof(screen_chunk));

    if (assets_loaded) {
        memcpy(ind_p1, indicator_file + 2, sizeof(ind_p1));
        memcpy(ind_p2, indicator_file + 2 + sizeof(ind_p1) + 2, sizeof(ind_p2));
        memcpy(bot_p1, bottom_file, sizeof(bot_p1));
        memcpy(bot_p2, bottom_file + sizeof(bot_p1), sizeof(bot_p2));
        rng_set_rom(random_rom);
    } else {
        fill(0, 0, SCREEN_W, SCREEN_H, 10 /* bright red */);
    }
    set_rocket_bonus_sprite_height(ROCKET_BONUS_H_PX);

    high_score_name[0] = high_score_name[1] = high_score_name[2] = 0x0A;
    high_score_load(&high_score, high_score_name);
    timer_install();
    kbd_install();

    while (state != ST_QUIT) {
        switch (state) {
            case ST_TITLE:   state = run_title();   break;
            case ST_MENU:    state = run_menu();    break;
            case ST_HISCORE: state = run_hiscore(); break;
            case ST_LEVEL:   state = run_level();   break;
            default:         state = ST_QUIT;       break;
        }
    }

    kbd_restore();
    timer_restore();
    write_profile_report();
    return 0;
}
