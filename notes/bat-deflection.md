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
`bat_reflect_dir`/`bat_dir_index` in `src/main.c`), replacing the 5-zone
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

## Next steps

1. Extend the gate to more incoming dirs (0x04, 0x14, 0x18, 0x1C) — the
   capture harness and port test already parameterize on it.
2. Port the MAGNET catch branch (bonus $03) faithfully (the port keeps a
   simplified catch today; LAB1F has the exact offset-quantize logic).
3. Apply the exact deflection to the multi-ball secondaries
   (`step_extra_ball`), which still use the 5-zone split on integer
   motion — needs a multi-ball reference (no ground truth yet).
