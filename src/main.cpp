/* batty — the game: screen states, level flow, and everything that
 * moves. The state machine is main() at the bottom of this file. */

#include <bios.h>
#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets.h"
#include "bricks.h"
#include "hud.h"
#include "objects.h"
#include "bonus_codes.h"
#include "enemies.h"
#include "weapons.h"
#include "replay_parse.h"
#include "replay.h"
#include "sound.h"
#include "physics.h"
#include "rng.h"
#include "scoring.h"
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

#define LIVES_INIT          3
/* Per-player state. The original keeps two of each — `score_1up_in_game`
 * / `score_2up_in_game`, `lives_1up` / `lives_2up` — and `game_restart`
 * zeroes both, so a 2-player game alternates between them while the HUD
 * shows both at once.
 *
 * The high score is deliberately NOT in here: it is one number for the
 * machine (`hi_score_in_game`), shown in the middle HUD slot, and keeping
 * it per-player makes the HI column change when the players swap. */
struct PlayerState {
    unsigned long score;
    int           lives;
    unsigned char live_adds_awarded;
    /* The original's per-player block is eight bytes at `lives_1up`,
     * and `players_swap` exchanges it wholesale with `lives_2up`:
     *
     *   +0  lives
     *   +1  briks_quantity        bricks left on THAT player's level
     *   +2  current_level_number  0..14
     *   +3  round_number          may exceed 15; the level wraps, this
     *                             does not
     *   +4..6 current_score       three BCD cells
     *   +7  ctrl_type             the input device, PER PLAYER
     *
     * So a player resumes their own round and level, not a shared one.
     * `ctrl_type` is where WS1's device selection belongs, and `p1_dev`
     * / `p2_dev` are aliases onto it rather than separate bytes. */
    unsigned char level_number;
    unsigned char round_number;
    unsigned char ctrl_type;
};
static PlayerState players[2] = {{0, LIVES_INIT, 0, 0, 0, 0},
                                {0, LIVES_INIT, 0, 0, 0, 0}};


/* Player input-device state. The menu's A and B keys cycle it (the
 * original's handlers at $94BD and $9502), range 0..3 =
 * KEYBOARD / KEMPSTON / CURSOR / INTERFACE II.
 *
 * The menu only CYCLES it: nothing reads it to choose an input source
 * yet, and the original's bat-2 reader `get_right_player_ctrl_state` has
 * no port equivalent. PLAN.md WS1. */
#define p1_dev (players[0].ctrl_type)
#define p2_dev (players[1].ctrl_type)

/* Player indicators 32×16 each: P1 at blob 0x92C1, P2 at 0x9303.
 * Stored on disk as one 132 B blob: P1 header+body (66) then P2 (66). */
#define INDICATOR_W_BYTES  4
#define INDICATOR_H        16
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
/* Where the score digits sit, and how far down they reach. The original
 * prints them at y=$15 and each glyph is 8 pixel rows, so the last row
 * they touch is 28 — FIVE rows BELOW the 24-row top frame.
 *
 * That gap was a real defect: the in-place HUD patch (update_static_hud_top,
 * taken whenever a score changes on a magnet-free level) copied and
 * flushed FRAME_TOP_H_PX rows, so rows 24..28 of every digit — the lower
 * half of the glyph — kept the PREVIOUS score's pixels in the cache and
 * on screen until something else forced a full rebuild. Subtle, because
 * the top three rows updated normally and digits differ least at the
 * bottom. known-bugs.md #22. */
#define HUD_SCORE_Y      0x15
#define HUD_DIGIT_H_PX   8
#define HUD_PATCH_H_PX   (HUD_SCORE_Y + HUD_DIGIT_H_PX)   /* rows 0..28 */
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
 * original — see notes/sprites.md). */
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

static unsigned char levels[LVL_SIZE];

/* Mutable per-game copy of the current level's 180 cells (the
 * original keeps the equivalent at $6100, current_level_copy). Bricks
 * destroyed by the ball get bit 7 set here, making print_briks_c skip
 * them on the next repaint. */
static unsigned char live_level[LVL_CELLS];

/* Each player's brick grid, saved across their turn.
 *
 * The original has no separate save area: current_level_2up_copier
 * exchanges `current_level_copy` with the arriving player's slot in the
 * LEVEL TABLE itself, 180 bytes at a time. The port keeps the level
 * table immutable and holds the two grids here instead — same effect,
 * and it does not need the table to be writable.
 *
 * `valid` is port bookkeeping the original does not need: its level
 * table always holds a playable grid, whereas player_grid[1] is zeroed
 * until player 2 has actually had a turn, and restoring zeros would read
 * as "every brick destroyed" and clear the level instantly. */
static unsigned char player_grid[2][LVL_CELLS];
static unsigned char player_grid_valid[2] = { 0, 0 };
static unsigned char resume_player_grid = 0;

/* Set by lose_a_life, acted on by run_level: the frame loop has to
 * unwind to the level-entry point before the turn can change, because
 * the arriving player may be on a different level. */
static unsigned char pending_turn_change = 0;

/* How many hand-overs have happened, split by which path took them.
 *
 * These exist because active_player alone is not observable from a
 * capture: PROBE.TXT is rewritten at every level entry, and a scenario
 * that dies repeatedly re-enters repeatedly, so a probe read at any
 * moment reports whoever happened to enter last. Counters accumulate
 * instead and survive every later write — the same reason
 * enemy_repicks exists. Without them the LBC10_7 path shipped ungated.
 *
 *   life   a life was lost with lives to spare (LBC10's DEC/JR Z else)
 *   over   the player ran OUT and the other carried on (LBC10_7) */
static unsigned long turn_changes_life = 0;
static unsigned long turn_changes_over = 0;
static unsigned long game_overs_reached = 0;
static unsigned char level_attrs[ATTR_TOTAL_SIZE];

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
 * flipped to "on", verified by the L2 magnet showing up in the 271-px
 * diff at y=44..70 — which is the whole reason both sprites are
 * painted. See notes/magnets.md. */
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
 * ($8DB7/$8DB8). magnets.px/py keep the PAINT origin (x0,y0 from the
 * level table); the original's slot stores (x0+5, y0+5) — the circle's
 * physics box origin — with body size 15x14 px (slot +$0C/+$0D).
 * magnets.on_state mirrors slot +$01: 1 = sprite $06 (ON, bit0 clear),
 * 0 = sprite $07 (OFF). Set once per level entry by magnet_level_init
 * (the print_magnets coin), flipped at random by magnet_random_toggle
 * (print_one_magnet $8E72). */
struct MagnetState {
    unsigned char count;
    unsigned char px[MAGNETS_MAX_PER_LEVEL];
    unsigned char py[MAGNETS_MAX_PER_LEVEL];
    unsigned char on_state[MAGNETS_MAX_PER_LEVEL];
    /* Magnet whose ON/OFF flip still needs its incremental circle
     * redraw applied at the next frame compose. */
    unsigned char toggle_pending;
};
#define MAGNET_TOGGLE_NONE  0xFF
static MagnetState magnets = {0, {0}, {0}, {0}, MAGNET_TOGGLE_NONE};
#define MAGNET_BODY_W  15        /* slot +$0C */
#define MAGNET_BODY_H  14        /* slot +$0D */

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
#define BG_TILE_H_PX 16
#define BG_TILE_SIZE (BG_TILE_H_PX * 2)
#define BG_TILE_CYCLES 4
static unsigned char bg_tile[BG_TILE_CYCLES * BG_TILE_SIZE];

/* Bat geometry, from the original spr_bat_normal ($7E38): 4 bytes wide
 * (32 px) * 13 rows. Top 8 rows are the body, last 3 are the dithered
 * shadow drop. */
#define BAT_W_BYTES 4
#define BAT_H_PX    13
#define BAT_Y_PX    0xAD            /* = 173, matches object_bat_1.y_coord */
/* The shipped frame strip is wider than the original's, so the clamps in
 * physics.h let the bat clip the frame at the extremes. Deliberate: full
 * reach beats partial-visibility cosmetics until the frame ornament
 * painter is ported. */
#define BAT_X_INIT  0x74             /* = 116, matches object_bat_1.x_coord */

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
/* Deterministic mid-game capture checkpoints. BATTY_VISUAL_PROBE_FRAMES
 * is a comma-separated list of ascending absolute frame indices; the
 * port runs to each in turn, halts (so the harness can grab a drift-free
 * capture), resumes on a key, and quits after the last. A single value
 * reproduces the original single-shot behaviour. This is the port side
 * of the frame-step parity sweep (see notes/replay-harness.md). */
#define VISUAL_PROBE_MAX 16

#define BULLET_W_PX     8    /* sprite width incl. transparent column */
#define BULLET_H_PX     8
/* Port of the `bullet` counter at $A160. The original sets it to $16
 * (=22) on each fire, then `SUB $02` per frame; SPACE is ignored until
 * the counter underflows. Net effect: ~11 frames between shots however
 * fast SPACE is mashed. */
static unsigned char bullet_cooldown = 0;

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
struct BrickFlashState {
    unsigned char ticks;
    int           x;
    int           y;
};
static BrickFlashState brick_flash = {0, 0, 0};
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
#define ROCKET_H_PX     27
#define ROCKET_BONUS_H_PX 0x0C
struct RocketState {
    unsigned char active;
    int           x;
    int           y;
    unsigned int  acc;
    unsigned char frac;
    unsigned char counter;
    /* Set when the rocket flies the bat off the top; suppresses the
     * no-ball death until the level-clear sequence takes over. */
    unsigned char clear_completed;
};
static RocketState rocket = {0, 0, 0, 0, 0, 0, 0};
/* Stuck-on-bat dwell counter. While ball.stuck[BALL_PRIMARY], the ball rides the
 * bat; SPACE detaches immediately; after STUCK_TIMEOUT ticks the ball
 * auto-launches. ~5 sec at 50 Hz. */
/* Mirror of ball.bonus_applied = $C0 at all_var_init's level entry: the
 * original counts down 192 ticks (= 3.84 s at 50 Hz) before auto-
 * releasing a stuck ball. */
#define STUCK_TIMEOUT 192

/* The original shares object_bonus between the bomb and a regular
 * bonus, making them mutually exclusive; the port keeps them separate.
 * orig: $A977 bomb_appear */
#define BOMB_W_PX       8
#define BOMB_H_PX       12

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

/* The original keeps the score as 3-byte BCD across current_score_1up
 * and the in-game digits at score_1up_in_game; the port uses a plain
 * integer. Milestone thresholds for live_adds_awarded live in
 * scoring.h. orig: $B9A0 game_restart sets lives to 3 */
/* The original's `game_mode`, and it is 0-BASED:
 *
 *   0  1 Player
 *   1  2 Players (alternating)
 *   2  Double Play
 *
 * read off LBC10 — `CP $02 / CALL Z,print_txt_players_1_and_2` prints
 * "PLAYERS 1 AND 2" for mode 2, and the life-loss path's
 * `LD A,(game_mode) / DEC A / CALL Z,current_level_2up_copier` runs the
 * alternation for mode 1. The menu's `selected_mode` is 1..3, because
 * it comes straight from `k - '0'`, so the conversion is a subtract-one
 * and `game_mode_from_selection` is the only place allowed to do it.
 * See notes/menu.md. */
static unsigned char game_mode = 0;

static unsigned char game_mode_from_selection(unsigned char selection) {
    return (unsigned char)((selection >= 1 && selection <= 3)
                           ? selection - 1 : 0);
}

/* Whose turn it is: 0 = 1UP, 1 = 2UP. */
static unsigned char active_player = 0;

/* round_number and the level index are the ACTIVE player's, because the
 * original keeps them in the per-player block `players_swap` exchanges —
 * each player resumes their own round and level, not a shared one. */
#define player                players[active_player]
#define round_number          players[active_player].round_number
#define current_level_idx_var players[active_player].level_number

static unsigned long high_score = 0;

/* The primary ball's OWNER side in Double Play — bit 7 of the original's
 * `object_ball_1+$12`. Not a coordinate and not derived from one: the
 * rest of that byte is a counter, and every operation on it preserves
 * bit 7 deliberately —
 *
 *   LA27E_17:  LD A,(IX+$12) / AND $80 / LD (IX+$12),A   ; zero counter
 *   LA27E_22:  ... INC A ... AND $7F / CP $7F ...        ; wrap counter
 *              LD A,(IX+$12) / AND $80 / LD (IX+$12),A
 *
 * so the COUNTER half never disturbs it. The bit itself is written in
 * two places, and the second uses RES/SET rather than LD, so it does not
 * turn up in a grep for `LD (IX+$12)`:
 *
 *   LAB1F_0:  RES 7,(IX+$12) / BIT 7,(IY+$02) / JR Z / SET 7,(IX+$12)
 *
 * — on every bat deflection, from the x of the bat that hit it. So the
 * ball changes hands in play. The other write is the initial one at
 * `all_var_init`, from which side the ball STARTS on, and the start side
 * alternates every entry:
 *
 *   LD A,(ball_x_coord+$01) / XOR $88 / LD (ball_x_coord+$01),A
 *   ...
 *   ball_x_coord: LD A,$48        ; self-modified: $48 <-> $C0
 *   LD (object_ball_1+$02),A
 *   CP $C0 / JR NZ,LB7F8_1
 *   LD A,(object_ball_1+$12) / OR $80 / LD (object_ball_1+$12),A
 *
 * $48 XOR $88 = $C0 and back, so the ball starts left, right, left...
 *
 * The consequence is worth stating because it is not what "score by
 * side" suggests: brick points go to whoever the BALL belongs to, for
 * that ball's whole life, wherever the brick is. Only the bat, bullet
 * and bonus sites are positional.
 *
 * One per ball, not one per game: LAB1F_0 writes the bit on whichever
 * ball is being handled and `handling_ball` runs once per ball object.
 * Slot 0..2 = OBJ_BALL_1..3, as for the stuck and mag_* arrays. */
static unsigned char ball_owner_side[3] = {0, 0, 0};   /* 0 = 1UP, 1 = 2UP */
static unsigned char ball_start_right = 0;     /* the alternating flag */

/* orig: add_points_to_score ($018D). In Double Play the score goes to the
 * side the event happened on, not to whoever is "active". The original
 * reads `need_change_player`, which is set from a top-bit test at five
 * sites — and WHICH object's bit differs by caller:
 *
 *   handling_bat      (IX+$02) & $80   the BAT's x    -> kill_enemy_by_bat
 *   LA67B_1           (IY+$02) & $80   the BAT's x    -> get_bonus
 *   handling_bullet   (IX+$02) & $80   the BULLET's x
 *   handling_ball     (IX+$12) & $80   the ball's OWNER flag, not its x
 *   add_points_for_left_briks          zero, then alternated to split
 *                                      the leftover bricks evenly
 *
 * See notes/double-play.md. */
static void add_points_to_score(unsigned long pts, int side_x) {
    if (game_mode == 2 && (side_x & 0x80))
        players[1].score += pts;
    else
        player.score += pts;
}


/* The replay harness's own state. None of this affects the game: every
 * field is driven by a BATTY_* environment variable and read back through
 * PROBE.TXT, so a gate can seed a scenario and check what happened. */
struct ProbeState {
    unsigned long brik_anim_ticks;    /* intro shimmer duration, in PIT edges */

    /* known-bugs #15. Both clocks latched at the first gameplay frame,
     * so a probe write later in the SAME run yields two readings a known
     * number of frames apart. Reading bios_ticks() once per run only
     * ever compared separate boots, which mixes guest time with host
     * time and cannot give a rate. */
    unsigned long bios_at_frame1, pit_at_frame1;
    unsigned char clocks_latched;

    /* The last primary-ball launch, read back by the launch gate. */
    struct LaunchCapture {
        unsigned char valid, x, y, dir, speed;
    } last_launch;

    /* Halt after N primary-ball launches / N frames. */
    unsigned int  launch_frames, launch_countdown;
    unsigned char launch_active;
    unsigned int  frame_frames, frame_countdown;
    unsigned char frame_active;

    /* Whether the next PROBE write is a gameplay checkpoint or the
     * pre-gameplay seed. */
    unsigned char from_gameplay;

    /* Frames at which to dump the screen for a visual comparison. */
    unsigned int  visual_list[VISUAL_PROBE_MAX];
    unsigned char visual_count, visual_index;
    unsigned int  visual_countdown;
    unsigned char visual_active;

    unsigned int  shots_fired;        /* laser cadence gate */
    unsigned char serial_enabled;     /* frame-completion signal over COM1 */
};

static ProbeState probe;

/* The balls' state, beyond what the object descriptors carry.
 *
 * The PRIMARY ball is objects[OBJ_BALL_1]; dx/dy here are its whole-pixel
 * velocity, kept alongside the descriptor's 6-bit direction because the
 * two collision paths speak different languages (see known-bugs #8).
 *
 * The two EXTRA balls exist only during multiball. They are separate
 * fields rather than an array because the original gives them their own
 * object slots and steps them through different code — step_extra_ball,
 * not step_ball.
 *
 * The mag_* arrays are per-ball magnet capture state, indexed by ball
 * 0..2, and are the one part here that does cover all three uniformly. */
struct BallState {
    int          dx, dy;              /* primary ball, whole pixels */
    /* Only [0] is ever non-zero today: an extra ball is spawned in
     * flight and nothing can catch it, because catch_ball_on_bat is only
     * reachable from the primary's path. Widening it is the structural
     * half of WS6 item 2 (MAGNET catch for secondaries) and of WS3's
     * bat-2 catch — see notes/bat-deflection.md. */
    unsigned char stuck[3];           /* resting on the bat, awaiting launch */
    /* WHICH bat is holding it. In Double Play either can, and a held
     * ball rides the bat that caught it — so this has to be state, not
     * a derivation: bat 2 is not "the bat on the right half" once a
     * court clamp has moved it, and the ball's own x is the bat's. */
    unsigned char stuck_bat[3];
    int          stuck_offset_x[3];   /* where on the bat it rests */
    unsigned int stuck_ticks[3];      /* counts up to the auto-launch */
    unsigned int speed_ramp;
    unsigned int big_ticks;           /* BIG_BALL bonus, counts down */

    unsigned char extra2_active, extra3_active;

    /* Magnet capture, per ball — port of the 4-byte LA270 (ball 1) /
     * LA274 (ball 2) / LA278 (other) state:
     *   cool:  +0  re-capture cooldown (2 frames after a release)
     *   delta: +1  per-frame dir rotation while captured (+1/-1, 0 = free)
     *   exit:  +2  quantized release direction, recomputed every frame
     *   idx:   +3  capturing magnet's slot index */
    unsigned char mag_cool[3], mag_delta[3], mag_exit[3], mag_idx[3];
};

/* Ball slot 0, named so that `[0]` meaning "the first ball" and `[0]`
 * meaning "the only ball that can be here" do not read alike. */
#define BALL_PRIMARY 0

static BallState ball = {
    +BALL_SPEED, -BALL_SPEED,
    {1, 0, 0}, {OBJ_BAT_1, OBJ_BAT_1, OBJ_BAT_1},
    {BALL_X_OFFSET_ON_BAT, 0, 0}, {0, 0, 0},
    0, 0,
    0, 0,
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};

/* The bat's state, beyond objects[OBJ_BAT_1].
 *
 * `extra_px` is how far the BIG_BAT bonus has grown it, and
 * `extra_target` where it is growing to — the resize is animated, so the
 * two differ while it is in motion.
 *
 * The drawn_* fields are what the LAST redraw actually put on screen.
 * The narrow bat-only refresh path compares against them to decide which
 * pixels to repaint; a stale value there leaves fragments behind, which
 * is what test-bat-redraw-window guards. */
struct BatState {
    int           extra_px, extra_target;
    unsigned char fire_anim_ticks;     /* laser cannon flash, counts down from 8 */
    unsigned int  big_ticks;           /* BIG_BAT bonus, counts down */

    int           drawn_extra_px;
    int           drawn_y;
    unsigned char drawn_bonus, drawn_fire_ticks;
};

/* INDEX BY BAT_SLOT(), NEVER by the object index. `OBJ_BAT_1` is 6 and
 * `OBJ_BAT_2` is 5 — positions in the eleven-slot OBJECT table, not 0
 * and 1. Writing `bats[OBJ_BAT_1]` compiles clean, indexes four elements
 * past the end and silently corrupts whatever `.data` follows.
 *
 * WS3: bat 2 has nowhere to record a width yet, so a BIG_BAT caught by
 * it is guarded to bat 1 and widens nobody. */
static BatState bats[2] = {
    { 0, 0, 0, 0, 0, BAT_Y_PX, 0xFF, 0 },
    { 0, 0, 0, 0, 0, BAT_Y_PX, 0xFF, 0 },
};

/* Object index -> bats[] slot. Anything that is not bat 2 is bat 1's,
 * so an out-of-range argument lands on a real element rather than off
 * the end. */
#define BAT_SLOT(o)  ((o) == OBJ_BAT_2 ? 1 : 0)

/* Bat 1's slot. NOT named `bat`: functions here take an `int bat` /
 * `int bat_idx` parameter that such a macro would shadow. */
#define bat1 bats[BAT_SLOT(OBJ_BAT_1)]

/* Mirror of flag_extra_life — set when the player catches a LIFE bonus
 * in the current round, prevents another LIFE drop until the round
 * ends. Reset at each new round entry in run_level. */
static unsigned char life_dropped_this_round = 0;
static unsigned char high_score_beaten_this_game = 0;
/* Three-letter initials saved with the high score. Stored as glyph
 * codes (A = 0x0A .. Z = 0x23). Default "AAA" when no save exists. */
static unsigned char high_score_name[3] = { 0x0A, 0x0A, 0x0A };

/* Catch hit-box. The visible bonus sprite is 24 px wide x 13 rows
 * tall (with a drop-shadow band), but we run the bat-collision
 * against the central body region only — 16 wide x 8 tall — so a
 * marginal "shadow touched the bat" doesn't register as a catch. */
#define BONUS_W_PX        16
#define BONUS_H_PX        8
/* The original's bonus codes (set_bonus / bonus_table_* at $9E4A) are
 * translated by bonus_from_original in bonus_codes.{cpp,h}, which is the
 * authority — do not keep a second copy of the mapping here. */

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
/* Ball speed model (handling_ball LA27E_22 / get_bonus LA67B_7):
 * the original ball ACCELERATES over a level. A per-ball counter
 * (object+$13) increments when (counter_misc & 7) == 0 — every 8
 * frames — and when it reaches $94 (148) it resets and the speed byte
 * (+$07) increments, capped at $06. So speed climbs $02 -> $06, one
 * step per ~1184 frames (~24 s). SLOW ($04) just sets all ball speeds
 * back to $02 (it does NOT touch the ramp counter), so it naturally
 * wears off as the speed ramps back up. We model this with the shared
 * ball.speed_ramp counter + the per-frame ball_speed_ramp_tick(), and
 * SLOW resets objects[].speed to BALL_SPEED. */
/* BIG_BAT is permanent in the original — handling_bat_no_transform
 * keys off bat.bonus_applied == \$00, with no auto-expire. The bat
 * stays wide until another bonus is caught or the ball is lost.
 * Setting to UINT_MAX-ish so the timer-based expiration never fires;
 * big_bat_active_of() AND with the bat's bonus_applied does the real
 * gating. */
#define BIG_BAT_DURATION  0xFFFFu
/* Original smash_counter expires at $F8 and advances only on every
 * other counter_misc value while the big-ball sprite is being printed. */
#define BIG_BALL_DURATION 0xF8u
#define BAT_BIG_EXTRA_PX    8     /* width added on each side in big mode */
struct BonusState {
    int           x;
    int           y;
    unsigned char type;
    unsigned char active;
    motion_acc_t  motion;
};
static BonusState bonus = {0, 0, 0, 0, {0, 0}};
static int big_ball_active(void);    /* forward — defined below */
static int big_bat_active_of(int b); /* forward — defined below */

/* "+400" floating-marker state spawned on bonus catch (port of
 * sprite_set $0B transition at $A6BA + handling_400pts at $A58D).
 * The original puts the marker in the same slot the bonus occupied;
 * we use side state for now since the bonus state is also side. */
struct PtsMarkerState {
    int           x;
    int           y;
    unsigned char active;
    motion_acc_t  motion;
    /* Per-frame X drift, one of {-2, -1, +1, +2} chosen from
     * random_number at spawn. orig: SMC at $A590 in handling_400pts */
    int           dx;
    /* "+400" by default; a SCORE_5K catch overrides to "+5000" so the
     * unusual reward has its own cue. */
    unsigned int  sprite;
};
static PtsMarkerState pts_marker = {0, 0, 0, {0, 0}, 0, 0};


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
 * 0..9 through all ten, so indexing this `& 7` silently drops the 3,2
 * tail (notes/bird-render-parity.md). Indexed by the LAAD2-stepped
 * sprite_num. */
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
 * frame 9). */
static const unsigned int spr_blast_frames[10] = {
    SPR_BLAST_1, SPR_BLAST_2, SPR_BLAST_3, SPR_BLAST_4, SPR_BLAST_5,
    SPR_BLAST_4, SPR_BLAST_3, SPR_BLAST_2, SPR_BLAST_1, SPR_BLAST_1
};
static const unsigned int spr_spark_frames[5] = {
    SPR_SPARK_1, SPR_SPARK_2, SPR_SPARK_3, SPR_SPARK_4, SPR_SPARK_5
};
#define BLAST_FRAMES 10
/* (blast cadence now comes from LAAD2 / handling_blast_obj: the kill sites
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
#define FRAME_SIDE_PX    (FRAME_SIDE_W * FRAME_SIDE_H_PX)
/* PIXELS ONLY — paint_frame_to_buff takes its attrs from `lattr`
 * (level_attrs.bin), never from this blob. */
#define FRAME_SIZE  (FRAME_TOP_PX + 2 * FRAME_SIDE_PX)
#define FRAME_CYCLES 4
static unsigned char frame_l1[FRAME_CYCLES * FRAME_SIZE];

/* The perimeter-frame sprites, $6B3F..$6CBC. Offsets are relative to
 * $6B3F. Each block is (w, h) then w*h pixel bytes then its attr block;
 * only the pixels are used here, since paint_frame_to_buff takes attrs
 * from level_attrs. */
#define BORDER_BLOB_SIZE 382
static unsigned char border_spr[BORDER_BLOB_SIZE];
#define BSPR_SIDE_BOLD_L    0     /* $6B3F */
#define BSPR_SIDE_BOLD_R   40     /* $6B67 — must follow the left one */
#define BSPR_SIDE_THIN_L   80     /* $6B8F */
#define BSPR_SIDE_THIN_R  111     /* $6BAE */
#define BSPR_H_LEFT_EDGE  142     /* $6BCD */
#define BSPR_H_LEFT_THIN  182     /* $6BF5 */
#define BSPR_H_LEFT_BOLD  222     /* $6C1D */
#define BSPR_H_RIGHT_THIN 262     /* $6C45 */
#define BSPR_H_RIGHT_BOLD 302     /* $6C6D */
#define BSPR_H_RIGHT_EDGE 342     /* $6C95 */

/* set_border_horizontal ($BFE7), in order. */
static const unsigned int border_top_seq[8] = {
    BSPR_H_LEFT_EDGE,  BSPR_H_LEFT_THIN,  BSPR_H_LEFT_BOLD, BSPR_H_LEFT_THIN,
    BSPR_H_RIGHT_THIN, BSPR_H_RIGHT_BOLD, BSPR_H_RIGHT_THIN, BSPR_H_RIGHT_EDGE
};

/* border_horizontal_addon ($BFFB): ANDed into scr_buff+$101, i.e. row 8
 * bytes 1..30 — a one-pixel inner outline under the top border. */
static const unsigned char border_addon[30] = {
    0x00, 0x00, 0x03, 0xFF, 0xFF, 0xFF, 0xC0, 0x00, 0x00, 0x00,
    0x03, 0xFF, 0xFF, 0xFF, 0xC0, 0x03, 0xFF, 0xFF, 0xFF, 0xC0,
    0x00, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0xFF, 0xC0, 0x00, 0x00
};







/* Where the render profile accumulates. Written only when
 * BATTY_RENDER_PROFILE is set, and dumped to PROFILE.TXT at exit.
 *
 * The `blocked_by_*` counters exist to answer one question: when a frame
 * could not take the cheap ball-only redraw path, what stopped it? Each
 * frame increments exactly the one that vetoed, so the tally says where
 * to look rather than just how often the fast path missed. */
struct RenderProfile {
    /* PIT ticks spent in each stage. */
    unsigned long bg_pit, frame_pit, hud_pit, bricks_pit, vga_pit, band_pit;

    unsigned long frames;
    unsigned long static_rebuilds;
    unsigned long band_rebuilds, band_rows;   /* rows/rebuilds shows the
                                               * incremental win directly:
                                               * a full band is 14 rows,
                                               * a scoped one about 3. */

    /* Which redraw path each frame took. */
    unsigned long full_dynamic_frames, ball_only_frames, ball_object_frames;

    /* What vetoed the ball-only path, when something did. */
    unsigned long blocked_by_bat, blocked_by_static, blocked_by_hud;
    unsigned long blocked_by_objects, blocked_by_bricks, blocked_by_balls;
    unsigned long blocked_by_bat_fx;
};

static RenderProfile prof;

/* BATTY_* switches that change how the port behaves, for A/B baselines
 * and for putting the game into a state a gate can reach. Unlike
 * ProbeState these DO affect play — that is what they are for.
 *
 * use_laffc and rng_perframe default ON: they select the accurate model,
 * and the switch exists to get the old one back for an A/B. LAFFC
 * collision is byte-exact against the Spectrum over L3's 150-frame
 * trajectory (test-laffc-ball-frame1) and falls back to brick_collision
 * when it reports no hit, so it can never pass a brick through;
 * multi-ball secondaries use brick_collision throughout.
 */

/* RNG-model alignment (see notes/rng-model.md). ON is the original's
 * model and byte-exact: random_number lives at $8D48 and the original
 * ticks it once per frame at the main-loop top (`CALL random_generate`
 * at LB9E8_2), with consumers reading without advancing. Seeded to the
 * original's frame-0 state the port reproduces that sequence exactly,
 * which is what makes the enemy steering match the GT — `make
 * test-rng-walk` pins it. BATTY_RNG_PERFRAME=0 reverts to advance-on-
 * read, which consumes the RNG faster than the original. */
/* Each field states its default as `default=0` or `default=1`, and
 * test-switch-defaults checks that against the initialiser below. */
struct DebugSwitches {
    unsigned char auto_fire;              /* BATTY_AUTO_FIRE: hold SPACE, default=0 */
    unsigned char full_band_rebuild;      /* BATTY_FULL_BAND_REBUILD, default=0 */
    unsigned char bat_full_redraw;        /* BATTY_FORCE_BAT_FULL_REDRAW, default=0 */
    unsigned char ball_full_redraw;       /* BATTY_FORCE_BALL_FULL_REDRAW, default=0 */
    unsigned char full_flush_each_frame;  /* BATTY_FORCE_FULL_FLUSH_EACH_FRAME, default=0 */
    unsigned char suppress_no_ball_death; /* BATTY_SUPPRESS_NO_BALL_DEATH, default=0 */
    unsigned char use_laffc;              /* BATTY_LEGACY_COLLISION clears it, default=1 */
    unsigned char rng_perframe;           /* BATTY_RNG_PERFRAME, default=1 */
    unsigned long profile_auto_frames;    /* BATTY_PROFILE_AUTO_FRAMES, default=0 */
    unsigned char kinnock;                /* BATTY_KINNOCK, default=0 */
    unsigned char fast_holds;             /* BATTY_FAST_HOLDS, default=0 */
    unsigned char infinite_lives;         /* BATTY_INFINITE_LIVES, default=0 */
};
static DebugSwitches dbg = { 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0 };

