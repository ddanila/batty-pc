# Visual regression test

Once the original screen content is reproducible by *our* renderer, we
lock it in as a pixel-identical regression test.

## How `make test` works

1. **Build the floppy** via the normal Makefile path.
2. **Launch QEMU headless** with `-display none -monitor stdio`. The
   Python harness drives the monitor: sleep for DOS boot, `screendump`
   the framebuffer to a PPM, `sendkey` to advance our state machine,
   repeat, `quit`.
3. **Decode the captured PPM** for each checkpoint:
   - QEMU emits mode-13h frames at 2× scale (640×400, each VGA pixel
     doubled in both dimensions for aspect correction).
   - DAC scaling: `DAC=56 → 224`, `DAC=63 → 255` (plain `<<2` for
     non-bright, max-out for bright). Not the textbook `(v<<2)|(v>>4)`.
   - Sample one pixel per VGA cell, look up the RGB in our 16-entry ZX
     palette → palette-index byte.
4. **Build the expected indices** from the matching ZEsarUX snapshot's
   `screen.scr` via `extract_scr.py` — gives the same palette-index
   buffer.
5. **Compare in RGB space, not index space.** Indices 0 and 8 both
   render as `(0,0,0)` (non-bright vs bright black) — visually identical
   but `extract_scr` emits each per the ZX attr's bright bit. Comparing
   `PALETTE_RGB[a] == PALETTE_RGB[e]` makes them equivalent.
6. **Diff PNG on failure.** Mismatches are saved to
   `build/test_visual/<checkpoint>_diff.png` (red where pixels disagree,
   grey for background context).

## Current checkpoints

The test exercises the full attract-mode flow (TITLE → MENU → HISCORE → LEVEL)
on the `batty-test.img` floppy, which sets `BATTYALL=1` in AUTOEXEC so
the C side disables auto-advance — every transition is driven by
`sendkey ret` from the Python harness.

| State | Renderer in C            | Expected snapshot                  | Notes                                                                       |
|-------|--------------------------|------------------------------------|-----------------------------------------------------------------------------|
| 1     | `LOADING.BIN` static blit | `original/Batty.scr`              | Title / loading screen, decoded from the tape's screen$ block.              |
| 2     | Markup + sprites + blink   | `20260513T202041Z` (snap2)        | Main menu rendered from `MENUMARK.BIN` + indicators + bottom sprites.       |
| 3     | Markup hi-score            | `20260513T202038Z` (snap1)        | Hi-score table rendered from `MARKUP.BIN`.                                  |
| 4     | Full level-N gameplay paint | `build/level_gt/level_NN.scr` (modded-tape GT, *post-first-paint*) | Bricks + frame + bat + ball + lives + magnets. Pixel-identical for all 15 levels via `BATTY_LEVEL=N`. |
| 5     | Same captured frame, ROI'd to bat band (y=160..192) | same GT | Sub-diff of state4 — surfaces bat-render regressions on their own so they don't hide inside the whole-frame number. |

All five states are FAIL-gated on L1 default. All 15 level-entry
captures are pixel-identical via `BATTY_LEVEL=N` — see
[`per-level-profile.md`](per-level-profile.md).

`make test-brick-flash` drives a dynamic L3 gameplay path and fails if
the bright-white brick destruction flash remains after it should clear,
or if no brick-sized cell stays visibly removed after the hit. The
stale-flash decision is reference-derived: the test compares each brick
cell's bright-white coverage against the original-captured L3 render in
`build/level_gt/level_03.scr`, allowing only a small margin above the
original brick art. That catches the dirty-line white-block failure and
the stale-static-background failure without hard-coding that every white
pixel is wrong.

`make test-rocket-bonus` is a source-level regression for the rocket /
next-level bonus. The original main loop checks `object_rocket` before
`balls_quantity` (`LBAED -> LBAED_6`), so catching the rocket can hide
all balls while the level-clear sequence runs without entering the
bat-death path. The test fails if the port's no-ball death guard stops
excluding `rocket_active`.

`make test-rocket-flight-redraw` is a visual regression for the caught
rocket path. It seeds an already-attached rocket, lets `handling_rocket`
lift the bat for 18 rendered frames, and compares normal dirty redraw
against a forced full-flush baseline. It catches stale bat/rocket pixels
left behind when the bat's Y coordinate changes during level clear.

