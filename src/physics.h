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
 * The collision half of ball physics is not here yet: brick_collision and
 * laffc_collision still mix geometry with effects (scoring, sound, grid
 * mutation, bonus spawning), so they cannot move until that split is
 * made. */

#ifndef BATTY_PHYSICS_H
#define BATTY_PHYSICS_H

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

#endif /* BATTY_PHYSICS_H */
