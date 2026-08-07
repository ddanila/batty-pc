# Magnets — runtime system decode + port (known-bugs #5)

The port had rendered magnets at level paint since the GT capture work,
but the entire RUNTIME system was missing: magnets never toggled and
never touched the ball. Decoded 2026-06-11 from
`original/disasm/routines/magnets.asm` + `batty.asm` (handling_ball);
ported the same day. Gate: `make test-magnet-ball`.

## Original anatomy

Three pieces, all driven from the main play loop:

### 1. Level paint + initial state (`print_magnets` $8D4C)

Per magnet (level table `magnets`/`magnet_level_NN` at $8E06): slot
`+$01 = $06` (sprite ON), draw ON sprite (height SMC'd $17→$1E=30 so the
bottom "spark" rows 23..29 paint too), then `CALL random_generate /
LD A,(random_number) / RRA / JR C,stay-ON` — the coin **advances the RNG
once per magnet** and keeps the magnet ON when bit0==1; on bit0==0 it
sets `+$01 = $07` and draws the OFF circle over the same origin (the
second `print_obj_to_buff` reuses the buffer address computed before the
`+5` coordinate adjustments, so both sprites land at the paint origin).
The slot's stored coords end up `(x0+5, y0+5)` with body 15×14
(`+$0C/+$0D`) — the circle's physics box. NOTE: the spark rows persist
under BOTH states forever (toggles redraw only 23 circle rows).

(rng-model.md previously classified this coin as read-current of
`random_number+$01` — that was wrong on both counts; corrected there.)

### 2. Random toggle (`LB9E8_2` gate + `print_one_magnet` $8E72)

Main-loop top, BEFORE the frame's `random_generate`: if the read-current
`random_number+$01 == $99` (~1/256 per frame), `print_one_magnet`:
returns if `magnets_quantity == 0` (before any RNG use — so non-magnet
levels never perturb the walk); picks a uniform slot by rejection
(`CALL random_generate / AND $03`, retry while `> count-1` — each retry
advances); XORs `+$01` with 1; draws the toggled circle sprite at the
paint origin (ON drawn with its native height $17 = 23 rows — circle
only); plays `play_sound_magnet` ($C151, a blocking 24-step descending
sweep). `current_magnet_prop` flags the slot so
`restore_objs_and_magnet` ($987A) blits the 4-char × 23-px window
buffer→screen at the next frame top.

### 3. Ball capture/curve/release (top of `handling_ball`, LA27E_0..11)

Each ball owns a 4-byte state block (ball1→LA270, ball2→LA274, anything
else→LA278; cleared by `all_var_init` and on bottom-exit):

    +0 cooldown   +1 delta   +2 exit_dir   +3 magnet_idx

- **cooldown != 0**: decrement, skip everything (2 frames of
  no-recapture after a release).
- **delta != 0 (captured)**: `dir = (dir + delta) & $3F` (±1/64 per
  frame); recompute `exit = (dir+2) & $3C`, and if `exit & $0F == 0`
  (a pure right/up/left/down code) nudge it `±4` (`-4` when
  `dir & $0C`, else `+4`) — exit is always a multiple of 4. Then:
  - if the magnet is now OFF (`BIT 0,(IY+$01)`) or the ball left the box
    (`obj_compare` $AC22) → **release**: cooldown=2, delta=0, dir=exit,
    and the frame continues at LA27E_23 (normal move + bounce_wall +
    collisions).
  - else **stay captured**: move with the CURVED dir (LAD69) +
    `check_margins` (CLAMP, no wall reflect), then run the collision
    chain (LA27E_24: bat LAB1F, bricks LAFFC, enemy) with dir
    temporarily = exit; keep the collision's dir if it changed,
    otherwise restore the curved dir. Return.
- **free**: scan the slots (skip OFF; first `obj_compare` overlap wins):
  `B = ((dir+$10) & $3F) >= $20 ? $FE : 0`; if
  `magnet_y_slot + 4 >= ball_y` (ball at/above the centre line)
  `B ^= $FE`; `delta = $FF ^ B` (= $FF or $01), store idx. The capture
  frame itself still moves normally — curving starts next frame.