`make test-rocket-completion-no-ball` covers the next frame in that same
path: the rocket has left the playfield, the remaining bricks are
awarded, and the level-clear pause is about to begin. The port holds the
frame via `BATTY_HOLD_ROCKET_CLEAR` and compares the old bat/ball band
against a forced full-flush baseline, catching the regression where the
primary ball briefly reappeared after rocket flight completed.

`make test-round-banner-border` covers the "PLAYER 1 / ROUND NN" black
window. The original clears the 80x32 pixel window from coordinate
`$A458`, which maps to playfield x=88 with the top row at y=133 after
the upward `dec_scr_line` loop. The test holds the banner via
`BATTY_HOLD_ROUND_BANNER` and asserts that the top band is black, so the
window cannot slide down and lose its top edge again.

`make test-death-sparks` is a source-level regression for the bat death
fanout. It locks the port to the original `LBC10` spawn constants
(`$1B` direction seed, `$05` direction step, `$AE` Y, speed `$02`,
`bat_x + body_width/2 - $0C`, 3 px X spacing) and `bounce_wall`
reflection thresholds, especially the right wall clamp at `$F8 -
spark_body_width`. It also checks the `LAD13` signed direction math:
negative components are table-byte magnitudes negated after speed
multiplication, not `table_byte - 256`. Finally, it checks the post-spark
`pause_long B=$03` hold before the life is decremented and the bat
respawns.

`make test-l3-replay-seed` is a source-level guard for the deterministic
L3 brick replay. It parses `replays/l3-brick-flash.json` and the
Makefile env to keep the original and port seeds aligned: the ball
descriptor must be 22 bytes, direction `$1F`, launch/stuck counter
`$00`, Makefile `BATTY_REPLAY_BALL_OBJECT` must match the JSON seed,
and the original dynamic probe must read active L3 data at `$6E43`.
It also checks that `bricks_quantity` and original destroyed `$13`
markers are fail-gated.

`make test-midgame-brick-replay` is the first fail-gated dynamic replay
state test. It seeds L3 with an in-flight ball near destructible bricks,
runs the DOS port through the replay harness, then checks the extracted
post-run probe: brick count must drop below the seeded `$1A`, score must
increase, RNG must advance, and the level copy must contain a destroyed
`$13` brick marker. It also guards the primary ball against the old
integer-velocity failure mode where the replay dropped and respawned the
ball at `$84,$A6` instead of keeping the seeded descriptor-motion ball in
play. `make replay-l3-brick-flash-both` also runs the
original side and is fail-gated on stable two-runner invariants: both
runners must be at L3, both must reduce brick count to the same value,
and both active level buffers must contain destroyed `$13` cells. Exact
moving-object rows and exact destroyed-cell positions remain
informational because the current seed still exposes a collision-geometry
mismatch rather than byte parity.

`make test-hud` is a separate normal-build check because `make test`
uses `BATTY_SCORELESS_HUD`. It boots the regular floppy to L1 and
compares the stable original HUD regions (`1UP` / `HI` / `2UP`, player
1 zero score, player 2 zero score) against the original
`20260513T202101Z` capture. The high-score digits are intentionally
excluded because `HISCORE.DAT` can vary between local runs.

`make test-bat-redraw-window` covers the narrow bat-only refresh path.
It runs the same hidden-ball bat movement twice: once with the normal
byte-window repaint and once with `BATTY_FORCE_BAT_FULL_REDRAW=1`.
The gate compares the bat band, catching stale pixels from a too-tight
bat refresh window without requiring a hand-authored expected screen.

`make test-ball-dirty-redraw` covers the primary-ball-only dirty redraw
path. It seeds a ball in open play, halts after 12 rendered frames via
`BATTY_VISUAL_PROBE_FRAMES=12`, and compares the whole playfield against
a forced full-redraw baseline using `BATTY_FORCE_BALL_FULL_REDRAW=1`.
That catches stale old-ball pixels and missing new-ball pixels while
keeping brick hits, HUD updates, bonuses, enemies, bullets, rockets, and
extra balls on the conservative full-compose path.

`make test-ball-object-dirty-redraw` covers the next dirty redraw tier:
primary ball plus simple moving objects. It seeds a UFO/bird object and
compares the dirty-object renderer against the same forced full-redraw
baseline. Bombs, rockets, bullets, brick animations, extra balls, HUD
changes, and bat motion are still excluded from this path.

