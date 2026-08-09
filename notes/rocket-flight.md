# Rocket bonus flight + level-clear — decode & parity (2026-06-05)

The ROCKET bonus ($06, `spr_bonus_rocket`): when caught, the bat turns
into a rocket that flies up and ends the round. Full decode of the
original control flow vs the port, with the parity verdict for each part.

## Launch transition — `LBAED_6` ($BAED area)

The normal play loop top (`LBAED`) checks `object_rocket`; when non-zero
(set by `get_rocket` at $AA9D when the bonus is caught) it jumps **once**
to `LBAED_6`:

```
LBAED_6:
  LD B,$0B; LD DE,$0016; LD IX,object_ball_1
LBAED_7:                       ; deactivate all 11 objects (balls, enemy, bonus…)
  LD A,(IX+$00); AND A; JR Z,LBAED_8
  SET 7,(IX+$00)
LBAED_8:
  ADD IX,DE; DJNZ LBAED_7
  LD A,$01
LBB83:
  LD HL,$0000; LD (HL),A        ; (self-mod target = the caught bat) mark it
  LD A,$06; LD (object_rocket),A
  LD A,$05; LD (sounds_queue),A
  XOR A; LD (counter_misc),A     ; *** counter_misc reset to 0 at launch ***
  JR LBB97_0
```

The `XOR A; LD (counter_misc),A` is the key detail: the global frame
counter's low byte is **reset to 0** at rocket launch. `get_rocket` also
resets the velocity accumulator `LD HL,$0000; LD (LA8CF),HL`.

## Flight loop — `LBB97` / `LBB97_0` ($BB97)

A dedicated loop, separate from the normal play loop:

```
LBB97:
  LD A,(counter_misc); INC A; LD (counter_misc),A   ; ++ each flight frame (from 0)
  CALL random_generate
  CALL call_hl_for_all_obj (handling_object)         ; -> handling_rocket (motion only)
  CALL fill_briks_data                               ; metal-shimmer slots only
LBB97_0:
  … ix_buf_addr_calc / save_objs_to_buff / print … EI/HALT/DI … render/restore … pause_game
  LD A,(object_rocket); AND A; JP Z,LBBFB            ; rocket gone -> award + next round
  JP LBB97
```

**There is NO brick destruction anywhere in the flight loop.**
`handling_object` dispatches the rocket to `handling_rocket`, which is
motion-only (below); `fill_briks_data` only cycles the metal-shimmer
animation slots; bricks are not objects. The rocket sprite is simply
drawn **over** the still-intact brick field as it rises.

## Motion — `handling_rocket` ($A89A) — PORT IS FAITHFUL ✔

```
handling_rocket:
  LD A,(counter_misc); AND $01; LD (IX+$01),A   ; 2-frame sprite flicker
  CALL calc_write_spr_addr
  LD HL,(LA8CF); LD DE,$FFE0; ADD HL,DE          ; velocity += -32 (0xFFE0)
  LD A,(counter_misc); CP $38; JR C,LA89A_0      ; only PERSIST velocity once counter>=56
  LD (LA8CF),HL
LA89A_0:
  LD A,(LA8D1); LD E,A; LD D,(IX+$04); ADD HL,DE ; pos(16.8: hi=y, lo=frac) += velocity
  LD A,L; LD (LA8D1),A; LD A,H; LD (IX+$04),A     ; store frac + y
  SUB $06; LD (object_bat_1+$04),A; LD (object_bat_2+$04),A   ; bat y = rocket y - 6
  RET
```

So: 16.8 fixed-point position, velocity accumulator that grows by -32 per
frame but only **persists** once the counter reaches $38 (56). Because
`counter_misc` was reset to 0 at launch and `++`s each flight frame, the
`CP $38` gate means "start accumulating velocity 56 frames after launch"
— i.e. a slow near-constant initial lift (velocity = stored-0 + -32 =
-0.125 px/frame) then a quadratic acceleration once persistence kicks in.

The port's `step_rocket` reproduces this exactly with a **per-rocket**
`rocket_counter` (reset to 0 at launch, `++` each step, `unsigned char`
so it wraps at 256 like the byte counter): `hl = rocket_acc - 0x20; if
(rocket_counter >= 0x38) rocket_acc = hl; pos = hl + (y<<8|frac)`. This
is byte-equivalent to the original — the per-rocket counter is correct
**because `counter_misc` is reset to 0 at launch** (an earlier worry that
the global-vs-per-rocket distinction mattered was unfounded; verified via
the $BAED launch reset). Bat y = rocket y - 6 is matched.

