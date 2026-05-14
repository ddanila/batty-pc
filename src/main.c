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
#include <i86.h>
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

/* Stream a 256x192 8bpp asset straight into VGA. 192 reads of 256 B —
 * keeps the small-model near-data segment unburdened. */
static int blit_screen(const char *path) {
    FILE *f = fopen(path, "rb");
    int y;
    unsigned char row_buf[PLAYFIELD_W];
    if (!f) return -1;
    for (y = 0; y < PLAYFIELD_H; y++) {
        if (fread(row_buf, 1, PLAYFIELD_W, f) != PLAYFIELD_W) {
            fclose(f);
            return -2;
        }
        _fmemcpy(vga + (long)(BORDER_Y + y) * SCREEN_W + BORDER_X,
                 row_buf, PLAYFIELD_W);
    }
    fclose(f);
    return 0;
}

/* Repaint the border + blit one named asset. On asset-missing, paint
 * the playfield magenta so the failure is unmistakable in QEMU. */
static void show(const char *path) {
    fill(0, 0, SCREEN_W, SCREEN_H, COL_BORDER);
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
#define BRICK_W_PX 16
#define BRICK_H_PX  8
#define CACHE_SIZE 3584
#define BRICK_FIELD_X  8     /* relative to playfield top-left */
#define BRICK_FIELD_Y 16

/* Per-level attribute band: 12 char-rows x 32 cols of ZX attribute
 * bytes, captured from the brick-field zone of each level's GT
 * .scr. Lookup: attr = level_attrs[lvl*ATTR_BAND_SIZE + r*32 + col]. */
#define ATTR_ROWS       12
#define ATTR_COLS       32
#define ATTR_BAND_SIZE  (ATTR_ROWS * ATTR_COLS)
#define ATTR_TOTAL_SIZE (N_LEVELS * ATTR_BAND_SIZE)

static unsigned char levels[LVL_SIZE];
static unsigned char sprite_cache[CACHE_SIZE];
static unsigned char level_attrs[ATTR_TOTAL_SIZE];

/* 16x16-pixel hex pattern tile (1bpp; 2 bytes wide x 16 rows = 32 B).
 * Tiled across the playfield as the level background; colour comes
 * from the level's bg attribute (top-left of its attr band). */
#define BG_TILE_W_PX 16
#define BG_TILE_H_PX 16
#define BG_TILE_SIZE (BG_TILE_H_PX * 2)
static unsigned char bg_tile[BG_TILE_SIZE];

static int load_levels(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fread(levels, 1, sizeof(levels), f) != sizeof(levels)) {
        fclose(f); return -2;
    }
    fclose(f);
    return 0;
}