`make test-normal-ball-launch` covers the regular player launch path
that the seeded L3 replay does not exercise. It boots directly into L1,
uses `BATTY_REPLAY_WAIT_KEY=1` to pause after level entry, presses SPACE,
and uses `BATTY_LAUNCH_FRAMES=12` to stop after exactly 12 primary-ball
steps before reading `PROBE.TXT`. The gate asserts that the normal launch
records the original bat-derived `$34` descriptor direction at the
on-bat position, then resolves into an up/right trajectory rather than
wrapping to the upper-left. This catches regressions where primary-ball
movement reads `object_ball_1.dir/speed` but SPACE or timeout launch
updates only the legacy integer `ball_dx/ball_dy` side state, and the
16-bit DOS fixed-point overflow class where `x << 8` wraps for x >= 128.

`make test-levels-sweep` FAIL-gates state4 for every one of the 15
levels (a `BATTY_LEVEL=N make test` boot each). `make test` alone gates
only the default L1; the other levels used to be INFO-only, which hid
the L3/L9 alien-race artefact for an unknown stretch of the campaign
(see notes/per-level-profile.md "RESOLVED (2026-06-11)"). Slow (15 QEMU
boots) — wired into parity-check-full. Related pin: BATTYALL disables
NATURAL alien spawns (enemy_prepare early-return) so the level-entry
captures are deterministic; tests seed aliens via
BATTY_REPLAY_ENEMY_OBJECT.

`make test-magnet-ball` covers the magnet ball physics (handling_ball
LA27E_0..11). It boots L2 twice with a ball seeded inside the magnet's
empty brick pocket aimed straight up, forcing the magnet ON in one run
and OFF in the other via `BATTY_REPLAY_MAGNET`, and asserts the ON run
curves/releases the ball (dir leaves the seeded $10) while the OFF run
flies arrow-straight. See notes/magnets.md.

`make test-brik-anim-pace` covers the level-intro all-metal-bricks
shimmer pace (all_metal_briks_animation_snd $B765). The floppy is built
with `BATTY_TEST_KEY_BEFORE_ANIM=1` so the port stuffs an ENTER into the
BIOS keyboard buffer right before the animation; the gate asserts the
PROBE.TXT `brik_anim_ticks` duration is >= 16 PIT edges (8 frames x 2
full edge waits) and that the key survives the animation (it must
release BATTY_REPLAY_WAIT_KEY — the old abort-on-any-key code consumed
it at frame 0). See notes/metal-shimmer.md.

`make test-ball-left-wall-escape` covers a seeded primary-ball wall
reflection case that is hard to reach reliably through normal input. It
starts the ball near the left wall with an up-left descriptor and stops
after 12 primary-ball steps via `BATTY_FRAME_PROBE=12`. The gate asserts
that the ball has moved back into the field and no longer points left,
catching descriptor-reflection regressions that can otherwise leave the
ball jiggling between the wall and bat.

`make test-ball-no-tunnel` is the collision-invariant sweep (known-bugs #6
class). For each level it boots once to read the initial brick grid
(`current_level_copy`), picks solid target bricks, then for each
(target x approach x speed) seeds the primary ball one step away aimed into
the brick (via `BATTY_REPLAY_BALL_OBJECT` + `BATTY_FRAME_PROBE`), runs N
frames, and asserts the *invariant*: a ball aimed into a still-solid brick
must change that brick's state (destroy / half-hit / count drop) OR reverse
direction; if it crosses the brick's far edge while still overlapping its
column AND nothing changed, it tunneled → FAIL. ZEsarUX-free. The default
subset (`NO_TUNNEL_ARGS`) covers L1/L5/L7 — L5/L7 have row-0 metal bricks
against the top boundary, the exact #6 repro (a downward ball passed
through them before the LAFFC edge-face fix). `FULL=1` runs all 15 levels x
speeds 2/4/6 x straight+diagonal approaches (wired into parity-check-full).
This catches the whole class of "ball passes a solid brick unhit" across
the primary ball path regardless of root cause.

