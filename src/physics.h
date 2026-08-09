/* physics — the ball's direction model and bat deflection.
 *
 * Everything here is pure: no game state, no rendering, no side effects.
 * That is what lets tests/test_physics.cpp check it exhaustively against
 * the Spectrum's captured tables in microseconds.
 *
 * DIRECTIONS are 6-bit angles, as in the original: bits 4-5 select the
 * quadrant, bits 0-3 the angle within it. They are not interchangeable
 * with dx/dy pairs — dir_to_dxdy is the only conversion the game uses for
 * motion, and it is a literal port of the original's table lookup rather
 * than trigonometry, because the port must reproduce its rounding.
 *
 * COLLISION is split in two. The sweeps below decide WHICH brick the ball
 * reached and HOW it should bounce — pure, given the grid. What that hit
 * then costs (score, sound, grid mutation, bonus spawn, animation) stays
 * in the game code, because none of it is geometry. */

#ifndef BATTY_PHYSICS_H
#define BATTY_PHYSICS_H

#include "level.h"
#include "types.h"

/* --- Direction <-> velocity ------------------------------------------ */

/* orig: hl_bc_calc_direction. Returns 8.8 fixed-point components scaled
 * by `speed`. The original crosses the two terms relative to the table
 * read (it PUSHes HL, multiplies BC into X, then POPs HL for Y) — getting
 * that wrong puts the right magnitude on the wrong axis and only shows up
 * as a slow drift out of parity. */
void dir_to_dxdy(u8 dir, u8 speed, int *out_dx, int *out_dy);

/* Whole-pixel deltas, used by the launch and reflection paths. */
void dir_to_delta(u8 dir, int *dx, int *dy);
u8   delta_to_dir(int dx, int dy);

/* --- Bat deflection --------------------------------------------------- */

/* orig: LAB1F. `offset` is the contact point relative to the bat's left
 * edge; negative means contact left of the bat.
 *
 * This is a literal translation of the opcode flow, not a derivation.
 * Hand-tracing the zone tables predicts 0x2C for offset 21, but the
 * hardware returns 0x38: zones with bit 2 set fall through and reflect
 * TWICE. tests/test_physics.cpp pins the captured values.
 * See notes/bat-deflection.md. */
u8 bat_deflect_dir(u8 dir, int offset, bool big_bat);

/* orig: the `(dir XOR $1F) + 1` reflection used either side of the zone
 * lookup. Exposed because the lookup can fail (see below). */
u8 bat_reflect_dir(u8 dir);

/* Index of `dir` in the deflection table's 6 incoming directions, or -1.
 * The original's match loop SKIPS dir 0x10 (pure vertical), which never
 * arises in play — a synthetic 0x10 ball does not return cleanly. */
int bat_dir_index(u8 dir);

/* --- The bat ---------------------------------------------------------- */

const int BAT_BODY_W = 28;
/* The playfield the bat is clamped into. */
const int BAT_MARGIN_LEFT  = 0x08;
const int BAT_MARGIN_RIGHT = 0xF8;

/* One frame of bat movement, clamped to the playfield.
 *
 * orig: handling_bat $9F64 (SUB/ADD $04), then check_left_margin $ACA2 /
 * check_right_margin $ACBC.
 *
 * The ORDER is the point. The original moves unconditionally and clamps
 * afterwards, every frame. Guarding the move instead lets the bat rest
 * up to 3 px past the margin -- from x = min+2 a guarded -4 lands on
 * min-2 and sticks -- where the original always finishes exactly on the
 * margin. Both keys held cancels out, as it does on the Spectrum.
 *
 * A big bat is drawn centred on x, so its visible body extends
 * `extra_px` on each side and the clamp tightens by that much. */
int bat_step_x(int bat_x, int extra_px, bool move_left, bool move_right);

/* --- Sweep: the rectangle-overlap path (secondary balls, fallback) ---- */

struct BrickHit {
    bool hit;
    int  row, col;
    int  axis;        /* 1 = flip dy, 2 = flip dx */
};

BrickHit brick_sweep(const BrickField &field, int ball_w, int ball_h,
                     int prev_x, int prev_y, int new_x, int new_y);

/* --- Sweep: LAFFC, the byte-exact path (primary ball) ----------------- */

/* Which of the cell's faces are OPEN to the ball — bit0 left, 1 right,
 * 2 up, 3 down. A brick against a playfield boundary keeps that boundary
 * face open, so the ball bounces off it; inverting that is what let a ball
 * fall through row-0 metal bricks (known-bugs #6). */
struct LaffcHit {
    bool hit;
    int  row, col;
    int  cell_x, cell_y;    /* orig: Lx, Hy */
    u8   face_mask;
};

/* orig: LAFFC $AFFC, phases 1-5b. */
LaffcHit laffc_sweep(const BrickField &field, u8 dir,
                     int ball_w, int ball_h, int new_x, int new_y);

/* Where the ball ends up once the sweep has chosen a face: snapped to
 * that cell edge, with the direction reflected. orig: LAFFC_26-29. */
struct BallBounce { u8 x, y, dir; };

BallBounce laffc_bounce(const LaffcHit &hit, u8 dir,
                        int ball_w, int ball_h, int new_x, int new_y);

/* orig: change_direction $ACEE. $1F flips horizontal, $3F vertical. */
u8 laffc_change_dir(u8 dir, u8 mask);

/* Triple ball: the two extras take directions derived from the
 * primary's low nibble, keeping its quadrant. See known-bugs #8 — the
 * extras' dir bytes are read back through dir_to_delta, whose quadrant
 * convention is mirrored from the primary's in two of four quadrants.
 * orig: LA67B_8 $A67B */
struct ExtraBallDirs { u8 second, third; };

ExtraBallDirs extra_ball_dirs(u8 base_dir);

/* The original's shared fixed-point fall accelerator, LA55A_0.
 *
 * One 16-bit accumulator plus an 8-bit fraction. Each step adds `de` to
 * the accumulator, CLAMPS it by high byte to `cap_hi`, then adds the
 * accumulator to the fraction and returns the carry out as a signed
 * pixel delta. Three things fall through it with different constants:
 *
 *   falling bonuses  de=$0008 cap=$02   accelerate to 2 px/frame
 *   enemy bombs      de=$0008 cap=$02   the same curve
 *   the +400 marker  de=$0028 cap=$80   much faster, dies at y=$C0
 *
 * The clamp compares the HIGH BYTE for equality rather than the value
 * for >=, which is what the Z80 did; it matters because the accumulator
 * can step past the cap in one add if `de` is large enough, and then the
 * equality never fires. The constants above are all safe, and
 * tests/test_physics.cpp pins that.
 *
 * Pure arithmetic: no game state, which is why it lives here and not
 * with any one of its three callers. */
typedef struct {
    unsigned int  acc;
    unsigned char frac;
} motion_acc_t;

int motion_accel_step(motion_acc_t *m, unsigned int de, unsigned char cap_hi);

#endif /* BATTY_PHYSICS_H */
