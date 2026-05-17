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

static const unsigned char briks_colors[10] = {
    0x00,                          /* [0] never indexed (low nibble == 0
                                    * means "skip" per the cell format). */
    0x57, 0x4F, 0x5F, 0x20, 0x70,  /* normal bricks */
    0x47, 0x57, 0x5F, 0x4F         /* metal bricks */
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
#define BALL_Y_TOP    24
#define BALL_X_MIN     8
#define BALL_X_MAX   240        /* 256 - 8 - body 8 */
/* Ball state - x/y now live in objects[OBJ_BALL_1].x_coord/y_coord
 * (the descriptor is the source of truth, mirroring the original's
 * IX-relative access). Sub-pixel motion (dx, dy) and the
 * stuck/visible flags stay as side state for now; full direction-
 * byte port from handling_ball is deferred. */
static int ball_dx     = +BALL_SPEED;
static int ball_dy     = -BALL_SPEED;
static unsigned char ball_stuck   = 1;
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
#define BULLET_W_PX     8
#define BULLET_H_PX     8
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

/* Brick destruction flash — a solid bright-white 16x8 rectangle
 * painted at the destroyed cell for a few ticks before the cell
 * fully disappears. Makes a brick popping feel less abrupt. We
 * only track the most-recent destruction; if multiple cells go
 * down in one tick (rocket sweep), the flash lands on the last
 * one. */
#define BRICK_FLASH_TICKS 4
static unsigned char brick_flash_ticks = 0;
static int           brick_flash_x     = 0;
static int           brick_flash_y     = 0;
static void render_brick_flash_to_buff(void);    /* forward decl — defined alongside brick_collision */

/* Rocket bonus animation — instead of insta-destroying every brick
 * on catch, fly a rocket up from the bat through the playfield and
 * destroy whatever it passes through. Two animation frames cycled
 * each tick. Deactivates when off the top of the playfield. */
#define ROCKET_W_PX     16
#define ROCKET_H_PX     14
#define ROCKET_SPEED    3
static unsigned char rocket_active = 0;
static int           rocket_x      = 0;
static int           rocket_y      = 0;
/* Stuck-on-bat dwell counter. While ball_stuck, the ball rides the
 * bat; SPACE detaches immediately; after STUCK_TIMEOUT ticks the ball
 * auto-launches. ~5 sec at 50 Hz. */
/* Mirror of ball.bonus_applied = $C0 at all_var_init's level entry: the
 * original counts down from 192 ticks (= 3.84 s at 50 Hz) before auto-
 * releasing a stuck ball. We were waiting ~25 % longer at 5 s. */
#define STUCK_TIMEOUT 192
static unsigned int stuck_ticks = 0;

/* Bomb state - port of bomb_appear at $A977. UFOs (and birds) drop a
 * single bomb that falls toward the bat. Mutually exclusive with a
 * regular bonus in the original since they share object_bonus; here
 * we keep separate side state. Bat collision = lose life like a
 * ball drop. */
static unsigned char bomb_active = 0;
static int           bomb_x = 0;
static int           bomb_y = 0;
/* Bomb shares handling_bonus's LA55A_0 accumulator in the original
 * (it's a bonus with sprite_num=\$0A), so it falls at the same ~2
 * px/frame peak as a regular bonus. */
#define BOMB_FALL_SPEED 2
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
/* Original handling_bonus drives Y via the LA55A_0 accumulator
 * (DE=\$0008, B=\$02) — accelerates ~8 frames to a peak of ~2 px/frame.
 * Constant 2 closely matches the bulk of that motion. Was 1, making
 * bonuses fall almost half as fast as the original. */
#define BONUS_FALL_SPEED  2
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
extern unsigned char round_number;
#define SLOW_DURATION     250     /* ~5 sec at 50 Hz */
/* BIG_BAT is permanent in the original — handling_bat_no_transform
 * keys off bat.bonus_applied == \$00, with no auto-expire. The bat
 * stays wide until another bonus is caught or the ball is lost.
 * Setting to UINT_MAX-ish so the timer-based expiration never fires;
 * big_bat_active() AND with bat.bonus_applied does the real gating. */
#define BIG_BAT_DURATION  0xFFFFu
#define BIG_BALL_DURATION 500
#define BAT_BIG_EXTRA_PX    8     /* width added on each side in big mode */
static int           bonus_x = 0;
static int           bonus_y = 0;
static unsigned char bonus_type   = 0;
static unsigned char bonus_active = 0;
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

/* "+400" floating-marker state spawned on bonus catch (port of
 * sprite_set $0B transition at $A6BA + handling_400pts at $A58D).
 * The original puts the marker in the same slot the bonus occupied;
 * we use side state for now since the bonus state is also side. */
#define PTS_400_DURATION  30          /* ~0.6 sec at 50 Hz */
static int           pts_400_x = 0;
static int           pts_400_y = 0;
static unsigned char pts_400_ticks = 0;
/* X drift per frame, mirror of the SMC at \$A590 in handling_400pts.
 * Original chooses from {-2, -1, +1, +2} based on random_number bits at
 * spawn (LA67B_3 area at \$3030-\$3038). Picked once per +400 spawn. */
static int           pts_400_dx = 0;
/* Sprite for the floating points marker. Defaults to the universal
 * "+400" reward; SCORE_5K bonus catches override to the larger
 * "+5000" sprite so the unusual reward has its own visible cue. */
static unsigned int  pts_marker_spr = 0;  /* set on catch */

/* Bonus colours indexed by type (ZX VGA palette indices). Picked so
 * they don't collide with the per-level bg cycle colours (yellow /
 * green / cyan / white). */
static const unsigned char bonus_colours[BONUS_TYPE_COUNT] = {
    10,   /* L (life)        - bright red     */
    11,   /* S (slow)        - bright magenta */
     9,   /* B (big bat)     - bright blue    */
    14,   /* G (big ball)    - bright yellow  */
    13,   /* K (kill aliens) - bright cyan    */
    12,   /* C (catch)       - bright green   */
    10,   /* R (rocket)      - bright red     */
    15,   /* $ (5000 points) - bright white   */
    11,   /* L (laser)       - bright magenta */
    13    /* 3 (multi-ball)  - bright cyan    */
};

/* Position of the rightmost dynamic life indicator; we paint
 * spr_lives_indicator (16x6 px sprite at $7AFC) here. */
#define LIVES_X_PX    24
#define LIVES_Y_PX    184

/* The original game's sprite block, extracted verbatim from the
 * program at $7A8C..$17E0 (offset 0x128c..0x17e0 within
 * 03_DATA_headless.dat.bin). Format per sprite:
 *   byte 0  -- width in bytes
 *   byte 1  -- height in rows
 *   then h rows of w (mask, pixel) pairs - blit semantics described
 *   in blit_masked_sprite below.
 * The constants below are offsets WITHIN sprites_blob. */
#define SPRITES_BLOB_SIZE 0x1274
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

static int load_sprites(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fread(sprites_blob, 1, sizeof(sprites_blob), f) != sizeof(sprites_blob)) {
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
    int rshift    = 8 - shift;
    int row, col_byte;
    for (row = 0; row < h; row++) {
        int y = y_px + row;
        if (y < 0 || y >= PLAYFIELD_H) { p += (unsigned)w * 2; continue; }
        for (col_byte = 0; col_byte < w; col_byte++) {
            unsigned char mask = *p++;
            unsigned char pix  = *p++;
            int dst_l = start_col + col_byte;
            int dst_r = dst_l + 1;
            unsigned char m_l, p_l, m_r, p_r;
            unsigned int row_base = (unsigned int)y * 32U;
            if (shift == 0) {
                m_l = mask; p_l = pix; m_r = 0; p_r = 0;
            } else {
                m_l = (unsigned char)(mask >> shift);
                p_l = (unsigned char)(pix  >> shift);
                m_r = (unsigned char)(mask << rshift);
                p_r = (unsigned char)(pix  << rshift);
            }
            if (dst_l >= 0 && dst_l < 32) {
                unsigned char *d = &scr_buff[row_base + dst_l];
                *d = (unsigned char)(((unsigned char)(m_l | *d)) ^ p_l);
            }
            if (shift != 0 && dst_r >= 0 && dst_r < 32) {
                unsigned char *d = &scr_buff[row_base + dst_r];
                *d = (unsigned char)(((unsigned char)(m_r | *d)) ^ p_r);
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
#define BAT_PREV_X  (objects[OBJ_BAT_1].prev_x)
#define BALL_X      (objects[OBJ_BALL_1].x_coord)
#define BALL_Y      (objects[OBJ_BALL_1].y_coord)
/* BIT 7 of sprite_set marks the object inactive in the original. We
 * use the same convention for "ball not yet released / hidden". */
#define BALL_VISIBLE     ((objects[OBJ_BALL_1].sprite_set & 0x80) == 0)
#define BALL_SHOW()      (objects[OBJ_BALL_1].sprite_set = 0x02)
#define BALL_HIDE()      (objects[OBJ_BALL_1].sprite_set = 0x82)

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

/* Birds/UFOs cross the playfield horizontally. Original handlers
 * ($A9BC handling_bird, $A958 handling_ufo) do flap animation +
 * bomb-drop subprocess; we simplify to straight-line motion that
 * leaves the descriptor's IX+$06 = dir byte intact for collision
 * checks the rest of the code can probe. */
static void bomb_appear(object_t *o);     /* forward decl */
static void handling_bird_obj(object_t *o) {
    /* Advance per IX+$07 = speed; cross from left edge to right when
     * dir LSB = 0, else right-to-left. misc_12 doubles as the frame
     * tick counter - the original keeps animation timing in $12/$13;
     * we use misc_12 as a single per-alien counter and pick frame
     * index = (misc_12 >> 2) mod 3 for a ~12 Hz wing flap at 50 Hz. */
    int dx = (o->dir & 1) ? -(int)o->speed : (int)o->speed;
    int nx = (int)o->x_coord + dx;
    o->misc_12++;
    o->sprite_num = (unsigned char)((o->misc_12 >> 2) % 3);
    bomb_appear(o);
    if (nx < 8 || nx >= PLAYFIELD_W - 8 - (int)o->w_body_px) {
        o->sprite_set |= 0x80;       /* off-screen: BIT 7 = inactive */
        return;
    }
    o->x_coord = (unsigned char)nx;
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
static void render_bat(unsigned char cycle, unsigned char attr) {
    /* Bat sprite layout (both spr_bat_normal and spr_bat_big):
     *   rows 0..9  - body (mask=1 = bat colour, pixel=1 = paper for
     *                internal texture).
     *   rows 10..12 - shadow drop (mask=0 dotted pattern with pix=1 -
     *                  the original OR-blit's (mask|screen)^pix flips
     *                  bg bits at those positions, producing the
     *                  textured shadow band below the bat).
     * One blit into scr_buff covers the whole sprite; we also force
     * the bg_attr into attr_buff for every cell the bat touches so
     * the bat stays bg-coloured even when it slides into the side
     * strip cells (whose attrs were set by paint_frame_to_buff). */
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
                int yy = BAT_Y_PX + 1 + row;
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
    y = BAT_Y_PX;
    /* Force bg attr on every cell the bat sprite covers (body + shadow
     * rows). Bat is 13 rows tall = 2 char cells vertically; reach can
     * extend into the side-strip cells when at extremes. */
    blit_sprite_attrs_to_buff(x - bat_extra_px, y,
                              sprite_w, 13, attr);
    blit_masked_to_scr_buff(spr, x, y);
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

static void render_running_dot(void) {
    int bat_w, bat_left;
    int frame, dir, span;
    if (bat_extra_px >= BAT_BIG_EXTRA_PX) {
        bat_w    = 32 + 2 * BAT_BIG_EXTRA_PX;   /* 48 px */
        bat_left = BAT_X - BAT_BIG_EXTRA_PX;
    } else {
        bat_w    = 32 + 2 * bat_extra_px;
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
#define LIVES_DYNAMIC_MAX 4
static void render_lives(unsigned char cycle, unsigned char attr) {
    int show = lives - 2;
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
static void render_brick_band(unsigned char level_idx) {
    int char_row, char_col;
    const unsigned char *cells = live_level;
    const unsigned char *lattr = &level_attrs[(int)level_idx * ATTR_BAND_SIZE];
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];

    if (level_idx >= N_LEVELS) return;

    /* Copy the per-level attrs into char rows 3..16 (the brick band,
     * including frame side strips and pre-dimmed shadow rows). */
    for (char_row = 3; char_row < 17; char_row++) {
        for (char_col = 0; char_col < 32; char_col++) {
            attr_buff[char_row * 32 + char_col] =
                lattr[char_row * ATTR_COLS + char_col];
        }
    }

    /* level_attrs.bin was captured with all bricks alive, so it carries
     * the brick colour in every brick cell. For cells whose brick is
     * destroyed (bit 7), reset the body attr to bg_attr — otherwise
     * destroyed bricks keep showing brick colour even though
     * print_briks_c skips the body pixels. Also clear the shadow row
     * (the char row below) when there's no brick below to keep its own
     * attr. */
    {
        int lvl_row, lvl_col;
        for (lvl_row = 0; lvl_row < LVL_ROWS; lvl_row++) {
            for (lvl_col = 0; lvl_col < LVL_COLS; lvl_col++) {
                unsigned char cell = cells[lvl_row * LVL_COLS + lvl_col];
                if (cell & 0x80) {
                    int cr  = 4 + lvl_row;
                    int cc1 = 1 + 2 * lvl_col;
                    int cc2 = cc1 + 1;
                    attr_buff[cr * 32 + cc1] = bg_attr;
                    attr_buff[cr * 32 + cc2] = bg_attr;
                    if (lvl_row + 1 < LVL_ROWS) {
                        unsigned char below = cells[(lvl_row + 1) * LVL_COLS + lvl_col];
                        if (below & 0x80) {
                            attr_buff[(cr + 1) * 32 + cc1] = bg_attr;
                            attr_buff[(cr + 1) * 32 + cc2] = bg_attr;
                        }
                    } else {
                        attr_buff[(cr + 1) * 32 + cc1] = bg_attr;
                        attr_buff[(cr + 1) * 32 + cc2] = bg_attr;
                    }
                }
            }
        }
    }

    print_briks_c(cells);
}

/* Pre-fill scr_buff with the hex tile and attr_buff uniformly with
 * the level's bg_attr, across the WHOLE 256x192 playfield. This
 * replaces the prior direct-to-VGA paint_hex_bg path. buff_to_vga
 * does the final pixel expansion using the (possibly overwritten)
 * attr_buff. */
static void paint_bg_to_buff(unsigned char attr, unsigned char cycle) {
    const unsigned char *tile = bg_tile + (int)cycle * BG_TILE_SIZE;
    int y, byte_col, char_row, char_col;
    for (y = 0; y < PLAYFIELD_H; y++) {
        int ty = y & 15;
        for (byte_col = 0; byte_col < 32; byte_col++) {
            scr_buff[y * 32 + byte_col] = tile[ty * 2 + (byte_col & 1)];
        }
    }
    for (char_row = 0; char_row < ATTR_ROWS; char_row++) {
        for (char_col = 0; char_col < ATTR_COLS; char_col++) {
            attr_buff[char_row * 32 + char_col] = attr;
        }
    }
}

/* Single-pass buffer-to-VGA conversion: walk scr_buff bits and emit
 * each pixel via the surrounding char cell's attr_buff entry. Mirrors
 * the original game's final-frame paint (game_screen_draw_to_buffer
 * at $BE6B followed by buffer-to-screen copy). */
static void buff_to_vga(void) {
    int y, byte_col, bit;
    for (y = 0; y < PLAYFIELD_H; y++) {
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

static void render_level_screen(unsigned char level_idx) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle   = (unsigned char)(level_idx & 3);
    fill(0, 0, SCREEN_W, SCREEN_H, COL_BORDER);
    draw_frame(10);              /* bright red — placeholder */
    paint_bg_to_buff(bg_attr, cycle);
    render_brick_band(level_idx);
    render_brick_flash_to_buff();
    /* Frame must paint AFTER bricks so its side-strip attrs override
     * the leftmost / rightmost brick's body attrs that print_briks_c
     * lays into the same cells; and BEFORE sprites so the bat / ball
     * OR-blit over the frame pixels (fixes "bat invisible at edges"). */
    paint_frame_to_buff(cycle, level_idx);
    render_bat(cycle, bg_attr);
    render_lives(cycle, bg_attr);
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

/* --- PC speaker sound (PIT channel 2 + port 0x61) ---------------------
 *
 * sound_play programs PIT counter 2 (input 1.193 MHz) to (freq) Hz and
 * gates the speaker via port 0x61 bits 0 (PIT2 gate) and 1 (speaker
 * enable). The 50 Hz PIT-0 IRQ we install earlier is on counter 0, so
 * we never collide.
 *
 * Tones are non-blocking: each call sets sound_end_tick = pit_frame_counter
 * + duration_ticks; sound_tick() (called from the per-frame body)
 * silences when due. A new sound_play before the previous tone expires
 * overrides it - the latest event wins, which matches how a fast
 * action game's audio normally feels. */
static unsigned long sound_end_tick = 0;

/* Pause flag: while set, the per-frame body does no physics; only P
 * (toggle), ESC (quit), and ENTER (advance) are responded to. */
static unsigned char paused = 0;

static void sound_silence(void) {
    _disable();
    outp(0x61, (unsigned char)(inp(0x61) & 0xFC));
    _enable();
    sound_end_tick = 0;
}

static void sound_play(unsigned int freq_hz, unsigned int duration_ticks) {
    unsigned int divisor;
    if (freq_hz < 20) return;             /* below audible / divisor too big */
    divisor = (unsigned int)(1193180UL / freq_hz);
    _disable();
    outp(0x43, 0xB6);                     /* counter 2, lo+hi, mode 3 */
    outp(0x42, (unsigned char)(divisor & 0xFF));
    outp(0x42, (unsigned char)((divisor >> 8) & 0xFF));
    outp(0x61, (unsigned char)(inp(0x61) | 0x03));
    _enable();
    sound_end_tick = pit_frame_counter + duration_ticks;
}

static void sound_tick(void) {
    if (sound_end_tick != 0 && pit_frame_counter >= sound_end_tick) {
        sound_silence();
    }
}

/* --- Sound queue (port of sounds_queue at $C0B8 + play_sounds_queue) ---
 *
 * 4 slots, each tracks a sound id + per-sound state byte. snd_q_push
 * adds an event; snd_q_tick is called from the 50 Hz frame body and
 * dispatches each active slot to its play_sound_<id> handler.
 * Single-shot sounds clear their slot on first tick; multi-frame
 * sounds (live-add ascending sweep, ball-launch / shot descending
 * sweep) advance state per frame and clear when exhausted.
 *
 * Frequencies derive from the original sound_beep's period:
 *   period = 26 * E T-states; freq = 3500000 / period = 134615 / E.
 * Each handler matches the E parameter ranges of the original's
 * play_sound_<event> routine (sound.asm at $C0F3+). */
#define SQ_SLOTS 4
typedef struct { unsigned char id; unsigned char state; } sound_slot_t;

/* Sound IDs match the original play_sounds_list at $C0BC. */
#define SND_NORMAL_BRIK   1
#define SND_BAT_BEAT      3
#define SND_BALL_START    4
#define SND_ALIEN_BLAST   6
#define SND_LIVE_ADD      7
#define SND_BAT_RESIZE_1  9
#define SND_TRIPLE_BALL   0x0A
#define SND_SHOT          0x0B
#define SND_BAT_RESIZE_2  0x0C

static sound_slot_t snd_q[SQ_SLOTS];

static void snd_q_push(unsigned char id) {
    int i;
    for (i = 0; i < SQ_SLOTS; i++) {
        if (snd_q[i].id == 0) {
            snd_q[i].id = id;
            switch (id) {
                case SND_LIVE_ADD:    snd_q[i].state = 0x20; break;
                case SND_BALL_START:  snd_q[i].state = 0x00; break;
                case SND_SHOT:        snd_q[i].state = 0x00; break;
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
            /* $C0F3: E=$44 -> ~1976 Hz, single 4 ms click. */
            sound_play(1976, 1);
            return 1;

        case SND_BAT_BEAT:
            /* $C16F: E=$66 -> ~1320 Hz. */
            sound_play(1320, 1);
            return 1;

        case SND_LIVE_ADD: {
            /* $C1CF: state starts $20, every 4th frame plays a beep
             * at E = state + $14 (ascending pitch as state shrinks).
             * state -= 2 per frame; cleared when 0. */
            if ((s->state & 3) == 0) {
                unsigned int e = (unsigned int)s->state + 0x14;
                if (e == 0) e = 1;
                sound_play(134615U / e, 1);
            }
            if (s->state == 0) return 1;
            s->state -= 2;
            return 0;
        }

        case SND_BALL_START: {
            /* $C116: C=9 cycles, E=$14 initially - play_sound_LC122
             * descends through the (C, E) pairs producing a quick
             * down-chirp. 9 frames at E rising 4 each frame. */
            unsigned int e = 0x14 + s->state * 4;
            sound_play(134615U / e, 1);
            if (s->state >= 8) return 1;
            s->state++;
            return 0;
        }

        case SND_SHOT: {
            /* $C235: C=4, E=$0F starting. 4-frame zip. */
            unsigned int e = 0x0F + s->state * 4;
            sound_play(134615U / e, 1);
            if (s->state >= 3) return 1;
            s->state++;
            return 0;
        }

        case SND_BAT_RESIZE_1: {
            /* $C200: state starts $C0 (from the bonus_resize push at
             * \$3212), decrements by $0B per frame until below $10. */
            unsigned int e = (unsigned int)s->state;
            if (e == 0) e = 1;
            sound_play(134615U / e, 1);
            if (s->state < 0x10 + 0x0B) return 1;
            s->state -= 0x0B;
            return 0;
        }

        case SND_TRIPLE_BALL: {
            /* $C21D: state starts $10 (from the LA67B_8 push at
             * \$3072), increments by $0B per frame until past $C0. */
            unsigned int e = (unsigned int)s->state;
            if (e == 0) e = 1;
            sound_play(134615U / e, 1);
            if (s->state >= 0xC1 - 0x0B) return 1;
            s->state += 0x0B;
            return 0;
        }

        case SND_ALIEN_BLAST: {
            /* $C1A8: state starts $30, each frame plays a noisy tone at
             * E = (random & $3F) + state with D=1; state += 8; wraps
             * from $60 to $21 once; stops at $A1. ~22 frames of zip-
             * style noise. */
            unsigned int e = ((unsigned int)next_random() & 0x3Fu) + (unsigned int)s->state;
            if (e == 0) e = 1;
            sound_play(134615U / e, 1);
            s->state = (unsigned char)(s->state + 8);
            if (s->state == 0x60) s->state = 0x21;
            if (s->state == 0xA1) return 1;
            return 0;
        }

        case SND_BAT_RESIZE_2: {
            /* $C247: one-shot fixed-pitch beep, D=$0A E=$30 — single
             * frame in our PIT-paced queue. Mirror of push_resize_sound
             * at $A645 (sound_bat_resize_2). */
            sound_play(2800, 2);
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

/* Restore a horizontal strip of scr_buff to the level's hex bg tile
 * and attr_buff (for the char rows the strip covers) to bg_attr.
 * Used to wipe a sprite's previous position before re-blitting. */
static void paint_bg_strip_to_buff(unsigned char attr, unsigned char cycle,
                                    int y0, int h) {
    const unsigned char *tile = bg_tile + (int)cycle * BG_TILE_SIZE;
    int y, byte_col, char_row;
    int y_end = y0 + h;
    if (y_end > PLAYFIELD_H) y_end = PLAYFIELD_H;
    for (y = y0; y < y_end; y++) {
        int ty = y & 15;
        for (byte_col = 0; byte_col < 32; byte_col++) {
            scr_buff[y * 32 + byte_col] = tile[ty * 2 + (byte_col & 1)];
        }
    }
    {
        int char_row_lo = y0 / 8;
        int char_row_hi = (y_end - 1) / 8;
        for (char_row = char_row_lo; char_row <= char_row_hi && char_row < ATTR_ROWS; char_row++) {
            for (byte_col = 0; byte_col < 32; byte_col++) {
                attr_buff[char_row * 32 + byte_col] = attr;
            }
        }
    }
}

/* Flush a horizontal strip of scr_buff/attr_buff to VGA — partial
 * version of buff_to_vga used after a paint_bg_strip_to_buff +
 * sprite blits to redraw just the affected band. */
static void buff_to_vga_strip(int y0, int h) {
    int y, byte_col, bit;
    int y_end = y0 + h;
    if (y_end > PLAYFIELD_H) y_end = PLAYFIELD_H;
    for (y = y0; y < y_end; y++) {
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

/* Paint each active laser bullet as a small 2x6 solid bar. Apply a
 * bright-magenta attr at each bullet's cells so they stand out against
 * bg pattern + bricks. Two slots = up to two bullets in flight. */
static void render_bullet_to_buff(void) {
    static unsigned char bullet_anim_tick = 0;
    unsigned int spr;
    unsigned char attr;
    int i;
    bullet_anim_tick++;
    spr = (bullet_anim_tick & 1) ? SPR_BULLET_2 : SPR_BULLET_1;
    attr = (unsigned char)(0x40 | (bonus_colours[BONUS_TYPE_LASER] & 7));
    for (i = 0; i < N_BULLETS; i++) {
        if (!bullet_active[i]) continue;
        blit_sprite_attrs_to_buff(bullet_x[i], bullet_y[i],
                                    BULLET_W_PX, BULLET_H_PX, attr);
        blit_masked_to_scr_buff(spr, bullet_x[i], bullet_y[i]);
    }
}

/* Paint the rocket into scr_buff. Alternates between the two
 * spr_bonus_rocket_* frames based on a per-tick counter for a
 * crude flame-flicker effect. Attr forced to LASER's bright
 * magenta so the rocket stands out against any bg cell. */
static void render_rocket_to_buff(void) {
    static unsigned char rocket_anim_tick = 0;
    unsigned int spr;
    unsigned char attr;
    if (!rocket_active) return;
    rocket_anim_tick++;
    /* Original handling_rocket at \$A89A toggles sprite each frame:
     *   LD A,(counter_misc); AND \$01; LD (IX+\$01),A
     * Was masking \& 2 which only flipped every 2 ticks — half the
     * original's flame flicker rate. */
    spr = (rocket_anim_tick & 1) ? SPR_BONUS_ROCKET_2 : SPR_BONUS_ROCKET_1;
    attr = (unsigned char)(0x40 | (bonus_colours[BONUS_TYPE_ROCKET] & 7));
    blit_sprite_attrs_to_buff(rocket_x, rocket_y, ROCKET_W_PX, ROCKET_H_PX, attr);
    blit_masked_to_scr_buff(spr, rocket_x, rocket_y);
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

/* Paint the bonus into scr_buff + attr_buff. The bonus sprite is
 * actually 24 px wide x 13 rows tall (3 bytes x 13 rows) including
 * a trailing drop-shadow band — earlier code used BONUS_W_PX=8 /
 * BONUS_H_PX=6 for the attr override which only covered ~1/3 of the
 * sprite, leaving the outer body pixels rendering with bg_attr (=
 * invisible yellow on yellow). Read the actual sprite header for
 * the attr-write extent.
 *
 * Don't clear scr_buff under the bonus: that produced a solid-black
 * 16x16 backdrop that was MORE jarring than the natural ZX colour
 * clash. With the bg pattern preserved and the cell attr set to
 * (bright | bonus_ink | bg_paper), bg-pattern clear bits stay in the
 * bg's paper colour (matches surrounding cells) and only the set
 * bits + bonus body recolour to bonus_ink — exactly the artefact
 * the original game produces. */
static void render_bonus_to_buff(unsigned char bg) {
    unsigned int spr = spr_for_bonus(bonus_type);
    unsigned char idx = (bonus_type < BONUS_TYPE_COUNT)
                        ? bonus_type : BONUS_TYPE_LIFE;
    unsigned char col = bonus_colours[idx];
    unsigned char attr = (unsigned char)(0x40
                                         | (col & 7)
                                         | (bg & 0x38));
    int spr_w_px = sprites_blob[spr]     * 8;
    int spr_h_px = sprites_blob[spr + 1];
    blit_sprite_attrs_to_buff(bonus_x, bonus_y, spr_w_px, spr_h_px, attr);
    blit_masked_to_scr_buff(spr, bonus_x, bonus_y);
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
                /* Original get_rocket at $AA9D:
                 *   rocket_x = bat_x + 4 (normal) or +12 (big)
                 *   rocket_y = bat_y + 6 (inside the bat body)
                 * Both put the rocket on the bat's left half emerging
                 * up from inside it. Our sprite is masked so the bat
                 * pixels stay visible through the transparent regions. */
                rocket_x = BAT_X + 4;
                if (bat_extra_px >= BAT_BIG_EXTRA_PX) rocket_x += 8;
                rocket_y = BAT_Y_PX + 6;
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
             * $A67B). Directions fan out: ball2 mirrors ball1's dx,
             * ball3 goes straight up with the opposite of that. */
            /* Original at LA67B_8 (\$3074): `LD (IY+\$14),\$FF` after
             * setting balls_quantity = 3 — overwrites bat.bonus_applied
             * to \$FF (TRIPLE_BALL is ball-side, not bat-side). */
            objects[OBJ_BAT_1].bonus_applied = 0xFF;
            objects[OBJ_BAT_2].bonus_applied = 0xFF;
            if (!ball2_active && !ball3_active) {
                ball2_active = 1;
                objects[OBJ_BALL_2].sprite_set = 0x02;
                objects[OBJ_BALL_2].x_coord = BALL_X;
                objects[OBJ_BALL_2].y_coord = BALL_Y;
                /* Original at LA67B_8 / \$3100 picks ball2 + ball3
                 * directions from a 3-way split based on ball1's
                 * 6-bit direction & \$0F. In our integer dx/dy the
                 * three balls fan: ball1 keeps its trajectory, ball2
                 * mirrors horizontally, ball3 takes a half-magnitude
                 * mirror so it travels a different angle from both. */
                ball2_dx = -ball_dx;       /* mirror primary */
                ball2_dy = -BALL_SPEED;    /* upward */
                ball3_active = 1;
                objects[OBJ_BALL_3].sprite_set = 0x02;
                objects[OBJ_BALL_3].x_coord = BALL_X;
                objects[OBJ_BALL_3].y_coord = BALL_Y;
                /* Half-magnitude same-sign so ball3 fans between
                 * ball1 (e.g. +2,-2) and ball2 (-2,-2): goes shallow
                 * in ball1's direction, less steep upward. */
                ball3_dx = (ball_dx > 0) ? +1 : -1;
                ball3_dy = -BALL_SPEED;
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
static int eff_ball_size(void) { return big_ball_active() ? 12 : BALL_W_PX; }

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
    if (big_ball_ticks > 0) {
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
    bonus_y += BONUS_FALL_SPEED;
    bat_left  = eff_bat_left();
    bat_right = eff_bat_right();
    /* Catch test uses bat body (10 px) not full sprite (13 px =
     * body + shadow). obj_compare_2pix at \$94BC reads (IY+\$0C, IY+\$0D)
     * which are body dimensions on object_bat_1. Shadow rows aren't
     * a catch surface. */
    if (bonus_y + BONUS_H_PX >= BAT_Y_PX
        && bonus_y < BAT_Y_PX + 10
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
        pts_400_ticks = PTS_400_DURATION;
        /* Pick X drift in {-2, -1, +1, +2} — port of \$3030's
         * `AND \$01 / INC A / RL B / JR C / NEG` sequence:
         *   bit 0 of random → +1 or +2 magnitude
         *   bit 7 of random → keep positive or negate */
        {
            unsigned int r = next_random();
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
    if (pts_400_ticks == 0) return;
    if ((pts_400_ticks & 1) == 0) pts_400_y++;
    /* Apply the X drift each frame (port of LA590's ADD A,SMC). Clamp
     * to playfield via the original's check_left/right_margin pattern. */
    pts_400_x += pts_400_dx;
    if (pts_400_x < 8) pts_400_x = 8;
    if (pts_400_x > PLAYFIELD_W - 16) pts_400_x = PLAYFIELD_W - 16;
    if (pts_400_y >= PLAYFIELD_H) { pts_400_ticks = 0; return; }
    pts_400_ticks--;
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
    if ((next_random() & 0x0F) >= 5) return;
    tbl = (round_number >= 6) ? bonus_table_second : bonus_table_first;
    for (tries = 0; tries < 16; tries++) {
        unsigned char idx = (unsigned char)(next_random() & 0x0F);
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
         * The round-6+ extra-rare-rocket re-roll (random & $C0 != 0)
         * is dropped here for simplicity. */
        if (code == 0x02 && (ball2_active || ball3_active)) continue;
        if (code == 0x04 && slow_ticks > 0) continue;
        if (code == 0x05 && life_dropped_this_round) continue;
        if (code == 0x06 && rocket_active) continue;
        mapped = map_orig_to_our_bonus(code);
        if (mapped != BONUS_TYPE_UNSUPPORTED) {
            bonus_active = 1;
            bonus_x = 8 + col * 16 + (16 - BONUS_W_PX) / 2;
            bonus_y = 32 + row * 8;
            bonus_type = mapped;
            return;
        }
    }
}

/* Spawn the brick destruction flash at a level-grid cell. Replaces
 * a popping brick with a bright-white solid rectangle for
 * BRICK_FLASH_TICKS ticks. Only one flash visible at a time. */
static void brick_flash_spawn(int col, int row) {
    brick_flash_x = 8 + col * 16;
    brick_flash_y = 32 + row * 8;
    brick_flash_ticks = BRICK_FLASH_TICKS;
}

static void step_brick_flash(void) {
    if (brick_flash_ticks) brick_flash_ticks--;
}

/* Paint the flash: scr_buff bytes set solid for 7 rows (matching
 * the original spr_brik_5 pattern: 0xFF/0xFE per row + 1 blank
 * trailing row); attr_buff at the brick's two char cells overridden
 * to bright white. Runs after render_brick_band so it sits on top
 * of the bg pattern that would otherwise show through a destroyed
 * cell. */
static void render_brick_flash_to_buff(void) {
    int col_byte, char_row, r;
    if (!brick_flash_ticks) return;
    col_byte = brick_flash_x >> 3;       /* brick spans col_byte and col_byte+1 */
    char_row = brick_flash_y >> 3;
    if (col_byte < 0 || col_byte + 1 >= 32) return;
    if (char_row < 0 || char_row >= ATTR_ROWS) return;
    for (r = 0; r < 7; r++) {
        int y = brick_flash_y + r;
        if (y < 0 || y >= PLAYFIELD_H) continue;
        scr_buff[y * 32 + col_byte]     = 0xFF;
        scr_buff[y * 32 + col_byte + 1] = 0xFE;
    }
    /* Last row left blank for the inter-brick gap, matching the
     * destroyed brick's footprint exactly. */
    attr_buff[char_row * 32 + col_byte]     = 0x47;
    attr_buff[char_row * 32 + col_byte + 1] = 0x47;
}

static int brick_collision(int prev_x, int prev_y, int new_x, int new_y) {
    int sz = eff_ball_size();
    int cx = new_x + sz / 2;
    int cy = new_y + sz / 2;
    int col, row, brick_top, brick_bot, prev_cy, axis;
    unsigned char *cell;
    if (cy < 32 || cy >= 32 + LVL_ROWS * 8) return 0;
    if (cx < 8  || cx >= 8  + LVL_COLS * 16) return 0;
    col = (cx - 8) / 16;
    row = (cy - 32) / 8;
    cell = &live_level[row * LVL_COLS + col];
    /* BIT 7 = no brick / destroyed: ball passes through. */
    if (*cell & 0x80) return 0;

    /* Determine the bounce axis (1 = flip dy, 2 = flip dx) for both
     * the destructible and undestructible paths. */
    brick_top = 32 + row * 8;
    brick_bot = brick_top + 8;
    prev_cy   = prev_y + sz / 2;
    (void)prev_x;
    axis = (prev_cy < brick_top || prev_cy >= brick_bot) ? 1 : 2;

    /* BIT 5 = undestructible: bounce, never destroy.
     * BIT 4 = "this hit destroys" (1-hit brick OR multi-hit's final
     *          hit registered by an earlier collision).
     * Otherwise (bit 4 + bit 5 both clear) = multi-hit brick: this is
     *          the FIRST collision, so SET BIT 4 and bounce; the next
     *          hit will hit the BIT 4 branch above and destroy. */
    if (*cell & 0x20) {
        /* Undestructible: bounce off with the same brick-tick sound
         * as a destruction (the original plays SND_NORMAL_BRIK on
         * every brick collision regardless of survival). */
        snd_q_push(SND_NORMAL_BRIK);
        return axis;
    }
    /* SMASH (BIG_BALL) bypasses the multi-hit half-state — port of
     * LAFFC's `CP \$07; JR Z,LAFFC_38` test that jumps directly to
     * the destroy path. Without this, multi-hit bricks still need
     * two hits even with SMASH active. */
    if (!big_ball_active() && !(*cell & 0x10)) {
        *cell |= 0x10;
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
    snd_q_push(SND_NORMAL_BRIK);            /* brick-break click */
    /* BIG_BALL (smash) bonus: ball ploughs through bricks rather
     * than bouncing — keep the bonus-spawn check below intact but
     * stash the "no bounce" intent. */
    if (big_ball_active()) axis = 0;
    brick_flash_spawn(col, row);
    try_spawn_bonus(col, row);
    return axis;
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
static unsigned int rng_state = 0xACE1u;
static unsigned int next_random(void) {
    rng_state = (unsigned int)(rng_state * 25173u + 13849u);
    return rng_state;
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
    r = (unsigned char)(next_random() & 3);
    e->x_coord = prop_x_coord[r];
    /* If x starts on the right, set dir BIT 0 so handling_bird_obj
     * moves left. The original uses dir=$10 as initial; we encode the
     * direction in BIT 0 directly to keep our simplified handler
     * trivial. */
    e->dir = (r & 1) ? 0x11 : 0x10;
    e->bonus_applied = 0x10;
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
    by_t = BAT_Y_PX;
    by_b = BAT_Y_PX + 10;             /* h_body_px = \$0A per object_bat_1 init */
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

/* Port of bomb_appear at $A977 - called per alien tick. Probability
 * (random + random+1) & $3F == 0 = ~1/64 chance per call. Bomb
 * shares the bonus slot in the original; we keep separate state. */
static void bomb_appear(object_t *o) {
    unsigned int r;
    if (bomb_active) return;
    if (bonus_active) return;
    r = next_random();
    if ((((r >> 8) ^ (r & 0xFF)) & 0x3F) != 0) return;
    /* Only spawn while alien still in upper half (y < $C0 = 192). */
    if (o->y_coord + 8 >= 0xC0) return;
    bomb_active = 1;
    bomb_x = (int)o->x_coord + 8;
    bomb_y = (int)o->y_coord + 8;
}

/* Step the bomb each frame: fall, check bat collision, deactivate
 * past the bottom. Bat hit costs a life and respawns the ball. */
static void step_bomb(void) {
    int bx_l, bx_r, by_t, by_b;
    if (!bomb_active) return;
    bomb_y += BOMB_FALL_SPEED;
    /* Check bat collision (8 x 12 bomb rect vs bat top 8-px band). */
    bx_l = bomb_x; bx_r = bomb_x + BOMB_W_PX;
    by_t = bomb_y + BOMB_H_PX - 4; by_b = bomb_y + BOMB_H_PX;
    if (by_b >= BAT_Y_PX && by_t < BAT_Y_PX + 10
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
        respawn_primary_ball();
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
        if (bullet_x[b] + BULLET_W_PX > ex_l && bullet_x[b] < ex_r
            && bullet_y[b] + BULLET_H_PX > ey_t && bullet_y[b] < ey_b) {
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
            } else if (!(*cell & 0x10)) {
                *cell |= 0x10;                 /* multi-hit, set bit 4 */
                hit = 1;
            } else {
                unsigned int idx = (unsigned int)((row < 12) ? row : 11);
                unsigned int pts = points_table[idx];
                if ((*cell & 0x0F) >= 6) pts *= 2;
                score += pts;
                *cell |= 0x80;
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
    unsigned char attr = (unsigned char)(0x40 | (bonus_colours[BONUS_TYPE_LASER] & 7));
    int i;
    for (i = 0; i < N_BULLETS; i++) {
        unsigned char frame;
        if (!bullet_blast_ticks[i]) continue;
        frame = (unsigned char)((bullet_blast_ticks[i] - 1) / BULLET_BLAST_TICKS_PER_FRAME);
        if (frame >= BULLET_BLAST_FRAMES) frame = BULLET_BLAST_FRAMES - 1;
        frame = (unsigned char)((BULLET_BLAST_FRAMES - 1) - frame);
        blit_sprite_attrs_to_buff(bullet_blast_x[i], bullet_blast_y[i], 8, 8, attr);
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
        }
    }
}

static void step_rocket(void) {
    int col_lo, col_hi, row_lo, row_hi, r, c;
    int killed_this_tick = 0;
    if (!rocket_active) return;
    rocket_y -= ROCKET_SPEED;
    if (rocket_y + ROCKET_H_PX < 0) {
        rocket_active = 0;
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

/* Step the ball one frame: handle wall + bat collisions. If the ball
 * exits the bottom of the playfield it respawns stuck on the bat. */
static void step_ball(void) {
    int next_x, next_y;
    int bat_left  = eff_bat_left();
    int bat_right = eff_bat_right();
    int bat_top   = BAT_Y_PX;
    int ball_sz   = eff_ball_size();
    if (ball_stuck) {
        BALL_X = BAT_X + stuck_offset_x;
        BALL_Y = BAT_Y_PX - ball_sz;
        return;
    }
    next_x = BALL_X + ball_dx;
    next_y = BALL_Y + ball_dy;
    /* Side walls: flip dx, preserving the magnitude the bat-deflection
     * may have set (so a sharp angle survives wall bounces). */
    {
        int x_max = PLAYFIELD_W - 8 - ball_sz;   /* 244 normal, 240 big */
        if (next_x < BALL_X_MIN)        { next_x = BALL_X_MIN; ball_dx = -ball_dx; }
        else if (next_x > x_max)        { next_x = x_max;      ball_dx = -ball_dx; }
    }
    if (next_y < BALL_Y_TOP) { next_y = BALL_Y_TOP; ball_dy = +BALL_SPEED; }
    /* Bat top: ball moving down, ball overlaps bat in X. Use a 5-zone
     * deflection so the ball gains horizontal control from where the
     * player intercepts it - the classic brick-breaker mechanic. */
    if (ball_dy > 0
        && next_y + ball_sz >= bat_top
        && next_y < bat_top
        && next_x + ball_sz > bat_left
        && next_x < bat_right) {
        int hit_x = (next_x + ball_sz / 2) - bat_left;
        int span  = bat_right - bat_left;
        next_y  = bat_top - ball_sz;
        /* If the bat is carrying the CATCH bonus (matches the original's
         * BAT+$14 = $03), the ball sticks on contact and waits for SPACE
         * to release. Otherwise bounce with 5-zone deflection. */
        if (objects[OBJ_BAT_1].bonus_applied == 0x03) {
            ball_stuck      = 1;
            stuck_ticks     = 0;
            ball_dy         = -BALL_SPEED;
            /* Record the offset where the ball hit so the stuck-ball
             * tracker keeps it at the catch position as the bat slides. */
            stuck_offset_x  = next_x - BAT_X;
            BALL_X          = next_x;
            BALL_Y          = next_y;
            snd_q_push(SND_BAT_BEAT);
            return;
        }
        ball_dy = -BALL_SPEED;
        /* Same 5-zone split, normalised to the (possibly-bigger) bat span. */
        if      (hit_x * 5 < span * 1) ball_dx = -2;
        else if (hit_x * 5 < span * 2) ball_dx = -1;
        else if (hit_x * 5 < span * 3) ball_dx = (ball_dx >= 0) ? +1 : -1;
        else if (hit_x * 5 < span * 4) ball_dx = +1;
        else                           ball_dx = +2;
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
        respawn_primary_ball();
        return;
    }
    /* Brick collision: side-aware. brick_collision tells us which axis
     * the ball entered through; we reverse + unwind that axis. */
    {
        int hit = brick_collision(BALL_X, BALL_Y, next_x, next_y);
        if (hit == 1)        { ball_dy = -ball_dy; next_y = BALL_Y; }
        else if (hit == 2)   { ball_dx = -ball_dx; next_x = BALL_X; }
    }
    BALL_X = next_x;
    BALL_Y = next_y;
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
    int bat_top   = BAT_Y_PX;
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
    /* Bat bounce: same 5-zone deflection as the primary ball. */
    if (dy > 0
        && next_y + ball_sz >= bat_top
        && next_y < bat_top
        && next_x + ball_sz > bat_left
        && next_x < bat_right) {
        int hit_x = (next_x + ball_sz / 2) - bat_left;
        int span  = bat_right - bat_left;
        next_y  = bat_top - ball_sz;
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
/* Redraw the bat band only (y=167..185). Independent of bricks.
 * Re-fills the strip's scr_buff/attr_buff with bg, blits bat + lives
 * via the scr_buff pipeline, then flushes the strip to VGA. */
static void redraw_bat(unsigned char cycle, unsigned char bg_attr) {
    /* Note: paint_frame_to_buff writes outside this strip too, but
     * buff_to_vga_strip only flushes BAT_Y_PX..BAT_H_PX so the off-
     * strip side-strip pixels just sit in scr_buff until the next
     * full redraw. Inside the strip, the frame side cells are
     * restored before the bat OR-blit lands on them. */
    paint_bg_strip_to_buff(bg_attr, cycle, BAT_Y_PX, BAT_H_PX);
    paint_frame_to_buff(cycle, current_level_idx_var);
    render_bat(cycle, bg_attr);
    render_running_dot();
    render_lives(cycle, bg_attr);
    buff_to_vga_strip(BAT_Y_PX, BAT_H_PX);
}

static void render_hud_score(void);
static void render_hud_high_score(void);
static void render_hud_powerups(void);

/* Full-frame compose. Walks the same scr_buff -> attr_buff -> VGA
 * path as the original (game_screen_draw_to_buffer @ $BE6B):
 *   - paint bg + bricks + bat + lives into scr_buff/attr_buff
 *   - paint ball, bomb, 400pts, alien into scr_buff (each picks up
 *     its surrounding char cell's bg attr at buff_to_vga time)
 *   - single buff_to_vga pass converts everything to VGA
 *   - frame ornament + HUD text painted direct-VGA on top
 *   - falling bonus painted direct-VGA (uses per-type colour, not
 *     bg-attr — original game writes specific attrs into attr_buff
 *     for the bonus's two cells; we approximate with a coloured blit). */
static void redraw_full_with_ball(unsigned char level_idx) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle   = (unsigned char)(level_idx & 3);
    object_t *enemy = &objects[OBJ_ENEMY];

    fill(0, 0, SCREEN_W, SCREEN_H, COL_BORDER);
    draw_frame(10);              /* bright red — placeholder */
    paint_bg_to_buff(bg_attr, cycle);
    render_brick_band(level_idx);
    render_brick_flash_to_buff();
    paint_frame_to_buff(cycle, level_idx);
    render_bat(cycle, bg_attr);
    render_running_dot();
    render_lives(cycle, bg_attr);
    if (BALL_VISIBLE) render_ball_to_buff(BALL_X, BALL_Y, bg_attr);
    if (bomb_active) {
        blit_masked_to_scr_buff_ptr(spr_bomb_data, bomb_x, bomb_y);
    }
    if (pts_400_ticks > 0) {
        blit_masked_to_scr_buff(pts_marker_spr, pts_400_x, pts_400_y);
    }
    if ((enemy->sprite_set & 0x7F) != 0 && !(enemy->sprite_set & 0x80)) {
        unsigned int spr;
        int spr_w_px, spr_h_px;
        if ((enemy->sprite_set & 0x7F) == 0x0A) {
            unsigned char frame = enemy->sprite_num;
            if (frame >= BLAST_FRAMES) frame = BLAST_FRAMES - 1;
            spr = spr_blast_frames[frame];
        } else {
            /* Alien is either bird (\$09) or UFO (\$08) — the only
             * other types enemy_prepare ever sets. L4-spark slot
             * type \$07 was an earlier port-only feature; the
             * original's \$9EAA returns early for L4. */
            unsigned char frame = enemy->sprite_num % 3;
            spr = (enemy->sprite_set == 0x09)
                ? spr_bird_frames[frame]
                : spr_ufo_frames[frame];
        }
        /* Force a contrast attr in the enemy's char cells so it
         * shows over the brick row's attrs (otherwise it would
         * inherit red / cyan / etc brick colours and visually
         * disappear). */
        spr_w_px = sprites_blob[spr]     * 8;
        spr_h_px = sprites_blob[spr + 1];
        blit_sprite_attrs_to_buff(enemy->x_coord, enemy->y_coord,
                                   spr_w_px, spr_h_px, bg_attr);
        blit_masked_to_scr_buff(spr, enemy->x_coord, enemy->y_coord);
    }
    if (bonus_active) render_bonus_to_buff(bg_attr);
    render_bullet_to_buff();
    if (any_bullet_blast()) render_bullet_blast_to_buff();
    if (rocket_active) render_rocket_to_buff();
    if (ball2_active) {
        render_ball_to_buff(objects[OBJ_BALL_2].x_coord,
                            objects[OBJ_BALL_2].y_coord, bg_attr);
    }
    if (ball3_active) {
        render_ball_to_buff(objects[OBJ_BALL_3].x_coord,
                            objects[OBJ_BALL_3].y_coord, bg_attr);
    }
    buff_to_vga();
    render_hud_score();
    render_hud_high_score();
    render_hud_powerups();
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

/* Position of the live score in the empty band between the brick zone
 * (y <= 127) and the bat (y >= 167). 6 digits * 8 px = 48 px wide,
 * centred at playfield x. Only drawn once score > 0 so state4_level1
 * (captured at score=0) stays pixel-identical against the GT. */
/* Live 1UP score in the top HUD strip, overlaying the frame's
 * baked-in placeholder. Char row 1 (y=8), char col 4 (x=32).
 * 6 digits * 8 px = 48 px wide. */
#define HUD_SCORE_X (BORDER_X + 32)
#define HUD_SCORE_Y (BORDER_Y + 8)
/* Hi-score also in the top HUD strip, centre region — char col 15
 * (x=120), same row. */
#define HUD_HISCORE_X (BORDER_X + 120)
#define HUD_HISCORE_Y (BORDER_Y + 8)
/* Power-up letter chips sit in the empty band between the brick
 * field (y <= 127) and the bat (y >= 167), away from the top HUD. */
#define HUD_POWERUP_X (BORDER_X + 192)
#define HUD_POWERUP_Y (BORDER_Y + 140)
static void render_hud_score(void) {
    unsigned char digits[6];
    /* score=0 leaves the frame ornament's baked-in "000000" visible —
     * that pattern is part of the captured frame_l1.bin and aligns
     * with the original. Once the player scores, our digits overlay
     * with the matching colour (idx 6 = non-bright yellow). */
    if (score == 0) return;
    score_to_codes(score, digits);
    draw_text(HUD_SCORE_X, HUD_SCORE_Y, 6, digits, 6);
}
static void render_hud_high_score(void) {
    unsigned char digits[6];
    /* Mirror render_hud_score: leave the frame's baked placeholder
     * showing when no real high score has been recorded yet. Once
     * the player beats the placeholder threshold, the dynamic value
     * overlays. */
    if (high_score == 0) return;
    score_to_codes(high_score, digits);
    draw_text(HUD_HISCORE_X, HUD_HISCORE_Y, 6, digits, 6);
}
/* Letter chips for the active power-up effects, painted in the bonus
 * colour so the player can see at a glance what's running. Encoded
 * letter codes (notes/encoding.md): B=0x0B, C=0x0C, G=0x10, K=0x14,
 * S=0x1C. KILL_ALIENS + CATCH are lasting bat-attached effects keyed
 * off OBJ_BAT_1.bonus_applied (matches the original's BAT+$14 = $09
 * / $03). */
static void render_hud_powerups(void) {
    int x = HUD_POWERUP_X;
    /* Blink the chip in its final second of life (= ticks < 50 at
     * 50 Hz). On the off-phase we just skip the draw entirely so the
     * letter flickers — a "your bonus is about to expire" cue. */
#define HUD_CHIP_BLINK_THRESHOLD 50
    int blink_off = (blink_phase() == 0);
    if (slow_ticks > 0
        && !(slow_ticks < HUD_CHIP_BLINK_THRESHOLD && blink_off)) {
        draw_glyph(x, HUD_POWERUP_Y, bonus_colours[BONUS_TYPE_SLOW], 0x1C);
    }
    if (slow_ticks > 0) x += 10;
    if (big_bat_active()
        && !(big_bat_ticks < HUD_CHIP_BLINK_THRESHOLD && blink_off)) {
        draw_glyph(x, HUD_POWERUP_Y, bonus_colours[BONUS_TYPE_BIG_BAT], 0x0B);
    }
    if (big_bat_active()) x += 10;
    if (big_ball_active()
        && !(big_ball_ticks < HUD_CHIP_BLINK_THRESHOLD && blink_off)) {
        draw_glyph(x, HUD_POWERUP_Y, bonus_colours[BONUS_TYPE_BIG_BALL], 0x10);
    }
    if (big_ball_active()) x += 10;
    if (objects[OBJ_BAT_1].bonus_applied == 0x09) {
        draw_glyph(x, HUD_POWERUP_Y, bonus_colours[BONUS_TYPE_KILL_ALIENS], 0x14);
        x += 10;
    }
    if (objects[OBJ_BAT_1].bonus_applied == 0x03) {
        draw_glyph(x, HUD_POWERUP_Y, bonus_colours[BONUS_TYPE_CATCH], 0x0C);
        x += 10;
    }
    if (objects[OBJ_BAT_1].bonus_applied == 0x01) {
        /* L (laser), letter code 0x15 per notes/encoding.md. */
        draw_glyph(x, HUD_POWERUP_Y, bonus_colours[BONUS_TYPE_LASER], 0x15);
        x += 10;
    }
    if (ball2_active) {
        /* Digit 3 (multi-ball) — digits use code = digit value. */
        draw_glyph(x, HUD_POWERUP_Y, bonus_colours[BONUS_TYPE_MULTI_BALL], 0x03);
    }
}

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
    for (step = 0; step < 8; step++) {
        unsigned long t;
        unsigned char frame = brik_anim_order[step];
        brik_anim_apply_frame(frame);
        buff_to_vga_strip(32, 96);
        if (frame == 4) sound_play(2600, 2);    /* spr_brik_5 ping */
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

/* "ROUND XX" intro banner — port of show_window_round_number at $8F60
 * + the pause_long B=4 wait at LB9E8_1. Draws an 80x24 black panel
 * centred between the brick zone and the bat, with "ROUND<sp>NN" in
 * bright white. Holds for ~1.2 s (60 PIT ticks) or until a key is
 * pressed. ESC during the wait returns 1 so the caller can quit.
 *
 * We skip the original's secondary "PLAYER X" line because the port
 * is single-player only for now; once 2-player wiring lands we add it. */
static int show_round_banner(unsigned int round_num_display) {
    int round_num = (int)round_num_display;
    int banner_x = BORDER_X + 88;
    int banner_y = BORDER_Y + 143;
    int text_x   = BORDER_X + 96;
    int text_y   = BORDER_Y + 151;
    unsigned long start;
    unsigned char codes[8];
    codes[0] = 0x1B;  /* R */
    codes[1] = 0x18;  /* O */
    codes[2] = 0x1E;  /* U */
    codes[3] = 0x17;  /* N */
    codes[4] = 0x0D;  /* D */
    codes[5] = 0x26;  /* space */
    codes[6] = (unsigned char)((round_num / 10) % 10);
    codes[7] = (unsigned char)(round_num % 10);

    fill(banner_x, banner_y, 80, 24, 0);
    draw_text(text_x, text_y, 15, codes, 8);

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
 * Returns 16-bit signed 8.8 fixed-point per-tick displacement. */
static void dir_to_dxdy(unsigned char dir, unsigned char speed,
                         int *out_dx, int *out_dy) {
    unsigned char q = dir & 0x30;
    unsigned char d = dir & 0x0F;
    int C = dir_sin_tbl[d];
    int L = dir_sin_tbl[16 - d];
    int hl, bc;
    switch (q) {
        case 0x00: hl = L;          bc = C;          break;   /* +x +y */
        case 0x10: hl = C;          bc = L - 256;    break;   /* +x -y */
        case 0x20: hl = L - 256;    bc = C - 256;    break;   /* -x -y */
        default:   hl = C - 256;    bc = L;          break;   /* -x +y */
    }
    *out_dx = hl * (int)speed;
    *out_dy = bc * (int)speed;
}

#define DEATH_SPARK_COUNT 10
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
    fill(0, 0, SCREEN_W, SCREEN_H, COL_BORDER);
    draw_frame(10);
    paint_bg_to_buff(bg_attr, cycle);
    render_brick_band(level_idx);
    render_brick_flash_to_buff();
    paint_frame_to_buff(cycle, level_idx);
    render_lives(cycle, bg_attr);
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
    render_hud_score();
    render_hud_high_score();
    render_hud_powerups();
}

/* Block in a self-contained PIT-paced loop while the bat explodes —
 * the original's per-frame LBAED keeps running through LBC10's spark
 * lifetime; we play it as a separate phase, then return to the outer
 * run_level for the lives-- + respawn step. Mirrors the LBC10_3 spawn
 * loop: 10 sparks, dir = $1B + 5*i (mod 64), speed 2, starting at
 * (bat_center - 12 + 3*i, $AE), 5-frame decay with halving speed. */
/* Reset the primary ball + bat to fresh-life state. Mirror of
 * all_var_init at \$B7F8 (called from LB9E8_1 on each life-start in
 * the original): ball stuck on bat, bat.bonus_applied = \$FF (= no
 * bonus), all timer-based bat / ball effects cleared. Called from
 * each of the 3 post-explosion respawn sites. */
static void respawn_primary_ball(void) {
    ball_stuck     = 1;
    stuck_ticks    = 0;
    stuck_offset_x = BALL_X_OFFSET_ON_BAT;
    BALL_SHOW();
    BALL_X = BAT_X + BALL_X_OFFSET_ON_BAT;
    BALL_Y = BAT_Y_PX - eff_ball_size();
    ball_dx = +BALL_SPEED;
    ball_dy = -BALL_SPEED;
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
    int bat_center = BAT_X + (BAT_W_BYTES * 8) / 2;
    int x_start = bat_center - 12;
    unsigned char dir = 0x1B;
    unsigned long last;
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
    pts_400_ticks = 0;
    bullet_active[0] = 0;
    bullet_active[1] = 0;
    bullet_blast_ticks[0] = 0;
    bullet_blast_ticks[1] = 0;
    brick_flash_ticks = 0;
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
    /* Push sound $08 — same beep LBC10's level-cleared branch uses, but
     * here it covers the spark-fanout. Approximated with a longer
     * fixed-frequency tone until play_sound_08's state-driven envelope
     * is mapped properly. */
    sound_play(700, 30);
    last = pit_ticks();
    do {
        unsigned long now;
        do {
            sound_tick();
            now = pit_ticks();
        } while (now == last);
        last = now;
        alive = 0;
        for (i = 0; i < DEATH_SPARK_COUNT; i++) {
            int dx_q88, dy_q88;
            int xp, yp;
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
            if (xp < 8) {
                death_sparks[i].x_q88 = 8L << 8;
                death_sparks[i].dir = (unsigned char)((death_sparks[i].dir ^ 0x20) & 0x3F);
            } else if (xp >= PLAYFIELD_W - 8) {
                death_sparks[i].x_q88 = (long)(PLAYFIELD_W - 8) << 8;
                death_sparks[i].dir = (unsigned char)((death_sparks[i].dir ^ 0x20) & 0x3F);
            }
            if (yp < 8) {
                death_sparks[i].y_q88 = 8L << 8;
                death_sparks[i].dir = (unsigned char)((death_sparks[i].dir ^ 0x20) & 0x3F);
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

    /* The original loops levels forever (increment_round_number at
     * $BBE0 wraps current_level_number_1up at 15 → 0 while
     * round_number_1up keeps bumping). Game only ends on lives == 0. */
    round_number = 0;
    /* BAT_X resets to BAT_X_INIT only on NEW GAME (game_restart at
     * \$B9A0 in the original). Across level transitions and death
     * respawns the bat stays wherever the player left it. */
    BAT_X      = BAT_X_INIT;
    BAT_PREV_X = BAT_X_INIT;
    for (;;) {
        int k;
        unsigned char lvl_idx = (unsigned char)(round_number % N_LEVELS);
        current_level_idx_var = lvl_idx;
        i = lvl_idx;                                 /* keep `i` alias for the cycle / bg_attr code below */
        objects[OBJ_ENEMY].sprite_set = 0;     /* alien cleared on level entry */
        ball_stuck    = 1;
        stuck_offset_x = BALL_X_OFFSET_ON_BAT;
        BALL_SHOW();                      /* visible from level entry; sits on the bat */
        BALL_X        = BAT_X + BALL_X_OFFSET_ON_BAT;
        BALL_Y        = BAT_Y_PX - BALL_H_PX;
        stuck_ticks   = 0;                /* counts up while waiting for launch */
        ball_dx       = +BALL_SPEED;
        ball_dy       = -BALL_SPEED;
        bonus_active   = 0;
        bomb_active        = 0;
        bullet_active[0]      = 0;
        bullet_active[1]      = 0;
        bullet_blast_ticks[0] = 0;
        bullet_blast_ticks[1] = 0;
        bullet_cooldown       = 0;
        rocket_active      = 0;
        brick_flash_ticks  = 0;
        ball2_active   = 0;
        objects[OBJ_BALL_2].sprite_set = 0x82;
        ball3_active   = 0;
        objects[OBJ_BALL_3].sprite_set = 0x82;
        pts_400_ticks  = 0;
        slow_ticks     = 0;
        big_bat_ticks  = 0;
        big_ball_ticks = 0;
        life_dropped_this_round = 0;       /* mirrors flag_extra_life clear */
        run_dot_frame = 0x0E;               /* matches running_dot_frame_1up reset */
        bat_extra_px   = 0;
        bat_extra_tgt  = 0;
        objects[OBJ_BAT_1].bonus_applied = 0xFF;
        objects[OBJ_BAT_2].bonus_applied = 0xFF;
        /* Mirror all_var_init's `clear_hl_buff` of sounds_queue at line
         * 5984 — sounds in-flight at level entry shouldn't bleed into
         * the new round. */
        snd_q_silence_all();
        for (k = 0; k < LVL_CELLS; k++) {
            live_level[k] = levels[(int)i * LVL_CELLS + k];
        }
        render_level_screen(i);
        if (show_round_banner((unsigned int)round_number + 1)) return ST_QUIT;
        render_level_screen(i);                /* re-paint to clear the banner */
        if (play_brik_anim()) return ST_QUIT;
        cycle = (unsigned char)(i & 3);
        bg_attr = bg_attr_per_cycle[i & 3];
        start     = bios_ticks();
        last_tick = pit_ticks();
        for (;;) {
            unsigned long now;
            int ball_moved = 0;
            int bat_moved  = 0;

            if (kbhit()) {
                int k = getch();
                if (k == KEY_ESC) return ST_QUIT;
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
                        /* Shallow launch angle (|dx| < |dy|) so the ball
                         * traverses each brick row across multiple cols
                         * - lots of L1's bricks would otherwise be missed
                         * by a 45-deg deterministic launch from the bat's
                         * default x. Aimed AWAY from the nearest wall. */
                        {
                            int bat_centre = BAT_X + (BAT_W_BYTES * 8) / 2;
                            ball_dx = (bat_centre < PLAYFIELD_W / 2) ? +1 : -1;
                            ball_dy = -BALL_SPEED;
                        }
                    }
                    /* If the bat carries the LASER bonus and a free
                     * bullet slot exists, also fire one from the bat
                     * top centre. Two slots = up to two in flight at
                     * once (port of object_bullet_1 / _2 at $A0FA).
                     * Independent of ball state — SPACE can refire
                     * the laser while the ball is in play. */
                    if (objects[OBJ_BAT_1].bonus_applied == 0x01
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
                            bullet_y[free_slot] = BAT_Y_PX - 1;
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
                    if (key_state[SC_LEFT]  && BAT_X > min_now) BAT_X -= 4;
                    if (key_state[SC_RIGHT] && BAT_X < max_now) BAT_X += 4;
                }
                if (ball_stuck) {
                    /* Ball rides the bat at the catch offset (= where it
                     * hit, when the CATCH bonus stuck it; otherwise the
                     * default BALL_X_OFFSET_ON_BAT) until SPACE or
                     * timeout. */
                    BALL_X = BAT_X + stuck_offset_x;
                    BALL_Y = BAT_Y_PX - eff_ball_size();
                    ball_moved = 1;
                    stuck_ticks++;
                    if (stuck_ticks >= STUCK_TIMEOUT) {
                        ball_stuck = 0;          /* auto-launch */
                        snd_q_push(SND_BALL_START);
                        /* Pick an initial direction toward the centre. */
                        int bat_centre = BAT_X + (BAT_W_BYTES * 8) / 2;
                        ball_dx = (bat_centre < PLAYFIELD_W / 2) ? +1 : -1;
                        ball_dy = -BALL_SPEED;
                    }
                } else if (BALL_VISIBLE) {
                    int slow_skip = (slow_ticks > 0) && ((now & 1) == 0);
                    if (!slow_skip) {
                        step_ball();
                        ball_moved = 1;
                    }
                }
                step_bonus();
                step_pts_400();
                step_bomb();
                step_bullet();
                step_bullet_blast();
                step_rocket();
                step_brick_flash();
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
                /* Mirror balls_quantity == 0 → LBC10's bat-explosion
                 * branch. If the primary ball is hidden (= it fell while
                 * extras were in play) and the last extra just fell,
                 * the player has no balls left: run the death animation
                 * and respawn the primary stuck on the bat. */
                if (!BALL_VISIBLE && !ball2_active && !ball3_active) {
                    play_bat_explosion(current_level_idx_var);
                    if (lives > 0) lives--;
                    respawn_primary_ball();
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
                if (pts_400_ticks > 0) ball_moved = 1;
                if (bat_extra_px != bat_extra_tgt) bat_moved = 1;
                if (objects[OBJ_ENEMY].sprite_set != 0) ball_moved = 1;
                if (bomb_active) ball_moved = 1;
                if (any_bullet_active()) ball_moved = 1;
                if (any_bullet_blast()) ball_moved = 1;
                if (rocket_active) ball_moved = 1;
                if (brick_flash_ticks) ball_moved = 1;
                if (ball2_active) ball_moved = 1;
                if (ball3_active) ball_moved = 1;
            }

            if (BAT_X != BAT_PREV_X) {
                bat_moved = 1;
                BAT_PREV_X = BAT_X;
            }

            if (ball_moved) {
                redraw_full_with_ball(i);
            } else if (bat_moved) {
                redraw_bat(cycle, bg_attr);
                if (BALL_VISIBLE && ball_stuck) {
                    BALL_X = BAT_X + BALL_X_OFFSET_ON_BAT;
                    render_ball(BALL_X, BALL_Y, bg_attr);
                }
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
                 * × 3.6 s ≈ 65. */
                start = bios_ticks();
                while (!TIMED_OUT(start, 65UL)) {
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
                /* Original's LBBFB_0 pauses ~0.6 s (pause_long B=2)
                 * before the next level's setup — let the player see
                 * the cleared brick zone briefly. Plays one beep of
                 * sound $08 (param $3D) at the start of the pause —
                 * approximated here with a fixed-frequency tone since
                 * our sound mapping doesn't yet implement play_sound_08
                 * faithfully. ESC still quits. */
                unsigned long t = pit_ticks();
                sound_play(700, 25);            /* ~0.5 s 700 Hz beep */
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
        load_frame("FRAMEL1.BIN") != 0 ||
        load_sprites("SPRITES.BIN") != 0) {
        fill(0, 0, SCREEN_W, SCREEN_H, 10 /* bright red */);
    }

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
    set_mode(0x03);
    return 0;
}
