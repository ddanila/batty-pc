/* Host-side tests for src/objects.cpp — the descriptor and its helpers.
 *
 * The descriptor's SIZE and FIELD ORDER are load-bearing: the original
 * walks the slot table by adding $16 to IX, and the replay harness seeds
 * a slot by memcpy-ing 22 raw bytes over it. A field inserted in the
 * middle would silently reinterpret every seeded replay.
 *
 * The reflection helper is worth pinning because its two axes are easy to
 * swap — doing so leaves a ball pinned against a wall, juggling its other
 * component forever, which is a bug that has actually shipped here. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../src/objects.cpp"
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

static int tests_run = 0;

static void report(const char *name, int before, const char *detail) {
    tests_run++;
    printf("  %-28s %s\n", name, failures > before ? "FAIL" : detail);
}

/* Offset of each field, as the disasm names them. A mismatch here means
 * seeded replays and the original's IX arithmetic disagree. */
static void test_descriptor_layout() {
    const int before = failures;
    Object o;
    const char *base = (const char *)&o;
    struct Field { const char *name; const char *at; int want; };
    const Field fields[] = {
        { "sprite_set",    (const char *)&o.sprite_set,    0x00 },
        { "sprite_num",    (const char *)&o.sprite_num,    0x01 },
        { "x_coord",       (const char *)&o.x_coord,       0x02 },
        { "y_coord",       (const char *)&o.y_coord,       0x04 },
        { "dir",           (const char *)&o.dir,           0x06 },
        { "speed",         (const char *)&o.speed,         0x07 },
        { "buf_addr_hi",   (const char *)&o.buf_addr_hi,   0x0A },
        { "w_body_px",     (const char *)&o.w_body_px,     0x0C },
        { "h_body_px",     (const char *)&o.h_body_px,     0x0D },
        { "prev_x",        (const char *)&o.prev_x,        0x0E },
        { "bonus_applied", (const char *)&o.bonus_applied, 0x14 },
        { "bat_props",     (const char *)&o.bat_props,     0x15 },
    };
    for (unsigned i = 0; i < sizeof(fields) / sizeof(fields[0]); i++)
        check(fields[i].at - base == fields[i].want,
              "%s is at +%02X, the original has it at +%02X\n",
              fields[i].name, int(fields[i].at - base), fields[i].want);
    check(sizeof(Object) == 0x16, "descriptor is %u bytes, must be 22\n",
          unsigned(sizeof(Object)));
    report("descriptor_layout", before, "12 fields + stride   ok");
}

/* The slot order is what call_hl_for_all_obj walks. */
static void test_slot_order() {
    const int before = failures;
    check(OBJ_BALL_1 == 0 && OBJ_BALL_2 == 1 && OBJ_BALL_3 == 2,
          "the three balls must lead the table\n");
    check(OBJ_BAT_1 == 6, "the bat moved from slot 6 to %d\n", int(OBJ_BAT_1));
    check(N_OBJECTS == 11, "the table has %d slots, the original has 11\n",
          int(N_OBJECTS));
    report("slot_order", before, "11 slots             ok");
}

/* The animation reaches its TOP frame before wrapping.
 *
 *     CP E / JR NC,LAAD2_0        ; A = range high nibble, E = num + 1
 *     LD A,D / AND $0F / LD E,A   ; else wrap to the low nibble
 *
 * `JR NC` skips the wrap when high >= E, so the wrap happens only when
 * high < E — the port's `if (((d >> 4) & 0x0F) < e)`. With `<=` the
 * sprite wraps one frame early and the top frame is never shown.
 *
 * Equality is not a corner here: the counter walks up to the range high
 * every cycle, so `high == e` happens once per loop of every animation
 * in the game. It still survived the host suite. */
