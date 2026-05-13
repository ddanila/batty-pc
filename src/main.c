/* batty — mode-13h hello + ZX loading screen blit.
 *
 * Sets VGA mode 0x13, programs the first 16 palette entries to the
 * standard ZX Spectrum colours (non-bright + bright × ink/paper),
 * then streams LOADING.BIN row-by-row into the 256x192 playfield
 * area at (32, 4). ESC quits.
 *
 * LOADING.BIN is produced offline by scripts/extract_scr.py from
 * the original .SCR — one byte per pixel, palette index 0..15. */

#include <conio.h>
#include <i86.h>
#include <stdio.h>
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

/* Stream LOADING.BIN straight into VGA. 192 reads of 256 bytes —
 * keeps the small-model near-data segment unburdened. */
static int blit_loading_screen(const char *path) {
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

int main(void) {
    int rc;
    set_mode(0x13);
    set_palette(zx_palette, 16);
    fill(0, 0, SCREEN_W, SCREEN_H, COL_BORDER);

    rc = blit_loading_screen("LOADING.BIN");
    if (rc != 0) {
        /* Asset missing — paint a magenta band so the failure is obvious. */
        fill(BORDER_X, BORDER_Y, PLAYFIELD_W, PLAYFIELD_H, 3 /* magenta */);
    }

    while (getch() != 27) { /* ESC quits */ }

    set_mode(0x03);
    return 0;
}
