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

static const unsigned char spr_brik_1[16] = {
    0xFF, 0xFE, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00,
    0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x00, 0x00
};

static const unsigned char briks_colors[10] = {
    0x00,                          /* [0] never indexed (low nibble == 0
                                    * means "skip" per the cell format). */
    0x57, 0x4F, 0x5F, 0x20, 0x70,  /* normal bricks */
    0x47, 0x57, 0x5F, 0x4F         /* metal bricks */
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
#define BAT_W_BYTES 5
#define BAT_H_PX    19
#define BAT_SIZE    (BAT_W_BYTES * BAT_H_PX)
#define BAT_Y_PX    167
#define BAT_CYCLES  4
/* X clamp: bat is 40 px wide. Left frame edge ~= pixel 16, right
 * ~= 240 in playfield coords. Per the original, handling_bat
 * advances X by +/-4 per frame so we stick to multiples of 4. */
#define BAT_X_MIN     8
#define BAT_X_MAX   216
#define BAT_X_INIT  112
static unsigned char bat_l1[BAT_CYCLES * BAT_SIZE];
static int bat_x      = BAT_X_INIT;
static int bat_x_prev = BAT_X_INIT;

/* Ball state. The original keeps this in object_ball_1 (descriptor at
 * $9AD0); we use a simpler 4x4 placeholder until we port the proper
 * obj table + masked sprite blitter. ball_stuck = ball sits on the
 * bat (moves with it); SPACE releases. */
#define BALL_W_PX   4
#define BALL_H_PX   4
#define BALL_SPEED  2
#define BALL_X_OFFSET_ON_BAT 18  /* relative to bat_x: roughly centred
                                  * on the 40-px bat. */
#define BALL_Y_TOP    24         /* just below the 24-px HUD */
#define BALL_X_MIN     8
#define BALL_X_MAX   244         /* 256 - 8 - BALL_W_PX */
static int ball_x      = BAT_X_INIT + BALL_X_OFFSET_ON_BAT;
static int ball_y      = BAT_Y_PX - BALL_H_PX;
static int ball_dx     = +BALL_SPEED;
static int ball_dy     = -BALL_SPEED;
static unsigned char ball_stuck   = 1;
static unsigned char ball_visible = 0;  /* drawn only after first SPACE
                                         * (keeps state4 capture clean). */

/* Game-loop state. score is a plain integer; the original uses a
 * 3-byte BCD-ish representation across current_score_1up + the in-game
 * digits at score_1up_in_game. lives starts at 3 per original
 * game_restart at $B9A0 (LD A,$03 / LD (lives_1up),A). */
#define POINTS_PER_BRICK   50      /* placeholder; the original picks
                                    * per-colour values via brik_value
                                    * at $B2BD - port deferred. */
#define LIVES_INIT          3
static unsigned long score = 0;
static int           lives = LIVES_INIT;

/* Second life indicator (the right-hand of the pair at bottom-left).
 * The first one is captured by the 3-col-wide left frame strip; this
 * is the part that falls outside the frame. 2 bytes wide x 8 rows. */
#define LIVES_W_BYTES 2
#define LIVES_H_PX    8
#define LIVES_SIZE    (LIVES_W_BYTES * LIVES_H_PX)
#define LIVES_X_PX    24     /* byte_x 3 = pixel x 24 */
#define LIVES_Y_PX    183
#define LIVES_CYCLES  4
static unsigned char lives_l1[LIVES_CYCLES * LIVES_SIZE];

/* Perimeter frame (top + left + right, no bottom). Each side strip is
 * 3 cols wide -- the third col (col 2 left, col 29 right) is the
 * shadow the original casts just inside the cyan frame edge.
 *   top  pixels: 32 cols x 16 rows  = 512 B
 *   top  attrs : 32 cols x  2 rows  =  64 B
 *   left pixels:  3 cols x 176 rows = 528 B
 *   left attrs :  3 cols x  22 rows =  66 B
 *   right pixels: 3 cols x 176 rows = 528 B
 *   right attrs : 3 cols x  22 rows =  66 B
 * Total: 1764 B. */
#define FRAME_SIDE_W     3
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

static int load_bat(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fread(bat_l1, 1, sizeof(bat_l1), f) != sizeof(bat_l1)) {
        fclose(f); return -2;
    }
    fclose(f);
    return 0;
}

