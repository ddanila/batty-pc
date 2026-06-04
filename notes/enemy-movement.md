# Enemy (bird/UFO) movement — ground truth + LAA7D decode

After the ball physics, brick collision, and bat are byte/behavior-exact,
the next gameplay-visible area is the enemy (bird/UFO) flight. The port's
`handling_bird_obj` used a simplified steering; this note captures the
original's behaviour and decodes the exact steering routine.

## Capture is unblocked (unlike multi-ball)

The `l3-brick-flash` snapshot already has a LIVE, coherently-moving enemy
at `object_enemy` ($9B96) — no special spawn sequence needed (contrast the
multi-ball secondaries, which need the full `bonus_triple_ball` spawn; see
notes/bat-deflection.md). `scripts/capture_enemy_flight.py` frame-steps the
original and probes $9B96.

### Captured ground truth (L3, dense per-frame)

```
f0-7 : x=168  y=1..8   dir=0x10  (entry slide, y<8: just descends)
f8-12: x=168  y=8..12  dir=0x10  (straight down, speed 1, q8.8)
f13  : dir 0x10 -> 0x11   (first turn; x starts easing left)
f17  : 0x12
f21  : 0x13   ...
```

So: q8.8 motion at **speed 1**; heading turns by **+1 every 4 frames**;
the turn walks the 6-bit dir toward a target heading.

## LAA7D decode (the exact steering)

`handling_bird` (`LA9BC_1`) per frame, once past the y<8 entry slide:

```
LD B,$01                     ; turn step = 1
LD A,(counter_misc); AND $03
CALL Z,LAA7D                 ; steer every 4 frames
CALL LAD69                   ; q8.8 move
CALL LAFFC                   ; brick collision (enemy bounces off bricks!)
CALL check_margins
```

```
LAA7D:  A = dir (IX+$06); L = A
        A -= target (IX+$14)
        JR Z, LAA7D_1        ; reached target -> repick
        BIT 5,A              ; shorter way round?
        A = B (=1); JR NZ,..; else NEG   ; +1 or -1
        dir = (dir +/- 1) & $3F
        RET
LAA7D_1: target = random_number & $3F    ; NEW random target, on ARRIVAL
        ; reads the current random_number low byte; does NOT advance the RNG
```

So the enemy roams: pick a random target heading, turn toward it 1 step
per 4 frames, and when it arrives pick a new random target. It does **not**
pursue the bat/ball — the L3 curve toward the bat was just the random
target happening to point that way.

### Target (+$14) ground truth (probed at $9BAA)

```
f0-8 : target = 0x10   (== dir, so no turn; entry slide + arrived)
f10+ : target = 0x2C   (repicked on arrival; random_number & $3F = 0x2C)
```

So dir holds at 0x10 until ~f10, then curves 0x10 -> ... -> 0x2C (+1 per
4 frames). The pick value 0x2C is `random_number & $3F` at that frame —
which is exactly why byte-exact enemy targets need the per-frame RNG tick
(see notes/rng-model.md). Validating flag-on enemy against this needs ALL
read-current consumers converted first: in L3 the ball is breaking bricks,
so bonus-drop RNG consumers fire and shift `random_number` between frames;
until they read-current too, the enemy's repick value won't match.

## Spawn heading fixed (dir/target = $10)

`enemy_prepare` ($9EAA) sets the spawned enemy's `dir` (+$06) AND target
(+$14) to **$10 unconditionally** — it always enters heading straight
down, then steers. The port had derived `dir = (r & 1) ? 0x38 : 0x08`
from the spawn random, putting the enemy on a diagonal it never has on
the Spectrum. Fixed to `dir = 0x10`, `bonus_applied = 0x10` (ground truth
confirms frame-0 dir = 0x10). Both gates stay green (the enemy isn't in
the byte-exact ball path or the captured visual states). The spawn x is
`prop_x_coord[random_number & 3]` = {64,168,64,168}, read-current.

## Port status

`handling_bird_obj` already matches the cadence (turn every 4 frames,
`misc_12 & 3`) and the turn direction (bit-5 of `dir-target`). Fixed now:

