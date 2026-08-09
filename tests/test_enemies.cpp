/* Host-side tests for src/enemies.cpp — how the alien steers.
 *
 * The equivalent QEMU gate (test-enemy-steer, ~35 s) boots the game,
 * seeds one alien and reads three directions. These drive the same logic
 * over every starting angle and every target, which is where the two
 * interesting properties live: the turn must take the SHORTER way round,
 * and the alien must never be able to settle against a wall. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/enemies.cpp"

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

static void report(const char *name, int before, const char *detail) {
    printf("  %-28s %s\n", name, failures > before ? "FAIL" : detail);
}

/* Fixed random source, so a failure is reproducible. */
static u8 canned = 0;
static u8 read_canned() { return canned; }

static Object alien(u8 dir, u8 target) {
    Object o;
    memset(&o, 0, sizeof(o));
    o.dir = dir;
    o.bonus_applied = target;
    o.w_body_px = 16;
    o.h_body_px = 12;
    o.x_coord = 120;
    o.y_coord = 60;
    return o;
}

/* Six-bit angles wrap, so "toward" has two candidates; the original
 * always takes the shorter arc. Taking the longer one is not a crash —
 * the alien just visibly swings the wrong way. */
static void test_turns_the_shorter_way() {
    const int before = failures;
    enemy_set_random(read_canned, read_canned);
    int wrong = 0;
    for (int start = 0; start < 0x40; start++) {
        for (int target = 0; target < 0x40; target++) {
            if (start == target) continue;
            Object o = alien(u8(start), u8(target));
            enemy_turn_towards_target(o);

            const int before_gap = ((target - start) & 0x3F);
            const int after_gap  = ((target - o.dir) & 0x3F);
            /* The gap must shrink, measured the way round it was going. */
            const int shortest_before = before_gap > 32 ? 64 - before_gap : before_gap;
            const int shortest_after  = after_gap  > 32 ? 64 - after_gap  : after_gap;
            if (shortest_after >= shortest_before) wrong++;
        }
    }
    check(wrong == 0, "%d (start, target) pairs turned the long way\n", wrong);
    report("turns_the_shorter_way", before, "4032 pairs           ok");
}

/* Steering must converge: from any angle to any target, repeated turns
 * must arrive. A step that overshoots would orbit forever. */
static void test_steering_converges() {
    const int before = failures;
    enemy_set_random(read_canned, read_canned);
    int stuck = 0;
    for (int start = 0; start < 0x40; start++) {
        for (int target = 0; target < 0x40; target++) {
            Object o = alien(u8(start), u8(target));
            canned = u8(target);           /* re-picks land on the same target */
            int steps = 0;
            while (o.dir != target && steps < 64) {
                enemy_turn_towards_target(o);
                steps++;
            }
            if (o.dir != target) stuck++;
        }
    }
    check(stuck == 0, "%d pairs never reached their target\n", stuck);
    report("steering_converges", before, "4096 pairs, <=64 steps ok");
}

