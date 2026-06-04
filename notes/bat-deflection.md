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

## Next steps

1. Port `LAB1F` literally into the bat-contact path of `step_ball`
   (replacing the 5-zone block), including the `LABEE/LABFC/LAC0A`
   tables and the double-reflect for bit2 zones.
2. Add a regression that asserts the port's deflected dir matches this
   ground-truth table (extend `capture_bat_deflection.py` to cover more
   incoming dirs: 0x04, 0x14, 0x18, 0x1C).
3. Port the MAGNET catch branch (bonus $03) separately.