`make test-enemy-attr-parity` is the sprite-attribute-parity gate
(known-bugs #7 class). The original draws moving objects with
`print_obj_to_buff` ($B82C) — PIXELS only, never `print_sprite_attrib` —
so a flying enemy leaves every cell's attr untouched and renders in ZX
colour-clash (bg over texture, the BRICK's attr over bricks). The gate
seeds the deterministic L3 fresh bird (test_enemy_descend's descend) so its
footprint overlaps L3's row-0/1 brick cells, dumps `attr_buff` and
`bg_attr_buff` (the static-background snapshot) via the probe, and asserts
they are EQUAL across the sprite's footprint — i.e. the enemy changed no
cell attr. ZEsarUX-free; the original's behaviour was confirmed by an
oracle attr read (build/orig_flyover, notes/bird-render-parity.md).

## Per-level testing via `BATTY_LEVEL` env

```sh
BATTY_LEVEL=9 make test    # builds floppy with SET BATTY_LEVEL=9 in AUTOEXEC,
                           # boots into level 9 directly, diffs against L9 GT
```

The `BATTY_LEVEL=N` env var:
- Makefile injects `SET BATTY_LEVEL=N` into the test floppy's AUTOEXEC.BAT
  (the bytes don't change with env, so the `test` target also `rm -f`s the
  floppy first to force a rebuild on env changes).
- Makefile also passes `BATTY_START_LEVEL=1` through for replay targets;
  when set, the DOS port starts directly in `ST_LEVEL` after asset load.
- `src/main.c` `getenv("BATTY_LEVEL")` in `run_level` sets
  `round_number = N-1` so the run-level loop enters at level N.
- `scripts/test_visual.py` switches `state4_level1`'s expected snapshot
  to `build/level_gt/level_NN.scr` (default = L1).

## INFO is for accepted drift, not unmeasured surface

State 4's *original* GT was captured before the gameplay loop drew bat /
ball / lives — so the renderer's bat sat in a regression-test blind
spot. Diff stayed at ~228 px (the bat-overlay overhead), rationalized
as "the absolute floor without recapturing the GT mid-render". A green
check on a metric that excluded the surface under iteration. Fixed by:

- `scripts/build_modded_batty.py` patches line 6261 (`CALL
  restore_objs_and_magnet` → `JR $`) so the spin trap fires *after*
  the first gameplay-loop iter has painted bat / ball / lives into
  scr_buff and flushed to VRAM. Trap PC: **0xBB61**.
- `scripts/test_visual.py` adds `state5_bat_band` — same captured
  frame, ROI'd to `y=160..192`. ROI-only checkpoints reuse another
  state's PPM via the `source_label` field.

**Rule of thumb.** A residual diff that's always the same shape (a
band, a sprite, a strip) is a signal that the metric is excluding the
surface where you're iterating. Either:

- recapture a GT that covers it, or
- split it into its own ROI checkpoint with its own number.

`INFO` should mean "we accept this drift while we focus elsewhere",
not "the test can't see this region". The latter is just an alibi.

### Determinism for the menu checkpoint

The menu has an active blink: by default `selected_mode = 1` (= "1 -
1 PLAYER") and that line strobes white ↔ invisible at ~4.5 Hz. snap2
was captured during the BLACK half (selected row's 11 attr cells at
`0x58AE..0x58B8` are all `0x00`).

To keep the test pixel-identical regardless of capture timing, the C
helper `blink_phase()` pins the phase to 0 (BLACK) when `auto_advance`
is off (= the test mode signalled by `BATTYALL`). `make run`'s floppy
leaves `BATTYALL` unset and the user sees the natural blink.

## Mid-game parity gate (frame-step)

`make test` covers static screens. Mid-game ball physics / collision is
covered by the **frame-step gate** (`notes/replay-harness.md`,
`notes/laffc-decode.md`):

- `make capture-timeline-both [LAFFC_FLAG=1]` — frame-steps the port and
  ZEsarUX from a byte-identical L3 `$BA83` start and diffs each frame in
  the brick-play ROI (RGB palette space). Frame 0 is 0 px (aligned
  start); with `LAFFC_FLAG=1` (the byte-exact `LAFFC` collision path) the
  residual is purely the cosmetic brick-hit render.
- `make gate-laffc-long` — same, over 40 frames (residual stays bounded;
  the ball stays in lockstep with the Spectrum).
- `make test-laffc-ball-frame1` — **ZEsarUX-free** regression asserting
  the L3 frame-1 ball object equals the Spectrum probe
  (x=0x69 xf=0x09 y=0x41 yf=0x48 dir=0x21); guards the whole exact-motion
  + collision chain, suitable as a fast CI parity guard.

Ball motion and collision (cell / axis / position / direction) are
byte-exact vs the Spectrum. What still differs frame-by-frame is the
**brick-hit render** (the damaged multi-hit brick frame + the
`briks_data` shimmer) — cosmetic and shared by both collision paths.

The reusable replay harness is in `scripts/replay_harness.py`; see
[`replay-harness.md`](replay-harness.md). It supports DOS-port and
ZEsarUX-original runs plus INFO comparisons; replays become fail-gated
once their spec marks the original and port start states aligned. Ad-hoc
smoke scripts under `scripts/exercise_*.py` cover individual scenarios.