/* Against an edge the alien must aim AWAY, not re-roll into the wall. */
static void test_margins_aim_inward() {
    const int before = failures;
    enemy_set_random(read_canned, read_canned);
    canned = 0;

    struct Case { int x, y; const char *edge; };
    const Case cases[] = {
        {   4,  60, "left"  },
        { 236,  60, "right" },
        { 120,   4, "top"   },
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        Object o = alien(0x00, 0x00);
        o.x_coord = u8(cases[i].x);
        o.y_coord = u8(cases[i].y);
        const unsigned long before_margin = enemy_margin_repicks;
        enemy_target_away_from_margins(o);
        check(enemy_margin_repicks > before_margin,
              "%s edge did not take the margin path\n", cases[i].edge);
        check(o.bonus_applied <= 0x3F, "%s edge produced target %02X\n",
              cases[i].edge, o.bonus_applied);
    }

    /* WHICH angle, not just "an angle in range".
     *
     * The checks above assert that the margin path was TAKEN and that
     * the result is 6-bit. Neither says where the alien is now aimed, so
     * swapping the two left-edge angles ($08 and $00) survived — the
     * alien would turn the wrong way at an edge, which is the exact
     * thing this routine exists to prevent.
     *
     * These are the original's escape angles, a value table rather than
     * a derived property: LAA7D picks by edge and by whether the alien
     * is in the upper part of the field. Pinned the way the bat
     * deflection table is. */
    struct Angle { int x, y; u8 want; const char *what; };
    const Angle angles[] = {
        {   4,   4, 0x08, "left edge, upper"  },
        {   4,  60, 0x00, "left edge, lower"  },
        { 236,   4, 0x38, "right edge, upper" },
        { 236,  60, 0x20, "right edge, lower" },
        {  40,   4, 0x08, "top edge, left half"  },
        { 200,   4, 0x38, "top edge, right half" },
    };
    for (unsigned i = 0; i < sizeof(angles) / sizeof(angles[0]); i++) {
        Object o = alien(0x00, 0x00);
        o.x_coord = u8(angles[i].x);
        o.y_coord = u8(angles[i].y);
        enemy_target_away_from_margins(o);
        check(o.bonus_applied == angles[i].want,
              "%s aimed at %02X, expected %02X\n",
              angles[i].what, o.bonus_applied, angles[i].want);
    }

    /* The thresholds are inclusive, and only a bracketing pair shows it.
     * x <= 8 is the left margin, so x=8 takes it and x=9 does not — the
     * cases above sit at x=4 and pass either way. */
    canned = 0x2A;
    Object at8 = alien(0x00, 0x00);
    at8.x_coord = 8; at8.y_coord = 60;
    enemy_target_away_from_margins(at8);
    check(at8.bonus_applied == 0x00,
          "x=8 is inside the left margin but aimed at %02X\n",
          at8.bonus_applied);

    Object at9 = alien(0x00, 0x00);
    at9.x_coord = 9; at9.y_coord = 60;
    enemy_target_away_from_margins(at9);
    check(at9.bonus_applied == 0x2A,
          "x=9 is outside the left margin but did not take the random "
          "target (got %02X)\n", at9.bonus_applied);

    /* Well inside the field, it should just pick at random. */
    Object mid = alien(0x00, 0x00);
    canned = 0x2A;
    enemy_target_away_from_margins(mid);
    check(mid.bonus_applied == 0x2A,
          "an alien in open space did not take the random target (%02X)\n",
          mid.bonus_applied);
    report("margins_aim_inward", before, "6 angles + threshold ok");
}

/* Targets are 6-bit; a stray high bit would steer toward an angle that
 * does not exist. */
static void test_targets_stay_six_bit() {
    const int before = failures;
    enemy_set_random(read_canned, read_canned);
    int bad = 0;
    for (int r = 0; r < 256; r++) {
        canned = u8(r);
        Object o = alien(0x00, 0x00);
        enemy_pick_new_target(o);
        if (o.bonus_applied > 0x3F) bad++;
    }
    check(bad == 0, "%d random values produced an out-of-range target\n", bad);
    report("targets_stay_six_bit", before, "256 random values    ok");
}

/* Arrival reads the CURRENT number; picking SAMPLES. Feeding the two
 * sources different values proves the paths are not crossed — the
 * distinction notes/rng-model.md says desynchronises the game if lost. */
static u8 current_val = 0x11;
static u8 sample_val  = 0x22;
static u8 read_current_src() { return current_val; }
static u8 read_sample_src()  { return sample_val; }

static void test_rng_sources_are_not_crossed() {
    const int before = failures;
    enemy_set_random(read_current_src, read_sample_src);

    /* Arrival (dir == target) re-picks via the CURRENT read. */
    Object arrived = alien(0x10, 0x10);
    enemy_turn_towards_target(arrived);
    check(arrived.bonus_applied == (current_val & 0x3F),
          "arrival used %02X, expected the current read %02X\n",
          arrived.bonus_applied, current_val & 0x3F);

    /* Explicit target-picking goes through the SAMPLER. */
    Object picking = alien(0x00, 0x00);
    enemy_pick_new_target(picking);
    check(picking.bonus_applied == (sample_val & 0x3F),
          "picking used %02X, expected the sampled read %02X\n",
          picking.bonus_applied, sample_val & 0x3F);
    report("rng_sources_not_crossed", before, "current vs sample    ok");
}