static void test_anim_reaches_the_top_frame() {
    const int before = failures;
    Object o;
    memset(&o, 0, sizeof(o));
    o.misc_12 = 0;              /* cadence spent: this call animates */
    o.misc_13 = 0x53;           /* range high 5, low 3 */
    o.sprite_num = 4;           /* so e = 5, and high == e */

    object_step_animation(o);
    check(o.sprite_num == 5,
          "the frame equal to the range high must be SHOWN, not wrapped: "
          "got %u, expected 5 (a `<=` wrap gives 3)\n",
          unsigned(o.sprite_num));

    /* One past it does wrap, so the assertion above is about the
     * boundary and not about wrapping being broken. */
    o.misc_12 = 0;
    o.sprite_num = 5;           /* e = 6, and high 5 < 6 */
    object_step_animation(o);
    check(o.sprite_num == 3,
          "past the range high the frame must wrap to the low nibble: "
          "got %u, expected 3\n", unsigned(o.sprite_num));

    report("anim_reaches_top_frame", before, "high == e shows      ok");
}

/* LAAD2's cadence counter spends a whole call at exactly $40.
 *
 *     LAAD2: LD A,(IX+$12) / LD B,A / SUB $40 / JR NC,LAAD2_1
 *
 * `SUB $40` on $40 leaves 0 with no borrow, so `JR NC` takes the
 * subtract-and-return path: a counter of $40 is consumed, and the
 * sprite does NOT advance that call. Only $3F and below animate.
 *
 * That matters because misc_12 reaches $40 by ordinary counting — a
 * seed of $80 becomes $40 and then 0 — so this is a frame of animation,
 * not a corner. Mutating the port's `a >= 0x40` to `>` advances the
 * sprite a call early and passed the whole host suite. */
static void test_anim_cadence_consumes_exactly_40() {
    const int before = failures;
    Object o;
    memset(&o, 0, sizeof(o));
    o.sprite_num = 3;
    o.misc_13    = 0x50;      /* some range; unused on the early-out */

    o.misc_12 = 0x40;
    object_step_animation(o);
    check(o.misc_12 == 0x00,
          "a $40 counter must be spent down to 0, got %02X\n", o.misc_12);
    check(o.sprite_num == 3,
          "the sprite must NOT advance on the call that spends $40; "
          "it moved to %u\n", unsigned(o.sprite_num));

    /* And one below it does animate, so the assertion above is about
     * the boundary rather than about the function doing nothing. */
    o.misc_12 = 0x3F;
    object_step_animation(o);
    check(o.sprite_num != 3,
          "a $3F counter should have advanced the sprite; it stayed at "
          "%u\n", unsigned(o.sprite_num));

    report("anim_cadence_edge", before, "$40 spent, $3F steps ok");
}

/* Reflection must negate the intended axis, and only that axis. */
static void test_reflection_axes() {
    const int before = failures;
    /* dir $20 travels left; a side-wall bounce must stop it doing so. */
    Object o;
    memset(&o, 0, sizeof(o));
    o.dir = 0x20;
    object_reflect(o, true, false);
    check(o.dir != 0x20, "a horizontal bounce left dir $20 unchanged\n");

    /* Reflecting the same axis twice returns the original direction. */
    int bad = 0;
    for (int d = 0; d < 0x40; d++) {
        Object a;
        memset(&a, 0, sizeof(a));
        a.dir = u8(d);
        object_reflect(a, true, false);
        object_reflect(a, true, false);
        if (a.dir != u8(d)) bad++;
        a.dir = u8(d);
        object_reflect(a, false, true);
        object_reflect(a, false, true);
        if (a.dir != u8(d)) bad++;
    }
    check(bad == 0, "%d directions did not survive a double reflect\n", bad);

    /* Reflecting neither axis changes nothing. */
    Object still;
    memset(&still, 0, sizeof(still));
    still.dir = 0x2C;
    object_reflect(still, false, false);
    check(still.dir == 0x2C, "reflecting no axis changed dir to %02X\n", still.dir);
    report("reflection_axes", before, "64 directions        ok");
}


