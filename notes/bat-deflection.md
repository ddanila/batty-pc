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


## Sizing the stuck-ball split (2026-08-09)

PLAN.md WS6 item 2 defers the MAGNET catch for secondary balls as "the
primary-ball stuck system spans ~32 sites". Counted: **24 real sites**
(26 matches, two of them comments) across 15 functions, and they are not
all equal.

    3  fields    ball.stuck, ball.stuck_offset_x, ball.stuck_ticks

**Primary by construction — no ball index needed (10 sites).** An extra
ball is spawned in flight and is never stuck, so these are about the
primary whatever happens to the others:

| function | sites | why |
|---|---|---|
| `reset_level_state` | 3 | the level starts with the primary on the bat |
| `respawn_primary_ball` | 3 | in its name |
| `apply_replay_ball_motion_override` | 2 | `BATTY_REPLAY_BALL_STUCK` seeds the primary |
| `apply_replay_rocket_override` | 1 | harness |
| `hide_objects_for_rocket_clear` | 1 | clears the primary before the tally |

**Need a ball index (14 sites).** These are the refactor:

| function | sites | what it does |
|---|---|---|
| `catch_ball_on_bat` | 3 | the catch itself — sets all three fields |
| `ride_stuck_ball_on_bat` | 3 | dwell counter and auto-launch |
| `launch_or_fire` | 3 | FIRE releases the held ball |
| `rest_ball_on_bat` | 1 | positions it from `stuck_offset_x` |
| `primary_ball_launch_from_bat` (now `ball_launch_from_bat`) | 1 | reads `stuck_offset_x` for the angle |
| `step_ball`, `step_primary_ball` | 2 | early-out while held |
| `deflect_ball_off_bat` | 1 | decides catch vs deflect |
| `steer_bat_from_keys`, `draw_bottom_sprites` | 2 | a held ball follows the bat |

So the shape is: make the three fields per-ball, thread an index through
eight functions, and leave ten sites alone.

**The first half is done (2026-08-09).** `stuck`, `stuck_offset_x` and
`stuck_ticks` are `[3]` now, indexed the same way the `mag_*` arrays
already were, and every one of the 24 sites reads `[0]`. Behaviour is
unchanged by construction — all 90 gates green — which is the point of
doing it as its own step: the feature that follows can be judged on its
own diff instead of on a rename buried inside it.

Only `[0]` is ever non-zero today, because `catch_ball_on_bat` is
reachable only from the primary's path. What remains is the fourteen
sites above learning WHICH ball, and `catch_ball_on_bat` learning which
BAT. That is a real change but a
bounded one — and rather smaller than "~32 sites" suggests, which is
probably why it has stayed deferred.

It is also the same refactor bat 2's catch needs (notes/double-play.md):
`catch_ball_on_bat` reads `BAT_X` directly, so the index has to name a
BAT as well as a ball. Doing it once serves both.


## The second half is threaded (2026-08-10)

The fourteen sites now know WHICH ball. The eight functions take a slot:

    ball_launch_from_bat(int b)        was primary_ball_launch_from_bat
    catch_ball_on_bat(int b, contact)
    rest_ball_on_bat(int b)
    ride_stuck_ball_on_bat(int b)

and the four readers that are not functions of their own — `step_ball`,
`step_primary_ball`, `launch_or_fire`, `redraw_frame` — spell the slot
`BALL_PRIMARY` instead of a bare `0`.

