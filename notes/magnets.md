# Magnets — the runtime system

Magnets had been RENDERED since the GT capture work, but the entire runtime
system was missing: they never toggled and never touched the ball. Decoded
from `original/disasm/routines/magnets.asm` + `handling_ball`, and ported.
Gate: `make test-magnet-ball`. Level-paint appearance is in
`notes/per-level-profile.md`.

## Three pieces

### 1. Level paint and initial state (`print_magnets` $8D4C)

Per magnet in the level table (`magnet_level_NN` at $8E06): set slot
`+$01 = $06` (sprite ON), draw the ON sprite with its height SMC'd
`$17 -> $1E` = 30 so the bottom "spark" rows 23..29 paint too, then

    CALL random_generate / LD A,(random_number) / RRA / JR C,stay-ON

so the coin **advances the RNG once per magnet** and keeps the magnet ON when
bit 0 == 1. On bit 0 == 0 it sets `+$01 = $07` and draws the OFF circle over
the same origin — the second `print_obj_to_buff` reuses the buffer address
computed BEFORE the `+5` coordinate adjustments, so both sprites land at the
paint origin.

The slot's stored coordinates end up `(x0+5, y0+5)` with body 15x14
(`+$0C`/`+$0D`), which is the circle's physics box. The spark rows persist
under BOTH states for ever, because toggles redraw only the 23 circle rows.

### 2. Random toggle (the `LB9E8_2` gate + `print_one_magnet` $8E72)

At the main-loop top, BEFORE the frame's `random_generate`: if the
read-current `random_number+$01 == $99` (~1/256 per frame), call
`print_one_magnet`, which

- returns if `magnets_quantity == 0`, before any RNG use — so non-magnet
  levels never perturb the walk;
- picks a uniform slot by rejection (`CALL random_generate / AND $03`, retry
  while `> count-1`; each retry advances);
- XORs `+$01` with 1 and draws the toggled circle at the paint origin, ON
  drawn with its NATIVE height `$17` = 23 rows, circle only;
- plays `play_sound_magnet` ($C151, a blocking 24-step descending sweep).

`current_magnet_prop` flags the slot so `restore_objs_and_magnet` ($987A)
blits the 4-char x 23-px window from buffer to screen at the next frame top.

### 3. Ball capture, curve and release (`handling_ball` LA27E_0..11)

Each ball owns a 4-byte state block (ball 1 -> LA270, ball 2 -> LA274,
anything else -> LA278; cleared by `all_var_init` and on bottom-exit):

    +0 cooldown   +1 delta   +2 exit_dir   +3 magnet_idx

- **cooldown != 0**: decrement and skip everything — two frames of
  no-recapture after a release.
- **delta != 0 (captured)**: `dir = (dir + delta) & $3F`, so ±1/64 per frame.
  Recompute `exit = (dir + 2) & $3C`, and if `exit & $0F == 0` (a pure
  right/up/left/down code) nudge it ±4 (`-4` when `dir & $0C`, else `+4`), so
  the exit direction is always a multiple of 4. Then:
  - if the magnet is now OFF (`BIT 0,(IY+$01)`) or the ball left the box
    (`obj_compare` $AC22) -> **release**: cooldown=2, delta=0, dir=exit, and
    the frame continues at LA27E_23 with the normal move, `bounce_wall` and
    collisions;
  - else **stay captured**: move with the CURVED dir (`LAD69`) plus
    `check_margins` (CLAMP, no wall reflect), then run the collision chain
    with dir temporarily = exit, keeping the collision's dir if it changed and
    otherwise restoring the curved one.
- **free**: scan the slots, skipping OFF ones, first `obj_compare` overlap
  wins. `B = ((dir + $10) & $3F) >= $20 ? $FE : 0`; if
  `magnet_y_slot + 4 >= ball_y` (the ball is at or above the centre line)
  `B ^= $FE`; then `delta = $FF ^ B` (so `$FF` or `$01`) and store the index.
  The capture frame itself still moves normally — curving starts next frame.

`obj_compare` returns carry for overlap, with the original's asymmetry:
strict `<` against the IX (ball) body when the magnet origin is >= the
ball's, `<=` against the magnet body otherwise.

Direction encoding, for reading the above: $00=right, $10=up, $20=left,
$30=down.

## The port

- `magnet_count/px/py/on_state[4]` + `ball_mag_cool/delta/exit/idx[3]`.
  `magnet_level_init(level)` rolls the initial coins at level entry
  (`next_random() & 1` per magnet — an ADVANCE, not read-current) and clears
  the ball blocks. Test pin under `BATTYALL`: slots 0/1 ON, 2/3 OFF, no RNG,
  which keeps the state4 captures byte-stable.
  `BATTY_REPLAY_MAGNET=<hexmask>` forces the pattern for tests.
- `render_magnets` paints FROM the state, so mid-game static-background
  rebuilds no longer re-roll the look. The old code called the RNG inside
  render, which would have re-randomised on every full rebuild.
- `magnet_random_toggle()` is the `LB9E8_2` gate at the frame top
  (`random_d == 0x99`, read-current, sampled BEFORE the per-frame tick),
  pinned off under `BATTYALL`. It pushes `SND_MAGNET`, a queued
  approximation of the blocking sweep (`notes/sound.md`).
- `apply_magnet_toggle_visual()` is the deferred redraw, run inside
  `redraw_full_with_ball` right after the prev-dirty restore — where
  `scr_buff` is clean background, the original's frame-top point. It blits the
  toggled circle (ON via a height-23 header copy of `spr_magnet_on`, the
  original's native `$17`), bakes the 5-byte x 23-row window into
  `bg_scr_buff` since the magnet is part of the cached static background, and
  marks it dirty.
- `magnet_ball_frame(o, si)` in `step_ball`/`step_extra_ball` is the literal
  `LA27E_0..11` port, including the captured-frame "collide as exit-dir, keep
  only if changed" dance (`magnet_captured_move`).

Two branches are omitted as UNREACHABLE while captured, not as
simplifications: `LA27E_24`'s bat contact (the deepest box ends at y≈165,
below the y>=167 contact line) and the bottom-exit check (the boxes end far
above y=192). Ball-vs-enemy runs in the main loop regardless. State is
cleared on bottom-exit, extra-ball deactivation, respawn and level entry.

## The gate

`make test-magnet-ball` uses L2's single magnet (paint $74,$2C -> box x
114..136, y 43..63), which sits in an empty brick pocket (L2 rows 1..4, cols
6..8 are all `$C0`). It seeds the ball at (124, 64) dir $10 (up) speed 4 and
probes frame 10, over two boots differing only in `BATTY_REPLAY_MAGNET`:

    ON  (mask 1): x=115 y=47 dir=0x14 — captured, curved, released with a
                  quantized (multiple-of-4) dir != $10
    OFF (mask 0): x=124 y=48 dir=0x10 — arrow-straight, state empty

**Parity caveat.** The gate pins PORT behaviour — capture, curve direction,
quantized release, OFF inertness, ON/OFF gating. It is NOT a byte-exact
trajectory comparison against ZEsarUX, which would need an L2 aligned replay
seed (the harness aligns L3). The block was transcribed
instruction-by-instruction from the disassembly; for a byte-exact magnet
gate, build an L2 `$BA83` seed and reuse `capture-timeline-both` with a
magnet-pocket ROI.
