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

#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_W      320
#define SCREEN_H      200
#define PLAYFIELD_W   256
#define PLAYFIELD_H   192
#define BORDER_X      ((SCREEN_W - PLAYFIELD_W) / 2)   /* 32 */
#define BORDER_Y      ((SCREEN_H - PLAYFIELD_H) / 2)   /*  4 */

#define COL_BORDER    0x00   /* black border; the loading screen has its own paper */

static unsigned char __far *vga = (unsigned char __far *)0xA0000000L;

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

static void fill(int x, int y, int w, int h, unsigned char c) {
    int row;
    for (row = 0; row < h; row++) {
        _fmemset(vga + (long)(y + row) * SCREEN_W + x, c, (size_t)w);
    }
}

#define SCREEN_CHUNK_ROWS 16
static unsigned char screen_chunk[SCREEN_CHUNK_ROWS * PLAYFIELD_W];

static void clear_playfield_border(void) {
    fill(0, 0, SCREEN_W, BORDER_Y, COL_BORDER);
    fill(0, BORDER_Y + PLAYFIELD_H, SCREEN_W,
         SCREEN_H - BORDER_Y - PLAYFIELD_H, COL_BORDER);
    fill(0, BORDER_Y, BORDER_X, PLAYFIELD_H, COL_BORDER);
    fill(BORDER_X + PLAYFIELD_W, BORDER_Y,
         SCREEN_W - BORDER_X - PLAYFIELD_W, PLAYFIELD_H, COL_BORDER);
}

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
            _fmemcpy(vga + (long)(BORDER_Y + y + r) * SCREEN_W + BORDER_X,
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

/* 36 × 6 = 216 B. Small enough to live in near data, big enough to
 * justify loading from disk once rather than embedding as a const. */
#define FONT_N      43
#define FONT_ROWS   6
static unsigned char font[FONT_N * FONT_ROWS];

static int load_font(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fread(font, 1, sizeof(font), f) != sizeof(font)) {
        fclose(f);
        return -2;
    }
    fclose(f);
    return 0;
}

/* Draw glyph `code` (0..35) at VGA pixel (x, y) using palette index
 * `color`. Bits are OR'd onto whatever's there; pixels with bit 0
 * are left as-is. Glyph is 6 rows × 8 cols. */
static void draw_glyph(int x, int y, unsigned char color, unsigned char code) {
    int r, i;
    unsigned char b;
    unsigned char __far *dst;
    if (code >= FONT_N) return;
    dst = vga + (long)y * SCREEN_W + x;
    for (r = 0; r < FONT_ROWS; r++) {
        b = font[code * FONT_ROWS + r];
        for (i = 0; i < 8; i++) {
            if (b & (0x80 >> i)) dst[i] = color;
        }
        dst += SCREEN_W;
    }
}

/* ZX attribute byte (paper=0 always here) -> our palette index. */
static unsigned char attr_to_palette(unsigned char attr) {
    /* bit 6 = bright, bits 0..2 = ink colour. */
    unsigned char bright = (attr >> 6) & 1;
    unsigned char ink    = attr & 7;
    return (unsigned char)(bright * 8 + ink);
}

/* The markup buffer (extracted from snap1 RAM at 0x8FD1).
 * Format (see notes/encoding.md):
 *   header row: <0x38|0x30> Y attr count digits... 0x24
 *   data row:   <0x58>      Y attr count <count payload bytes>
 *   payload:    0x00..0x09 digit, 0x0A..0x23 letter,
 *               0x26 space, 0x40..0x47 inline colour change.
 * Records terminate when the next row-marker (0x58|0x38|0x30) is seen
 * or at 0x24 (end-of-field). */
#define MARKUP_MAX 512
static unsigned char markup[MARKUP_MAX];
static int markup_len;

static int load_markup(const char *path) {
    FILE *f = fopen(path, "rb");
    int n;
    if (!f) return -1;
    n = fread(markup, 1, MARKUP_MAX, f);
    fclose(f);
    markup_len = n;
    return (n > 0) ? 0 : -2;
}

/* All row markers observed are multiples of 8 in [0x30, 0x70):
 *   0x30 0x38 0x40 0x50 0x58 0x60 0x68 …
 * And `marker / 8` is the X column in cells (0x30→6, 0x68→13).
 * One record-walker handles them all. */
static int is_row_marker(unsigned char b) {
    /* Any non-zero multiple of 8 — col = b / 8. Observed markers
     * span col 2 (0x10, "000000" P1 score) through col 25 (0xC8,
     * "2 UP" label). Zero is filtered out so it's never a marker. */
    return b != 0 && (b & 7) == 0;
}

/* Record: marker | Y | attr | count | count payload bytes.
 * Payload bytes: 0x00-0x09 = digit, 0x0A-0x23 = letter, 0x24-0x2A =
 * specials (period/comma/space/dash/_/II/=), 0x40-0x4F = in-band
 * colour escape. */
static int render_record(int p) {
    unsigned char marker = markup[p++];
    unsigned char y_pix  = markup[p++];
    unsigned char attr   = markup[p++];
    unsigned char count  = markup[p++];
    unsigned char colour = attr_to_palette(attr);
    int x = BORDER_X + (int)(marker / 8) * 8;
    int y = BORDER_Y + y_pix - 5;
    int i;
    for (i = 0; i < count; i++) {
        unsigned char c = markup[p++];
        if (c == 0x26) {
            /* explicit no-draw — the font's space glyph might have
             * stray bits we don't want painted. */
        } else if (c <= 0x2A) {
            draw_glyph(x, y, colour, c);
        } else if (c >= 0x40 && c <= 0x4F) {
            colour = attr_to_palette(c);
            x -= 8;             /* attribute is in-band: don't advance X */
        }
        x += 8;
    }
    return p;
}

static void render_markup(void) {
    int p = 0;
    while (p < markup_len) {
        if (is_row_marker(markup[p])) p = render_record(p);
        else                          p++;
    }
}

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

static int load_indicator(const char *path) {
    FILE *f = fopen(path, "rb");
    unsigned char hdr[2];
    if (!f) return -1;
    if (fread(hdr, 1, 2, f) != 2 ||
        fread(ind_p1, 1, sizeof(ind_p1), f) != sizeof(ind_p1) ||
        fread(hdr, 1, 2, f) != 2 ||
        fread(ind_p2, 1, sizeof(ind_p2), f) != sizeof(ind_p2)) {
        fclose(f); return -2;
    }
    fclose(f);
    return 0;
}

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

static int load_bottom_sprites(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fread(bot_p1, 1, sizeof(bot_p1), f) != sizeof(bot_p1) ||
        fread(bot_p2, 1, sizeof(bot_p2), f) != sizeof(bot_p2)) {
        fclose(f); return -2;
    }
    fclose(f);
    return 0;
}

#define HUD_SPRITES_SIZE 0x0128
#define HUD_SPR_1UP      0x0000
#define HUD_SPR_2UP      0x0032
#define HUD_SPR_HI       0x0064
#define HUD_SCORE_DIGITS 0x0086
static unsigned char hud_sprites[HUD_SPRITES_SIZE];

static int load_hud_sprites(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fread(hud_sprites, 1, sizeof(hud_sprites), f) != sizeof(hud_sprites)) {
        fclose(f); return -2;
    }
    fclose(f);
    return 0;
}

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
#define LVL_SIZE   (N_LEVELS * LVL_CELLS)
/* Per-level attribute band: FULL 24 char-rows x 32 cols of ZX
 * attribute bytes captured from each level's GT .scr.
 * Lookup: attr = level_attrs[lvl*ATTR_BAND_SIZE + r*32 + col]
 * where r is the char-row index (0..23):
 *    0..1   top HUD
 *    2..13  brick zone (rendered by render_brick_band)
 *   14..21  side-frame interior
 *   22..23  bottom (bat / lives) */
#define ATTR_ROWS       24
#define ATTR_COLS       32
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
 *
 * `scr_buff` and `attr_buff` mirror the original's offscreen buffers
 * at $DA00 / $D700. Layout: scr_buff is row-major, 32 bytes/row * 192
 * rows; attr_buff is 32 cols * 24 rows of ZX attribute bytes. Pixel
 * (x, y) lives at scr_buff[y*32 + x/8] bit (7 - x%8); attribute at
 * attr_buff[(y/8)*32 + x/8].
 *
 * `spr_brik_1` and `briks_colors` are pulled verbatim from
 * original/disasm/gfx/briks.asm and original/disasm/batty.asm (label
 * `briks_colors`, 1-indexed by the brick code's low nibble). */
static unsigned char scr_buff[6144];
static unsigned char attr_buff[768];
static void fast_memcpy(void *dest, const void *src, unsigned int n_bytes);

static const unsigned char spr_brik_1[16] = {
    0xFF, 0xFE, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00,
    0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x00, 0x00
};

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

/* Mirror of `briks_colors` at $AEEC. Original ASM uses
 *   LD HL,briks_colors-$01 ; CALL hl_add_a (A = cell low nibble 1..14)
 * so it reads briks_colors[low_nibble - 1]. Our C-side does
 *   briks_colors[iy_byte & 0x0F]
 * which expects 1-based indexing — slot [0] is a never-indexed placeholder. */
static const unsigned char briks_colors[16] = {
    0x00,                          /* [0] never indexed (low nibble == 0
                                    * means "skip" per the cell format). */
    0x57, 0x4F, 0x5F, 0x20, 0x70,  /* [1..5] normal bricks */
    0x47, 0x57, 0x5F, 0x4F,        /* [6..9] hard (multi-hit) bricks */
    0x00,                          /* [10] unused per original */
    0x47, 0x57, 0x4F, 0x5F,        /* [11..14] indestructible bricks (e.g. L5 $2E) */
    0x00                           /* [15] unused per original */
};

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
/* Collision body — original object_bullet_1 has (IX+$0C) = $04 and
 * (IX+$0D) = $08 (= 4x8 hitbox). The sprite is 8 px wide but only
 * the right 4 are opaque; obj_compare reads the body fields, not the
 * sprite extents. Earlier port used BULLET_W_PX for AABB which
 * gave the bullet a wider hitbox than the original. */
#define BULLET_BODY_W   4
#define BULLET_BODY_H   8
/* Original handling_bullet at \$A5A3 advances bullet Y by 6 px/tick
 * (`LD A,(IX+\$04); SUB \$06`). We had 4 which made bullets visibly
 * slower than the original's. */
#define BULLET_SPEED    6
/* Two-bullet pool — original at $A0FA tries object_bullet_1 then
 * object_bullet_2 so the player can have up to 2 in flight. We had
 * one slot, capping rapid-fire to one bullet per ~30 frames. */
#define N_BULLETS 2
static unsigned char bullet_active[N_BULLETS] = {0, 0};
static int           bullet_x[N_BULLETS]      = {0, 0};
static int           bullet_y[N_BULLETS]      = {0, 0};
/* Bullet-impact blast: 4 frames, ~3 ticks each. Spawned wherever
 * step_bullet's collision deactivates the bullet. Per-slot so two
 * simultaneous bullets get their own blast on impact. */
#define BULLET_BLAST_TICKS_PER_FRAME 3
#define BULLET_BLAST_FRAMES          4
static unsigned char bullet_blast_ticks[N_BULLETS] = {0, 0};
static int           bullet_blast_x[N_BULLETS]     = {0, 0};
static int           bullet_blast_y[N_BULLETS]     = {0, 0};
/* Bat laser-fire animation: ticks down from 8 to 0; while non-zero
 * render_bat picks spr_bat_gun_1..4 based on the count so the bat's
 * cannon visibly flashes when SPACE fires a bullet. */
static unsigned char bat_fire_anim_ticks = 0;
/* Laser fire cooldown — port of the `bullet` counter at $A160. Original
 * sets it to ~\$16 (=22) on each fire, then `SUB \$02` per frame; SPACE
 * is ignored until the counter underflows. Net effect: ~11 frames
 * between shots regardless of how fast SPACE is mashed. */
static unsigned char bullet_cooldown = 0;

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
#define RANDOM_ROM_SIZE 0x2000
static unsigned char __far *random_rom = NULL;
static unsigned char random_e = 0x17;
static unsigned char random_d = 0x8E;
static unsigned int  random_seed_addr = 0x8000;

static unsigned char ball_dir_from_delta(int dx, int dy) {
    unsigned char q;
    unsigned char d;
    if (dx >= 0 && dy >= 0) q = 0x00;
    else if (dx >= 0)       q = 0x10;
    else if (dy < 0)        q = 0x20;
    else                    q = 0x30;
    d = (abs(dx) >= BALL_SPEED) ? 0x08 : 0x04;
    return (unsigned char)(q | d);
}

static void ball_delta_from_dir(unsigned char dir, int *dx, int *dy) {
    int mag_x;
    int mag_y;
    switch (dir & 0x0F) {
        case 0x04: mag_x = 2; mag_y = 1; break;
        case 0x08: mag_x = 1; mag_y = 1; break;
        default:   mag_x = 1; mag_y = 2; break;
    }
    switch (dir & 0x30) {
        case 0x00: *dx =  mag_x; *dy =  mag_y; break;
        case 0x10: *dx =  mag_x; *dy = -mag_y; break;
        case 0x20: *dx = -mag_x; *dy = -mag_y; break;
        default:   *dx = -mag_x; *dy =  mag_y; break;
    }
}

/* Brick destruction dirty marker. The original remove path restores
 * background/window data; it does not paint a bright-white replacement
 * block. Keep the most recent cell live for a couple of ticks only so
 * dirty redraw includes print_one_brik_buf's wider 18x10 footprint. */
#define BRICK_FLASH_TICKS 2
static unsigned char brick_flash_ticks = 0;
static int           brick_flash_x     = 0;
static int           brick_flash_y     = 0;
static void render_brick_flash_to_buff(void);    /* forward decl — defined alongside brick_collision */

/* Original briks_data: up to five simultaneous hard-brick shimmer
 * animations after a non-destroying hit. Each slot lasts 16 ticks:
 * anim_brik's eight frames, two ticks per frame. */
#define BRICK_HIT_ANIM_SLOTS 5
#define BRICK_HIT_ANIM_TICKS 16
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
#define HIGH_SCORE_FILE "HISCORE.DAT"
static void load_high_score(void) {
    FILE *f = fopen(HIGH_SCORE_FILE, "rb");
    unsigned char buf[7];
    size_t n;
    high_score = 0;
    high_score_name[0] = 0x0A;
    high_score_name[1] = 0x0A;
    high_score_name[2] = 0x0A;
    if (!f) return;
    n = fread(buf, 1, 7, f);
    if (n >= 4) {
        high_score =  (unsigned long)buf[0]
                   | ((unsigned long)buf[1] <<  8)
                   | ((unsigned long)buf[2] << 16)
                   | ((unsigned long)buf[3] << 24);
    }
    if (n >= 7) {
        /* Three name bytes appended (post-v2 file). Pre-v2 files
         * stop after 4 bytes; the AAA default above stays. */
        high_score_name[0] = buf[4];
        high_score_name[1] = buf[5];
        high_score_name[2] = buf[6];
    }
    fclose(f);
}
static void save_high_score(void) {
    FILE *f = fopen(HIGH_SCORE_FILE, "wb");
    unsigned char buf[7];
    if (!f) return;
    buf[0] = (unsigned char)(high_score & 0xFF);
    buf[1] = (unsigned char)((high_score >> 8) & 0xFF);
    buf[2] = (unsigned char)((high_score >> 16) & 0xFF);
    buf[3] = (unsigned char)((high_score >> 24) & 0xFF);
    buf[4] = high_score_name[0];
    buf[5] = high_score_name[1];
    buf[6] = high_score_name[2];
    fwrite(buf, 1, 7, f);
    fclose(f);
}

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
#define BONUS_TYPE_LIFE         0
#define BONUS_TYPE_SLOW         1
#define BONUS_TYPE_BIG_BAT      2
#define BONUS_TYPE_BIG_BALL     3
#define BONUS_TYPE_KILL_ALIENS  4
#define BONUS_TYPE_CATCH        5
#define BONUS_TYPE_ROCKET       6
#define BONUS_TYPE_SCORE_5K     7
#define BONUS_TYPE_LASER        8
#define BONUS_TYPE_MULTI_BALL   9
#define BONUS_TYPE_COUNT        10
#define BONUS_TYPE_UNSUPPORTED  0xFF

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

static unsigned char map_orig_to_our_bonus(unsigned char code) {
    switch (code) {
        case 0x00: return BONUS_TYPE_BIG_BAT;       /* spr_bonus_size */
        case 0x01: return BONUS_TYPE_LASER;         /* spr_bonus_gun */
        case 0x02: return BONUS_TYPE_MULTI_BALL;    /* spr_bonus_triple_ball */
        case 0x03: return BONUS_TYPE_CATCH;         /* spr_bonus_hand */
        case 0x04: return BONUS_TYPE_SLOW;
        case 0x05: return BONUS_TYPE_LIFE;
        case 0x06: return BONUS_TYPE_ROCKET;        /* spr_bonus_rocket_1 */
        case 0x07: return BONUS_TYPE_BIG_BALL;
        case 0x08: return BONUS_TYPE_SCORE_5K;      /* spr_bonus_5000_points */
        case 0x09: return BONUS_TYPE_KILL_ALIENS;
        default:   return BONUS_TYPE_UNSUPPORTED;
    }
}

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
/* SLOW is permanent within a life in the original — ball speed is
 * set at $A67B_7 and not auto-restored. all_var_init at level/life
 * entry resets ball speed back to $03 (= our BALL_SPEED). Earlier
 * port auto-expired SLOW after 5 sec, which made the bonus
 * pointless. Setting effectively-forever so respawn_primary_ball /
 * level entry are the only way out, matching the original. */
#define SLOW_DURATION     0xFFFFu
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
static unsigned int  slow_ticks      = 0;
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
#define SPR_BIRD_1       (0x860E - 0x7A8C)   /* = 0xb82 */
#define SPR_BIRD_2       (0x866A - 0x7A8C)   /* = 0xbde */
#define SPR_BIRD_3       (0x86C6 - 0x7A8C)   /* = 0xc3a */

/* Frame tables for alien animation. The original cycles via the
 * sprite_num field per the per-tick logic in handling_bird at $A9BC;
 * we walk these arrays directly using descriptor's misc_12 as the
 * tick counter and (misc_12 >> 2) % N_FRAMES as the frame index. */
static const unsigned int spr_bird_frames[3] = { SPR_BIRD_1, SPR_BIRD_2, SPR_BIRD_3 };
static const unsigned int spr_ufo_frames[3]  = { SPR_UFO_1,  SPR_UFO_2,  SPR_UFO_3  };

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
static const unsigned int spr_blast_frames[5] = {
    SPR_BLAST_1, SPR_BLAST_2, SPR_BLAST_3, SPR_BLAST_4, SPR_BLAST_5
};
static const unsigned int spr_spark_frames[5] = {
    SPR_SPARK_1, SPR_SPARK_2, SPR_SPARK_3, SPR_SPARK_4, SPR_SPARK_5
};
#define BLAST_FRAMES 5
#define BLAST_TICKS_PER_FRAME 3
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

static int load_levels(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fread(levels, 1, sizeof(levels), f) != sizeof(levels)) {
        fclose(f); return -2;
    }
    fclose(f);
    return 0;
}

