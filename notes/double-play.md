# Double Play, traced

`game_mode == $02`. Complete (PLAN.md WS3). This is what the mode actually
is, which differs from what watching the game suggests.

`game_mode` is 0-based — 0 = 1 Player, 1 = 2 Players, 2 = Double Play — while
the port's `selected_mode` is 1..3 (`k - '0'` in `run_menu`), so any dispatch
on it subtracts one.

## The divider confines the BATS, and nothing else

`handling_bat` moves the bat +/-4 from the control word and then clamps it
with `check_left_margin` / `check_right_margin` — the same two routines the
alien uses, over the FULL playfield, with no divider term. The court clamps
are a SEPARATE pair, run once per frame after both bats have been handled:

    LD IX,object_bat_1 / CALL LACCE       ; $ACCE
    LD IX,object_bat_2 / CALL LACAD       ; $ACAD

    LACCE:  A = (IX+$0C) + (IX+$02)       ; width + x, 8-bit
            CP $80 / RET C                ; below the divider, nothing to do
            (IX+$02) = $80 - (IX+$0C)     ; right edge pinned ON $80
            if width == $1C or $2C: SET 0,(IX+$01)

    LACAD:  CP $80 / RET NC               ; at or above it, nothing to do
            (IX+$02) = $80                ; left edge pinned ON $80
            RES 0,(IX+$01)

They are asymmetric on purpose: bat 1's RIGHT edge stops at the divider (the
comparison includes the width), bat 2's LEFT edge does (it does not). Written
symmetrically, bat 1 would overlap the separator by its own 28 px.

The `(IX+$01)` pokes are the 4 px sub-character sprite shift, normally derived
from bit 2 of x. A bat whose x was just overwritten carries a stale one, so
the clamp sets it by hand. **Not ported** — that bit picks a pre-shifted copy
of the sprite because ZX bitmaps are byte-aligned; mode 13h blits at any pixel
column, so the port has no shifted variant and nothing reads bat `sprite_num`.

`bat_court_clamp_1` / `bat_court_clamp_2` in `src/physics.cpp` are pure and
host-tested. They take the ORIGINAL's (left edge, width) pair, so a grown
bat 1 passes `eff_bat_left()` rather than the port's centre-ish `BAT_X`.

### LACCE's 8-bit sum wraps, and the CALLER is what makes it safe

`LD A,(IX+$0C) / ADD A,(IX+$02) / CP $80 / RET C` is an 8-bit add, and the
port keeps it as one. Feed it x=$FF, w=$1C and the sum is $1B — below $80, so
the clamp lets the bat straight through.

