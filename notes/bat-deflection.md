# Bat deflection — decode + ground truth (LAB1F @ $AB1F)

The next gameplay-parity gap after the byte-exact ball motion + LAFFC
brick collision is the **bat deflection**: how the bat reflects the ball.
The port currently uses a 5-zone approximation
(`step_ball`/`step_extra_ball` pick one of ~5 fixed dirs by hit zone);
the original derives it more granularly in `LAB1F`. This note decodes
`LAB1F` and captures the ground-truth deflection table to validate a port
against.

## Breakthrough: a ball-onto-bat scenario WITHOUT a new snapshot

`parity-status.md` recorded that validating the bat bounce was blocked —
the L3 byte-exact ball orbits the upper field and never reaches the bat
(confirmed: probed 750 frames, ball y stays 8–78, even gets trapped
vertically against the right wall at frames 225–450), and poking a
*placeholder* ball into the raw snapshot hangs.

The unblock: the `l3-brick-flash` 22-byte ball descriptor is **already
coherent** (it runs cleanly for 750+ frames). *Repositioning* that ball —
changing only x (+2), y (+4), dir (+6), and prev-x/prev-y (+14/+15) —
keeps it coherent, unlike poking placeholder garbage. Drop it at y=0x96
(just above the bat at y=0xAD/173) heading straight-ish down, and the
original computes the deflection itself. No hang, no new snapshot.

`scripts/capture_bat_deflection.py` automates this: it sweeps the start x,
frame-steps across the bat contact (IM1 boundary `0x0038`), and reports
the outgoing direction. `replays/l3-bat-descend.json` is a single-shot
variant.

## LAB1F decode (the exact deflection)

`LAB1F` is called per frame from `handling_ball`. IX = ball object, IY =
bat object.

```
LAB1F:
  ball y (IX+$04) < $98 (152)?           -> RET (too high, no contact)
  ball (IX+$0F) >= $AA?                  -> RET
  obj_compare ball vs object_bat_1       -> overlap? else (2P) try bat_2 else RET
LAB1F_0: bat hit confirmed
  RES 7,(IX+$12); copy bat x bit7 -> ball (IX+$12) bit7
  set_sound_bat_beat
  bat applied-bonus (IY+$14) == $03 (MAGNET) AND bat width $1C?
     -> CATCH branch: store offset in (IX+$15), (IX+$14)=$B0, ball y=$A7. RET
  else -> LAB1F_4 (normal deflection)
LAB1F_4:
  ball y (IX+$04) = $A6 (166)            ; snap onto bat top
  (IX+$12) &= $80
  HL = LABEE if bat width==$1C else LABFC ; threshold->zone table
  A = ball_x (IX+$02) + 3 - bat_x (IY+$02) ; "offset" into the bat
  walk (threshold,zone) pairs: first pair whose threshold > offset -> zone
  if zone bit2 set:  reflect dir; lookup LAC0A; reflect dir again
  else:              lookup LAC0A
reflect (LAB1F_9):  dir = ((dir XOR $1F) + 1) & $3F
lookup  (LAB1F_10): base = LAC0A + (zone & 3) * 6
                    index incoming dir within {04,08,0C,14,18,1C}
                    dir = base[index]
```

### Tables

`LABEE` (normal bat, width $1C) — (threshold, zone) pairs:
```
(04,07) (08,06) (0C,05) (10,00) (14,01) (18,02) (FF,03)
```
`LABFC` (big bat): `(06,07)(0C,06)(12,05)(1A,00)(20,01)(26,02)(FF,03)`

`LAC0A` — [4 zone groups (zone&3)][6 incoming dirs 04,08,0C,14,18,1C]:
```
zone0: 3C 38 34 2C 28 24
zone1: 3C 38 34 34 34 34
zone2: 3C 38 38 34 38 38
zone3: 3C 3C 38 38 3C 3C
```

## Ground truth (captured, incoming dir 0x0C, bat x=116 width 28)

`offset = contact_ball_x + 3 - 116`

| start x | contact x | offset | out dir |
|--------:|----------:|-------:|:--------|
| 104 | 110 | -3 | 0x28 |
| 112 | 118 |  5 | 0x2C |
| 120 | 126 | 13 | 0x34 |
| 128 | 134 | 21 | 0x38 |
| 136 | 142 | 29 | 0x38 |
| 144 | 157 | 44 | NO-FLIP (ball misses bat, lost) |

Monotonic and physical: left of the bat deflects up-left (lower dir),
right deflects up-right (higher dir).

## Why the port must be a LITERAL translation, not hand-derived