static int load_level_attrs(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fread(level_attrs, 1, sizeof(level_attrs), f) != sizeof(level_attrs)) {
        fclose(f); return -2;
    }
    fclose(f);
    return 0;
}

static int load_bg_tile(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fread(bg_tile, 1, sizeof(bg_tile), f) != sizeof(bg_tile)) {
        fclose(f); return -2;
    }
    fclose(f);
    return 0;
}

static int load_sprites(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fread(sprites_blob, 1, sizeof(sprites_blob), f) != sizeof(sprites_blob)) {
        fclose(f); return -2;
    }
    fclose(f);
    return 0;
}

static int load_random_rom(const char *path) {
    FILE *f = fopen(path, "rb");
    unsigned int off = 0;
    if (!f) return -1;
    if (random_rom == NULL) {
        random_rom = _fmalloc(RANDOM_ROM_SIZE);
        if (random_rom == NULL) {
            fclose(f); return -3;
        }
    }
    while (off < RANDOM_ROM_SIZE) {
        unsigned int n = RANDOM_ROM_SIZE - off;
        if (n > sizeof(screen_chunk)) n = sizeof(screen_chunk);
        if (fread(screen_chunk, 1, n, f) != n) {
            fclose(f); return -2;
        }
        _fmemcpy(random_rom + off, screen_chunk, n);
        off += n;
    }
    fclose(f);
    return 0;
}

static int load_frame(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fread(frame_l1, 1, sizeof(frame_l1), f) != sizeof(frame_l1)) {
        fclose(f); return -2;
    }
    fclose(f);
    return 0;
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
static unsigned short vga_attr_nibble_words[128][16][2];

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
            vga_attr_nibble_words[i][n][0] = (unsigned short)(p0 | ((unsigned short)p1 << 8));
            vga_attr_nibble_words[i][n][1] = (unsigned short)(p2 | ((unsigned short)p3 << 8));
        }
    }
}

static unsigned long prof_bg_pit = 0;
static unsigned long prof_frame_pit = 0;
static unsigned long prof_hud_pit = 0;
static unsigned long prof_bricks_pit = 0;
static unsigned long prof_vga_pit = 0;
static unsigned long prof_frames_count = 0;
static unsigned long prof_vga_rects = 0;
static unsigned long prof_vga_bytes = 0;
static unsigned long prof_static_rebuilds = 0;
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
 * L3 ball gate stays byte-exact with the flag ON. NOT YET the default
 * because the RNG-*dependent* behaviour has no byte-exact gate of its own
 * (the ball gate is RNG-independent, and the enemy target isn't
 * reproducible across separate-run builds due to WAIT_KEY release-timing
 * jitter). flag-ON IS behaviour-correct for the enemy (stable target,
 * repicks once on arrival — the earlier "thrash" was a separate-run
 * measurement artifact, see notes/enemy-movement.md). The flip stays
 * deliberate pending a real RNG-behaviour gate (a single multi-frame
 * probing run). Flag OFF (default) is fine regardless. */
static unsigned char rng_perframe = 0;
static unsigned char suppress_no_ball_death = 0;
static int sound_disabled = 0;

static unsigned short last_prof_tick = 0;

