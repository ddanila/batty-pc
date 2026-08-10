# Bat deflection — `LAB1F` ($AB1F)

How the bat reflects the ball, and the MAGNET catch. Ported literally and
validated against the Spectrum at 13 (direction, position) cases:
`make test-bat-deflection`, in `parity-check`.

## Reaching a ball-onto-bat state without a new snapshot

The L3 byte-exact ball orbits the upper field and never reaches the bat
(probed 750 frames: y stays 8-78). Poking a placeholder ball into the raw
snapshot HANGS the original — the 22-byte descriptor carries buffer addresses
and prev-x/y that must stay coherent.

The unblock: the `l3-brick-flash` descriptor is already coherent, so
*repositioning* it keeps it coherent. Change only x (+2), y (+4), dir (+6) and
prev-x/prev-y (+14/+15), drop it at y=0x96 just above the bat at y=0xAD
heading down, and the original computes the deflection itself.

`scripts/capture_bat_deflection.py` sweeps the start x, frame-steps across the
contact on the IM1 boundary `0x0038`, and reports the outgoing direction.
`replays/l3-bat-descend.json` is a single-shot variant.

**Use `$0038`, not `$BA83`, for anything touching the bat.** The main-loop top
is skipped whenever the game branches into the bat / ball-lost paths; the
interrupt vector fires once per 50 Hz frame regardless. It samples at a
different point in the frame, so a byte comparison needs the port aligned to
the same phase.

## The routine

    LAB1F:
      ball y (IX+$04) < $98 ?                -> RET
      ball (IX+$0F) >= $AA ?                 -> RET
      obj_compare vs object_bat_1            -> overlap? else (mode 2) bat_2, else RET
    LAB1F_0:
      RES 7,(IX+$12); copy the hitting bat's x bit 7 into it   ; the owner bit
      set_sound_bat_beat
      bat bonus (IY+$14) == $03 AND bat width $1C ? -> CATCH
      else -> LAB1F_4
    LAB1F_4:
      ball y = $A6                           ; snap onto the bat top
      (IX+$12) &= $80
      HL = LABEE if width == $1C else LABFC   ; threshold -> zone table
      offset = ball_x (IX+$02) + 3 - bat_x (IY+$02)
      walk (threshold, zone) pairs; first pair whose threshold > offset wins
      if zone bit 2 set: reflect, look up LAC0A, reflect AGAIN
      else:              look up LAC0A
    reflect (LAB1F_9):  dir = ((dir XOR $1F) + 1) & $3F
    lookup  (LAB1F_10): base = LAC0A + (zone & 3) * 6
                        index the incoming dir within {04,08,0C,14,18,1C}
                        dir = base[index]

`LABEE` (normal bat, width $1C): `(04,07) (08,06) (0C,05) (10,00) (14,01)
(18,02) (FF,03)`. `LABFC` (big bat): `(06,07) (0C,06) (12,05) (1A,00) (20,01)
(26,02) (FF,03)`.

`LAC0A`, indexed `[zone & 3][incoming dir]`:

    zone0: 3C 38 34 2C 28 24
    zone1: 3C 38 34 34 34 34
    zone2: 3C 38 38 34 38 38
    zone3: 3C 3C 38 38 3C 3C

The port is `bat_deflect_dir` in `src/physics.cpp`, over `zone_tbl_normal` /
`zone_tbl_big` and `deflect_tbl`, with `bat_reflect_dir` and `bat_dir_index`.
An enlarged bat (`extra_px != 0`) selects the big table. Unmatched incoming
dirs — pure vertical `$10`, or a non-multiple of 4, neither of which reaches
the bat in real play — fall back to a plain vertical reflect so the port cannot
hang.

## Why it has to be a LITERAL translation

Hand-tracing the decode for offset 21 predicts `0x2C`; the hardware gives
`0x38`. The bit-2 zone path falls through `LAB1F_8` into `LAB1F_9` *after*
`LAB1F_10` returns, so those zones reflect, look up, then reflect again — and
the exact contact pixel, hence the offset and the zone, is subpixel-sensitive.
Copy the opcode flow and validate against the captured table.

Ground truth, incoming dir 0x0C, bat x=116 width 28,
`offset = contact_x + 3 - 116`:

| start x | contact x | offset | out dir |
|--------:|----------:|-------:|:--------|
| 104 | 110 | -3 | 0x28 |
| 112 | 118 |  5 | 0x2C |
| 120 | 126 | 13 | 0x34 |
| 128 | 134 | 21 | 0x38 |
| 136 | 142 | 29 | 0x38 |
| 144 | 157 | 44 | ball misses the bat, lost |

Monotonic and physical: left of the bat deflects up-left, right deflects
up-right. The gate covers three incoming dirs across two `dir_to_dxdy`
quadrants and both threshold tables including the double reflect.

