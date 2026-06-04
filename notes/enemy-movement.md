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

## Port status

`handling_bird_obj` already matches the cadence (turn every 4 frames,
`misc_12 & 3`) and the turn direction (bit-5 of `dir-target`). Fixed now:

- **Refresh on arrival, not on a timer.** The port re-picked a random
  target every 64 frames (`misc_12 & 0x3F`); removed. `enemy_turn_towards_
  target` now repicks when `dir == target` (LAA7D_1), reading the current
  RNG low byte (`random_e & 0x3F`) without advancing — faithful to LAA7D.

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
