/* Host-side tests for src/scoring.cpp — extra lives from score
 * milestones.
 *
 * The rule is easy to get subtly wrong in ways no screenshot shows: an
 * award that fires twice for one threshold, or one that is missed when a
 * single jump crosses two. Both change how long a game lasts without
 * changing a single pixel. */

#include <stdarg.h>
#include <stdio.h>

#include "../src/scoring.cpp"

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

/* Thresholds must ascend — a flat or descending pair would award two
 * lives at one score. */
static void test_thresholds_ascend() {
    const int before = failures;
    for (int i = 1; i < LIVE_ADD_COUNT; i++)
        check(live_add_thresholds[i] > live_add_thresholds[i - 1],
              "threshold %d (%lu) does not exceed %d (%lu)\n",
              i, live_add_thresholds[i], i - 1, live_add_thresholds[i - 1]);
    report("thresholds_ascend", before, "8 thresholds         ok");
}

/* Playing a whole game one point at a time must award exactly eight
 * lives, one per threshold, each at the right score. */
static void test_each_threshold_awards_once() {
    const int before = failures;
    int awarded = 0;
    int awards_at[LIVE_ADD_COUNT];
    for (int i = 0; i < LIVE_ADD_COUNT; i++) awards_at[i] = -1;

    for (unsigned long score = 0; score <= 800000UL; score += 250) {
        const int earned = lives_earned(score, awarded);
        for (int e = 0; e < earned; e++) {
            if (awarded < LIVE_ADD_COUNT) awards_at[awarded] = int(score);
            awarded++;
        }
    }
    check(awarded == LIVE_ADD_COUNT, "awarded %d lives, expected %d\n",
          awarded, int(LIVE_ADD_COUNT));
    for (int i = 0; i < LIVE_ADD_COUNT; i++)
        check(awards_at[i] >= int(live_add_thresholds[i])
              && awards_at[i] < int(live_add_thresholds[i]) + 250,
              "threshold %d awarded at %d, expected just past %lu\n",
              i, awards_at[i], live_add_thresholds[i]);
    report("each_threshold_awards_once", before, "0..800k in 250s      ok");
}

/* Asking again without awarding must give the same answer — the count of
 * awards already made is what makes it idempotent, not a side effect. */
static void test_query_is_idempotent() {
    const int before = failures;
    int mismatches = 0;
    for (unsigned long score = 0; score <= 800000UL; score += 1000)
        for (int awarded = 0; awarded <= LIVE_ADD_COUNT; awarded++)
            if (lives_earned(score, awarded) != lives_earned(score, awarded))
                mismatches++;
    check(mismatches == 0, "%d queries were not repeatable\n", mismatches);

    /* Once every threshold is spent, nothing more is ever earned. */
    for (unsigned long score = 0; score <= 2000000UL; score += 10000)
        check(lives_earned(score, LIVE_ADD_COUNT) == 0,
              "score %lu earned a life with all thresholds spent\n", score);
    report("query_is_idempotent", before, "repeat + exhausted   ok");
}

/* A single jump crossing two thresholds must award two lives — scoring
 * is unbounded, so a big bonus can leap a gap. */
static void test_multi_threshold_jump() {
    const int before = failures;
    check(lives_earned(60000UL, 0) == 2,
          "a jump straight to 60000 awarded %d lives, expected 2\n",
          lives_earned(60000UL, 0));
    check(lives_earned(750000UL, 0) == LIVE_ADD_COUNT,
          "a jump to the top awarded %d, expected %d\n",
          lives_earned(750000UL, 0), int(LIVE_ADD_COUNT));
    check(lives_earned(29999UL, 0) == 0, "29999 should earn nothing\n");
    check(lives_earned(30000UL, 0) == 1, "30000 should earn exactly one\n");
    report("multi_threshold_jump", before, "gap jumps + edges    ok");
}

int main() {
    printf("scoring tests\n");
    test_thresholds_ascend();
    test_each_threshold_awards_once();
    test_query_is_idempotent();
    test_multi_threshold_jump();
    printf("\n%s\n", failures ? "FAILED" : "4 tests, 0 failed");
    return failures ? 1 : 0;
}
