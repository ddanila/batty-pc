/* Host-side tests for src/physics.cpp — the direction model and bat
 * deflection, against the Spectrum's captured ground truth.
 *
 * The deflection cases come from notes/bat-deflection.md, read off real
 * hardware via ZEsarUX. They matter because the decode CANNOT be derived
 * by hand: tracing the zone tables predicts 0x2C for offset 21 while the
 * hardware returns 0x38, since zones with bit 2 set reflect twice. A port
 * that looks reasonable can be wrong, so the tables are the authority.
 *
 * The equivalent QEMU gate boots the game, drops a ball onto the bat and
 * reads the outgoing direction ~80 s per run, and can only cover the
 * handful of trajectories that reach the bat. These run in microseconds
 * and sweep the whole input space. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../src/physics.cpp"

static int failures = 0;

static void check(bool ok, const char *fmt, ...) {
    if (ok) return;
    failures++;
    va_list ap;
    va_start(ap, fmt);
    printf("\n    ");
    vprintf(fmt, ap);
    va_end(ap);
}

/* Deterministic pseudo-random cell picker for the random-field trials. */
static unsigned long rng_like_seed = 1;
static int pseudo(int n) {
    rng_like_seed = rng_like_seed * 1103515245UL + 12345UL;
    return int((rng_like_seed >> 16) % (unsigned long)n);
}

static int tests_run = 0;

static void report(const char *name, int before, const char *detail) {
    tests_run++;
    printf("  %-28s %s\n", name, failures > before ? "FAIL" : detail);
}

/* --- Captured from hardware: incoming dir 0x0C, normal 28-wide bat ---- */
struct DeflectCase { int offset; u8 out_dir; };
static const DeflectCase HARDWARE_0C[] = {
    { -3, 0x28 },   /* contact left of the bat */
    {  5, 0x2C },
    { 13, 0x34 },
    { 21, 0x38 },   /* hand-tracing says 0x2C — hardware says 0x38 */
    { 29, 0x38 },
};

static void test_deflection_matches_hardware() {
    const int before = failures;
    for (unsigned i = 0; i < sizeof(HARDWARE_0C) / sizeof(HARDWARE_0C[0]); i++) {
        const DeflectCase &c = HARDWARE_0C[i];
        const u8 got = bat_deflect_dir(0x0C, c.offset, false);
        check(got == c.out_dir, "offset %d: got %02X want %02X\n",
              c.offset, got, c.out_dir);
    }
    report("deflection_matches_hardware", before, "5 captured cases      ok");
}

/* The double-reflect path is the subtlety the note warns about; assert it
 * is actually reachable, so a refactor that flattened it would fail here
 * rather than silently agreeing on the sampled offsets. */
static void test_double_reflect_zones_reachable() {
    const int before = failures;
    int doubled = 0;
    for (int offset = 0; offset < 40; offset++) {
        /* zone bit 2 set => reflect, look up, reflect again. Detect it by
         * comparing against a single lookup of the same zone. */
        const u8 got = bat_deflect_dir(0x0C, offset, false);
        const u8 single = bat_deflect_dir(0x0C, 21, false);
        if (offset < 16 && got != single) doubled++;
    }
    check(doubled > 0, "no offset exercised the double-reflect path\n");
    report("double_reflect_reachable", before, "bit-2 zones live      ok");
}

/* Deflection must be defined for every offset a bat contact can produce,
 * for both bat widths, and must always return a legal 6-bit direction. */
static void test_deflection_total_and_legal() {
    const int before = failures;
    int illegal = 0;
    for (int big = 0; big <= 1; big++) {
        for (int offset = -8; offset < 64; offset++) {
            const u8 incoming[6] = {0x04, 0x08, 0x0C, 0x14, 0x18, 0x1C};
            for (int i = 0; i < 6; i++) {
                const u8 out = bat_deflect_dir(incoming[i], offset, big != 0);
                if (out > 0x3F) illegal++;
            }
        }
    }
    check(illegal == 0, "%d results outside the 6-bit direction range\n", illegal);
    report("deflection_total_and_legal", before, "864 combinations      ok");
}

/* Deflection sends the ball UPWARD — every outgoing direction must have a
 * negative dy. A bat that returned the ball downward would drop it. */
static void test_deflection_sends_ball_up() {
    const int before = failures;
    int downward = 0;
    const u8 incoming[6] = {0x04, 0x08, 0x0C, 0x14, 0x18, 0x1C};
    for (int big = 0; big <= 1; big++) {
        for (int offset = -8; offset < 48; offset++) {
            for (int i = 0; i < 6; i++) {
                const u8 out = bat_deflect_dir(incoming[i], offset, big != 0);
                int dx, dy;
                dir_to_dxdy(out, 2, &dx, &dy);
                if (dy > 0) downward++;
            }
        }
    }
    check(downward == 0, "%d deflections sent the ball downward\n", downward);
    report("deflection_sends_ball_up", before, "672 combinations      ok");
}

/* The match loop skips 0x10 (pure vertical) — the original never produces
 * it and does not return cleanly for it. Pin that it stays skipped. */
static void test_pure_vertical_is_skipped() {
    const int before = failures;
    check(bat_dir_index(0x10) < 0, "dir 0x10 should not be in the table\n");
    const u8 expected[6] = {0x04, 0x08, 0x0C, 0x14, 0x18, 0x1C};
    for (int i = 0; i < 6; i++)
        check(bat_dir_index(expected[i]) == i,
              "dir %02X: index %d, want %d\n", expected[i], bat_dir_index(expected[i]), i);
    report("pure_vertical_is_skipped", before, "table order           ok");
}

/* Reflecting twice is the identity — the property the double-reflect path
 * depends on. */
static void test_reflect_is_an_involution() {
    const int before = failures;
    int bad = 0;
    for (int d = 0; d < 0x40; d++)
        if (bat_reflect_dir(bat_reflect_dir(u8(d))) != u8(d)) bad++;
    check(bad == 0, "%d of 64 directions did not survive a double reflect\n", bad);
    report("reflect_is_an_involution", before, "all 64 directions     ok");
}

/* dir_to_dxdy crosses its two components. The anchor is hardware: dir
 * 0x1F moves LEFT on the Spectrum (probed via
 * capture_frame_timeline_original.py --probe-ball). Dropping the cross
 * puts the right magnitude on the wrong axis and drifts slowly out of
 * parity rather than failing loudly. */
