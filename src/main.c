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
    /* 0x28 (col 5, COPYRIGHT) .. 0x70 (col 14, BATTY title), all
     * multiples of 8. col = b / 8. */
    return (b & 7) == 0 && b >= 0x28 && b < 0x78;
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

static void demo_full(void) {
    load_markup("MARKUP.BIN");
    fill(0, 0, SCREEN_W, SCREEN_H, COL_BORDER);
    draw_frame(10);   /* bright red */
    render_markup();
}

/* Render the main menu from the inline markup at 0x9571 in snap2.
 * Menu's frame is bright magenta (palette 11), not red. */
static void demo_menu(void) {
    load_markup("MENUMARK.BIN");
    fill(0, 0, SCREEN_W, SCREEN_H, COL_BORDER);
    draw_frame(11);   /* bright magenta */
    render_markup();
}

/* Cycle controlled by BATTYALL env var (set by the test floppy's
 * AUTOEXEC). Unset -> menu-only loop. Set (any value) -> full 4-state
 * cycle. Env var is reliable; argc/argv plumbing through the DOS PSP
 * with the 16-bit small-model startup is not. */
int main(void) {
    int full_cycle = (getenv("BATTYALL") != NULL);
    set_mode(0x13);
    set_palette(zx_palette, 16);

    if (load_font("FONT.BIN") != 0) {
        fill(0, 0, SCREEN_W, SCREEN_H, 10 /* bright red */);
    }

    for (;;) {
        show("MAINMENU.BIN"); if (getch() == 27) break;
        demo_menu();          if (getch() == 27) break;
        if (full_cycle) {
            show("HISCORE.BIN"); if (getch() == 27) break;
            demo_full();         if (getch() == 27) break;
        }
    }

    set_mode(0x03);
    return 0;
}