Unreachable in play, because `bat_step_x` has already capped the right edge at
$F8, eight short of the wrap, for both settled widths. The arithmetic is not
safe; the caller is. That is why the `u8` stays rather than being quietly
widened, and why the host test characterises the wrap instead of pretending it
is absent. The enemy's copy of this idiom does overflow for real
(known-bugs #16).

**`spr_separator` is 16 px wide and 24 rows tall at (125, 169)** — down in the
bat band, not spanning the court. Nothing tests the BALL against it: a ball
crosses freely, which is what makes the mode co-operative rather than two solo
games.

## What the halves ARE for: scoring

`handling_bat` records which side it is dealing with straight off the x
coordinate's top bit:

    LD A,(IX+$02) / AND $80
    LD (need_change_player),A   ; 0 if x < 128

and `add_points_to_score` reads it: in mode $02 with the flag set it swaps the
1up/2up score blocks, adds, and swaps back — the same swap-add-swap idiom
`players_swap` uses (`notes/menu.md`). Outside Double Play the whole block is
skipped.

**WHICH object's top bit is tested differs by caller**, and that is the part
worth not re-deriving:

| site | flag from |
|---|---|
| `handling_bat` -> `kill_enemy_by_bat` | the BAT's x, `(IX+$02)` |
| `LA67B_1` -> `get_bonus` | the CATCHING BAT's x, `(IY+$02)` |
| `handling_bullet` | the BULLET's x, `(IX+$02)` |
| `handling_ball` | the ball's `(IX+$12)` — an OWNER bit, not its x |
| `add_points_for_left_briks` | zero, then alternated to split evenly |

All four live sites set it as the FIRST thing their routine does, before any
collision test, so the value is unambiguous at the moment of the kill.
`add_points_to_score(pts, side_x)` is the single place a score is added and
`check_two_player_state` holds that — a second `+=` site would bypass the side
rule, and the gate asserts by IDENTITY (three brick-scoring sites, three
DIFFERENT sides) rather than by count.

### The ball's owner bit is per BALL

`+$12` is a counter in bits 0..6 with bit 7 as a separate flag, and every
counter operation preserves it (`AND $80` / `AND $7F`). The flag itself moves
on every bat deflection:

    LAB1F_0:
      RES 7,(IX+$12)      ; the ball being handled
      BIT 7,(IY+$02)      ; the HITTING bat's x
      JR Z,LAB1F_1
      SET 7,(IX+$12)

`IY` is whichever bat `obj_compare` matched and `handling_ball` runs once per
ball, so **every ball carries its own owner and brick points follow whoever
last hit THAT ball**. A grep for `LD (IX+$12)` finds only the counter writes:
Z80 bit twiddling is `RES`/`SET`/`BIT`.

The initial owner comes from the start side, which alternates every level
entry (`LD A,(ball_x_coord+$01) / XOR $88`, so `$48 <-> $C0` — left, right,
left).

Ported as `ball_owner_side[3]`, indexed like the stuck and `mag_*` arrays;
`brick_hit_resolve`, `brick_collision` and `laffc_collision` take the slot.
Level entry seeds all three from the start side (no extras are alive then, so
a slot cannot be read before it is written); `spawn_extra_ball` inherits the
primary's; a deflection re-owns only the ball it hit. Because the entry side
alternates, `test-extra-ball-owner` asserts a RELATIONSHIP rather than
literals: read the side at frame 2, require all three to agree, then require
exactly slot 1 to differ at frame 12.

### The leftover-brick tally splits evenly, not by side

`add_points_for_left_briks` — the rocket-clear tally — alternates the flag per
surviving brick regardless of where it sits ("Добавляет двум игрокам поровну
очки"). Note `XOR $01`, not `$80`: everywhere else the flag is a coordinate's
top bit, here it is bit 0. `add_points_to_score` only tests `AND A / JR Z`, so
both conventions coexist; the port passes `0x80` for "2UP" everywhere and
keeps the alternation.

## Death sparks split across both bats

`LBC10` seeds ten object slots as `anim_spark` around bat 1. In Double Play
`LBC10_4` then translates half onto bat 2:

    LD A,(object_bat_1+$02) / LD C,A
    LD A,(object_bat_2+$02) / SUB C / LD (LBCE6+$01),A   ; self-modified delta
    LD IX,object_ball_2 / LD DE,$0016 / LD B,$05
    LBC10_4: LD A,(IX+$02) / [ADD A,delta] / LD (IX+$02),A / ADD IX,DE x2 / DJNZ

It starts at the SECOND seeded slot and steps two objects at a time, five
times, so it moves the odd-indexed half. It is a SPLIT, not a second spawn:
each bat explodes with five sparks, not ten. `spawn_death_sparks` does a
post-pass over the odd indices, which is the same set.

## The split keyboard

The original seats two players at one Spectrum by cutting the keyboard down
the middle:

    $FDFE  A S D F G     AND $05 -> A,D = LEFT    AND $0A -> S,F = RIGHT
    $BFFE  Ent L K J H   AND $0A -> J,L = LEFT    AND $05 -> K,Ent = RIGHT

The interleaving is not a transcription error: each direction gets two keys
straddling the other's, so the cluster works whichever way a player rests
their hand. These are LETTERS, so unlike the device list in WS1 they
transcribe to PC scancodes unchanged.

The fire cluster is one combined half-row, `$5FFE = $7FFE | $DFFE` — Y U I O P
with B N M, SYMBOL SHIFT and SPACE, `AND $1F` — so any of them fires. Polled
from `key_state` rather than the BIOS buffer, which is both closer to the
original and the only option: one BIOS key queue cannot serve two players.

**Two keys are deliberately dropped, both because the port spent them
first.** ENTER is bat 2's RIGHT on the Spectrum, and it is this port's
attract-chain key, pressed by `--wait-key` to start every capture
(known-bugs #19). SPACE is player 1's fire, pressed by `test-visual` and
`test-normal-ball-launch`. The pattern: **transcribe the cluster, minus
whatever the port has already spent elsewhere.**

**One deliberate addition:** player 1's arrow keys keep working in Double
Play. The original forces both players onto the split keyboard, but on a PC
the arrows sit a long way from HJKL, so leaving them live costs player 2
nothing. A superset, gated as such.

**Not ported:** both original readers bail to the per-device poll unless BOTH
players are on `ctrl_type` 0. Nothing selects a device yet (WS1), so
`ctrl_type` is 0 by construction and there is nothing to gate. The mode-2
START X ($48 / $C0 rather than resting on a bat) is also not ported — the
alternation drives the owner either way.

## Per-bat state

Everything a bonus touches exists twice in the original, kept apart by
`bonus_flag_swap` around the bat-2 call. The port arrived at the same place
in five steps, and the ORDER is the reusable part: the bonus BYTE stops being
mirrored; `BatState` becomes `bats[2]` with `BAT_SLOT()`; `render_bat_of(b,
attr)` replaces bat 2's three-line copy; the per-frame redraw paths learn
about bat 2; the width and the laser follow the catching bat. Each structural
step was verifiable by a green sweep alone, so each behaviour step was small
enough to read on its own diff.

`get_bonus` ($A67B) and `LAB1F` share one fall-through shape — bat 1 first via
`obj_compare_2pix`, bat 2 only in mode $02 and only when bat 1 missed — so
catching, deflecting, holding and being paid all reach bat 2 the same way.
`bonus_catching_bat` returns which bat or -1. `kill_enemy_by_bat` has NO bonus
condition: any bat touching an alien destroys it.

`free_bullet_2` ($A14C) reads the firing bat out of IX (`LD A,(IX+$02) /
ADD A,$0C`), so the bullet leaves whichever bat fired, 12 px in from its left
edge. `try_fire_laser_from(b)` is that. The bullet POOL and the cooldown stay
global — the original's `bullet` counter is a single byte, so two armed bats
compete for the same two slots. Checked, not assumed.

`tick_bat_resize` ramps both bats off the SAME every-other-frame gate: the
original steps from `counter_misc`, one global counter, so two growing bats
stay in step rather than each keeping private phase.

### `ball.stuck_bat[3]` had to be added

A held ball rides the bat that caught it, and which bat that is cannot be
derived: "the bat on the right half" is not bat 2 once a court clamp has moved
either of them, and the ball's own x IS the bat's. `respawn_primary_ball` and
the level-entry reset put it back to `OBJ_BAT_1` explicitly — without that, a
ball caught by bat 2 comes back after a life loss still held by bat 2. State
outliving the thing that justified it is the standard hazard of adding a
field.

### `extra_ball_meets_bat` returns three outcomes, not two

    0  no contact — try the next bat
    1  deflected  — *next_y snapped, direction rewritten
    2  caught     — stop the frame

A boolean "was it caught" made a bat-1 DEFLECTION read as "no contact", so the
ball was offered to bat 2 and could be handled twice in one frame. `LAB1F`
falls through only when bat 1 did not OVERLAP, so a deflection has to stop the
search as firmly as a catch. Not gated, because the bats are 28 px apart at
their closest and no seeded trajectory here produces both deflections in one
frame.

## The original needs none of this

It runs ONE `handling_ball` per ball object and `LAB1F` does not care which
one it is. The port's split into `step_ball` and `step_extra_ball` is what made
the catch primary-only — so the whole per-ball thread was the port repaying a
divergence it had introduced, not porting a mechanic. Worth knowing before the
next "the original must special-case this" hypothesis.

## Two seeding traps

**`BATTY_HOLD_KEYS=1F,24`** seeds `key_state[]` with scancodes never released.
The capture harness runs headless so INT 9 never fires and the bits survive
the whole run — a held key is the only kind this harness can express. It has
to be in the Makefile's AUTOEXEC passthrough list, and the seeded bat has to
start INSIDE its own court, so the gate seeds $38 rather than the stock $74.

**A seed outside a table's domain measures the table.** Seeding the ball
`dir=$10`, straight down at bat 2, made it STICK: `$10` is not one of the
directions `LAB1F_11` searches for ({$04,$08,$0C,$14,$18,$1C}), so the lookup
returns its input and the ball re-collides for ever. Real trajectories are
diagonal; a seeded gate can produce what the game cannot.
`test-double-play-bat2` uses `$08`.

`test-double-play-bat2-width` asserts that every changed pixel lies inside
bat 2's widened footprint and that both 8 px side lobes contain at least one
change — not equality, and not "all 16 lobe pixels": at `extra_px >= 8` the
renderer swaps to the 48 px `SPR_BAT_BIG` with its own texture, and a lobe
column can match the background it replaced.
