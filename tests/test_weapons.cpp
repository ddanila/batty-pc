/* Host-side tests for src/weapons.cpp — bullet flight and impact.
 *
 * A bullet is a point travelling up 6 px a step, so it can pass BETWEEN
 * two tests if the step ever exceeds what the checks cover. The tunnel
 * test below is the one that matters: fired from anywhere below the
 * brick band, a bullet must never emerge above it with every brick still
 * standing. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../src/weapons.cpp"

/* weapons.cpp needs the slot table; it only reads the enemy. */
Object objects[N_OBJECTS];

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

static u8 cells[FIELD_ROWS * FIELD_COLS];
static void fill_field(bool standing) {
    memset(cells, standing ? 0x01 : 0x80, sizeof(cells));
}

static Object no_enemy() {
    Object e;
    memset(&e, 0, sizeof(e));
    e.sprite_set = 0x80;            /* inactive */
    return e;
}

static void fire(int i, int x, int y) {
    bullet_active[i] = 1;
    bullet_frame[i] = 0;
    bullet_x[i] = x;
    bullet_y[i] = y;
}

/* A bullet fired below a full brick band must always hit something. */
static void test_bullet_cannot_tunnel() {
    const int before = failures;
    fill_field(true);
    const BrickField field(cells);
    const Object none = no_enemy();
    int escaped = 0;

    for (int x = FIELD_X0; x < FIELD_X0 + FIELD_COLS * BRICK_W_PX; x++) {
        for (int start_y = FIELD_Y_END; start_y < FIELD_Y_END + 40; start_y += 3) {
            bullets_clear();
            fire(0, x, start_y);
            bool struck = false;
            for (int step = 0; step < 64 && bullet_active[0]; step++) {
                const BulletHit h = bullet_advance(0, none, field);
                if (h.what == BulletHit::BRICK) { struck = true; break; }
                if (h.what == BulletHit::LEFT_SCREEN) break;
            }
            if (!struck) escaped++;
        }
    }
    check(escaped == 0, "%d bullets crossed a full brick band without hitting\n",
          escaped);
    report("bullet_cannot_tunnel", before, "240 x 14 launches    ok");
}

/* A reported brick hit must name a cell that is actually standing. */
static void test_hits_name_standing_cells() {
    const int before = failures;
    unsigned long seed = 99;
    const Object none = no_enemy();
    int bad = 0;
    for (int trial = 0; trial < 200; trial++) {
        fill_field(true);
        for (int k = 0; k < 70; k++) {
            seed = seed * 1103515245UL + 12345UL;
            cells[(seed >> 16) % (FIELD_ROWS * FIELD_COLS)] = 0x80;
        }
        const BrickField field(cells);
        for (int x = FIELD_X0; x < 240; x += 7) {
            bullets_clear();
            fire(0, x, FIELD_Y_END + 8);
            for (int step = 0; step < 40 && bullet_active[0]; step++) {
                const BulletHit h = bullet_advance(0, none, field);
                if (h.what != BulletHit::BRICK) continue;
                if (!field.standing(h.row, h.col)) bad++;
                break;
            }
        }
    }
    check(bad == 0, "%d hits named a destroyed or out-of-range cell\n", bad);
    report("hits_name_standing_cells", before, "200 random fields    ok");
}

/* Every impact leaves a blast; leaving the screen must not. */
static void test_blast_only_on_impact() {
    const int before = failures;
    fill_field(true);
    const BrickField field(cells);
    const Object none = no_enemy();

    bullets_clear();
    fire(0, 100, FIELD_Y_END + 4);
    while (bullet_active[0]) bullet_advance(0, none, field);
    check(bullet_blast_ticks[0] > 0, "a brick hit left no blast\n");

    /* Empty field: the bullet should fly off the top with no blast. */
    fill_field(false);
    const BrickField empty(cells);
    bullets_clear();
    fire(0, 100, FIELD_Y_END + 4);
    while (bullet_active[0]) bullet_advance(0, none, empty);
    check(bullet_blast_ticks[0] == 0, "flying off the screen left a blast\n");
    report("blast_only_on_impact", before, "hit vs fly-off       ok");
}

/* The blast sprite is byte-aligned, so its x snaps to the column grid. */
static void test_blast_snaps_to_column() {
    const int before = failures;
    fill_field(true);
    const BrickField field(cells);
    const Object none = no_enemy();
    int unaligned = 0;
    for (int x = FIELD_X0; x < 240; x++) {
        bullets_clear();
        fire(0, x, FIELD_Y_END + 4);
        while (bullet_active[0]) bullet_advance(0, none, field);
        if (bullet_blast_x[0] & 7) unaligned++;
    }
    check(unaligned == 0, "%d blasts landed off the 8 px column grid\n", unaligned);
    report("blast_snaps_to_column", before, "232 impact columns   ok");
}