`obj_compare` carry=overlap, with the original's asymmetry: strict `<`
against the IX (ball) body when the magnet origin is ≥ the ball's, `<=`
against the magnet body otherwise.

Direction encoding (empirical, from the gate runs): $00=right, $10=up,
$20=left, $30=down.

## Port (src/main.cpp)

- `magnet_count/px/py/on_state[4]` + `ball_mag_cool/delta/exit/idx[3]` —
  the runtime state; `magnet_level_init(level)` at level entry rolls the
  initial coins (`next_random() & 1`, per magnet — advance, not
  read-current; bit0==1 = ON) and clears the ball blocks. Test pin
  (BATTYALL): slots 0/1 ON, 2/3 OFF, no RNG — keeps the state4 captures
  byte-stable (same pixels as the old render-time pin).
  `BATTY_REPLAY_MAGNET=<hexmask>` forces the initial pattern for tests.
- `render_magnets` now paints FROM the state (ON sprite always, OFF
  overlay for off slots) — so mid-game static-background rebuilds no
  longer re-roll the magnet look (the old code called the RNG inside
  render, which would have re-randomized on every full rebuild).
- `magnet_random_toggle()` — the LB9E8_2 gate runs at the main-loop
  frame top (`random_d == 0x99`, read-current, sampled BEFORE the
  per-frame `next_random()`), pinned off under BATTYALL. Pushes
  SND_MAGNET (queued approximation of the $C151 sweep, per the
  PC-speaker policy in parity-gaps.md).
- `apply_magnet_toggle_visual()` — the deferred redraw, run inside
  `redraw_full_with_ball` right after the prev-dirty restore (scr_buff
  is clean background there, the original's frame-top point): blits the
  toggled circle (ON via a height-23 header copy of `spr_magnet_on` —
  the original's native $17; OFF natively 23) at the paint origin into
  scr_buff, bakes the 5-byte × 23-row window into `bg_scr_buff` (the
  magnet is part of the cached static background), and marks it dirty
  for the flush.
- `magnet_ball_frame(o, si)` in step_ball/step_extra_ball (si 0/1/2 =
  the LA270/LA274/LA278 blocks) — the literal LA27E_0..11 port,
  including the captured-frame "collide as exit-dir, keep only if
  changed" dance (`magnet_captured_move`). Omitted as unreachable while
  captured: LA27E_24's bat contact (deepest box ends y≈165 < the y≥167
  contact line) and the bottom-exit check (boxes end far above y=192);
  ball-vs-enemy runs in the main loop regardless. State cleared on
  bottom-exit / extra-ball deactivation / respawn / level entry
  (all_var_init + LA27E_25 equivalents).

## Gate

`make test-magnet-ball` (scripts/test_magnet_ball.py): L2's single
magnet (paint $74,$2C → box x 114..136, y 43..63) sits in an empty brick
pocket (L2 rows 1..4, cols 6..8 = $C0). Seed the ball at (124, 64)
dir $10 (up) speed 4, probe at frame 10, two boots differing only in
`BATTY_REPLAY_MAGNET`:

    ON  (mask 1): x=115 y=47 dir=0x14 — captured + curved + released
                  with a quantized (multiple-of-4) dir != $10
    OFF (mask 0): x=124 y=48 dir=0x10 — arrow-straight, state empty

Parity caveat: the gate pins PORT behaviour (capture, curve direction,
quantized release, OFF inertness, ON/OFF gating). It is NOT a byte-exact
trajectory comparison against ZEsarUX — that would need an L2 aligned
replay seed (the existing harness aligns L3). The block was transcribed
instruction-by-instruction from the disasm; if a byte-exact magnet gate
is ever wanted, build an L2 `$BA83` seed and reuse
`capture-timeline-both` with a magnet-pocket ROI.