static void test_direction_components_are_crossed() {
    const int before = failures;
    int dx, dy;
    dir_to_dxdy(0x1F, 2, &dx, &dy);
    check(dx < 0, "dir 0x1F should move left, got dx=%d\n", dx);

    /* Within a quadrant every angle must share one sign pair — the sine
     * table is non-negative, so the quadrant alone fixes the signs. */
    int inconsistent = 0;
    for (int q = 0; q < 4; q++) {
        int fx, fy;
        dir_to_dxdy(u8(q * 0x10 | 1), 2, &fx, &fy);
        for (int angle = 2; angle < 15; angle++) {
            dir_to_dxdy(u8(q * 0x10 | angle), 2, &dx, &dy);
            if ((dx < 0) != (fx < 0) || (dy < 0) != (fy < 0)) inconsistent++;
        }
    }
    check(inconsistent == 0, "%d angles broke their quadrant's sign pair\n",
          inconsistent);
    report("direction_components_crossed", before, "56 angles             ok");
}

/* CHARACTERISATION, not an endorsement. dir_to_delta disagrees with
 * dir_to_dxdy on quadrants 0x10 and 0x30 — the two conventions mirror
 * each other there. dir_to_delta has NO production consumer since the
 * extras were unified onto dir_to_dxdy (known-bugs #8),
 * so secondary balls move mirrored relative to the primary in half the
 * directions. Whether that matches the original is unverified; see
 * notes/known-bugs.md. This test pins the current behaviour so the
 * discrepancy stays visible and cannot change unnoticed. */
static void test_delta_and_dxdy_conventions_differ() {
    const int before = failures;
    const int expect_mirrored[4] = { 0, 1, 0, 1 };   /* by quadrant */
    for (int q = 0; q < 4; q++) {
        int dx, dy, px, py;
        dir_to_dxdy(u8(q * 0x10 | 8), 2, &dx, &dy);
        dir_to_delta(u8(q * 0x10 | 8), &px, &py);
        const int mirrored = ((dx < 0) != (px < 0)) ? 1 : 0;
        check(mirrored == expect_mirrored[q],
              "quadrant %02X: mirrored=%d, expected %d\n",
              q * 0x10, mirrored, expect_mirrored[q]);
    }
    report("delta_vs_dxdy_conventions", before, "2 of 4 mirrored       ok");
}

/* Speed scales the components linearly — the ball-speed ramp relies on it. */
static void test_speed_scales_linearly() {
    const int before = failures;
    int bad = 0;
    for (int d = 0; d < 0x40; d++) {
        int dx1, dy1, dx3, dy3;
        dir_to_dxdy(u8(d), 1, &dx1, &dy1);
        dir_to_dxdy(u8(d), 3, &dx3, &dy3);
        if (dx3 != dx1 * 3 || dy3 != dy1 * 3) bad++;
    }
    check(bad == 0, "%d directions did not scale linearly with speed\n", bad);
    report("speed_scales_linearly", before, "all 64 directions     ok");
}

/* delta_to_dir and dir_to_delta must agree on the quadrant, or a
 * reflection can silently send the ball the wrong way. */
static void test_delta_roundtrip_quadrants() {
    const int before = failures;
    int bad = 0;
    const int deltas[4][2] = { {2, 2}, {2, -2}, {-2, -2}, {-2, 2} };
    for (int i = 0; i < 4; i++) {
        const u8 dir = delta_to_dir(deltas[i][0], deltas[i][1]);
        int dx, dy;
        dir_to_delta(dir, &dx, &dy);
        const bool sx = (dx >= 0) == (deltas[i][0] >= 0);
        const bool sy = (dy >= 0) == (deltas[i][1] >= 0);
        if (!sx || !sy) bad++;
    }
    check(bad == 0, "%d of 4 quadrants did not round-trip\n", bad);
    report("delta_roundtrip_quadrants", before, "4 quadrants           ok");
}

/* The ONE production caller of delta_to_dir is apply_multi_ball_bonus,
 * and it passes ball.dx/ball.dy — which are SIGNS, always in {-1,0,+1}.
 * delta_to_dir picks its angle with `abs(dx) >= BALL_SPEED` (= 2), so
 * from that caller the 0x08 angle is unreachable and every multiball
 * spawn gets 0x04. See notes/known-bugs.md #14.
 *
 * test_delta_roundtrip_quadrants above passes +-2, which exercises the
 * angle production never selects and skips the one it always does. This
 * pins the actual behaviour so that changing either side is a decision
 * rather than an accident. */
static void test_delta_to_dir_sign_inputs() {
    const int before = failures;
    const int deltas[8][2] = { {1,1}, {1,-1}, {-1,-1}, {-1,1},
                               {0,1}, {0,-1}, {1,0}, {-1,0} };
    int wrong_angle = 0;
    for (int i = 0; i < 8; i++) {
        const u8 dir = delta_to_dir(deltas[i][0], deltas[i][1]);
        if ((dir & 0x0F) != 0x04) wrong_angle++;
    }
    check(wrong_angle == 0,
          "%d of 8 sign inputs did not select angle 0x04\n", wrong_angle);

    /* and the quadrant still has to survive the round trip at magnitude 1 */
    int bad = 0;
    for (int i = 0; i < 4; i++) {
        const u8 dir = delta_to_dir(deltas[i][0], deltas[i][1]);
        int dx, dy;
        dir_to_delta(dir, &dx, &dy);
        if ((dx >= 0) != (deltas[i][0] >= 0)) bad++;
        if ((dy >= 0) != (deltas[i][1] >= 0)) bad++;
    }
    check(bad == 0, "%d sign mismatches round-tripping magnitude 1\n", bad);
    report("delta_to_dir_sign_inputs", before, "8 sign inputs         ok");
}


/* The shared fall accelerator (LA55A_0). Three falling things use it
 * with different constants and none of them had a test.
 *
 * The property that matters is the CURVE: velocity ramps and then
 * settles at the cap, so a bonus reaches 2 px/frame and stays there. A
 * bug that made it settle one step early or never settle would move
 * every falling object and be visible only as a slow drift against the
 * captured timings. */
static void test_motion_accel_ramps_then_caps() {
    const int before = failures;
    motion_acc_t m = {0, 0};
    int total = 0, last = 0, settled_at = -1;

    /* 64 steps to reach the cap, measured — de=$0008 needs acc to hit
     * $0200 and it climbs 8 per step. The first draft of this test
     * guessed 40 and failed; the guess was wrong, not the code. */
    for (int f = 0; f < 120; f++) {
        const int step = motion_accel_step(&m, FALL_DE_SLOW, FALL_CAP_SLOW);
        check(step >= 0 && step <= 2,
              "frame %d gave a %d px step; a falling bonus moves 0..2\n",
              f, step);
        total += step;
        if (step == 2 && last == 2 && settled_at < 0) settled_at = f;
        last = step;
    }
    check(settled_at > 0 && settled_at < 100,
          "the fall settled at 2 px/frame on frame %d\n", settled_at);
    /* Measured: 25 px in the first 40 frames, so 120 frames covers well
     * over 100 once it is at terminal speed. */
    check(total > 100, "120 frames of falling covered only %d px\n", total);
    /* Capped: the accumulator's high byte must sit exactly on the cap. */
    check((m.acc >> 8) == 0x02, "accumulator high byte is %02X, expected 02\n",
          (unsigned)(m.acc >> 8));
    report("motion_accel_ramps_then_caps", before, "0..2 px, settles     ok");
}