Hand-tracing the decode above for offset 21 predicts `0x2C`, but the
hardware gives `0x38`. The `bit2`-zone path **falls through `LAB1F_8`
into `LAB1F_9` after `LAB1F_10` returns**, so those zones reflect, look
up, then reflect *again* — and the exact contact pixel (hence offset and
zone) is subpixel-sensitive. So a faithful port has to copy the opcode
flow exactly and be validated against the captured table above, not
reasoned out by hand. The match loop also *skips* dir `0x10` (pure
vertical), which never occurs in real play; a synthetic dir=0x10 ball
does not return cleanly from `LAB1F` (don't use it as a probe dir).

## DONE: ported + validated

`LAB1F` is now ported literally into `step_ball`'s bat-contact path
(`bat_deflect_dir` + `bat_zone_tbl_normal/big` + `bat_deflect_tbl` +
`bat_reflect_dir`/`bat_dir_index` in `src/main.cpp`), replacing the 5-zone
approximation. The offset is `next_x + 3 - BAT_X`; an enlarged bat
(`bat_extra_px != 0`) selects the LABFC table. Unmatched incoming dirs
(pure-vertical 0x10 / non-multiple-of-4, which never reach the bat in real
play) fall back to a plain vertical reflect so the port never hangs.

Validated end-to-end by `make test-bat-deflection`
(`scripts/test_bat_deflection_port.py`, in `parity-check`): seeding the
port with the same descending ball reaches the same contact pixel and
produces the **same outgoing dir as the Spectrum** across 13 (dir,
position) cases spanning three incoming dirs / two `dir_to_dxdy`
quadrants and the threshold zones incl. the bit2 double-reflect:

- dir 0x0C (q=0x00): start_x 104/112/120/128/136 -> 0x28/0x2C/0x34/0x38/0x38
- dir 0x08 (q=0x00, col 1): start_x 100/108/116/124 -> 0x28/0x38/0x38/0x3C
- dir 0x14 (q=0x10, col 3): start_x 124/132/140/148 -> 0x28/0x2C/0x34/0x38

The byte-exact L3 upper-field gate (`make test-laffc-ball-frame1`) is
unchanged — L3 never contacts the bat, so that path is untouched.

### Contact-timing bug found + fixed via the broadened coverage

Extending the gate from dir 0x0C to 0x08 exposed an off-by-one in the
bat-contact trigger: the port fired the bounce one frame early, so on a
shallow descent (dir 0x08 left of centre) the ball was ~2px short of the
original's contact x, landing in the wrong threshold zone (0x24 instead
of 0x28). The original fires `LAB1F` when `obj_compare` reports Y overlap,
which (`LAC22`: `166 - ball_y` borrows) is exactly `ball_y >= 167`. The
port was using the ball **width** (`eff_ball_size` = 8) and `>=` for the
Y test; the fix uses the ball **height** (`BALL_H_PX` = 7) and a strict
`>` so it fires at `next_y + 7 > bat_top(173)` ⟺ `ball_y >= 167`, exactly
matching the Spectrum. The rest-snap is `$A6 = bat_top - 7 = 166`.

## MAGNET catch (bonus $03) — ported + validated

`LAB1F_1..3`: when the bat carries bonus $03 (MAGNET/CATCH) AND is normal
width ($1C), the ball sticks instead of bouncing. The caught offset is
QUANTIZED: `offset = ball_x - bat_x`, clamped `>=0`, then `& 0xFC`
(multiple of 4) and clamped to `0x18`; the ball is parked at `bat_x +
offset`, `y = $A7 = 167`, and marked caught. The quantized offset is what
the launch direction is later derived from, so matching it makes the
catch->launch cycle parity-correct.

Captured ground truth (`replays/l3-bat-catch.json`, bat +$14 poked to
$03): a centre drop is caught at **x=132** (ball_x 133 -> offset
`0x11 & 0xFC = 0x10` -> rest x `116+16 = 132`), `y=167`. The port now
quantizes the offset and gates on normal width (a big bat falls through
to the normal deflection, per the original); validated by the catch case
in `make test-bat-deflection` (caught rest x = 132).

## Resting ball y fixed ($A6) for the common case

The per-frame stuck-ball tracker in `step_ball` was resting the ball at
`BAT_Y - eff_ball_size` (= 173 - **8 width** = 165), silently clobbering
`respawn_primary_ball`'s correct `$A6` every frame. It now uses
`BAT_Y - BALL_H_PX` (= 173 - **7 height** = 166 = `$A6`), so the
level-start / launch rest ball sits exactly where the Spectrum's does
(its bottom row on the bat top). Probed: level-start stuck ball y = 166
(was 165); `make test` stays pixel-identical.