/* An alien already exploding (sprite set $0A) is not a target again. */
static void test_exploding_alien_is_not_a_target() {
    const int before = failures;
    fill_field(false);
    const BrickField empty(cells);

    Object alien;
    memset(&alien, 0, sizeof(alien));
    alien.sprite_set = 0x03;
    alien.x_coord = 100; alien.y_coord = 80;
    alien.w_body_px = 16; alien.h_body_px = 12;

    bullets_clear();
    fire(0, 104, 96);
    BulletHit h = bullet_advance(0, alien, empty);
    check(h.what == BulletHit::ENEMY, "a live alien was not hit\n");

    alien.sprite_set = 0x0A;                 /* already exploding */
    bullets_clear();
    fire(0, 104, 96);
    h = bullet_advance(0, alien, empty);
    check(h.what != BulletHit::ENEMY, "an exploding alien was hit again\n");

    alien.sprite_set = 0x83;                 /* inactive */
    bullets_clear();
    fire(0, 104, 96);
    h = bullet_advance(0, alien, empty);
    check(h.what != BulletHit::ENEMY, "an inactive alien was hit\n");
    report("exploding_alien_not_target", before, "live/blast/inactive  ok");
}

/* Blasts must expire, and clearing must empty everything. */
static void test_blasts_expire() {
    const int before = failures;
    bullets_clear();
    bullet_blast_ticks[0] = BULLET_BLAST_FRAMES * BULLET_BLAST_TICKS_PER_FRAME;
    int ticks = 0;
    while (any_bullet_blast() && ticks < 100) { bullet_blasts_tick(); ticks++; }
    check(ticks == BULLET_BLAST_FRAMES * BULLET_BLAST_TICKS_PER_FRAME,
          "the blast lasted %d ticks, expected %d\n",
          ticks, BULLET_BLAST_FRAMES * BULLET_BLAST_TICKS_PER_FRAME);

    fire(0, 10, 10); fire(1, 20, 20);
    bullet_blast_ticks[1] = 5;
    bullets_clear();
    check(!any_bullet_active() && !any_bullet_blast(),
          "clearing left a bullet or blast behind\n");
    report("blasts_expire", before, "8 ticks + clear      ok");
}

/* Each bullet animates on ITS OWN index, advanced once per step. The
 * frame used to come from a static bumped inside the RENDERER, so it
 * depended on how many frames had taken the full redraw path and both
 * bullets always drew the same one (known-bugs #10). The original
 * indexes the $77E6 frame table by each object's own sprite_num. */
static void test_bullet_frame_is_per_bullet() {
    const int before = failures;
    Object no_enemy = {};
    no_enemy.sprite_set = 0;
    for (int i = 0; i < FIELD_ROWS * FIELD_COLS; i++) cells[i] = 0x80;
    const BrickField empty(cells);

    bullets_clear();
    fire(0, 40, 180);
    for (int n = 0; n < 3; n++) bullet_advance(0, no_enemy, empty);
    fire(1, 80, 180);

    check(bullet_frame[0] == 3, "bullet 0 advanced to frame %u, expected 3\n",
          unsigned(bullet_frame[0]));
    check(bullet_frame[1] == 0, "a freshly fired bullet started at frame %u, "
          "expected 0\n", unsigned(bullet_frame[1]));

    bullet_advance(1, no_enemy, empty);
    check(bullet_frame[0] == 3, "stepping bullet 1 moved bullet 0's frame to "
          "%u\n", unsigned(bullet_frame[0]));
    check(bullet_frame[1] == 1, "bullet 1 advanced to frame %u, expected 1\n",
          unsigned(bullet_frame[1]));
    report("bullet_frame_is_per_bullet", before, "independent indices  ok");
}

/* An inactive slot must not animate — otherwise the phase drifts while
 * nothing is in flight, which is the shape of the original bug. */
static void test_inactive_bullet_does_not_animate() {
    const int before = failures;
    Object no_enemy = {};
    no_enemy.sprite_set = 0;
    for (int i = 0; i < FIELD_ROWS * FIELD_COLS; i++) cells[i] = 0x80;
    const BrickField empty(cells);

    bullets_clear();
    check(bullet_frame[0] == 0,
          "bullets_clear left frame %u behind\n", unsigned(bullet_frame[0]));
    const u8 idle = bullet_frame[0];
    for (int n = 0; n < 5; n++) bullet_advance(0, no_enemy, empty);
    check(bullet_frame[0] == idle,
          "an inactive slot drifted from frame %u to %u\n",
          unsigned(idle), unsigned(bullet_frame[0]));
    report("inactive_bullet_does_not_animate", before, "clear + 5 steps     ok");
}

int main() {
    printf("weapons tests\n");
    test_bullet_cannot_tunnel();
    test_hits_name_standing_cells();
    test_blast_only_on_impact();
    test_blast_snaps_to_column();
    test_exploding_alien_is_not_a_target();
    test_blasts_expire();
    test_bullet_frame_is_per_bullet();
    test_inactive_bullet_does_not_animate();
    printf("\n%s\n", failures ? "FAILED" : "6 tests, 0 failed");
    return failures ? 1 : 0;
}