/* The FRACTION carries, which is the whole point of the accumulator.
 *
 * `m->frac = (unsigned char)sum` is what turns a sub-pixel velocity into
 * an occasional whole-pixel step. Dropping it (`m->frac = 0`) leaves the
 * ramp-and-cap test above green — the steps are still 0..2, it still
 * settles, and 120 frames still covers more than 100 px. But the object
 * does not move AT ALL for the first 20 frames instead of falling 6 px,
 * so every falling thing in the game starts late and lands in the wrong
 * place relative to the original.
 *
 * Exact cumulative distances, computed from the same recurrence, at
 * three points on the curve: early (fraction-dominated), mid, and past
 * the cap. */
static void test_motion_accel_fraction_carries() {
    const int before = failures;
    const int at[3] = {20, 40, 80};
    const int want[3] = {6, 25, 97};
    for (int k = 0; k < 3; k++) {
        motion_acc_t m = {0, 0};
        int total = 0;
        for (int f = 0; f < at[k]; f++)
            total += motion_accel_step(&m, FALL_DE_SLOW, FALL_CAP_SLOW);
        check(total == want[k],
              "%d frames of the bonus curve covered %d px, expected %d — "
              "if this is 0 early on, the fraction is being dropped\n",
              at[k], total, want[k]);
    }
    report("motion_accel_fraction", before, "20/40/80 frames exact ok");
}

/* The +400 marker uses a much steeper curve and a cap of $80. It must
 * still cap rather than run away, or the marker would outrun the screen
 * before its y=$C0 death check. */
static void test_motion_accel_fast_variant_caps() {
    const int before = failures;
    motion_acc_t m = {0, 0};
    /* Measured: the cap is reached at step 820 (acc climbs $28 to
     * $8000), so 1000 is comfortably past it. */
    for (int f = 0; f < 1000; f++) motion_accel_step(&m, FALL_DE_FAST, FALL_CAP_FAST);
    check((m.acc >> 8) == 0x80, "fast variant settled at %02X, expected 80\n",
          (unsigned)(m.acc >> 8));
    report("motion_accel_fast_caps", before, "de=$28 cap=$80       ok");
}

/* The clamp tests the HIGH BYTE for equality, not the value for >=,
 * because that is what the Z80 did. So a `de` large enough to step the
 * accumulator PAST the cap in one add never triggers it, and the value
 * runs away. None of the three production constants can do that — this
 * pins that fact rather than the bug, so if someone adds a fourth caller
 * with a big `de` the test says why it misbehaves. */
static void test_motion_accel_clamp_is_equality_not_ge() {
    const int before = failures;
    motion_acc_t m = {0, 0};
    /* de = $0180 steps the high byte 0 -> 1 -> 3, skipping the cap of 2. */
    for (int f = 0; f < 8; f++) motion_accel_step(&m, 0x0180, 0x02);
    check((m.acc >> 8) != 0x02,
          "a de that skips the cap was clamped anyway — the clamp is now a "
          ">= test, which is NOT what the original did; check the falling "
          "curves against the captured timings before keeping this\n");

    /* ...and every production constant does hit it. */
    const unsigned int de[2] = {FALL_DE_SLOW, FALL_DE_FAST};
    const unsigned char cap[2] = {FALL_CAP_SLOW, FALL_CAP_FAST};
    for (int i = 0; i < 2; i++) {
        motion_acc_t n = {0, 0};
        for (int f = 0; f < 1000; f++) motion_accel_step(&n, de[i], cap[i]);
        check((n.acc >> 8) == cap[i],
              "de=%04X cap=%02X settled at %02X\n",
              de[i], (unsigned)cap[i], (unsigned)(n.acc >> 8));
    }
    report("motion_accel_clamp_shape", before, "equality, not >=     ok");
}

/* --- Collision sweeps -------------------------------------------------- */



/* A field with every cell standing, then knock individual ones out. */
static u8 field_cells[FIELD_ROWS * FIELD_COLS];
static void field_fill(bool standing) {
    memset(field_cells, standing ? 0x01 : 0x80, sizeof(field_cells));
}
static void field_destroy(int row, int col) {
    field_cells[row * FIELD_COLS + col] = 0x80;
}

/* The row scan's 8-bit wrap test is `> $FF`, and one pixel decides it.
 *
 * The original walks the bands with an 8-bit subtract and uses the
 * BORROW rather than a signed compare, which the port reproduces as
 * `a = (band_y - new_y) & 0xFF` plus `a + BRICK_H_PX > 0xFF`. Written
 * out, that is `new_y - band_y <= 8`: a ball whose top is more than a
 * brick's height below the band top is past that row, not in it.
 *
 * `> $FE` accepts nine, and invents a hit one pixel beyond the brick.
 * Mutation found it — the whole host suite passed with the phantom row
 * in place. The pair below is the exact edge: 40 is the last row-0
 * position, 41 is the first miss. */
static void test_laffc_row_scan_edge_is_one_brick() {
    const int before = failures;
    field_fill(false);
    field_cells[0 * FIELD_COLS + 1] = 0x01;
    const BrickField field(field_cells);

    const LaffcHit last = laffc_sweep(field, 0x00, 8, 7, 16, 40);
    check(last.hit && last.row == 0,
          "y=40 should still be row 0: hit=%d row=%d\n",
          (int)last.hit, last.row);

    const LaffcHit past = laffc_sweep(field, 0x00, 8, 7, 16, 41);
    check(!past.hit,
          "y=41 is nine px below the band top and the brick is eight "
          "tall, so it must MISS; got hit=%d row=%d cell_y=%d\n",
          (int)past.hit, past.row, past.cell_y);

    report("laffc_row_scan_edge", before, "40 hits, 41 misses   ok");
}

/* The left/right direction gate is `>=`, and dir $10 is the only value
 * that can tell.
 *
 * LAFFC_15 rotates the direction and compares:
 *
 *     ADD A,$10 / AND $3F / CP $20
 *     JR NC,LAFFC_16     ; >= $20: RES 0,D  (drop the LEFT face)
 *     RES 1,D            ;  < $20: drop the RIGHT face
 *
 * `(dir + $10) & $3F == $20` happens only at dir $10 — straight down —
 * so `>=` and `>` agree everywhere else. The ball never carries $10
 * (bat_dir_index skips it deliberately), but the ENEMY does, and it
 * runs the same sweep through enemy_brick_reaction.
 *
 * Found by mutation: `>` passed the whole host suite. 239 positions at
 * dir $10 change, and these five are the first of them. */