/* --- the brick-hit walk (LAA44) ------------------------------------
 *
 * The QEMU gates cannot reach this at all yet: nothing sets the target,
 * so the mode is unreachable in-game until the hit detection lands.
 * These drive it directly, over every offset that fits the playfield. */

static void test_home_walk_converges() {
    const int before = failures;
    int not_cleared = 0, overshot = 0, too_slow = 0;

    for (int tx = 0x10; tx < 0xE0; tx += 7) {
        for (int ty = 8; ty < 0xB0; ty += 11) {
            for (int dx = -20; dx <= 20; dx += 5) {
                if (tx + dx < 0) continue;   /* off the left edge */
                Object o = alien(0, 0);
                o.x_coord = u8(tx + dx);
                o.y_coord = u8(ty - 6);
                EnemyHomeTarget t = { u8(tx), u8(ty) };

                /* Bound: |dx| when x is the longer axis, else |dy| + 1.
                 * Allow a little slack and check the exact shape below. */
                const int budget = 64;
                int steps = 0;
                for (; steps < budget && t.y != 0; steps++) {
                    const int px = o.x_coord, py = o.y_coord;
                    enemy_home_step(o, t);
                    if (t.y == 0) break;          /* arrived and cleared */
                    /* Never move away from, or past, the target. */
                    if (abs(int(o.x_coord) - tx) > abs(px - tx)) overshot++;
                    if (abs(int(o.y_coord) - ty) > abs(py - ty)) overshot++;
                    /* At most one pixel per axis per step. */
                    if (abs(int(o.x_coord) - px) > 1) overshot++;
                    if (abs(int(o.y_coord) - py) > 1) overshot++;
                }
                if (t.y != 0) { not_cleared++; continue; }
                if (o.x_coord != tx || o.y_coord != ty) not_cleared++;
                const int want = (abs(dx) > 6) ? abs(dx) : 7;
                if (steps + 1 != want) too_slow++;
            }
        }
    }
    check(overshot == 0, "%d steps moved away from, past, or more than "
                         "one pixel toward the target\n", overshot);
    check(not_cleared == 0, "%d walks did not land on the target and "
                            "clear the word\n", not_cleared);
    check(too_slow == 0, "%d walks took the wrong number of steps "
                         "(expect max(|dx|,|dy|), +1 when y is not "
                         "shorter than x)\n", too_slow);
    report("home_walk_converges", before, "1px/axis, lands, clears");
}

/* The ONE clamp LAA44 has, and it is written back to the shared word. */
static void test_home_target_x_clamped_left() {
    const int before = failures;
    Object o = alien(0, 0);
    o.x_coord = 0x40;
    o.y_coord = 0x30;
    EnemyHomeTarget t = { 0x02, 0x30 };
    enemy_home_step(o, t);
    check(t.x == 0x10, "target x %02X was not clamped to $10\n", t.x);
    check(o.x_coord == 0x3F, "alien x %02X did not step toward the "
                             "clamped target\n", o.x_coord);

    /* No matching clamp on the right or on y — asserting the absence
     * stops someone "fixing" the asymmetry back out of parity. */
    EnemyHomeTarget high = { 0xF0, 0xC0 };
    Object far_ = alien(0, 0);
    far_.x_coord = 0x10;
    far_.y_coord = 0x10;
    enemy_home_step(far_, high);
    check(high.x == 0xF0 && high.y == 0xC0,
          "a high target was clamped to %02X,%02X; LAA44 clamps only "
          "the low x\n", high.x, high.y);
    report("home_target_x_clamped_left", before, "low x only, written back");
}

int main() {
    printf("enemies tests\n");
    test_turns_the_shorter_way();
    test_steering_converges();
    test_margins_aim_inward();
    test_targets_stay_six_bit();
    test_rng_sources_are_not_crossed();
    test_home_walk_converges();
    test_home_target_x_clamped_left();
    printf("\n%s\n", failures ? "FAILED" : "7 tests, 0 failed");
    return failures ? 1 : 0;
}