static int load_lives(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fread(lives_l1, 1, sizeof(lives_l1), f) != sizeof(lives_l1)) {
        fclose(f); return -2;
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

/* Paint a strip of pixels with per-char-cell ink/paper. `pixels` is
 * `cols_bytes` bytes wide and `rows_px` rows tall (row-major). `attrs`
 * is also indexed [char_row][char_col] but its row stride is
 * `attr_stride` (in bytes), which can differ from `cols_bytes` when
 * the attr source is a wider 2D buffer (e.g. the 32-col-wide
 * level_attrs band sliced down to a 3-col side strip). The strip is
 * drawn at playfield-relative (x0_px, y0_px). */
static void paint_strip(const unsigned char *pixels,
                        const unsigned char *attrs, int attr_stride,
                        int cols_bytes, int rows_px,
                        int x0_px, int y0_px) {
    int char_row, char_col, pix_row, bit;
    unsigned char attr, ink, paper, b;
    int char_rows = rows_px / 8;
    for (char_row = 0; char_row < char_rows; char_row++) {
        for (char_col = 0; char_col < cols_bytes; char_col++) {
            attr  = attrs[char_row * attr_stride + char_col];
            ink   = ink_pal(attr);
            paper = paper_pal(attr);
            for (pix_row = 0; pix_row < 8; pix_row++) {
                int y = char_row * 8 + pix_row;
                b = pixels[y * cols_bytes + char_col];
                for (bit = 0; bit < 8; bit++) {
                    vga[(long)(BORDER_Y + y0_px + y) * SCREEN_W +
                        BORDER_X + x0_px + char_col * 8 + bit] =
                        (b & (0x80 >> bit)) ? ink : paper;
                }
            }
        }
    }
}

/* Paint the perimeter frame using L1's pixel bits (the ornament
 * shape is level-invariant; HUD score digits do drift across our
 * patched-capture GTs but L1's "0/100k/0" matches a fresh start
 * anywhere) with PER-LEVEL attrs sourced from level_attrs. The
 * attrs colour the cyan ornament, the bg shadow band, and the
 * HUD digits correctly per level. */
static void render_frame(unsigned char cycle, unsigned char level_idx) {
    const unsigned char *base     = frame_l1 + (unsigned int)cycle * FRAME_SIZE;
    const unsigned char *top_px   = base;
    const unsigned char *left_px  = top_px  + FRAME_TOP_PX  + FRAME_TOP_ATTRS;
    const unsigned char *right_px = left_px + FRAME_SIDE_PX + FRAME_SIDE_ATTRS;
    const unsigned char *lattr = level_attrs + (unsigned int)level_idx * ATTR_BAND_SIZE;
    int right_col = 32 - FRAME_SIDE_W;
    paint_strip(top_px,   lattr, 32, 32, FRAME_TOP_H_PX,
                0, 0);
    paint_strip(left_px,  lattr + (FRAME_TOP_H_PX / 8) * ATTR_COLS, 32,
                FRAME_SIDE_W, FRAME_SIDE_H_PX,
                0, FRAME_TOP_H_PX);
    paint_strip(right_px, lattr + (FRAME_TOP_H_PX / 8) * ATTR_COLS + right_col, 32,
                FRAME_SIDE_W, FRAME_SIDE_H_PX,
                right_col * 8, FRAME_TOP_H_PX);
}

/* Paint a width-bytes x height-px raw-pixel block at (x_px, y_px)
 * playfield-relative, using ink/paper from `attr`. Used for all
 * paint-it-verbatim composites (bat, lives, etc.). */
static void paint_block(const unsigned char *src, int w_bytes, int h_px,
                        int x_px, int y_px, unsigned char attr) {
    unsigned char ink   = ink_pal(attr);
    unsigned char paper = paper_pal(attr);
    int r, c, bit;
    unsigned char b;
    int x0 = BORDER_X + x_px;
    int y0 = BORDER_Y + y_px;
    for (r = 0; r < h_px; r++) {
        for (c = 0; c < w_bytes; c++) {
            b = src[r * w_bytes + c];
            for (bit = 0; bit < 8; bit++) {
                vga[(long)(y0 + r) * SCREEN_W + x0 + c * 8 + bit] =
                    (b & (0x80 >> bit)) ? ink : paper;
            }
        }
    }
}

static void render_bat(unsigned char cycle, unsigned char attr) {
    const unsigned char *src = bat_l1 + (int)cycle * BAT_SIZE;
    paint_block(src, BAT_W_BYTES, BAT_H_PX, bat_x, BAT_Y_PX, attr);
}

static void render_lives(unsigned char cycle, unsigned char attr) {
    const unsigned char *src = lives_l1 + (int)cycle * LIVES_SIZE;
    paint_block(src, LIVES_W_BYTES, LIVES_H_PX,
                LIVES_X_PX, LIVES_Y_PX, attr);
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
     * spans. */
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

/* Pre-fill scr_buff with the hex tile and attr_buff with per-level
 * bg attrs across the brick band, run the brick compositor, then blit
 * the band from scr_buff/attr_buff to VGA. Replaces the prior
 * render_brick_field path. */
static void render_brick_band(unsigned char level_idx) {
    int char_row, char_col, y, byte_col, bit;
    unsigned char cycle = (unsigned char)(level_idx & 3);
    const unsigned char *tile  = bg_tile + (int)cycle * BG_TILE_SIZE;
    const unsigned char *cells = live_level;
    const unsigned char *lattr = &level_attrs[(int)level_idx * ATTR_BAND_SIZE];

    if (level_idx >= N_LEVELS) return;

    /* (1) Background: fill scr_buff brick band (pixel y 24..135) with
     * the hex tile bits — covers char rows 3..16 so the brick edge
     * writes at y=31 and y=128 land on a defined bg. */
    for (y = 24; y < 136; y++) {
        int ty = y & 15;
        for (byte_col = 0; byte_col < 32; byte_col++) {
            scr_buff[y * 32 + byte_col] = tile[ty * 2 + (byte_col & 1)];
        }
    }

    /* (2) Attrs: copy per-level attrs for char rows 3..16. */
    for (char_row = 3; char_row < 17; char_row++) {
        for (char_col = 0; char_col < 32; char_col++) {
            attr_buff[char_row * 32 + char_col] =
                lattr[char_row * ATTR_COLS + char_col];
        }
    }

    /* (3) Brick compositor — overwrites brick bodies, edges, and the
     * brick + shadow attrs. */
    print_briks_c(cells);

    /* (4) Blit pixel y 31..128 from scr_buff/attr_buff to VGA. */
    for (y = BRICK_BAND_Y_TOP; y <= BRICK_BAND_Y_BOT; y++) {
        for (byte_col = 0; byte_col < 32; byte_col++) {
            unsigned char b = scr_buff[y * 32 + byte_col];
            unsigned char attr = attr_buff[(y / 8) * 32 + byte_col];
            unsigned char ink = ink_pal(attr);
            unsigned char paper = paper_pal(attr);
            for (bit = 0; bit < 8; bit++) {
                vga[(long)(BORDER_Y + y) * SCREEN_W + BORDER_X + byte_col * 8 + bit] =
                    (b & (0x80 >> bit)) ? ink : paper;
            }
        }
    }
}

/* Tile the 16x16 hex pattern across the full 256x192 playfield,
 * using `attr`'s ink for set bits and paper for clear bits. Tile
 * pattern picked from `cycle` (0..3 = yellow/green/cyan/white).
 * Painted BEFORE the bricks so the bricks overwrite the pattern in
 * their 16x8 cells. */
static void paint_hex_bg(unsigned char attr, unsigned char cycle) {
    unsigned char ink   = ink_pal(attr);
    unsigned char paper = paper_pal(attr);
    const unsigned char *tile = bg_tile + (int)cycle * BG_TILE_SIZE;
    int x, y, tx, ty, byte_col, bit;
    unsigned char b;
    for (y = 0; y < PLAYFIELD_H; y++) {
        ty = y & 15;
        for (byte_col = 0; byte_col < PLAYFIELD_W / 8; byte_col++) {
            b = tile[ty * 2 + (byte_col & 1)];
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
     * stripes; the bulk bg starts at col 2. Sample the brick-zone
     * top row (= attr-row 2) at col 14 — deep inside the brick band,
     * where the attr is reliably the level's bg colour. */
    unsigned char bg_attr = level_attrs[(int)level_idx * ATTR_BAND_SIZE
                                        + BRICK_ATTR_ROW_BASE * ATTR_COLS + 14];
    unsigned char cycle   = (unsigned char)(level_idx & 3);
    fill(0, 0, SCREEN_W, SCREEN_H, COL_BORDER);
    draw_frame(10);              /* bright red — placeholder */
    paint_hex_bg(bg_attr, cycle);
    render_brick_band(level_idx);
    render_bat(cycle, bg_attr);
    render_lives(cycle, bg_attr);
    render_frame(cycle, level_idx);
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

/* Repaint a horizontal strip of the playfield with the level's hex bg
 * tile + attr. Used to erase the bat's previous position before
 * redrawing at the new X. */
static void paint_bg_strip(unsigned char attr, unsigned char cycle,
                           int y0, int h) {
    unsigned char ink   = ink_pal(attr);
    unsigned char paper = paper_pal(attr);
    const unsigned char *tile = bg_tile + (int)cycle * BG_TILE_SIZE;
    int x, y, ty, byte_col, bit;
    unsigned char b;
    for (y = y0; y < y0 + h; y++) {
        ty = y & 15;
        for (byte_col = 0; byte_col < PLAYFIELD_W / 8; byte_col++) {
            b = tile[ty * 2 + (byte_col & 1)];
            for (bit = 0; bit < 8; bit++) {
                x = byte_col * 8 + bit;
                vga[(long)(BORDER_Y + y) * SCREEN_W + BORDER_X + x] =
                    (b & (0x80 >> bit)) ? ink : paper;
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

/* Paint a 4x4 ball at (x, y) playfield-relative in `colour`. */
static void render_ball(int x, int y, unsigned char colour) {
    int r, c;
    int x0 = BORDER_X + x;
    int y0 = BORDER_Y + y;
    for (r = 0; r < BALL_H_PX; r++) {
        for (c = 0; c < BALL_W_PX; c++) {
            vga[(long)(y0 + r) * SCREEN_W + x0 + c] = colour;
        }
    }
}

/* Brick band geometry: 12 rows * 8 px starting at y=32, 15 cols * 16 px
 * starting at x=8. Determines whether the ball's new center overlaps a
 * live brick and, if so, marks the brick destroyed and returns which
 * axis to reverse:
 *   0 = no hit
 *   1 = vertical hit (entered from top or bottom)  -> caller flips dy
 *   2 = horizontal hit (entered from a side)       -> caller flips dx
 * The previous ball position is needed to disambiguate corner cases. */
static int brick_collision(int prev_x, int prev_y, int new_x, int new_y) {
    int cx = new_x + BALL_W_PX / 2;
    int cy = new_y + BALL_H_PX / 2;
    int col, row, brick_top, brick_bot, prev_cy;
    unsigned char *cell;
    if (cy < 32 || cy >= 32 + LVL_ROWS * 8) return 0;
    if (cx < 8  || cx >= 8  + LVL_COLS * 16) return 0;
    col = (cx - 8) / 16;
    row = (cy - 32) / 8;
    cell = &live_level[row * LVL_COLS + col];
    if (*cell & 0x90) return 0;
    *cell |= 0x80;
    score += POINTS_PER_BRICK;
    brick_top = 32 + row * 8;
    brick_bot = brick_top + 8;
    prev_cy   = prev_y + BALL_H_PX / 2;
    (void)prev_x;
    if (prev_cy < brick_top || prev_cy >= brick_bot) return 1;  /* vertical */
    return 2;                                                    /* horizontal */
}

/* Count remaining destructible bricks (bit 7 clear, bit 4 clear).
 * Used to detect level-complete. */
static int live_bricks_remaining(void) {
    int i, n = 0;
    for (i = 0; i < LVL_CELLS; i++) {
        if (!(live_level[i] & 0x90)) n++;
    }
    return n;
}

/* Step the ball one frame: handle wall + bat collisions. If the ball
 * exits the bottom of the playfield it respawns stuck on the bat. */
static void step_ball(void) {
    int next_x, next_y;
    int bat_left  = bat_x;
    int bat_right = bat_x + BAT_W_BYTES * 8;     /* 40 px */
    int bat_top   = BAT_Y_PX;
    if (ball_stuck) {
        ball_x = bat_x + BALL_X_OFFSET_ON_BAT;
        ball_y = BAT_Y_PX - BALL_H_PX;
        return;
    }
    next_x = ball_x + ball_dx;
    next_y = ball_y + ball_dy;
    /* Side walls: flip dx, preserving the magnitude the bat-deflection
     * may have set (so a sharp angle survives wall bounces). */
    if (next_x < BALL_X_MIN) { next_x = BALL_X_MIN; ball_dx = -ball_dx; }
    else if (next_x > BALL_X_MAX) { next_x = BALL_X_MAX; ball_dx = -ball_dx; }
    if (next_y < BALL_Y_TOP) { next_y = BALL_Y_TOP; ball_dy = +BALL_SPEED; }
    /* Bat top: ball moving down, ball overlaps bat in X. Use a 5-zone
     * deflection so the ball gains horizontal control from where the
     * player intercepts it - the classic brick-breaker mechanic. */
    if (ball_dy > 0
        && next_y + BALL_H_PX >= bat_top
        && next_y < bat_top
        && next_x + BALL_W_PX > bat_left
        && next_x < bat_right) {
        int hit_x = (next_x + BALL_W_PX / 2) - bat_left;   /* 0..40 */
        next_y  = bat_top - BALL_H_PX;
        ball_dy = -BALL_SPEED;
        if      (hit_x <  8) ball_dx = -2;
        else if (hit_x < 16) ball_dx = -1;
        else if (hit_x < 24) ball_dx = (ball_dx >= 0) ? +1 : -1;
        else if (hit_x < 32) ball_dx = +1;
        else                 ball_dx = +2;
    }
    /* Past the bat (= lost ball). Decrement lives and respawn stuck
     * on the bat. The outer loop checks lives==0 to trigger game over. */
    if (next_y > BAT_Y_PX + BAT_H_PX) {
        if (lives > 0) lives--;
        ball_stuck = 1;
        ball_visible = 0;            /* hide ball until next SPACE */
        ball_x = bat_x + BALL_X_OFFSET_ON_BAT;
        ball_y = BAT_Y_PX - BALL_H_PX;
        ball_dx = +BALL_SPEED;
        ball_dy = -BALL_SPEED;
        return;
    }
    /* Brick collision: side-aware. brick_collision tells us which axis
     * the ball entered through; we reverse + unwind that axis. */
    {
        int hit = brick_collision(ball_x, ball_y, next_x, next_y);
        if (hit == 1)        { ball_dy = -ball_dy; next_y = ball_y; }
        else if (hit == 2)   { ball_dx = -ball_dx; next_x = ball_x; }
    }
    ball_x = next_x;
    ball_y = next_y;
}

/* M3 minimal play loop. For each level: full render once, then poll
 * arrows for bat motion (LEFT/RIGHT = +-4 px, matching the original's
 * handling_bat step). Any non-arrow key advances; ESC quits.
 *
 * In auto-advance mode (the default attract loop) the per-level
 * timeout still trips, so the cycle keeps moving even with no input.
 * Under BATTYALL=1 (test floppy) auto-advance is off and the test
 * orchestrator drives every transition via sendkey. */
/* Redraw the bat band only (y=167..185). Independent of bricks. */
static void redraw_bat(unsigned char cycle, unsigned char bg_attr) {
    paint_bg_strip(bg_attr, cycle, BAT_Y_PX, BAT_H_PX);
    render_bat(cycle, bg_attr);
    render_lives(cycle, bg_attr);
}

/* Redraw the whole level (frame, bg, bricks, bat, lives) and paint the
 * ball on top. Used when the ball is in motion - the cheapest correct
 * way to handle ball-over-brick passage without per-pixel bookkeeping. */
static void redraw_full_with_ball(unsigned char level_idx) {
    render_level_screen(level_idx);
    if (ball_visible) render_ball(ball_x, ball_y, 15);
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

/* Show a "GAME OVER" screen with the final score, hold ~3 seconds. */
static void render_game_over(void) {
    /* "GAME OVER" = G A M E (space) O V E R, glyph codes via
     * (letter - 'A' + 0x0A). Space is 0x26 per notes/encoding.md. */
    static const unsigned char go[]    = { 0x10, 0x0A, 0x16, 0x0E, 0x26,
                                           0x18, 0x1F, 0x0E, 0x1B };
    static const unsigned char sc_lbl[]= { 0x1C, 0x0C, 0x18, 0x1B, 0x0E,
                                           0x26 /* space */ };
    unsigned char digits[6];
    fill(0, 0, SCREEN_W, SCREEN_H, 0);
    /* "GAME OVER" - 9 chars at (12, 80) (= centred-ish at 320x200). */
    draw_text(BORDER_X + 4 * 8 + 4, BORDER_Y + 80, 15,
              go, (int)sizeof(go));
    /* "SCORE " followed by 6 digits. */
    score_to_codes(score, digits);
    draw_text(BORDER_X + 3 * 8, BORDER_Y + 100, 15,
              sc_lbl, (int)sizeof(sc_lbl));
    draw_text(BORDER_X + 3 * 8 + 6 * 8, BORDER_Y + 100, 15,
              digits, 6);
}

static state_t run_level(void) {
    unsigned char i;
    unsigned long start;
    unsigned long last_tick;
    unsigned char cycle;
    unsigned char bg_attr;

    /* New game: reset score + lives. The score/lives carry across
     * levels within one game; they reset only when re-entering
     * run_level from ST_HISCORE. */
    score = 0;
    lives = LIVES_INIT;

    for (i = 0; i < N_LEVELS; i++) {
        int k;
        bat_x         = BAT_X_INIT;
        bat_x_prev    = BAT_X_INIT;
        ball_stuck    = 1;
        ball_visible  = 0;
        ball_x        = BAT_X_INIT + BALL_X_OFFSET_ON_BAT;
        ball_y        = BAT_Y_PX - BALL_H_PX;
        ball_dx       = +BALL_SPEED;
        ball_dy       = -BALL_SPEED;
        for (k = 0; k < LVL_CELLS; k++) {
            live_level[k] = levels[(int)i * LVL_CELLS + k];
        }
        render_level_screen(i);
        cycle = (unsigned char)(i & 3);
        bg_attr = level_attrs[(int)i * ATTR_BAND_SIZE
                              + BRICK_ATTR_ROW_BASE * ATTR_COLS + 14];
        start     = bios_ticks();
        last_tick = start;
        for (;;) {
            unsigned long now;
            int ball_moved = 0;
            int bat_moved  = 0;

            if (kbhit()) {
                int k = getch();
                if (k == KEY_ESC) return ST_QUIT;
                if (k == KEY_EXT_PREFIX) {
                    int ext = getch();
                    if (ext == KEY_LEFT) {
                        if (bat_x > BAT_X_MIN) bat_x -= 4;
                    } else if (ext == KEY_RIGHT) {
                        if (bat_x < BAT_X_MAX) bat_x += 4;
                    }
                    start = bios_ticks();
                } else if (k == KEY_SPACE) {
                    ball_visible = 1;
                    ball_stuck   = 0;
                    start = bios_ticks();
                } else if (k == KEY_ENTER) {
                    break;
                }
            }

            /* Frame tick at ~18.2 Hz (BIOS) - M4 will swap to 50 Hz. */
            now = bios_ticks();
            if (now != last_tick) {
                last_tick = now;
                if (ball_visible && !ball_stuck) {
                    step_ball();
                    ball_moved = 1;
                }
            }

            if (bat_x != bat_x_prev) {
                bat_moved = 1;
                bat_x_prev = bat_x;
            }

            if (ball_moved) {
                redraw_full_with_ball(i);
            } else if (bat_moved) {
                redraw_bat(cycle, bg_attr);
                if (ball_visible && ball_stuck) {
                    ball_x = bat_x + BALL_X_OFFSET_ON_BAT;
                    render_ball(ball_x, ball_y, 15);
                }
            }

            /* End-of-life conditions. */
            if (lives == 0) {
                render_game_over();
                start = bios_ticks();
                while (!TIMED_OUT(start, 54UL))  /* ~3 sec at 18 Hz */ {
                    if (kbhit()) { getch(); break; }
                }
                return ST_TITLE;
            }
            if (live_bricks_remaining() == 0) break;  /* next level */

            if (auto_advance && TIMED_OUT(start, LEVEL_TIMEOUT_TICKS)) break;
        }
    }
    /* Cleared all 15 levels - show GAME OVER with final score then home. */
    render_game_over();
    {
        unsigned long t = bios_ticks();
        while (!TIMED_OUT(t, 54UL)) {
            if (kbhit()) { getch(); break; }
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
        load_level_attrs("LVLATTR.BIN") != 0 ||
        load_bg_tile("BGTILE.BIN") != 0 ||
        load_bat("BATL1.BIN") != 0 ||
        load_frame("FRAMEL1.BIN") != 0 ||
        load_lives("LIVESL1.BIN") != 0) {
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