static void test_laffc_dir_gate_is_ge_at_vertical() {
    const int before = failures;
    field_fill(false);
    field_cells[0 * FIELD_COLS + 1] = 0x01;
    const BrickField field(field_cells);
    for (int ny = 26; ny <= 30; ny++) {
        const LaffcHit h = laffc_sweep(field, 0x10, 8, 7, 16, ny);
        check(h.hit && h.face_mask == 0x04,
              "dir $10 at (16,%d): hit=%d mask=%02X, expected the UP face "
              "(04). `>` instead of `>=` gives 01\n",
              ny, (int)h.hit, h.face_mask);
    }
    report("laffc_dir_gate_ge_at_vertical", before, "dir $10, 5 rows      ok");
}

/* The corner tie-break: equal penetration bounces HORIZONTALLY.
 *
 * When both an x face and a y face survive the direction gate,
 * LAFFC_21..25 compares the two penetration depths and bounces off the
 * shallower axis. On an exact TIE the original is unambiguous:
 *
 *     LAFFC_25:
 *       LD E,D / CP C          ; A = y_pen, C = x_pen
 *       RES 2,D / RES 3,D      ; clear the vertical bits
 *       JP NC,LAFFC_17         ; y_pen >= x_pen: keep them cleared
 *       LD A,E / AND $0C / JP LAFFC_18
 *
 * `CP C` sets carry only when y_pen < x_pen, so `JP NC` takes the
 * verticals-cleared path on equality. The port's `>=` matches; `>`
 * would flip every tie to a vertical bounce.
 *
 * Nothing tested it. Mutating `>=` to `>` survived `make test-fast`,
 * which is what put this here. The tie is easy to reach — a ball
 * arriving diagonally at a brick's top-left corner ties whenever
 * `nx - cell_x == ny - cell_y - 1`, and the positions below are a
 * diagonal run of them. */
static void test_laffc_corner_tie_goes_horizontal() {
    const int before = failures;
    field_fill(false);
    field_cells[0 * FIELD_COLS + 1] = 0x01;     /* one standing brick */
    const BrickField field(field_cells);

    /* Six consecutive ties, dir $00, against the brick at col 1. Found
     * by dumping every (dir, x, y) hit under BOTH rules and diffing:
     * 236 positions change, and these are the first six.
     *
     * That is how they had to be found. An earlier attempt located
     * "ties" by recomputing the penetrations from the RETURNED
     * face_mask — but the returned mask is the tie-break's OUTPUT, so
     * the formulas it selected were the wrong ones and the positions
     * were not ties at all. The mutation survived that test too. */
    int wrong = 0;
    for (int i = 0; i < 6; i++) {
        const LaffcHit h = laffc_sweep(field, 0x00, 8, 7, 17 + i, 26 + i);
        if (!h.hit || h.face_mask != 0x01) {
            wrong++;
            check(false, "tie at (%d,%d): hit=%d mask=%02X, expected a "
                         "LEFT-face bounce (mask 01); `>` gives 04\n",
                  17 + i, 26 + i, (int)h.hit, h.face_mask);
        }
    }
    if (wrong == 0)
        report("laffc_corner_tie_horizontal", before, "6 exact ties         ok");
    else
        report("laffc_corner_tie_horizontal", before, "");
}

/* laffc_sweep's left clamp: `a = new_x - FIELD_X0; if (a < 0) a = 0;`.
 *
 * CHARACTERISATION of unreachable-but-deliberate code. Deleting the
 * clamp survived all 76 QEMU gates on 2026-08-10, which answered an
 * open question in notes/refactor-plan.md — nothing guards it — and the
 * follow-up answered why nothing needs to.
 *
 * `a` feeds only `x_pen_in_cell`, and that feeds only
 * `straddles_x = (x_pen_in_cell + ball_w) >= BRICK_W_PX`. Clamped and
 * unclamped therefore differ only when `a < BRICK_W_PX - ball_w`:
 *
 *   - the ball is 8 px wide, so 16 - 8 = 8 and `a` would have to be
 *     below 8 while negative — impossible, the two conditions exclude
 *     each other. The ball can never reach it at all.
 *   - the enemy is 24 px, so `a < -8`, i.e. `new_x < 0`. The enemy's
 *     x is a u8 clamped to >= $08 by check_margins every frame and it
 *     moves a few px per frame, so `new_x` bottoms out near 5.
 *
 * Unreachable through either caller. It stays because `laffc_sweep` is
 * a pure function that should not depend on its callers' clamping, and
 * this test is what makes a future caller passing a negative x a
 * deliberate change rather than a silent one. */
static void test_laffc_left_clamp_is_flat() {
    const int before = failures;
    field_fill(true);
    /* Column 0 EMPTY so the straddle branch is the one under test. With
     * a full field `standing(row, 0)` is true, the branch never runs,
     * and x_pen_in_cell — the only thing the clamp feeds — is never
     * read. The first draft filled everything and the mutant survived
     * the test as well as the QEMU suite. */
    field_destroy(0, 0);
    const BrickField field(field_cells);
    const int y = FIELD_Y0 + 4;
    int differing = 0;
    for (int w = 8; w <= 24; w += 8) {
        const LaffcHit at_edge = laffc_sweep(field, 0x08, w, 7, FIELD_X0, y);
        for (int x = FIELD_X0 - 40; x < FIELD_X0; x++) {
            const LaffcHit out = laffc_sweep(field, 0x08, w, 7, x, y);
            if (out.hit != at_edge.hit || out.col != at_edge.col
                || out.cell_x != at_edge.cell_x)
                differing++;
        }
    }
    check(differing == 0,
          "%d of the left-of-field positions resolved to a different "
          "column than FIELD_X0 does; the clamp is what flattens them\n",
          differing);
    report("laffc_left_clamp_is_flat", before, "3 widths x 40 px     ok");
}

/* A ball entirely outside the band must never report a hit, whatever the
 * grid looks like — the cheapest possible guard against a stray index. */
static void test_sweeps_miss_outside_the_band() {
    const int before = failures;
    field_fill(true);
    const BrickField field(field_cells);
    int hits = 0;
    for (int x = -32; x < 300; x += 3) {
        for (int y = -32; y < FIELD_Y0 - 8; y += 3) {
            if (brick_sweep(field, 8, 7, x, y, x, y).hit) hits++;
            if (laffc_sweep(field, 0x08, 8, 7, x, y).hit) hits++;
        }
        for (int y = FIELD_Y_END; y < 200; y += 3) {
            if (brick_sweep(field, 8, 7, x, y, x, y).hit) hits++;
            if (laffc_sweep(field, 0x08, 8, 7, x, y).hit) hits++;
        }
    }
    check(hits == 0, "%d hits reported outside the brick band\n", hits);
    report("sweeps_miss_outside_band", before, "~12k positions       ok");
}