static int load_sprite_cache(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fread(sprite_cache, 1, sizeof(sprite_cache), f) != sizeof(sprite_cache)) {
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

/* ZX attribute byte -> our 16-entry palette indices for ink + paper.
 * attr = flash | bright | paper(3) | ink(3). bit 6 = bright. */
static unsigned char ink_pal(unsigned char attr) {
    return (unsigned char)((attr & 7) | ((attr & 0x40) >> 3));
}
static unsigned char paper_pal(unsigned char attr) {
    return (unsigned char)(((attr >> 3) & 7) | ((attr & 0x40) >> 3));
}

/* Blit one 16x8 chunk from sprite_cache[cell*16..] to VGA at (x, y).
 * Bit-set pixels use `ink`, bit-clear use `paper`. */
static void draw_brick(int x, int y, unsigned char cell,
                       unsigned char ink, unsigned char paper) {
    int row, byte_col, bit;
    unsigned char b;
    const unsigned char *chunk = sprite_cache + (int)cell * 16;
    for (row = 0; row < BRICK_H_PX; row++) {
        for (byte_col = 0; byte_col < 2; byte_col++) {
            b = chunk[row * 2 + byte_col];
            for (bit = 0; bit < 8; bit++) {
                vga[(long)(y + row) * SCREEN_W + x + byte_col * 8 + bit] =
                    (b & (0x80 >> bit)) ? ink : paper;
            }
        }
    }
}

static void render_brick_field(unsigned char level_idx) {
    int r, c, base, attr_base, x, y;
    unsigned char cell, attr, ink, paper;
    if (level_idx >= N_LEVELS) return;
    base      = (int)level_idx * LVL_CELLS;
    attr_base = (int)level_idx * ATTR_BAND_SIZE;
    for (r = 0; r < LVL_ROWS; r++) {
        for (c = 0; c < LVL_COLS; c++) {
            cell = levels[base + r * LVL_COLS + c];
            /* Brick at grid col c occupies char cols (1+c*2) and (1+c*2+1)
             * in the 32-col attr band. Left half's attr is canonical
             * (both halves share colour for any one brick). */
            attr  = level_attrs[attr_base + r * ATTR_COLS + 1 + c * 2];
            ink   = ink_pal(attr);
            paper = paper_pal(attr);
            x = BORDER_X + BRICK_FIELD_X + c * BRICK_W_PX;
            y = BORDER_Y + BRICK_FIELD_Y + r * BRICK_H_PX;
            /* sub_adbch's per-frame blitter skips on (bit 7 | bit 4)
             * because bit-4-set cells are statically painted once at
             * level-init from a different sprite source we haven't
             * located yet (see notes/levels.md). We mask only bit 7
             * here so the 0x11..0x15 cells still render via cache[V*16]
             * — visually imperfect but covers the GT frame area
             * better than leaving plain hex bg. */
            if (cell & 0x80) continue;
            draw_brick(x, y, cell, ink, paper);
        }
    }
}

/* Tile the 16x16 hex pattern across the full 256x192 playfield,
 * using `attr`'s ink for set bits and paper for clear bits. Painted
 * BEFORE the bricks so the bricks overwrite the pattern in their
 * 16x8 cells. */
static void paint_hex_bg(unsigned char attr) {
    unsigned char ink   = ink_pal(attr);
    unsigned char paper = paper_pal(attr);
    int x, y, tx, ty, byte_col, bit;
    unsigned char b;
    for (y = 0; y < PLAYFIELD_H; y++) {
        ty = y & 15;
        for (byte_col = 0; byte_col < PLAYFIELD_W / 8; byte_col++) {
            /* Source byte: tile row ty, byte_col mod 2 (= 16-px h period). */
            b = bg_tile[ty * 2 + (byte_col & 1)];
            tx = byte_col * 8;
            for (bit = 0; bit < 8; bit++) {
                x = tx + bit;
                vga[(long)(BORDER_Y + y) * SCREEN_W + BORDER_X + x] =
                    (b & (0x80 >> bit)) ? ink : paper;
            }
        }
    }
}

static void render_level_screen(unsigned char level_idx) {
    /* Per-level background attribute: cols 0 / 1 carry side-edge
     * stripes; the bulk bg starts at col 2. Sample (row 0, col 14) —
     * deep inside the brick band, where the attr is reliably the
     * level's bg colour. */
    unsigned char bg_attr = level_attrs[(int)level_idx * ATTR_BAND_SIZE + 14];
    fill(0, 0, SCREEN_W, SCREEN_H, COL_BORDER);
    draw_frame(10);              /* bright red — placeholder */
    paint_hex_bg(bg_attr);
    render_brick_field(level_idx);
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

/* In test mode (BATTYALL=1) we want explicit key control over every
 * transition. Set at startup based on env. */
static int auto_advance = 1;
#define TIMED_OUT(start, ticks) (auto_advance && (bios_ticks() - (start) > (ticks)))

/* Blink phase for the selected option's text. Test mode pins it to 0
 * (BLACK / invisible) so the screendump matches snap2's captured BLACK
 * half deterministically. `make run` uses real-time bios_ticks so the
 * user sees the actual blink. */
static int blink_phase(void) {
    if (!auto_advance) return 0;
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

/* Cycle through L1..L15 (and back to TITLE), pausing
 * LEVEL_TIMEOUT_TICKS on each. Any key advances; ESC quits. */
static state_t run_level(void) {
    unsigned char i;
    unsigned long start;
    for (i = 0; i < N_LEVELS; i++) {
        render_level_screen(i);
        start = bios_ticks();
        for (;;) {
            if (kbhit()) {
                int k = getch();
                if (k == 27) return ST_QUIT;
                break;
            }
            if (TIMED_OUT(start, LEVEL_TIMEOUT_TICKS)) break;
        }
    }
    return ST_TITLE;
}

int main(void) {
    state_t state = ST_TITLE;
    /* BATTYALL=1 (test floppy AUTOEXEC) disables auto-advance so the
     * test orchestrator's `sendkey` drives every transition. Plain
     * `make run` floppy leaves it on for the natural attract cycle. */
    if (getenv("BATTYALL") != NULL) auto_advance = 0;
    set_mode(0x13);
    set_palette(zx_palette, 16);

    if (load_font("FONT.BIN") != 0 ||
        load_indicator("INDICAT.BIN") != 0 ||
        load_bottom_sprites("BOTSPR.BIN") != 0 ||
        load_levels("LEVELS.BIN") != 0 ||
        load_sprite_cache("CACHE.BIN") != 0 ||
        load_level_attrs("LVLATTR.BIN") != 0 ||
        load_bg_tile("BGTILE.BIN") != 0) {
        fill(0, 0, SCREEN_W, SCREEN_H, 10 /* bright red */);
    }

    while (state != ST_QUIT) {
        switch (state) {
            case ST_TITLE:   state = run_title();   break;
            case ST_MENU:    state = run_menu();    break;
            case ST_HISCORE: state = run_hiscore(); break;
            case ST_LEVEL:   state = run_level();   break;
            default:         state = ST_QUIT;       break;
        }
    }

    set_mode(0x03);
    return 0;
}