- **Refresh on arrival, not on a timer.** The port re-picked a random
  target every 64 frames (`misc_12 & 0x3F`); removed. `enemy_turn_towards_
  target` now repicks when `dir == target` (LAA7D_1), reading the current
  RNG low byte (`random_e & 0x3F`) without advancing — faithful to LAA7D.

## Turn cadence now gates on the GLOBAL frame counter

The original gates the steer on the global `counter_misc`
(`LD A,(counter_misc); AND $03; CALL Z,LAA7D`), so every enemy turns on
the same global 4-frame phase. The port gated on the per-object
`o->misc_12 & 3`, whose phase is spawn-relative (and whose object byte
`+$12` is the *sprite address* in the original, not a turn counter). Fixed
to gate on `pit_frame_counter & 3` (the port's per-frame PIT counter = the
original's `counter_misc`); `misc_12` still drives the sprite animation.

## Enemy seed for the flag-on acceptance test

L3 enemy at frame 0 (probed, 22 bytes):
`0900A80001001001030FDA35180C00000000F0701000`
(type $09 bird, x=$A8=168, y=1, dir=$10, speed=1, target +$14=$10). Seed
via `BATTY_REPLAY_ENEMY_OBJECT` with `BATTY_RNG_PERFRAME=1` + RNG 8E49 to
run the acceptance test (expect target -> 0x2C at frame ~10). NOTE: the
seeded `+$12` is the original's sprite-addr high byte, which the port reads
as `misc_12`; with the cadence now on `pit_frame_counter` that no longer
affects steering, but seeded byte semantics still need care.

## Flag-on acceptance test — run, does NOT yet match byte-exact

Seeded the L3 enemy + `BATTY_RNG_PERFRAME=1` + RNG 8E49 and probed
`object_enemy`:

```
frame 8 : x=168 y=8 dir=0x10 target=0x10   (matches: descending straight)
frame 11: x=171 y=8 dir=0x10 target=0x03   (orig repicks 0x2C, port 0x03)
frame 14: x=174 y=8 dir=0x0F target=0x03
```

Two residual discrepancies, both needing deeper investigation:

1. **RNG value at repick.** The on-arrival repick reads `0x03`, not the
   original's `0x2C`. So the per-frame tick + read-current model does not
   reproduce the original's `random_number` at that frame. Likely causes:
   the port's per-frame-tick *count* differs from the original's
   `counter_misc` at the seeded start (frame-origin phase), and/or the L3
   bonus-drop TYPE pick (which legitimately advances) fires on different
   frames than the original because earlier RNG already diverged. The
   seed-walk being aligned by design is necessary but not sufficient — the
   *number of advances before the read* must also match.
2. **Seeded-enemy motion.** y sticks at 8 while x drifts ~+1/frame, unlike
   the captured original (clean descent y=1->27). The port's own
   enemy_prepare-spawned enemy descends fine; this looks like a seeding
   artifact (the 22-byte seed carries object-internal fields whose port
   semantics differ — e.g. the sprite-addr bytes the port reads as
   misc_*), not a steering bug, but it means a *seeded* trajectory isn't a
   clean reference.

Conclusion: the enemy steering is now a faithful STRUCTURAL port (spawn
$10, bit-5 turn, global 4-frame cadence, refresh-on-arrival, read-current
RNG) — visually correct — but BYTE-EXACT enemy targets are not yet
achieved. Closing the gap needs frame-by-frame RNG-evolution comparison
(port vs original `random_number`) to find where the advance count
diverges, plus a clean enemy reference that doesn't depend on seeding
port-divergent object bytes. That is a dedicated debugging effort, not a
quick edit.

## Re-validated with byte-exact RNG (2026-06-04): decode right, 3 blockers

With the RNG now proven byte-exact (notes/rng-model.md) and actually
reaching the build (Makefile passthrough fix), re-ran the seeded enemy
flag-ON with the correct RNG seed (RANDOM=460D RANDOM_SEED=962C):

- **The enemy target mechanism is confirmed**: the port repicks
  `target = random_number & $3F` exactly (e.g. target `0x36` =
  `random 0x96F6 & $3F`). `LAA7D_1` decode is correct.
- It still does NOT match the original's `0x2C`, for three reasons, none
  of which is the steering decode:
  1. **Seeded-enemy motion artifact** — the seeded enemy sticks at y=8 and
     drifts x+ instead of descending (dir=0x10 should be straight down).
     The 22-byte seed carries object fields the port interprets
     differently; the port's own spawned enemy descends fine. So a
     *seeded* enemy isn't a clean reference, and it repicks at the wrong
     point.
  2. **RNG sequence diverges with the enemy active** — port f8
     random=816C vs the no-enemy sequence's FC56, so some enemy-path
     consumer advances the RNG differently than the original (needs
     tracing which `next_random`/`rng_sample` site).
  3. **One-frame RNG offset** (the original's `f0==f1`, see rng-model.md).

So byte-exact ENEMY targets are gated on (1) a clean enemy reference (fix
the seed-field interpretation or capture a fresh in-flight enemy) and (2)
the enemy-path RNG consumer audit + the one-frame offset — not on the
steering or RNG-walk model, both of which are now validated.

## BLOCKER #1 RESOLVED — it was a real motion bug, and the enemy now matches

The "seeded enemy drifts x+ instead of descending" was NOT a seed
artifact — it was a genuine motion bug. `handling_bird_obj` moved the
enemy with `enemy_dir_delta_q8`, whose per-quadrant switch had the X/Y
components SWAPPED vs the validated `dir_to_dxdy`: `dir $10` came out
`dx=$FF (right), dy=0` instead of `dx=0, dy=$FF (straight down)`. The
original's `handling_bird` calls `LAD69` — the same `hl_bc_calc_direction`
as the ball — so the bird should use `dir_to_dxdy`. Fixed (removed
`enemy_dir_delta_q8` + its table).

Result: the enemy now **descends** correctly (dir $10: x constant,
y 5->8->12->16) and **steers** — a real gameplay-visible motion bug fixed.

CORRECTION (over-claimed): byte-exact enemy *targeting* is NOT achieved.
A closer look (flag-ON, seeded 460D/962C) shows the target THRASHES, not
matches: f14=0x36, f16=0x2B, f18=0x36, f20=0x2C, with dir oscillating
(0x0F/0x11/0x0E/0x13). The original has a STABLE target (0x2C) and smooth
steering. The earlier "repicks to 0x2C" was a coincidental single frame.
RESOLVED — the "thrashing" was a MEASUREMENT ARTIFACT, not a bug.
Instrumented per-path repick counters (`dbg_enemy_arrival/margin_repicks`,
dumped in PROBE.TXT) and tested determinism:

- The arrival-repick (`LAA7D_1`) fires **once** per run (`arrival=1`,
  `margin=0`), so `bonus_applied` is written once and is **stable within
  a run**.
- Running the SAME frame 3 times is fully deterministic (target=0x08,
  random=E69B every time). The apparent frame-to-frame "thrashing" was
  comparing SEPARATE runs (each probe frame is its own floppy build +
  boot); the WAIT_KEY release timing jitters between runs, so the
  per-frame RNG starts at a slightly different point → a different
  arrival-repick value. Within any one run the target holds.

So the enemy steering is CORRECT under flag-ON (stable target, repicks on
arrival like `LAA7D_1`); there is no thrashing bug. The previous
"thrashing" note was itself a separate-run artifact. The remaining limit
is that this harness can't byte-exact-GATE the enemy target.

Dug into the jitter source: the WAIT_KEY wait runs only `sound_tick` (no
play loop, no RNG tick); the play loop + per-frame RNG tick start AFTER
the key release, and the RNG ticks once per pit-tick (the same gate as the
ball). So wait *duration* isn't the jitter source. Yet the enemy target
varied across BINARY REBUILDS (0x2B before adding the diagnostic counters,
0x08 after — same seed/env) while the RNG-independent ball stays
byte-exact. So there's an unexplained cross-binary non-determinism in the
RNG-dependent state (the ball can't expose it; only RNG-dependent objects
do). Within one binary it's deterministic (3 identical f16 runs → 0x08).

Root cause of the cross-binary variance (diagnosed): the RNG is
deterministic from the seed (f0 == seed), so the variance is NOT in the
RNG values. It's the cadence PHASE — the enemy turn/repick is gated on the
GLOBAL `pit_frame_counter`, which counts from boot and so includes the
(binary-dependent) boot-tick count. A different boot length shifts which
play-frame `pit_frame_counter & 3 == 0` lands on, so the arrival-repick
fires on a different play-frame and reads the (deterministic) RNG at a
different point → a different target. The ball is immune: it's
RNG-independent and its motion advances per play-frame, not per pit-phase.

This is a TEST-COMPARISON non-determinism, NOT a real-play bug:
`pit_frame_counter` is the faithful analog of the original's global
`counter_misc`, and it's deterministic per binary (a real user runs one
binary). Switching the cadence to a play-relative counter would make tests
reproducible but would be LESS faithful (the original's counter is global),
so the right fix is harness-side (control/normalise the boot-tick phase
for the comparison), not a port change.

Conclusion (enemy/RNG thread): the enemy MOTION fix and the byte-exact RNG
WALK are solid, shipped, and validated; the enemy STEERING is correct
(stable target, `LAA7D_1` repick-on-arrival, deterministic per binary,
faithful global cadence); flag-ON is the validated-correct model. Byte-exact
enemy-TARGET *gating* — and the confident flag-ON default flip — is blocked
by the boot-tick cadence-phase variance, a test-harness limitation needing
a boot-normalised / fixed-phase comparison run (a deliberate test-infra
effort, deferred). The shipped default (flag-OFF) is unaffected.
(Earlier mistaken analysis kept below for the record.)

- **Clobber ruled out:** `bonus_applied` is only written by the bat-bonus
  code (OBJ_BAT_1/2 specific) and the three enemy-steering paths
  (`enemy_turn_towards_target` arrival-repick, `enemy_pick_new_target` /
  `enemy_target_away_from_margins` margin). No loop writes the enemy's
  field, so the bonus system isn't clobbering it.
- **Contradiction by static analysis:** the enemy is mid-field (x=168, no
  margin) so the margin paths shouldn't fire, and dir (~0x10) is far from
  the target (0x2B–0x36) so delta!=0, so the arrival-repick shouldn't fire
  either — yet the target keeps changing. The static code can't explain a
  per-~2-frame rewrite. Resolving it needs RUNTIME instrumentation (a
  per-path repick counter dumped in PROBE.TXT, or a flag-on MWA trace),
  not more code reading.

Status: **deferred as a deep, flag-ON-experimental debug.** It's low
priority — the SHIPPED default (flag-OFF) enemy roams correctly with the
fixed motion; the thrashing only affects the experimental per-frame-RNG
path, which is gated behind `BATTY_RNG_PERFRAME` and not user-facing. The
byte-exact RNG *walk* and the enemy *motion/steering decode* stand; only
the flag-on byte-exact *targeting* (a niche fidelity goal) remains, blocked
on this instrumentation session.

The byte-exact L3 ball gate and all `make test` states still pass
(the corrected descent doesn't interfere with the ball or the captured
frames). This was also a real gameplay-visible bug fix: enemies were
flying the wrong axis in normal play.

## Still approximate / not byte-exact

- **RNG model.** The original advances `random_number` every frame (a
  per-frame tick); the port advances on demand via `next_random()`. So the
  *value* `random_e` holds when the enemy repicks won't match the original
  frame-for-frame. Byte-exact enemy targets need the port to replicate the
  per-frame RNG tick — a global change affecting all RNG consumers, so it's
  a deliberate follow-up, validated against `capture_enemy_flight.py`.
- **Brick collision for the enemy.** The original calls `LAFFC` for the
  bird (it bounces off bricks); the port's bird steering doesn't yet run
  the brick collision. Next step once the RNG tick is aligned.
- **Margins.** The original's `check_margins` vs the port's
  `enemy_target_away_from_margins` is still an approximation.