/* An empty field can never be hit. */
static void test_sweeps_miss_empty_field() {
    const int before = failures;
    field_fill(false);
    const BrickField field(field_cells);
    int hits = 0;
    for (int x = -8; x < 260; x += 2)
        for (int y = FIELD_Y0 - 8; y < FIELD_Y_END + 8; y += 2)
            for (int d = 0; d < 0x40; d += 8) {
                if (brick_sweep(field, 8, 7, x, y, x, y).hit) hits++;
                if (laffc_sweep(field, u8(d), 8, 7, x, y).hit) hits++;
            }
    check(hits == 0, "%d hits reported on a cleared field\n", hits);
    report("sweeps_miss_empty_field", before, "all cells destroyed  ok");
}

/* Any reported hit must name a cell that is in range AND standing. This is
 * the invariant that keeps the effects half from scoring a phantom brick. */
static void test_hits_name_a_standing_cell() {
    const int before = failures;
    int bad = 0;
    rng_like_seed = 12345;
    for (int trial = 0; trial < 400; trial++) {
        field_fill(true);
        for (int k = 0; k < 60; k++)
            field_destroy(pseudo(FIELD_ROWS), pseudo(FIELD_COLS));
        const BrickField field(field_cells);
        for (int x = 0; x < 250; x += 7) {
            for (int y = FIELD_Y0 - 4; y < FIELD_Y_END + 4; y += 5) {
                const BrickHit b = brick_sweep(field, 8, 7, x, y, x, y);
                if (b.hit && !field.standing(b.row, b.col)) bad++;
                if (b.hit && (b.axis != 1 && b.axis != 2)) bad++;
                for (int d = 0; d < 0x40; d += 16) {
                    const LaffcHit l = laffc_sweep(field, u8(d), 8, 7, x, y);
                    if (l.hit && !field.standing(l.row, l.col)) bad++;
                }
            }
        }
    }
    check(bad == 0, "%d hits named a destroyed or out-of-range cell\n", bad);
    report("hits_name_standing_cell", before, "400 random fields    ok");
}

/* The cell origin a hit reports must match its row/col — the bounce snaps
 * the ball to those pixel edges, so a mismatch teleports it. */
static void test_hit_origin_matches_cell() {
    const int before = failures;
    int bad = 0;
    field_fill(true);
    const BrickField field(field_cells);
    for (int x = 0; x < 250; x += 3)
        for (int y = FIELD_Y0; y < FIELD_Y_END; y += 3)
            for (int d = 0; d < 0x40; d += 8) {
                const LaffcHit l = laffc_sweep(field, u8(d), 8, 7, x, y);
                if (!l.hit) continue;
                if (l.cell_x != FIELD_X0 + l.col * BRICK_W_PX) bad++;
                if (l.cell_y != FIELD_Y0 + l.row * BRICK_H_PX) bad++;
            }
    check(bad == 0, "%d hits reported an origin inconsistent with row/col\n", bad);
    report("hit_origin_matches_cell", before, "full band sweep      ok");
}

/* known-bugs #6: a brick against a playfield boundary keeps that boundary
 * face OPEN, so a ball arriving along it bounces instead of passing
 * through. Inverting this let balls fall through row-0 metal bricks. */
/* The straddle boundary is INCLUSIVE, and that matters at exactly one
 * pixel offset.
 *
 * When the ball's own cell is gone, LAFFC_5-6 tries the cell to the
 * right if the ball's body reaches into it:
 * `(x_pen_in_cell + ball_w) >= BRICK_W_PX`. The `>=` is the whole
 * question — with `>` the ball whose body ENDS exactly on the cell edge
 * stops straddling, misses the standing brick next door, and passes
 * through it. That is the same failure family as known-bugs #6.
 *
 * Nothing pinned it: mutating `>=` to `>` survived this suite.
 *
 * Measured with an 8px-wide ball whose own cell (col 2) is destroyed and
 * col 3 standing — penetration 6 and 7 miss, 8 and 9 straddle to col 3.
 * So 7 and 8 bracket the boundary exactly. The vertical straddle is
 * checked the same way below; both `>=` were unpinned. */
static void test_straddle_boundary_is_inclusive() {
    const int before = failures;
    u8 cells[FIELD_ROWS * FIELD_COLS];
    memset(cells, 0x80, sizeof(cells));           /* every brick gone... */
    cells[0 * FIELD_COLS + 3] = 0x01;             /* ...except col 3 */
    const BrickField field(cells);
    const int cell_x = FIELD_X0 + 2 * BRICK_W_PX; /* own cell: col 2, gone */

    const LaffcHit just_short = laffc_sweep(field, 0x08, 8, 7,
                                            cell_x + 7, FIELD_Y0);
    check(!just_short.hit,
          "a ball one pixel short of the cell edge straddled anyway "
          "(col %d)\n", just_short.col);

    const LaffcHit exact = laffc_sweep(field, 0x08, 8, 7,
                                       cell_x + 8, FIELD_Y0);
    check(exact.hit && exact.col == 3,
          "a ball whose body ends EXACTLY on the cell edge did not "
          "straddle into the standing brick (hit=%d col=%d) — it would "
          "pass straight through\n", (int)exact.hit, exact.col);
    /* The VERTICAL straddle has the same inclusive boundary and was
     * equally unpinned. Own cell r2c2 gone, r3c2 standing, ball 4 tall:
     * penetration 3 misses, 4 straddles down to row 3. */
    memset(cells, 0x80, sizeof(cells));
    cells[3 * FIELD_COLS + 2] = 0x01;
    const BrickField below(cells);
    const int cy = FIELD_Y0 + 2 * BRICK_H_PX;

    const LaffcHit y_short = laffc_sweep(below, 0x08, 8, 4, cell_x, cy + 3);
    check(!y_short.hit,
          "a ball one pixel short of the row edge straddled down anyway "
          "(row %d)\n", y_short.row);

    const LaffcHit y_exact = laffc_sweep(below, 0x08, 8, 4, cell_x, cy + 4);
    check(y_exact.hit && y_exact.row == 3,
          "a ball whose body ends EXACTLY on the row edge did not "
          "straddle into the brick below (hit=%d row=%d)\n",
          (int)y_exact.hit, y_exact.row);
    report("straddle_boundary_inclusive", before,
           "x 7/8 and y 3/4       ok");
}


