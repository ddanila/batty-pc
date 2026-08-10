/* hud — text, the markup screens, and the score/indicator furniture.
 *
 * These draw STRAIGHT TO VGA, not through the two planes: they sit in the
 * border and HUD areas the playfield compositor never touches, so there is
 * no attribute cell to clash into and no dirty tracking to do.
 *
 * MARKUP
 * ------
 * The hi-score and menu screens are not bitmaps — the original stores
 * them as a byte stream describing where text goes, and this replays it.
 * The decode (notes/encoding.md):
 *
 *   record  = marker, y, attr, count, then `count` payload bytes
 *   marker  = a non-zero multiple of 8; its value / 8 is the column
 *   payload = 0x00..0x09 digit    0x0A..0x23 letter
 *             0x26      space     0x40..0x4F inline colour change
 *
 * Anything that is not a marker is skipped, which is how the stream
 * tolerates the padding between records.
 *
 * An inline colour change occupies a payload slot but must NOT advance
 * the cursor — get that wrong and every glyph after the first colour
 * change on a line sits one cell right. */

#ifndef BATTY_HUD_H
#define BATTY_HUD_H

#include "types.h"

/* --- Art: rendered from here, loaded by the caller -------------------- */

const int FONT_GLYPHS = 43;
const int FONT_ROWS   = 6;      /* 8x6; the char cell's top 2 rows are pad */
const int MARKUP_MAX  = 512;

extern u8 font[FONT_GLYPHS * FONT_ROWS];
extern u8 markup[MARKUP_MAX];
extern unsigned markup_len;

/* --- Text ------------------------------------------------------------ */

/* `code` indexes the font: 0..9 digits, 0x0A..0x23 letters. Out-of-range
 * codes draw nothing rather than reading past the font. */
void draw_glyph(int x, int y, u8 colour, u8 code);

void render_markup();

/* True for a byte that starts a record — see the decode above. */
bool is_row_marker(u8 b);

/* --- Score ----------------------------------------------------------- */

/* Six digits, most significant first. The display is fixed-width, so a
 * score is always padded with leading zeros rather than trimmed. */
void score_to_digits(unsigned long score, u8 digits[6]);

#endif /* BATTY_HUD_H */