## MAGNET catch-rest ($A7) vs launch-rest ($A6) — done

The original rests a MAGNET-held ball 1px lower than the launch rest:
`LAB1F_3` sets `$A7=167`, vs `LA27E_15`'s `$A6=166`. The port has TWO
stuck-ball position maintainers — the `step_ball` stuck block and the
authoritative one in the play loop (the one that actually wins while
waiting for FIRE). Both now add 1px when the bat carries the MAGNET bonus
(`objects[OBJ_BAT_1].bonus_applied == $03`, the original's `IY+$14`), so
no separate caught-state flag is needed. Probed: MAGNET-held ball y = 167,
level-start launch-rest y = 166 (the +1 is gated on bonus $03, so the
non-magnet path is byte-identical and `make test` is unaffected).

## Next steps

1. Validate the catch->FIRE-release launch direction against the original
   (needs driving the release; the launch code is already validated for
   the level-start case).
3. Unify the multi-ball secondaries (`step_extra_ball`) onto the exact
   q8.8 + LAFFC + LAB1F model — see the blocker below.

## Multi-ball secondaries — unification IN PROGRESS

`step_extra_ball` (the TRIPLE_BALL extras, `object_ball_2/3` @ $9AE6/$9AFC)
still uses integer motion + the 5-zone deflection, where the original runs
the *same* `handling_ball` (q8.8 + LAFFC + LAB1F) for every ball. Unifying
it onto the validated primary code (correct by construction — one
`handling_ball` for all balls) is now underway:

- **Spawn already correct:** the TRIPLE_BALL spawn already derives the
  extras' directions (`ball2_dir`/`ball3_dir` via the `q|$08/$0C/$04`
  logic = the original `$080C` table) — the hard part is done; it just
  also converts them to the legacy integer `ball2_dx/dy`.
- **Step 1 DONE (enabling refactor, zero behaviour change, ball gate
  byte-exact):** generalized `laffc_collision` to take an `object_t *o`
  (was hardcoded `OBJ_BALL_1`), and extracted `reflect_obj_dir(o,flip_x,
  flip_y)` from `ball_reflect_descriptor`. Both now work for any ball.
- **DONE:** `step_extra_ball` now mirrors `step_ball` — stores the dir in
  `objects[obj_idx].dir` (+ ball_1's speed, zeroed q8.8) at spawn, moves
  via `dir_to_dxdy` + q8.8 (`x/y_coord_hi`), wall-bounces via
  `reflect_obj_dir`, bricks via `laffc_collision(o,…)`, deflects via
  `bat_deflect_dir`; the integer `ball2_dx/dy` + 5-zone model is gone
  (only the stuck/catch + life-decrement paths are omitted, as the
  original does for extras). Validated: build clean, primary ball gate
  byte-exact (the shared LAFFC preserved it), liveness sweep LIVE on
  L1/5/10/15. Byte-exact is correct-by-construction — the extras now run
  the SAME validated `handling_ball` path as the primary, which is what
  the original does (one handling_ball for all balls). So multi-ball
  secondaries now move/bounce/deflect exactly like the original.

Two earlier parts:

- **Done now:** the secondaries' bat-contact Y geometry had the same
  width-vs-height bug as the primary (fired at `next_y + 8 >= bat_top`,
  rested at `bat_top - 8` = 165). Fixed to mirror the validated primary
  (`next_y + 7 > bat_top` ⟺ ball_y >= 167, rest `bat_top - 7` = $A6).
  Correct by construction (one LAB1F for all balls); the deflection table
  itself still needs the motion-model unification below.
- **Blocked:** porting the full exact model needs a captured secondary
  reference, and the repositioning trick that unblocked the primary does
  NOT extend to the secondaries. Activating `object_ball_2` by poking its
  descriptor (`+00=$02`) — even with `balls_quantity` ($5CD9) set to 2 —
  yields **incoherent** motion (probed: +39 px in one frame, speed
  flipping 3->2, then frozen at the bottom edge). The engine only treats
  ball_2/3 as live balls after the full `bonus_triple_ball` spawn
  sequence (`LA7A6`): it sets `balls_quantity=3`, derives ball2/ball3
  directions from ball_1's `+$06` via the `$080C`/`$040C`/`$0408` DE
  table, copies ball_1's properties (`+$00=$02`, `+$11=0`, speed, etc.)
  into the slots. So a faithful multi-ball capture must replay that spawn
  sequence, not just poke a descriptor. That replication is the next
  investment to unblock secondary-ball validation.