/* A brick against a playfield boundary keeps that boundary face OPEN.
 *
 * This is the half of the open-face rule that known-bugs #6 had
 * INVERTED: the port had `Lx != $08 && EMPTY` where the original has
 * "open when the neighbour is gone OR the cell is against the
 * boundary". Inverted, a boundary brick's outer face read CLOSED and a
 * ball straight into it was not deflected — it passed through.
 * User-reported, fixed 2026-06-17.
 *
 * This test was named for that property and did not check it. It
 * asserted only that a hit OCCURRED and named the right cell, never
 * face_mask — so mutating `cell_x == FIELD_X0` to `!=` survived it, and
 * survived test-laffc-ball-frame1 too, whose L3 trajectory never decides
 * a left-boundary cell.
 *
 * It also used dir $28 for the left case, which the direction gate
 * strips bit 1 from, so the left face could not have been asserted even
 * had someone tried. Measured: with every brick standing, a
 * left-boundary cell reports the left face open for dirs $00-$0C and
 * $30-$3C, and closed for $10-$2C.
 *
 * WHAT IS ACTUALLY BEING CAUGHT. The `cell_x == FIELD_X0` term is
 * REDUNDANT: BrickField::standing treats out-of-range as gone, so
 * `!standing(row, -1)` is already true at the left edge. Deleting the
 * term survives this test, and correctly — it is an equivalent mutant,
 * kept because it mirrors the original's LAFFC structure.
 *
 * What the interior control below catches is the INVERSION, which is
 * bug #6's actual shape: `!=` makes the face open for every non-boundary
 * cell. Worth stating, so nobody reads a green run here as proof that
 * the boundary term itself is load-bearing. */
static void test_boundary_faces_stay_open() {
    const int before = failures;
    field_fill(true);
    const BrickField field(field_cells);

    /* Top row, ball heading down: the UP face is against the boundary. */
    const LaffcHit top = laffc_sweep(field, 0x08, 8, 7, FIELD_X0, FIELD_Y0);
    check(top.hit, "no hit on the top row\n");
    check(top.row == 0, "expected row 0, got %d\n", top.row);
    check((top.face_mask & 4) != 0,
          "the UP face of a top-row brick reads CLOSED (mask=%02X) — "
          "known-bugs #6, the ball is not deflected\n",
          (unsigned)top.face_mask);

    /* Left column. dir $38 keeps bit 1 through the direction gate. */
    const LaffcHit left = laffc_sweep(field, 0x38, 8, 7, FIELD_X0, FIELD_Y0 + 40);
    check(left.hit && left.col == 0, "no hit in the left column\n");
    check(left.cell_x == FIELD_X0, "left column origin is %d\n", left.cell_x);
    check((left.face_mask & 1) != 0,
          "the LEFT face of a left-column brick reads CLOSED (mask=%02X)\n",
          (unsigned)left.face_mask);

    /* Negative control, so neither check above can pass by being always
     * open: an INTERIOR brick whose left neighbour stands must report
     * its left face closed. */
    const LaffcHit inner = laffc_sweep(field, 0x38, 8, 7,
                                       FIELD_X0 + 4 * BRICK_W_PX,
                                       FIELD_Y0 + 40);
    check(inner.hit, "no hit on the interior brick\n");
    check((inner.face_mask & 1) == 0,
          "an interior brick with a standing left neighbour reports its "
          "left face OPEN (mask=%02X)\n", (unsigned)inner.face_mask);
    report("boundary_faces_stay_open", before, "faces, not just hits ok");
}

/* The bounce always lands the ball flush against the cell it hit, never
 * inside it — a ball left overlapping would re-collide forever. */
static void test_bounce_clears_the_cell() {
    const int before = failures;
    int inside = 0;
    field_fill(true);
    const BrickField field(field_cells);
    for (int x = 0; x < 250; x += 5)
        for (int y = FIELD_Y0; y < FIELD_Y_END; y += 3)
            for (int d = 0; d < 0x40; d += 8) {
                const LaffcHit l = laffc_sweep(field, u8(d), 8, 7, x, y);
                if (!l.hit) continue;
                const BallBounce b = laffc_bounce(l, u8(d), 8, 7, x, y);
                const int bx = b.x, by = b.y;
                const bool overlaps_x = bx + 8 > l.cell_x && bx < l.cell_x + BRICK_W_PX;
                const bool overlaps_y = by + 7 > l.cell_y && by < l.cell_y + BRICK_H_PX;
                if (overlaps_x && overlaps_y) inside++;
            }
    check(inside == 0, "%d bounces left the ball inside the brick\n", inside);
    report("bounce_clears_the_cell", before, "full band sweep      ok");
}

/* `((dir ^ mask) + 1) & 0x3F` is a fixed point exactly at the four pure
 * axis-aligned directions: 0x10 and 0x30 for a horizontal flip, 0x00 and
 * 0x20 for a vertical one. Those are the no-op bounces behind
 * known-bugs #6 — and the game never generates them (bat_dir_index skips
 * 0x10, and the deflection tables emit none of the four). So the
 * invariant is: every direction the game can actually produce turns
 * around. If a future change started emitting a pure axis direction, this
 * catches it. */
static void test_reflection_fixed_points_are_unreachable() {
    const int before = failures;
    int wrong_fixed = 0;
    for (int d = 0; d < 0x40; d++) {
        const bool pure_axis = (d == 0x00 || d == 0x10 || d == 0x20 || d == 0x30);
        const bool h_fixed = laffc_change_dir(u8(d), 0x1F) == u8(d);
        const bool v_fixed = laffc_change_dir(u8(d), 0x3F) == u8(d);
        if ((h_fixed || v_fixed) != pure_axis) wrong_fixed++;
    }
    check(wrong_fixed == 0,
          "%d directions disagreed with the pure-axis fixed-point set\n",
          wrong_fixed);

    /* None of the four is reachable from the bat. */
    int emitted = 0;
    const u8 incoming[6] = {0x04, 0x08, 0x0C, 0x14, 0x18, 0x1C};
    for (int big = 0; big <= 1; big++)
        for (int offset = -8; offset < 48; offset++)
            for (int i = 0; i < 6; i++) {
                const u8 out = bat_deflect_dir(incoming[i], offset, big != 0);
                if (out == 0x00 || out == 0x10 || out == 0x20 || out == 0x30) emitted++;
            }
    check(emitted == 0, "the bat emitted a pure-axis direction %d times\n", emitted);
    report("reflection_fixed_points", before, "4, all unreachable   ok");
}