## End of flight — `LBBFB` → `add_points_for_left_briks` ($AF0D)

When the rocket leaves the field the loop goes to `LBBFB`:

```
LBBFB:
  CALL add_points_for_left_briks
  … play_sounds_queue / increment_round_number / pause_long … JP LB9E8_1 (next round)
```

`add_points_for_left_briks` walks the 15×12 grid and for every brick that
is **not** empty/undestructible (`AND $A0` = bit7 destroyed | bit5 metal):

```
  CALL points_calc_and_add        ; add this brick's points
  CALL scr_score_update
  … toggle need_change_player …
  CALL play_sounds_queue
  LD D,$03; CALL pause_short      ; *** a short pause PER BRICK ***
  INC IY  …                       ; next cell
```

It **ticks points up brick-by-brick with a pause + sound each**, and it
**never clears the bricks** (no `SET 7`, no cell write). The bricks stay
on screen through the score tally; the subsequent `increment_round_number`
→ next-level load is what clears them.

## Parity verdict

| Part | Original | Port | Verdict |
|---|---|---|---|
| Rocket motion (accel, bat-attach) | `handling_rocket` $A89A | `step_rocket` accel block | **FAITHFUL** ✔ (byte-equiv; counter reset at launch confirmed) |
| Brick destruction during flight | **none** — flies over intact bricks | none — `step_rocket` has no cell loop | **FAITHFUL** ✔ |
| End-of-flight award | sequential tick-up, pause+sound/brick, bricks NOT cleared | `play_rocket_award_tally`: one brick per PIT tick, bricks stay visible | **FAITHFUL** ✔ |

**Both rows said DIVERGENT until 2026-08-09**, long after the work was
done. `award_left_bricks` — the instant clear-and-award they named — no
longer exists; the port sweeps row-major one brick per PIT tick with the
bricks on screen (`play_rocket_award_tally`), and clears the cells only
after the whole tally, which is what keeps the level advancing. The
in-flight bbox sweep is gone too: `step_rocket` touches no cells.

Found by `scripts/notes_symbols.py`, which lists identifiers the notes
cite and the tree no longer defines. A parity table that claims a
divergence which has been fixed is worse than one that is merely out of
date — it invites someone to "fix" it again.

## The two divergences — DONE (kept as the record of how they were planned)

Both divergences are long-standing port design choices (the tunnel sweep
predates the recent `a433417` "rocket clear" commit, which only reworked
the brick-flash *render*). Fixing them to match the original means:

1. **Remove the in-flight bbox sweep** so the rocket flies over intact
   bricks (drawn on top), matching `handling_rocket` + the destruction-free
   `LBB97` loop.
2. **Replace the instant clear-and-award** with a port of
   `add_points_for_left_briks`: a sequential per-brick score tally with a
   short pause + sound each, leaving the bricks on screen until the level
   transition.

This is held, not flipped autonomously, because:
- There is **no rocket-flight ground-truth capture** (the L3 snapshot has
  no rocket scenario), so the visual can't be frame-step gated the way the
  ball is — flipping blind violates validate-before-flip.
- `scripts/test_rocket_completion_no_ball.py` encodes the *current*
  (tunnel + instant-clear) behaviour; the rewrite must reconcile/replace
  that test.
- The sequential tally needs the frame-paced `pause_short` loop wired into
  the port's level-clear flow — non-trivial, and a clear visible behaviour
  change the user should greenlight.

**Next step to unblock:** capture a ZEsarUX rocket flight (drive the
original into rocket mode, dump frames) to confirm "flies over intact
bricks + end-of-flight sequential tally", then port both parts behind the
existing rocket test, replacing its expectations.

## Executable implementation plan + GT recipe (2026-06-05)

The decode is complete; this is the ready-to-run plan for when the
behaviour change is greenlit. Two code changes + one risk + the GT recipe.

### Change A — flight: fly over INTACT bricks (remove the tunnel)

In `step_rocket` (src/main.cpp), delete the bbox-sweep cell-destruction loop
(the `for (r...) for (c...)` that does `*cell |= 0x80` /
`brick_flash_spawn` / `try_spawn_bonus` while the rocket rises). Keep the
motion block, the `BAT_Y = rocket_y - 6` attach, and the fly-off branch.
After this the rocket sprite simply draws over the still-rendered bricks
(matches `handling_rocket` motion-only + the destruction-free `LBB97`).

### Change B — end: SEQUENTIAL tally, don't clear (port add_points_for_left_briks $AF0D)