/* The VALUE, not just the shape.
 *
 * test_reflection_axes above checks that a reflect is an involution and
 * is not the identity. Both hold for the WRONG constant: changing the
 * `+ 1` in object_reflect to `+ 2` leaves every assertion there green.
 * That constant is not free — object_reflect is the original's
 * bounce_wall ($AC75) calling change_direction ($ACEE), and getting it
 * wrong once pinned the ball against a side wall juggling its dy
 * forever.
 *
 * physics.cpp implements the SAME original routine as
 * laffc_change_dir(dir, mask), used for brick faces. Tying the two
 * together pins the value without hardcoding a table, and makes it
 * impossible for the two ports of one routine to drift apart. */
static void test_reflection_matches_change_direction() {
    const int before = failures;
    int bad_x = 0, bad_y = 0;
    for (int d = 0; d < 0x40; d++) {
        Object a;
        memset(&a, 0, sizeof(a));
        a.dir = u8(d);
        object_reflect(a, true, false);
        if (a.dir != laffc_change_dir(u8(d), 0x1F)) bad_x++;

        memset(&a, 0, sizeof(a));
        a.dir = u8(d);
        object_reflect(a, false, true);
        if (a.dir != laffc_change_dir(u8(d), 0x3F)) bad_y++;
    }
    check(bad_x == 0, "%d of 64 horizontal reflects disagree with "
          "laffc_change_dir(dir, $1F)\n", bad_x);
    check(bad_y == 0, "%d of 64 vertical reflects disagree with "
          "laffc_change_dir(dir, $3F)\n", bad_y);

    /* One anchored value, so a change to BOTH implementations still
     * fails: dir $20 travels left, and the original turns it into $00. */
    Object o;
    memset(&o, 0, sizeof(o));
    o.dir = 0x20;
    object_reflect(o, true, false);
    check(o.dir == 0x00, "dir $20 bounced to $%02X, expected $00\n", o.dir);
    report("reflect_matches_change_dir", before, "64 dirs x 2 axes     ok");
}

/* The cached buffer offset must agree with the position it came from. */
static void test_buffer_offset() {
    const int before = failures;
    int bad = 0;
    for (int y = 0; y < 192; y += 3) {
        for (int x = 0; x < 256; x += 5) {
            Object o;
            memset(&o, 0, sizeof(o));
            o.x_coord = u8(x);
            o.y_coord = u8(y);
            object_update_buffer_offset(o);
            const unsigned got = (unsigned(o.buf_addr_hi) << 8) | o.buf_addr_lo;
            const unsigned want = unsigned(y) * 32u + unsigned(x) / 8u;
            if (got != want) bad++;
        }
    }
    check(bad == 0, "%d positions produced the wrong buffer offset\n", bad);
    report("buffer_offset", before, "3300 positions       ok");
}

/* Active/inactive is BIT7 of sprite_set, and toggling it must leave the
 * rest of the byte — the sprite set id — alone. */
static void test_active_flag() {
    const int before = failures;
    int bad = 0;
    for (int set = 0; set < 0x80; set++) {
        Object o;
        memset(&o, 0, sizeof(o));
        o.sprite_set = u8(set);
        if (!object_active(o)) bad++;
        object_deactivate(o);
        if (object_active(o) || (o.sprite_set & 0x7F) != set) bad++;
        object_activate(o);
        if (!object_active(o) || o.sprite_set != set) bad++;
    }
    check(bad == 0, "%d sprite-set values lost data across a toggle\n", bad);
    report("active_flag", before, "128 sprite sets      ok");
}

int main() {
    printf("objects tests\n");
    test_descriptor_layout();
    test_slot_order();
    test_anim_reaches_the_top_frame();
    test_anim_cadence_consumes_exactly_40();
    test_reflection_axes();
    test_reflection_matches_change_direction();
    test_buffer_offset();
    test_active_flag();
    printf("\n%d tests, %d failed\n", tests_run, failures);
    return failures ? 1 : 0;
}