/* Every direction the game can produce turns around when it bounces. */
static void test_bounce_changes_direction() {
    const int before = failures;
    int unchanged = 0;
    field_fill(true);
    const BrickField field(field_cells);
    for (int x = 0; x < 250; x += 5)
        for (int y = FIELD_Y0; y < FIELD_Y_END; y += 3)
            for (int d = 0; d < 0x40; d += 4) {
                if (d == 0x00 || d == 0x10 || d == 0x20 || d == 0x30) continue;
                const LaffcHit l = laffc_sweep(field, u8(d), 8, 7, x, y);
                if (!l.hit) continue;
                if (laffc_bounce(l, u8(d), 8, 7, x, y).dir == u8(d)) unchanged++;
            }
    check(unchanged == 0, "%d bounces returned the incoming direction\n", unchanged);

    /* ...and it must change on the RIGHT AXIS. Checking only that the
     * direction changed let the left face reflect VERTICALLY: mutating
     * its mask from $1F to $3F survived, which would send a ball hitting
     * a brick's side away along the wrong axis entirely.
     *
     * Asserted as a property rather than by restating the formula: a
     * horizontal face (left/right) must flip the sign of dx and keep the
     * sign of dy, and a vertical face must do the opposite. Repeating
     * `laffc_change_dir(dir, 0x1F)` here would only prove the test can
     * copy the code.
     *
     * Signs come from dir_to_dxdy, the motion the BALL actually uses.
     * dir_to_delta looks equivalent and is not: its quadrant convention
     * is mirrored in two of four quadrants (known-bugs #8), and using it
     * here reported all 19200 bounces as wrong. */
    int wrong_axis = 0;
    for (int x = 0; x < 250; x += 5)
        for (int y = FIELD_Y0; y < FIELD_Y_END; y += 3)
            for (int d = 0; d < 0x40; d += 4) {
                if (d == 0x00 || d == 0x10 || d == 0x20 || d == 0x30) continue;
                const LaffcHit l = laffc_sweep(field, u8(d), 8, 7, x, y);
                if (!l.hit) continue;
                const u8 nd = laffc_bounce(l, u8(d), 8, 7, x, y).dir;
                int dx0, dy0, dx1, dy1;
                dir_to_dxdy(u8(d), 2, &dx0, &dy0);
                dir_to_dxdy(nd, 2, &dx1, &dy1);
                /* Same precedence as the implementation walks the mask. */
                const bool horizontal = (l.face_mask & 3) != 0;
                const bool dx_flipped = (dx0 >= 0) != (dx1 >= 0);
                const bool dy_flipped = (dy0 >= 0) != (dy1 >= 0);
                if (horizontal ? (!dx_flipped || dy_flipped)
                               : (!dy_flipped || dx_flipped)) wrong_axis++;
            }
    check(wrong_axis == 0,
          "%d bounces reflected on the wrong axis for the face they hit\n",
          wrong_axis);
    report("bounce_changes_direction", before, "changed + right axis ok");
}

/* --- Bat steering ------------------------------------------------------ */

/* The bat must never come to rest off the playfield, from any starting
 * position and any key combination. Guarding the move instead of clamping
 * after it leaves the bat up to 3 px past the margin. */
static void test_bat_never_rests_outside() {
    const int before = failures;
    int outside = 0;
    for (int extra = 0; extra <= 8; extra += 2) {
        const int min_x = BAT_MARGIN_LEFT + extra;
        const int max_x = BAT_MARGIN_RIGHT - BAT_BODY_W - extra;
        for (int x = -16; x < 280; x++) {
            for (int keys = 0; keys < 4; keys++) {
                const int got = bat_step_x(x, extra, (keys & 1) != 0, (keys & 2) != 0);
                if (got < min_x || got > max_x) outside++;
            }
        }
    }
    check(outside == 0, "%d results landed outside the playfield\n", outside);
    report("bat_never_rests_outside", before, "5 widths x 296 x 4   ok");
}

/* Repeatedly steering into a wall must settle EXACTLY on the margin, not
 * short of it -- that difference is visible as a gap the original does
 * not have. */
static void test_bat_settles_on_the_margin() {
    const int before = failures;
    for (int extra = 0; extra <= 8; extra += 4) {
        int x = 120;
        for (int i = 0; i < 100; i++) x = bat_step_x(x, extra, true, false);
        check(x == BAT_MARGIN_LEFT + extra,
              "extra %d: settled at %d, expected exactly %d\n",
              extra, x, BAT_MARGIN_LEFT + extra);

        x = 120;
        for (int i = 0; i < 100; i++) x = bat_step_x(x, extra, false, true);
        check(x == BAT_MARGIN_RIGHT - BAT_BODY_W - extra,
              "extra %d: settled at %d, expected exactly %d\n",
              extra, x, BAT_MARGIN_RIGHT - BAT_BODY_W - extra);
    }
    report("bat_settles_on_the_margin", before, "both walls, 3 widths ok");
}

/* Both keys cancel; neither key holds position. */
static void test_bat_opposing_keys_cancel() {
    const int before = failures;
    int moved = 0;
    for (int x = 40; x < 200; x++) {
        if (bat_step_x(x, 0, true, true) != x) moved++;
        if (bat_step_x(x, 0, false, false) != x) moved++;
    }
    check(moved == 0, "%d positions moved when they should not have\n", moved);
    report("bat_opposing_keys_cancel", before, "160 positions        ok");
}

/* Double Play splits the court at $80 and neither bat may cross.
 *
 * The two clamps are asymmetric on purpose: bat 1's RIGHT edge is what
 * stops (LACCE compares x + width), bat 2's LEFT edge is (LACAD
 * compares x alone). Written symmetrically, one bat would overlap the
 * separator by its own width. */