static unsigned short pit_current_ticks(void) {
    unsigned char low, high;
    unsigned short val;
    _disable();
    outp(0x43, 0x00);
    low = inp(0x40);
    high = inp(0x40);
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
        diff = (last_prof_tick - now) + 23864u;
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
        fprintf(f, "  sound disabled:       %u\n", (unsigned)sound_disabled);
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
            fast_memcpy(&scr_buff[(y0_px + y) * 32 + byte_col_off],
                        &pixels[y * cols_bytes],
                        (unsigned int)cols_bytes);
        }
        for (char_row = 0; char_row < char_rows; char_row++) {
            fast_memcpy(&attr_buff[(char_row_off + char_row) * 32 + byte_col_off],
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
        fast_memcpy(&scr_buff[y * 32 + 8], &top_px[y * 32 + 8], 3);
    }
    for (cr = 0; cr < FRAME_TOP_H_PX / 8; cr++) {
        fast_memcpy(&attr_buff[cr * 32 + 8], &lattr[cr * 32 + 8], 3);
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
 *   shadow rows pick up mask=1 bits on their own. */
static void blit_masked_sprite_ptr(const unsigned char *src,
                                    int x_px, int y_px,
                                    unsigned char ink, unsigned char paper);

static void blit_masked_sprite(unsigned int sprite_off, int x_px, int y_px,
                               unsigned char ink, unsigned char paper) {
    blit_masked_sprite_ptr(sprites_blob + sprite_off, x_px, y_px, ink, paper);
}

/* Original blit into the 1-bit scr_buff: per byte,
 *   scr_buff' = (mask | scr_buff) ^ pixel
 * mirroring sub_94BC. Three useful pixel outcomes:
 *   - mask=1, pix=0  -> bit forced to 1 (solid body, ink colour in
 *     the buff_to_vga pass).
 *   - mask=1, pix=1  -> bit forced to 0 (sprite's internal texture,
 *     paper colour).
 *   - mask=0, pix=1  -> bit inverted (XOR shadow — toggles the bg
 *     pattern bit at that pixel; what produces the dotted bat-shadow
 *     band on rows 10..12).
 *   - mask=0, pix=0  -> bit preserved (transparent).
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

static void blit_masked_to_scr_buff(unsigned int sprite_off,
                                     int x_px, int y_px) {
    blit_masked_to_scr_buff_ptr(sprites_blob + sprite_off, x_px, y_px);
}

/* Port of print_sprite_attrib @ $B656. Sets `attr` in every attr_buff
 * char cell the sprite's bounding box overlaps, so a buff_to_vga pass
 * colours the sprite (and any bg pattern bits in the same cells)
 * with the new ink/paper. Mirrors the original's per-cell colour-clash
 * behaviour where a sprite repaints its cells' attrs without checking
 * which bits inside the cell are sprite vs bg. */
static void blit_sprite_attrs_to_buff(int x_px, int y_px,
                                       int w_px, int h_px,
                                       unsigned char attr) {
    int col_lo = x_px / 8;
    int col_hi = (x_px + w_px - 1) / 8;
    int row_lo = y_px / 8;
    int row_hi = (y_px + h_px - 1) / 8;
    int r, c;
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

/* Bat ink + paper come from the BG attr at the bat's position - same
 * palette as the surrounding hex pattern, as in the original. The bat
 * texture-detail (mask=1, pixel=1 pixels) renders as paper colour;
 * body pixels (mask=1, pixel=0) render as ink. */
/* --- Object model (port of the 22-byte descriptor at $9AD0+) ----------
 *
 * Mirror of the original's per-object property block. Field names
 * match the disasm comment at line 1280+. The 11-slot table lives at
 * $9AD0..$9BC1 in the original game's RAM; here we keep it as a
 * fixed-order array indexed by symbolic OBJ_* constants. The 3
 * special slots (lives indicator, score indicator, player separator)
 * sit after the main 11 and are NOT walked by call_hl_for_all_obj.
 *
 * sprite_set value selects the per-object handler via
 * handling_table_routines ($9F35); BIT7=1 marks the slot inactive
 * (off-screen / not processed this frame). */
typedef struct {
    unsigned char sprite_set;        /* +00  set id; BIT7=1 inactive */
    unsigned char sprite_num;        /* +01  sprite within set */
    unsigned char x_coord;           /* +02  X (px) */
    unsigned char x_coord_hi;        /* +03  X high byte */
    unsigned char y_coord;           /* +04  Y (px) */
    unsigned char y_coord_hi;        /* +05  Y high byte */
    unsigned char dir;               /* +06  ball direction */
    unsigned char speed;             /* +07  movement speed */
    unsigned char w_shadow;          /* +08  sprite width in bytes (incl shadow) */
    unsigned char h_shadow;          /* +09  sprite height in px (incl shadow) */
    unsigned char buf_addr_hi;       /* +0A  high byte of scr_buff address */
    unsigned char buf_addr_lo;       /* +0B  low byte */
    unsigned char w_body_px;         /* +0C  body width in px */
    unsigned char h_body_px;         /* +0D  body height in px */
    unsigned char prev_x;            /* +0E */
    unsigned char prev_y;            /* +0F */
    unsigned char prev_w_shadow;     /* +10 */
    unsigned char prev_h_shadow;     /* +11 */
    unsigned char misc_12;           /* +12  enemy-specific */
    unsigned char misc_13;           /* +13  enemy / 3-ball slow */
    unsigned char bonus_applied;     /* +14  $FF = none */
    unsigned char bat_props;         /* +15  bat-state flags
                                      *      BIT0 expanded
                                      *      BIT1 expanding
                                      *      BIT5 expansion in progress
                                      *      BIT6 reduction in progress
                                      *      BIT7 not in transformation */
} object_t;

/* Slot indices match the original iteration order:
 *   call_hl_for_all_obj starts at object_ball_1, advances by $16 (22)
 *   bytes 11 times. */
#define OBJ_BALL_1     0
#define OBJ_BALL_2     1
#define OBJ_BALL_3     2
#define OBJ_BULLET_1   3
#define OBJ_BULLET_2   4
#define OBJ_BAT_2      5
#define OBJ_BAT_1      6
#define OBJ_BAT_TEMP   7
#define OBJ_BONUS      8
#define OBJ_ENEMY      9
#define OBJ_ROCKET    10
#define N_OBJECTS     11

/* Initial values are byte-exact copies of the DEFB blocks at $9AD0+. */
static object_t objects[N_OBJECTS] = {
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

static void ball_dir_delta_q8(unsigned char dir, unsigned char speed,
                              int *dx_q8, int *dy_q8);
static void dir_to_dxdy(unsigned char dir, unsigned char speed,
                        int *out_dx, int *out_dy);

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
    ball_dir_delta_q8(dir, BALL_SPEED, &ball_dx, &ball_dy);
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

typedef void (*obj_handler_t)(object_t *obj);

static void handling_bat_stub(object_t *o)  { (void)o; }
static void handling_ball_obj(object_t *o)  { (void)o; }
static void handling_bonus_obj(object_t *o) { (void)o; }
static void handling_bullet_obj(object_t *o){ (void)o; }
static void handling_rocket_obj(object_t *o){ (void)o; }
/* L4's spark enemy: short-lived bouncing dot that decays through
 * 5 sprite frames before vanishing. dir bit 0 = X heading (0=right,
 * 1=left); bit 1 = Y heading (0=down, 1=up). Frame index ramps up
 * with misc_12 against a decay threshold table — rough port of the
 * original's halving-timer mechanic, simpler to reason about. */
static const unsigned char spark_frame_threshold[SPARK_FRAMES] = {
    16, 24, 28, 30, 31
};
static void handling_spark_obj(object_t *o) {
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
/* Port of handling_blast at $AA30. Advances the blast frame counter
 * via misc_12 (tick) and sprite_num (frame index = misc_12 / 3 mod 5).
 * Original deactivates when sprite_num reaches 9; we deactivate when
 * we've shown all BLAST_FRAMES frames. */
static void handling_blast_obj(object_t *o) {
    o->misc_12++;
    if (o->misc_12 >= BLAST_FRAMES * BLAST_TICKS_PER_FRAME) {
        /* Clear sprite_set to 0 (= empty slot) so enemy_prepare can
         * spawn a fresh alien next time the spawn conditions hit. The
         * original "SET 7,(IX+\$00)" leaves sprite_set as \$8A; combined
         * with our enemy_prepare check of "sprite_set != 0" that would
         * permanently block respawns for the rest of the level. */
        o->sprite_set = 0;
        return;
    }
    o->sprite_num = (unsigned char)(o->misc_12 / BLAST_TICKS_PER_FRAME);
}
static void handling_400pts_obj(object_t *o){ (void)o; }

/* (Removed enemy_dir_delta_q8 + direction_table_q8: the enemy now moves
 * with the exact dir_to_dxdy / hl_bc_calc_direction, like the ball and the
 * original's LAD69. The old routine had the X/Y components swapped per
 * quadrant, flying the bird on the wrong axis.) */

static void ball_dir_delta_q8(unsigned char dir, unsigned char speed,
                              int *dx_q8, int *dy_q8) {
    /* Use the exact hl_bc_calc_direction port (dir_to_dxdy). */
    dir_to_dxdy(dir, speed, dx_q8, dy_q8);
}

static void ball_reflect_descriptor(int flip_x, int flip_y) {
    int dx_q8, dy_q8;
    unsigned char dir = objects[OBJ_BALL_1].dir;
    if (flip_x) dir = (unsigned char)((0x3F - dir) & 0x3F);
    if (flip_y) dir = (unsigned char)((0x1F - dir) & 0x3F);
    objects[OBJ_BALL_1].dir = dir;
    ball_dir_delta_q8(dir, objects[OBJ_BALL_1].speed, &dx_q8, &dy_q8);
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
static unsigned int dbg_enemy_arrival_repicks = 0;  /* diag: LAA7D_1 fires */
static unsigned int dbg_enemy_margin_repicks  = 0;  /* diag: margin path fires */
static unsigned int dbg_enemy_turn_calls      = 0;  /* diag: turn fn called */
static void enemy_turn_towards_target(object_t *o) {
    unsigned char target = (unsigned char)(o->bonus_applied & 0x3F);
    unsigned char delta = (unsigned char)((o->dir - target) & 0x3F);
    dbg_enemy_turn_calls++;
    if (delta == 0) {
        dbg_enemy_arrival_repicks++;
        o->bonus_applied = (unsigned char)(random_e & 0x3F);   /* LAA7D_1 */
        return;
    }
    if (delta & 0x20) o->dir = (unsigned char)((o->dir + 1) & 0x3F);
    else             o->dir = (unsigned char)((o->dir - 1) & 0x3F);
}

static void enemy_pick_new_target(object_t *o) {
    /* Read-current consumer (rng_sample): the original samples
     * random_number for the enemy target without its own advance. */
    o->bonus_applied = (unsigned char)(random_lo(rng_sample()) & 0x3F);
}

static void enemy_target_away_from_margins(object_t *o) {
    dbg_enemy_margin_repicks++;
    if (o->x_coord <= 8) {
        o->bonus_applied = (o->y_coord <= 12) ? 0x08 : 0x00;
    } else if (o->x_coord >= PLAYFIELD_W - 8 - o->w_body_px) {
        o->bonus_applied = (o->y_coord <= 12) ? 0x38 : 0x20;
    } else if (o->y_coord <= 8) {
        o->bonus_applied = (o->x_coord < PLAYFIELD_W / 2) ? 0x08 : 0x38;
    } else {
        enemy_pick_new_target(o);
    }
}

/* Birds/UFOs use a reduced port of the original 6-bit direction-table
 * movement. The original also uses LAA7B target steering and collision
 * reactions; here we keep the same q8.8 motion shape and periodically
 * steer to a new target so enemies roam through the playfield instead
 * of patrolling only along the top edge. */
static void bomb_appear(object_t *o);     /* forward decl */
static void handling_bird_obj(object_t *o) {
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
    o->misc_12++;
    o->sprite_num = (unsigned char)((o->misc_12 >> 2) % 3);
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
        enemy_turn_towards_target(o);
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
        enemy_target_away_from_margins(o);
    } else if (nx >= PLAYFIELD_W - 8 - (int)o->w_body_px) {
        nx = PLAYFIELD_W - 8 - (int)o->w_body_px; nx_q8 = (long)nx << 8;
        o->dir = (unsigned char)((0x20 - o->dir) & 0x3F);
        o->x_coord = (unsigned char)nx;
        o->y_coord = (unsigned char)ny;
        enemy_target_away_from_margins(o);
    }
    if (ny < 8) {
        ny = 8; ny_q8 = (long)ny << 8;
        o->dir = (unsigned char)((0x40 - o->dir) & 0x3F);
        o->x_coord = (unsigned char)nx;
        o->y_coord = (unsigned char)ny;
        enemy_target_away_from_margins(o);
    } else if (ny >= PLAYFIELD_H) {
        o->sprite_set |= 0x80;
        return;
    }
    o->x_coord = (unsigned char)nx;
    o->x_coord_hi = (unsigned char)(nx_q8 & 0xFF);
    o->y_coord = (unsigned char)ny;
    o->y_coord_hi = (unsigned char)(ny_q8 & 0xFF);
}
static void handling_ufo_obj(object_t *o) { handling_bird_obj(o); }

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
static void handling_object(object_t *obj) {
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
static void call_for_all_obj(obj_handler_t fn) {
    int i;
    for (i = 0; i < N_OBJECTS; i++) {
        if (objects[i].sprite_set != 0) fn(&objects[i]);
    }
}

/* Port of ix_buf_addr_calc at $B684. Computes the scr_buff offset
 * from (x_coord, y_coord) and stores it in the descriptor's +0A/+0B
 * fields. Our scr_buff is row-major (32 B per row, 192 rows), so the
 * offset is y*32 + x/8. We pack big-endian into +0A:+0B to match the
 * original's H:L convention. */
static void ix_buf_addr_calc(object_t *obj) {
    unsigned int off = (unsigned int)obj->y_coord * 32u
                     + (unsigned int)(obj->x_coord >> 3);
    obj->buf_addr_hi = (unsigned char)((off >> 8) & 0xFF);
    obj->buf_addr_lo = (unsigned char)(off & 0xFF);
}
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

static int test_mode_pin_blink;   /* forward — defined further down */

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


/* --- Brick compositor (port of $ADE1..$AEEC) -------------------------- */

/* Cursor offsets within scr_buff / attr_buff, walked by print_briks_c.
 * Mirror the original brik_addr_buf and brik_attr_buf. */
static unsigned int brik_addr_buf;
static unsigned int brik_attr_buf;

/* Port of print_one_brik_buf ($AE82). Paints one brick into scr_buff at
 * `hl` (the top-left byte of the brick's 2-byte body), plus decoration:
 *   - 2 zero bytes one pixel-row above the brick (top edge)
 *   - bit 0 cleared in the 8 bytes left of the brick (left edge),
 *     unless the brick sits in the leftmost column
 *   - 2 zero bytes one pixel-row below (bottom edge)
 *   - bit 7 cleared in the 8 bytes right of the brick (right edge),
 *     unless the brick sits in the rightmost column
 * Finally the brick's two char cells in attr_buff are set to the colour
 * from briks_colors keyed by the descriptor's low nibble. */
static void print_one_brik_buf_c(unsigned int hl, unsigned char iy_byte) {
    unsigned int h;
    int col_byte = (int)(hl & 0x1F);
    int i;
    unsigned int brick_top_y_px = hl >> 5;
    unsigned int attr_off       = (brick_top_y_px >> 3) * 32u + (unsigned int)col_byte;
    unsigned char attr;

    /* Top edge: 2 zero bytes one pixel-row above the brick. */
    scr_buff[hl - 32] = 0;
    scr_buff[hl - 31] = 0;

    /* Left edge: only if not the leftmost brick column (col_byte == 1). */
    if (col_byte != 1) {
        h = hl - 1;
        for (i = 0; i < 8; i++) { scr_buff[h] &= 0xFE; h += 32; }
    }

    /* Body: 8 rows * 2 bytes from spr_brik_1. */
    h = hl;
    for (i = 0; i < 8; i++) {
        scr_buff[h]     = spr_brik_1[i * 2];
        scr_buff[h + 1] = spr_brik_1[i * 2 + 1];
        h += 32;
    }
    /* h == hl + 256 now (= byte at col 0 of the row one below the brick). */

    /* Bottom edge: 2 zero bytes one pixel-row below the brick. */
    scr_buff[h]     = 0;
    scr_buff[h + 1] = 0;

    /* Right edge: only if not the rightmost brick column. The original
     * tests `(L+1) AND $1F == $1E`, which is `col_byte == 29`. */
    if (col_byte != 29) {
        h = hl + 2;
        for (i = 0; i < 8; i++) { scr_buff[h] &= 0x7F; h += 32; }
    }

    /* Color attr: 1-indexed lookup, write to both char cells the brick
     * spans. Mirror of LAE82_4 ($AE9C): straight briks_colors lookup
     * by low nibble, no per-state dimming. Earlier port dimmed
     * multi-hit bricks after their first hit as a UX cue but the
     * original draws them with their fresh colour throughout. */
    attr = briks_colors[iy_byte & 0x0F];
    attr_buff[attr_off]     = attr;
    attr_buff[attr_off + 1] = attr;
}

/* Port of brik_shadow ($AE2A). Clears the bright bit (bit 6) on the 2
 * attr cells one char-row below each non-skip brick, dimming them so
 * they read as a drop shadow. Skips the second cell when it would
 * wrap past the rightmost attr column. */
static void brik_shadow_c(unsigned int hl_attr, const unsigned char *cell_row) {
    unsigned int h = hl_attr;
    int col;
    for (col = 0; col < 15; col++) {
        unsigned char cell = cell_row[col];
        if (!(cell & 0x80)) {
            attr_buff[h] &= 0xBF;
            if (((h + 1) & 0x1F) != 31) attr_buff[h + 1] &= 0xBF;
        }
        h += 2;
    }
}

/* Port of print_briks ($ADE1). Walks the 12x15 level cell grid and
 * compositess each non-skip cell into scr_buff/attr_buff via
 * print_one_brik_buf_c + brik_shadow_c. */
static void print_briks_c(const unsigned char *cells) {
    int row, col;
    brik_addr_buf = 0x401;   /* scr_buff + $401 = pixel (8, 32). */
    brik_attr_buf = 0xA2;    /* attr_buff + $A2 = char (2, 5). */
    for (row = 0; row < LVL_ROWS; row++) {
        unsigned int hl = brik_addr_buf;
        const unsigned char *cell_row = &cells[row * LVL_COLS];
        for (col = 0; col < LVL_COLS; col++) {
            if (!(cell_row[col] & 0x80)) {
                print_one_brik_buf_c(hl, cell_row[col]);
            }
            hl += 2;
        }
        brik_shadow_c(brik_attr_buf, cell_row);
        brik_addr_buf += 0x100;   /* +8 pixel rows (next brick row). */
        brik_attr_buf += 0x20;    /* +1 char row. */
    }
}

/* Brick-band y range. print_briks_c writes to pixel rows 31..128 (the
 * 1-row top edge above the first brick row, 12 brick rows, and the
 * 1-row bottom edge below the last). */
#define BRICK_BAND_Y_TOP 31
#define BRICK_BAND_Y_BOT 128

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
    fast_memcpy(&attr_buff[3 * 32], &lattr[3 * ATTR_COLS], 14 * 32);

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
                    attr_buff[cr * 32 + cc1] = bg_attr;
                    attr_buff[cr * 32 + cc2] = bg_attr;
                    attr_buff[(cr + 1) * 32 + cc1] = bg_attr;
                    attr_buff[(cr + 1) * 32 + cc2] = bg_attr;
                }
            }
        }
    }

    print_briks_c(cells);
    print_border_shadow_c();
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
static void buff_to_vga_rect_bytes(int y0, int h, int byte_lo, int byte_hi);
static void paint_bg_window_to_buff(unsigned char attr, unsigned char cycle,
                                    int y0, int h, int byte_lo, int byte_hi);

static unsigned char bg_scr_buff[6144];
static unsigned char bg_attr_buff[768];
#define DIRTY_NONE 0xFF
#define DIRTY_SLOTS 2
static unsigned char dirty_min_byte[DIRTY_SLOTS][PLAYFIELD_H];
static unsigned char dirty_max_byte[DIRTY_SLOTS][PLAYFIELD_H];
static unsigned char prev_dirty_min_byte[DIRTY_SLOTS][PLAYFIELD_H];
static unsigned char prev_dirty_max_byte[DIRTY_SLOTS][PLAYFIELD_H];
static int static_bg_dirty = 1;
static int static_bg_cache_dirty = 0;
static int force_full_flush = 1;

static unsigned long prev_score = 0xFFFFFFFFUL;
static unsigned long prev_high_score = 0xFFFFFFFFUL;
static int prev_lives = -1;

static void clear_dirty_ranges(unsigned char mins[DIRTY_SLOTS][PLAYFIELD_H],
                               unsigned char maxs[DIRTY_SLOTS][PLAYFIELD_H]) {
    int s;
    for (s = 0; s < DIRTY_SLOTS; s++) {
        memset(mins[s], DIRTY_NONE, PLAYFIELD_H);
        memset(maxs[s], 0, PLAYFIELD_H);
    }
}

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

static void mark_dirty_sprite_rect(unsigned int spr, int x, int y) {
    mark_dirty_rect_px(x, y, (int)sprites_blob[spr] * 8, sprites_blob[spr + 1]);
}

static void mark_all_dirty(void) {
    int y;
    for (y = 0; y < PLAYFIELD_H; y++) {
        dirty_min_byte[0][y] = 0;
        dirty_max_byte[0][y] = 31;
        dirty_min_byte[1][y] = DIRTY_NONE;
        dirty_max_byte[1][y] = 0;
    }
}

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

static void buff_to_vga(void) {
    int y, byte_col;
    for (y = 0; y < PLAYFIELD_H; y++) {
        unsigned char __far *dest = vga + (long)(BORDER_Y + y) * SCREEN_W + BORDER_X;
        const unsigned char *scr_row = &scr_buff[y * 32];
        const unsigned char *attr_row = &attr_buff[(y >> 3) * 32];
        for (byte_col = 0; byte_col < 32; byte_col++) {
            unsigned char b = scr_row[byte_col];
            unsigned char attr = attr_row[byte_col];
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
        }
    }
}

/* Port of print_magnets ($8D4C) — paints each magnet specified in
 * magnets_per_level[level_idx]. Original draws spr_magnet_circle_off
 * at (x, y) then on a 50% random coin-flip overlays
 * spr_magnet_circle_on at (x, y+5). For the deterministic test we
 * paint both unconditionally (matches the GT capture moment where
 * the coin flipped "on"). Inherits each cell's attr — no override,
 * same monochrome rule as other moving sprites. */
static void render_magnets(unsigned char level_idx) {
    const unsigned char *rec;
    unsigned char n;
    int i;
    if (level_idx >= N_LEVELS) return;
    rec = magnets_per_level[level_idx];
    n = rec[0];
    for (i = 0; i < n; i++) {
        int x = rec[1 + 2*i];
        int y = rec[1 + 2*i + 1];
        /* Draw order matches the original's print_magnets ($8D4C):
         *   sprite_num $06 = spr_magnet_circle_ON (lightning, w=4, h=30
         *                    with SMC) — drawn UNCONDITIONALLY first.
         *   sprite_num $07 = spr_magnet_circle_OFF (bare outline, w=3,
         *                    h=23) — drawn CONDITIONALLY after coin.
         * (Iter 21 had this backwards, treating $06 as the "off state"
         * and $07 as the "on overlay"; gfx_screen_elements actually
         * maps $06 → spr_magnet_circle_on and $07 → spr_magnet_circle_off.)
         *
         * Both blits use the SAME (x, y) — original's `ADD A,$05` to
         * (IX+$04) between calls is dead state since ix_buf_addr_calc
         * only runs once.
         *
         * Coin-pin in test mode: slots 0/1 SKIP the OFF overlay (= GT
         * shows them as pure lightning, ~70% set pixels). Slots 2/3
         * DRAW the OFF overlay (= GT shows them at ~43% set with the
         * outline replacing the lightning's bright body). Normal play
         * follows the original coin flip from print_magnets. */
        blit_masked_to_scr_buff_ptr(spr_magnet_on, x, y);
        /* Magnet on/off coin-flip: the original print_magnets reads
         * random_number+$01 without advancing (read-current) -> rng_sample. */
        if (test_mode_pin_blink ? (i >= 2) : (random_lo(rng_sample()) & 1)) {
            blit_masked_to_scr_buff_ptr(spr_magnet_off, x, y);
        }
    }
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
    fast_memcpy(bg_scr_buff, scr_buff, sizeof(bg_scr_buff));
    fast_memcpy(bg_attr_buff, attr_buff, sizeof(bg_attr_buff));
    static_bg_cache_dirty = 0;
}

static void build_static_brick_band_cache(unsigned char level_idx) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle   = (unsigned char)(level_idx & 3);
    int y;
    int cr;

    paint_bg_window_to_buff(bg_attr, cycle,
                            BRICK_BAND_Y_TOP,
                            BRICK_BAND_Y_BOT - BRICK_BAND_Y_TOP + 1,
                            1, 30);
    render_brick_band(level_idx);
    for (y = BRICK_BAND_Y_TOP; y <= BRICK_BAND_Y_BOT; y++) {
        fast_memcpy(&bg_scr_buff[(y << 5) + 1],
                    &scr_buff[(y << 5) + 1],
                    30);
    }
    for (cr = 3; cr <= 16; cr++) {
        fast_memcpy(&bg_attr_buff[cr << 5], &attr_buff[cr << 5], 32);
    }
    static_bg_cache_dirty = 0;
}

static void mark_static_bg_cache_dirty(void) {
    static_bg_cache_dirty = 1;
}

static void restore_prev_dirty_from_static_cache(void) {
    int y;
    int cr;
    /* Restore only the byte ranges touched by moving sprites last
     * frame. Untouched rows and columns retain the cached static
     * background. */
    for (y = 0; y < PLAYFIELD_H; y++) {
        int s;
        for (s = 0; s < DIRTY_SLOTS; s++) {
            if (prev_dirty_min_byte[s][y] != DIRTY_NONE) {
                unsigned char lo = prev_dirty_min_byte[s][y];
                unsigned char hi = prev_dirty_max_byte[s][y];
                fast_memcpy(&scr_buff[(y << 5) + lo],
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
            fast_memcpy(&attr_buff[(cr << 5) + byte_lo],
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
        fast_memcpy(&scr_buff[(y << 5) + byte_lo],
                    &bg_scr_buff[(y << 5) + byte_lo],
                    (unsigned int)(byte_hi - byte_lo + 1));
    }
    for (cr = y_start >> 3; cr <= (y_end - 1) >> 3; cr++) {
        fast_memcpy(&attr_buff[(cr << 5) + byte_lo],
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
        fast_memcpy(&bg_scr_buff[y << 5], &scr_buff[y << 5], 32);
    }
    for (cr = 0; cr < FRAME_TOP_H_PX / 8; cr++) {
        fast_memcpy(&bg_attr_buff[cr << 5], &attr_buff[cr << 5], 32);
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
    int86(0x1A, &r, &r);
    return ((unsigned long)r.x.cx << 16) | r.x.dx;
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
static void (__interrupt __far *prev_int8)(void) = NULL;
static volatile unsigned long pit_frame_counter = 0;
static volatile unsigned int  bios_acc          = 0;

static void __interrupt __far new_int8(void) {
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
static void (__interrupt __far *prev_int9)(void) = NULL;
static volatile unsigned char key_state[128];

#define SC_ESC      0x01
#define SC_P        0x19
#define SC_ENTER    0x1C
#define SC_SPACE    0x39
#define SC_LEFT     0x4B    /* arrow / keypad 4 */
#define SC_RIGHT    0x4D    /* arrow / keypad 6 */

static void __interrupt __far new_int9(void) {
    unsigned char sc = inp(0x60);
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

/* --- PC speaker sound (PIT channel 2, original-style envelopes) -------
 *
 * The Spectrum routines are CPU-timed beeper loops. A literal PC port
 * that spins in the 50 Hz frame body makes audio steal frame time, so
 * this backend latches a PIT channel-2 tone and lets `sound_tick`
 * release it asynchronously on later PIT ticks. */
/* Pause flag: while set, the per-frame body does no physics; only P
 * (toggle), ESC (quit), and ENTER (advance) are responded to. */
static unsigned char paused = 0;
static unsigned char sound_ticks_left = 0;
static unsigned long sound_last_tick = 0;

static void sound_silence(void) {
    _disable();
    outp(0x61, (unsigned char)(inp(0x61) & 0xFC));
    _enable();
    sound_ticks_left = 0;
}

static void sound_pit_set_period(unsigned int period) {
    if (period < 20) period = 20;
    _disable();
    outp(0x43, 0xB6);                 /* counter 2, lo+hi, mode 3 */
    outp(0x42, (unsigned char)(period & 0xFF));
    outp(0x42, (unsigned char)((period >> 8) & 0xFF));
    _enable();
}

static void sound_gate_on(void) {
    _disable();
    outp(0x61, (unsigned char)(inp(0x61) | 0x03));
    _enable();
}

static void sound_gate_off(void) {
    _disable();
    outp(0x61, (unsigned char)(inp(0x61) & 0xFC));
    _enable();
}

static void sound_start_period(unsigned int period, unsigned char ticks) {
    if (sound_disabled || ticks == 0) return;
    sound_pit_set_period(period);
    sound_gate_on();
    sound_ticks_left = ticks;
    sound_last_tick = pit_ticks();
}

static void sound_beep_e(unsigned char e) {
    /* Original period is proportional to E. PIT divisor ~= 1193180 /
     * (3500000 / (26*E)) = 8.86*E; use 9*E as the close integer form. */
    sound_start_period((unsigned int)e * 9u, 1);
}

static void sound_beep2_bd(unsigned char b, unsigned char d) {
    unsigned char period = (unsigned char)(((unsigned int)b + (unsigned int)d) / 2u);
    if (period == 0) period = 1;
    sound_start_period((unsigned int)period * 9u, 1);
}

static void sound_beep_cont_d(unsigned char d, unsigned char e) {
    (void)d;
    sound_beep_e(e);
}

static void sound_beep_cont_de(unsigned char d, unsigned char e) {
    (void)d;
    sound_beep_e(e);
}

static void sound_play_lc122(unsigned char c, unsigned char e) {
    unsigned char a = (unsigned char)(c ^ e);
    unsigned char b = (unsigned char)((a << 1) & 0x0C);
    unsigned char d = (unsigned char)((a << 1) & 0x0F);
    sound_beep2_bd((unsigned char)(b + 0x08), d);
}

static void sound_tick(void) {
    unsigned long now;
    if (sound_disabled || sound_ticks_left == 0) return;
    now = pit_ticks();
    if (now == sound_last_tick) return;
    sound_last_tick = now;
    if (--sound_ticks_left == 0) sound_gate_off();
}

/* --- Sound queue (port of sounds_queue at $C0B8 + play_sounds_queue) ---
 *
 * 5 slots, each tracks a sound id + per-sound state byte. snd_q_push
 * adds an event; snd_q_tick is called from the 50 Hz frame body and
 * dispatches each active slot to its play_sound_<id> handler.
 * Single-shot sounds clear their slot on first tick; multi-frame
 * sounds (live-add ascending sweep, ball-launch / shot descending
 * sweep) advance state per frame and clear when exhausted.
 *
 * Each handler mirrors the D/E/B/C parameters of the original's
 * play_sound_<event> routine (sound.asm at $C0F3+). */
#define SQ_SLOTS 5
typedef struct { unsigned char id; unsigned char state; } sound_slot_t;

/* Sound IDs match the original play_sounds_list at $C0BC. */
#define SND_NORMAL_BRIK   1
#define SND_BAT_BEAT      3
#define SND_BALL_START    4
#define SND_ALIEN_BLAST   6
#define SND_LIVE_ADD      7
#define SND_SPARK_FANOUT  8
#define SND_BAT_RESIZE_1  9
#define SND_TRIPLE_BALL   0x0A
#define SND_SHOT          0x0B
#define SND_BAT_RESIZE_2  0x0C

static sound_slot_t snd_q[SQ_SLOTS];

static void snd_q_push(unsigned char id) {
    int i;
    if (sound_disabled) return;
    if (id == SND_NORMAL_BRIK || id == SND_BAT_BEAT || id == SND_SHOT) {
        for (i = 0; i < SQ_SLOTS; i++) {
            if (snd_q[i].id == id) return;
        }
    }
    for (i = 0; i < SQ_SLOTS; i++) {
        if (snd_q[i].id == 0) {
            snd_q[i].id = id;
            switch (id) {
                case SND_LIVE_ADD:    snd_q[i].state = 0x20; break;
                case SND_BALL_START:  snd_q[i].state = 0x00; break;
                case SND_SHOT:        snd_q[i].state = 0x00; break;
                case SND_SPARK_FANOUT:snd_q[i].state = 0x3D; break;  /* LBC10 push */
                case SND_BAT_RESIZE_1:snd_q[i].state = 0xC0; break;  /* matches \$3212 push */
                case SND_TRIPLE_BALL: snd_q[i].state = 0x10; break;  /* matches \$3072 push */
                case SND_ALIEN_BLAST: snd_q[i].state = 0x30; break;
                default:              snd_q[i].state = 0; break;
            }
            return;
        }
    }
}

/* Returns 1 when the slot should be cleared (sound done). */
static int snd_tick_one(sound_slot_t *s) {
    switch (s->id) {
        case SND_NORMAL_BRIK:
            /* $C0F3: D=$08,E=$44. */
            sound_beep_cont_d(0x08, 0x44);
            return 1;

        case SND_BAT_BEAT:
            /* $C16F: D=$04,E=$66. */
            sound_beep_cont_d(0x04, 0x66);
            return 1;

        case SND_LIVE_ADD: {
            /* $C1CF: state starts $20, every 4th frame plays a beep
             * at E = state + $14 (ascending pitch as state shrinks).
             * state -= 2 per frame; cleared when 0. */
            if ((s->state & 3) == 0) {
                sound_beep_cont_d(0x03, (unsigned char)(s->state + 0x14));
            }
            if (s->state == 0) return 1;
            s->state -= 2;
            return 0;
        }

        case SND_BALL_START: {
            /* $C116: C=$09,E=$14, then clear the slot. */
            sound_play_lc122(0x09, 0x14);
            return 1;
        }

        case SND_SHOT: {
            /* $C235: C=$04,E=$0F, then clear the slot. */
            sound_play_lc122(0x04, 0x0F);
            return 1;
        }

        case SND_BAT_RESIZE_1: {
            /* $C200: state starts $C0 (from the bonus_resize push at
             * \$3212), decrements by $0B per frame until below $10. */
            sound_beep_cont_d(0x01, s->state);
            if (s->state < 0x10 + 0x0B) return 1;
            s->state -= 0x0B;
            return 0;
        }

        case SND_TRIPLE_BALL: {
            /* $C21D: state starts $10 (from the LA67B_8 push at
             * \$3072), increments by $0B per frame until past $C0. */
            sound_beep_cont_d(0x01, s->state);
            if (s->state >= 0xC1 - 0x0B) return 1;
            s->state += 0x0B;
            return 0;
        }

        case SND_ALIEN_BLAST: {
            /* $C1A8: state starts $30, each frame plays a noisy tone at
             * E = (random & $3F) + state with D=1; state += 8; wraps
             * from $60 to $21 once; stops at $A1. ~22 frames of zip-
             * style noise. */
            unsigned int e = ((unsigned int)random_lo(next_random()) & 0x3Fu) + (unsigned int)s->state;
            sound_beep_cont_d(0x01, (unsigned char)e);
            s->state = (unsigned char)(s->state + 8);
            if (s->state == 0x60) s->state = 0x21;
            if (s->state == 0xA1) return 1;
            return 0;
        }

        case SND_SPARK_FANOUT: {
            /* $C1ED: E = ((state >> 2) & $3F) + $20, D=2. */
            unsigned int e = (((unsigned int)s->state >> 2) & 0x3Fu) + 0x20u;
            sound_beep_cont_de(0x02, (unsigned char)e);
            s->state++;
            return s->state == 0xA1;
        }

        case SND_BAT_RESIZE_2: {
            /* $C241: D=$0A,E=$30. */
            sound_beep_cont_de(0x0A, 0x30);
            return 1;
        }

        default:
            return 1;
    }
}

static void snd_q_tick(void) {
    int i;
    for (i = 0; i < SQ_SLOTS; i++) {
        if (snd_q[i].id == 0) continue;
        if (snd_tick_one(&snd_q[i])) snd_q[i].id = 0;
        break;      /* PC speaker is one voice; avoid N port-programs/frame. */
    }
}

static void snd_q_silence_all(void) {
    int i;
    for (i = 0; i < SQ_SLOTS; i++) snd_q[i].id = 0;
    sound_silence();
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
static int test_mode_pin_blink = 0;          /* set by BATTYALL env */
static int blink_phase(void) {
    if (test_mode_pin_blink) return 0;
    return (int)((bios_ticks() >> 1) & 1);   /* ~4.5 Hz half-period */
}
static void render_hiscore_screen(void) {
    load_markup("MARKUP.BIN");
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
    load_markup("MENUMARK.BIN");
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

/* Flush a horizontal strip of scr_buff/attr_buff to VGA — partial
 * version of buff_to_vga used after a paint_bg_strip_to_buff +
 * sprite blits to redraw just the affected band. */
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
        unsigned char __far *dest = vga + (long)(BORDER_Y + y) * SCREEN_W
                                  + BORDER_X + byte_lo * 8;
        const unsigned char *scr_row = &scr_buff[y * 32];
        const unsigned char *attr_row = &attr_buff[(y >> 3) * 32];
        for (byte_col = byte_lo; byte_col <= byte_hi; byte_col++) {
            unsigned char b = scr_row[byte_col];
            unsigned char attr = attr_row[byte_col];
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
    blit_masked_sprite(spr, x, y, ink_pal(attr), paper_pal(attr));
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
static unsigned char our_to_orig_bonus(unsigned char type) {
    switch (type) {
        case BONUS_TYPE_BIG_BAT:     return 0x00;
        case BONUS_TYPE_LASER:       return 0x01;
        case BONUS_TYPE_MULTI_BALL:  return 0x02;
        case BONUS_TYPE_CATCH:       return 0x03;
        case BONUS_TYPE_SLOW:        return 0x04;
        case BONUS_TYPE_LIFE:        return 0x05;
        case BONUS_TYPE_ROCKET:      return 0x06;
        case BONUS_TYPE_BIG_BALL:    return 0x07;
        case BONUS_TYPE_SCORE_5K:    return 0x08;
        case BONUS_TYPE_KILL_ALIENS: return 0x09;
        default:                     return 0xFF;
    }
}

/* Apply the effect that comes with `type`. Catching the same type
 * while already active extends the duration. */
static void bonus_apply(unsigned char type) {
    /* Original get_bonus at $A67B: every catch awards 400 points and
     * plays a sound — sound_live_add ($07) for the LIFE bonus, the
     * resize-2 beep ($0C) for everything else (push_resize_sound at
     * $A645, gated by `CP $05; CALL NZ,push_resize_sound`). Our port
     * had been routing every catch through SND_LIVE_ADD. */
    snd_q_push(type == BONUS_TYPE_LIFE ? SND_LIVE_ADD : SND_BAT_RESIZE_2);
    /* Original LA67B_3 at \$A6FC writes the bonus type code into
     * bat.bonus_applied for every catch except ROCKET (which jumps
     * out earlier to get_rocket). Catching a new bonus thus REPLACES
     * any previous bat-side effect — e.g. catching BIG_BAT after
     * LASER clears the LASER state. */
    if (type != BONUS_TYPE_ROCKET) {
        unsigned char orig_code = our_to_orig_bonus(type);
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
            slow_ticks = SLOW_DURATION;
            break;
        case BONUS_TYPE_BIG_BAT:  big_bat_ticks  = BIG_BAT_DURATION;
                                  bat_extra_tgt  = BAT_BIG_EXTRA_PX;
                                  snd_q_push(SND_BAT_RESIZE_1);
                                  break;
        case BONUS_TYPE_BIG_BALL: big_ball_ticks = BIG_BALL_DURATION; break;
        case BONUS_TYPE_KILL_ALIENS:
            /* bat.bonus_applied = \$09 has already been set above —
             * enemy_prepare reads that to skip further alien spawns.
             * Also clear any currently active alien for immediate
             * visible effect. */
            {
                object_t *e = &objects[OBJ_ENEMY];
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
                    e->misc_12 = 0;
                    score += 350;
                    snd_q_push(SND_ALIEN_BLAST);
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
                unsigned char base_dir = ball_dir_from_delta(ball_dx, ball_dy);
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
                ball_delta_from_dir(ball2_dir, &ball2_dx, &ball2_dy);
                ball3_active = 1;
                objects[OBJ_BALL_3].sprite_set = 0x02;
                objects[OBJ_BALL_3].x_coord = BALL_X;
                objects[OBJ_BALL_3].y_coord = BALL_Y;
                ball_delta_from_dir(ball3_dir, &ball3_dx, &ball3_dy);
                snd_q_push(SND_TRIPLE_BALL);
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
#define BAT_BODY_W 28
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
    if (slow_ticks    > 0) slow_ticks--;
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
        /* Original generate_new_bonus at $9DFE re-rolls when the picked
         * bonus matches the bat's currently-applied bonus (= LASER /
         * CATCH / KILL_ALIENS — the ones tracked in bat.bonus_applied).
         * Prevents back-to-back duplicates of the same bat effect. */
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
        if (code == 0x04 && slow_ticks > 0) continue;
        if (code == 0x05 && life_dropped_this_round) continue;
        if (code == 0x06 && rocket_active) continue;
        if (code == 0x06 && round_number >= 6
            && (random_lo(rnd) & 0xC0) != 0) continue;
        mapped = map_orig_to_our_bonus(code);
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
static int laffc_collision(int prev_x, int prev_y, int new_x, int new_y);

static int brick_collision(int prev_x, int prev_y, int new_x, int new_y) {
    int sz = eff_ball_size();
    int body_h = BALL_H_PX;
    int left = new_x;
    int right = new_x + sz - 1;
    int top = new_y;
    int bottom = new_y + body_h - 1;
    int col0, col1, row0, row1;
    int col, row, r, c, brick_top, brick_bot, brick_left, brick_right, axis;
    if (bottom < 32 || top >= 32 + LVL_ROWS * 8) return 0;
    if (right < 8  || left >= 8  + LVL_COLS * 16) return 0;
    col0 = (left < 8) ? 0 : (left - 8) / 16;
    col1 = (right >= 8 + LVL_COLS * 16) ? LVL_COLS - 1 : (right - 8) / 16;
    row0 = (top < 32) ? 0 : (top - 32) / 8;
    row1 = (bottom >= 32 + LVL_ROWS * 8) ? LVL_ROWS - 1 : (bottom - 32) / 8;

    row = -1;
    col = -1;
    for (r = row0; r <= row1 && row < 0; r++) {
        for (c = col0; c <= col1; c++) {
            unsigned char v = live_level[r * LVL_COLS + c];
            if ((v & 0x80) == 0) {
                row = r;
                col = c;
                break;
            }
        }
    }
    if (row < 0) return 0;

    /* Determine the bounce axis (1 = flip dy, 2 = flip dx) for both
     * the destructible and undestructible paths. */
    brick_top = 32 + row * 8;
    brick_bot = brick_top + 8;
    brick_left = 8 + col * 16;
    brick_right = brick_left + 16;
    if (prev_y + body_h <= brick_top || prev_y >= brick_bot) {
        axis = 1;
    } else if (prev_x + sz <= brick_left || prev_x >= brick_right) {
        axis = 2;
    } else {
        int overlap_x = (right < brick_right ? right : brick_right - 1)
                      - (left > brick_left ? left : brick_left) + 1;
        int overlap_y = (bottom < brick_bot ? bottom : brick_bot - 1)
                      - (top > brick_top ? top : brick_top) + 1;
        axis = (overlap_y <= overlap_x) ? 1 : 2;
    }
    return brick_hit_resolve(col, row, axis);
}

/* Shared brick-hit tail (undestructible / multi-hit half-state /
 * destroy + shimmer + bonus), split out of brick_collision so the
 * LAFFC port can reuse it. Returns the (possibly smash-zeroed) axis. */
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
        snd_q_push(SND_NORMAL_BRIK);
        return axis;
    }
    /* SMASH (BIG_BALL) bypasses the multi-hit half-state — port of
     * LAFFC's `CP \$07; JR Z,LAFFC_38` test that jumps directly to
     * the destroy path. Without this, multi-hit bricks still need
     * two hits even with SMASH active. */
    if (!big_ball_active() && !(*cell & 0x10)) {
        *cell |= 0x10;
        brick_hit_anim_spawn(col, row);
        snd_q_push(SND_NORMAL_BRIK);
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
    mark_static_bg_cache_dirty();
    snd_q_push(SND_NORMAL_BRIK);            /* brick-break click */
    /* BIG_BALL (smash) bonus: ball ploughs through bricks rather
     * than bouncing — keep the bonus-spawn check below intact but
     * stash the "no bounce" intent. */
    if (big_ball_active()) axis = 0;
    brick_flash_spawn(col, row);
    try_spawn_bonus(col, row);
    return axis;
}

/* change_direction at $AD5C: dir = ((dir XOR mask) + 1) & 0x3F. mask
 * $1F flips the horizontal component (left/right bounce), $3F the
 * vertical (up/down). */
static unsigned char laffc_change_dir(unsigned char dir, unsigned char mask) {
    return (unsigned char)(((dir ^ mask) + 1) & 0x3F);
}

/* Port of LAFFC ($AFFC) brick collision, gated behind BATTY_LAFFC while
 * it is brought to parity (the default game keeps brick_collision). See
 * notes/laffc-decode.md. Phases: (1) early exits, (2/3) find the grid
 * cell at the ball's position via the disasm's byte loops, (4) build a
 * 4-bit neighbour-solidity mask, (5) gate it by ball direction, (6)
 * bounce off the chosen solid neighbour and destroy IT (not the ball's
 * own cell) — that is the "same count, different cells" fix. The
 * penetration-depth corner case (LAFFC_21-25) and exact change_direction
 * masks are still approximated by brick_hit_resolve's axis reflect;
 * refined in later iterations against the frame-step gate. */
static int laffc_collision(int prev_x, int prev_y, int new_x, int new_y) {
    object_t *o = &objects[OBJ_BALL_1];
    int h = o->h_body_px;
    unsigned char dir = o->dir;
    int row = -1, Hy = 0, col = 0, Lx = 0x08, mask, rem = 0;
    (void)prev_x; (void)prev_y;
    /* phase 1: early exits (ball below / above the brick band) */
    if (new_y >= 0x80) return 0;
    if (new_y + h < 0x20) return 0;
    /* phase 2: find the row band (byte-faithful LAFFC_0..2) */
    {
        int Cv = 0x20, rr;
        for (rr = 0; rr < LVL_ROWS; rr++) {
            int a = (Cv - new_y) & 0xFF;
            if (new_y > Cv) {                 /* borrow -> LAFFC_1 */
                if (a + 8 > 0xFF) { row = rr; Hy = Cv; break; }
            } else {                          /* LAFFC_0 main */
                if (a < h) { row = rr; Hy = Cv; break; }
            }
            Cv += 8;
        }
    }
    if (row < 0) return 0;
    /* phase 3: find the column (LAFFC_4) */
    {
        int a = (new_x - 0x08);
        if (a < 0) a = 0;
        while (a >= 0x10 && col < LVL_COLS - 1) { a -= 0x10; col++; Lx += 0x10; }
        rem = a;   /* X penetration within the cell, 0..15 */
    }
    /* phase 4 head (LAFFC_5-6): land (row,col)/(Lx,Hy) on the SOLID cell
     * the ball body overlaps. The body straddles into the next COLUMN
     * when it crosses the cell's right edge (rem + width >= 16, not at the
     * $E8 edge) and into the next ROW when it penetrates >= 8 px
     * (new_y + h - Hy >= 8, not at the $78 bottom). LAFFC tries, in order:
     * own cell, right, down, then down-right (only when the horizontal
     * straddle applied). My earlier port did right only, which missed the
     * down/down-right brick at a row boundary (see laffc-decode Update 14). */
#define LAFFC_SOLID(rr,cc) ((rr) >= 0 && (rr) < LVL_ROWS && (cc) >= 0 && \
        (cc) < LVL_COLS && !(live_level[(rr) * LVL_COLS + (cc)] & 0x80))
    if (!LAFFC_SOLID(row, col)) {
        int hstrad = (rem + o->w_body_px) >= 0x10 && Lx != 0xE8;
        int vstrad = (new_y + h - Hy) >= 8 && Hy < 0x78;
        int landed = 0;
        if (hstrad && LAFFC_SOLID(row, col + 1)) {          /* right */
            col++; Lx += 0x10; landed = 1;
        } else if (vstrad) {
            row++; Hy += 8;
            if (LAFFC_SOLID(row, col)) {                    /* down */
                landed = 1;
            } else if (hstrad && LAFFC_SOLID(row, col + 1)) { /* down-right */
                col++; Lx += 0x10; landed = 1;
            }
        }
        if (!landed) return 0;
    }
#undef LAFFC_SOLID
    /* phase 4: open-face mask (bit0 L, 1 R, 2 U, 3 D). LAFFC clears a bit
     * when that neighbour is SOLID or past a playfield edge, so a set bit
     * = an OPEN (empty/destroyed) face the ball can reflect off. */
#define LAFFC_EMPTY(rr,cc) ((rr) < 0 || (rr) >= LVL_ROWS || (cc) < 0 || \
        (cc) >= LVL_COLS || (live_level[(rr) * LVL_COLS + (cc)] & 0x80))
    mask = 0;
    if (Lx != 0x08 && LAFFC_EMPTY(row, col - 1)) mask |= 1;
    if (Lx != 0xE8 && LAFFC_EMPTY(row, col + 1)) mask |= 2;
    if (Hy >= 0x21 && LAFFC_EMPTY(row - 1, col)) mask |= 4;
    if (Hy <  0x78 && LAFFC_EMPTY(row + 1, col)) mask |= 8;
#undef LAFFC_EMPTY
    /* phase 5: gate by direction (LAFFC_13..17). Leaves at most one of
     * {left,right} and one of {up,down}. */
    if (dir < 0x20) mask &= ~8; else mask &= ~4;
    if (((dir + 0x10) & 0x3F) >= 0x20) mask &= ~1; else mask &= ~2;
    /* phase 5b: corner case (LAFFC_21-25). When both a horizontal and a
     * vertical open face survive, the ball entered through the shallower-
     * penetrated one — bounce off that axis. X-pen/Y-pen are measured
     * from the open side (left: x+w-Lx, right: Lx+$10-x; up: y+h-Hy,
     * down: Hy+8-y); if Y-pen >= X-pen keep horizontal, else vertical. */
    if ((mask & 0x03) && (mask & 0x0C)) {
        int w = o->w_body_px;
        int xpen = (mask & 1) ? ((w + new_x) - Lx) : ((Lx + 0x10) - new_x);
        int ypen = (mask & 4) ? ((h + new_y) - Hy) : ((Hy + 8) - new_y);
        xpen &= 0xFF; ypen &= 0xFF;
        if (ypen >= xpen) mask &= ~0x0C;   /* horizontal bounce */
        else              mask &= ~0x03;   /* vertical bounce */
    }
    /* phase 6: resolve the hit cell (destroy / half-hit / shimmer). SMASH
     * (big-ball) returns 0 = plough through: cell destroyed, no bounce. */
    if (brick_hit_resolve(col, row, 1) == 0) return 0;
    /* Reflect via change_direction and snap the ball to the cell edge of
     * the chosen open face (LAFFC_26-29). $1F flips horizontal, $3F
     * vertical. The non-snapped axis advances to the new position. */
    {
        int w = o->w_body_px;
        int dx_q8, dy_q8;
        if (mask & 1) {            /* open left -> horizontal bounce */
            o->x_coord = (unsigned char)(Lx - w);  o->y_coord = (unsigned char)new_y;
            o->dir = laffc_change_dir(dir, 0x1F);
        } else if (mask & 2) {     /* open right */
            o->x_coord = (unsigned char)(Lx + 0x10); o->y_coord = (unsigned char)new_y;
            o->dir = laffc_change_dir(dir, 0x1F);
        } else if (mask & 4) {     /* open up -> vertical bounce */
            o->y_coord = (unsigned char)(Hy - h);  o->x_coord = (unsigned char)new_x;
            o->dir = laffc_change_dir(dir, 0x3F);
        } else {                   /* open down or fully enclosed (default) */
            o->y_coord = (unsigned char)(Hy + 8);  o->x_coord = (unsigned char)new_x;
            o->dir = laffc_change_dir(dir, 0x3F);
        }
        ball_dir_delta_q8(o->dir, o->speed, &dx_q8, &dy_q8);
        ball_dx = (dx_q8 < 0) ? -1 : (dx_q8 > 0 ? 1 : 0);
        ball_dy = (dy_q8 < 0) ? -1 : (dy_q8 > 0 ? 1 : 0);
    }
    return 3;   /* handled: reflected + snapped; step_ball must not re-reflect */
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

static unsigned int next_random(void) {
    unsigned char src = (random_rom != NULL)
        ? random_rom[random_seed_addr & (RANDOM_ROM_SIZE - 1)]
        : 0;
    random_e = (unsigned char)(random_e + src + 0x05 + ctrl_btns_pressed_value());
    random_d = (unsigned char)(random_d + (unsigned char)(~src) + 0x16
                               + (unsigned char)random_seed_addr);
    random_seed_addr = (unsigned int)((random_seed_addr + 1) & 0x9FFF);
    if (random_seed_addr < 0x8000) random_seed_addr |= 0x8000;
    return (unsigned int)(((unsigned int)random_d << 8) | random_e);
}

/* Sample the RNG for a "read-current" consumer (the original's
 * `LD A,(random_number)` without a preceding `CALL random_generate`).
 * With rng_perframe OFF this is identical to next_random() (advance on
 * read) so behaviour and all gates are byte-unchanged; with it ON it
 * returns the current random_number WITHOUT advancing, because the
 * per-frame tick (added in the play loop) is the only advance — matching
 * the original. Consumers the original advances-then-reads (bonus
 * generation) keep calling next_random() directly. */
static unsigned int rng_sample(void) {
    if (rng_perframe)
        return (unsigned int)(((unsigned int)random_d << 8) | random_e);
    return next_random();
}

static void apply_replay_random_override(void) {
    const char *p = getenv("BATTY_REPLAY_RANDOM");
    const char *s;
    char *endp;
    unsigned long v;
    if (p != NULL && *p != '\0') {
        v = strtoul(p, &endp, 16);
        if (*endp == '\0' && v <= 0xFFFFUL) {
            random_d = (unsigned char)(v >> 8);   /* random_number high */
            random_e = (unsigned char)v;          /* random_number low  */
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
            random_seed_addr = (unsigned int)(v & 0x9FFF);
            if (random_seed_addr < 0x8000) random_seed_addr |= 0x8000;
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
    unsigned char bytes[sizeof(object_t)];
    if (replay_parse_hex_bytes(getenv("BATTY_REPLAY_BAT_OBJECT"),
                               bytes, (int)sizeof(bytes)) != 0) return;
    fast_memcpy(&objects[OBJ_BAT_1], bytes, sizeof(bytes));
}

static void apply_replay_ball_object_override(void) {
    unsigned char bytes[sizeof(object_t)];
    if (replay_parse_hex_bytes(getenv("BATTY_REPLAY_BALL_OBJECT"),
                               bytes, (int)sizeof(bytes)) != 0) return;
    fast_memcpy(&objects[OBJ_BALL_1], bytes, sizeof(bytes));
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
    unsigned char bytes[sizeof(object_t)];
    if (replay_parse_hex_bytes(getenv("BATTY_REPLAY_ENEMY_OBJECT"),
                               bytes, (int)sizeof(bytes)) != 0) return;
    fast_memcpy(&objects[OBJ_ENEMY], bytes, sizeof(bytes));
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

static void write_replay_probe(void) {
    FILE *f;
    int i;
    if (getenv("BATTY_REPLAY_PROBE") == NULL) return;
    f = fopen("PROBE.TXT", "wt");
    if (!f) return;
    fprintf(f, "round_number=%02X\n", (unsigned)round_number);
    fprintf(f, "current_level=%02X\n", (unsigned)current_level_idx_var);
    fprintf(f, "bricks_quantity=%02X\n", (unsigned)live_bricks_remaining());
    fprintf(f, "score=%06lu\n", score);
    fprintf(f, "random_number=%02X%02X\n", (unsigned)random_d, (unsigned)random_e);
    fprintf(f, "random_seed=%04X\n", random_seed_addr);
    fprintf(f, "enemy_repicks=arrival%u_margin%u_turns%u\n",
            dbg_enemy_arrival_repicks, dbg_enemy_margin_repicks,
            dbg_enemy_turn_calls);
    fprintf(f, "object_ball_1=");
    for (i = 0; i < (int)sizeof(object_t); i++) {
        fprintf(f, "%02X", ((unsigned char *)&objects[OBJ_BALL_1])[i]);
    }
    fprintf(f, "\nobject_bat_1=");
    for (i = 0; i < (int)sizeof(object_t); i++) {
        fprintf(f, "%02X", ((unsigned char *)&objects[OBJ_BAT_1])[i]);
    }
    fprintf(f, "\nobject_enemy=");
    for (i = 0; i < (int)sizeof(object_t); i++) {
        fprintf(f, "%02X", ((unsigned char *)&objects[OBJ_ENEMY])[i]);
    }
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
    fprintf(f, "\n");
    fclose(f);
}

static void enemy_prepare(void) {
    object_t *e = &objects[OBJ_ENEMY];
    const unsigned char *prop;
    unsigned char r;
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
    object_t *e = &objects[OBJ_ENEMY];
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
    e->misc_12 = 0;                                 /* reset tick counter */
    score += 350;                                   /* $0350 BCD */
    snd_q_push(SND_ALIEN_BLAST);                    /* port of $C1A8 */
}

/* Mirror of kill_enemy_by_bat for the ball — original
 * kill_enemy_by_bat at $A4B8 is called from BOTH handling_bat AND
 * handling_ball (see the cross-reference at line 2745 of the disasm),
 * so a ball plunking down on an alien destroys it the same way a bat
 * crashing into one does. AABB between the ball body (8x7) and the
 * alien body. */
static void kill_enemy_by_ball_rect(int bx_l, int by_t, int bw, int bh) {
    object_t *e = &objects[OBJ_ENEMY];
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
    e->misc_12 = 0;
    score += 350;
    snd_q_push(SND_ALIEN_BLAST);
}

/* Port of bomb_appear at $A977 - called per alien tick. Probability
 * (random + random+1) & $3F == 0 = ~1/64 chance per call. Bomb
 * shares the bonus slot in the original; we keep separate state. */
static void bomb_appear(object_t *o) {
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
    int col, row;
    unsigned char *cell;
    object_t *enemy;
    if (!bullet_active[b]) return;
    bullet_y[b] -= BULLET_SPEED;
    if (bullet_y[b] < 0) {
        bullet_active[b] = 0;                    /* fly-off: no blast */
        return;
    }
    /* Alien hit (AABB on alien body rect). */
    enemy = &objects[OBJ_ENEMY];
    if ((enemy->sprite_set & 0x7F) != 0
        && !(enemy->sprite_set & 0x80)
        && (enemy->sprite_set & 0x7F) != 0x0A) {
        int ex_l = enemy->x_coord;
        int ex_r = enemy->x_coord + enemy->w_body_px;
        int ey_t = enemy->y_coord;
        int ey_b = enemy->y_coord + enemy->h_body_px;
        if (bullet_x[b] + BULLET_BODY_W > ex_l && bullet_x[b] < ex_r
            && bullet_y[b] + BULLET_BODY_H > ey_t && bullet_y[b] < ey_b) {
            /* Centre 16x13 blast over alien (mirror of \$A4D2). */
            enemy->x_coord = (unsigned char)(enemy->x_coord + (int)enemy->w_body_px / 2 - 8);
            enemy->y_coord = (unsigned char)(enemy->y_coord + 4);
            enemy->w_body_px = 16;
            enemy->h_body_px = 13;
            enemy->sprite_set = 0x0A;       /* transition to 5-frame blast */
            enemy->sprite_num = 0;
            enemy->misc_12    = 0;
            score += 350;
            snd_q_push(SND_ALIEN_BLAST);
            bullet_active[b] = 0;
            /* Align blast x to 8-px boundary — port of LA5A3_0's
             * `LD A,(IX+\$02); AND \$F8; LD (IX+\$02),A`. The blast
             * sprite is byte-aligned, so the impact point snaps to
             * the nearest cell column. */
            bullet_blast_x[b] = bullet_x[b] & ~7;
            bullet_blast_y[b] = bullet_y[b];
            bullet_blast_ticks[b] = BULLET_BLAST_FRAMES * BULLET_BLAST_TICKS_PER_FRAME;
            return;
        }
    }
    /* Brick hit (point-vs-grid lookup matching brick_collision's cell
     * arithmetic). Undestructible bricks stop the bullet without
     * destroying; multi-hit bricks set bit 4 (= half-damaged) on
     * first hit; bit-4 bricks destroy on this hit. Every hit spawns
     * the 4-frame impact blast. */
    if (bullet_y[b] >= 32 && bullet_y[b] < 32 + LVL_ROWS * 8
        && bullet_x[b] >= 8 && bullet_x[b] < 8 + LVL_COLS * 16) {
        col = (bullet_x[b] - 8) / 16;
        row = (bullet_y[b] - 32) / 8;
        cell = &live_level[row * LVL_COLS + col];
        if (!(*cell & 0x80)) {
            int hit = 0;
            if (*cell & 0x20) {
                hit = 1;                       /* undestructible: stop, no destroy */
                brick_hit_anim_spawn(col, row);
            } else if (!(*cell & 0x10)) {
                *cell |= 0x10;                 /* multi-hit, set bit 4 */
                brick_hit_anim_spawn(col, row);
                hit = 1;
            } else {
                unsigned int idx = (unsigned int)((row < 12) ? row : 11);
                unsigned int pts = points_table[idx];
                if ((*cell & 0x0F) >= 6) pts *= 2;
                score += pts;
                *cell |= 0x80;
                mark_static_bg_cache_dirty();
                brick_flash_spawn(col, row);
                try_spawn_bonus(col, row);
                hit = 1;
            }
            if (hit) {
                /* Original LAFFC checks colliding object's sprite_set
                 * == \$05 (bullet) and skips sound_normall_brik on
                 * bullet hits. Visual feedback comes from the 4-frame
                 * bullet-blast at the impact point instead. */
                bullet_active[b] = 0;
                bullet_blast_x[b] = bullet_x[b];
                bullet_blast_y[b] = bullet_y[b];
                bullet_blast_ticks[b] = BULLET_BLAST_FRAMES * BULLET_BLAST_TICKS_PER_FRAME;
                return;
            }
        }
    }
}

static void step_bullet(void) {
    int i;
    for (i = 0; i < N_BULLETS; i++) step_bullet_one(i);
}

/* Step the bullet-impact blasts one tick each. Per-slot countdown
 * matches the per-slot bullet that spawned each blast. */
static void step_bullet_blast(void) {
    int i;
    for (i = 0; i < N_BULLETS; i++) {
        if (bullet_blast_ticks[i]) bullet_blast_ticks[i]--;
    }
}

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
static int any_bullet_blast(void) {
    int i;
    for (i = 0; i < N_BULLETS; i++) if (bullet_blast_ticks[i]) return 1;
    return 0;
}

/* True if any bullet slot is in flight — used by the inner loop to
 * decide whether to redraw and to expose firing capacity to SPACE. */
static int any_bullet_active(void) {
    int i;
    for (i = 0; i < N_BULLETS; i++) if (bullet_active[i]) return 1;
    return 0;
}

/* Step the rocket one frame: move up, destroy any destructible
 * brick its bbox overlaps, leave undestructible bricks alone (no
 * bounce — the rocket just continues past them). Deactivate when
 * the rocket leaves the top of the playfield. */
/* Award bonus points for every still-live destructible brick on the
 * current level — port of add_points_for_left_briks at $AF81. Called
 * when the rocket exits the playfield (LBB97 → LBBFB). Iterates the
 * 12x15 grid, awards points_table[row] (×2 for metal) for each cell
 * that's neither destroyed (bit 7) nor undestructible (bit 5), then
 * marks them destroyed so live_bricks_remaining() = 0 ends the level. */
static void award_left_bricks(void) {
    int row, col;
    for (row = 0; row < LVL_ROWS; row++) {
        for (col = 0; col < LVL_COLS; col++) {
            unsigned char *cell = &live_level[row * LVL_COLS + col];
            unsigned int pts, idx;
            if (*cell & 0xA0) continue;          /* destroyed or undestructible */
            idx = (unsigned int)((row < 12) ? row : 11);
            pts = points_table[idx];
            if ((*cell & 0x0F) >= 6) pts *= 2;
            score += pts;
            *cell |= 0x80;
            mark_static_bg_cache_dirty();
        }
    }
}

static void step_rocket(void) {
    int col_lo, col_hi, row_lo, row_hi, r, c;
    int killed_this_tick = 0;
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
        /* Mirror LBB97 → LBBFB: award bonus points for every brick the
         * rocket didn't reach and end the level. Matches the original's
         * "rocket clears the round" gameplay loop. */
        award_left_bricks();
        return;
    }
    /* Map the rocket bbox onto level-grid cells (8 + col*16, 32 +
     * row*8) and destroy every overlapping non-undestructible cell.
     * Use the body region only so the trailing flame doesn't extend
     * the destruction zone. */
    {
        int rx_l = rocket_x;
        int rx_r = rocket_x + ROCKET_W_PX;
        int ry_t = rocket_y;
        int ry_b = rocket_y + ROCKET_H_PX;
        if (rx_l < 8) rx_l = 8;
        if (rx_r > 8 + LVL_COLS * 16) rx_r = 8 + LVL_COLS * 16;
        if (ry_t < 32) ry_t = 32;
        if (ry_b > 32 + LVL_ROWS * 8) ry_b = 32 + LVL_ROWS * 8;
        col_lo = (rx_l - 8) / 16;
        col_hi = (rx_r - 8 - 1) / 16;
        row_lo = (ry_t - 32) / 8;
        row_hi = (ry_b - 32 - 1) / 8;
        for (r = row_lo; r <= row_hi; r++) {
            for (c = col_lo; c <= col_hi; c++) {
                unsigned char *cell;
                if (r < 0 || r >= LVL_ROWS || c < 0 || c >= LVL_COLS) continue;
                cell = &live_level[r * LVL_COLS + c];
                if (*cell & 0x80) continue;          /* already gone */
                if (*cell & 0x20) continue;          /* undestructible */
                {
                    unsigned int idx = (unsigned int)((r < 12) ? r : 11);
                    unsigned int pts = points_table[idx];
                    if ((*cell & 0x0F) >= 6) pts *= 2;
                    score += pts;
                }
                *cell |= 0x80;
                mark_static_bg_cache_dirty();
                brick_flash_spawn(c, r);
                try_spawn_bonus(c, r);
                killed_this_tick = 1;
            }
        }
    }
    /* One brick-click per tick rather than per cell, so the rocket's
     * flight produces a steady rattle instead of a thousand-snd-q
     * spam when it lines up with a packed brick row. */
    if (killed_this_tick) snd_q_push(SND_NORMAL_BRIK);
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

/* LABEE: (threshold, zone) pairs, normal (28-wide) bat. */
static const unsigned char bat_zone_tbl_normal[14] = {
    0x04,0x07, 0x08,0x06, 0x0C,0x05, 0x10,0x00,
    0x14,0x01, 0x18,0x02, 0xFF,0x03
};
/* LABFC: (threshold, zone) pairs, enlarged bat. */
static const unsigned char bat_zone_tbl_big[14] = {
    0x06,0x07, 0x0C,0x06, 0x12,0x05, 0x1A,0x00,
    0x20,0x01, 0x26,0x02, 0xFF,0x03
};
/* LAC0A: [zone&3][incoming-dir index in {04,08,0C,14,18,1C}]. */
static const unsigned char bat_deflect_tbl[4][6] = {
    {0x3C,0x38,0x34,0x2C,0x28,0x24},
    {0x3C,0x38,0x34,0x34,0x34,0x34},
    {0x3C,0x38,0x38,0x34,0x38,0x38},
    {0x3C,0x3C,0x38,0x38,0x3C,0x3C}
};

/* LAB1F_9: dir = ((dir ^ 0x1F) + 1) & 0x3F (vertical reflect). */
static unsigned char bat_reflect_dir(unsigned char dir) {
    return (unsigned char)(((dir ^ 0x1F) + 1) & 0x3F);
}

/* LAB1F_11: index of a downward dir within {04,08,0C,14,18,1C} (A starts
 * at 4, +4 each step, skipping 0x10). Returns 0..5, or -1 for a dir not
 * in the set — only pure-vertical 0x10 / non-multiple-of-4 dirs, which
 * the original assumes never reach the bat (it would loop forever). The
 * caller treats -1 as a plain vertical reflect so the port never hangs. */
static int bat_dir_index(unsigned char dir) {
    int a = 0x04, idx = 0;
    while (idx < 6) {
        if ((unsigned char)a == dir) return idx;
        a += 4;
        if (a == 0x10) a += 4;
        idx++;
    }
    return -1;
}

/* Port of LAB1F_4..LAB1F_12: outgoing dir for a normal (non-catch) bat
 * bounce. big_bat picks the LABFC threshold table. */
static unsigned char bat_deflect_dir(unsigned char dir, int offset,
                                     int big_bat) {
    const unsigned char *t = big_bat ? bat_zone_tbl_big : bat_zone_tbl_normal;
    unsigned char zone;
    int didx;
    if (offset < 0) {
        zone = t[1];                 /* LAB1F_5 carry: first pair's zone */
    } else {
        int i = 0;
        while (i < 12 && (unsigned char)offset >= t[i]) i += 2;
        zone = t[i + 1];
    }
    if (zone & 0x04) {               /* LAB1F_8: reflect, lookup, reflect */
        dir  = bat_reflect_dir(dir);
        didx = bat_dir_index(dir);
        if (didx < 0) return bat_reflect_dir(dir);
        return bat_reflect_dir(bat_deflect_tbl[zone & 3][didx]);
    }
    didx = bat_dir_index(dir);       /* LAB1F_10: lookup only */
    if (didx < 0) return bat_reflect_dir(dir);
    return bat_deflect_tbl[zone & 3][didx];
}

/* Step the ball one frame: handle wall + bat collisions. If the ball
 * exits the bottom of the playfield it respawns stuck on the bat. */
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
    ball_dir_delta_q8(objects[OBJ_BALL_1].dir, objects[OBJ_BALL_1].speed,
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
            snd_q_push(SND_BAT_BEAT);
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
        ball_dir_delta_q8(objects[OBJ_BALL_1].dir, objects[OBJ_BALL_1].speed,
                          &dx_q8, &dy_q8);
        ball_dx = (dx_q8 < 0) ? -1 : (dx_q8 > 0 ? 1 : 0);
        ball_dy = (dy_q8 < 0) ? -1 : (dy_q8 > 0 ? 1 : 0);
        snd_q_push(SND_BAT_BEAT);            /* ball-on-bat */
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
            hit = laffc_collision(BALL_X, BALL_Y, next_x, next_y);
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
/* Shared step routine for the two extra balls spawned by the
 * TRIPLE_BALL bonus. Reads/writes the per-ball velocity through the
 * pointers in_dx/in_dy and the active flag in_active, and the position
 * via the object table at obj_idx. Logic is identical to step_ball
 * minus the catch-bonus and life-decrement paths. */
static void step_extra_ball(unsigned char *in_active,
                             int *in_dx, int *in_dy,
                             unsigned char obj_idx) {
    int next_x, next_y;
    int bat_left  = eff_bat_left();
    int bat_right = eff_bat_right();
    int bat_top   = BAT_Y;
    int ball_sz   = eff_ball_size();
    int bx, by;
    int dx, dy;
    if (!*in_active) return;
    bx = objects[obj_idx].x_coord;
    by = objects[obj_idx].y_coord;
    dx = *in_dx;
    dy = *in_dy;
    next_x = bx + dx;
    next_y = by + dy;
    {
        int x_max = PLAYFIELD_W - 8 - ball_sz;
        if (next_x < BALL_X_MIN)        { next_x = BALL_X_MIN; dx = -dx; }
        else if (next_x > x_max)        { next_x = x_max;      dx = -dx; }
    }
    if (next_y < BALL_Y_TOP) { next_y = BALL_Y_TOP; dy = +BALL_SPEED; }
    /* Bat bounce. The Y geometry mirrors the primary ball's validated
     * LAB1F contact: fire on Y overlap (ball_y >= 167 ⟺ next_y + 7 >
     * bat_top) and rest at $A6 = bat_top - 7 (height, not the width
     * eff_ball_size = 8). The original runs one LAB1F for every ball, so
     * this is correct by construction. The deflection itself still uses
     * the 5-zone approximation here (the secondaries use integer motion,
     * not the q8.8 + dir model, so bat_deflect_dir can't drop in until
     * they're unified — blocked on a multi-ball reference, see
     * notes/bat-deflection.md). */
    if (dy > 0
        && next_y + BALL_H_PX > bat_top
        && next_y < bat_top
        && next_x + ball_sz > bat_left
        && next_x < bat_right) {
        int hit_x = (next_x + ball_sz / 2) - bat_left;
        int span  = bat_right - bat_left;
        next_y  = bat_top - BALL_H_PX;
        dy = -BALL_SPEED;
        if      (hit_x * 5 < span * 1) dx = -2;
        else if (hit_x * 5 < span * 2) dx = -1;
        else if (hit_x * 5 < span * 3) dx = (dx >= 0) ? +1 : -1;
        else if (hit_x * 5 < span * 4) dx = +1;
        else                           dx = +2;
        snd_q_push(SND_BAT_BEAT);
    }
    /* Off-the-bottom: deactivate without life penalty. Threshold matches
     * the primary ball (Y >= 192 = $C0). */
    if (next_y >= PLAYFIELD_H) {
        *in_active = 0;
        objects[obj_idx].sprite_set = 0x82;
        return;
    }
    /* Brick collision identical to ball1 — same brick_collision call. */
    {
        int hit = brick_collision(bx, by, next_x, next_y);
        if (hit == 1)        { dy = -dy; next_y = by; }
        else if (hit == 2)   { dx = -dx; next_x = bx; }
    }
    objects[obj_idx].x_coord = (unsigned char)next_x;
    objects[obj_idx].y_coord = (unsigned char)next_y;
    *in_dx = dx;
    *in_dy = dy;
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
    for (y = 0; y < PLAYFIELD_H; y++) {
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
    }
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
    object_t *enemy = &objects[OBJ_ENEMY];
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
    clear_dirty_ranges(dirty_min_byte, dirty_max_byte);
    if (!static_bg_dirty && score_dirty && can_local_hud) {
        update_static_hud_top(level_idx);
        prev_score = score;
        prev_high_score = high_score;
        mark_dirty_bytes(0, FRAME_TOP_H_PX, 0, 31);
    }
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

    if (bomb_active) {
        blit_masked_to_scr_buff_ptr(spr_bomb_data, bomb_x, bomb_y);
        mark_dirty_rect_px(bomb_x, bomb_y, 16, 16);
    }
    if (pts_400_active) {
        blit_masked_to_scr_buff(pts_marker_spr, pts_400_x, pts_400_y);
        mark_dirty_sprite_rect(pts_marker_spr, pts_400_x, pts_400_y);
    }
    if ((enemy->sprite_set & 0x7F) != 0 && !(enemy->sprite_set & 0x80)) {
        unsigned int spr;
        int spr_w_px, spr_h_px;
        if ((enemy->sprite_set & 0x7F) == 0x0A) {
            unsigned char frame = enemy->sprite_num;
            if (frame >= BLAST_FRAMES) frame = BLAST_FRAMES - 1;
            spr = spr_blast_frames[frame];
        } else {
            unsigned char frame = enemy->sprite_num % 3;
            spr = (enemy->sprite_set == 0x09)
                ? spr_bird_frames[frame]
                : spr_ufo_frames[frame];
        }
        spr_w_px = sprites_blob[spr]     * 8;
        spr_h_px = sprites_blob[spr + 1];
        blit_sprite_attrs_to_buff(enemy->x_coord, enemy->y_coord,
                                   spr_w_px, spr_h_px, bg_attr);
        blit_masked_to_scr_buff(spr, enemy->x_coord, enemy->y_coord);
        mark_dirty_rect_px(enemy->x_coord, enemy->y_coord, spr_w_px, spr_h_px);
    }
    if (bonus_active) {
        unsigned int spr = spr_for_bonus(bonus_type);
        render_bonus_to_buff(bg_attr);
        mark_dirty_sprite_rect(spr, bonus_x, bonus_y);
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
    if (rocket_active) {
        unsigned int spr = current_rocket_spr();
        render_rocket_to_buff();
        mark_dirty_sprite_rect(spr, rocket_x, rocket_y);
    }
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
    restore_top_frame_center(cycle, level_idx);
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
    if (!BALL_VISIBLE || ball_stuck) blockers |= BALL_DIRTY_BLOCK_BALLS;
    if (static_bg_dirty || static_bg_cache_dirty || force_full_flush) blockers |= BALL_DIRTY_BLOCK_STATIC;
    if (score != prev_score || high_score != prev_high_score || lives != prev_lives) blockers |= BALL_DIRTY_BLOCK_HUD;
    if (bonus_active || pts_400_active || bomb_active || rocket_active) blockers |= BALL_DIRTY_BLOCK_OBJECTS;
    if (objects[OBJ_ENEMY].sprite_set != 0) blockers |= BALL_DIRTY_BLOCK_OBJECTS;
    if (any_bullet_active() || any_bullet_blast()) blockers |= BALL_DIRTY_BLOCK_OBJECTS;
    if (brick_flash_ticks || any_brick_hit_anim()) blockers |= BALL_DIRTY_BLOCK_BRICKS;
    if (ball2_active || ball3_active || big_ball_active()) blockers |= BALL_DIRTY_BLOCK_BALLS;
    if (bat_extra_px != bat_extra_tgt || bat_fire_anim_ticks) blockers |= BALL_DIRTY_BLOCK_BAT_FX;
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
    if (!bonus_active && !pts_400_active && objects[OBJ_ENEMY].sprite_set == 0) return 0;
    if (bomb_active || rocket_active) return 0;
    if (any_bullet_active() || any_bullet_blast()) return 0;
    return 1;
}

static void render_enemy_to_buff_and_mark(unsigned char bg_attr) {
    object_t *enemy = &objects[OBJ_ENEMY];
    unsigned int spr;
    int spr_w_px, spr_h_px;
    if ((enemy->sprite_set & 0x7F) == 0 || (enemy->sprite_set & 0x80)) return;
    if ((enemy->sprite_set & 0x7F) == 0x0A) {
        unsigned char frame = enemy->sprite_num;
        if (frame >= BLAST_FRAMES) frame = BLAST_FRAMES - 1;
        spr = spr_blast_frames[frame];
    } else {
        unsigned char frame = enemy->sprite_num % 3;
        spr = (enemy->sprite_set == 0x09)
            ? spr_bird_frames[frame]
            : spr_ufo_frames[frame];
    }
    spr_w_px = sprites_blob[spr] * 8;
    spr_h_px = sprites_blob[spr + 1];
    blit_sprite_attrs_to_buff(enemy->x_coord, enemy->y_coord,
                              spr_w_px, spr_h_px, bg_attr);
    blit_masked_to_scr_buff(spr, enemy->x_coord, enemy->y_coord);
    mark_dirty_rect_px(enemy->x_coord, enemy->y_coord, spr_w_px, spr_h_px);
}

static void render_simple_objects_to_buff_and_mark(unsigned char bg_attr) {
    if (pts_400_active) {
        blit_masked_to_scr_buff(pts_marker_spr, pts_400_x, pts_400_y);
        mark_dirty_sprite_rect(pts_marker_spr, pts_400_x, pts_400_y);
    }
    render_enemy_to_buff_and_mark(bg_attr);
    if (bonus_active) {
        unsigned int spr = spr_for_bonus(bonus_type);
        render_bonus_to_buff(bg_attr);
        mark_dirty_sprite_rect(spr, bonus_x, bonus_y);
    }
}

static void redraw_ball_only(unsigned char level_idx) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle = (unsigned char)(level_idx & 3);
    int bat_x0, bat_x1;

    prof_start();
    restore_prev_dirty_from_static_cache();
    clear_dirty_ranges(dirty_min_byte, dirty_max_byte);
    prof_bg_pit += prof_elapsed();

    render_ball_to_buff(BALL_X, BALL_Y, bg_attr);
    mark_dirty_rect_px(BALL_X, BALL_Y, 16, 12);
    bat_sprite_bounds(BAT_X, bat_extra_px, &bat_x0, &bat_x1);
    paint_bg_window_to_buff(bg_attr, cycle, BAT_Y + 6, 1,
                            bat_x0 >> 3, (bat_x1 - 1) >> 3);
    render_bat(cycle, bg_attr);
    render_running_dot();
    mark_dirty_rect_px(bat_x0, BAT_Y + 6, bat_x1 - bat_x0, 1);
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
    int bat_x0, bat_x1;

    prof_start();
    restore_prev_dirty_from_static_cache();
    clear_dirty_ranges(dirty_min_byte, dirty_max_byte);
    prof_bg_pit += prof_elapsed();

    render_ball_to_buff(BALL_X, BALL_Y, bg_attr);
    mark_dirty_rect_px(BALL_X, BALL_Y, 16, 12);
    bat_sprite_bounds(BAT_X, bat_extra_px, &bat_x0, &bat_x1);
    paint_bg_window_to_buff(bg_attr, cycle, BAT_Y + 6, 1,
                            bat_x0 >> 3, (bat_x1 - 1) >> 3);
    render_bat(cycle, bg_attr);
    render_running_dot();
    mark_dirty_rect_px(bat_x0, BAT_Y + 6, bat_x1 - bat_x0, 1);
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
static void score_to_codes(unsigned long s, unsigned char out[6]) {
    int i;
    for (i = 5; i >= 0; i--) {
        out[i] = (unsigned char)(s % 10);
        s /= 10;
    }
}

#ifndef BATTY_SCORELESS_HUD
static void draw_score_digits_original(int x, int y, unsigned long value) {
    unsigned char digits[6];
    int i;
    score_to_codes(value, digits);
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
    blit_masked_to_scr_buff_ptr(hud_sprites + HUD_SPR_1UP, 0x1C, 0x0C);
    blit_masked_to_scr_buff_ptr(hud_sprites + HUD_SPR_2UP, 0xCC, 0x0C);
    blit_masked_to_scr_buff_ptr(hud_sprites + HUD_SPR_HI,  0x78, 0x0C);
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
    score_to_codes(score, digits);
    draw_text(BORDER_X + 3 * 8,        BORDER_Y +  95, 15, sc_lbl, (int)sizeof(sc_lbl));
    draw_text(BORDER_X + 3 * 8 + 6*8,  BORDER_Y +  95, 15, digits, 6);
    score_to_codes(high_score, digits);
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

static void brick_hit_anim_spawn(int col, int row) {
    int i;
    for (i = 0; i < BRICK_HIT_ANIM_SLOTS; i++) {
        if (brick_hit_anim_ticks[i]
            && brick_hit_anim_col[i] == (unsigned char)col
            && brick_hit_anim_row[i] == (unsigned char)row) {
            brick_hit_anim_ticks[i] = 1;
            return;
        }
    }
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
        /* Free the slot only when the brick is destroyed (cell bit 7) —
         * metal_brik_anim's `BIT 7,(HL) -> mark slot free`. An
         * undestructible (metal) brick is never destroyed, so it
         * shimmers PERMANENTLY once hit; a multi-hit brick shimmers until
         * its final hit sets bit 7. */
        {
            int col = brick_hit_anim_col[i];
            int row = brick_hit_anim_row[i];
            if (row >= LVL_ROWS || col >= LVL_COLS
                || (live_level[row * LVL_COLS + col] & 0x80)) {
                brick_hit_anim_ticks[i] = 0;
                continue;
            }
        }
        /* Cycle the 8-frame (x2-tick) counter forever instead of stopping
         * after one pass — matches the original counter `(c+1) & $0F`. */
        if (++brick_hit_anim_ticks[i] > BRICK_HIT_ANIM_TICKS)
            brick_hit_anim_ticks[i] = 1;
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
 * 2 PIT ticks per frame = 16 ticks = ~0.32 s total. The "solid" frame
 * (spr_brik_5, index 4) emits a quick metallic beep — port of the
 * play_sound_metal_brik call gated on the "any-metal-brick" check at
 * $B73F. ESC quits, any other key short-circuits. */
static int play_brik_anim(void) {
    int step;
    int ping_played = 0;    /* SMC trick in original: one_play_sound_metal_brik
                             * rewrites itself to RET after the first call, so
                             * the ping fires once even though spr_brik_5 appears
                             * twice in anim_brik. */
    for (step = 0; step < 8; step++) {
        unsigned long t;
        unsigned char frame = brik_anim_order[step];
        brik_anim_apply_frame(frame);
        buff_to_vga_rect_bytes(32, 96, 1, 30);
        if (frame == 4 && !ping_played) {
            sound_beep_cont_d(0x18, 0x30);      /* play_sound_metal_brik */
            ping_played = 1;
        }
        t = pit_ticks();
        while (pit_ticks() - t < 2UL) {
            sound_tick();
            if (kbhit()) {
                int k = getch();
                if (k == 27) return 1;
                sound_silence();
                return 0;
            }
        }
    }
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
    /* PLAYER 1 — single-player mode hardcodes the digit. Once 2-player
     * wiring lands, swap the trailing $01 for the active player number. */
    draw_text(text_x, BORDER_Y + 143, 15, player_codes, 7);
    {
        unsigned char one = 0x01;
        draw_text(text_x + 7 * 8, BORDER_Y + 143, 15, &one, 1);
    }
    draw_text(text_x, BORDER_Y + 158, 15, round_codes, 8);

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

/* Direction LUT at $AD58 — 17 sin values in [0, $FF]. Mirror of
 * direction_table; used by dir_to_dxdy to decode a 6-bit angle into
 * (dx, dy) in 8.8 fixed point. */
static const unsigned char dir_sin_tbl[17] = {
    0xFF,0xFD,0xFA,0xF4,0xE6,0xE0,0xD4,0xC5,
    0xB4,0xA1,0x8D,0x78,0x61,0x4A,0x31,0x18,
    0x00
};

/* Port of hl_bc_calc_direction at $AD22 + the LAD13 speed multiply.
 * LAD13 treats the direction-table byte as a magnitude, multiplies by
 * speed, then two's-complement negates the product when the component's
 * sign byte is $FF. Negative components are therefore `-magnitude`, not
 * `magnitude - 256`. Returns signed 8.8 fixed-point displacement. */
static void dir_to_dxdy(unsigned char dir, unsigned char speed,
                         int *out_dx, int *out_dy) {
    unsigned char q = dir & 0x30;
    unsigned char d = dir & 0x0F;
    int C = dir_sin_tbl[d];
    int L = dir_sin_tbl[16 - d];
    int hl, bc;
    switch (q) {
        case 0x00: hl = L;          bc = C;          break;
        case 0x10: hl = C;          bc = -L;         break;
        case 0x20: hl = -L;         bc = -C;         break;
        default:   hl = -C;         bc = L;          break;
    }
    /* LAD69 adds the hl_bc_calc_direction BC term to X and the HL term to
     * Y (it PUSHes HL, multiplies BC into X, then POPs HL for Y), so the
     * two components are CROSSED relative to the table read. Assigning
     * them straight (out_dx=hl) put the wrong magnitude on X — e.g. dir
     * $1F gave dx=+0.28 px where the Spectrum moves dx=-3 px (probed via
     * scripts/capture_frame_timeline_original.py --probe-ball). */
    *out_dx = bc * (int)speed;
    *out_dy = hl * (int)speed;
}

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
    snd_q_silence_all();
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
    slow_ticks     = 0;
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
    snd_q_push(SND_SPARK_FANOUT);
    last = pit_ticks();
    do {
        unsigned long now;
        do {
            sound_tick();
            now = pit_ticks();
        } while (now == last);
        last = now;
        snd_q_tick();
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
    snd_q_silence_all();
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
    slow_ticks = 0;
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
        i = lvl_idx;                                 /* keep `i` alias for the cycle / bg_attr code below */
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
        slow_ticks     = 0;
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
        snd_q_silence_all();
        fast_memcpy(live_level, &levels[(int)i * LVL_CELLS], LVL_CELLS);
        apply_replay_random_override();
        apply_replay_bat_object_override();
        apply_replay_ball_object_override();
        apply_replay_ball_motion_override();
        apply_replay_enemy_object_override();
        apply_replay_rocket_override();
        write_replay_probe();
        render_level_screen(i);
        if (show_round_banner((unsigned int)round_number + 1)) return ST_QUIT;
        render_level_screen(i);                /* re-paint to clear the banner */
        if (play_brik_anim()) return ST_QUIT;
        /* Replay parity hook: block here until the harness sends a key,
         * giving the original side a matching breakpoint to halt at and
         * letting both runners capture the static L3-entry screen with
         * no wall-clock drift. The wake key is consumed below so it
         * doesn't double as the next main-loop input. */
        if (getenv("BATTY_REPLAY_WAIT_KEY") != NULL) {
            while (!kbhit()) {
                sound_tick();
            }
            (void)getch();
        }
        cycle = (unsigned char)(i & 3);
        bg_attr = bg_attr_per_cycle[i & 3];
        start     = bios_ticks();
        last_tick = pit_ticks();
        for (;;) {
            unsigned long now;
            int ball_moved = 0;
            int bat_moved  = 0;
            int frame_ticked = 0;

            if (kbhit()) {
                int k = getch();
                if (k == KEY_ESC) {
                    write_replay_probe();
                    return ST_QUIT;
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
                    continue;
                }
                if (paused) {
                    if (k == KEY_ENTER) break;        /* allow level advance */
                    continue;                          /* swallow other input */
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
                        snd_q_push(SND_BALL_START); /* descending launch blip */
                        primary_ball_launch_from_bat();
                        record_primary_launch();
                    }
                    /* If the bat carries the LASER bonus and a free
                     * bullet slot exists, also fire one from the bat
                     * top centre. Two slots = up to two in flight at
                     * once (port of object_bullet_1 / _2 at $A0FA).
                     * Independent of ball state — SPACE can refire
                     * the laser while the ball is in play. */
                    if (!rocket_active
                        && objects[OBJ_BAT_1].bonus_applied == 0x01
                        && bullet_cooldown == 0) {
                        int free_slot = -1;
                        int j;
                        for (j = 0; j < N_BULLETS; j++) {
                            if (!bullet_active[j]) { free_slot = j; break; }
                        }
                        if (free_slot >= 0) {
                            bullet_active[free_slot] = 1;
                            /* Original free_bullet_2 at \$A14C:
                             *   bullet_x = bat_x + \$0C (= +12 px)
                             *   bullet_y = \$AC (= 172, one px above bat top)
                             * Bullet emerges from the bat surface, not floating above. */
                            bullet_x[free_slot] = BAT_X + 12;
                            bullet_y[free_slot] = BAT_Y - 1;
                            bat_fire_anim_ticks = 8;
                            bullet_cooldown = 0x16;          /* ~11 frames @ -2 / frame */
                            snd_q_push(SND_SHOT);
                        }
                    }
                    start = bios_ticks();
                }
                /* Mirror the original: no level-skip key. ENTER while
                 * playing does nothing (only the pause overlay above
                 * consumes ENTER to dismiss). The level holds until
                 * the player clears it or loses all lives. */
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
                /* Per-frame RNG tick (original LB9E8_2: one `CALL
                 * random_generate` per main-loop pass). Gated so the
                 * default on-demand model is byte-unchanged. */
                if (rng_perframe) next_random();
                /* Per-frame keyboard polling - mirrors
                 * get_left_player_ctrl_state ($A161) which reads the
                 * keyboard half-row IN A,($FE) and updates
                 * ctrl_btns_pressed.x bits 0/1, then handling_bat at
                 * $9F64 SUB/ADD $04 on (IX+$02). Step is 4 px / 50 Hz
                 * tick = 200 px/sec, matching the original. */
                /* Margins port of check_left_margin (\$ACA2) +
                 * check_right_margin (\$ACBC). The original BIG_BAT
                 * grows body to the right only (sprite stays anchored
                 * at bat_x), so its clamp uses (\$F8 - body_w). Our
                 * port renders BIG_BAT centred on BAT_X by shifting
                 * the sprite \$bat_extra_px\$ to the left, so the
                 * VISIBLE body extends bat_extra_px on each side:
                 *   visible left  = BAT_X - bat_extra_px
                 *   visible right = BAT_X + BAT_BODY_W + bat_extra_px
                 * Clamp those to the playfield [8, 248]. */
                {
                    int min_now = 8 + bat_extra_px;
                    int max_now = 248 - BAT_BODY_W - bat_extra_px;
                    if (!rocket_active && key_state[SC_LEFT]  && BAT_X > min_now) BAT_X -= 4;
                    if (!rocket_active && key_state[SC_RIGHT] && BAT_X < max_now) BAT_X += 4;
                }
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
                        snd_q_push(SND_BALL_START);
                        primary_ball_launch_from_bat();
                        record_primary_launch();
                    }
                } else if (BALL_VISIBLE) {
                    int slow_skip = (slow_ticks > 0) && ((now & 1) == 0);
                    if (!slow_skip) {
                        step_ball();
                        ball_moved = 1;
                        if (launch_probe_active) {
                            if (launch_probe_countdown > 0) launch_probe_countdown--;
                            if (launch_probe_countdown == 0) {
                                write_replay_probe();
                                return ST_QUIT;
                            }
                        }
                        if (frame_probe_active) {
                            if (frame_probe_countdown > 0) frame_probe_countdown--;
                            if (frame_probe_countdown == 0) {
                                write_replay_probe();
                                return ST_QUIT;
                            }
                        }
                    }
                }
                step_bonus();
                step_pts_400();
                step_bomb();
                step_bullet();
                step_bullet_blast();
                step_rocket();
                step_brick_flash();
                step_brick_hit_anim();
                if (bat_fire_anim_ticks) bat_fire_anim_ticks--;
                if (bullet_cooldown >= 2) bullet_cooldown -= 2;     /* SUB \$02 / frame */
                else bullet_cooldown = 0;
                /* SLOW affects ALL balls in the original (sets ball_1/2/3
                 * speed bytes simultaneously) — mirror by gating extras
                 * on the same slow_skip half-frame the primary uses. */
                {
                    int extras_slow_skip = (slow_ticks > 0) && ((now & 1) == 0);
                    if (!extras_slow_skip) {
                        step_ball2();
                        step_ball3();
                    }
                }
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
                call_for_all_obj(ix_buf_addr_calc);
                snd_q_tick();
                sound_tick();
                /* Score-milestone extra life — port of score_update_3
                 * at $0395. Each crossed threshold in live_add_thresholds
                 * awards one extra life and pushes the live-add sound. */
                while (live_adds_awarded < LIVE_ADD_COUNT
                       && score >= live_add_thresholds[live_adds_awarded]) {
                    lives++;
                    snd_q_push(SND_LIVE_ADD);
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
                snd_q_silence_all();
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
                    save_high_score();
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
    if (getenv("BATTY_RNG_PERFRAME") != NULL) rng_perframe = 1;
    if (getenv("BATTY_FORCE_FULL_FLUSH_EACH_FRAME") != NULL) force_full_flush_each_frame = 1;
    if (getenv("BATTY_SUPPRESS_NO_BALL_DEATH") != NULL) suppress_no_ball_death = 1;
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
        sound_disabled = 1;
    set_mode(0x13);
    set_palette(zx_palette, 16);
    init_pal_tables();

    if (load_font("FONT.BIN") != 0 ||
        load_indicator("INDICAT.BIN") != 0 ||
        load_bottom_sprites("BOTSPR.BIN") != 0 ||
        load_hud_sprites("HUDSPR.BIN") != 0 ||
        load_levels("LEVELS.BIN") != 0 ||
        load_level_attrs("LVLATTR.BIN") != 0 ||
        load_bg_tile("BGTILE.BIN") != 0 ||
        load_frame("FRAMEL1.BIN") != 0 ||
        load_sprites("SPRITES.BIN") != 0 ||
        load_random_rom("RANDOM.BIN") != 0) {
        fill(0, 0, SCREEN_W, SCREEN_H, 10 /* bright red */);
    }
    set_rocket_bonus_sprite_height(ROCKET_BONUS_H_PX);

    load_high_score();
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
    set_mode(0x03);
    return 0;
}