Replace the instant clear-and-award with a frame-paced
sweep of the 15x12 grid (row-major, the original's `C=$0C` rows x `B=$0F`
cols):
- for each cell: if `!(cell & $A0)` (live, destructible) add
  `points_table[row]` (doubled if colour nibble >= 6) + a score sound;
- a short pause every cell (`pause_short D=$03` ~= 3 PIT ticks) so the
  score visibly ticks up; use the existing frame-pace primitive (the
  death-spark loop / `pit_ticks()` busy-wait is the template);
- do NOT set `cell |= $80` — leave bricks on screen; the next-level load
  clears them.

### THE RISK — level-advance gating

The instant award `|= $80`s every brick, which is likely what
drives the port's "round complete -> advance" (bricks_remaining hits 0)
and/or avoids the no-ball-death branch during the rocket sequence. If
Change B stops clearing, the advance may not fire (level hangs) or
no-ball-death may trigger. So Change B must ALSO ensure the round still
advances — either keep a final `bricks_remaining = 0` / explicit
`rocket_clear_completed` advance after the tally, or clear the cells only
AFTER the tally completes (just before the transition). Verify with
`make test-rocket-completion-no-ball`, `test-rocket-bonus`, and
`test-midgame-brick-replay` — these catch a broken advance / no-ball-death
(they are the regression guard; there is no pixel gate for the tally).

### GT recipe (to verify the visual, optional but recommended)

The rocket flight runs its OWN loop `LBB97` ($BB97), not the `$BA83` main
loop, so frame-step on **PC=$BB97** (not $BA83). To enter it: from the L3
`$BA83` state, poke a COHERENT rocket object (`get_rocket` builds it from
the bat: object_rocket $9B80-region: +0=$06, +2=bat_x+ ($04 or $0C), +4=
bat_y+6, sprite dims) — an incoherent poke hangs the original (see the
"scenario construction" notes). Then step on $BB97 and read the brick grid
each frame to confirm it stays INTACT, and read object_rocket+$04 for the
y-rise. Expect: bricks unchanged through the whole flight; at fly-off,
`add_points_for_left_briks` ticks the score with the grid still intact.

## Change B reassessed (2026-06-05): low-value, timing-unreproducible

Change A (fly over intact bricks) is DONE + gated. On Change B (the
sequential end-award), a closer look at `pause_short` changes the verdict:
it is a **CPU busy-wait** (`pause_short: LD E,$FF; DEC E/JR NZ; DEC D/JR
NZ`), so `D=$03` ≈ 765 Z80 iterations ≈ ~3 ms — NOT a frame delay. The
180-cell tally is therefore a brief **~0.5–1 s score-count blip**, not a
multi-second sequence, and its speed is tied to the Z80 clock — not
reproducible on the port's (faster) CPU without an arbitrary fudge factor.
Since Change A already awards the full score (correct total) and clears +
advances correctly, Change B would only add a sub-second, non-byte-exact
score-count animation at end-of-level. Net: low value + unreproducible
timing → deferred indefinitely (not worth the frame-paced-loop +
score-redraw machinery + the level-advance-gating risk). The rocket-clear
is effectively parity-complete after Change A.

## Change B IMPLEMENTED after all (2026-06-05, user-greenlit) — SUPERSEDES the deferral above

The deferral above was the *engineering* recommendation; the user
explicitly chose "Implement rocket end-tally" (AskUserQuestion), overriding
it for the visible end-of-level score-count behaviour. So Change B shipped:
`play_rocket_award_tally(level_idx)` + `redraw_level_scene` (defined after
`redraw_with_death_sparks` in src/main.cpp). It sweeps the 15×12 grid
row-major, awards `points_table[row]` (×2 if the colour nibble >= 6) per
live brick, paces **one PIT tick per brick** with the scene + score
redrawn each tick (bricks stay visible — matching `add_points_for_left_briks`
which never `SET 7`s), then clears every cell (`*cell |= 0x80`) at the end
to drive the round advance (resolving THE RISK above — the advance still
fires because the final clear zeroes bricks_remaining). The per-brick pace
uses PIT ticks rather than the original's CPU-busy `pause_short`, so the
tally speed is a deliberate non-byte-exact choice (the timing is
unreproducible, per the reassessment) — the *score total*, the
bricks-stay-visible render, and the row-major order ARE faithful. Gated by
`test-rocket-completion-no-ball`, `test-rocket-bonus`,
`test-rocket-flight-redraw`, `test-midgame-brick-replay` (now all in
`parity-check-full`). The rocket-clear is parity-complete (A + B).