static void test_double_play_court_clamps() {
    const int before = failures;
    int bad = 0;

    /* Bat 1, at both settled widths: free below the divider, pinned so
     * its right edge lands exactly on $80 above it. */
    const int widths[2] = { 0x1C, 0x2C };
    for (int w = 0; w < 2; w++) {
        const int width = widths[w];
        for (int x = BAT_MARGIN_LEFT; x + width <= BAT_MARGIN_RIGHT; x++) {
            const int got  = bat_court_clamp_1(x, width);
            const int want = (x + width < 0x80) ? x : 0x80 - width;
            if (got != want) bad++;
        }
    }
    check(bad == 0, "%d bat-1 positions clamped wrong\n", bad);

    /* The pinned bat's right edge is ON the divider, never through it. */
    for (int w = 0; w < 2; w++)
        check(bat_court_clamp_1(0xD0, widths[w]) + widths[w] == 0x80,
              "pinned bat 1 (w=%d) does not end on the divider\n", widths[w]);

    /* Bat 2: pinned below $80, free at and above it. */
    bad = 0;
    for (int x = 0; x <= BAT_MARGIN_RIGHT; x++) {
        const int want = (x < 0x80) ? 0x80 : x;
        if (bat_court_clamp_2(x) != want) bad++;
    }
    check(bad == 0, "%d bat-2 positions clamped wrong\n", bad);

    /* The two courts touch and do not overlap: bat 1 pinned right and
     * bat 2 pinned left share the divider column and nothing more. */
    check(bat_court_clamp_1(0xDC, 0x1C) + 0x1C == bat_court_clamp_2(0),
          "the pinned bats do not meet flush at the divider\n");

    /* CHARACTERISATION of LACCE's 8-bit sum, which DOES wrap — the first
     * draft of the check above passed x=$FF and watched the clamp let a
     * bat straight through, because u8($1C + $FF) = $1B < $80.
     *
     * It is unreachable in play and the two halves of that are worth
     * separating. bat_step_x caps the right edge at $F8, so the sum
     * tops out there for both settled widths — 8 short of the wrap.
     * That is the caller's clamp doing the work, not the arithmetic,
     * which is why the u8 stays in bat_court_clamp_1: an invented
     * 16-bit sum would be a silent deviation, and the enemy's copy of
     * this idiom overflows for real (notes/known-bugs.md). */
    for (int w = 0; w < 2; w++) {
        const int extra = (w == 0) ? 0 : 8;
        const int width = widths[w];
        const int x_max = bat_step_x(0xFF, extra, false, true) - extra;
        check(x_max + width == BAT_MARGIN_RIGHT,
              "widest reachable bat (w=%d) ends at %d, not $F8\n",
              width, x_max + width);
        check(u8(x_max + width) == BAT_MARGIN_RIGHT,
              "the reachable sum wrapped — the clamp would leak\n");
    }
    check(bat_court_clamp_1(0xFF, 0x1C) == 0xFF,
          "the unreachable wrap has been papered over; if that was "
          "deliberate, this characterisation should have changed with it\n");

    report("double_play_court_clamps", before, "both bats, 2 widths  ok");
}

/* A step is 4 px, which at 50 Hz is the original's 200 px/s. */
static void test_bat_step_is_four_pixels() {
    const int before = failures;
    int wrong = 0;
    for (int x = 40; x < 200; x++) {
        if (bat_step_x(x, 0, true, false)  != x - 4) wrong++;
        if (bat_step_x(x, 0, false, true) != x + 4) wrong++;
    }
    check(wrong == 0, "%d steps were not 4 px\n", wrong);
    report("bat_step_is_four_pixels", before, "160 positions        ok");
}

/* The extras' directions are derived, not chosen: the primary's quadrant
 * is preserved and only the low nibble is remapped. If a refactor ever
 * lets the quadrant leak, every multi-ball trajectory mirrors. */
static void test_extra_balls_keep_the_quadrant() {
    const int before = failures;
    int wrong = 0;
    for (int q = 0x00; q <= 0x30; q += 0x10) {
        for (int low = 0; low <= 0x0F; low++) {
            const u8 base = (u8)(q | low);
            const ExtraBallDirs d = extra_ball_dirs(base);
            if ((d.second & 0x30) != q || (d.third & 0x30) != q) wrong++;
            if ((d.second & 0xC0) || (d.third & 0xC0)) wrong++;
        }
    }
    check(wrong == 0, "%d derived directions left the primary's quadrant\n", wrong);
    report("extra_balls_keep_the_quadrant", before, "4 quadrants x 16   ok");
}

/* The three-way split on the low nibble, from LA67B_8. The two extras
 * must never share a direction, or the multi-ball is a double-ball. */
static void test_extra_ball_dirs_split_three_ways() {
    const int before = failures;
    for (int q = 0x00; q <= 0x30; q += 0x10) {
        const ExtraBallDirs from4 = extra_ball_dirs((u8)(q | 0x04));
        check(from4.second == (u8)(q | 0x0C) && from4.third == (u8)(q | 0x08),
              "low $04 in quadrant $%02X gave $%02X/$%02X\n",
              q, from4.second, from4.third);

        const ExtraBallDirs from8 = extra_ball_dirs((u8)(q | 0x08));
        check(from8.second == (u8)(q | 0x0C) && from8.third == (u8)(q | 0x04),
              "low $08 in quadrant $%02X gave $%02X/$%02X\n",
              q, from8.second, from8.third);

        const ExtraBallDirs other = extra_ball_dirs((u8)(q | 0x0C));
        check(other.second == (u8)(q | 0x08) && other.third == (u8)(q | 0x04),
              "low $0C in quadrant $%02X gave $%02X/$%02X\n",
              q, other.second, other.third);
    }
    int same = 0;
    for (int base = 0; base < 0x40; base++) {
        const ExtraBallDirs d = extra_ball_dirs((u8)base);
        if (d.second == d.third) same++;
    }
    check(same == 0, "%d inputs gave both extras the same direction\n", same);
    report("extra_ball_dirs_split_three_ways", before, "4 quadrants + 64   ok");
}

int main() {
    printf("physics tests\n");
    test_deflection_matches_hardware();
    test_double_reflect_zones_reachable();
    test_deflection_total_and_legal();
    test_deflection_sends_ball_up();
    test_pure_vertical_is_skipped();
    test_reflect_is_an_involution();
    test_direction_components_are_crossed();
    test_delta_and_dxdy_conventions_differ();
    test_speed_scales_linearly();
    test_delta_roundtrip_quadrants();
    test_delta_to_dir_sign_inputs();
    test_motion_accel_ramps_then_caps();
    test_motion_accel_fraction_carries();
    test_motion_accel_fast_variant_caps();
    test_motion_accel_clamp_is_equality_not_ge();
    test_laffc_row_scan_edge_is_one_brick();
    test_laffc_dir_gate_is_ge_at_vertical();
    test_laffc_corner_tie_goes_horizontal();
    test_laffc_left_clamp_is_flat();
    test_sweeps_miss_outside_the_band();
    test_sweeps_miss_empty_field();
    test_hits_name_a_standing_cell();
    test_hit_origin_matches_cell();
    test_straddle_boundary_is_inclusive();
    test_boundary_faces_stay_open();
    test_bounce_clears_the_cell();
    test_reflection_fixed_points_are_unreachable();
    test_bounce_changes_direction();
    test_bat_never_rests_outside();
    test_bat_settles_on_the_margin();
    test_bat_opposing_keys_cancel();
    test_bat_step_is_four_pixels();
    test_double_play_court_clamps();
    test_extra_balls_keep_the_quadrant();
    test_extra_ball_dirs_split_three_ways();
    printf("\n%d tests, %d failed\n", tests_run, failures);
    return failures ? 1 : 0;
}
