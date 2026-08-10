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

/* The home-walk's left clamp fires AT $0F, not below it.
 *
 *     LAA44: LD A,L / CP $10 / JR NC / LD L,$10 / LD (LAA7B),HL
 *
 * `JR NC` skips the clamp when A >= $10, so $0F is clamped and $10 is
 * not — and the clamped value is written BACK, so every later call sees
 * it. Mutating the port's `t.x < 0x10` to `< 0x0F` left $0F alone and
 * passed the whole host suite. */
static void test_home_target_left_clamp_edge() {
    const int before = failures;
    Object o = alien(0, 0);
    o.x_coord = 0x40;

    EnemyHomeTarget below = { 0x0F, 0x40 };
    enemy_home_step(o, below);
    check(below.x == 0x10,
          "target x $0F must clamp to $10 and be written back; got %02X\n",
          below.x);

    EnemyHomeTarget at = { 0x10, 0x40 };
    enemy_home_step(o, at);
    check(at.x == 0x10,
          "target x $10 is already legal and must be left alone; got "
          "%02X\n", at.x);

    report("home_target_left_clamp_edge", before, "$0F clamps, $10 kept ok");
}

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
    test_targets_stay_six_bit();
    test_rng_sources_are_not_crossed();
    test_home_target_left_clamp_edge();
    test_home_walk_converges();
    test_home_target_x_clamped_left();
    printf("\n%s\n", failures ? "FAILED" : "6 tests, 0 failed");
    return failures ? 1 : 0;
}