/* While set the frame body does no physics; only P (toggle), ESC
 * (quit) and ENTER (advance) are acted on. */
static unsigned char paused = 0;
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
        unsigned long total = prof.bg_pit + prof.frame_pit + prof.hud_pit + prof.bricks_pit + prof.vga_pit;
        fprintf(f, "Profiling Report over %lu frames:\n", prof.frames);
        if (total > 0) {
            fprintf(f, "  paint_bg_to_buff:     %lu (%u%%)\n", prof.bg_pit, (unsigned)((prof.bg_pit * 100) / total));
            fprintf(f, "  paint_frame_to_buff:  %lu (%u%%)\n", prof.frame_pit, (unsigned)((prof.frame_pit * 100) / total));
            fprintf(f, "  HUD / Lives:          %lu (%u%%)\n", prof.hud_pit, (unsigned)((prof.hud_pit * 100) / total));
            fprintf(f, "  render_brick_band:    %lu (%u%%)\n", prof.bricks_pit, (unsigned)((prof.bricks_pit * 100) / total));
            fprintf(f, "  buff_to_vga:          %lu (%u%%)\n", prof.vga_pit, (unsigned)((prof.vga_pit * 100) / total));
        }
        fprintf(f, "  static rebuilds:      %lu\n", prof.static_rebuilds);
        fprintf(f, "  band rebuilds:        %lu\n", prof.band_rebuilds);
        fprintf(f, "  band rows rebuilt:    %lu\n", prof.band_rows);
        fprintf(f, "  band rebuild PIT:     %lu\n", prof.band_pit);
        fprintf(f, "  full dynamic frames:  %lu\n", prof.full_dynamic_frames);
        fprintf(f, "  ball-only frames:     %lu\n", prof.ball_only_frames);
        fprintf(f, "  ball-object frames:   %lu\n", prof.ball_object_frames);
        fprintf(f, "  ball block bat:       %lu\n", prof.blocked_by_bat);
        fprintf(f, "  ball block static:    %lu\n", prof.blocked_by_static);
        fprintf(f, "  ball block HUD:       %lu\n", prof.blocked_by_hud);
        fprintf(f, "  ball block objects:   %lu\n", prof.blocked_by_objects);
        fprintf(f, "  ball block bricks:    %lu\n", prof.blocked_by_bricks);
        fprintf(f, "  ball block balls:     %lu\n", prof.blocked_by_balls);
        fprintf(f, "  ball block bat FX:    %lu\n", prof.blocked_by_bat_fx);
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

/* Build frame_l1[] from the tape's sprites instead of loading a capture.
 *
 * This is LBE8B's pixel work, in its order, and it reproduces the blob
 * that used to be extracted from emulator screens byte for byte —
 * `test-frame-derivable` is the same construction in Python, checked
 * against the last captured copy.
 *
 * print_sprite_pix is a plain UNMASKED copy that walks UPWARD: the first
 * data row lands at the given y and later rows stack above it. Every
 * placement below is a bottom edge, not a top one. */
static void draw_border_sprite_up(unsigned char *dest, int dest_rows,
                                  unsigned int off, int x_byte, int y) {
    const int w = border_spr[off];
    const int h = border_spr[off + 1];
    int r, bx;
    for (r = 0; r < h; r++) {
        const int yy = y - r;
        if (yy < 0 || yy >= dest_rows) continue;
        for (bx = 0; bx < w; bx++) {
            const int col = x_byte + bx;
            if (col < 0 || col >= 32) continue;
            dest[yy * 32 + col] = border_spr[off + 2 + r * w + bx];
        }
    }
}

static void build_frame_from_sprites(void) {
    int cycle;
    for (cycle = 0; cycle < FRAME_CYCLES; cycle++) {
        unsigned char *base = frame_l1 + (unsigned int)cycle * FRAME_SIZE;
        unsigned char *top  = base;
        unsigned char *left = top + FRAME_TOP_PX;
        unsigned char *right = left + FRAME_SIDE_PX;
        const unsigned char *tile = bg_tile + cycle * BG_TILE_SIZE;
        int y, i, x_byte, placement;

        /* 1. the level's background texture, as LBE8B paints it first */
        for (y = 0; y < FRAME_TOP_H_PX; y++) {
            const int ty = (y & 15) * 2;
            for (i = 0; i < 32; i++)
                top[y * 32 + i] = tile[ty + (i & 1)];
        }
        /* 2. LBE8B_1's SEVENTH side placement, bold from y=$17. The
         *    other six are the side strips below. */
        draw_border_sprite_up(top, FRAME_TOP_H_PX, BSPR_SIDE_BOLD_L, 0, 0x17);
        draw_border_sprite_up(top, FRAME_TOP_H_PX, BSPR_SIDE_BOLD_R, 31, 0x17);
        /* 3. LBE8B_2's FIRST inner-outline band. inner_border_line_c has
         *    the lower three; this one starts 56 rows above them, at
         *    y=-6, so rows 0..21 here. BEFORE the top border, which
         *    overwrites rows 0..7 of it. */
        for (y = 0; y < 22; y++) {
            top[y * 32 + 1]  &= 0x7F;
            top[y * 32 + 30] &= 0xFE;
        }
        /* 4. the top border: eight sprites, x stepping $20 per turn */
        x_byte = 0;
        for (i = 0; i < 8; i++) {
            draw_border_sprite_up(top, FRAME_TOP_H_PX, border_top_seq[i],
                                  x_byte, 0x07);
            x_byte += border_spr[border_top_seq[i]];
        }
        /* 5. border_horizontal_addon, ANDed into row 8 bytes 1..30 */
        for (i = 0; i < 30; i++) top[8 * 32 + 1 + i] &= border_addon[i];

        /* The side strips, y 24..191. Placements 1..6 of the same loop:
         * bold from $BF, thin from $9F, each -56 per turn. */
        for (y = 0; y < FRAME_SIDE_H_PX; y++) {
            const int ty = ((y + FRAME_TOP_H_PX) & 15) * 2;
            left[y]  = tile[ty];        /* byte col 0 is even */
            right[y] = tile[ty + 1];    /* byte col 31 is odd  */
        }
        {
            int y_bold = 0xBF, y_thin = 0x9F;
            for (placement = 0; placement < 6; placement++) {
                const int bold = (placement % 2) == 0;
                const unsigned int lo = bold ? BSPR_SIDE_BOLD_L
                                             : BSPR_SIDE_THIN_L;
                const unsigned int ro = bold ? BSPR_SIDE_BOLD_R
                                             : BSPR_SIDE_THIN_R;
                const int base_y = bold ? y_bold : y_thin;
                const int h = border_spr[lo + 1];
                for (i = 0; i < h; i++) {
                    const int yy = base_y - i;
                    if (yy < FRAME_TOP_H_PX || yy >= PLAYFIELD_H) continue;
                    left[yy - FRAME_TOP_H_PX]  = border_spr[lo + 2 + i];
                    right[yy - FRAME_TOP_H_PX] = border_spr[ro + 2 + i];
                }
                if (bold) y_bold -= 56; else y_thin -= 56;
            }
        }
    }
}

/* Build level_attrs[] the way game_screen_draw_to_buffer does, instead
 * of loading a capture of the result.
 *
 * The band is the "every brick alive" state that paint_brick_band
 * re-bases from, so this runs LBE8B's and print_briks' ATTRIBUTE passes
 * in their order:
 *
 *   1. the level's bg_attr everywhere
 *   2. the frame sprites' own attr blocks — row 0 from the eight
 *      horizontal pieces, columns 0 and 31 from the seven side
 *      placements
 *   3. print_briks, via paint_bricks, which writes each live brick's
 *      colour and calls paint_shadow_row (brik_shadow) per row
 *   4. print_border_shadow last, which is what makes column 1 and row 1
 *      non-bright
 *
 * The brick colours the original's band carries are deliberately NOT
 * reproduced: the port repaints them at every level entry. See the
 * comment on step 3. */
static const unsigned char *border_attrs(unsigned int off, int *aw, int *ah) {
    const unsigned int after_px = off + 2u
        + (unsigned int)border_spr[off] * (unsigned int)border_spr[off + 1];
    *aw = border_spr[after_px];
    *ah = border_spr[after_px + 1];
    return &border_spr[after_px + 2];
}

static void build_level_attrs_from_data(void) {
    int lvl;
    for (lvl = 0; lvl < N_LEVELS; lvl++) {
        unsigned char *band = level_attrs + (unsigned int)lvl * ATTR_BAND_SIZE;
        const unsigned char bg = bg_attr_per_cycle[lvl & 3];
        int i, cr, cc, x_char, aw, ah, placement, y_bold, y_thin;

        memset(attr_buff, bg, ATTR_BUFF_SIZE);

        /* 2a. the top border's attr row, char row 0 */
        x_char = 0;
        for (i = 0; i < 8; i++) {
            const unsigned char *av = border_attrs(border_top_seq[i], &aw, &ah);
            for (cc = 0; cc < aw; cc++)
                attr_buff[0 * 32 + x_char + cc] = av[cc];
            x_char += aw;
        }
        /* 2b. the side placements' attrs, stacking UPWARD like the
         *     pixels. All seven: the last supplies char rows 0..2. */
        y_bold = 0xBF;
        y_thin = 0x9F;
        for (placement = 0; placement < 7; placement++) {
            const int bold = (placement % 2) == 0;
            const unsigned int lo = bold ? BSPR_SIDE_BOLD_L : BSPR_SIDE_THIN_L;
            const unsigned int ro = bold ? BSPR_SIDE_BOLD_R : BSPR_SIDE_THIN_R;
            const int base_y = bold ? y_bold : y_thin;
            const unsigned char *lav = border_attrs(lo, &aw, &ah);
            const unsigned char *rav = border_attrs(ro, &aw, &ah);
            for (i = 0; i < ah; i++) {
                cr = base_y / 8 - i;
                if (cr < 0 || cr >= ATTR_ROWS) continue;
                attr_buff[cr * 32 + 0]  = lav[i];
                attr_buff[cr * 32 + 31] = rav[i];
            }
            if (bold) y_bold -= 56; else y_thin -= 56;
        }

        /* 3. NO brick pass. The original's band is the "every brick
         *    alive" state and the capture recorded it that way, but the
         *    port repaints it: paint_brick_band calls paint_bricks at
         *    every level entry, which writes each live brick's colour
         *    and its shadow row over whatever the base held.
         *
         *    Measured, not assumed — dropping this leaves all 15 levels
         *    pixel-identical, while dropping the border shadow below
         *    breaks L01 by 1696 pixels. So the base band is the EMPTY
         *    playfield's attributes, and saying so here is worth more
         *    than a paint_bricks call whose output is overwritten. */

        /* 4. print_border_shadow ($BFCF), last */
        for (cr = 1; cr <= 23; cr++) attr_buff[cr * 32 + 1] &= 0xBF;
        for (cc = 2; cc <= 30; cc++) attr_buff[1 * 32 + cc] &= 0xBF;

        memcpy(band, attr_buff, ATTR_BAND_SIZE);
    }
}