### An offset ON a zone boundary belongs to the zone ABOVE

    LAB1F_6: CP (HL) / JR C,LAB1F_7 / INC HL / INC HL / JR LAB1F_6

`JR C` leaves the walk only when `offset < boundary`, so it keeps advancing
while `offset >= boundary`. The port's
`while (i < 12 && u8(offset) >= zones[i]) i += 2;` matches.

**Nothing tested it.** Mutating `>=` to `>` passed the whole host suite,
because the captured cases are offsets -3, 5, 13, 21 and 29 and not one sits
on a boundary — a consequence of how the table was built, since offsets were
chosen to span the zones. **A table built to show that zones differ will not
test where one ends.** `bat_zone_boundary_above` asserts the property directly:
for each boundary b and each of the six incoming dirs, the deflection at b
equals the one at b+1 and differs from b-1 somewhere.

### The contact trigger is a HEIGHT test, and strict

The original fires when `obj_compare` reports Y overlap, which (`LAC22`:
`166 - ball_y` borrows) is exactly `ball_y >= 167`. The port had used the
ball's WIDTH (`eff_ball_size` = 8) with `>=` and fired one frame early, so on
a shallow descent the ball was ~2 px short of the original's contact x and
landed in the wrong zone (0x24 instead of 0x28). It uses the HEIGHT
(`BALL_H_PX` = 7) with a strict `>`, so it fires at `next_y + 7 >
bat_top(173)`. The rest snap is `$A6 = bat_top - 7 = 166`.

That width-for-height confusion appeared three times: the trigger, the
primary's per-frame stuck-ball rest, and the secondaries' copy of both.

## The MAGNET catch (bonus $03)

`LAB1F_1..3`: with bonus $03 AND a normal-width bat the ball sticks instead of
bouncing. The caught offset is QUANTIZED — `offset = ball_x - bat_x`, clamped
`>= 0`, then `& 0xFC`, then clamped to `0x18` — and the ball parks at
`bat_x + offset`, `y = $A7`. A big bat falls through to the normal deflection.

The quantized offset is what the launch direction is later derived from, so
matching it makes the catch -> launch cycle parity-correct. Captured: a centre
drop is caught at x=132 (ball_x 133 -> `0x11 & 0xFC = 0x10` -> `116 + 16`),
y=167.

**A held ball rests 1 px lower than a launch-ready one**: `LAB1F_3` sets `$A7`,
`LA27E_15` sets `$A6`. Both of the port's stuck-ball position maintainers add
1 px when the bat carries bonus $03, so no separate caught-state flag is
needed.

## Every ball runs one `handling_ball`

The original has no separate secondary-ball path: `handling_ball` runs per ball
object and `LAB1F` does not care which. The port's `step_extra_ball` is unified
onto the same model — `dir_to_dxdy` + q8.8 + `laffc_collision` +
`bat_deflect_dir` — so the extras move, bounce and deflect exactly like the
primary, correct by construction.

The spawn was already right: the multi-ball path at `LA7A6` (reached from
`LA67B_8`) derives the extras' directions from ball 1's `+$06` through the
`$080C`/`$040C`/`$0408` table.
Note that poking `object_ball_2`'s descriptor is NOT enough to activate a
secondary in the original even with `balls_quantity` set — the engine only
treats the slots as live after the full spawn sequence, so a hand-poked
secondary yields incoherent motion.

## The per-ball stuck state

`ball.stuck_*` are `[3]` arrays. **`BALL_PRIMARY` rather than a bare `0`** at
the fourteen sites that need an index, because `[0]` reads two ways that must
not be confused: "the first ball", and "the only ball that can be here". The
sites that are primary BY CONSTRUCTION — an extra ball is spawned in flight
and is never stuck (`respawn_primary_ball`, the level reset, the replay
overrides, the rocket-clear hide) — keep `[0]` inside functions whose names
already say primary.

One slot is deliberately ignored: `ball_launch_from_bat` passes `&objects[b]`
to `refresh_ball_motion_signs`, which returns early for anything but the
primary — the dx/dy sign cache is the primary's alone (known-bugs #13). For
`b > 0` that call is a no-op ON PURPOSE, and the comment says so, because a
reader who did not know would "fix" it.

FIRE frees every held ball. The original has no launch routine to be selective
with — the release lives inside `handling_ball`, so one press frees whatever is
resting. `record_primary_launch()` stays guarded to the primary, since it reads
the primary's coordinates.

### MAGNET + TRIPLE cannot be reached by catching

The bat holds ONE bonus code at `+$14`: MULTI_BALL is $02, MAGNET is $03, and
catching the second overwrites the first. In the game the pair arises the other
way round — the extras OUTLIVE the code that spawned them, so a bat picking up
MAGNET while they fly can catch them. `BATTY_REPLAY_MULTIBALL=1` seeds the
spawn at level entry (directions derived from the primary's seeded dir, so it
must run after the ball override) and the seeded CATCH bonus supplies the code
a frame or two later.
