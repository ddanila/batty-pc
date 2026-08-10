/* See hud.h. */

#include "hud.h"
#include "zxvga.h"

u8       font[FONT_GLYPHS * FONT_ROWS];
u8       markup[MARKUP_MAX];
unsigned markup_len;

void draw_glyph(int x, int y, u8 colour, u8 code) {
    int r, i;
    unsigned char b;
    unsigned char *dst;
    if (code >= FONT_GLYPHS) return;
    dst = vga + (long)y * SCREEN_W + x;
    for (r = 0; r < FONT_ROWS; r++) {
        b = font[code * FONT_ROWS + r];
        for (i = 0; i < 8; i++) {
            if (b & (0x80 >> i)) dst[i] = colour;
        }
        dst += SCREEN_W;
    }
}

/* Any non-zero multiple of 8, where `marker / 8` is the X column in
 * cells. Observed markers span col 2 (0x10, the "000000" P1 score)
 * through col 25 (0xC8, the "2 UP" label), so one record-walker handles
 * them all. Zero is filtered out so it is never a marker. */
bool is_row_marker(u8 b) {
    return b != 0 && (b & 7) == 0;
}

namespace {

unsigned render_record(unsigned p) {
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

}  /* namespace */

void render_markup() {
    unsigned p = 0;
    while (p < markup_len) {
        if (is_row_marker(markup[p])) p = render_record(p);
        else                          p++;
    }
}

void score_to_digits(unsigned long s, u8 out[6]) {
    int i;
    for (i = 5; i >= 0; i--) {
        out[i] = (unsigned char)(s % 10);
        s /= 10;
    }
}