**No behaviour change, and that is the whole point of doing it alone.**
Every caller still passes `BALL_PRIMARY`, because `catch_ball_on_bat` is
still only reachable from the primary's path. The feature that follows —
a secondary ball being caught — is then a diff about the feature rather
than a rename with a feature buried in it. Verified by the full sweep:
66 of 67 QEMU gates green, and the 67th was already red before this
work (known-bugs #18).

### Why `BALL_PRIMARY` rather than leaving `0`

`[0]` reads two ways that must not be confused: "the first ball", and
"the only ball that can be here". Ten of the twenty-four sites are
primary BY CONSTRUCTION — an extra ball is spawned in flight and is
never stuck, so `respawn_primary_ball` and the replay overrides can
never mean anything else. The other fourteen are primary FOR NOW.

Both were spelled `0`. Now the second group says `BALL_PRIMARY` at a
call site, which is a thing you can grep for and change; the first group
keeps its `[0]` inside functions whose names already say primary.

### The one place the slot is deliberately ignored

`ball_launch_from_bat` passes `&objects[b]` to
`refresh_ball_motion_signs`, which returns early for anything that is
not the primary — the dx/dy sign cache is the primary's alone
(known-bugs #13). For `b > 0` that call is a no-op ON PURPOSE, and the
comment says so, because a reader who did not know would "fix" it.

### The bat followed, same day

`catch_ball_on_bat`, `rest_ball_on_bat` and `ride_stuck_ball_on_bat` now
take a BAT as well as a ball, and read `objects[bat].x_coord` instead of
`BAT_X`. Still no behaviour change on its own — every caller passed
`OBJ_BAT_1` — and then the feature was four lines.

**One field had to be added: `ball.stuck_bat[3]`.** A held ball rides
the bat that caught it, and which bat that is cannot be derived. "The
bat on the right half" is not bat 2 once a court clamp has moved either
of them, and the ball's own x IS the bat's, so it answers nothing.
`catch_ball_on_bat` records it; `step_ball` and `redraw_frame` read it.

`respawn_primary_ball` and the level-entry reset put it back to
`OBJ_BAT_1` explicitly. Without that a ball caught by bat 2 comes back
after a life loss still held by bat 2 — the state outlives the thing
that justified it, which is the standard hazard of adding a field.

Gated by `test-double-play-bat2-catch`, which compares two probe frames
rather than one: a single frame cannot tell a caught ball from one
passing through the rest height. Two mutations caught — recording
`OBJ_BAT_1` at the catch, and removing bat 2's catch branch.


## WS6 item 2 is closed (2026-08-10)

A MAGNET bat holds a SECONDARY ball. The case the item was opened for,
and the one that kept it deferred as "substantial new code for the niche
MAGNET+TRIPLE case".

It was substantial, and it was four commits, none of them large:

    fields -> [3]                   2026-08-09  no behaviour change
    functions take a BALL           2026-08-10  no behaviour change
    functions take a BAT            2026-08-10  no behaviour change
    the two catches                 2026-08-10  bat 2, then the extras

The final step in `step_extra_ball` is one branch, in a bat block whose
comment used to end "No catch." Splitting it this way is why: each
structural commit was verifiable by a green sweep alone, and the
behaviour commits were small enough to read.

### The original needs none of this

It runs ONE `handling_ball` per ball object, and `LAB1F` does not care
which one it is. The port's split into `step_ball` and
`step_extra_ball` is what made the catch primary-only — so this whole
item was the port repaying a divergence it had introduced, not porting a
mechanic. Worth knowing before the next "the original must special-case
this" hypothesis.

### FIRE frees every held ball

`launch_or_fire` releases the extras too. Same reasoning: the original
has no launch routine to be selective with, the release lives inside
`handling_ball`, and one press frees whatever is resting. Leaving them
would also strand them for the full `STUCK_TIMEOUT` with the player
pressing FIRE at them.

The auto-launch's `record_primary_launch()` is guarded to the primary
rather than indexed — it reads the primary's coordinates, so a
secondary recording itself there would corrupt the launch gates.

### Reproducing MAGNET + TRIPLE, which cannot be caught for

The bat holds ONE bonus code at +$14: MULTI_BALL is $02, MAGNET is $03,
and catching the second overwrites the first. No sequence of catches
puts both in play.

In the game the pair arises the other way round — the extras OUTLIVE the
code that spawned them, so a bat picking up MAGNET while they fly can
catch them. `BATTY_REPLAY_MULTIBALL=1` seeds the spawn at level entry
(directions derived from the primary's seeded dir, so it runs after the
ball override) and the seeded CATCH bonus supplies the code a frame or
two later.

### Still open, and deliberately not half-done

`step_extra_ball`'s bat block reads `eff_bat_left`/`eff_bat_right` and
never consults bat 2, so in Double Play a secondary cannot be caught OR
deflected by bat 2. That is a different gap — extras vs bat 2 — and it
is named in the gate's docstring so the gate is not mistaken for
covering it.


## An offset ON a zone boundary belongs to the zone ABOVE (2026-08-10)

    LAB1F_6: CP (HL) / JR C,LAB1F_7 / INC HL / INC HL / JR LAB1F_6

`JR C` leaves the walk only when `offset < boundary`, so it keeps
advancing while `offset >= boundary`. An offset equal to a boundary
therefore lands in the HIGHER zone. The port's
`while (i < 12 && u8(offset) >= zones[i]) i += 2;` matches.

Normal-bat boundaries are $04, $08, $0C, $10, $14, $18; big-bat $06,
$0C, $12, $1A, $20, $26.

**Nothing tested it.** Mutating `>=` to `>` — which makes a boundary
offset deflect as though it were one pixel to the LEFT — passed the
whole host suite. The reason is in this file: the captured hardware
cases are offsets **-3, 5, 13, 21 and 29**, and not one of them sits on
a boundary.

That is not an accident of sampling so much as a consequence of how the
table was built — offsets were chosen to span the zones, which means
choosing points comfortably inside them. A table built to demonstrate
that zones differ will not test where one ends.

`bat_zone_boundary_above` asserts the property directly: for each
boundary b and each of the six incoming directions, the deflection at b
equals the deflection at b+1, and differs from b-1 somewhere (or the
first half would hold under either rule).