static void paint_frame_to_buff(unsigned char cycle, unsigned char level_idx) {
    const unsigned char *base     = frame_l1 + (unsigned int)cycle * FRAME_SIZE;
    const unsigned char *top_px   = base;
    const unsigned char *left_px  = top_px  + FRAME_TOP_PX;
    const unsigned char *right_px = left_px + FRAME_SIDE_PX;
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

/* The original's 3 special slots — lives_indicator, score_indicator and
 * separator at $9BC2 / $9BD4 / $9BDC — are NOT part of the 11-slot
 * iteration, and are rendered here by their own paths. */

/* spr_separator ($7A2A), the Double Play court divider: 2 bytes wide,
 * $18 rows, loaded from its own asset because it sits just BELOW
 * SPRITES.BIN's range. object_separator ($9BDC) carries its position:
 *
 *   DEFB $03,$05,$7D,$00,$A9,...
 *         set  spr   x     y
 *
 * so sprite set 3 (gfx_screen_elements) index 5, at x=$7D=125,
 * y=$A9=169 — down in the bat band, not a full-height wall.
 *
 * LBE8B_10 draws it only for game_mode $02, immediately before the
 * 1UP/HI/2UP sprites, which is why this is called just above
 * render_hud_to_buff at both full-scene composers. */
static unsigned char separator_spr[98];

static void render_separator(void) {
    if (game_mode != 2) return;                 /* CP $02 / JR NZ */
    blit_masked_to_scr_buff(separator_spr, 0x7D, 0xA9);
}

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


/* ball.dx/ball.dy cache the PRIMARY ball's direction reduced to signs
 * (-1/0/+1). They are not physics — the motion is q8.8 in the object
 * descriptor — they are a summary two places read:
 *
 *   ball_lands_on_bat()      gates the bat hit on ball.dy > 0
 *   apply_multi_ball_bonus() derives the extras' launch dirs from them
 *
 * so anything that rewrites the primary's direction must refresh them.
 *
 * The deltas may be at ANY scale — q8.8 from dir_to_dxdy, or whole
 * pixels from primary_ball_set_velocity. Only their SIGN is taken.
 *
 * Hence the `from` parameter: two callers take an Object* that may be an
 * EXTRA ball — laffc_collision (reached from step_extra_ball) and
 * magnet_ball_frame (slots 1 and 2) — and refreshing the PRIMARY's cache
 * from an extra's direction is known-bugs.md #13. Passing the object
 * through makes that unexpressible at a call site. */
static void refresh_ball_motion_signs(const Object *from, int dx_q8, int dy_q8) {
    if (from != &objects[OBJ_BALL_1]) return;
    ball.dx = (dx_q8 < 0) ? -1 : (dx_q8 > 0 ? 1 : 0);
    ball.dy = (dy_q8 < 0) ? -1 : (dy_q8 > 0 ? 1 : 0);
}

static void primary_ball_set_velocity(int dx, int dy) {
    /* The cache holds SIGNS. Callers pass whatever reads naturally at
     * their site — +1/-BALL_SPEED at the two launch points, raw parsed
     * values from BATTY_REPLAY_BALL_VEL — so normalise HERE rather than
     * trusting each of them (known-bugs.md #14). Behaviour is unchanged:
     * the dir selection below uses only the signs of dx and dy, and so
     * do both readers of the cache. */
    refresh_ball_motion_signs(&objects[OBJ_BALL_1], dx, dy);
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

static void ball_launch_from_bat(int b) {
    unsigned char dir;
    int launch_offset = ball.stuck_offset_x[b] - 4;
    if (launch_offset < 0) launch_offset = 0;
    /* Original LA27E_15 derives the release direction from the stuck
     * bat offset, with $30 remapped to $34. The first movement step can
     * still resolve against the bat and produce the actual upward
     * trajectory; skipping that step was what made launch drift from
     * the Spectrum behavior. */
    dir = (unsigned char)((launch_offset + 0x24) & 0x3F);
    if (dir == 0x30) dir = 0x34;
    {   /* Not written in place: dir_to_dxdy yields q8.8 deltas, so
         * normalising ball.dx/ball.dy afterwards leaves the cache
         * holding non-sign values in between. Go through the one
         * function that owns it. */
        int dx_q8, dy_q8;
        dir_to_dxdy(dir, BALL_SPEED, &dx_q8, &dy_q8);
        /* Correct for every slot without a guard here:
         * refresh_ball_motion_signs already ignores anything that is not
         * the primary, because the dx/dy sign cache is the primary's
         * alone (known-bugs #13). For b > 0 this is a deliberate no-op,
         * not an oversight. */
        refresh_ball_motion_signs(&objects[b], dx_q8, dy_q8);
    }
    objects[b].dir = dir;
    objects[b].speed = BALL_SPEED;
    objects[b].x_coord_hi = 0;
    objects[b].y_coord_hi = 0;
}

static void record_primary_launch(void) {
    probe.last_launch.valid = 1;
    probe.last_launch.x = BALL_X;
    probe.last_launch.y = BALL_Y;
    probe.last_launch.dir = objects[OBJ_BALL_1].dir;
    probe.last_launch.speed = objects[OBJ_BALL_1].speed;
    if (probe.launch_frames != 0) {
        probe.launch_countdown = probe.launch_frames;
        probe.launch_active = 1;
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


/* Wall reflect for ANY ball object: flip the 6-bit dir about the X and/or
 * Y axis (the original's wall-bounce dir mapping). Shared by the primary
 * and (once unified) the multi-ball secondaries. */

static void ball_reflect_descriptor(int flip_x, int flip_y) {
    int dx_q8, dy_q8;
    object_reflect(*(&objects[OBJ_BALL_1]), flip_x, flip_y);
    dir_to_dxdy(objects[OBJ_BALL_1].dir, objects[OBJ_BALL_1].speed,
                      &dx_q8, &dy_q8);
    refresh_ball_motion_signs(&objects[OBJ_BALL_1], dx_q8, dy_q8);
}

/* orig: check_margins, which is three clamps and nothing else:
 *
 *   check_top_margin    y < $08              -> y = $08
 *   check_left_margin   x < $08              -> x = $08
 *   check_right_margin  (u8)(w + x) >= $F9   -> x = $F8 - w
 *
 * No direction change and no re-aim: LAA7D turns one step toward the
 * held target and re-picks only on ARRIVAL, so nothing here steers an
 * alien off a wall. It presses against the clamp, its dir eventually
 * reaches its target, it re-picks at random, and it leaves.
 *
 * The right clamp's ADD is 8-BIT and unguarded, so for the bird's
 * w = $18 the sum wraps at x >= $E8 and the clamp does not fire at all —
 * x in [$E1,$E7] clamps back to $E0, x >= $E8 escapes. Reproduced here
 * rather than fixed. It needs a jump of more than 2px past $E7 to reach,
 * which ordinary flight cannot do; a LAFFC snap is the only way in.
 *
 * PLAYFIELD_W is 256, so PLAYFIELD_W - 8 - w is exactly $F8 - w.
 *
 * There is no floor in check_margins. The bottom exit below is the
 * port's own and is left alone: returning false deactivates the alien
 * (bit 7 of sprite_set) and ends its frame. */
static bool enemy_check_margins(Object *o, int *nx, int *ny,
                                long *nx_q8, long *ny_q8) {
    if (*nx < 8) {                                  /* check_left_margin */
        *nx = 8;
        *nx_q8 = (long)*nx << 8;
    } else if ((unsigned char)((unsigned)o->w_body_px + (unsigned)*nx)
               >= 0xF9) {                           /* check_right_margin */
        *nx = 0xF8 - (int)o->w_body_px;
        *nx_q8 = (long)*nx << 8;
    }

    if (*ny >= PLAYFIELD_H) {
        o->sprite_set |= 0x80;
        return false;
    }
    if (*ny < 8) {                                  /* check_top_margin */
        *ny = 8;
        *ny_q8 = (long)*ny << 8;
    }
    return true;
}

static void bomb_appear(Object *o);     /* forward decl */

/* LAFFC as the ALIEN reaches it. `LAFFC_30` tests the object's
 * sprite_set and only $02 (the ball) takes the destroy-and-score path,
 * so an alien neither destroys the brick nor is destroyed by it. What it
 * does get is what `LAFFC_26`..`LAFFC_29` already did on the way in: the
 * direction reflected by change_direction, and the position snapped to
 * the struck cell's edge.
 *
 * The snap, though, is RECORDED in LAA7B and then UNDONE — LAFFC_30
 * falls through into LB1C3, whose `LD HL,$0000` is self-modified with
 * the position at LAFFC entry. So the alien stays where the move put it
 * and walks to the snapped position over the next few frames (LAA44,
 * ported as enemy_home_step), steering and colliding for none of them.
 *
 * y == 0 is the "no target" marker, so a snap to y == 0 would be
 * indistinguishable from no hit at all. The original has the same hole
 * and it is unreachable for the same reason: the brick band starts well
 * below y 0, so no face can snap an alien there.
 *
 * See notes/enemy-movement.md for the full trace. */
static void enemy_brick_reaction(Object *o, int nx, int ny) {
    const LaffcHit hit = laffc_sweep(BrickField(live_level), o->dir,
                                     o->w_body_px, o->h_body_px, nx, ny);
    if (!hit.hit) return;
    const BallBounce snap = laffc_bounce(hit, o->dir,
                                         o->w_body_px, o->h_body_px, nx, ny);
    o->dir = snap.dir;                      /* CALL change_direction */
    enemy_home_target.x = snap.x;           /* LD (LAA7B),HL */
    enemy_home_target.y = snap.y;
    enemy_repick_target_current(*o);        /* flag_2 -> LAA7D_1 */
}

static void handling_bird_obj(Object *o) {
    int dx_q8, dy_q8;
    long nx_q8, ny_q8;
    int nx, ny;
    /* The entry slide at $A9BC:
     *   LD A,(IX+$04); CP $08; JR NC,LA9BC_0; INC (IX+$04); RET
     * The alien spawns at Y=0 and slides down 8 px before starting its
     * horizontal traverse. */
    if (o->y_coord < 8) {
        o->y_coord++;
        return;
    }
    /* The original calls LAAD2 at the handler tail (LA902_3/LA9BC_3);
     * within-frame position doesn't matter, since rendering happens
     * after every handler has run. */
    object_step_animation(*(o));
    bomb_appear(o);
    /* Steer every 4 frames. The original gates on the GLOBAL counter_misc
     * (`LD A,(counter_misc); AND $03; CALL Z,LAA7D`), not a per-object
     * counter, so all enemies turn on the same global phase — hence
     * pit_frame_counter here rather than o->misc_12, whose phase would be
     * spawn-relative. LAA7D refreshes the target on arrival, so there is
     * no separate timer-based re-target. */
    /* The two-mode dispatch at LA9BC:
     *   LD HL,(LAA7B) / LD A,H / AND A / JR Z,LA9BC_1 / CALL LAA44
     * A latched brick-hit target REPLACES steering, movement and the
     * collision scan for as long as it lasts — the alien walks to the
     * position the hit snapped it to and does nothing else. Only H (the
     * y) is tested, so y == 0 is the "no target" marker.
     *
     * Nothing sets the target yet: the hit DETECTION half is still to
     * come (parity-gaps.md, "enemy vs bricks"), so this branch is
     * unreachable in-game today and enemy flight is byte-for-byte what
     * it was. The dispatch is here because it is the fork the original
     * has, and the walk it guards is tested host-side. */
    if (enemy_home_target.y != 0) {
        enemy_home_step(*o, enemy_home_target);
        return;
    }
    if (((unsigned long)pit_frame_counter & 0x03UL) == 0)
        enemy_turn_towards_target(*o);
    /* The original handling_bird calls LAD69 — the same motion routine
     * as the ball, so dir_to_dxdy is shared here too. */
    dir_to_dxdy(o->dir, o->speed, &dx_q8, &dy_q8);
    nx_q8 = ((long)o->x_coord << 8) + o->x_coord_hi + dx_q8;
    ny_q8 = ((long)o->y_coord << 8) + o->y_coord_hi + dy_q8;
    nx = (int)(nx_q8 >> 8);
    ny = (int)(ny_q8 >> 8);
    /* LA9BC_1 order: LAD69 (move) -> LAFFC -> check_margins. */
    enemy_brick_reaction(o, nx, ny);
    if (!enemy_check_margins(o, &nx, &ny, &nx_q8, &ny_q8)) return;
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

/* The bat's body sprite while it is normal width: the gun-mounted one
 * when LASER is up, stepping through four fire frames at two ticks each
 * off the countdown, and the plain body otherwise. All share
 * spr_bat_normal's 32 x 13 footprint. */
static unsigned int bat_body_sprite(int b) {
    const BatState &st = bats[BAT_SLOT(b)];
    if (objects[b].bonus_applied != 0x01) return SPR_BAT_NORMAL;
    if (st.fire_anim_ticks >= 7) return SPR_BAT_GUN_1;
    if (st.fire_anim_ticks >= 5) return SPR_BAT_GUN_2;
    if (st.fire_anim_ticks >= 3) return SPR_BAT_GUN_3;
    if (st.fire_anim_ticks >= 1) return SPR_BAT_GUN_4;
    return SPR_BAT_GUN;
}

/* Mid-resize, the bat is wider than its sprite. Stuff solid bits into
 * the gap on each side so buff_to_vga lights them in the background's
 * ink — there is no sprite for the in-between widths, only the normal
 * and big bodies. */
static void fill_bat_resize_sides(int b) {
    const int side_w = bats[BAT_SLOT(b)].extra_px;
    const int bat_x  = (int)objects[b].x_coord;
    const int bat_y  = (int)objects[b].y_coord;
    int row;
    if (side_w <= 0) return;
    for (row = 0; row < 8; row++) {
        const int yy = bat_y + 1 + row;
        int bx;
        if (yy < 0 || yy >= PLAYFIELD_H) continue;
        for (bx = bat_x - side_w; bx < bat_x; bx++) {
            if (bx >= 0 && bx < PLAYFIELD_W)
                scr_buff[yy * 32 + (bx >> 3)] |= (unsigned char)(0x80 >> (bx & 7));
        }
        for (bx = bat_x + BAT_W_BYTES * 8;
             bx < bat_x + BAT_W_BYTES * 8 + side_w; bx++) {
            if (bx >= 0 && bx < PLAYFIELD_W)
                scr_buff[yy * 32 + (bx >> 3)] |= (unsigned char)(0x80 >> (bx & 7));
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
/* Draw one bat. Every input comes from its object and its BatState, so
 * the same code serves both. */
static void render_bat_of(int b, unsigned char attr) {
    const int extra = bats[BAT_SLOT(b)].extra_px;
    const int bat_x = (int)objects[b].x_coord;
    unsigned int spr;
    int x, y, sprite_w;
    if (extra >= BAT_BIG_EXTRA_PX) {
        spr = SPR_BAT_BIG;
        x   = bat_x - BAT_BIG_EXTRA_PX;
        sprite_w = BAT_W_BYTES * 8 + 2 * BAT_BIG_EXTRA_PX;
    } else {
        spr = bat_body_sprite(b);
        x   = bat_x;
        sprite_w = BAT_W_BYTES * 8 + 2 * extra;
        fill_bat_resize_sides(b);
    }
    y = (int)objects[b].y_coord;
    /* Force bg attr on the interior playfield cells the bat covers, but
     * leave the side-frame attr cells alone. The sprite pixels still
     * OR into the frame at the extremes; only the static tube colour
     * must stay owned by paint_frame_to_buff. */
    blit_sprite_attrs_to_buff_clipped(x - extra, y,
                                      sprite_w, 13, attr,
                                      8, PLAYFIELD_W - 8);
    blit_masked_to_scr_buff(spr, x, y);
}

static void render_bat(unsigned char cycle, unsigned char attr) {
    (void)cycle;
    render_bat_of(OBJ_BAT_1, attr);
}

/* The second bat. `all_var_init` (LB7F8) activates it and moves BOTH
 * bats when game_mode is $02:
 *
 *   LD A,(game_mode) / CP $02 / JR NZ,LB7F8_1
 *   LD A,$01 / LD (object_bat_2),A          ; sprite_set: activate
 *   LD A,$38 / LD (object_bat_1+$02),A      ; bat 1 x = 56
 *   LD A,$B0 / LD (object_bat_2+$02),A      ; bat 2 x = 176
 *
 * so Double Play does not add a bat beside the existing one — it moves
 * bat 1 left and puts bat 2 on the right, one per half. */
static void render_bat_2(unsigned char attr) {
    if (game_mode != 2 || !object_active(objects[OBJ_BAT_2])) return;
    render_bat_of(OBJ_BAT_2, attr);
}

static int bat_draw_extra_for_bounds(int extra) {
    return (extra >= BAT_BIG_EXTRA_PX) ? BAT_BIG_EXTRA_PX : extra;
}

static void bat_sprite_bounds(int x, int extra, int *x0, int *x1) {
    int e = bat_draw_extra_for_bounds(extra);
    *x0 = x - e;
    *x1 = x + BAT_W_BYTES * 8 + e;
}

static void remember_bat_draw_state_of(int b) {
    BatState &st = bats[BAT_SLOT(b)];
    objects[b].prev_x   = objects[b].x_coord;   /* = last DRAWN x */
    st.drawn_extra_px   = st.extra_px;
    st.drawn_y          = (int)objects[b].y_coord;
    st.drawn_bonus      = objects[b].bonus_applied;
    st.drawn_fire_ticks = st.fire_anim_ticks;
}

static void remember_bat_draw_state(void) {
    remember_bat_draw_state_of(OBJ_BAT_1);
    if (game_mode == 2) remember_bat_draw_state_of(OBJ_BAT_2);
}

/* Both bats, wherever the bat band is drawn. `render_bat_2` early-outs
 * outside mode $02, so this is safe on every path. Drawing bat 2 only
 * from compose_level_scene (level entry) leaves its sprite behind while
 * the object steers away, and the probe-reading gates cannot see that. */
static void render_bats(unsigned char cycle, unsigned char attr) {
    render_bat(cycle, attr);
    render_bat_2(attr);
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

/* Walk the dot one step along the bat and turn it round at the ends.
 * The travel runs between 9 and bat_w - 10, so the dots stay inside the
 * body cap.
 *
 * This MUTATES animation state from inside a render call, which is the
 * shape that made known-bugs #10 a bug for bullets. It is safe here only
 * because the frame loop's redraw dispatch is mutually exclusive — see
 * redraw_frame — so exactly one path draws the bat each frame. Call both
 * in one frame and the dot moves at double speed. */
static void advance_run_dot(int frame, int bat_w) {
    if (run_dot_frame & 0x80) {
        frame--;
        if (frame <= 9) {
            run_dot_frame = 9;                               /* now increasing */
        } else {
            run_dot_frame = (unsigned char)(0x80 | frame);
        }
        return;
    }
    frame++;
    if (frame >= bat_w - 10) {
        run_dot_frame = (unsigned char)(0x80 | (bat_w - 10));  /* now decreasing */
    } else {
        run_dot_frame = (unsigned char)frame;
    }
}

static void render_running_dot(void) {
    int bat_w, bat_left;
    int frame;
    /* Test-mode determinism: the GT was captured after exactly one
     * gameplay-loop iter (PC=0xBB61), so the original's running_dot
     * punched at frame=0x0E. Our test reaches the screendump after many
     * iters during which run_dot_frame would have advanced and the dots
     * would land elsewhere. Pin it so the dots stay where the GT has
     * them. Same trick as test_mode_pin_blink. */
    if (test_mode_pin_blink) run_dot_frame = 0x0E;
    /* Bat "logical width" per object_bat_1+$0C = $1C = 28 px. The sprite
     * is 32 px wide, but running_dot uses this narrower W for the
     * mirror-position calc, so the dots land inside the body cap rather
     * than at the tapered sprite edges. */
    if (bat1.extra_px >= BAT_BIG_EXTRA_PX) {
        bat_w    = 28 + 2 * BAT_BIG_EXTRA_PX;   /* 44 px in big-bat mode */
        bat_left = BAT_X - BAT_BIG_EXTRA_PX;
    } else {
        bat_w    = 28 + 2 * bat1.extra_px;
        bat_left = BAT_X - bat1.extra_px;
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
    advance_run_dot(frame, bat_w);
}

/* Port of LBE8B_8's `ADD A,$10; CP $E9; JR NC` cap: the indicator stops
 * advancing X once it would land at $E9 (= 233), giving at most 14
 * distinct sprite slots between $08 and $D8. */
#define LIVES_DYNAMIC_MAX 14
static void render_lives(unsigned char cycle, unsigned char attr) {
    /* Port of LBE8B_7's `LD A,(lives_1up); DEC A; JR Z,skip`: draw
     * (lives - 1) indicator bats, so on the last life the meter is
     * empty. */
    int show = player.lives - 1;
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









/* Assumes paint_bg_to_buff already pre-filled the buffers, and does NOT
 * touch VGA — buff_to_vga handles the final pass. */
static void print_border_shadow_c(void);
static void dim_border_shadow_column(int cr0, int cr1);
static void render_brick_band(unsigned char level_idx) {
    if (level_idx >= N_LEVELS) return;
    paint_brick_band(live_level,
                     &level_attrs[(int)level_idx * ATTR_BAND_SIZE],
                     bg_attr_per_cycle[level_idx & 3]);
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
    if (level_idx >= N_LEVELS) return;
    paint_brick_band_rows(live_level,
                          &level_attrs[(int)level_idx * ATTR_BAND_SIZE],
                          bg_attr_per_cycle[level_idx & 3],
                          r0, r1, cr0, cr1);
    dim_border_shadow_column(cr0, cr1);
}

/* Port of the inner-border-line routine at LBE8B_2 ($BE99), adjusted to
 * the port's combined frame asset. The original clears y=0..21 before
 * it draws the top border, so those top-frame pixels are restored later.
 * Since paint_frame_to_buff() already includes the top border, the net
 * visible effect here starts at the lower three bands only. */
#define INNER_BORDER_BANDS   3
#define INNER_BORDER_BAND_H  28
static const int inner_border_band_y0[INNER_BORDER_BANDS] = { 50, 106, 162 };

static bool in_inner_border_band(int y) {
    int i;
    if (y < 0 || y >= PLAYFIELD_H) return false;
    for (i = 0; i < INNER_BORDER_BANDS; i++) {
        const int y0 = inner_border_band_y0[i];
        if (y >= y0 && y < y0 + INNER_BORDER_BAND_H) return true;
    }
    return false;
}

/* The two pixel columns the border line blacks out — x=8 and x=247, the
 * inner edges of the frame's side strips. Split per side because a
 * window repaint may reach one byte and not the other. */
static void black_inner_border_left(int y) {
    scr_buff[y * 32 + 1] &= 0x7F;    /* leftmost bit of byte 1 -> x=8 */
}

static void black_inner_border_right(int y) {
    scr_buff[y * 32 + 30] &= 0xFE;   /* rightmost bit of byte 30 -> x=247 */
}

static void black_inner_border_pixels(int y) {
    black_inner_border_left(y);
    black_inner_border_right(y);
}

static void inner_border_line_c(void) {
    int y;
    for (y = 0; y < PLAYFIELD_H; y++) {
        if (in_inner_border_band(y)) black_inner_border_pixels(y);
    }
}

/* Re-apply the border line inside a window that was just repainted from
 * the background tile. paint_bg_window_to_buff lays down the plain tile,
 * which does not carry these two columns, so any repaint whose byte
 * range reaches byte 1 or byte 30 must put them back — otherwise the
 * line disappears wherever the sprite above it happens to be
 * transparent. See known-bugs.md #11. */
static void restore_inner_border_line(int y0, int h, int byte_lo, int byte_hi) {
    const int left  = (byte_lo <= 1  && byte_hi >= 1);
    const int right = (byte_lo <= 30 && byte_hi >= 30);
    int y;
    if (!left && !right) return;                  /* window misses both */
    for (y = y0; y < y0 + h; y++) {
        if (!in_inner_border_band(y)) continue;
        if (left)  black_inner_border_left(y);
        if (right) black_inner_border_right(y);
    }
}

/* Dim char column 1 over [cr0, cr1] — the left arm of the border
 * shadow. Split out because a scoped band rebuild has to re-apply it for
 * the rows it repainted, exactly as restore_inner_border_line does for
 * the border line. */
static void dim_border_shadow_column(int cr0, int cr1) {
    int cr;
    for (cr = cr0; cr <= cr1; cr++) attr_buff[cr * 32 + 1] &= 0xBF;
}

/* The frame's drop shadow: the whole left column and the top row. It
 * spans the playfield, not the brick band, which is why it stays here
 * rather than in the bricks module.
 *
 * Clears bit 6 (bright) at char col 1 of rows 1..23 and at row 1 cols
 * 2..30. Without it, bricks at lvl_col=0 leak their bright colour into
 * col 1 — L1 char row 7 col 1 should be $1F, and print_briks writes $5F.
 * orig: print_border_shadow $BFCF */
static void print_border_shadow_c(void) {
    int cc;
    dim_border_shadow_column(1, 23);
    for (cc = 2; cc <= 30; cc++) attr_buff[1 * 32 + cc] &= 0xBF;
}

/* Pre-fill scr_buff with the hex tile and attr_buff uniformly with the
 * level's bg_attr, across the WHOLE 256x192 playfield. buff_to_vga does
 * the final pixel expansion using the (possibly overwritten) attrs. */
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

static void paint_bg_window_to_buff(unsigned char attr, unsigned char cycle,
                                    int y0, int h, int byte_lo, int byte_hi);

static unsigned char bg_scr_buff[6144];
static unsigned char bg_attr_buff[768];
/* The static background cache: the level scene with no moving objects,
 * held in bg_scr_buff/bg_attr_buff so a dirty frame can restore from it
 * instead of repainting. */
struct StaticCache {
    int bg_dirty;             /* the whole cache needs rebuilding */
    int band_dirty;           /* only the brick band does */
    /* Brick rows changed since the last band build, so the rebuild can
     * re-composite just those. Default spans the whole band. */
    int brick_row_lo;
    int brick_row_hi;
    int full_flush;           /* push every row to VGA this frame */
    /* What the cache currently shows. The HUD skips its redraw while
     * these still match the live values. */
    /* One entry per HUD score slot, in the order render_hud_to_buff
     * draws them: 1UP, 2UP. The dirty test must cover what is DRAWN, not
     * what the active player happens to hold — with two players the 2UP
     * slot changes while `player` does not, and the HUD would keep
     * showing a stale number. */
    unsigned long drawn_score[2];
    unsigned long drawn_high_score;
    int drawn_lives;
};
static StaticCache cache = {
    1, 0, 0, LVL_ROWS - 1, 1, 0xFFFFFFFFUL, 0xFFFFFFFFUL, -1
};

/* Set up the level's runtime magnet state — the state half of
 * print_magnets ($8D4C). Coordinates from magnets_per_level; the
 * initial ON/OFF coin is the original's per-magnet
 * `CALL random_generate / LD A,(random_number) / RRA / JR C,stay-ON`:
 * ADVANCES the RNG once per magnet, keeps the magnet ON when bit0==1.
 * The coin is rolled once HERE; render_magnets and the toggle only read
 * the state, so a per-magnet coin is not re-sampled at render time.
 *
 * Test-mode pin (BATTYALL): slots 0/1 ON, 2/3 OFF, no RNG consumed —
 * keeps the state4 level-entry captures deterministic. */
static void magnet_level_init(unsigned char level_idx) {
    const unsigned char *rec;
    int i;
    magnets.count = 0;
    magnets.toggle_pending = MAGNET_TOGGLE_NONE;
    for (i = 0; i < 3; i++) {
        ball.mag_cool[i] = 0;
        ball.mag_delta[i] = 0;
        ball.mag_exit[i] = 0;
        ball.mag_idx[i] = 0;
    }
    for (i = 0; i < MAGNETS_MAX_PER_LEVEL; i++) magnets.on_state[i] = 0;
    if (level_idx >= N_LEVELS) return;
    rec = magnets_per_level[level_idx];
    magnets.count = rec[0];
    for (i = 0; i < magnets.count; i++) {
        magnets.px[i] = rec[1 + 2*i];
        magnets.py[i] = rec[1 + 2*i + 1];
        magnets.on_state[i] = test_mode_pin_blink
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
                for (i = 0; i < magnets.count; i++)
                    magnets.on_state[i] = (unsigned char)((v >> i) & 1);
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
    for (i = 0; i < magnets.count; i++) {
        int x = magnets.px[i];
        int y = magnets.py[i];
        /* Draw order matches the original's print_magnets ($8D4C):
         *   sprite_num $06 = spr_magnet_circle_ON (lightning, w=4, h=30
         *                    with SMC) — drawn UNCONDITIONALLY first.
         *   sprite_num $07 = spr_magnet_circle_OFF (bare outline, w=3,
         *                    h=23) — drawn CONDITIONALLY for an OFF slot.
         * gfx_screen_elements maps $06 -> spr_magnet_circle_ON and $07 ->
         * spr_magnet_circle_OFF, which is the opposite of what the names'
         * draw order suggests.
         *
         * Both blits use the SAME (x, y) — original's `ADD A,$05` to
         * (IX+$04) between calls is dead state since ix_buf_addr_calc
         * only runs once. Note this means the ON sprite's bottom "spark"
         * rows (23..29) are painted under BOTH states and persist
         * regardless of later toggles, exactly like the original (the
         * toggle redraw is 23 rows tall — circle only). */
        blit_masked_to_scr_buff(spr_magnet_on, x, y);
        if (!magnets.on_state[i]) {
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
    if (magnets.count == 0) return;
    b = (unsigned char)(magnets.count - 1);
    do {
        a = (unsigned char)(random_lo(next_random()) & 0x03);
    } while (a > b);
    magnets.on_state[a] ^= 1;
    magnets.toggle_pending = a;
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
    unsigned char i = magnets.toggle_pending;
    int x, y, yy;
    int byte_lo, byte_hi;
    if (i == MAGNET_TOGGLE_NONE) return;
    magnets.toggle_pending = MAGNET_TOGGLE_NONE;
    if (i >= magnets.count) return;
    x = magnets.px[i];
    y = magnets.py[i];
    if (magnets.on_state[i]) {
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

/* The level scene, in the original's paint order at $BE8B.
 *
 * Score labels/digits go down immediately before the magnets, and the
 * magnets before the bricks (`CALL print_magnets; CALL print_briks`).
 * Both orderings matter on levels where the magnets overlap HUD rows:
 * magnets may overwrite the score area but not the reverse, and the
 * brick top row must overwrite the magnets' lower shadow rows — invert
 * it and those shadow rows punch through the brick tops. */
static void compose_level_scene(unsigned char level_idx, bool with_bat) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle   = (unsigned char)(level_idx & 3);

    paint_bg_to_buff(bg_attr, cycle);
    paint_frame_to_buff(cycle, level_idx);
    if (with_bat) render_bat(cycle, bg_attr);
    /* NOT gated on with_bat: bat 2 has no input device yet (WS3), so it
     * is scenery and belongs in the static cache. Bat 1 is a moving
     * object and the dirty path redraws it each frame. */
    render_bat_2(bg_attr);
    render_lives(cycle, bg_attr);
    if (with_bat) remember_bat_draw_state();
    render_separator();
    render_hud_to_buff();
    render_magnets(level_idx);
    inner_border_line_c();
    render_brick_band(level_idx);
    restore_top_frame_center(cycle, level_idx);
}

static void render_level_screen_static(unsigned char level_idx) {
    compose_level_scene(level_idx, false);
}

static void build_static_background(unsigned char level_idx) {
    prof.static_rebuilds++;
    render_level_screen_static(level_idx);
    memcpy(bg_scr_buff, scr_buff, sizeof(bg_scr_buff));
    memcpy(bg_attr_buff, attr_buff, sizeof(bg_attr_buff));
    cache.band_dirty = 0;
}

/* The whole band is dirty: repaint it and recapture, the proven path.
 *
 * The rebuild rewrites scr_buff/attr_buff well beyond any small dirty
 * rect — whole rows, the shadow attrs on the row below, the 32-byte
 * attr rows — so every pixel row of every touched attr cell is flushed.
 * Marking only the rect leaves the rest stale on VGA, which is what
 * known-bugs #1's post-destroy leftovers were. */
static void rebuild_band_cache_full(unsigned char level_idx,
                                    unsigned char bg_attr, unsigned char cycle) {
    int y, cr;
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
    mark_dirty_bytes(3 * 8, (16 - 3 + 1) * 8, 0, 31);
    prof.band_rows += 14;
}

/* Only some brick rows changed. Re-composite [R0, R1] — the dirty rows
 * widened by one on each side, so every attr and pixel the window
 * inherits from a neighbouring row is re-derived rather than left stale
 * (see render_brick_band_rows' boundary notes).
 *
 * The pixel window is those rows' bodies plus R1's bottom-edge row. The
 * shared top-edge row (31 + 8*R0) is deliberately NOT bg-erased: print
 * re-zeros it only under live R0 bricks, and that is the canonical
 * content. */
static void rebuild_band_cache_rows(unsigned char level_idx,
                                    unsigned char bg_attr, unsigned char cycle,
                                    int lo, int hi) {
    int y, cr;
    /* The paint window and the capture window differ by one pixel row at
     * the top, and which one it is depends on R0 — band_rebuild_window
     * owns that rule and tests/test_bricks.cpp gates it. */
    int R0, R1, py_cap0, py0, py1, cr0, cr1;
    band_rebuild_window(lo, hi, &R0, &R1, &py_cap0, &py0, &py1, &cr0, &cr1);
    paint_bg_window_to_buff(bg_attr, cycle, py0, py1 - py0 + 1, 1, 30);
    /* The bg repaint erased the border line; put it back before the
     * bricks, which then overwrite it — the canonical order. */
    restore_inner_border_line(py0, py1 - py0 + 1, 1, 30);
    render_brick_band_rows(level_idx, R0, R1, cr0, cr1);
    /* Capture from the shared top-edge row down (print touches it). */
    for (y = py_cap0; y <= py1; y++) {
        memcpy(&bg_scr_buff[(y << 5) + 1], &scr_buff[(y << 5) + 1], 30);
    }
    for (cr = cr0; cr <= cr1; cr++) {
        memcpy(&bg_attr_buff[cr << 5], &attr_buff[cr << 5], 32);
    }
    /* Flush every pixel row of every recomposited attr cell, plus
     * the shared top-edge pixel row (same rule as the full branch). */
    mark_dirty_bytes(py_cap0, (cr1 * 8 + 7) - py_cap0 + 1, 0, 31);
    prof.band_rows += (unsigned long)(cr1 - cr0 + 1);
}

static void build_static_brick_band_cache(unsigned char level_idx) {
    const unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    const unsigned char cycle   = (unsigned char)(level_idx & 3);
    const int lo = cache.brick_row_lo;
    const int hi = cache.brick_row_hi;
    const unsigned short t0 = pit_current_ticks();
    unsigned short t1;

    if (dbg.full_band_rebuild || (lo <= 0 && hi >= LVL_ROWS - 1)) {
        rebuild_band_cache_full(level_idx, bg_attr, cycle);
    } else {
        rebuild_band_cache_rows(level_idx, bg_attr, cycle, lo, hi);
    }

    t1 = pit_current_ticks();
    prof.band_pit += (t1 <= t0) ? (unsigned long)(t0 - t1)
                                : (unsigned long)((t0 - t1) + 23864u);
    prof.band_rebuilds++;
    cache.band_dirty = 0;
}

/* Everything on screen is suspect — rebuild the static cache whole and
 * push every row. The one caller is the end of the bat explosion; the
 * reasoning for why it cannot be left implicit is there. */
static void invalidate_static_cache_after_death(void) {
    cache.bg_dirty = 1;
    cache.full_flush = 1;
}

static void mark_static_bg_cache_dirty(void) {
    /* Whole-band dirty (level entry, rocket clear, multi-cell changes). */
    cache.brick_row_lo = 0;
    cache.brick_row_hi = LVL_ROWS - 1;
    cache.band_dirty = 1;
}

/* Mark a single brick row dirty, unioning into the pending range. Lets the
 * band-cache rebuild scope to just the rows a brick hit touched. */
static void mark_brick_row_dirty(int row) {
    if (!cache.band_dirty) {
        cache.brick_row_lo = row;
        cache.brick_row_hi = row;
    } else {
        if (row < cache.brick_row_lo) cache.brick_row_lo = row;
        if (row > cache.brick_row_hi) cache.brick_row_hi = row;
    }
    cache.band_dirty = 1;
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
    for (y = 0; y < HUD_PATCH_H_PX; y++) {
        memcpy(&bg_scr_buff[y << 5], &scr_buff[y << 5], 32);
    }
    for (cr = 0; cr < FRAME_TOP_H_PX / 8; cr++) {
        memcpy(&bg_attr_buff[cr << 5], &attr_buff[cr << 5], 32);
    }
}

static void render_level_screen(unsigned char level_idx) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle   = (unsigned char)(level_idx & 3);

    cache.bg_dirty = 1;
    cache.band_dirty = 0;
    clear_dirty_ranges(prev_dirty_min_byte, prev_dirty_max_byte);
    cache.drawn_score[0] = 0xFFFFFFFFUL;
    cache.drawn_score[1] = 0xFFFFFFFFUL;
    cache.drawn_high_score = 0xFFFFFFFFUL;
    cache.drawn_lives = -1;

    fill(0, 0, SCREEN_W, SCREEN_H, COL_BORDER);
    draw_frame(10);              /* bright red — placeholder */
    compose_level_scene(level_idx, true);
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

#define SC_SPACE    0x39
#define SC_LEFT     0x4B    /* arrow / keypad 4 */
#define SC_RIGHT    0x4D    /* arrow / keypad 6 */

/* --- Double Play: two players, one keyboard ---------------------------
 *
 * The original splits the keyboard down the middle so two people can sit
 * at one Spectrum. `get_left_player_ctrl_state` ($A161) reads the ASDFG
 * half-row for player 1 and `get_right_player_ctrl_state` ($A19E) reads
 * the HJKL+Enter half-row for player 2:
 *
 *     $FDFE  A S D F G    AND $05 -> A,D = LEFT   AND $0A -> S,F = RIGHT
 *     $BFFE  Ent L K J H  AND $0A -> J,L = LEFT   AND $05 -> K,Ent = RIGHT
 *
 * with ONE key dropped: Enter. See the bat-2 reader below.
 *
 * The interleaving is not a typo. Each direction gets two keys either
 * side of the other direction's, so the cluster works whichever way a
 * player rests their hand.
 *
 * These are LETTERS, so they transcribe to a PC keyboard unchanged —
 * unlike the device list in WS1, which is Spectrum hardware and has to
 * be adapted rather than ported. The one addition is that P1's arrow
 * keys keep working in Double Play. The original takes them away (mode
 * $02 forces both players onto the split keyboard); on a PC the arrows
 * sit next to the numpad, far from HJKL, so leaving them live costs
 * player 2 nothing and spares player 1 a mode-dependent control change.
 * A superset, and a deliberate one.
 *
 * Both readers bail to the standard per-device poll unless BOTH players
 * are on ctrl_type 0. That gate is not reproduced yet: nothing selects a
 * device (WS1), so ctrl_type is 0 for both by construction. */
#define SC_A        0x1E
#define SC_S        0x1F
#define SC_D        0x20
#define SC_F        0x21
#define SC_J        0x24
#define SC_K        0x25
#define SC_L        0x26

/* Player 2's FIRE cluster. The original reads it as one half-row pair,
 * $5FFE = $7FFE | $DFFE, which is Y U I O P together with B N M, SYMBOL
 * SHIFT and SPACE — `AND $1F` over the combined read, so any of them
 * fires.
 *
 * SPACE is the one key not carried over: this port committed it to
 * player 1's fire long before Double Play existed, and `test-visual`
 * and `test-normal-ball-launch` both press it. Same call as dropping
 * ENTER from bat 2's RIGHT — transcribe the cluster, minus the key the
 * port has already spent. */
#define SC_Y        0x15
#define SC_U        0x16
#define SC_I        0x17
#define SC_O        0x18
#define SC_P        0x19
#define SC_B        0x30
#define SC_N        0x31
#define SC_M        0x32

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

/* The original has no attract auto-cycle: the title, menu and hi-score
 * screens wait for a key rather than rotating on a timer. Nothing sets
 * this, so every TIMED_OUT below is permanently false — the timeouts are
 * kept because they are the cycle the original's screens WOULD use. Do
 * not read a TIMED_OUT branch as reachable. */
static int auto_advance = 0;   /* never assigned; see above */
#define TIMED_OUT(start, ticks) (auto_advance && (bios_ticks() - (start) > (ticks)))

/* Blink phase for the selected option's text on the MENU. Test mode
 * pins it to 0 (BLACK / invisible) so the screendump matches snap2's
 * captured BLACK half deterministically. `make run` uses real-time
 * bios_ticks so the user sees the actual blink. */
static int blink_phase(void) {
    if (test_mode_pin_blink) return 0;
    /* PIT, not BIOS. bios_ticks() does not advance during gameplay —
     * measured, not assumed: a probe latching both clocks at frame 1 and
     * again at the checkpoint reported dbios0 over dpit678, i.e. ~13.6 s
     * of 50 Hz frames with the BIOS counter frozen (known-bugs.md #15).
     * With BIOS frozen this returned a constant and nothing blinked.
     * 2 BIOS ticks at 18.2 Hz is 0.110 s, so 6 PIT frames at ~50 Hz is
     * the same 4.5 Hz half-period the original had. */
    return (int)((pit_ticks() / 6UL) & 1UL);
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
            /* orig main_menu tail ($93F8_10):
             *   LD A,$EF / CALL in_a_fe / AND $01 / RET NZ
             * in_a_fe CPLs the port read, so bits are active HIGH after
             * it and `RET NZ` means "key 0 IS pressed -> leave the menu
             * and start the game". Key 0 is the ONLY way out; every
             * other key falls back into the poll loop.
             *
             * ENTER is the port's own attract-chain affordance — not in
             * the original at all — because test-visual walks title ->
             * menu -> hi-score -> level with it. Making ENTER start the
             * game turns that gate's hi-score checkpoint into a level
             * capture. */
            if (k == '0') {
                /* The original's game_mode is always one of the three;
                 * the port's 0 means "nothing picked yet". */
                if (selected_mode == 0) selected_mode = 1;
                game_mode = game_mode_from_selection(selected_mode);
                return ST_LEVEL;
            }
            if (k == '\r') return ST_HISCORE;    /* attract chain */
            /* Anything else: the original ignores it. */
            continue;
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

/* Paint the ball into scr_buff via the original masked OR-blit. Cell
 * attrs are NOT overridden, so there is no colour-clash halo: the body
 * bits (mask=1, pix=0) render as the cell's ink and the texture/shadow
 * bits (mask=1, pix=1) as its paper, as on the Spectrum. */
static void render_ball_to_buff(int x, int y, unsigned char bg) {
    unsigned int spr = big_ball_active() ? SPR_BIG_BALL : SPR_BALL_NORMAL;
    (void)bg;
    blit_masked_to_scr_buff(spr, x, y);
}

/* Paint each active laser bullet via the original masked OR-blit, no
 * per-cell attr override. The original's print_obj_to_buff writes pixels
 * only — the bullet renders in whichever attr each cell already has.
 *
 * Each bullet draws its OWN animation frame. Sharing one static counter
 * bumped per call makes the phase depend on how many frames took the
 * full redraw path — known-bugs.md #10. */
static void render_bullet_to_buff(void) {
    int i;
    for (i = 0; i < N_BULLETS; i++) {
        if (!bullet_active[i]) continue;
        blit_masked_to_scr_buff((bullet_frame[i] & 1) ? SPR_BULLET_2 : SPR_BULLET_1,
                                bullet_x[i], bullet_y[i]);
    }
}

/* Paint the rocket into scr_buff, alternating the two
 * spr_bonus_rocket_* frames for the flame flicker. */
static void render_rocket_to_buff(void) {
    unsigned int spr;
    if (!rocket.active) return;
    /* Original handling_rocket at $A89A toggles the sprite EVERY frame:
     *   LD A,(counter_misc); AND $01; LD (IX+$01),A */
    spr = (rocket.counter & 1) ? SPR_BONUS_ROCKET_2 : SPR_BONUS_ROCKET_1;
    blit_masked_to_scr_buff(spr, rocket.x, rocket.y);
}

static unsigned int current_rocket_spr(void) {
    return (rocket.counter & 1) ? SPR_BONUS_ROCKET_2 : SPR_BONUS_ROCKET_1;
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
    unsigned int spr = spr_for_bonus(bonus.type);
    (void)bg;
    blit_masked_to_scr_buff(spr, bonus.x, bonus.y);
}

static void set_rocket_bonus_sprite_height(unsigned char height) {
    /* Original startup/all_var_init stores $0C into spr_bonus_rocket_1+1
     * so the falling "next level" bonus is just the compact rocket pack.
     * get_rocket patches the same byte to $1B when it attaches to the
     * bat for the level-clear flight. */
    sprites_blob[SPR_BONUS_ROCKET_1 + 1] = height;
}

/* Put an extra ball on top of the primary, travelling in `dir` at the
 * primary's speed. The extras read dir/speed/q8.8 from their object;
 * the original copies ball_1's speed and a derived direction. */
static void spawn_extra_ball(unsigned char slot,
                             unsigned char x, unsigned char y,
                             unsigned char dir) {
    Object *o = &objects[slot];
    o->sprite_set = 0x02;
    o->x_coord    = x;
    o->y_coord    = y;
    o->dir        = dir;
    o->speed      = objects[OBJ_BALL_1].speed;
    o->x_coord_hi = 0;
    o->y_coord_hi = 0;
    /* The extras come out of the primary at the primary's position, so
     * they start owned by whoever owned it. The original does not copy
     * anything here — `+$12` is part of the object and LA67B_8 spawns
     * into slots that already carry a bit — but inheriting is the only
     * reading that does not credit a brick to a player who never
     * touched the ball that broke it. */
    ball_owner_side[slot] = ball_owner_side[BALL_PRIMARY];
}

/* An extra ball's liveness is recorded twice: the flag the step loop
 * reads, and bit 7 of the object's sprite_set, which is what the
 * compositor reads. Clear one without the other and you get a ball that
 * is drawn but never stepped, or stepped but never drawn. */
static void hide_extra_balls(void) {
    ball.extra2_active = 0;
    ball.extra3_active = 0;
    objects[OBJ_BALL_2].sprite_set = 0x82;
    objects[OBJ_BALL_3].sprite_set = 0x82;
}

/* The rocket clear runs with only the caught bat and the rocket alive —
 * everything else vanishes rather than carrying on underneath it.
 * orig: LBAED_6 */
static void hide_objects_for_rocket_clear(void) {
    int i;
    BALL_HIDE();
    ball.stuck[BALL_PRIMARY] = 0;
    hide_extra_balls();
    objects[OBJ_ENEMY].sprite_set = 0;
    bomb.active = 0;
    pts_marker.active = 0;
    for (i = 0; i < N_BULLETS; i++) {
        bullet_active[i] = 0;
        bullet_blast_ticks[i] = 0;
    }
}

/* The CATCHING bat's active bonus. $FF means "no bat-side effect".
 *
 * Writes ONE bat. The original keeps the two bytes deliberately APART —
 * `LA67B_0` runs inside `bonus_flag_swap` for a bat-2 catch, so only the
 * catching bat's byte moves.
 *
 * Where an effect belongs to the BALL rather than to a bat, the original
 * checks both instead. LA27E's big-ball test is the model:
 *
 *     LD A,(object_bat_1+$14) / CP $07 / JR Z,set_big_ball
 *     LD A,(object_bat_2+$14) / CP $07 / JR NZ,obj_processing
 *
 * and the expiry a few lines later clears each byte independently.
 * `big_ball_active` follows that shape. */
static void set_bat_bonus(int bat_idx, unsigned char code) {
    objects[bat_idx].bonus_applied = code;
}

/* Sit the rocket on the bat, ready to fly: x = bat_x + 4, or + 12 when
 * the bat is big, and y = bat_y + 6 — inside the body, which works
 * because the sprite is masked and the bat shows through.
 * orig: get_rocket $AA9D */
static void place_rocket_on_bat(void) {
    rocket.x = BAT_X + 4;
    if (bat1.extra_px >= BAT_BIG_EXTRA_PX) rocket.x += 8;
    rocket.y = BAT_Y + 6;
    rocket.acc = 0;
    rocket.frac = 0;
    rocket.counter = 0;
}

/* Start the level-clear flight: the rocket emerges from inside the bat
 * and step_rocket destroys every destructible cell it passes through,
 * so the level is visibly cleared rather than just dissolving.
 *
 * No catch sound: get_rocket pushes none. */
static void attach_rocket_to_bat(void) {
    rocket.active = 1;
    rocket.clear_completed = 0;
    set_rocket_bonus_sprite_height(ROCKET_H_PX);
    hide_objects_for_rocket_clear();

    place_rocket_on_bat();

    /* INC (IY+$14) at $AA72: the ROCKET catch bumps bonus_applied by one,
     * silently cancelling whatever bat-side bonus was active (CATCH $03
     * -> $04, LASER $01 -> $02, both inert). This is why bonus_apply's
     * universal assignment skips ROCKET.
     *
     * IY is the CATCHING bat, but only bat 1 can catch a rocket here —
     * the flight is bat-1 state throughout (place_rocket_on_bat reads
     * BAT_X) — so this stays explicit rather than parameterised. */
    objects[OBJ_BAT_1].bonus_applied++;
}

/* Turn the alien on screen into its death blast — the ONE place that
 * transition happens, whether the bat, a ball, a bullet or the
 * KILL_ALIENS bonus caused it.
 *
 * sprite_set $0A makes handling_table_routines dispatch to the blast
 * handler, which animates five frames and then deactivates. The 16x13
 * blast is centred over the alien first, mirroring $A4D2's adjustment
 * of (old_w_shadow - 2) * 4 on X and 4 on Y. An alien already
 * exploding is left alone.
 * orig: kill_enemy $A4C4 / $A4D2, sound $C1A8, score $0350 BCD */
static void blast_active_alien(int side_x) {
    Object *e = &objects[OBJ_ENEMY];
    if ((e->sprite_set & 0x7F) == 0) return;
    if (e->sprite_set & 0x80) return;
    if ((e->sprite_set & 0x7F) == 0x0A) return;

    e->x_coord   = (unsigned char)(e->x_coord + (int)e->w_body_px / 2 - 8);
    e->y_coord   = (unsigned char)(e->y_coord + 4);
    e->w_body_px = 16;
    e->h_body_px = 13;
    e->sprite_set = 0x0A;
    e->sprite_num = 0;
    e->misc_12    = 0x50;   /* kill_enemy $A4C4 seed */
    /* The 350 goes to whichever side `need_change_player` names, and
     * the four routines that reach kill_enemy each set it from a
     * DIFFERENT thing (notes/double-play.md). Hence the parameter: this
     * used to hardcode BAT_X, which is right for one of the four. */
    add_points_to_score(350, side_x);
    sound_queue(SND_ALIEN_BLAST);
}

/* SLOW is ball-side: it drops every ball to the minimum speed and
 * clears the bat's bonus, since the bat tracks nothing here. It does
 * NOT reset the speed-up ramp, so the ball starts climbing back toward
 * $06 from the next $94 tick — which is how SLOW wears off.
 * orig: LA67B_7, plus the `LD (IY+$14),$FF` on that path */
static void apply_slow_bonus(int bat_idx) {
    set_bat_bonus(bat_idx, 0xFF);
    objects[OBJ_BALL_1].speed = BALL_SPEED;
    objects[OBJ_BALL_2].speed = BALL_SPEED;
    objects[OBJ_BALL_3].speed = BALL_SPEED;
}

/* Two extra balls on top of the primary, for three in play. Also
 * ball-side, so it clears the bat's bonus the same way — and it does so
 * even when the extras are already out, matching the original, which
 * writes $FF before testing anything.
 * orig: LA67B_8 $A67B */
static void apply_multi_ball_bonus(int bat_idx) {
    set_bat_bonus(bat_idx, 0xFF);
    if (ball.extra2_active || ball.extra3_active) return;

    /* The primary's DIR BYTE, not a direction reconstructed from the sign
     * cache. The original reads `(IY+$06)` straight (LA67B_8 $A67B) and
     * splits it: `AND $0F` picks the branch, `AND $30` carries the
     * quadrant.
     *
     * NOT `delta_to_dir(ball.dx, ball.dy)`: that round-trips through
     * {-1,0,+1} signs, and delta_to_dir picks its angle with
     * `abs(dx) >= BALL_SPEED`, which a sign never satisfies — the low
     * nibble comes out $04 every time and the first branch of
     * extra_ball_dirs is always taken (known-bugs #14). */
    const ExtraBallDirs dirs = extra_ball_dirs(objects[OBJ_BALL_1].dir);
    spawn_extra_ball(OBJ_BALL_2, BALL_X, BALL_Y, dirs.second);
    spawn_extra_ball(OBJ_BALL_3, BALL_X, BALL_Y, dirs.third);
    ball.extra2_active = 1;
    ball.extra3_active = 1;
    sound_queue(SND_TRIPLE_BALL);
}

/* Apply the effect that comes with `type`. Catching the same type
 * while already active extends the duration. */
static void bonus_apply(unsigned char type, int bat_idx) {
    const int catcher_x = (int)objects[bat_idx].x_coord;
    /* Original get_bonus at $A67B: every catch awards 400 points and
     * plays a sound — sound_live_add ($07) for the LIFE bonus, the
     * resize-2 beep ($0C) for everything else (push_resize_sound at
     * $A645, gated by `CP $05; CALL NZ,push_resize_sound`). */
    sound_queue(type == BONUS_TYPE_LIFE ? SND_LIVE_ADD : SND_BAT_RESIZE_2);
    /* Original LA67B_3 at \$A6FC writes the bonus type code into
     * bat.bonus_applied for every catch except ROCKET (which jumps
     * out earlier to get_rocket). Catching a new bonus thus REPLACES
     * any previous bat-side effect — e.g. catching BIG_BAT after
     * LASER clears the LASER state. */
    if (type != BONUS_TYPE_ROCKET) {
        unsigned char orig_code = bonus_to_original(type);
        set_bat_bonus(bat_idx, orig_code);
    }
    switch (type) {
        case BONUS_TYPE_LIFE:     player.lives++; life_dropped_this_round = 1; break;
        case BONUS_TYPE_SLOW:     apply_slow_bonus(bat_idx); break;
        case BONUS_TYPE_BIG_BAT:
            /* The CATCHING bat widens. */
            {
                BatState &st = bats[BAT_SLOT(bat_idx)];
                st.big_ticks    = BIG_BAT_DURATION;
                st.extra_target = BAT_BIG_EXTRA_PX;
                sound_queue(SND_BAT_RESIZE_1);
            }
            break;
        case BONUS_TYPE_BIG_BALL: ball.big_ticks = BIG_BALL_DURATION; break;
        case BONUS_TYPE_KILL_ALIENS:
            /* bonus_applied = $09 is already set above; enemy_prepare
             * reads it to stop spawning. This only clears the alien
             * already on screen. LA67B_1 sets the side from the CATCHING
             * bat's x. */
            blast_active_alien(catcher_x);
            break;
        case BONUS_TYPE_CATCH:
            /* bat.bonus_applied = \$03 has already been set above —
             * step_ball reads it on bat-bounce to decide whether to
             * stick the ball. */
            break;
        case BONUS_TYPE_ROCKET:
            if (!rocket.active) attach_rocket_to_bat();
            break;
        case BONUS_TYPE_SCORE_5K:
            /* Pure score bonus. 5000 in BCD-equivalent decimal. */
            add_points_to_score(5000, catcher_x);  /* LA67B_1: IY = bat */
            break;
        case BONUS_TYPE_LASER:
            /* bat.bonus_applied = \$01 has already been set above —
             * the inner-loop SPACE handler reads it to enable laser
             * fires. Cleared automatically when another bonus is
             * caught (since bat.bonus_applied is rewritten). */
            break;
        case BONUS_TYPE_MULTI_BALL: apply_multi_ball_bonus(bat_idx); break;
        default: break;
    }
}

/* A bat's collision extents, widened by its own BIG_BAT growth.
 *
 * Collision uses the bat BODY — 28 px, object_bat_1's w_body_px = $1C —
 * not the full 32 px sprite. The sprite's last 4 px (rows 2+ carry mask
 * $F0 in byte 3) are transparent shadow, not bat surface, so a ball
 * passing through them must not register a hit. BAT_BODY_W is in
 * physics.h. */
static int bat_left_of(int b) {
    return (int)objects[b].x_coord - bats[BAT_SLOT(b)].extra_px;
}
static int bat_right_of(int b) {
    return (int)objects[b].x_coord + BAT_BODY_W + bats[BAT_SLOT(b)].extra_px;
}
static int eff_bat_left(void)  { return bat_left_of(OBJ_BAT_1); }
static int eff_bat_right(void) { return bat_right_of(OBJ_BAT_1); }

/* SMASH (BIG_BALL) is active in the original iff bonus_applied == $07
 * (at $0397: `CP $07; JR NZ,obj_processing`). Catching another bonus
 * rewrites the byte and the ball reverts on the very next frame. The
 * timer (mirroring smash_counter's wrap at $F8) is OR'd with the bat
 * state so the effect ends either way. */
static int big_ball_active(void) {
    /* EITHER bat: SMASH is a property of the BALL, and the original
     * tests both bytes before deciding (LA27E, quoted on
     * set_bat_bonus). With the bonus byte no longer mirrored, a SMASH
     * caught by bat 2 would otherwise stop working. */
    return ball.big_ticks > 0
        && (objects[OBJ_BAT_1].bonus_applied == 0x07
            || objects[OBJ_BAT_2].bonus_applied == 0x07);
}
/* BIG_BAT is active iff bat.bonus_applied == \$00 in the original — the
 * bat-resize state machine in handling_bat_no_transform reads the byte
 * each frame. Catching another bonus immediately ends the wide-bat
 * state via the extra_target = 0 below in tick_bat_resize_of. */
static int big_bat_active_of(int b) {
    return bats[BAT_SLOT(b)].big_ticks > 0
        && objects[b].bonus_applied == 0x00;
}
/* Collision body stays 8x7 even with BIG_BALL active — $9D5A_1 sets
 * bonus_applied=$07 and swaps the sprite to spr_big_ball, but never
 * touches the ball's (IX+$0C, IX+$0D) body dimensions. The bigger sprite
 * is purely cosmetic; growing the hitbox to match makes SMASH
 * artificially easier to catch and to hit with. */
static int eff_ball_size(void) { return BALL_W_PX; }

/* BIG_BAT expiry and the width ramp toward it. The timer can also be
 * cut short by another catch replacing bat.bonus_applied; either way the
 * target goes to zero. The original's bat_decrease_size ($9DE0) is
 * silent — the resize cue comes from the replacing catch, not the
 * shrink.
 *
 * The ramp is gated every other tick: one step of extra_px changes
 * the centred body by 2 px, so every-2-ticks gives the original's
 * 1 px/frame and its ~16-frame full grow. Ungated it grew twice as fast
 * as the disassembly prescribes. */
static void tick_bat_resize_of(int b, int step) {
    BatState &st = bats[BAT_SLOT(b)];
    if (st.big_ticks > 0) {
        st.big_ticks--;
        if (st.big_ticks == 0 || !big_bat_active_of(b)) {
            st.extra_target = 0;
            st.big_ticks = 0;              /* keep the two in sync */
        }
    }
    if (!step) return;
    if (st.extra_px < st.extra_target) st.extra_px++;
    else if (st.extra_px > st.extra_target) st.extra_px--;
}

/* Both bats ramp off the SAME every-other-frame gate. The original picks
 * its resize step from `counter_misc`, one global frame counter, so two
 * growing bats stay in step with each other rather than each keeping
 * private phase. */
static void tick_bat_resize(void) {
    static unsigned char resize_gate = 0;
    int step;

    resize_gate++;
    step = ((resize_gate & 1) == 0);
    tick_bat_resize_of(OBJ_BAT_1, step);
    if (game_mode == 2) tick_bat_resize_of(OBJ_BAT_2, step);
}

/* BIG_BALL expiry, on every other PIT tick. orig: smash_counter $F8 at
 * $03B0 — it clears bat.bonus_applied so a later BIG_BALL drop is not
 * blocked by the duplicate-exclusion check. */
static void tick_big_ball_timer(void) {
    if (ball.big_ticks == 0 || (pit_ticks() & 1UL) != 0) return;
    ball.big_ticks--;
    if (ball.big_ticks != 0) return;
    /* Per byte, exactly as LA27E's expiry does it: test bat 1's for $07
     * and clear it, then test bat 2's and clear that. A bat whose byte
     * holds some OTHER bonus must keep it — this is a SMASH expiry, not
     * a general reset. */
    if (objects[OBJ_BAT_1].bonus_applied == 0x07)
        set_bat_bonus(OBJ_BAT_1, 0xFF);
    if (objects[OBJ_BAT_2].bonus_applied == 0x07)
        set_bat_bonus(OBJ_BAT_2, 0xFF);
}

/* The floating reward marker left behind by a catch. Always the +400
 * glyph, even for SCORE_5K: LA67B_3 at $A6FC sets sprite_num = $00 for
 * every catch before the type dispatch, so the +5000 sprite is only ever
 * a falling-bonus glyph.
 *
 * X drift is {-2, -1, +1, +2}, from $3030's `AND $01 / INC A / RL B /
 * JR C / NEG`: bit 0 of random picks the magnitude, bit 7 the sign. The
 * read does not advance the RNG. */
static void spawn_pts_marker(int x, int y) {
    unsigned int r;
    int mag;
    pts_marker.sprite = SPR_400_POINTS;
    pts_marker.x = x;
    pts_marker.y = y;
    pts_marker.active = 1;
    pts_marker.motion.acc = (unsigned int)(((pit_ticks() & 1UL) ? 0xFEu : 0xFFu) << 8);
    pts_marker.motion.frac = 0;
    r = rng_sample();
    mag = (int)((r & 1) + 1);
    pts_marker.dx = (r & 0x80) ? mag : -mag;
}

/* Does a falling object's BODY overlap the bat's BODY?
 *
 * Two body-vs-sprite distinctions meet here, and both are easy to get
 * wrong in the same way — by reaching for the sprite extent, which is
 * larger and makes the hit register early:
 *
 *   the BAT is 10 px, not 13. obj_compare_2pix at $94BC reads
 *   (IY+$0C, IY+$0D), the body dimensions on object_bat_1; the extra
 *   3 px are shadow and are not a catch surface.
 *
 *   the caller passes the object's body too. A bomb's is 8x8
 *   (bomb_appear at $A977 sets object_bonus+$0C/+$0D to $08,$08), NOT
 *   its 8x12 sprite — an earlier port used the sprite and triggered
 *   when the bomb's bottom reached the bat's top, rather than when the
 *   bodies overlapped.
 *
 * Uses the EFFECTIVE bat extents, so BIG_BAT widens the catch zone. */
static int overlaps_bat_body(int x, int y, int w, int h) {
    return y + h >= BAT_Y && y < BAT_Y + 10
        && x + w > eff_bat_left() && x < eff_bat_right();
}

/* Which bat is catching this falling thing, or -1.
 *
 * orig: get_bonus $A67B, the same fall-through shape as LAB1F —
 *
 *     LD IY,object_bat_1 / CALL obj_compare_2pix / JR C,LA67B_0
 *     LD A,(game_mode) / CP $02 / RET NZ
 *     LD IY,object_bat_2 / CALL obj_compare / RET NC
 *
 * bat 1 first, and bat 2 only in mode $02 and only if bat 1 missed. */
static int bonus_catching_bat(int x, int y, int w, int h) {
    if (overlaps_bat_body(x, y, w, h)) return OBJ_BAT_1;
    if (game_mode == 2 && !(objects[OBJ_BAT_2].sprite_set & 0x80)) {
        const Object &b2 = objects[OBJ_BAT_2];
        if (y + h >= (int)b2.y_coord && y < (int)b2.y_coord + 10
            && x + w > bat_left_of(OBJ_BAT_2)
            && x < bat_right_of(OBJ_BAT_2))
            return OBJ_BAT_2;
    }
    return -1;
}

static void step_bonus(void) {
    int caught_by;

    tick_bat_resize();
    tick_big_ball_timer();
    if (!bonus.active) return;
    bonus.y += motion_accel_step(&bonus.motion, FALL_DE_SLOW, FALL_CAP_SLOW);
    caught_by = bonus_catching_bat(bonus.x, bonus.y, BONUS_W_PX, BONUS_H_PX);
    if (caught_by >= 0) {
        bonus_apply(bonus.type, caught_by);   /* effect + catch sound */
        bonus.active = 0;
        /* LA67B_1 sets need_change_player from the CATCHING bat's x, so
         * the 400 follows the bat that got it. */
        add_points_to_score(400, (int)objects[caught_by].x_coord);
        spawn_pts_marker(bonus.x, bonus.y);
        return;
    }
    if (bonus.y > PLAYFIELD_H) bonus.active = 0;
}

/* Advance the +400 floating marker each tick. Port of handling_400pts at
 * $A58D + the shared LA55A_0 advance with DE=$0028, B=$80.
 *
 * The marker moves DOWN, accelerating as the accumulator grows, and dies
 * at Y >= $C0 (= the bottom of the playfield) — it falls off, however
 * much a floating-up marker would look nicer. */
static void step_pts_400(void) {
    if (!pts_marker.active) return;
    pts_marker.y += motion_accel_step(&pts_marker.motion,
                                      FALL_DE_FAST, FALL_CAP_FAST);
    /* Apply the X drift each frame (port of LA590's ADD A,SMC). Clamp
     * to playfield via the original's check_left/right_margin pattern. */
    pts_marker.x += pts_marker.dx;
    if (pts_marker.x < 8) pts_marker.x = 8;
    if (pts_marker.x > PLAYFIELD_W - 16) pts_marker.x = PLAYFIELD_W - 16;
    if (pts_marker.y >= PLAYFIELD_H) pts_marker.active = 0;
}

/* Roll a bonus type from `tbl`, honouring the original's re-roll rules.
 * Returns BONUS_TYPE_UNSUPPORTED if 16 tries all landed on something
 * excluded or unported.
 *
 * generate_new_bonus re-rolls when the pick equals `current_bonus`,
 * which $9D5A sets to the bat's active bonus just before generating —
 * so comparing against bat.bonus_applied is byte-faithful, not an
 * approximation. It stops back-to-back duplicates of the same effect.
 *
 * Per-type exclusions, L9D5A_2..L9D5A_9:
 *   $02 TRIPLE_BALL  extras already in play
 *   $04 SLOW         a ball already at the minimum speed
 *   $05 LIFE         already dropped this round
 *   $06 ROCKET       one already in flight
 * and from round 6 the rocket takes an extra (random & $C0) re-roll, so
 * about three in four would-be rockets are rejected and the bonus turns
 * ~4x rarer late on ($9D6F's CP $06 / JR C / AND $C0 / JR NZ chain).
 *
 * Each try advances the RNG: generate_new_bonus re-CALLs
 * random_generate per retry. */
/* Which bonus to drop, given everything currently going on.
 *
 * THIS STAYS IN main.cpp. It reads seven pieces of live game state to
 * reject inappropriate draws:
 *
 *   objects[OBJ_BAT_1].bonus_applied   do not re-drop what is active
 *   ball.extra2_active / extra3_active no multiball while extras fly
 *   objects[OBJ_BALL_1].speed          no slow-ball at base speed
 *   life_dropped_this_round            one extra life per round
 *   rocket.active                      no second rocket
 *   round_number                       gates the rocket after round 6
 *
 * That is not a module's worth of geometry, it is the game deciding what
 * is appropriate. Moving it would drag most of the game's state into
 * whatever module received it. The consequence: BATTY_FORCE_SPAWN_BONUS's
 * seeder cannot follow the bomb's into src/replay.cpp, because it calls
 * this. */
static unsigned char pick_bonus_type(const unsigned char *tbl) {
    int tries;
    for (tries = 0; tries < 16; tries++) {
        const unsigned int rnd = next_random();
        const unsigned char code = tbl[random_hi(rnd) & 0x0F];
        unsigned char mapped;

        if (code == objects[OBJ_BAT_1].bonus_applied) continue;
        if (code == 0x02 && (ball.extra2_active || ball.extra3_active)) continue;
        if (code == 0x04 && objects[OBJ_BALL_1].speed <= BALL_SPEED) continue;
        if (code == 0x05 && life_dropped_this_round) continue;
        if (code == 0x06 && rocket.active) continue;
        if (code == 0x06 && round_number >= 6
            && (random_lo(rnd) & 0xC0) != 0) continue;

        mapped = bonus_from_original(code);
        if (mapped != BONUS_TYPE_UNSUPPORTED) return mapped;
    }
    return BONUS_TYPE_UNSUPPORTED;
}

/* A destroyed brick may drop a bonus. Called from every destruction
 * site — ball, bullet, rocket sweep — so the cadence does not depend on
 * what destroyed the brick.
 *
 * The chance is 5/16 (about 31%) per brick, from the test at $A2CC. It
 * reads random_number WITHOUT advancing it (rng_sample); only the type
 * pick advances the RNG. A deterministic every-Nth counter gives the
 * same average rate but is visible as a pattern.
 *
 * A bomb in flight blocks the drop: the original shares object_bonus
 * between the two, and the port mirrors that exclusion despite keeping
 * them in separate state. */
static void try_spawn_bonus(int col, int row) {
    const unsigned char *tbl;
    unsigned char type;

    if (bonus.active || bomb.active) return;
    if ((random_hi(rng_sample()) & 0x0F) >= 5) return;

    tbl  = (round_number >= 6) ? bonus_table_second : bonus_table_first;
    type = pick_bonus_type(tbl);
    if (type == BONUS_TYPE_UNSUPPORTED) return;

    bonus.active = 1;
    bonus.x = 8 + col * 16 + (16 - BONUS_W_PX) / 2;
    bonus.y = 32 + row * 8;
    bonus.type = type;
    bonus.motion.acc = 0;
    bonus.motion.frac = 0;
}

/* Track the destroyed brick cell long enough to dirty its full original
 * blit footprint. print_one_brik_buf writes one row above, one row below,
 * and one pixel into neighbouring byte columns. */
static void brick_flash_spawn(int col, int row) {
    brick_flash.x = 8 + col * 16;
    brick_flash.y = 32 + row * 8;
    brick_flash.ticks = BRICK_FLASH_TICKS;
}

static void step_brick_flash(void) {
    if (brick_flash.ticks) brick_flash.ticks--;
}

/* No visual flash here. The original destruction path leaves the brick
 * absent after background recovery; this marker exists only for dirty
 * rectangle scheduling. */
static void render_brick_flash_to_buff(void) {
    (void)brick_flash.x;
    (void)brick_flash.y;
}

static int brick_hit_resolve(int col, int row, int axis, int slot);
static int laffc_collision(Object *o, int prev_x, int prev_y, int new_x,
                           int new_y, int slot);

static int brick_hit_resolve(int col, int row, int axis, int slot) {
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
    /* SMASH (BIG_BALL) bypasses the multi-hit half-state — LAFFC's
     * `CP $07; JR Z,LAFFC_38` jumps straight to the destroy path. */
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
        add_points_to_score(pts, ball_owner_side[slot] ? 0x80 : 0);
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

/* Does the ball's new centre overlap a live brick? If so the brick is
 * marked destroyed and the axis to reverse is returned:
 *   0 = no hit
 *   1 = vertical   (entered top or bottom) -> caller flips dy
 *   2 = horizontal (entered from a side)   -> caller flips dx
 * The previous position disambiguates the corner cases. Field geometry
 * is in level.h. */
static int brick_collision(int prev_x, int prev_y, int new_x, int new_y,
                           int slot) {
    const BrickHit hit = brick_sweep(BrickField(live_level),
                                     eff_ball_size(), BALL_H_PX,
                                     prev_x, prev_y, new_x, new_y);
    if (!hit.hit) return 0;
    return brick_hit_resolve(hit.col, hit.row, hit.axis, slot);
}

static int laffc_collision(Object *o, int prev_x, int prev_y, int new_x,
                           int new_y, int slot) {
    (void)prev_x; (void)prev_y;
    const LaffcHit hit = laffc_sweep(BrickField(live_level), o->dir,
                                     o->w_body_px, o->h_body_px, new_x, new_y);
    if (!hit.hit) return 0;

    /* SMASH (big ball) ploughs through: the cell is destroyed and there is
     * no bounce to apply. */
    if (brick_hit_resolve(hit.col, hit.row, 1, slot) == 0) return 0;

    const BallBounce bounce = laffc_bounce(hit, o->dir,
                                           o->w_body_px, o->h_body_px,
                                           new_x, new_y);
    o->x_coord = bounce.x;
    o->y_coord = bounce.y;
    o->dir     = bounce.dir;

    int dx_q8, dy_q8;
    dir_to_dxdy(o->dir, o->speed, &dx_q8, &dy_q8);
    refresh_ball_motion_signs(o, dx_q8, dy_q8);
    return 3;   /* reflected and snapped; step_ball must not re-reflect */
}

/* The live-brick rule lives in bricks (with the contrast against
 * BrickField::solid spelled out there); this is just the binding to the
 * current level's grid. */
static int live_bricks_remaining(void) {
    return bricks_live_count(live_level);
}

static unsigned char ctrl_btns_pressed_value(void) {
    unsigned char v = 0;
    if (key_state[SC_RIGHT]) v |= 0x01;
    if (key_state[SC_LEFT])  v |= 0x02;
    if (key_state[SC_SPACE]) v |= 0x10;
    return v;
}

static unsigned int next_random(void) { return rng_next(ctrl_btns_pressed_value()); }

/* The noise envelope's random source. orig: the magnet zip reads
 * random_number like any other consumer. */
static u8 sound_random_byte(void) { return rng_low(u16(next_random())); }

/* The enemy's two reads, kept distinct on purpose: arrival looks at the
 * current number, target-picking goes through the model's sampler.
 * See notes/rng-model.md. */
static u8 enemy_random_current(void) { return rng_low(rng_current()); }
static u8 enemy_random_sample(void)  { return rng_low(u16(rng_sample())); }

/* Sample for a "read-current" consumer — the original's
 * `LD A,(random_number)` with no preceding `CALL random_generate`. With
 * rng_perframe ON this returns the current number WITHOUT advancing,
 * because the per-frame tick at the play-loop top is the only advance.
 * Consumers the original advances-then-reads (bonus generation) call
 * next_random() directly instead. */
static unsigned int rng_sample(void) {
    return dbg.rng_perframe ? rng_current() : next_random();
}




static void apply_replay_ball_motion_override(void) {
    const char *stuck = getenv("BATTY_REPLAY_BALL_STUCK");
    const char *vel = getenv("BATTY_REPLAY_BALL_VEL");
    if (stuck != NULL) {
        ball.stuck[BALL_PRIMARY] = (unsigned char)(atoi(stuck) != 0);
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
        ball.stuck[BALL_PRIMARY] = 0;
    }
}


/* Bake a falling bonus for the falling-object regression gate.
 * BATTY_REPLAY_BONUS = "type,x,y". Starts a fresh fall (motion zeroed)
 * so the accel progression is deterministic from y; put x clear of the
 * bat to test a pure fall with no catch. */
static void apply_replay_bonus_override(void) {
    long v[3];
    if (!replay_env_ints("BATTY_REPLAY_BONUS", v, 3)) return;
    bonus.active = 1;
    bonus.type = (unsigned char)v[0];
    bonus.x = (int)v[1];
    bonus.y = (int)v[2];
    bonus.motion.acc = 0;
    bonus.motion.frac = 0;
}

/* Bake a +400 score popup for the pts-400-fall gate.
 * BATTY_REPLAY_PTS400 = "x,y". Uses a DIFFERENT accel constant pair than
 * bonus/bomb, so this exercises the faster-grow path. dx is zeroed so
 * the y progression is pure. */
static void apply_replay_pts400_override(void) {
    long v[2];
    if (!replay_env_ints("BATTY_REPLAY_PTS400", v, 2)) return;
    pts_marker.active = 1;
    pts_marker.x = (int)v[0];
    pts_marker.y = (int)v[1];
    pts_marker.dx = 0;
    pts_marker.motion.acc = 0;
    pts_marker.motion.frac = 0;
}

/* Activate big-ball (SMASH) for the deterministic big-ball dirty-tier gate.
 * big_ball_active() needs ball.big_ticks>0 AND bat.bonus_applied==0x07. */
static void apply_replay_bigball(void) {
    if (getenv("BATTY_REPLAY_BIGBALL") == NULL) return;
    ball.big_ticks = BIG_BALL_DURATION;
    set_bat_bonus(OBJ_BAT_1, 0x07);
}

/* Bake two extra balls (multi-ball) for the deterministic extra-ball dirty
 * tier gate — WITHOUT a bonus catch (so no +400 popup) and placed BELOW the
 * brick band (y=150, clear of bricks at y<128 and the bat at y>=173), so a
 * few-frame probe stays clear of any emergent brick hit regardless of
 * direction. Dirs/speed copied from the primary. */
static void apply_replay_multiball(void) {
    if (getenv("BATTY_REPLAY_MULTIBALL") == NULL) return;
    if (ball.extra2_active || ball.extra3_active) return;
    /* Both extras inherit the primary's direction, unlike a real catch,
     * so the gate probes step_extra_ball on a known trajectory. */
    spawn_extra_ball(OBJ_BALL_2, 96, 150, objects[OBJ_BALL_1].dir);
    spawn_extra_ball(OBJ_BALL_3, 160, 150, objects[OBJ_BALL_1].dir);
    ball.extra2_active = 1;
    ball.extra3_active = 1;
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
 * (cap 6) when ball.speed_ramp reaches 0x94, ticking once per 8 frames
 * (ball_speed_ramp_tick). Seeding it near 0x94 lets the gate observe a bump
 * in a few frames instead of the full ~1184-frame climb. */
static void apply_replay_ball_ramp(void) {
    const char *spec = getenv("BATTY_REPLAY_BALL_RAMP");
    if (spec == NULL || !*spec) return;
    ball.speed_ramp = (unsigned int)strtol(spec, NULL, 0);
}

/* Plant a known brick for the per-row scoring gate.
 * BATTY_FORCE_BRICK = "col,row,value" overwrites live_level[row*COLS+col]
 * after the level load, so a bullet/ball can destroy a brick of a known row
 * + colour and the points_table[row] (x2 for colour nibble >= 6) award is
 * checkable. value 0x1X = single-hit (bit4 set) colour-X brick. */
static void apply_replay_force_brick(void) {
    long v[3];      /* col, row, cell value */
    if (!replay_env_ints("BATTY_FORCE_BRICK", v, 3)) return;
    if (v[0] < 0 || v[0] >= LVL_COLS || v[1] < 0 || v[1] >= LVL_ROWS) return;
    live_level[v[1] * LVL_COLS + v[0]] = (unsigned char)v[2];
}

static void apply_replay_rocket_override(void) {
    if (getenv("BATTY_REPLAY_ROCKET_ACTIVE") == NULL) return;
    rocket.active = 1;
    rocket.clear_completed = 0;
    set_rocket_bonus_sprite_height(ROCKET_H_PX);
    place_rocket_on_bat();
    BALL_HIDE();
    ball.stuck[BALL_PRIMARY] = 0;
}

/* prop_uneven / prop_even / prop_x_coord from $9F27. Fields:
 *   +0 type ($09=bird, $08=UFO)
 *   +1 misc_12
 *   +2 misc_13
 *   +3 width body
 *   +4 height body
 *   +5 speed */
static const unsigned char prop_uneven[6] = { 0x09, 0xF0, 0x70, 0x18, 0x0C, 0x01 };
/* prop_even, byte-exact per $9F2D: the UFO is 16 px tall and moves at
 * speed 1, the same as the bird. */
static const unsigned char prop_even[6]   = { 0x08, 0x60, 0x90, 0x18, 0x10, 0x01 };
static const unsigned char prop_x_coord[4]= { 0x40, 0xA8, 0x40, 0xA8 };

static void play_bat_explosion(unsigned char level_idx);   /* forward */
static void respawn_primary_ball(void);                     /* forward */

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

static void serial_probe_signal(void) {
    static const char msg[] = "PROBE\r\n";
    int k;
    if (!probe.serial_enabled) return;
    for (k = 0; msg[k] != '\0'; k++) {
        unsigned int spin = 0;
        while (!(inp(0x3FD) & 0x20) && ++spin < 60000u) { }
        outp(0x3F8, (unsigned char)msg[k]);
    }
}

/* `object_<name>=` and the descriptor's raw bytes. The gates decode
 * fields out of the same 22 bytes the game uses, so nothing here
 * interprets them. */
static void probe_write_object(FILE *f, const char *name, unsigned char slot) {
    const unsigned char *bytes = (const unsigned char *)&objects[slot];
    int i;
    fprintf(f, "object_%s=", name);
    for (i = 0; i < (int)sizeof(Object); i++) fprintf(f, "%02X", bytes[i]);
    fputc('\n', f);
}

/* Everything on the playfield besides the objects dumped above: the
 * falling bonus, bomb, +400 marker, bullets, and the counters the gates
 * read to check RNG-dependent drops against the original. */
static void probe_write_entities(FILE *f) {
    /* Bonus/bomb state (the original shares object_bonus $9B80 for both).
     * Used to verify RNG-dependent drops match the original (e.g. that the
     * RNG-perframe flip + seed do not spawn a spurious bonus). */
    fprintf(f, "bonus_state=active%02X_type%02X_x%02X_y%02X_bomb%02X",
            (unsigned)bonus.active, (unsigned)bonus.type,
            (unsigned)(bonus.x & 0xFF), (unsigned)(bonus.y & 0xFF),
            (unsigned)bomb.active);
    fprintf(f, "\nbomb_state=active%02X_x%02X_y%02X",
            (unsigned)bomb.active, (unsigned)(bomb.x & 0xFF),
            (unsigned)(bomb.y & 0xFF));
    fprintf(f, "\npts400_state=active%02X_x%02X_y%02X",
            (unsigned)pts_marker.active, (unsigned)(pts_marker.x & 0xFF),
            (unsigned)(pts_marker.y & 0xFF));
    fprintf(f, "\nbullet_state=active%02X_x%02X_y%02X",
            (unsigned)bullet_active[0], (unsigned)(bullet_x[0] & 0xFF),
            (unsigned)(bullet_y[0] & 0xFF));
    fprintf(f, "\nlaser_fire_state=shots%04X_cd%02X",
            (unsigned)probe.shots_fired, (unsigned)bullet_cooldown);
    fprintf(f, "\nspeed_ramp_state=ramp%04X_spd%02X",
            (unsigned)ball.speed_ramp, (unsigned)objects[OBJ_BALL_1].speed);
    fprintf(f, "\nblast_state=ticks%02X_frame%02X",
            (unsigned)bullet_blast_ticks[0],
            (unsigned)(bullet_blast_ticks[0]
                       ? (bullet_blast_ticks[0] - 1) / BULLET_BLAST_TICKS_PER_FRAME
                       : 0xFF));
    fprintf(f, "\neffects_state=b2%02X_b3%02X_xtgt%02X_bball%02X_lives%02X",
            (unsigned)ball.extra2_active, (unsigned)ball.extra3_active,
            (unsigned)(bat1.extra_target & 0xFF),
            (unsigned)(ball.big_ticks != 0),
            (unsigned)(player.lives & 0xFF));
}

/* The harness's own state — what the replay knobs seeded and what the
 * checkpoints counted. None of it is game state. */
static void probe_write_harness_state(FILE *f) {
    fprintf(f, "\nnormal_launch_state=%02X%02X%02X%02X%02X",
            (unsigned)probe.last_launch.valid,
            (unsigned)probe.last_launch.x,
            (unsigned)probe.last_launch.y,
            (unsigned)probe.last_launch.dir,
            (unsigned)probe.last_launch.speed);
    fprintf(f, "\nlaunch_probe_state=%04X%04X%02X",
            (unsigned)probe.launch_frames,
            (unsigned)probe.launch_countdown,
            (unsigned)probe.launch_active);
    fprintf(f, "\nframe_probe_state=%04X%04X%02X",
            (unsigned)probe.frame_frames,
            (unsigned)probe.frame_countdown,
            (unsigned)probe.frame_active);
    fprintf(f, "\ngame_mode=%02X_player%02X_life%lu_over%lu_go%lu",
            (unsigned)game_mode, (unsigned)active_player,
            turn_changes_life, turn_changes_over, game_overs_reached);
    /* The alien's brick-hit walk target (orig LAA7B). Zero y means no
     * target. Without this the whole reaction is invisible to every
     * capture — the bricks are untouched by design, so nothing on screen
     * says it fired. */
    fprintf(f, "\nenemy_home=%02X%02X",
            (unsigned)enemy_home_target.x, (unsigned)enemy_home_target.y);
    fprintf(f, "\nbonus_pts_raw=%02X%02X%02X%02X%02X%04X",
            (unsigned)bonus.active,
            (unsigned)bonus.type,
            (unsigned)(bonus.x & 0xFF),
            (unsigned)(bonus.y & 0xFF),
            (unsigned)pts_marker.active,
            (unsigned)(pts_marker.sprite & 0xFFFFu));
}

static void write_replay_probe(void) {
    FILE *f;
    int i;
    if (getenv("BATTY_REPLAY_PROBE") == NULL) return;
    f = fopen("PROBE.TXT", "wt");
    if (!f) return;
    fprintf(f, "probe_phase=%s\n", probe.from_gameplay ? "play" : "init");
    fprintf(f, "round_number=%02X\n", (unsigned)round_number);
    fprintf(f, "current_level=%02X\n", (unsigned)current_level_idx_var);
    fprintf(f, "bricks_quantity=%02X\n", (unsigned)live_bricks_remaining());
    fprintf(f, "score=%06lu\n", player.score);
    fprintf(f, "scores=%06lu_%06lu_own%02X\n",
            players[0].score, players[1].score,
            (unsigned)ball_owner_side[BALL_PRIMARY]);
    /* All three owners, as their own row. The `own` field above stays
     * the primary's: several gates parse it, and widening a field other
     * tests already read is how a probe change breaks things it was not
     * about. */
    fprintf(f, "ball_owners=%02X%02X%02X\n",
            (unsigned)ball_owner_side[0], (unsigned)ball_owner_side[1],
            (unsigned)ball_owner_side[2]);
    fprintf(f, "random_number=%04X\n", (unsigned)rng_current());
    fprintf(f, "random_seed=%04X\n", (unsigned)rng_seed_addr());
    fprintf(f, "enemy_repicks=arrival%u_turns%u\n",
            enemy_arrival_repicks, enemy_turn_calls);
    fprintf(f, "brik_anim_ticks=%lu\n", probe.brik_anim_ticks);
    /* Both clocks, side by side, to settle known-bugs #15: the game-over
     * hold is the only live user of bios_ticks() and it never expires
     * under QEMU. If bios advances while pit does, the hold is not a
     * clock problem; if it stays put, blink_phase() is dead too. */
    fprintf(f, "clocks=bios%lu_pit%lu_dbios%lu_dpit%lu\n",
            bios_ticks(), pit_ticks(),
            bios_ticks() - probe.bios_at_frame1,
            pit_ticks() - probe.pit_at_frame1);
    fprintf(f, "magnet_state=count%02X_on%02X%02X%02X%02X_ball0_c%02X_d%02X_e%02X_i%02X\n",
            (unsigned)magnets.count,
            (unsigned)magnets.on_state[0], (unsigned)magnets.on_state[1],
            (unsigned)magnets.on_state[2], (unsigned)magnets.on_state[3],
            (unsigned)ball.mag_cool[0], (unsigned)ball.mag_delta[0],
            (unsigned)ball.mag_exit[0], (unsigned)ball.mag_idx[0]);
    probe_write_object(f, "ball_1", OBJ_BALL_1);
    probe_write_object(f, "bat_1", OBJ_BAT_1);
    /* Bat 2 only moves in Double Play, and a gate on its steering has
     * nothing else to read: it leaves no mark a screendump can tell from
     * bat 1's, both being the same sprite on the same row. */
    probe_write_object(f, "bat_2", OBJ_BAT_2);
    probe_write_object(f, "enemy", OBJ_ENEMY);
    /* Extra balls (multiball) — so the collision-invariant sweep can probe
     * step_extra_ball's path (no-tunnel) the same way it probes the primary. */
    probe_write_object(f, "ball_2", OBJ_BALL_2);
    probe_write_object(f, "ball_3", OBJ_BALL_3);
    probe_write_entities(f);
    probe_write_harness_state(f);
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

/* Whether a natural alien spawn is allowed this frame. Six reasons it
 * might not be, and the first is a test hook rather than a game rule.
 * orig: $9EAA */
static bool enemy_spawn_allowed(void) {
    /* Test-mode pin (BATTYALL): no NATURAL spawns, the same determinism
     * trick as the menu blink, the running dot and the magnet toggle.
     * On levels whose starting brick count is already under the $2C
     * gate (L3: 26, L9: 7), an alien would spawn within the first
     * gameplay frame and the wall-clock state4 screendump would race its
     * descent — the L3/L9 "186 px drift" in notes/per-level-profile.md.
     * The ground truth is alien-free by construction, and a test that
     * wants one seeds it via BATTY_REPLAY_ENEMY_OBJECT, which bypasses
     * this spawner entirely. */
    if (test_mode_pin_blink) return false;

    /* L4 has no enemies at all — $9EAA returns immediately there. */
    if (current_level_idx_var == 4) return false;

    /* The KILL_ALIENS bonus stops further spawns while it is held. */
    if (objects[OBJ_BAT_1].bonus_applied == 0x09) return false;
    if (objects[OBJ_BAT_2].bonus_applied == 0x09) return false;

    if (live_bricks_remaining() >= 0x2C) return false;   /* too early */
    if (objects[OBJ_ENEMY].sprite_set != 0) return false; /* one already out */
    return true;
}

static void enemy_prepare(void) {
    Object *e = &objects[OBJ_ENEMY];
    const unsigned char *prop;
    unsigned char r;

    if (!enemy_spawn_allowed()) return;

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
     * target (+$14) = $10 UNCONDITIONALLY — NOT derived from the same
     * random byte. The enemy spawns heading straight down and steers
     * from there (ground truth: frame-0 dir = 0x10). */
    r = (unsigned char)(random_lo(rng_sample()) & 3);
    e->x_coord = prop_x_coord[r];
    e->x_coord_hi = 0;
    e->y_coord_hi = 0;
    e->dir = 0x10;             /* LD (IX+$06),$10 */
    e->bonus_applied = 0x10;   /* LD (IX+$14),$10 — initial target */
}


/* Does this rect reach a live alien? If so it becomes its blast. The
 * bat and every ball share this: the original's kill_enemy_by_bat at
 * $A4B8 is called from BOTH handling_bat AND
 * handling_ball (see the cross-reference at line 2745 of the disasm),
 * so a ball plunking down on an alien destroys it the same way a bat
 * crashing into one does. AABB between the ball body (8x7) and the
 * alien body. */
static void kill_enemy_in_rect(int bx_l, int by_t, int bw, int bh,
                               int side_x) {
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
    blast_active_alien(side_x);
}

/* Port of kill_enemy_by_bat at $A4B8 / kill_enemy at $A4C4: AABB between
 * the alien body rect and the bat; on overlap the alien becomes its
 * blast, worth 350 BCD points (LD BC,$0350 at $A4E0). */
static void kill_enemy_by_bat(void) {
    /* Effective extents, so BIG_BAT widens the kill zone: the original
     * uses obj_compare_2pix with (IY+$0C) = the current bat body width,
     * which grows with the bonus. Height is $0A per object_bat_1's
     * init, i.e. the body band without the shadow rows. */
    kill_enemy_in_rect(eff_bat_left(), BAT_Y,
                       eff_bat_right() - eff_bat_left(), 10, BAT_X);

    /* `kill_enemy_by_bat` is called from `handling_bat`, which in mode
     * $02 runs for BOTH bats — so bat 2 kills the alien too, and scores
     * for its own side. There is no bonus condition on either: any bat
     * touching the alien destroys it. */
    if (game_mode == 2) {
        const Object &b2 = objects[OBJ_BAT_2];
        if (!(b2.sprite_set & 0x80))
            kill_enemy_in_rect(bat_left_of(OBJ_BAT_2), BAT_Y,
                               bat_right_of(OBJ_BAT_2)
                               - bat_left_of(OBJ_BAT_2), 10,
                               (int)b2.x_coord);
    }
}

/* Port of bomb_appear at $A977 - called per alien tick. Probability
 * (random + random+1) & $3F == 0 = ~1/64 chance per call. Bomb
 * shares the bonus slot in the original; we keep separate state. */
static void bomb_appear(Object *o) {
    unsigned int r;
    if (bomb.active) return;
    if (bonus.active) return;
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
    bomb_launch((int)o->x_coord + 8, (int)o->y_coord + 8);
}

/* The one place a life is taken, so BATTY_INFINITE_LIVES has one place
 * to not take it. Two paths decrement — lose_a_life for the ball, and
 * step_bomb for a bomb on the bat — and a switch that guarded only the
 * first would work everywhere except the one death a bomb causes, which
 * is the death you are least likely to be looking for when you set it.
 *
 * It suppresses the DECREMENT only. The bat still explodes and the ball
 * still respawns, because a manual-testing build that skipped the death
 * animation would be testing a game the port does not ship. */
static void take_a_life(void) {
    if (dbg.infinite_lives) return;
    if (player.lives > 0) player.lives--;
}

/* Step the bomb each frame: fall, check bat collision, deactivate
 * past the bottom. Bat hit costs a life and respawns the ball. */
static void step_bomb(void) {
    if (!bomb.active) return;
    bomb_fall_step(PLAYFIELD_H);
    /* The fall owns the off-bottom deactivate, so this test sits ABOVE
     * the bat check. The order cannot matter: off-bottom means
     * bomb.y > 192, and overlaps_bat_body requires bomb.y < BAT_Y + 10 =
     * 186, so the two can never both hold. */
    if (!bomb.active) return;
    /* 8 high, not BOMB_H_PX: the body is 8x8, the sprite 8x12. See
     * overlaps_bat_body. */
    if (overlaps_bat_body(bomb.x, bomb.y, BOMB_W_PX, 8)) {
        /* Bomb hit the bat. Original at $A69D zeroes balls_quantity
         * which triggers LBC10's bat-explosion + lives-- branch on
         * the next frame — i.e. ALL balls die, not just the primary.
         * Mirror that here so multi-ball play can't soak bomb hits. */
        bomb.active = 0;
        hide_extra_balls();
        play_bat_explosion(current_level_idx_var);
        take_a_life();
        if (player.lives > 0) respawn_primary_ball();    /* else game-over fires next frame */
    }
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

    /* `handling_bullet` opens with `LD A,(IX+$02) / AND $80 /
     * LD (need_change_player),A` — the BULLET's own x, not the bat's and
     * not the ball's owner. Both scoring paths below take it. */
    const int side_x = bullet_x[b];

    if (hit.what == BulletHit::ENEMY) {
        blast_active_alien(side_x);
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
            add_points_to_score(pts, side_x);
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
 * BATTY_AUTO_FIRE) once per frame.
 *
 * Reset to $18 = 24, decrement 2 per frame, and the `== 0` test running
 * BEFORE the decrement means a shot is allowed on the 12th frame, not
 * the 13th. test-laser-cadence pins it. */
static void try_fire_laser_from(int b) {
    int free_slot = -1;
    int j;
    if (rocket.active
        || objects[b].bonus_applied != 0x01
        || bullet_cooldown != 0) return;
    for (j = 0; j < N_BULLETS; j++) {
        if (!bullet_active[j]) { free_slot = j; break; }
    }
    if (free_slot < 0) return;
    bullet_active[free_slot] = 1;
    bullet_frame[free_slot] = 0;
    bullet_x[free_slot] = (int)objects[b].x_coord + 12;
    bullet_y[free_slot] = (int)objects[b].y_coord - 1;
    bats[BAT_SLOT(b)].fire_anim_ticks = 8;
    bullet_cooldown = 0x18;          /* 12 frames @ -2 / frame */
    probe.shots_fired++;
    sound_queue(SND_SHOT);
}

static void try_fire_laser(void) { try_fire_laser_from(OBJ_BAT_1); }

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

/* End-of-level brick-points tally (port of add_points_for_left_briks
 * $AF0D): tick the remaining bricks' points up one-by-one with the scene
 * + score on screen (bricks stay visible), then clear them so
 * live_bricks_remaining()==0 advances the level. */
static void play_rocket_award_tally(unsigned char level_idx);

/* Step the rocket one frame: move up on the handling_rocket accel,
 * lifting the bat. The LBB97 flight loop does NO brick destruction — the
 * rocket flies over the INTACT field and the dirty redraw restores the
 * bricks behind it; they are awarded and cleared at fly-off. */
static void step_rocket(void) {
    if (!rocket.active) return;
    /* Port of handling_rocket at $A89A:
     *   HL = LA8CF - $20
     *   if counter_misc >= $38, persist HL back to LA8CF
     *   y/fract = HL + (current_y:LA8D1)
     * This gives a slow initial lift followed by acceleration instead
     * of a constant-pixel rocket climb. */
    {
        unsigned int hl;
        unsigned int sum;
        rocket.counter++;
        hl = (unsigned int)(rocket.acc - 0x0020u);
        if (rocket.counter >= 0x38) rocket.acc = hl;
        sum = (unsigned int)(hl + (((unsigned int)(unsigned char)rocket.y) << 8) + rocket.frac);
        rocket.frac = (unsigned char)sum;
        rocket.y = (int)(unsigned char)(sum >> 8);
        /* handling_rocket writes rocket.y - 6 into both bat objects,
         * so the rocket pack stays attached and lifts the bat. */
        BAT_Y = (unsigned char)(rocket.y - 6);
        objects[OBJ_BAT_2].y_coord = BAT_Y;
    }
    if (rocket.y >= PLAYFIELD_H || rocket.y + ROCKET_H_PX < 0) {
        rocket.active = 0;
        rocket.clear_completed = 1;
        /* Mirror LBB97 → LBBFB → add_points_for_left_briks: tick up the
         * points for every remaining brick (bricks stay on screen), then
         * clear them to end the level. */
        play_rocket_award_tally(current_level_idx_var);
        return;
    }
    /* No brick destruction during flight. A bbox sweep here carves a
     * tunnel the original does not have — see notes/rocket-flight.md. */
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
    if (++ball.speed_ramp != 0x94) return;
    ball.speed_ramp = 0;
    if (objects[OBJ_BALL_1].speed < 6) objects[OBJ_BALL_1].speed++;
    if (ball.extra2_active && objects[OBJ_BALL_2].speed < 6) objects[OBJ_BALL_2].speed++;
    if (ball.extra3_active && objects[OBJ_BALL_3].speed < 6) objects[OBJ_BALL_3].speed++;
}

/* ---- Magnet ball physics — port of handling_ball's LA27E_0..11 ------- */

/* obj_compare ($AC22) against magnet slot i: the slot's stored origin is
 * the paint origin +5 on both axes (print_magnets' post-draw ADD $05s),
 * body 15x14 px (slot +$0C/+$0D). Carry = overlap. Note the original's
 * asymmetry: strict `<` against the ball's body when the magnet is to
 * the right/below, `<=` against the magnet's body otherwise. */
static int magnet_ball_overlap(const Object *o, unsigned char i) {
    unsigned char mx = (unsigned char)(magnets.px[i] + 5);
    unsigned char my = (unsigned char)(magnets.py[i] + 5);
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
    hit = laffc_collision(o, o->x_coord, o->y_coord, next_x, next_y,
                          (int)(o - objects));
    if (hit == 0) hit = brick_collision(o->x_coord, o->y_coord, next_x, next_y,
                                        (int)(o - objects));
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
    if (ball.mag_cool[si]) {           /* post-release re-capture cooldown */
        ball.mag_cool[si]--;
        return 0;
    }
    if (ball.mag_delta[si]) {          /* captured: curve the trajectory */
        unsigned char dir = (unsigned char)((o->dir + ball.mag_delta[si]) & 0x3F);
        unsigned char ex;
        unsigned char mi = ball.mag_idx[si];
        o->dir = dir;
        /* Quantized exit dir, recomputed every captured frame:
         * (dir+2) & $3C, nudged ±4 off the pure up/down/left/right
         * codes (LA27E_1..3). Always a multiple of 4. */
        ex = (unsigned char)((dir + 2) & 0x3C);
        if ((ex & 0x0F) == 0) {
            if (dir & 0x0C) ex = (unsigned char)((ex - 4) & 0x3F);
            else            ex = (unsigned char)((ex + 4) & 0x3F);
        }
        ball.mag_exit[si] = ex;
        if (!magnets.on_state[mi] || !magnet_ball_overlap(o, mi)) {
            /* LA27E_5: release — exit dir, 2-frame cooldown, then the
             * NORMAL move/collision path runs this frame (LA27E_23). */
            ball.mag_cool[si]  = 2;
            ball.mag_delta[si] = 0;
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
        for (i = 0; i < magnets.count; i++) {
            unsigned char b = 0;
            if (!magnets.on_state[i]) continue;       /* BIT 0,(IY+$01) */
            if (!magnet_ball_overlap(o, i)) continue;
            if (((o->dir + 0x10) & 0x3F) >= 0x20) b = 0xFE;
            if ((unsigned char)(magnets.py[i] + 5 + 4) >= o->y_coord) b ^= 0xFE;
            ball.mag_delta[si] = (unsigned char)(0xFF ^ b);  /* $FF or $01 */
            ball.mag_idx[si]   = i;
            break;
        }
    }
    return 0;
}

static void magnet_ball_state_clear(unsigned char si) {
    /* LA27E_25 bottom-exit / LBC10: zero the cooldown + delta bytes. */
    ball.mag_cool[si]  = 0;
    ball.mag_delta[si] = 0;
}

/* MAGNET/CATCH: the ball sticks on contact and waits for FIRE. Only a
 * NORMAL-width bat catches — the original gates on width $1C, so a big
 * bat falls through to the ordinary deflection.
 *
 * The offset is QUANTIZED: ball_x - bat_x, clamped >= 0, then & 0xFC (a
 * multiple of 4) and capped at 0x18. That is what makes the rest x and
 * the launch direction derived from it match the Spectrum — probed,
 * ball_x 133 gives offset 0x10 and rest x 132.
 * orig: LAB1F_1..3 */
static void catch_ball_on_bat(int b, int bat, int contact_x) {
    const int bat_x = objects[bat].x_coord;
    int off = contact_x - bat_x;
    if (off < 0) off = 0;
    off &= 0xFC;
    if (off >= 0x19) off = 0x18;

    ball.stuck_offset_x[b] = off;
    ball.stuck[b]          = 1;
    ball.stuck_ticks[b]    = 0;
    ball.stuck_bat[b]      = (unsigned char)bat;
    /* A SIGN, not a speed: this cache holds {-1,0,+1}, and the value
     * written here SURVIVES, because a caught ball is stuck and step_ball
     * early-returns above the refresh. dx is deliberately left alone —
     * known-bugs.md #14 is whether the original wants a magnitude there
     * at all. Guarded rather than indexed because the cache is the
     * PRIMARY's alone (known-bugs #13). */
    if (b == BALL_PRIMARY) ball.dy = -1;
    objects[b].dir = 0x20;
    objects[b].x_coord = (unsigned char)(bat_x + off);
    /* A caught ball rests 1 px lower than the launch rest: LAB1F_3 sets
     * $A7 = 167, against $A6 = 166 for level start and launch. */
    objects[b].y_coord = (unsigned char)(BAT_Y - BALL_H_PX + 1);
    objects[b].x_coord_hi = 0;
    objects[b].y_coord_hi = 0;
    sound_queue(SND_BAT_BEAT);
}

/* Hand over to the other player. orig: current_level_2up_copier
 * ($BE0C), which exchanges the live grid with the arriving player's
 * level slot and then FALLS THROUGH into players_swap — one call does
 * the grid, the counters and the turn toggle. The port's counters are
 * indexed rather than swapped, so only the grid moves here.
 *
 * The original's `LD A,(lives_2up) / AND A / RET Z` guard covers both
 * halves, which is how a solo player keeps playing; the same guard is
 * the `lives <= 0` test below. */
static int two_player_turn_change(int after_game_over) {
    const unsigned char other = (unsigned char)(1 - active_player);
    if (game_mode != 1) return 0;                /* 1 = 2 Players */
    if (players[other].lives <= 0) return 0;
    memcpy(player_grid[active_player], live_level, LVL_CELLS);
    player_grid_valid[active_player] = 1;
    active_player = other;
    resume_player_grid = player_grid_valid[other];
    if (after_game_over) turn_changes_over++;
    else                 turn_changes_life++;
    return 1;
}

/* Blow up the bat, take a life, and put a new ball on it. The two
 * checks are separate: with the last life gone there is nothing to
 * respawn onto, and game-over fires on the next frame.
 *
 * In 2-player mode a life loss ALSO ends the turn — the original's
 * life-loss path is `DEC lives / JR Z,LBC10_6 / CALL Z,
 * current_level_2up_copier / JP LB9E8_1`, so with lives left it swaps
 * and re-enters the level rather than respawning in place. Respawning
 * is skipped here for that reason: run_level re-enters the level, which
 * puts a fresh ball on the bat anyway. */
static void lose_a_life(void) {
    play_bat_explosion(current_level_idx_var);
    take_a_life();
    if (player.lives > 0) {
        /* Whether the turn actually ends is two_player_turn_change's
         * decision alone, and run_level respawns if it declines. Do not
         * re-test `players[other].lives > 0` here: with two checks of one
         * condition, a mutation of the real guard survives untested. */
        if (game_mode == 1) {
            pending_turn_change = 1;
            return;
        }
        respawn_primary_ball();
    }
}

/* The primary ball has fallen past the bat. The original deactivates it
 * and decrements balls_quantity; only when that hits 0 does LBC10 fire
 * the explosion — so an extra ball still in flight lets the player
 * survive this fall. orig: LA27E_25 */
static void lose_primary_ball(void) {
    magnet_ball_state_clear(0);          /* LA27E_25 zeroes the LA270 pair */
    if (ball.extra2_active || ball.extra3_active) {
        BALL_HIDE();
        return;
    }
    lose_a_life();
}

/* Park the ball on the bat at its catch offset. The bottom row touches
 * the bat top: $A6 = 166, the original's launch rest at LA27E_15. A ball
 * HELD by the MAGNET bonus rests 1 px lower at $A7 = 167 (LAB1F_3), and
 * the bat's active bonus ($03 = MAGNET, the original's IY+$14) tells the
 * two apart without a separate caught-state flag.
 *
 * BALL_H_PX, not the effective ball size: using the latter put the ball
 * at 165 every frame, silently clobbering respawn_primary_ball's $A6. */
static void rest_ball_on_bat(int b, int bat) {
    objects[b].x_coord =
        (unsigned char)(objects[bat].x_coord + ball.stuck_offset_x[b]);
    objects[b].y_coord = (unsigned char)(BAT_Y - BALL_H_PX +
             (objects[bat].bonus_applied == 0x03 ? 1 : 0));
}

/* Is this step landing the descending ball on the bat?
 *
 * The Y test uses the ball's HEIGHT (7), not eff_ball_size (8), and a
 * STRICT `>`. The original fires LAB1F when obj_compare reports Y
 * overlap, which (LAC22: 166 - ball_y borrows) is exactly ball_y >= 167,
 * i.e. next_y + 7 > bat_top of 173.
 *
 * Matching the fire FRAME is what makes the ball's x — and so the
 * deflection zone derived from it — match the Spectrum. Firing at >=
 * (y = 166), or using the width instead of the height, fires one frame
 * early at a smaller x and shifts the zone on shallow descents: dir
 * $08 left of centre came out $24 instead of $28.
 * See notes/bat-deflection.md. */
/* LAB1F tries bat 1 and, in Double Play only, bat 2:
 *
 *   LD IY,object_bat_1 / CALL obj_compare / JR C,LAB1F_0
 *   LD A,(game_mode) / CP $02 / RET NZ
 *   LD IY,object_bat_2 / CALL obj_compare / RET NC
 *
 * Order matters: bat 1 wins an overlap. */
static bool ball_lands_on_bat_2(int next_x, int next_y, int ball_sz) {
    const Object &b2 = objects[OBJ_BAT_2];
    if (game_mode != 2 || !object_active(b2)) return false;
    return ball.dy > 0
        && next_y + BALL_H_PX > (int)b2.y_coord
        && next_y < (int)b2.y_coord
        && next_x + ball_sz > (int)b2.x_coord
        && next_x < (int)b2.x_coord + (int)b2.w_body_px;
}

static bool ball_lands_on_bat(int next_x, int next_y, int ball_sz) {
    return ball.dy > 0
        && next_y + BALL_H_PX > BAT_Y
        && next_y < BAT_Y
        && next_x + ball_sz > eff_bat_left()
        && next_x < eff_bat_right();
}

/* Side walls. The ball is snapped flush to the wall rather than left
 * where the step put it, and the sub-pixel fraction is dropped with it —
 * carrying a fraction past a snap would drift the ball off the wall.
 * The right limit depends on the ball's size: 244 normally, 240 while
 * BIG_BALL is active. orig: change_direction's masks */
static void bounce_ball_off_side_walls(int *x, long *x_q8, int ball_sz) {
    const int x_max = PLAYFIELD_W - 8 - ball_sz;
    if (*x >= BALL_X_MIN && *x <= x_max) return;
    *x = (*x < BALL_X_MIN) ? BALL_X_MIN : x_max;
    *x_q8 = (long)*x << 8;
    ball_reflect_descriptor(1, 0);
}

/* The ceiling, same treatment. There is no floor here: a ball past the
 * bottom is a lost ball, handled by lose_primary_ball. */
static void bounce_ball_off_ceiling(int *y, long *y_q8) {
    if (*y >= BALL_Y_TOP) return;
    *y = BALL_Y_TOP;
    *y_q8 = (long)*y << 8;
    ball_reflect_descriptor(0, 1);
}

/* Which axis, if any, the primary ball entered a brick through:
 *   0 = no hit
 *   1 = vertical   (caller flips dy)
 *   2 = horizontal (caller flips dx)
 *   3 = LAFFC already resolved it, position and direction included
 *
 * LAFFC is byte-exact where it fires but returns 0 for cases it does not
 * resolve — an unported two-cell straddle on a layout other than L3 —
 * so brick_collision backs it up and a ball can never pass through a
 * brick LAFFC declined. On L3 the fallback never triggers, so parity is
 * unchanged. BATTY_LEGACY_COLLISION drops back to brick_collision alone. */
static int sweep_bricks_for_primary(int next_x, int next_y) {
    int hit;
    if (!dbg.use_laffc)
        return brick_collision(BALL_X, BALL_Y, next_x, next_y, BALL_PRIMARY);
    hit = laffc_collision(&objects[OBJ_BALL_1], BALL_X, BALL_Y, next_x, next_y,
                          BALL_PRIMARY);
    if (hit == 0)
        hit = brick_collision(BALL_X, BALL_Y, next_x, next_y, BALL_PRIMARY);
    return hit;
}

/* Sweep the bricks along the ball's path and, on a hit, reverse the
 * direction and unwind the axis the ball entered through — putting both
 * the pixel position and its q8.8 fraction back where the collision
 * decided they belong. */
static void resolve_primary_brick_hit(int *next_x, int *next_y,
                                      long *next_x_q8, long *next_y_q8) {
    const int hit = sweep_bricks_for_primary(*next_x, *next_y);
    if (hit == 3) {
        /* LAFFC path already reflected the direction and snapped the
         * ball to the cell edge (in BALL_X/BALL_Y). LAFFC_26-29 set
         * only the pixel byte (IX+$02 / IX+$04) and LEAVE the q8.8
         * fraction from the move untouched, so keep the moved low
         * byte rather than zeroing it — matching the original's
         * sub-pixel accumulation (probed: x frac 9, y frac 72). */
        *next_x_q8 = ((long)BALL_X << 8) | (*next_x_q8 & 0xFF);
        *next_y_q8 = ((long)BALL_Y << 8) | (*next_y_q8 & 0xFF);
        *next_x = BALL_X;
        *next_y = BALL_Y;
    } else if (hit == 1) {
        ball_reflect_descriptor(0, 1);
        *next_y = BALL_Y;
        *next_y_q8 = ((long)BALL_Y << 8) + objects[OBJ_BALL_1].y_coord_hi;
    } else if (hit == 2) {
        ball_reflect_descriptor(1, 0);
        *next_x = BALL_X;
        *next_x_q8 = ((long)BALL_X << 8) + objects[OBJ_BALL_1].x_coord_hi;
    }
}

/* The primary ball has reached the bat's top edge. Snap it to rest
 * height and either CATCH it or deflect it.
 *
 * Returns 1 if the ball was caught, which fully handles the frame — the
 * caller must return without moving the ball any further. Returns 0
 * after a normal deflection, leaving *next_y snapped and the direction
 * rewritten for the caller to commit. */
static int deflect_ball_off_bat(int next_x, int *next_y) {
    int dx_q8, dy_q8;
    *next_y = BAT_Y - BALL_H_PX;   /* rests at $A6 = 166 */
    /* MAGNET/CATCH bonus (BAT+$14 == $03, LAB1F_1..3). Only a
     * NORMAL-width bat catches — the original gates on width $1C, so a
     * big bat falls through to the ordinary deflection. */
    if (objects[OBJ_BAT_1].bonus_applied == 0x03 && bat1.extra_px == 0) {
        catch_ball_on_bat(BALL_PRIMARY, OBJ_BAT_1, next_x);
        return 1;
    }
    /* No `ball.dy` store here: the deflection below rewrites the
     * direction and calls refresh_ball_motion_signs unconditionally, with
     * no return in between, so such a store is dead (known-bugs.md #14).
     *
     * Offset is ball_x + 3 - bat_x, from the bat object's left edge (the
     * original's IY+$02); an enlarged bat selects the LABFC table. */
    /* LAB1F_0 re-owns the ball to the side of the bat that hit it,
     * BEFORE the deflection:
     *
     *   RES 7,(IX+$12)      ; ball's owner bit
     *   BIT 7,(IY+$02)      ; the HITTING bat's x
     *   JR Z,LAB1F_1
     *   SET 7,(IX+$12)
     *
     * IY is whichever bat obj_compare matched, so in Double Play the
     * ball changes hands on every deflection. Outside it, bat 1's x is
     * the only source and the bit follows the bat across the middle —
     * harmless, since add_points_to_score ignores it unless mode $02. */
    ball_owner_side[BALL_PRIMARY] = (unsigned char)((BAT_X & 0x80) ? 1 : 0);
    objects[OBJ_BALL_1].dir =
        bat_deflect_dir(objects[OBJ_BALL_1].dir,
                        next_x + 3 - BAT_X, bat1.extra_px != 0);
    dir_to_dxdy(objects[OBJ_BALL_1].dir, objects[OBJ_BALL_1].speed,
                      &dx_q8, &dy_q8);
    refresh_ball_motion_signs(&objects[OBJ_BALL_1], dx_q8, dy_q8);
    sound_queue(SND_BAT_BEAT);            /* ball-on-bat */
    return 0;
}

/* Bat 2's half of LAB1F, the mirror of bat 1's above: it reads bat 2's
 * own bonus byte and its own x.
 *
 * The catch it can reach is still limited by the other end — the
 * stuck-ball system is written around the primary ball, so a catch here
 * parks THAT ball. PLAN.md WS6 item 2 scopes the rest. */
static int deflect_ball_off_bat_2(int next_x, int *next_y) {
    const Object &b2 = objects[OBJ_BAT_2];
    int dx_q8, dy_q8;
    *next_y = (int)b2.y_coord - BALL_H_PX;
    if (b2.bonus_applied == 0x03) {
        catch_ball_on_bat(BALL_PRIMARY, OBJ_BAT_2, next_x);
        return 1;
    }
    /* LAB1F_0 again: the owner follows the bat that hit it. */
    ball_owner_side[BALL_PRIMARY] =
        (unsigned char)((b2.x_coord & 0x80) ? 1 : 0);
    objects[OBJ_BALL_1].dir =
        bat_deflect_dir(objects[OBJ_BALL_1].dir,
                        next_x + 3 - (int)b2.x_coord, false);
    dir_to_dxdy(objects[OBJ_BALL_1].dir, objects[OBJ_BALL_1].speed,
                      &dx_q8, &dy_q8);
    refresh_ball_motion_signs(&objects[OBJ_BALL_1], dx_q8, dy_q8);
    sound_queue(SND_BAT_BEAT);
    return 0;
}

static void step_ball(void) {
    int next_x, next_y;
    int dx_q8, dy_q8;
    long next_x_q8, next_y_q8;
    int ball_sz   = eff_ball_size();
    if (ball.stuck[BALL_PRIMARY]) {
        rest_ball_on_bat(BALL_PRIMARY, ball.stuck_bat[BALL_PRIMARY]);
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
    refresh_ball_motion_signs(&objects[OBJ_BALL_1], dx_q8, dy_q8);
    bounce_ball_off_side_walls(&next_x, &next_x_q8, ball_sz);
    bounce_ball_off_ceiling(&next_y, &next_y_q8);
    if (ball_lands_on_bat(next_x, next_y, ball_sz)) {
        if (deflect_ball_off_bat(next_x, &next_y)) return;
    } else if (ball_lands_on_bat_2(next_x, next_y, ball_sz)) {
        if (deflect_ball_off_bat_2(next_x, &next_y)) return;
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
        lose_primary_ball();
        return;
    }
    /* Side-aware brick collision: the sweep says which axis the ball
     * entered through, and we reverse and unwind that axis. */
    resolve_primary_brick_hit(&next_x, &next_y, &next_x_q8, &next_y_q8);
    BALL_X = next_x;
    BALL_Y = next_y;
    objects[OBJ_BALL_1].x_coord_hi = (unsigned char)(next_x_q8 & 0xFF);
    objects[OBJ_BALL_1].y_coord_hi = (unsigned char)(next_y_q8 & 0xFF);
}

/* Step an extra (TRIPLE_BALL) ball. UNIFIED with the primary: the
 * original runs ONE handling_ball for every ball, so the extras use the
 * same q8.8 + dir motion (dir_to_dxdy), wall reflect, brick collision
 * (LAFFC) and bat deflection (LAB1F) as step_ball, reading dir/q8.8 from
 * the object table. Only the life-decrement path is omitted: an extra
 * just deactivates off the bottom. */
/* Defined with the other stuck-ball handling, further down. */
static void ride_stuck_ball_on_bat(int b, int bat);

/* One bat's worth of LAB1F for a SECONDARY ball: contact test, then
 * either the catch or the deflection.
 *
 *   0  no contact — the caller should try the next bat
 *   1  deflected  — *next_y snapped, direction rewritten
 *   2  caught     — the caller must stop the frame
 *
 * Three outcomes, not two, and that is load-bearing. LAB1F falls
 * through to bat 2 only when bat 1 did not OVERLAP, so a bat-1
 * DEFLECTION has to stop the search as firmly as a catch does. A
 * boolean "was it caught" collapses those two, and the first draft of
 * this did exactly that: a ball deflected by bat 1 was then offered to
 * bat 2 and could be handled twice in one frame.
 *
 * Factored out because bat 2 needs exactly the same thing with
 * different extents. */
static int extra_ball_meets_bat(Object *o, int bat_idx, int slot,
                                int bat_left, int bat_right, int bat_top,
                                int ball_sz, bool big,
                                int next_x, int *next_y, int dy_q8) {
    if (!(dy_q8 > 0
          && *next_y + BALL_H_PX > bat_top
          && *next_y < bat_top
          && next_x + ball_sz > bat_left
          && next_x < bat_right))
        return 0;

    if (objects[bat_idx].bonus_applied == 0x03 && !big) {
        catch_ball_on_bat(slot, bat_idx, next_x);
        return 2;
    }
    *next_y = bat_top - BALL_H_PX;
    /* LAB1F_0 on this ball: the owner follows the bat that hit it. */
    ball_owner_side[slot] =
        (unsigned char)((objects[bat_idx].x_coord & 0x80) ? 1 : 0);
    o->dir = bat_deflect_dir(o->dir, next_x + 3 - (int)objects[bat_idx].x_coord,
                             big);
    sound_queue(SND_BAT_BEAT);
    return 1;
}

static void step_extra_ball(unsigned char *in_active,
                             unsigned char obj_idx) {
    Object *o = &objects[obj_idx];
    int next_x, next_y, dx_q8, dy_q8, hit;
    long next_x_q8, next_y_q8;
    int bat_left  = eff_bat_left();
    int bat_right = eff_bat_right();
    int bat_top   = BAT_Y;
    int ball_sz   = eff_ball_size();
    int x_max     = PLAYFIELD_W - 8 - ball_sz;
    if (!*in_active) return;
    /* Held on a bat: ride it and count toward the auto-launch, exactly
     * as the primary does. Before the magnet block for the same reason
     * step_ball puts it there — a resting ball cannot overlap a magnet
     * box, since the boxes end far above the bat. */
    if (ball.stuck[obj_idx]) {
        ride_stuck_ball_on_bat(obj_idx, ball.stuck_bat[obj_idx]);
        o->x_coord_hi = 0;
        o->y_coord_hi = 0;
        return;
    }
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
    /* Bat: LAB1F contact (ball_y >= 167), catch or deflect. The original
     * runs one handling_ball per ball and LAB1F does not care which one
     * it is, so an extra meets a bat exactly as the primary does —
     * including bat 2 in mode $02, which it tries only when bat 1
     * missed, mirroring LAB1F's own fall-through order. */
    {
        int met = extra_ball_meets_bat(o, OBJ_BAT_1, obj_idx,
                                       bat_left, bat_right, bat_top,
                                       ball_sz, bat1.extra_px != 0,
                                       next_x, &next_y, dy_q8);
        if (met == 0 && game_mode == 2
            && !(objects[OBJ_BAT_2].sprite_set & 0x80)) {
            const Object &b2 = objects[OBJ_BAT_2];
            met = extra_ball_meets_bat(o, OBJ_BAT_2, obj_idx,
                                       bat_left_of(OBJ_BAT_2),
                                       bat_right_of(OBJ_BAT_2),
                                       (int)b2.y_coord, ball_sz,
                                       bats[BAT_SLOT(OBJ_BAT_2)].extra_px != 0,
                                       next_x, &next_y, dy_q8);
        }
        if (met == 2) return;       /* held: it does not move this frame */
    }
    if (next_y >= PLAYFIELD_H) {        /* off the bottom: deactivate */
        magnet_ball_state_clear((unsigned char)(obj_idx == OBJ_BALL_2 ? 1 : 2));
        *in_active = 0;
        o->sprite_set = 0x82;
        return;
    }
    hit = laffc_collision(o, o->x_coord, o->y_coord, next_x, next_y,
                          (int)(o - objects));
    if (hit == 0) hit = brick_collision(o->x_coord, o->y_coord, next_x, next_y,
                                        (int)(o - objects));
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
    step_extra_ball(&ball.extra2_active, OBJ_BALL_2);
}

static void step_ball3(void) {
    step_extra_ball(&ball.extra3_active, OBJ_BALL_3);
}

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
    bat_sprite_bounds(BAT_PREV_X, bat1.drawn_extra_px, &old_x0, &old_x1);
    bat_sprite_bounds(BAT_X, bat1.extra_px, &new_x0, &new_x1);
    if (new_x0 < old_x0) old_x0 = new_x0;
    if (new_x1 > old_x1) old_x1 = new_x1;
    /* In Double Play the window has to span whatever bat 2 vacated and
     * whatever it now covers, or its old sprite is never painted over.
     * Both bats share the band, so one window serves — at the cost of a
     * wide flush when they are at opposite ends, which is the same cost
     * the full path would pay anyway. */
    if (game_mode == 2 && object_active(objects[OBJ_BAT_2])) {
        const BatState &st2 = bats[BAT_SLOT(OBJ_BAT_2)];
        bat_sprite_bounds((int)objects[OBJ_BAT_2].prev_x, st2.drawn_extra_px,
                          &new_x0, &new_x1);
        if (new_x0 < old_x0) old_x0 = new_x0;
        if (new_x1 > old_x1) old_x1 = new_x1;
        bat_sprite_bounds((int)objects[OBJ_BAT_2].x_coord, st2.extra_px,
                          &new_x0, &new_x1);
        if (new_x0 < old_x0) old_x0 = new_x0;
        if (new_x1 > old_x1) old_x1 = new_x1;
    }
    byte_lo = old_x0 >> 3;
    byte_hi = (old_x1 + 7) >> 3;
    byte_lo--;
    byte_hi++;
    if (byte_lo < 0) byte_lo = 0;
    if (byte_hi > 32) byte_hi = 32;
    if (byte_lo >= byte_hi) return;

    paint_bg_window_to_buff(bg_attr, cycle, BAT_Y, BAT_H_PX,
                            byte_lo, byte_hi - 1);
    restore_inner_border_line(BAT_Y, BAT_H_PX, byte_lo, byte_hi - 1);
    paint_frame_to_buff(cycle, current_level_idx_var);
    render_bats(cycle, bg_attr);
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

static void mark_live_bullets_dirty(void) {
    int i;
    for (i = 0; i < N_BULLETS; i++)
        if (bullet_active[i])
            mark_dirty_rect_px(bullet_x[i], bullet_y[i],
                               BULLET_W_PX, BULLET_H_PX);
}

/* Blast sprites are 8px wide and at most 8 rows tall (sprites_blob
 * headers: frames are 6/7/8/8). mark_dirty_rect_px rounds X out to byte
 * boundaries but takes Y exactly, and a blast writes no attrs, so 8
 * rows is the whole sprite. See known-bugs.md #9 for why the two paths
 * once disagreed here. */
static void render_bullet_blasts_to_buff_and_mark(void) {
    int i;
    if (!any_bullet_blast()) return;
    render_bullet_blast_to_buff();
    for (i = 0; i < N_BULLETS; i++) {
        if (bullet_blast_ticks[i])
            mark_dirty_rect_px(bullet_blast_x[i], bullet_blast_y[i], 16, 8);
    }
}

/* Multi-ball extras: full 16x12 moving balls, same dirty treatment as
 * the primary (the carry erases last frame's position). */
static void render_extra_balls_to_buff(unsigned char bg_attr) {
    if (ball.extra2_active) {
        render_ball_to_buff(objects[OBJ_BALL_2].x_coord,
                            objects[OBJ_BALL_2].y_coord, bg_attr);
        mark_dirty_rect_px(objects[OBJ_BALL_2].x_coord,
                           objects[OBJ_BALL_2].y_coord, 16, 12);
    }
    if (ball.extra3_active) {
        render_ball_to_buff(objects[OBJ_BALL_3].x_coord,
                            objects[OBJ_BALL_3].y_coord, bg_attr);
        mark_dirty_rect_px(objects[OBJ_BALL_3].x_coord,
                           objects[OBJ_BALL_3].y_coord, 16, 12);
    }
}

/* The three single-sprite falling objects, blitted and dirty-marked the
 * same way on every redraw path. The bomb's bat-collision kill belongs
 * to step_bomb, not here. */
static void render_falling_objects_to_buff(unsigned char bg_attr) {
    if (bomb.active) {
        blit_masked_to_scr_buff(spr_bomb_data, bomb.x, bomb.y);
        mark_dirty_rect_px(bomb.x, bomb.y, 16, 16);
    }
    if (pts_marker.active) {
        blit_masked_to_scr_buff(pts_marker.sprite, pts_marker.x, pts_marker.y);
        mark_dirty_sprite_rect(pts_marker.sprite, pts_marker.x, pts_marker.y);
    }
    if (bonus.active) {
        unsigned int spr = spr_for_bonus(bonus.type);
        render_bonus_to_buff(bg_attr);
        mark_dirty_sprite_rect(spr, bonus.x, bonus.y);
    }
}

static void render_enemy_to_buff_and_mark(unsigned char bg_attr);

/* Push the composed frame to VGA.
 *
 * Normally the carry unions this frame's dirty rects with last frame's,
 * so the sprites' previous positions are erased as well as their new
 * ones drawn. A full flush pushes every row instead — but it still
 * copies this frame's rects forward first, because mark_all_dirty()
 * widens what goes out now and not what the NEXT frame restores. */
static void flush_composed_frame(void) {
    if (cache.full_flush) {
        int y, s;
        for (y = 0; y < PLAYFIELD_H; y++) {
            for (s = 0; s < DIRTY_SLOTS; s++) {
                prev_dirty_min_byte[s][y] = dirty_min_byte[s][y];
                prev_dirty_max_byte[s][y] = dirty_max_byte[s][y];
            }
        }
        mark_all_dirty();
        cache.full_flush = 0;
    } else {
        carry_dirty_with_previous();
    }
    flush_dirty_to_vga();
}

/* The brick band's transients: the destruction marker and the
 * multi-hit animations. Neither draws anything itself — the marker is
 * dirty-rect scheduling only — but both must be marked, because the
 * background beneath them was repainted.
 *
 * The flash rect is one pixel bigger on every side (18x10 around an
 * 16x8 cell), which is what test-brick-flash greps for. */
static void render_brick_effects_and_mark(void) {
    int i;
    render_brick_flash_to_buff();
    if (brick_flash.ticks) {
        mark_dirty_rect_px(brick_flash.x - 1, brick_flash.y - 1, 18, 10);
    }
    render_brick_hit_anim_to_buff();
    for (i = 0; i < BRICK_HIT_ANIM_SLOTS; i++) {
        if (!brick_hit_anim_ticks[i]) continue;
        mark_dirty_rect_px(8 + (int)brick_hit_anim_col[i] * 16,
                           32 + (int)brick_hit_anim_row[i] * 8,
                           16, 8);
    }
}

/* Anything about the bat that changes its pixels, not just its position:
 * a resize, a caught bonus (the body sprite carries it) or a laser
 * fire-animation frame all need the whole body redrawn. */
static int bat_changed(int b) {
    const BatState &st = bats[BAT_SLOT(b)];
    return (objects[b].x_coord != objects[b].prev_x)
        || ((int)objects[b].y_coord != st.drawn_y)
        || (st.extra_px != st.drawn_extra_px)
        || (objects[b].bonus_applied != st.drawn_bonus)
        || (st.fire_anim_ticks != st.drawn_fire_ticks);
}

static int bat_needs_full_redraw(void) {
    return bat_changed(OBJ_BAT_1)
        || (game_mode == 2 && bat_changed(OBJ_BAT_2));
}

/* Draw the bat on the full path and mark what it dirtied. When the body
 * changed, its previous footprint is restored from the static cache
 * first and the union of both positions is marked; otherwise only the
 * running-dot row needs flushing. */
static void compose_bat_full(unsigned char cycle, unsigned char bg_attr,
                             int bat_full_dirty) {
    int bat_x0, bat_x1, old_x0, old_x1;

    if (bat_full_dirty) {
        bat_sprite_bounds(BAT_PREV_X, bat1.drawn_extra_px, &old_x0, &old_x1);
        restore_static_cache_rect_bytes(bat1.drawn_y, 13,
                                        old_x0 >> 3, (old_x1 - 1) >> 3);
        mark_dirty_rect_px(old_x0, bat1.drawn_y, old_x1 - old_x0, 13);
        if (game_mode == 2 && object_active(objects[OBJ_BAT_2])) {
            const BatState &st2 = bats[BAT_SLOT(OBJ_BAT_2)];
            bat_sprite_bounds((int)objects[OBJ_BAT_2].prev_x,
                              st2.drawn_extra_px, &old_x0, &old_x1);
            restore_static_cache_rect_bytes(st2.drawn_y, 13,
                                            old_x0 >> 3, (old_x1 - 1) >> 3);
            mark_dirty_rect_px(old_x0, st2.drawn_y, old_x1 - old_x0, 13);
        }
    }

    render_bats(cycle, bg_attr);
    render_running_dot();

    if (game_mode == 2 && object_active(objects[OBJ_BAT_2])) {
        const BatState &st2 = bats[BAT_SLOT(OBJ_BAT_2)];
        bat_sprite_bounds((int)objects[OBJ_BAT_2].x_coord, st2.extra_px,
                          &bat_x0, &bat_x1);
        mark_dirty_rect_px(bat_x0, (int)objects[OBJ_BAT_2].y_coord,
                           bat_x1 - bat_x0, 13);
    }
    bat_sprite_bounds(BAT_X, bat1.extra_px, &bat_x0, &bat_x1);
    if (bat_full_dirty) {
        bat_sprite_bounds(BAT_PREV_X, bat1.drawn_extra_px, &old_x0, &old_x1);
        if (old_x0 < bat_x0) bat_x0 = old_x0;
        if (old_x1 > bat_x1) bat_x1 = old_x1;
        mark_dirty_rect_px(bat_x0, BAT_Y, bat_x1 - bat_x0, 13);
    } else {
        mark_dirty_rect_px(bat_x0, BAT_Y + 6, bat_x1 - bat_x0, 1);
    }
}

/* Bring the static cache up to date for this frame: rebuild it whole,
 * rebuild just the brick band, or restore from it unchanged.
 *
 * A score change alone does not force a full rebuild — the HUD top can
 * be patched in place instead, but only on levels with no magnets,
 * since a magnet may overlap the HUD rows and would be repainted over.
 * A lives change always forces one, because the indicators sit in the
 * bat band rather than the patchable strip.
 *
 * That last rule is about the INDICATORS and nothing else. It is not a
 * general "something happened, repaint" hook, though it worked as one by
 * accident for months: every bat explosion was followed by a life
 * decrement, so the rebuild the explosion needed arrived for free and no
 * caller had to ask for it. The first build where a death did not change
 * the counter (BATTY_INFINITE_LIVES) left the bat in fragments and the
 * magnets missing. The death path now asks explicitly —
 * invalidate_static_cache_after_death — and this stays what its name
 * says it is. */
static void refresh_static_background(unsigned char level_idx) {
    const int score_dirty = (players[0].score != cache.drawn_score[0]
                          || players[1].score != cache.drawn_score[1]
                          || high_score != cache.drawn_high_score);
    const int lives_dirty = (player.lives != cache.drawn_lives);
    const int can_patch_hud = (magnets_per_level[level_idx][0] == 0);

    if (cache.full_flush || lives_dirty || (score_dirty && !can_patch_hud)) {
        cache.bg_dirty = 1;
    }

    /* Clear BEFORE the branch, so build_static_brick_band_cache's window
     * mark survives into this frame's flush. */
    clear_dirty_ranges(dirty_min_byte, dirty_max_byte);

    if (cache.bg_dirty) {
        build_static_background(level_idx);
        cache.bg_dirty = 0;
        cache.drawn_score[0] = players[0].score;
        cache.drawn_score[1] = players[1].score;
        cache.drawn_high_score = high_score;
        cache.drawn_lives = player.lives;
        cache.full_flush = 1;
    } else {
        if (cache.band_dirty) build_static_brick_band_cache(level_idx);
        restore_prev_dirty_from_static_cache();
    }

    /* Runs after a full rebuild too — redundant there, since the rebuild
     * repainted the HUD and set full_flush, but kept because that is what
     * the original code did and nothing proves the redundancy. */
    if (score_dirty && can_patch_hud) {
        update_static_hud_top(level_idx);
        cache.drawn_score[0] = players[0].score;
        cache.drawn_score[1] = players[1].score;
        cache.drawn_high_score = high_score;
        /* The digits reach to row 28, not row 23 — HUD_PATCH_H_PX. */
        mark_dirty_bytes(0, HUD_PATCH_H_PX, 0, 31);
    }
}

static void compose_moving_objects(unsigned char bg_attr);

static void redraw_full_with_ball(unsigned char level_idx) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle   = (unsigned char)(level_idx & 3);
    int bat_full_dirty;

    prof_start();

    bat_full_dirty = bat_needs_full_redraw();
    refresh_static_background(level_idx);
    /* A magnet toggled this frame: redraw its circle now, while scr_buff
     * holds clean background in the window (objects not yet drawn), and
     * bake it into the static bg cache. After a full static rebuild the
     * blit is redundant (render_magnets painted from state) but
     * harmless — and the dirty mark is still needed. */
    apply_magnet_toggle_visual();
    /* Repair the top-frame centre (bytes 8..10, rows 0..23) BEFORE any
     * moving object is composed. At the END of the compose it instead
     * ERASES the slice of any sprite overlapping the frame centre, and an
     * alien or ball transiting x 64..87 / y < 24 flickers out on every
     * full-path frame. */
    restore_top_frame_center(cycle, level_idx);
    prof.bg_pit += prof_elapsed();

    /* The frame itself is static and baked into bg_scr_buff/bg_attr_buff. */
    prof.frame_pit += prof_elapsed();

    if (BALL_VISIBLE) {
        render_ball_to_buff(BALL_X, BALL_Y, bg_attr);
        mark_dirty_rect_px(BALL_X, BALL_Y, 16, 12);
    }
    compose_bat_full(cycle, bg_attr, bat_full_dirty);
    remember_bat_draw_state();

    /* Lives and HUD are static in the cached background and are rebuilt
     * only when score/lives/brick-animation invalidation requires it. */
    prof.hud_pit += prof_elapsed();

    render_brick_effects_and_mark();

    /* Slot-paint order and why it is shared: see compose_moving_objects. */
    compose_moving_objects(bg_attr);
    /* The rocket is the last slot ($9BAC) and paints over everything.
     * It lives here rather than in the shared function because it is
     * full-path-only — see there. */
    if (rocket.active) {
        unsigned int spr = current_rocket_spr();
        render_rocket_to_buff();
        mark_dirty_sprite_rect(spr, rocket.x, rocket.y);
    }
    prof.bricks_pit += prof_elapsed();

    flush_composed_frame();
    prof.vga_pit += prof_elapsed();

    prof.frames++;
    prof.full_dynamic_frames++;
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
    if (dbg.ball_full_redraw || dbg.bat_full_redraw) blockers |= BALL_DIRTY_BLOCK_FORCED;
    if (bat_moved) blockers |= BALL_DIRTY_BLOCK_BAT;
    /* A hidden primary can't be redrawn (nothing to draw) -> full path. A
     * STUCK ball, though, is visible and rides the bat at a known position
     * (BALL_X/Y set each frame in the stuck handler), so it redraws fine on
     * the dirty path like a moving ball — no need to force full (helps the
     * MAGNET-hold + pre-launch states). */
    if (!BALL_VISIBLE) blockers |= BALL_DIRTY_BLOCK_BALLS;
    if (cache.bg_dirty || cache.band_dirty || cache.full_flush) blockers |= BALL_DIRTY_BLOCK_STATIC;
    if (players[0].score != cache.drawn_score[0]
        || players[1].score != cache.drawn_score[1]
        || high_score != cache.drawn_high_score
        || player.lives != cache.drawn_lives) blockers |= BALL_DIRTY_BLOCK_HUD;
    if (bonus.active || pts_marker.active || bomb.active || rocket.active) blockers |= BALL_DIRTY_BLOCK_OBJECTS;
    if (objects[OBJ_ENEMY].sprite_set != 0) blockers |= BALL_DIRTY_BLOCK_OBJECTS;
    if (any_bullet_active() || any_bullet_blast()) blockers |= BALL_DIRTY_BLOCK_OBJECTS;
    if (brick_flash.ticks || any_brick_hit_anim()) blockers |= BALL_DIRTY_BLOCK_BRICKS;
    /* Extra balls (multi-ball) are full moving sprites like the primary —
     * route them to the simple-object dirty tier (OBJECTS), not a full
     * recompose. big-ball is the PRIMARY ball with a different sprite of the
     * SAME 16×12 footprint (verified: SPR_BIG_BALL/SPR_BALL_NORMAL both
     * 2 bytes × 12 rows), already drawn by render_ball_to_buff + covered by
     * the primary's 16×12 dirty mark — so it needs no blocker at all. */
    if (ball.extra2_active || ball.extra3_active) blockers |= BALL_DIRTY_BLOCK_OBJECTS;
    /* Resize transitions force a full frame: the bat changes width and
     * the vacated area needs restoring. The laser fire-anim does not —
     * redraw_bat_dirty handles it on the dirty path. */
    if (bat1.extra_px != bat1.extra_target) blockers |= BALL_DIRTY_BLOCK_BAT_FX;
    return blockers;
}

static void prof_note_ball_dirty_blockers(unsigned int blockers) {
    if (blockers & (BALL_DIRTY_BLOCK_BAT | BALL_DIRTY_BLOCK_FORCED))
        prof.blocked_by_bat++;
    if (blockers & BALL_DIRTY_BLOCK_STATIC)
        prof.blocked_by_static++;
    if (blockers & BALL_DIRTY_BLOCK_HUD)
        prof.blocked_by_hud++;
    if (blockers & BALL_DIRTY_BLOCK_OBJECTS)
        prof.blocked_by_objects++;
    if (blockers & BALL_DIRTY_BLOCK_BRICKS)
        prof.blocked_by_bricks++;
    if (blockers & BALL_DIRTY_BLOCK_BALLS)
        prof.blocked_by_balls++;
    if (blockers & BALL_DIRTY_BLOCK_BAT_FX)
        prof.blocked_by_bat_fx++;
}

static int can_redraw_ball_with_simple_objects(unsigned int blockers) {
    if ((blockers & ~BALL_DIRTY_BLOCK_OBJECTS) != 0) return 0;
    if (!bonus.active && !pts_marker.active && objects[OBJ_ENEMY].sprite_set == 0
        && !any_bullet_active() && !any_bullet_blast() && !bomb.active
        && !ball.extra2_active && !ball.extra3_active) return 0;
    if (rocket.active) return 0;
    /* The +400 catch popup renders correctly only via the full path — in
     * the simple tier its drift and catch-frame transition leave a trail.
     * It is brief and rare, so route those frames to the full path. */
    if (pts_marker.active) return 0;
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

/* Compose the moving objects in the ORIGINAL's slot-paint order (the
 * $9AD0 table; call_for_all_obj walks it low->high, so later slots paint
 * ON TOP):
 *
 *   balls 1-3 < bullets < bats < bonus/bomb/pts400 (they share the $9B80
 *   slot) < ENEMY ($9B96) < rocket ($9BAC)
 *
 * Both redraw paths call this, which is the point: two copies of the
 * order drift apart, and with a fresh bomb still overlapping its parent
 * UFO they then render different pixels (the f50 21px A/B delta,
 * notes/bird-render-parity.md). One implementation cannot drift.
 *
 * The bullets are drawn unconditionally, which is safe only because
 * render_bullet_to_buff carries no animation state (known-bugs #10).
 *
 * The rocket ($9BAC, last, paints over everything) is deliberately NOT
 * here: it is full-path-only. `entities_need_redraw` returns true while
 * it is active and `can_redraw_ball_with_simple_objects` bails on it, so
 * no dirty-path frame ever needs to compose it, and only
 * `redraw_full_with_ball` does. */
static void compose_moving_objects(unsigned char bg_attr) {
    render_extra_balls_to_buff(bg_attr);
    render_bullet_to_buff();
    mark_live_bullets_dirty();
    render_bullet_blasts_to_buff_and_mark();
    render_falling_objects_to_buff(bg_attr);
    render_enemy_to_buff_and_mark(bg_attr);
}


static void render_simple_objects_to_buff_and_mark(unsigned char bg_attr) {
    /* The dirty path's object tier. Identical to the full path's by
     * construction — both go through compose_moving_objects, which is
     * where the slot order and its provenance are documented. */
    compose_moving_objects(bg_attr);
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
    bat_sprite_bounds(BAT_X, bat1.extra_px, &bat_x0, &bat_x1);
    if (bat1.fire_anim_ticks) {
        paint_bg_window_to_buff(bg_attr, cycle, BAT_Y, BAT_H_PX,
                                bat_x0 >> 3, (bat_x1 - 1) >> 3);
        restore_inner_border_line(BAT_Y, BAT_H_PX,
                                  bat_x0 >> 3, (bat_x1 - 1) >> 3);
        render_bats(cycle, bg_attr);
        render_running_dot();
        mark_dirty_rect_px(bat_x0, BAT_Y, bat_x1 - bat_x0, BAT_H_PX);
    } else {
        paint_bg_window_to_buff(bg_attr, cycle, BAT_Y + 6, 1,
                                bat_x0 >> 3, (bat_x1 - 1) >> 3);
        restore_inner_border_line(BAT_Y + 6, 1,
                                  bat_x0 >> 3, (bat_x1 - 1) >> 3);
        render_bats(cycle, bg_attr);
        render_running_dot();
        mark_dirty_rect_px(bat_x0, BAT_Y + 6, bat_x1 - bat_x0, 1);
    }
}

/* The dirty redraw: restore what last frame dirtied, repaint the ball
 * and bat, flush. `with_objects` adds the simple-object tier — the two
 * variants differ only by that call and which profile bucket they
 * count in. */
static void redraw_ball_dirty(unsigned char level_idx, bool with_objects) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle = (unsigned char)(level_idx & 3);

    prof_start();
    restore_prev_dirty_from_static_cache();
    clear_dirty_ranges(dirty_min_byte, dirty_max_byte);
    prof.bg_pit += prof_elapsed();

    render_ball_to_buff(BALL_X, BALL_Y, bg_attr);
    mark_dirty_rect_px(BALL_X, BALL_Y, 16, 12);
    redraw_bat_dirty(cycle, bg_attr);
    if (with_objects) render_simple_objects_to_buff_and_mark(bg_attr);
    prof.bricks_pit += prof_elapsed();

    carry_dirty_with_previous();
    flush_dirty_to_vga();
    prof.vga_pit += prof_elapsed();

    prof.frames++;
    if (with_objects) prof.ball_object_frames++;
    else              prof.ball_only_frames++;
}

static void redraw_ball_only(unsigned char level_idx) {
    redraw_ball_dirty(level_idx, false);
}

static void redraw_ball_with_simple_objects(unsigned char level_idx) {
    redraw_ball_dirty(level_idx, true);
}

/* Render a short string of N character codes via draw_glyph, anchored
 * top-left at screen (x, y). `codes` follow the markup encoding:
 * 0..9 = digits, 0x0A..0x23 = A..Z (see notes/encoding.md). */
static void draw_text(int x, int y, unsigned char colour,
                      const unsigned char *codes, int n) {
    int i;
    for (i = 0; i < n; i++) draw_glyph(x + i * 8, y, colour, codes[i]);
}

#ifndef BATTY_SCORELESS_HUD
static void draw_score_digits_original(int x, int y, unsigned long value) {
    unsigned char digits[6];
    int i;
    score_to_digits(value, digits);
    for (i = 0; i < 6; i++) {
        const unsigned char *digit = hud_sprites + HUD_SCORE_DIGITS + 2 + digits[i] * 16;
        int row;
        for (row = 0; row < HUD_DIGIT_H_PX; row++) {
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
    draw_score_digits_original(0x10, HUD_SCORE_Y, players[0].score);
    draw_score_digits_original(0x68, HUD_SCORE_Y, high_score);
    draw_score_digits_original(0xC0, HUD_SCORE_Y, players[1].score);
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
    /* "PLAYER  n" — the original's SECOND line of the game-over
     * message. `print_message` is called with B=$02 and txt_player_0
     * follows txt_game_over in memory; the digit is the byte at
     * txt_player_0+$0C, patched with `LD A,(player_number) / INC A`.
     * Nine glyphs, same width as GAME OVER, so it aligns under it. */
    unsigned char pl[9] = { 0x19, 0x15, 0x0A, 0x22, 0x0E, 0x1B,
                            0x26, 0x26, 0x00 };
    unsigned char digits[6];
    pl[8] = (unsigned char)(active_player + 1);
    fill(0, 0, SCREEN_W, SCREEN_H, 0);
    draw_text(BORDER_X + 4 * 8 + 4, BORDER_Y + 70, 15, go, (int)sizeof(go));
    /* The original stacks these 24 px apart ($4F then $67, both at
     * x=$60). This screen's layout is the PORT's, not the original's —
     * it also carries SCORE and HIGH lines the original does not have —
     * and 24 px would put this one on top of SCORE. 12 px keeps the
     * order and the alignment. See notes/parity-gaps.md. */
    draw_text(BORDER_X + 4 * 8 + 4, BORDER_Y + 82, 15, pl, (int)sizeof(pl));
    score_to_digits(player.score, digits);
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

/* The name-entry screen's furniture: everything that is drawn once and
 * never repainted. Ink per line is deliberate and gated —
 * test-name-entry-visual checks each band by ink, so a colour change
 * here is caught even when the position is right. */
#define NAME_ENTRY_X   (BORDER_X + 14 * 8)
#define NAME_ENTRY_Y   (BORDER_Y + 90)

static void draw_name_entry_screen(void) {
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
    fill(0, 0, SCREEN_W, SCREEN_H, 0);
    draw_text(BORDER_X + 7 * 8,  BORDER_Y + 50,  14, title,  (int)sizeof(title));
    draw_text(BORDER_X + 6 * 8,  BORDER_Y + 70,  15, prompt, (int)sizeof(prompt));
    draw_text(BORDER_X + 4 * 8,  BORDER_Y + 130, 13, hint,   (int)sizeof(hint));
}

/* Repaint the three-letter row. The slot being edited alternates between
 * normal and dim so the player can see which one the arrows move; the
 * other two are always ink 15, which is why the gate can require the row
 * to exist in ink 15 without depending on the blink phase. */
static void draw_name_row(int pos) {
    const unsigned char blink = (unsigned char)((blink_phase() == 0) ? 0 : 1);
    int i;
    fill(NAME_ENTRY_X - 2, NAME_ENTRY_Y - 2, 3 * 16 + 4, 12, 0);
    for (i = 0; i < 3; i++) {
        const unsigned char colour =
            (i == pos && blink) ? 8 /* dim */ : 15;
        draw_glyph(NAME_ENTRY_X + i * 16, NAME_ENTRY_Y, colour,
                   high_score_name[i]);
    }
}

/* Cycle one letter. The alphabet wraps between $0A and $23, so stepping
 * off either end lands on the other — there is no clamp. */
static void step_name_letter(int pos, int delta) {
    unsigned char c = high_score_name[pos];
    if (delta < 0) c = (c == 0x0A) ? 0x23 : (unsigned char)(c - 1);
    else           c = (c == 0x23) ? 0x0A : (unsigned char)(c + 1);
    high_score_name[pos] = c;
}

static void input_new_record_name(void) {
    int pos = 0;

    draw_name_entry_screen();
    high_score_name[0] = 0x0A;
    high_score_name[1] = 0x0A;
    high_score_name[2] = 0x0A;

    for (;;) {
        draw_name_row(pos);
        if (kbhit()) {
            int k = getch();
            if (k == KEY_ESC) return;
            if (k == KEY_EXT_PREFIX) {
                int ext = getch();
                if (ext == KEY_LEFT)       step_name_letter(pos, -1);
                else if (ext == KEY_RIGHT) step_name_letter(pos, +1);
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

/* Free a hit-animation slot whose brick is no longer there — destroyed
 * (cell bit 7) or out of range. metal_brik_anim's
 * `BIT 7,(HL) -> mark slot free`. True means the slot was released and
 * the caller should skip it.
 *
 * This is an EARLY out only: an animation that outlives nothing still
 * ends by itself after one ~15-tick pass.
 *
 * Both the stepper and the RENDERER call this, so the renderer can free
 * a slot. That is safe because freeing is idempotent, but it is the
 * same shape as known-bugs #10 — state changed from inside a draw — so
 * it is worth knowing it is deliberate. */
static bool release_brick_hit_anim_if_gone(int i) {
    const int col = brick_hit_anim_col[i];
    const int row = brick_hit_anim_row[i];
    if (row < LVL_ROWS && col < LVL_COLS
        && !(live_level[row * LVL_COLS + col] & 0x80)) return false;
    brick_hit_anim_ticks[i] = 0;
    return true;
}

static void step_brick_hit_anim(void) {
    int i;
    for (i = 0; i < BRICK_HIT_ANIM_SLOTS; i++) {
        if (!brick_hit_anim_ticks[i]) continue;
        if (release_brick_hit_anim_if_gone(i)) continue;
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
        if (release_brick_hit_anim_if_gone(i)) continue;
        col = brick_hit_anim_col[i];
        row = brick_hit_anim_row[i];
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
 * The original is NOT interruptible by input, and aborting on any
 * buffered key lets a held or typematic-repeating key at level entry
 * skip the animation almost entirely (known-bugs #4). Only ESC reacts,
 * as a port convention; other keys are left IN the BIOS buffer for the
 * main loop.
 *
 * The two do{}while edge-waits below are the HALT equivalent: always two
 * full interrupt edges. Waiting `pit_ticks()-t < 2` from a mid-tick
 * sample gives 1..2 ticks per frame instead. */
static int play_brik_anim(void) {
    int step;
    int ping_played = 0;    /* SMC trick in original: one_play_sound_metal_brik
                             * rewrites itself to RET after the first call, so
                             * the ping fires once even though spr_brik_5 appears
                             * twice in anim_brik. */
    probe.brik_anim_ticks = pit_ticks();
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
    probe.brik_anim_ticks = pit_ticks() - probe.brik_anim_ticks;
    sound_silence();
    return 0;
}

/* "PLAYER 1" + "ROUND XX" intro banner — port of show_window_round_number
 * at $8F60 + the pause_long B=4 wait at LB9E8_1. Draws an 80x32 black
 * panel centred between the brick zone and the bat. Original puts
 * PLAYER X at Y=$8F (= 143) and ROUND XX at Y=$9E (= 158). Holds for
 * ~1.2 s (60 PIT ticks) or until a key is pressed. ESC during the
 * wait returns 1 so the caller can quit. */
static void draw_round_banner(int round_num) {
    int banner_x = BORDER_X + 88;
    int banner_y = BORDER_Y + 133;
    int text_x   = BORDER_X + 96;
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
     * byte. Using the raw bytes (143/158) as top-Y instead jams both
     * lines against the box bottom. */
    draw_text(text_x, BORDER_Y + 138, 15, player_codes, 7);
    {
        /* orig: `LD A,(player_number) / INC A / LD (txt_player_x+11),A`. */
        unsigned char digit = (unsigned char)(active_player + 1);
        draw_text(text_x + 7 * 8, BORDER_Y + 138, 15, &digit, 1);
    }
    draw_text(text_x, BORDER_Y + 153, 15, round_codes, 8);
}

/* Hold the banner for 60 PIT ticks — about 1.2 s — or until a key.
 * Returns 1 if that key was ESC, which quits the game.
 *
 * BATTY_HOLD_ROUND_BANNER waits indefinitely instead, so a gate can
 * capture the banner without racing the timer. */
static int hold_round_banner(void) {
    unsigned long start;

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

static int show_round_banner(unsigned int round_num_display) {
    draw_round_banner((int)round_num_display);
    return hold_round_banner();
}

/* --- "Bat explodes" death animation -----------------------------------
 *
 * When the ball is lost the original spawns 10 sparks at the bat
 * position (LBC10 at $BC10) which fan out for ~46 frames then die. It
 * runs the per-frame loop until all sparks expire, then decrements
 * lives and respawns. Our port plays this as a self-contained sub-loop
 * driven by PIT ticks — the outer run_level enters it from the two
 * ball-lost sites (bomb-on-bat, ball-past-bat). */


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

/* The level scene as the death and rocket-tally sub-loops want it: no
 * bat, no ball, no magnets. The sparks stand in for the bat during a
 * death, and the tally needs the bricks it is counting still on screen.
 * Unlike compose_level_scene this paints the brick flash and hit
 * animations, since these sub-loops drive their own frames and there is
 * no dirty carry to restore them. */
static void compose_scene_no_objects(unsigned char level_idx) {
    unsigned char bg_attr = bg_attr_per_cycle[level_idx & 3];
    unsigned char cycle   = (unsigned char)(level_idx & 3);
    paint_bg_to_buff(bg_attr, cycle);
    paint_frame_to_buff(cycle, level_idx);
    render_lives(cycle, bg_attr);
    render_separator();
    render_hud_to_buff();
    inner_border_line_c();
    render_brick_band(level_idx);
    render_brick_flash_to_buff();
    render_brick_hit_anim_to_buff();
}

/* Single-frame render of the level scene + the active death sparks.
 * Same compose as redraw_full_with_ball but with the bat / ball / multi-
 * ball hidden (the sparks ARE the bat at this moment). */
static void redraw_with_death_sparks(unsigned char level_idx) {
    int i;
    compose_scene_no_objects(level_idx);
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
    compose_scene_no_objects(level_idx);
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
    /* orig: XOR A / LD (need_change_player),A before the sweep. */
    unsigned char left_brik_side = 0;
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
            /* The original splits these EVENLY, not by side: it zeroes
             * need_change_player before the sweep and does
             * `LD A,(need_change_player) / XOR $01 / LD (...),A` after
             * each award. So the surviving bricks alternate 1UP, 2UP,
             * 1UP... regardless of where they sit. ($01, not $80 — the
             * consumer only tests non-zero.) */
            add_points_to_score(pts, left_brik_side ? 0x80 : 0);
            left_brik_side = (unsigned char)(!left_brik_side);
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
    ball.stuck[BALL_PRIMARY]     = 1;
    ball.stuck_ticks[BALL_PRIMARY]    = 0;
    ball.stuck_offset_x[BALL_PRIMARY] = BALL_X_OFFSET_ON_BAT;
    /* Always bat 1: a respawn or a level entry puts the ball on the
     * player's own bat, whichever bat happened to be holding it
     * when the life was lost. Without this a ball caught by bat 2
     * would come back held by bat 2. */
    ball.stuck_bat[BALL_PRIMARY] = OBJ_BAT_1;
    BALL_SHOW();
    BALL_X = BAT_X + BALL_X_OFFSET_ON_BAT;
    /* Ball sits at BAT_Y_PX - BALL_H_PX = 166 (= $A6) so its bottom row
     * touches the bat's top row, matching LA27E_15's `LD (IX+$04),$A6`
     * on the FIRE-launch path. BALL_H_PX, not eff_ball_size: the width
     * would put it 1 px too high. */
    BALL_Y = BAT_Y - BALL_H_PX;
    primary_ball_set_velocity(+1, -BALL_SPEED);
    set_bat_bonus(OBJ_BAT_1, 0xFF);
    set_bat_bonus(OBJ_BAT_2, 0xFF);
    bat1.big_ticks  = 0;
    ball.big_ticks = 0;
    ball.speed_ramp = 0;     /* fresh life: ball restarts at base speed */
    bat1.extra_target  = 0;
    bullet_cooldown = 0;       /* fresh life — no stale fire cooldown */
    /* Original LBC10 clears flag_extra_life on every life-loss, so
     * another LIFE bonus can drop on the next life within the same
     * round. */
    life_dropped_this_round = 0;
}

/* Mirror LBC10's `SET 7,(IX+$00)` sweep over all 11 object slots. Without
 * it a bomb in flight or an alien on screen survives the explosion and
 * reappears when the player respawns. */
static void clear_objects_for_death(void) {
    int i;
    bomb.active = 0;
    bonus.active = 0;
    pts_marker.active = 0;
    rocket.active = 0;
    rocket.clear_completed = 0;
    set_rocket_bonus_sprite_height(ROCKET_BONUS_H_PX);
    objects[OBJ_ENEMY].sprite_set = 0;
    brick_flash.ticks = 0;
    reset_brick_hit_anim();
    for (i = 0; i < N_BULLETS; i++) {
        bullet_active[i] = 0;
        bullet_blast_ticks[i] = 0;
    }
    /* The extras are normally already inactive; cleared defensively in
     * case a new call site forgets to do it upstream. */
    hide_extra_balls();
}

/* Ten sparks abreast of the bat centre, fanning out 5/64 of a turn
 * apart. orig: LBC10 $BC10 */
static void spawn_death_sparks(void) {
    int bat_center = BAT_X + (int)objects[OBJ_BAT_1].w_body_px / 2;
    int x_start = bat_center - 12;
    unsigned char dir = 0x1B;
    int i;
    for (i = 0; i < DEATH_SPARK_COUNT; i++) {
        death_sparks[i].active        = 1;
        death_sparks[i].x_q88         = (long)(x_start + i * 3) << 8;
        death_sparks[i].y_q88         = (long)0xAE << 8;
        death_sparks[i].dir           = dir;
        death_sparks[i].speed         = 2;        /* matches (IX+$07) at spawn */
        death_sparks[i].sprite_num    = 0;
        death_sparks[i].frame_ticks   = 0x18;     /* (IX+$15) at spawn */
        death_sparks[i].duration_base = 0x18;     /* (IX+$14) at spawn */
        dir = (unsigned char)((dir + 5) & 0x3F);
    }
    /* Double Play blows BOTH bats up. LBC10_4 walks the seeded slots
     * from object_ball_2 — the SECOND one — stepping two objects at a
     * time, five times, and adds a self-modified delta to each x:
     *
     *   LD A,(object_bat_1+$02) / LD C,A
     *   LD A,(object_bat_2+$02) / SUB C / LD (LBCE6+$01),A
     *   ...
     *   LD IX,object_ball_2 / LD DE,$0016 / LD B,$05
     *   LBC10_4: LD A,(IX+$02) / LBCE6: ADD A,$00 / LD (IX+$02),A
     *            ADD IX,DE / ADD IX,DE / DJNZ LBC10_4
     *
     * The delta is bat 2's x minus bat 1's, so the ODD-indexed half of
     * the fan is translated onto bat 2 and each bat ends up with five
     * sparks. It is not a second spawn: the same ten are split. */
    if (game_mode == 2) {
        const int delta = (int)objects[OBJ_BAT_2].x_coord - (int)BAT_X;
        for (i = 1; i < DEATH_SPARK_COUNT; i += 2)
            death_sparks[i].x_q88 += (long)delta << 8;
    }
}

/* Advance one death spark. False once it has expired — off the bottom,
 * or out of animation frames. The sides only CLAMP: a spark keeps
 * ticking against a wall until its frame counter runs out.
 * orig: handling_spark $A8BD */
static bool step_death_spark(int i) {
    int dx_q88, dy_q88;
    int xp, yp;
    int right_x;
    if (!death_sparks[i].active) return false;
    dir_to_dxdy(death_sparks[i].dir, death_sparks[i].speed,
                 &dx_q88, &dy_q88);
    death_sparks[i].x_q88 += dx_q88;
    death_sparks[i].y_q88 += dy_q88;
    xp = (int)(death_sparks[i].x_q88 >> 8);
    yp = (int)(death_sparks[i].y_q88 >> 8);
    /* Off the bottom = dead. Sides clamp the position so the
     * spark can keep ticking until its frame counter expires. */
    if (yp >= PLAYFIELD_H) { death_sparks[i].active = 0; return false; }
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
            return false;
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
    return true;
}

/* Block in a self-contained PIT-paced loop while the bat explodes. The
 * original's per-frame LBAED keeps running through LBC10's spark
 * lifetime; the port plays it as a separate phase and returns to
 * run_level for the lives-- + respawn step. */
static void play_bat_explosion(unsigned char level_idx) {
    unsigned long last;
    unsigned long death_pause_start;
    int alive;
    int i;
    clear_objects_for_death();
    spawn_death_sparks();
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
            if (step_death_spark(i)) alive = 1;
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
    /* The sparks flew over the whole playfield and the bat is gone, so
     * the screen no longer matches anything the per-frame dirty tests
     * track — and it has to be said explicitly, because NOTHING coming
     * back from respawn_primary_ball looks changed to them. The bat
     * returns to BAT_X_INIT with prev_x set to match, same y, same width,
     * same bonus byte, so bat_changed() is false and compose_bat_full
     * flushes one row of running dots over a bat that is not on screen.
     * Magnets are worse: they repaint only when they toggle.
     *
     * Do NOT rely on the lives_dirty rebuild for this. It covers the case
     * only because a death usually changes the life counter —
     * BATTY_INFINITE_LIVES does not, and the bat comes back in fragments.
     * known-bugs.md #21. */
    invalidate_static_cache_after_death();
}


/* --- Level entry ------------------------------------------------------
 * Everything between arriving at a level and the first frame of play. */

/* Reset the per-level state and load the level's bricks. orig: the LDIR
 * in all_var_init, which restores a template over the whole block. */
static void reset_level_state(unsigned char lvl_idx) {
    objects[OBJ_ENEMY].sprite_set = 0;     /* alien cleared on level entry */
    /* Port of all_var_init's LDIR — BAT_X resets to the default $74 at
     * every level entry: the original re-centres rather than leaving the
     * bat where the player left it. */
    BAT_X         = BAT_X_INIT;
    BAT_Y         = BAT_Y_PX;
    objects[OBJ_BAT_2].y_coord = BAT_Y_PX;
    BAT_PREV_X    = BAT_X_INIT;
    /* Double Play moves BOTH bats and activates the second — LB7F8's
     * `CP $02 / LD A,$01 / LD (object_bat_2),A / LD A,$38 /
     * LD (object_bat_1+$02),A / LD A,$B0 / LD (object_bat_2+$02),A`.
     * Outside mode 2 bat 2 stays inactive, which is object_bat_2's own
     * sprite_set $00 in the tape data. */
    if (game_mode == 2) {
        BAT_X      = 0x38;
        BAT_PREV_X = 0x38;
        objects[OBJ_BAT_2].x_coord = 0xB0;
        object_activate(objects[OBJ_BAT_2]);
        objects[OBJ_BAT_2].sprite_set = 0x01;
    } else {
        object_deactivate(objects[OBJ_BAT_2]);
    }
    ball.stuck[BALL_PRIMARY]    = 1;
    ball.stuck_offset_x[BALL_PRIMARY] = BALL_X_OFFSET_ON_BAT;
    ball.stuck_bat[BALL_PRIMARY] = OBJ_BAT_1;   /* see respawn_primary_ball */
    BALL_SHOW();                      /* visible from level entry; sits on the bat */
    /* all_var_init's first act is the alternation, and it happens in
     * every mode — only its effect on the ball's START X is mode-2
     * specific, since outside Double Play the ball rests on the bat. */
    ball_start_right = (unsigned char)(!ball_start_right);
    {   /* all_var_init writes the start side into the ball it is
         * initialising. There are no extras alive at level entry, so
         * seeding all three costs nothing and means a slot can never be
         * read before it is written. */
        const unsigned char start =
            (unsigned char)((game_mode == 2 && ball_start_right) ? 1 : 0);
        ball_owner_side[0] = start;
        ball_owner_side[1] = start;
        ball_owner_side[2] = start;
    }
    BALL_X        = BAT_X + BALL_X_OFFSET_ON_BAT;
    BALL_Y        = BAT_Y - BALL_H_PX;
    ball.stuck_ticks[BALL_PRIMARY]   = 0;                /* counts up while waiting for launch */
    primary_ball_set_velocity(+1, -BALL_SPEED);
    hide_extra_balls();

    bonus.active = 0;
    bomb.active  = 0;
    pts_marker.active = 0;
    bullets_clear();
    bullet_cooldown   = 0;
    probe.shots_fired = 0;
    rocket.active = 0;
    rocket.clear_completed = 0;
    set_rocket_bonus_sprite_height(ROCKET_BONUS_H_PX);
    brick_flash.ticks = 0;
    reset_brick_hit_anim();

    ball.speed_ramp = 0;
    bat1.big_ticks   = 0;
    ball.big_ticks  = 0;
    /* flag_extra_life is deliberately NOT cleared here: only LBC10 (the
     * death path) clears it in the original, so a LIFE catch blocks
     * future LIFE drops for the rest of the player's life, across
     * levels. */
    run_dot_frame = 0x0E;               /* matches running_dot_frame_1up reset */
    bat1.extra_px   = 0;
    bat1.extra_target  = 0;
    set_bat_bonus(OBJ_BAT_1, 0xFF);
    set_bat_bonus(OBJ_BAT_2, 0xFF);
    /* Mirror all_var_init's `clear_hl_buff` of sounds_queue at line
     * 5984 — sounds in-flight at level entry shouldn't bleed into
     * the new round. */
    sound_stop_all();
    memcpy(live_level, &levels[(int)lvl_idx * LVL_CELLS], LVL_CELLS);
}

/* Seeded overrides from the replay harness. Applied after the level's
 * bricks are loaded and before anything reads them, so a seeded run and
 * the original start from the same state. */
/* BATTY_REPLAY_CLEAR_BRICKS: mark every destructible cell destroyed, so
 * live_bricks_remaining() is 0 on the first frame and run_level takes the
 * level-clear branch immediately.
 *
 * This is what makes the level-clear -> next transition reachable from a
 * gate. Clearing a level for real means destroying ~50 bricks with the
 * ball, which is neither quick nor deterministic; the rocket-clear gates
 * cover the ROCKET route to the same place, not the ordinary one.
 *
 * Bit 7 is the destroyed flag. live_bricks_remaining() counts a cell as
 * live when `!(cell & 0xA0)`, so cells that already have bit 5 or bit 7
 * set are not live and are left exactly as they are — an indestructible
 * cell must stay indestructible, not become rubble. */
static void apply_replay_clear_bricks(void) {
    const char *p = getenv("BATTY_REPLAY_CLEAR_BRICKS");
    int i;
    if (p == NULL || *p == '\0' || *p == '0') return;
    for (i = 0; i < LVL_CELLS; i++) {
        if (!(live_level[i] & 0xA0)) live_level[i] |= 0x80;
    }
}

static void apply_replay_overrides(void) {
    replay_apply_random();
    replay_apply_object("BATTY_REPLAY_BAT_OBJECT",  OBJ_BAT_1);
    replay_apply_object("BATTY_REPLAY_BALL_OBJECT", OBJ_BALL_1);
    apply_replay_ball_motion_override();
    /* BATTY_REPLAY_MULTIBALL=1 spawns the two extras at level entry, as
     * if a TRIPLE had just been caught, with their directions derived
     * from the primary's SEEDED dir — so it must run after the ball
     * override above.
     *
     * It exists because MULTI_BALL and MAGNET cannot both be reached by
     * catching: the bat holds ONE bonus code ($02 vs $03) and a second
     * catch overwrites the first. In play the pair arises the other way
     * round — the extras outlive the code that spawned them, so a bat
     * that picks up MAGNET afterwards can catch them. Seeding the
     * spawn and catching the MAGNET reproduces that order. */
    if (getenv("BATTY_REPLAY_MULTIBALL") != NULL)
        apply_multi_ball_bonus(OBJ_BAT_1);
    replay_apply_object("BATTY_REPLAY_ENEMY_OBJECT", OBJ_ENEMY);
    apply_replay_bonus_override();
    replay_apply_bomb();
    apply_replay_pts400_override();
    apply_replay_force_brick();
    apply_replay_ball_ramp();
    replay_apply_bullet();
    replay_apply_blast();
    apply_replay_force_bonus();
    apply_replay_multiball();
    apply_replay_bigball();
    apply_replay_rocket_override();
    /* Last: it reads the grid every other override may have written. */
    apply_replay_clear_bricks();
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

/* Pin the global frame counter (= the original's counter_misc) at the
 * aligned start. It has been ticking since boot, so its low-bit PHASE
 * here is wall-clock roulette — and the enemy steer (&3), the ball speed
 * ramp (&7) and the other counter_misc cadences all key off it. Un-pinned,
 * a 4-frame steer turn slides across a probe frame run to run (the
 * test-enemy-steer flake). Pinned AFTER the WAIT_KEY release, so frame 1
 * sees counter == pin+1. */
static void pin_replay_frame_counter(void) {
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

/* P (or 1-4, as the original) toggles the pause overlay. Returns the
 * action the caller should take for this frame. */
static InputAction toggle_pause(int &ball_moved, int &bat_moved) {
    paused = !paused;
    sound_silence();
    if (paused) {
        static const unsigned char paused_codes[] = {
            0x19, 0x0A, 0x1D, 0x1C, 0x0E, 0x0D  /* P A U S E D */
        };
        draw_text(BORDER_X + 13 * 8, BORDER_Y + 90, 15, paused_codes, 6);
    } else {
        /* Resuming: schedule a full redraw to erase the banner. */
        bat_moved = 1;
        ball_moved = 1;
        cache.full_flush = 1;
    }
    return INPUT_SKIP_FRAME;
}

/* SPACE does two independent things, and the independence is the point:
 * it launches a ball only if one is WAITING, and it fires the laser
 * whenever the bonus and a slot allow. Hammering SPACE with the ball in
 * flight — which is what a player does to refire the laser — must not
 * teleport the ball back to its launch trajectory. */
static void launch_or_fire(void) {
    if (ball.stuck[BALL_PRIMARY]) {
        BALL_SHOW();
        ball.stuck[BALL_PRIMARY]       = 0;
        ball.stuck_ticks[BALL_PRIMARY] = 0;
        sound_queue(SND_BALL_START);   /* descending launch blip */
        ball_launch_from_bat(BALL_PRIMARY);
        record_primary_launch();
    }
    /* Every held ball goes, not only the primary. The original has no
     * "launch" routine to be selective with: the release lives inside
     * handling_ball, which runs once per ball object, so one press frees
     * whatever is resting. Leaving the extras behind would also strand
     * them for the full STUCK_TIMEOUT with the player pressing FIRE. */
    {
        int i;
        for (i = 1; i < 3; i++) {
            if (!ball.stuck[i]) continue;
            ball.stuck[i]       = 0;
            ball.stuck_ticks[i] = 0;
            ball_launch_from_bat(i);
        }
    }
    try_fire_laser();                  /* free_bullet_2 $A14C; see there */
}

static InputAction handle_input(int &ball_moved, int &bat_moved) {
    if (!kbhit()) return INPUT_NONE;
    {
        const int k = getch();
        if (k == KEY_ESC) return INPUT_QUIT;
        if (k == KEY_P_LOWER || k == KEY_P_UPPER
            || k == '1' || k == '2' || k == '3' || k == '4') {
            return toggle_pause(ball_moved, bat_moved);
        }
        if (paused) {
            if (k == KEY_ENTER) return INPUT_ADVANCE_LEVEL;
            return INPUT_SKIP_FRAME;                    /* swallow other input */
        }
        if (k == KEY_EXT_PREFIX) {
            /* Discard the scancode following 0 — arrows are handled by the
             * per-frame key_state[] polling, this just drains the buffer. */
            getch();
        } else if (k == KEY_SPACE) {
            launch_or_fire();
        }
        /* Mirror the original: no level-skip key. ENTER while playing does
         * nothing (only the pause overlay above consumes ENTER). The level
         * holds until the player clears it or loses all lives. */
    }
    return INPUT_NONE;
}

static void ride_stuck_ball_on_bat(int b, int bat) {
    rest_ball_on_bat(b, bat);
    ball.stuck_ticks[b]++;
    if (ball.stuck_ticks[b] >= STUCK_TIMEOUT) {
        ball.stuck[b] = 0;          /* auto-launch */
        sound_queue(SND_BALL_START);
        ball_launch_from_bat(b);
        /* Probe bookkeeping for the launch gates, and it reads the
         * PRIMARY's coordinates — so it is guarded rather than indexed.
         * A secondary's auto-launch recording itself as the primary's
         * would corrupt exactly the gate that watches for it. */
        if (b == BALL_PRIMARY) record_primary_launch();
    }
}

/* Mirror LBAED's ordering: object_rocket is checked
 * before balls_quantity, and an active rocket jumps to the rocket
 * loop instead of LBC10's bat-explosion path. The rocket catch hides
 * all balls while the level-clear sequence runs, so that temporary
 * no-ball state must not cost a life. */
static void handle_no_ball_death(void) {
    if (!rocket.active
        && !rocket.clear_completed
        && !dbg.suppress_no_ball_death
        && !BALL_VISIBLE
        && !ball.extra2_active
        && !ball.extra3_active) {
        lose_a_life();
    }
}

/* Each threshold in live_add_thresholds crossed since the last check
 * awards one extra life. orig: $0395 score_update_3 */
static void award_score_milestones(void) {
    for (int earned = lives_earned(player.score, player.live_adds_awarded);
         earned > 0; earned--) {
        player.lives++;
        sound_queue(SND_LIVE_ADD);
        player.live_adds_awarded++;
    }
}

/* The displayed HI rolls forward the moment it is passed; writing it to
 * disk still waits for game-over in save_high_score. */
static void roll_high_score(void) {
    if (player.score > high_score) {
        high_score = player.score;
        high_score_beaten_this_game = 1;
    }
}

/* Everything drawn over the playfield besides the primary ball. While
 * any of it is live the frame cannot take the ball-only redraw path. */
static bool entities_need_redraw(void) {
    return bonus.active
        || pts_marker.active
        || objects[OBJ_ENEMY].sprite_set != 0
        || bomb.active
        || any_bullet_active()
        || any_bullet_blast()
        || rocket.active
        || brick_flash.ticks
        || any_brick_hit_anim()
        || ball.extra2_active
        || ball.extra3_active;
}

static void kill_enemy_by_ball_slot(unsigned char slot) {
    /* `handling_ball` opens with `LD A,(IX+$12) / AND $80 /
     * LD (need_change_player),A` — the ball's OWNER bit, not its x. A
     * ball that crossed into the other half still scores for whoever
     * last deflected it, which is the whole point of the owner bit. */
    kill_enemy_in_rect((int)objects[slot].x_coord,
                            (int)objects[slot].y_coord,
                            BALL_W_PX, BALL_H_PX,
                            ball_owner_side[slot] ? 0x80 : 0);
}

/* Any ball landing on an alien destroys it, not just the bat: the
 * original calls kill_enemy_by_bat from handling_ball as well.
 * orig: $A4B8 kill_enemy_by_bat */
static void kill_enemies_by_balls(void) {
    if (BALL_VISIBLE)       kill_enemy_by_ball_slot(OBJ_BALL_1);
    if (ball.extra2_active) kill_enemy_by_ball_slot(OBJ_BALL_2);
    if (ball.extra3_active) kill_enemy_by_ball_slot(OBJ_BALL_3);
}

/* One frame of every entity that moves independently of the primary
 * ball, plus the per-frame timer decays. */
static void step_active_entities(void) {
    step_bonus();
    step_pts_400();
    step_bomb();
    step_bullet();
    bullet_blasts_tick();
    step_rocket();
    step_brick_flash();
    step_brick_hit_anim();

    if (bat1.fire_anim_ticks) bat1.fire_anim_ticks--;
    {   /* Bat 2's flash counts down too, or its gun frames stick on the
         * frame it fired. Outside mode $02 the slot is never set, so the
         * decrement is unconditional rather than mode-gated. */
        BatState &st2 = bats[BAT_SLOT(OBJ_BAT_2)];
        if (st2.fire_anim_ticks) st2.fire_anim_ticks--;
    }
    if (bullet_cooldown >= 2) bullet_cooldown -= 2;     /* SUB \$02 / frame */
    else bullet_cooldown = 0;

    /* SLOW sets the ball_1/2/3 speed bytes to $02 together in the
     * original; here it lives in the speed byte, so the extras step
     * every frame and their magnitude carries the effect. */
    step_ball2();
    step_ball3();
}

/* A replay checkpoint counts down one frame at a time; reaching zero
 * means write PROBE.TXT and end the run. Short-circuiting the two
 * callers is deliberate: the first to fire returns, so the second never
 * ticks — as when each checkpoint had its own inline block. */
static bool probe_checkpoint_due(unsigned char active, unsigned int *countdown) {
    if (!active) return false;
    if (*countdown > 0) (*countdown)--;
    return *countdown == 0;
}

/* Replay knobs that SEED player state, applied after new_game_reset has
 * finished zeroing things.
 *
 * Ordering is the whole point of it being a separate function: inline in
 * new_game_reset, above its `player.live_adds_awarded = 0;`, the seeding
 * is silently wiped — and the symptom is a gate sitting in the level for
 * 13 s instead of dying. */
static void apply_player_seed_env(void) {
    /* BATTY_REPLAY_LIVES: start with fewer lives. The game-over sequence
     * is otherwise unreachable from a gate — three deaths means three
     * death animations on wall-clock waits, which is exactly the shape
     * that made test-bat-redraw-window flaky (notes/testing.md). With
     * lives=1 and BATTY_HIDE_BALL, handle_no_ball_death fires on the
     * first frame and the whole sequence is deterministic. */
    {
        const char *lv = getenv("BATTY_REPLAY_LIVES");
        long want;
        if (lv != NULL && replay_parse_ints(lv, &want, 1)
            && want >= 1 && want <= LIVES_INIT)
            player.lives = (int)want;
    }
    /* BATTY_REPLAY_LIVES_2UP: the OTHER player's life count, 0 allowed.
     * Zero is the point of it — two_player_turn_change's `lives <= 0`
     * guard is what makes a solo player keep playing, and with player 2
     * always on three lives a mutation of that guard to `< 0` survives
     * every case. */
    {
        const char *lv = getenv("BATTY_REPLAY_LIVES_2UP");
        long want;
        if (lv != NULL && replay_parse_ints(lv, &want, 1)
            && want >= 0 && want <= LIVES_INIT)
            players[1].lives = (int)want;
    }
    /* BATTY_REPLAY_SCORE: start with a score already on the clock. The
     * name-entry screen only runs when the score BEATS the stored high
     * score, so with score 0 a gate reaches game over and stops there.
     * This is what makes the last unreached screen reachable. */
    {
        const char *sc = getenv("BATTY_REPLAY_SCORE");
        long want;
        if (sc != NULL && replay_parse_ints(sc, &want, 1) && want >= 0) {
            player.score = (unsigned long)want;
            /* Seed the extra lives that score would ALREADY have earned.
             * Without this, award_score_milestones hands them out on the
             * first frame and BATTY_REPLAY_LIVES=1 silently becomes
             * lives=N — a run set up to die immediately never dies. */
            player.live_adds_awarded = (unsigned char)
                lives_earned(player.score, 0);
        }
    }
}

static void new_game_reset(void) {
    /* game_restart zeroes BOTH players' scores and sets both life
     * counts, then starts with 1UP — it does not reset only whoever
     * happened to be active. Doing both here means a 2-player game
     * cannot inherit the previous game's 2UP score, which is the sort
     * of thing that only shows up two features later. */
    int i;
    for (i = 0; i < 2; i++) {
        players[i].score = 0;
        players[i].lives = LIVES_INIT;
        players[i].live_adds_awarded = 0;
    }
    active_player = 0;
    bonus.active = 0;
    ball.speed_ramp = 0;
    bat1.big_ticks = 0;
    ball.big_ticks = 0;
    bat1.extra_px = 0;
    bat1.extra_target = 0;
    paused = 0;
    high_score_beaten_this_game = 0;

    /* The bat's own new-game reset. The original also resets it at every
     * life and level entry, via all_var_init's LDIR from objects_buff_2;
     * that is handled at level entry and in respawn_primary_ball. */
    BAT_X      = BAT_X_INIT;
    BAT_Y      = BAT_Y_PX;
    objects[OBJ_BAT_2].y_coord = BAT_Y_PX;
    BAT_PREV_X = BAT_X_INIT;
    apply_player_seed_env();
}

static void probe_init_from_env(void) {
    const char *p = getenv("BATTY_LAUNCH_FRAMES");
    probe.launch_frames = (p && *p) ? (unsigned int)atoi(p) : 0;
    probe.launch_countdown = 0;
    probe.launch_active = 0;
    p = getenv("BATTY_FRAME_PROBE");
    probe.frame_frames = (p && *p) ? (unsigned int)atoi(p) : 0;
    probe.frame_countdown = probe.frame_frames;
    probe.frame_active = (probe.frame_frames != 0) ? 1 : 0;
    probe.visual_index = 0;
    probe.visual_count = (unsigned char)replay_parse_frame_list(
        getenv("BATTY_VISUAL_PROBE_FRAMES"),
        probe.visual_list, VISUAL_PROBE_MAX);
    probe.visual_active = (probe.visual_count != 0) ? 1 : 0;
    probe.visual_countdown = probe.visual_active ? probe.visual_list[0] : 0;
}

/* BATTY_LEVEL=N (1..15) starts at level N, so the visual suite can
 * capture any cycle's level entry without playing through. */
static unsigned char initial_round_number(void) {
    const char *p = getenv("BATTY_LEVEL");
    if (p && *p) {
        int n = atoi(p);
        if (n >= 1 && n <= N_LEVELS) return (unsigned char)(n - 1);
    }
    return 0;
}

/* The level is clear: hide every ball, show the emptied brick zone for
 * a moment, then let the caller advance. False means the player pressed
 * ESC during the hold.
 *
 * The pause is LBBFB_0's `pause_long B=2`, about 0.6 s. There is NO
 * level-clear sound — the original only drains the queue, so the last
 * brick's click is what the player hears. */
static bool finish_cleared_level(unsigned char lvl_idx) {
    unsigned long t;

    rocket.clear_completed = 0;
    BALL_HIDE();
    hide_extra_balls();
    cache.full_flush = 1;
    redraw_full_with_ball(lvl_idx);

    if (getenv("BATTY_HOLD_ROCKET_CLEAR") != NULL) {
        while (!kbhit()) sound_tick();
        if (getch() == KEY_ESC) return false;
    }

    t = pit_ticks();
    while (pit_ticks() - t < 30UL) {
        sound_tick();
        if (kbhit()) {
            if (getch() == KEY_ESC) return false;
            break;
        }
    }
    sound_silence();
    return true;
}

/* The end of a game: the GAME OVER hold, then the name-entry screen if
 * the player beat the high score.
 *
 * LBC10_6 plays no sound — its preceding pause_clear_screen_attrib just
 * drains the queue while the screen clears — and it holds for
 * `pause_long B=$0C` = 12 * 0.3 s, which is ~65 BIOS ticks at 18.2 Hz.
 * The hold is not gated on auto_advance: the original waits regardless,
 * and TIMED_OUT is permanently false, which would strand the player on
 * the screen. Any key cuts it short.
 *
 * The high score is saved only after name entry, so the file gets the
 * new score and the player's initials together. */
static void play_game_over(void) {
    unsigned long start;

    if (player.score > high_score) {
        high_score = player.score;
        high_score_beaten_this_game = 1;
    }
    sound_stop_all();
    render_game_over();

    /* BATTY_HOLD_GAME_OVER holds the screen for a key instead of 65
     * ticks, so a visual gate captures it without racing the timer —
     * the same hook, for the same reason, as BATTY_HOLD_ROUND_BANNER. */
    if (getenv("BATTY_HOLD_GAME_OVER") != NULL) {
        while (!kbhit()) sound_tick();
        getch();
    } else {
        /* 65 BIOS ticks at 18.2 Hz is 3.57 s = 178 PIT frames at ~50 Hz.
         * Counted in PIT frames because bios_ticks() does not advance
         * during gameplay (known-bugs.md #15), which would leave this
         * loop infinite except for the keypress.
         *
         * BATTY_FAST_HOLDS cuts the wait to 2 frames, so a capture window
         * does not end mid-hold. NOT merged with BATTY_HOLD_GAME_OVER,
         * which waits for a KEY instead — that serves visual gates that
         * want the screen to stay up, the opposite need. */
        const unsigned long hold = dbg.fast_holds ? 2UL : 178UL;
        start = pit_ticks();
        while (pit_ticks() - start < hold) {
            sound_tick();
            if (kbhit()) { getch(); break; }
        }
    }

    if (high_score_beaten_this_game) {
        input_new_record_name();
        high_score_save(high_score, high_score_name);
    }
    sound_silence();
}

/* Count down to the next BATTY_VISUAL_PROBE_FRAMES checkpoint and, on
 * reaching one, write the probe and halt so the harness can take a
 * deterministic capture. A key resumes play toward the next checkpoint.
 *
 * False means this was the last checkpoint and the run should quit, so
 * QEMU exits cleanly. A single-value probe has count == 1 and stops
 * here, exactly as the older single-shot path did.
 *
 * The countdown between checkpoints is a delta, which is why
 * probe_init_from_env drops any value not strictly greater than the one
 * before it. */
static bool visual_checkpoint_tick(void) {
    if (!probe.visual_active) return true;
    if (probe.visual_countdown > 0) probe.visual_countdown--;
    if (probe.visual_countdown != 0) return true;

    write_replay_probe();
    serial_probe_signal();               /* reached frame N -> tell harness */
    while (!kbhit()) sound_tick();
    (void)getch();

    probe.visual_index++;
    if (probe.visual_index >= probe.visual_count) return false;
    probe.visual_countdown = probe.visual_list[probe.visual_index]
                           - probe.visual_list[probe.visual_index - 1];
    return true;
}

/* One frame of the primary ball: riding the bat while stuck, otherwise
 * ramping its speed and stepping. A hidden ball does neither.
 *
 * SLOW is a speed reset ($02), not a frame skip — handling_ball runs
 * every frame in the original and the speed byte, via dir_to_dxdy's
 * magnitude, is what changes. So a visible ball always steps.
 *
 * False means a replay checkpoint fired: the probe is written and the
 * run should end. */
static bool step_primary_ball(int *ball_moved) {
    if (ball.stuck[BALL_PRIMARY]) {
        ride_stuck_ball_on_bat(BALL_PRIMARY, ball.stuck_bat[BALL_PRIMARY]);
        *ball_moved = 1;
        return true;
    }
    if (!BALL_VISIBLE) return true;

    ball_speed_ramp_tick();
    step_ball();
    *ball_moved = 1;

    if (probe_checkpoint_due(probe.launch_active, &probe.launch_countdown)
        || probe_checkpoint_due(probe.frame_active, &probe.frame_countdown)) {
        write_replay_probe();
        serial_probe_signal();
        return false;
    }
    return true;
}

/* The main loop's RNG work, in the original's order at LB9E8_2.
 *
 * The magnet toggle samples the CURRENT value — it is reading LAST
 * frame's, since the per-frame tick below has not run yet. Swap the two
 * and a different set of frames toggles a magnet.
 *
 * The toggle is pinned off in test mode (BATTYALL) so level-entry
 * captures stay deterministic, the same trick as the menu blink and the
 * running dot. The tick itself is gated so the older on-demand RNG
 * model stays byte-unchanged. */
static void tick_frame_rng(void) {
    if (!test_mode_pin_blink && rng_high(rng_current()) == 0x99)
        magnet_random_toggle();
    if (dbg.rng_perframe) next_random();
}

/* Arrows are polled from key_state[] rather than read from the BIOS
 * buffer, so holding one steers continuously at 4 px per 50 Hz tick =
 * 200 px/s, as the original's get_left_player_ctrl_state does. A rocket
 * in flight carries the bat, so the player cannot steer. */
static void steer_bat_from_keys(void) {
    int left  = key_state[SC_LEFT];
    int right = key_state[SC_RIGHT];

    /* In Double Play player 1 also has the ASDFG cluster, so both
     * players can reach a key without fighting over the arrows. */
    if (game_mode == 2) {
        left  = left  || key_state[SC_A] || key_state[SC_D];
        right = right || key_state[SC_S] || key_state[SC_F];
    }

    BAT_X = (unsigned char)bat_step_x(
        BAT_X, bat1.extra_px,
        !rocket.active && left, !rocket.active && right);

    if (game_mode != 2) return;

    /* Mirror of LB9E8_2's mode-$02 tail: bat 2 is handled with the same
     * movement code, then each bat is confined to its own court.
     *
     * Bat 2 has no rocket of its own — `rocket` is bat 1's, and a
     * WS3 residual — so nothing suspends its steering. */
    Object &b2 = objects[OBJ_BAT_2];
    const int b2_left  = key_state[SC_J] || key_state[SC_L];
    /* K only. The original's right-hand cluster is K *or* Enter
     * ($BFFE bit 0), and Enter is deliberately dropped: this port uses
     * ENTER as its attract-chain affordance (PLAN.md WS1) and the
     * capture harness presses it to start every run, so a bat-2 binding
     * makes ENTER nudge the bat 4 px at a moment nothing controls.
     *
     * It is not merely a test artefact — a player pressing ENTER to get
     * through a screen would move bat 2 in the next level too. It first
     * showed as test-double-play-court flaking between a 207 and a 211
     * px extent, one bat step apart. */
    const int b2_right = key_state[SC_K];
    b2.x_coord = (unsigned char)bat_step_x((int)b2.x_coord, 0,
                                           b2_left != 0, b2_right != 0);

    /* Player 2's FIRE, polled from key_state rather than read out of the
     * BIOS buffer the way bat 1's SPACE is. That is closer to the
     * original, which polls the row every frame — and it is the only
     * option here, since one BIOS key queue cannot serve two players.
     *
     * `try_fire_laser_from` gates on bat 2's OWN bonus byte, so this
     * does nothing until player 2 catches a LASER. The bullet pool and
     * the cooldown are shared, which is faithful: the original's
     * `bullet` counter is one global and `free_bullet_2` takes whichever
     * bat is in IX. */
    if (key_state[SC_Y] || key_state[SC_U] || key_state[SC_I]
        || key_state[SC_O] || key_state[SC_P]
        || key_state[SC_B] || key_state[SC_N] || key_state[SC_M])
        try_fire_laser_from(OBJ_BAT_2);

    /* The clamps take the original's (left edge, body width). A grown
     * bat 1 straddles BAT_X by extra_px on each side, so it is
     * eff_bat_left() that goes in and the offset that comes back off. */
    const int w1 = BAT_BODY_W + 2 * bat1.extra_px;
    BAT_X = (unsigned char)(bat_court_clamp_1(eff_bat_left(), w1)
                            + bat1.extra_px);

    b2.x_coord = (unsigned char)bat_court_clamp_2((int)b2.x_coord);
}

/* Pick this frame's redraw path and run it, cheapest first: the ball
 * alone, the ball plus the simple-object tier, or the full compose. A
 * frame where only the bat moved gets the narrower bat path.
 *
 * A stuck ball rides the bat, so the bat-only branch has to move it
 * too — through rest_ball_on_bat, so the position matches the recorded
 * catch offset that the launch direction is also derived from. */
static void redraw_frame(unsigned char lvl_idx, unsigned char cycle,
                         unsigned char bg_attr, int ball_moved, int bat_moved) {
    if (ball_moved) {
        const unsigned int blockers = ball_dirty_blockers(bat_moved);
        if (blockers == 0) {
            redraw_ball_only(lvl_idx);
        } else if (can_redraw_ball_with_simple_objects(blockers)) {
            redraw_ball_with_simple_objects(lvl_idx);
        } else {
            prof_note_ball_dirty_blockers(blockers);
            redraw_full_with_ball(lvl_idx);
        }
        return;
    }
    if (!bat_moved) return;
    redraw_bat(cycle, bg_attr);
    if (BALL_VISIBLE && ball.stuck[BALL_PRIMARY]) {
        rest_ball_on_bat(BALL_PRIMARY, ball.stuck_bat[BALL_PRIMARY]);
        render_ball(BALL_X, BALL_Y, bg_attr);
    }
}

/* The Kinnock easter egg. `kinnock` is a single byte at $B973 holding
 * $01, and the disassembly's own comment says what it is for: "если сюда
 * записать ноль, то перед игрой будет надпись про Киннока" — POKE
 * 47475,0 and you get the message. Then:
 *
 *   print_kinnock:
 *     LD A,(kinnock) / AND A / RET NZ
 *     LD DE,txt_kinnock / LD B,$02 / CALL print_message
 *     LD D,$00 / CALL pause_short
 *     JP clear_screen_attrib
 *
 * Two lines, a pause, and the attributes cleared. It is called from
 * LB9E8_1 — after the level has been drawn INTO THE BUFFER and the
 * attributes cleared, but before `buff_to_screen_pixs` flushes it — so
 * it appears over a blank screen at the start of EVERY level, not once
 * per game.
 *
 * The pause is much shorter than it sounds. `pause_short` with D=0 is
 * 256 outer iterations of a 255-step inner loop, about 1.05M T-states,
 * which at 3.5 MHz is ~0.30 s. The disassembly agrees with that
 * arithmetic elsewhere: `LD B,$04 / CALL pause_long` is annotated
 * "Пауза 1,2 сек. (4*0.3)". So it is a third of a second — a blink, not
 * a screen you read.
 *
 * Coordinates come straight from txt_kinnock's headers, (x, y, attr,
 * len): ($38,$37,$47,$13) and ($50,$47,$47,$0D). The y bytes are
 * BOTTOM-anchored — screen_addr_calc takes them as the glyph's lowest
 * row and print_line draws upward — and the ink is 6px tall, so the top
 * row is y-5, the same conversion the round banner uses. Attr $47 is
 * bright white on black.
 *
 * Off by default; BATTY_KINNOCK turns it on. */
static void print_kinnock(void) {
    /* KINNOCK COULDNT RUN */
    static const unsigned char line1[] = {
        0x14, 0x12, 0x17, 0x17, 0x18, 0x0C, 0x14, 0x26,
        0x0C, 0x18, 0x1E, 0x15, 0x0D, 0x17, 0x1D, 0x26,
        0x1B, 0x1E, 0x17 };
    /* A YOUTH CLUB. */
    static const unsigned char line2[] = {
        0x0A, 0x26, 0x22, 0x18, 0x1E, 0x1D, 0x11, 0x26,
        0x0C, 0x15, 0x1E, 0x0B, 0x24 };
    unsigned long until;

    if (!dbg.kinnock) return;

    fill(0, 0, SCREEN_W, SCREEN_H, 0);
    draw_text(BORDER_X + 0x38, BORDER_Y + 0x37 - 5, 15,
              line1, (int)sizeof(line1));
    draw_text(BORDER_X + 0x50, BORDER_Y + 0x47 - 5, 15,
              line2, (int)sizeof(line2));

    /* ~0.30 s: 15 PIT frames at the port's 50 Hz. pit_ticks(), not
     * bios_ticks() — the BIOS counter does not advance here
     * (known-bugs #15). */
    until = pit_ticks() + 15UL;
    while (pit_ticks() < until) {
        if (kbhit()) { getch(); break; }
    }
    fill(0, 0, SCREEN_W, SCREEN_H, 0);        /* clear_screen_attrib */
}

/* Set up a level and show its intro. False means the player quit during
 * the intro. */
static bool enter_level(unsigned char lvl_idx) {
    current_level_idx_var = lvl_idx;
    reset_level_state(lvl_idx);
    /* reset_level_state has just loaded a pristine grid. A player
     * resuming their turn gets theirs back instead — destroyed bricks
     * stay destroyed, which is what the original's 180-byte exchange
     * achieves. Before apply_replay_overrides, so a seeded gate still
     * wins. */
    if (resume_player_grid) {
        memcpy(live_level, player_grid[active_player], LVL_CELLS);
        resume_player_grid = 0;
    }
    apply_replay_overrides();
    /* After the RNG seed override, so the magnets' ON/OFF coins consume
     * the seeded walk exactly as print_magnets does; before
     * render_level_screen, which paints from this state. */
    magnet_level_init(lvl_idx);

    probe.from_gameplay = 0;         /* the pre-gameplay seed write */
    write_replay_probe();
    /* LB9E8_1 order: the level is in the buffer and the attributes are
     * cleared, print_kinnock runs over the blank screen, and only then
     * does buff_to_screen_pixs flush the level. */
    print_kinnock();
    render_level_screen(lvl_idx);
    if (!show_level_intro((unsigned int)round_number)) return false;
    pin_replay_frame_counter();
    return true;
}

static state_t run_level(void) {
    unsigned long last_tick;
    unsigned char cycle;
    unsigned char bg_attr;

    new_game_reset();
    probe_init_from_env();

    /* The original loops levels forever (increment_round_number at
     * $BBE0 wraps current_level_number_1up at 15 → 0 while
     * round_number_1up keeps bumping). Game only ends on lives == 0. */
    round_number = initial_round_number();
    for (;;) {
        unsigned char lvl_idx = (unsigned char)(round_number % N_LEVELS);
        /* A turn change re-enters the level for the ARRIVING player, who
         * is on their own round — so the round must not advance. Only a
         * cleared level advances it. */
        int turn_changed = 0;

        if (!enter_level(lvl_idx)) return ST_QUIT;

        cycle     = (unsigned char)(lvl_idx & 3);
        bg_attr   = bg_attr_per_cycle[lvl_idx & 3];
        probe.from_gameplay = 1;         /* PROBE writes below are checkpoints */
        if (!probe.clocks_latched) {
            probe.bios_at_frame1 = bios_ticks();
            probe.pit_at_frame1  = pit_ticks();
            probe.clocks_latched = 1;
        }
        last_tick = pit_ticks();
        for (;;) {
            unsigned long now;
            int ball_moved = 0;
            int bat_moved  = 0;
            int frame_ticked = 0;

            {
                const InputAction action = handle_input(ball_moved, bat_moved);
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
                tick_frame_rng();
                steer_bat_from_keys();
                if (!step_primary_ball(&ball_moved)) return ST_QUIT;
                if (dbg.auto_fire) try_fire_laser();   /* held-SPACE sim (test) */
                step_active_entities();
                handle_no_ball_death();
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
                kill_enemies_by_balls();
                call_for_all_obj(refresh_buffer_offset);
                sound_frame();
                sound_tick();
                award_score_milestones();
                roll_high_score();
                if (entities_need_redraw()) ball_moved = 1;
                if (bat1.extra_px != bat1.extra_target) bat_moved = 1;
            }

            /* Either bat. `bat_changed` compares against what was last
             * DRAWN, so it covers bat 2's steering, its width and its
             * sprite — and it is the same test the full path uses, so
             * the two cannot disagree about whether the band is stale.
             *
             * With bat 1's x alone, a Double Play frame in which only
             * player 2 moved took the early-out in redraw_frame and drew
             * nothing at all. */
            if (bat_changed(OBJ_BAT_1)
                || (game_mode == 2 && bat_changed(OBJ_BAT_2))) {
                bat_moved = 1;
            }
            if (dbg.bat_full_redraw && bat_moved) {
                ball_moved = 1;
            }
            if (dbg.full_flush_each_frame && ball_moved) {
                cache.full_flush = 1;
            }

            redraw_frame(lvl_idx, cycle, bg_attr, ball_moved, bat_moved);

            if (frame_ticked && !visual_checkpoint_tick()) return ST_QUIT;

            if (dbg.profile_auto_frames != 0
                && prof.frames >= dbg.profile_auto_frames) {
                write_replay_probe();
                return ST_QUIT;
            }

            /* A life was lost and the other player is waiting. The
             * frame loop has to unwind to the level-entry point first:
             * the arriving player may be on a different level, and the
             * grid swap has to happen before it is painted. */
            if (pending_turn_change) {
                pending_turn_change = 0;
                if (two_player_turn_change(0)) {
                    turn_changed = 1;
                    break;
                }
                /* The other player turned out to have no lives after
                 * all; carry on as a solo game. */
                respawn_primary_ball();
            }

            /* End-of-life conditions. orig LBC10_6 -> LBC10_7: the
             * finishing player gets the GAME OVER screen and their name
             * entry, and THEN, in 2-player mode, the other player
             * carries on if they have lives left:
             *
             *   LBC10_7:
             *     DEC A / JP NZ,game_restart          ; game_mode != 1
             *     LD A,(lives_2up) / AND A / JP Z,game_restart
             *     CALL current_level_2up_copier / JP LB9E8_1
             *
             * Both conditions are two_player_turn_change's already; do
             * not re-test them here, or a mutation of the real guard
             * survives. */
            if (player.lives == 0) {
                game_overs_reached++;
                play_game_over();
                if (two_player_turn_change(1)) {
                    turn_changed = 1;
                    break;      /* the re-entry writes the probe */
                }
                /* Nothing else does. Returning to the title otherwise
                 * leaves PROBE.TXT holding the LEVEL-ENTRY write from
                 * before the death, so every counter reads 0 and a
                 * working build looks exactly like a broken one. */
                write_replay_probe();
                return ST_TITLE;
            }
            /* Mirror LBAED_0's exit conditions:
             *   balls_quantity == 0  -> game over (handled above)
             *   briks_quantity_1up == 0 -> level cleared, advance.
             * No timeout, no key-driven skip — the level holds the
             * player until the bricks are gone. */
            if (live_bricks_remaining() == 0) {
                if (!finish_cleared_level(lvl_idx)) return ST_QUIT;
                break;
            }
        }
        if (!turn_changed) round_number++;   /* increment_round_number $BBE0 */
    }
}

/* Read the BATTY_* environment switches and return the state to start
 * in — the profile and start-level knobs skip the title screen. */
static state_t apply_env_switches(void) {
    state_t state = ST_TITLE;
    const char *e;

    /* BATTYALL=1 (test floppy AUTOEXEC) pins the menu blink phase to 0
     * (the BLACK half) so the state2_menu screendump is deterministic
     * against snap2. A plain `make run` floppy leaves it off and the
     * player sees the natural ~4.5 Hz blink. */
    if (getenv("BATTYALL") != NULL) test_mode_pin_blink = 1;
    if (getenv("BATTY_SERIAL_PROBE") != NULL) probe.serial_enabled = 1;

    if (getenv("BATTY_FORCE_BAT_FULL_REDRAW") != NULL)      dbg.bat_full_redraw = 1;
    if (getenv("BATTY_FORCE_BALL_FULL_REDRAW") != NULL)     dbg.ball_full_redraw = 1;
    if (getenv("BATTY_FORCE_FULL_FLUSH_EACH_FRAME") != NULL) dbg.full_flush_each_frame = 1;
    if (getenv("BATTY_FULL_BAND_REBUILD") != NULL)          dbg.full_band_rebuild = 1;
    if (getenv("BATTY_SUPPRESS_NO_BALL_DEATH") != NULL)     dbg.suppress_no_ball_death = 1;
    if (getenv("BATTY_AUTO_FIRE") != NULL)                  dbg.auto_fire = 1;
    if (getenv("BATTY_LAFFC") != NULL)                      dbg.use_laffc = 1;
    if (getenv("BATTY_KINNOCK") != NULL)                    dbg.kinnock = 1;
    if (getenv("BATTY_FAST_HOLDS") != NULL)                 dbg.fast_holds = 1;
    if (getenv("BATTY_INFINITE_LIVES") != NULL)             dbg.infinite_lives = 1;
    {
        /* BATTY_HOLD_KEYS=1E,24 seeds key_state[] with scancodes that
         * are then never released. The capture harness runs headless
         * with nobody at the keyboard, so INT 9 never fires and the
         * seeded bits survive for the whole run — which is exactly what
         * a gate on "does holding LEFT move the bat" needs.
         *
         * BATTY_AUTO_FIRE is the older, narrower version of this idea
         * (hold SPACE) and stays: it drives try_fire_laser directly
         * rather than the key, so it also works where the fire path is
         * not read from key_state. */
        const char *hk = getenv("BATTY_HOLD_KEYS");
        if (hk != NULL) {
            unsigned int sc = 0;
            int digits = 0;
            for (;; hk++) {
                const char c = *hk;
                int v = -1;
                if (c >= '0' && c <= '9')      v = c - '0';
                else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
                else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
                if (v >= 0) { sc = sc * 16 + (unsigned int)v; digits++; }
                else {
                    if (digits && sc < 128) key_state[sc] = 1;
                    sc = 0; digits = 0;
                    if (c == '\0') break;
                }
            }
        }
    }
    {
        /* BATTY_GAME_MODE takes the ORIGINAL's 0-based value, not the
         * menu's 1..3 — a gate that wants 2-player writes 1. Gates reach
         * gameplay through BATTY_START_LEVEL and never touch the menu,
         * so without this the mode is unreachable from a test. */
        const char *gm = getenv("BATTY_GAME_MODE");
        if (gm != NULL && *gm >= '0' && *gm <= '2' && gm[1] == '\0')
            game_mode = (unsigned char)(*gm - '0');
    }
    if (getenv("BATTY_LEGACY_COLLISION") != NULL)           dbg.use_laffc = 0;

    /* Unlike the rest, this one can force EITHER state: it defaults on,
     * and only the exact string "0" reverts to advance-on-read. */
    e = getenv("BATTY_RNG_PERFRAME");
    if (e != NULL) dbg.rng_perframe = (e[0] == '0' && e[1] == '\0') ? 0 : 1;

    e = getenv("BATTY_PROFILE_AUTO_FRAMES");
    if (e != NULL && *e != '\0') {
        dbg.profile_auto_frames = strtoul(e, NULL, 10);
        if (dbg.profile_auto_frames != 0) state = ST_LEVEL;
    }
    if (getenv("BATTY_START_LEVEL") != NULL) state = ST_LEVEL;

    if (getenv("BATTY_NOSOUND") != NULL || getenv("BATTY_SOUND_OFF") != NULL
        || getenv("BATTY_RENDER_PROFILE") != NULL)
        sound_set_enabled(false);
    return state;
}

int main(void) {
    state_t state = apply_env_switches();
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
        asset_load("BGTILE.BIN",  bg_tile,     sizeof(bg_tile)) &&
        asset_load("BORDER.BIN", border_spr, sizeof(border_spr)) &&
        asset_load("SPRITES.BIN", sprites_blob, sizeof(sprites_blob)) &&
        asset_load("SEPARAT.BIN", separator_spr, sizeof(separator_spr)) &&
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
        /* The perimeter frame is BUILT, not loaded. It needs bg_tile,
         * so it runs after the asset block rather than inside it. */
        build_frame_from_sprites();
        build_level_attrs_from_data();
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
