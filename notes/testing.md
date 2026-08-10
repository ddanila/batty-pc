# Visual regression test

> **`make test-fast` needs no emulator and runs in seconds** — 14 host
> test suites plus 29 source gates. Start there; it is also exactly what
> CI runs. `make test-video` is one of those suites: it compiles the
> video engine (`src/zxvga.cpp`) with the host compiler and checks the
> ZX attribute/colour-clash model exhaustively — every attr x every byte
> — in milliseconds. See [`video-engine.md`](video-engine.md).
>
> The QEMU gates below cost ~10 s per boot; `make parity-check-parallel`
> runs all 77 gates of the full sweep in about six minutes (76 QEMU
> plus `test-asan`, which is host-only but belongs to the same sweep).
>
> (This paragraph read "`make test-video` is the one gate here that
> needs no emulator" until 2026-08-09. That was true when zxvga was the
> only host suite, and became less true with every suite added since.)

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

`make test-ball-paths-no-tunnel` extends the no-tunnel invariant to the
NON-primary collision paths (known-bugs #6's secondary leads):
`step_extra_ball` (multiball) and `magnet_captured_move` (a ball captured +
curved inside an ON magnet). Both call the same `laffc_collision(); if
(hit==0) brick_collision()` the primary uses — the path the #6 edge-mask fix
lives in. PROBE.TXT now dumps `object_ball_2/3`; the gate seeds multiball
(L1) + a magnet-ON ball (L2 pocket) and asserts, at several probed frames,
that no ACTIVE ball's CENTRE sits inside a solid brick cell (a bounced ball
snaps to the cell edge — centre outside). Wired into parity-check-full.

`make test-sprite-attr-parity` generalizes the #7 attr invariant to ALL
moving sprites: a falling bonus / enemy bomb / laser bullet seeded over the
L3 brick band (primary ball stuck, so nothing hits a brick) must leave
`attr_buff == bg_attr_buff` for every one of the 768 cells. Locks "moving
objects blit pixels only, never recolour" for the sprites beyond the enemy.

The `test-ball-no-tunnel` sweep also carries a **field-bounds invariant**:
the ball must never escape the playfield walls (x in [8,244], y >= 8) — a
reflect/collision bug that pushed it past a wall fails across the whole
sweep matrix.

`make test-laffc-ball-l5-metal` broadens the byte-exact ball-vs-brick oracle
BEYOND L3. The L3 frame-step gate couldn't see #6 (it lived on the
edge-metal levels L5/L7). The original can be driven to ANY level — poke the
level counter `$B7EA` to the level index and set PC=`$BA24` (= `briks_calc`
level-init, loads that level's bricks), then run to `$BA83` (per
`capture_levels_modded.py`; replays/l5-entry.json + l5-metal-ball.json). This
gate seeds the #6 scenario on L5 (ball down into the row-0 col-0 metal
brick) and asserts the port's trajectory equals the ORIGINAL's captured via
ZEsarUX (f1 x12 y25 dir0x30 — bounce up; f3 up; f5 top-wall bounce). The
port matches byte-exact, so this is a fast ZEsarUX-free regression that locks
the #6 fix against the real game. The same poke-`$B7EA`+`$BA24` recipe
generalizes to any level for future oracle gates.

`make test-gameplay-soak` is the long-run invariant soak: it drives an
in-flight ball through sustained play on L1/L3/L5/L9 and samples checkpoints
(30..150 frames), asserting per-checkpoint that the ball's centre is never
inside a solid brick and never escapes the walls, and across checkpoints
that brick count only falls and score only rises. These hold whether the
ball is bouncing or has dropped+respawned, so it needs no ball-pinning.
Catches over-time / accumulation regressions a single-frame gate can't. It
uses the BATTY_SERIAL_PROBE deterministic frame wait — required here because
its 20 concurrent per-case boots oversubscribe cores, which the old
wall-clock waits couldn't survive (they read the pre-gameplay seed state and
produced false "bricks rose / score fell" violations).

## Every gate, and what it is for

Kept complete by `scripts/check_gate_index.py`, which fails if any of
the THREE places gates are defined — `run_gates_parallel.py`, the
`test-source-gates` recipe, and `parity-check-full` — names one this
section does not. A gate nobody can find
is a gate nobody reasons about — before this list existed, 30 of 59 were
mentioned nowhere in this file, including several of the oldest.

**The four-state cycle and the levels**
- `test` — the MENU/TITLE/HISCORE/LEVEL screens against captured originals.
- `test-levels-sweep` — the same state-4 check for each of the 15 levels.
- `test-laffc-levels-sane` — every level's grid loads and paints sanely.
- `test-level-advance` — clearing a level advances the round; the index wraps at 15.
- `test-hud` — score, lives and round digits in the HUD band.

**Ball physics**
- `test-normal-ball-launch` — the launch trajectory from the bat.
- `test-wall-bounce`, `test-ball-left-wall-escape` — side walls, including the clamp.
- `test-bat-deflection` — the LAB1F deflection table against captured ground truth.
- `test-ball-speed-ramp` — the speed-up over a rally.
- `test-ball-no-tunnel`, `test-ball-paths-no-tunnel` — the ball never passes through a brick.
- `test-laffc-ball-frame1`, `test-laffc-ball-l5-metal` — byte-exact ball-vs-brick, oracle-confirmed.
- `test-magnet-ball` — capture, curve and release.
- `test-stuck-ball-offset` — one function decides where a held ball rests (#12).
- `test-ball-sign-cache-owner` — only the primary ball writes its own sign cache (#13).

**Bricks and scoring**
- `test-brick-scoring` — points per row, doubled for metal.
- `test-brick-flash`, `test-brik-anim-pace` — the hard-brick shimmer and its cadence.
- `test-midgame-brick-replay` — a mid-game grid against the original.
- `test-l3-replay-seed` — the L3 seed the oracle gates depend on.
- `test-midgame-brick-replay` — a mid-game grid against the original (ZEsarUX oracle).
- `test-frame-step` — the byte-exact frame-step oracle (ZEsarUX).
- `test-gameplay-soak` — a long multi-level run holding every invariant.

**Enemies**
- `test-enemy-descend`, `test-enemy-steer` — the entry slide and the turn cadence.
- `test-enemy-anim` — the LAAD2 sprite walk.
- `test-enemy-attr-parity` — the alien's attribute cells.
- `test-enemy-flyover-redraw`, `test-enemy-brick-residue` — no residue when it passes over.
- `test-enemy-brick-walk` — it hits a brick: nothing breaks, and it walks to the snap point.
- `test-enemy-margin-clamp` — it reaches a wall: clamped, not bounced.

**Weapons, bonuses, the rocket**
- `test-bullet-fly`, `test-laser-cadence`, `test-bullet-blast` — the laser and its hit.
- `test-bomb-fall`, `test-pts400-fall`, `test-bonus-fall` — the three falling objects.
- `test-bonus-drop`, `test-bonus-typepick` — when a bonus drops and which one.
- `test-bonus-effects`, `test-bonus-effects2` — every bonus effect.
- `test-rocket-bonus`, `test-rocket-flight-redraw`, `test-rocket-completion-no-ball` — the level-clear rocket.
- `test-death-sparks` — the bat explosion.
- `test-round-banner-border` — the PLAYER/ROUND window's top band.

**Dirty-redraw A/B (narrow path must equal full path)**
- `test-ball-dirty-redraw`, `test-ball-object-dirty-redraw`, `test-stuck-ball-dirty-redraw`
- `test-bat-redraw-window`, `test-bat-fire-dirty-redraw`
- `test-bullet-dirty-redraw`, `test-bomb-dirty-redraw`, `test-blast-dirty-redraw`
- `test-multiball-dirty-redraw`, `test-bigball-dirty-redraw`
- `test-sprite-attr-parity` — no sprite corrupts an attribute cell.
- `test-visual-checkpoints` — the multi-checkpoint capture path itself.

**Game flow**
- `test-life-loss` — losing a life removes exactly one indicator.
- `test-game-over`, `test-game-over-visual` — the sequence's order, and the screen.
- `test-name-entry-visual` — the NEW HIGH SCORE name entry.

**Structural (no emulator)**
- `test-gate-greps` — the gates' source needles still match the source.
- `test-gate-index` — every gate is named in this section.
- `test-switch-defaults` — each debug switch's documented default
  matches the initialiser.
- `test-gate-freshness` — no gate can be satisfied by a previous run's
  captures or PROBE.TXT.
- `test-shimmer-one-pass` — the metal-brick shimmer plays one pass
  (known-bugs #3); no QEMU gate covers it.
- `test-multiball-source` — the multiball spawn reads the primary's dir
- `test-menu-start` — key 0 in the menu starts a game; ENTER still walks the attract chain.
- `test-kinnock` — the easter egg's text, coordinates and placement.
- `test-floppy-assets` — the image carries exactly what the port loads.
- `test-frame-derivable` — the frame's top and side strips are tape sprites, not capture.
- `test-bg-tile-derivable` — the hex tile and bg_attr_per_cycle are tape data.
- `test-asset-provenance` — every loaded asset is built from the tape.
- `test-sound-ids` — effect ids are play_sounds_list positions.
- `test-known-bugs-table` — the bug table agrees with the sections below it.
- `test-no-dead-constants` — every `#define` in src/ is used.
- `test-no-orphan-gates` — every gate script is run by a target.
- `test-plan-table-fresh` — PLAN.md's definition-of-done table is no
  older than the newest dated workstream entry.
- `test-level-attrs-derivable` — the live-brick fifth of level_attrs.bin is computed, not captured.
- `test-two-player-state` — two sets of counters, and the HUD/cache both read them.
- `test-two-player-turn` — a life loss hands the turn over in mode 1, not in mode 0.
- `test-double-play-court` — mode $02 moves both bats and draws the divider.
- `test-double-play-bat2` — bat 2 deflects the ball and takes ownership of it.
- `test-double-play-input` — the split keyboard steers one bat each, and
  the court clamps hold the divider.
- `test-double-play-alien-kill` — bat 2 kills the alien; the 350 lands
  on 2UP.
- `test-double-play-bat2-catch` — bat 2 catches the ball and holds it on
  its own bat.
- `test-secondary-ball-catch` — a MAGNET bat holds a secondary ball too.
- `test-extra-ball-bat2` — a secondary ball meets bat 2 in Double Play.
- `test-extra-ball-owner` — the owner bit is per ball; a deflection moves
  only the ball it hit.
- `test-double-play-bonus-catch` — bat 2 catches a falling bonus and is
  paid for it.
- `test-double-play-bat2-redraw` — bat 2's sprite tracks its object (a
  PIXEL gate; the object was always right).
- `test-double-play-bat2-width` — BIG_BAT widens the bat that caught it.
- `test-double-play-bat2-laser` — player 2's laser leaves bat 2.
- `test-double-play-bat2-gun` — an armed bat 2 draws the gun body.
- `test-stuck-auto-launch` — a held ball launches itself after 192 ticks.
  byte, and `delta_to_dir` has no production caller (known-bugs #14).
- `test-doc-links` — every file path cited in a comment or note exists.
- `test-host-tests-wired` — every host suite runs under `make test-fast`.
- `test-asan` — the same 14 suites rebuilt under ASan + UBSan. Not a
  source gate but listed here because it is wired into the full sweep:
  nine memory-safety defects had each needed a bespoke fixture, and two
  of them were invisible to a normal host build. It found a real
  out-of-bounds read in `replay_parse_hex_bytes` on its first run.
- `test-notes-numbers` — the plan's status block still states true numbers.
- `test-env-passthrough` — every `BATTY_*` knob reaches DOS on the test floppy.
- `test-frozen-clock` — nothing times anything with `bios_ticks()` (#15).
- `test-module-ownership` — a module that declares state defines it.
- `test-invariant-owners` — two-place state changes have one writer each.
- `test-rng-walk` — the RNG walk matches the original's sequence.

## Mutation testing (`scripts/mutate.py`)

A green test suite says the tests pass, not that they would fail if the
code were wrong. Asking the second question found five real gaps here:
`check_gate_greps` skipping a third of the gates, `test_objects` and
`test_weapons` pinning the shape of a value rather than the value, and
`zxvga` never checking where a sprite lands.

    scripts/mutate.py <file> <find> <replace> <make-target> [label]

    exit 0  caught     — the tests failed, which is the good outcome
    exit 1  SURVIVED   — a gap, or an equivalent mutant. Decide which.
    exit 2  ERROR      — the substitution matched nothing

Doing this by hand went wrong three ways, twice producing a confident
false result, so the script handles all three:

- **Stale binary, same second.** Restoring a source within the same
  second as the last build leaves the timestamp unchanged, `make` reruns
  the OLD binary, and the mutation looks caught.
- **Stale DOS EXE.** The QEMU gates boot `build/batty-test.exe`. A
  module change rebuilds its own `.obj`, but if the link lands inside
  the same filesystem second the EXE is left alone — mutating
  `src/physics.cpp` changed `physics-test.obj` (md5-verified) and left
  `batty-test.exe` byte-identical, so the gates ran the ORIGINAL code.
  Every QEMU-gate result from that run was meaningless. `mutate.py`
  deletes `build/*.obj` and `build/batty*.exe` too.
- **Stale binary, wrong name.** `make test-video` builds
  `build/test_zxvga`. Deleting `build/test_video` deletes nothing. Every
  run then used a stale binary and a REAL gap was reported as caught;
  it surfaced only because a restored source still failed, which cannot
  happen. The script deletes every `build/test_*` file rather than
  guessing, leaving directories alone (the QEMU gates keep captures
  there).
- **Silent no-op.** A substitution matching nothing leaves the source
  clean, the test passes, and it reads as "not caught" — the worst
  outcome, because it looks like a finding.

## Reading the original (`scripts/disasm.py`)

    scripts/disasm.py handling_bird     # by label
    scripts/disasm.py 0xA67B            # by address, via "; Routine at XXXX"
    scripts/disasm.py margin -l         # list labels containing a substring

`original/disasm/batty.asm` answers questions that otherwise cost
emulator runs, and it settled three in one week: whether the enemy is
reflected at a wall (no — `check_margins` clamps, `bounce_wall`
reflects, and the enemy gets the first), whether the multiball spawn
reads a velocity (no — it reads the dir byte), and what the bat resize's
gating actually is (every other frame, which the port already matched).

Each of those started as a plausible guess that turned out wrong. The
friction was part of it: reading a routine meant
`sed -n "$(grep -n '^name:' ...)"` and counting lines. Now it does not.

A routine prints from its label to the next one, with the following
routine's comment header trimmed off. Mid-routine entry points
(`LA67B_8` and the like) are labels too, so they print just their own
stretch.

### One class that is NOT gated, and why

Comments that duplicate an explanation and then drift have caused four
real problems: the bricks header copied into its `.cpp`, two blocks in
front of the SPACE handler, the RNG default saying OFF after it flipped,
and a bat-resize note claiming "roughly matches" long after the gate
that made it exact. The last one cost an afternoon chasing a bug that
was not there.

That looks gateable. It is not, and the reasons are worth recording so
the next person does not build the gate and trust it:

- **Exact-sentence matching finds nothing.** A scan for identical
  sentences across and within `src/*.{cpp,h}` currently reports ZERO.
  The four real cases were PARAPHRASES — "roughly matches the original's
  2-px-every-other-frame" versus "every-2-ticks gives the original's
  1 px/frame". A sentence gate would have been green for every one of
  them while feeling like coverage.

- **Provenance-address co-citation is too noisy.** 47 original
  addresses are cited from two or more comment blocks. Nearly all are
  legitimate: a declaration summary plus an implementation detail, or
  several call sites naming the routine they port. `$A67B` alone appears
  in four places, all correct. A gate here would fire constantly and be
  switched off inside a week.

What actually catches these is reading the code near what you are
changing, and noticing when two explanations of one thing disagree.
`test-switch-defaults` gates the one sub-case that IS mechanical — a
documented default versus the initialiser.

### Known equivalent mutants

Survive by design. Do not re-investigate; if one of these starts being
caught, something else changed.

- `zxvga.cpp` `byte_hi = (x_end - 1) >> 3` → `x_end >> 3`. Marks one
  extra dirty byte; the flush then copies a byte that is already
  correct, so the output is identical and the flush-equivalence tests
  pass by design.
- `bricks.cpp` `repaint_row_top_edge` — see the plan's entry: no test
  can distinguish it, for the same reason.
- `physics.cpp` `laffc_sweep`'s four boundary terms
  (`cell_x == FIELD_X0` and friends). `BrickField::standing` treats
  out-of-range as gone, so the neighbour check alone already opens an
  edge face. Deleting a term changes nothing; INVERTING one does, and
  test_boundary_faces_stay_open catches that.

## INFO rows go stale silently

`test-visual`'s checkpoint table has an `assert_match` flag. Setting it
False keeps the row — it still captures, still diffs, still prints
"pixel-identical" when it is — but it fails nothing. That is the right
tool for a known residual, and the wrong one to leave lying around: the
row cannot tell you the residual is gone.

Three were found passing silently in one week, each downgraded for
something long since fixed:

| row | downgraded for | found |
|---|---|---|
| `state2_menu` | menu blink, before `test_mode_pin_blink` pinned it | no comment at all |
| `state4_level1` | a rendering residual | printing pixel-identical for weeks |
| `current_level_copy` (replay) | port and original destroying different cells | byte-identical |

All three are asserting now, and `test-visual` lints its own table: a
bare `False` fails, `False,  # INFO: <why>` passes. Downgrading is still
allowed — leaving no reason is not.

## Tools and gates are told apart by name

`scripts/test_*.py` and `scripts/check_*.py` are gates; everything else
in `scripts/` is a tool. `test-no-orphan-gates` keeps the convention
honest — a gate-named script no Makefile target runs looks like coverage
in a directory listing and is not.

The worked example is `visualise_levels.py`, which was called
`sweep_levels.py` until 2026-08-10. Reading it mid-session — no failure
path, prints diff counts, always exits 0 — I nearly reported that the
levels gate could never fail. The gate is `test_levels_sweep.py`, one
character away in the listing, and it fails properly.

**Six gates were audited for "can this actually fail?" and all six can.**
They end `sys.exit(main())` with `return fails`, a COUNT that is
non-zero when something failed: `test_levels_sweep`, `test_wall_bounce`,
`test_magnet_ball`, `test_laffc_levels_sane`, `test_laffc_ball_frame1`,
`test_bat_deflection_port`. That is a third idiom alongside
`raise SystemExit(...)` and `return 1`, which is exactly why it is not
gated — a checker enumerating three idioms will call a working gate
broken the day someone writes a fourth.

## A checker's haystack decides what counts as a use

Several gates here answer "is X used?" by searching source for X's name.
That makes the haystack part of the assertion, and documentation inside
it is the trap: a MENTION is not a USE.

Four instances, all found by mutation rather than review:

| gate | what it counted | fix |
|---|---|---|
| `notes_symbols` | its own write-up's backticked names | name retired symbols plainly |
| `phase_sweep` | a docstring explaining `BATTY_REPLAY_COUNTER` | strip docstrings before matching |
| `check_no_dead_constants` | its own docstring listing the dead names | strip Python docstrings and comments |
| `check_floppy_assets`, `check_asset_provenance` | a C comment naming `asset_load("X.BIN")` | strip `/* */` and `//` |

The last pair had it in the other direction from the rest: a comment
that names an asset — explaining why it is NOT loaded, say — made the
gate DEMAND the floppy ship it.

`check_env_passthrough` was probed the same way and does not have the
defect; a comment naming a fake `BATTY_*` knob does not make it ask for
a passthrough.

So when writing one of these: decide explicitly whether prose counts,
and prove it with a mutation that puts the name in a comment. Three of
the four above passed review and failed that test.

## Counter-phase sweeps (`scripts/phase_sweep.py`)

`pit_frame_counter` free-runs from boot, and cadences key off its low
bits — the enemy steer (`& 3`), the ball speed ramp (`& 7`). How long
boot took therefore decides which phase a probe frame lands on, so a
gate whose expectations depend on the phase passes or fails by luck.
That is known-bugs #17, found because `test-enemy-descend` failed about
two runs in three.

Running a gate repeatedly is how it was found, but repetition is a weak
instrument: it samples whatever phases the machine happened to produce.
`BATTY_REPLAY_COUNTER` pins the counter at the aligned start, so the
phase can be varied on purpose instead:

    scripts/phase_sweep.py test-enemy-anim test-bat-deflection

runs each gate at phases 0..3 — every case `& 3` can produce. Passing at
all four means the gate does not depend on the phase. Failing at some
means it was passing by luck.

**Run it on any new QEMU gate before trusting it.**

Gates that set `BATTY_REPLAY_COUNTER` in their own env are reported as
SKIPPED rather than swept: their inline value overrides the outer
environment, so all four runs would use the same pin and report a
confident, meaningless "phase-independent". (The first version of the
tool detected that by searching the whole file, which skipped every gate
whose DOCSTRING merely explains the variable — a false negative wearing
the costume of a decision. It strips docstrings and comments now.)

Validated by removing the pin from `test-enemy-margin-clamp`, whose
`dir` expectations are exact: the sweep reported PHASE-DEPENDENT at
pins 1, 2 and 3. A tool that can only ever say "fine" is not a tool.

Audited so far, all **phase-independent**: `test`,
`test-laffc-ball-frame1`, `test-bat-deflection`, `test-ball-no-tunnel`,
`test-rng-walk`, `test-enemy-anim`, `test-enemy-attr-parity`,
`test-l3-replay-seed`. The rest of the suite has not been swept — at
four boots per gate it is a couple of hours, worth spending when a gate
next behaves oddly rather than pre-emptively.

## Stale symbol citations (`scripts/notes_symbols.py`)

`check_doc_links` catches a note that points at a file which no longer
exists. Nothing caught a note that names a ROUTINE which no longer
exists, and that is how these notes actually rot: something is renamed
or deleted, and prose that was true keeps naming it.

The case that prompted the tool: `bounce_enemy_off_margins` was deleted
on 2026-08-09 and three notes still named it — one of them, known-bugs
#16, in the PRESENT TENSE, asserting the exact opposite of the code
("The port does all three. `bounce_enemy_off_margins` clamps to ...").
The reasoning around it was still correct; only its premise had rotted,
which is the hard kind to notice.

    scripts/notes_symbols.py

lists every backticked `snake_case` identifier in `notes/*.md` that
nothing in `src/`, `tests/`, `scripts/`, the Makefile or the
disassembly defines.

### The corpus skips comments and docstrings — fixed 2026-08-10

It did not, until four present-tense claims rotted in one day and this
tool reported none of them.

`known_identifiers()` scanned raw text, so a name counted as "defined"
if it appeared ANYWHERE — including in a stale comment, and including in
the docstring of the very gate whose claim had rotted. A name that
survives only in the prose that is wrong about it looked exactly like a
name in use.

It was masking its own founding case. `bounce_enemy_off_margins` was
deleted on 2026-08-09 and is the deletion this tool was written for;
`src/` comments still mention it, so it had stopped being reported.

`strip_prose()` now removes C comments and Python docstrings before
scanning. The report went from 36 names to 58, and the 24 extra include
both `bounce_enemy_off_margins` and `primary_ball_launch_from_bat`
(renamed the same day). Script FILENAMES are added back to the known
set, because a tool called `notes_symbols.py` does define the name notes
cite it by, and stripping docstrings would otherwise report every script
in `scripts/` as missing.

More noise is the price, and for a report that a human triages it is the
right trade: a missed rot is a wrong note, a spurious name is ten
seconds.

**What it still cannot see: member references.** `bat.extra_px` is
invisible, because only bare backticked snake_case is matched.
Extending to `foo.bar` was tried the same day and reverted — it also
matches filenames, and four of six new hits were `zrcp.py`,
`brick_bitmaps.bin` and friends. So after a rename, grep the old name
across `src/`, `notes/` and `scripts/` by hand; that is how the four
rots were actually found.

**It is a report, not a gate, on purpose.** It currently names ~35
identifiers and most are legitimate: past-tense history this repo
deliberately keeps (the sentence describing `static_bg_dirty`'s rename
to `static_bg_cache_dirty` has to name both), partial names
(`all_metal_briks` for `all_metal_briks_animation_snd`), and probe-field
values that look like identifiers (`active01_type04`). Separating those
from a rotted present-tense claim needs a reader. Making it fail the
build would mean either an allowlist that goes stale exactly the way the
notes do, or pressure to delete history to get green — so the judgement
stays with whoever runs it, during a hygiene pass.

**Triage of the 24 names the corpus fix revealed, 2026-08-10.** Four
were real, and all four were the same shape — a name that a reader would
grep for and not find:

- `parity-status.md` called the SMASH timer `big_ball_ticks`; the field
  is `ball.big_ticks`.
- the same file said a gate does not read "the C `our_to_orig_bonus`";
  that function is `bonus_to_original`.
- `bird-render-parity.md` recorded LAAD2 as "[DONE] ported literally as
  `step_obj_anim`". It shipped as `handling_blast_obj` — the planned
  name was never the shipped one, and the DONE marker made it look
  authoritative. A `src/` comment repeated it.
- `enemy-movement.md`'s open-items list still said the port's
  `enemy_target_away_from_margins` "is still an approximation". That
  function was DELETED on 2026-08-09 and `check_margins` ported
  literally; the bullet outlived its subject by a day, in the present
  tense, in a list headed by what remains to do.

The rest are legitimate history, probe-field values that look like
identifiers (`active01_type04`), or disassembly label spellings
(`sub_9231h`). That ratio — 4 real in 24 — is why this stays a report.

**First full triage, 2026-08-09.** 41 candidates, 6 of them real rot,
and one of those was not a naming problem at all: `rocket-flight.md`'s
parity table still listed BOTH rocket divergences as `DIVERGENT ✗` —
the in-flight brick tunnel and the instant end-of-flight award — long
after both were fixed. `step_rocket` has no cell loop and
`play_rocket_award_tally` ticks one brick per PIT frame with the bricks
left on screen. A table claiming a divergence that has been fixed is
worse than one merely out of date: it invites someone to fix it again.
The rest were renames: award_left_bricks, blit_masked_to_scr_buff_ptr,
apply_replay_random_override, score_to_codes, bomb_active and
bonus_active. The 35 that remain are past-tense history, partial names,
and probe-field values.

Those six are written WITHOUT backticks on purpose. A retired name in
backticks is a citation as far as this tool is concerned, so writing up
the triage in the obvious way added six fresh findings to the next run.
Same convention `check_doc_links` already uses for removed FILES — name
them plainly and they read as history rather than as live pointers.

## CI (`.github/workflows/parity-check.yml`)

Hosted GitHub runners have no KVM, so QEMU runs under TCG emulation that is
SLOWER than real time — and the gate harness waits for game frames with
wall-clock sleeps (boot-wait + delta/fps). Under TCG the port hasn't
finished N frames when the capture fires, so EVERY QEMU gate lands on the
wrong frame and "diverges" (calibration run 27697521157: bat-deflection
14/14, laffc-ball 5/5, over a 21-min job). A bigger `BATTY_BOOT_WAIT` does
not fix it; it needs a frame-completion-aware harness or a KVM/self-hosted
runner. So CI runs the fast, deterministic, emulator-free signal: build
the EXE via the in-tree linux-amd64 OpenWatcom, then `make test-fast` —
every host suite and every source gate, in seconds.

It used to name `test-video` and three gates by hand, which meant CI ran
1 of 14 suites and 3 of 10 gates while showing a green tick. Delegating
to `test-fast` also removed the third copy of a list that had already
drifted twice; `check_host_tests_wired.py` guards the one that is left.

The QEMU suite runs locally via `make parity-check-parallel`;
ZEsarUX-oracle gates via `make parity-check-full`.

## Running the suite in parallel (`make parity-check-parallel J=8`)

The gates are boot-dominated (~10 s/boot) and historically serial because
every script hardcoded the one floppy `build/batty-test.img`. Now the floppy
path is read from **`BATTY_TEST_FLOPPY`** (default unchanged): the Makefile's
`TEST_FLOPPY_OUT` honours it and derives a per-floppy AUTOEXEC scratch
(`AUTOEXEC_T`), and the gate scripts read it via
`os.environ.get("BATTY_TEST_FLOPPY", …)` / `test_visual.test_floppy()`. So
each gate can run on its own image with no collision.

`scripts/run_gates_parallel.py` pre-builds the shared `TEST_EXE` once (so
workers don't race on `build/main-test.obj`), then runs the QEMU-only gates
concurrently, each with `BATTY_TEST_FLOPPY=build/batty-test-<i>.img`. Same
gates, same assertions — just concurrent (~Jx faster: the fast core drops
from ~15 min to a few). ZEsarUX gates (`test-frame-step`, `replay-l3-entry`,
`capture-timeline-both`, `replay-l3-brick-flash-both`) are EXCLUDED — they
drive a single ZRCP port (10000) and a shared snapshot, so run those via the
serial `make parity-check-full`.

**Two things that made the full run untrustworthy**, both fixed 2026-08-07:

1. **30 gates hardcoded `build/batty-test.img`** and ignored
   `BATTY_TEST_FLOPPY`, despite the claim below that they all read it.
   Under the parallel runner that variable points at a per-gate image, so
   the gate either died in 0.1 s (`make build/batty-test.img` has no
   rule) or built one image and read `PROBE.TXT` from another, timing out
   at ~211 s with "NO … in PROBE.TXT". They now go through
   `test_visual.test_floppy()`.

   It took three passes to find them all, because the same bug was
   spelled three ways: `Path("…")`, `FLOPPY = "…"`, and `FLOPPY = '…'`.
   If you add a gate, take the floppy from `test_floppy()` — never a
   literal.
2. **A gate is not one boot.** `test-ball-no-tunnel` boots dozens of
   times, `test-levels-sweep` fifteen. `--full` at J=8 starved QEMU below
   real time and produced "NO … in PROBE.TXT" failures that were pure
   contention. `--full` now defaults to a quarter of the core count, and
   **any failure is retried once alone** — only a gate that fails twice is
   reported, and gates that needed the retry are named, so a growing list
   means J is too high for that machine.

**`make test-gate-greps`** guards the other recurring failure: 20 gates
assert on the SHAPE of the source (that a constant is still `$1B`, that a
guard still excludes `rocket_active`) by searching for a literal. Those
rot silently when code moves or is renamed — twice in one session that
cost six commits of red CI and a gate that passed while testing nothing.
The check resolves needles through variables and list comprehensions,
only considers *required* ones (`not in src` guarding a failure), and
runs in about a second. It is part of `make test-fast`.

Reliability net: a wait-key gate that reads a probe written at level init
(`probe_phase=init`, i.e. a missed `BATTY_REPLAY_WAIT_KEY` wake on a slow
boot) re-boots until it sees a real checkpoint write (`probe_phase=play`).
This is centralized: `capture_frame_timeline.py` (the shared driver all ~20
wait-key + `BATTY_REPLAY_PROBE` gates route through) retries the boot
internally on a `probe_phase=init` read; gates that drive `run_qemu`
directly (the no-tunnel sweeps) use `test_visual.boot_until_gameplay()`. This self-heal let the fixed
boot-waits be trimmed (capture_frame_timeline 12→10 s, seeded gates 9→8 s).

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
- `src/main.cpp` `getenv("BATTY_LEVEL")` in `run_level` sets
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
helper `blink_phase()` pins the phase to 0 (BLACK) when
`test_mode_pin_blink` is set — the test mode signalled by `BATTYALL`.
`make run`'s floppy leaves `BATTYALL` unset and the player sees the
natural blink.

(This used to be gated on `auto_advance`; commit 45cad07 separated the
two so the menu blinks during normal play. The note said otherwise until
2026-08-09.)

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

### A gate that strips its own floppy out of the environment

`test_brick_flash.py` rebuilds the image with `BATTY_*` filtered out of
the environment, because the AUTOEXEC bakes in whatever replay knobs
were set at build time and a leftover seeded image never reaches L3.
That filter also removed `BATTY_TEST_FLOPPY` — which is not a knob but
the *name of the image to build*. Under the parallel runner the gate
therefore deleted its own per-job image, rebuilt the shared default, and
QEMU failed to open a file that no longer existed.

It reads as a game regression: `BrokenPipeError` out of `run_qemu`, no
mention of the floppy, and the gate passes standalone because there the
per-job path and the default path are the same file. The real message is
one line down in `build/test_brick_flash/qemu.log`:
`Could not open 'build/batty-test-9.img': No such file or directory`.

Read the qemu.log before reading the traceback.

### The full suite now completes

`python3 scripts/run_gates_parallel.py --full` — **51 gates, all green,
341.6s wall** (serial would be ~2090s; 6.1x on 14 cores, 7 at a time).

It had never finished before. Two earlier attempts were killed at 90
minutes, which read as "the emulator gates are just slow" and was wrong
on both counts: they were not slow, they were deadlocked on each other's
floppy images, and the suite is in fact a five-minute check.

The two fixes that got it there:

- every gate honours `BATTY_TEST_FLOPPY` (30 scripts hardcoded
  `build/batty-test.img` and silently read another job's image)
- `test_brick_flash.py` stopped filtering `BATTY_TEST_FLOPPY` out of the
  environment when rebuilding its own floppy

Slowest gates, worth knowing before adding more: `test-levels-sweep`
267s, `test-laffc-levels-sane` 205s, `test-ball-no-tunnel` 100s,
`test-bonus-typepick` 90s, `test-ball-paths-no-tunnel` 82s. The wall
time is set by `test-levels-sweep` alone, so the suite cannot go below
~4.5 minutes without splitting that one.

`test-bat-redraw-window` passed this run (26.3s). It is intermittent,
not broken — see the flakiness note above; one green run is not
evidence it is fixed.

### test-bat-redraw-window: the flakiness is explained, and it is not timing alone

Measured at HEAD: **5 failures in 8 runs**, diff varying 143..232 px.

The varying magnitude was the clue — a real redraw defect gives a stable
count. The two captures come from separate boots and QEMU `sendkey` is
wall-clock: each press holds the key for however many 50 Hz frames
elapse, and the bat moves 4 px per frame held. The two runs finished up
to 12 px apart, and the gate compared a bat at one X against a bat at
another.

Two things were wrong in my first reading of the evidence. Measuring the
"bat span" across the ROI showed the two runs agreeing — but that
measurement was picking up the level's background pattern, not the bat.
Rendering the band as ASCII showed the 12 px offset immediately. Look at
the pixels before trusting a summary statistic over them.

Driving the bat into a wall clamp fixes the position: `bat_step_x`
clamps at 8 and at 220, so once both runs saturate the end position is
independent of how many presses registered. Counting discrete presses
does **not** get there — each `sendkey` lands on roughly half a frame of
key-down, so 40 presses bought only ~22 of the 27 frames needed.
`sendkey <key> <hold_ms>` does: two 800 ms holds saturate with margin.

With that, the gate is deterministic — and **red**, at both clamps, for
the two reasons in known-bugs.md #11. The original ROI (64..192) only
passed because the bat's landing position wandered; any deterministic
parking puts the bat outside that window, which would make the
comparison vacuous rather than green.

So the gate is left as it was for now. It cannot be both deterministic
and green until #11 is fixed, and a gate that passes because its subject
drifted out of frame is worse than a flaky one. The reproduction is
recorded above; the work is now a render fix, not a test fix.

### Mutation-testing a host test needs the binary removed first

`make test-physics` correctly lists `src/physics.cpp` as a prerequisite,
but a mutate / `make` / restore / `make` cycle runs inside one second,
and make's timestamp comparison cannot see a change that fast. The
second and third builds silently reran the first binary — so a mutation
appeared "caught" when it had never been compiled.

`rm -f build/test_physics` before each run. The failure mode is quiet
and it flatters the test: it reports the result of whichever mutation
last triggered a real rebuild.

### One more gate reading the wrong floppy — in a harness, not a gate

`scripts/replay_harness.py` hardcoded `build/batty-test.img` and ignored
`BATTY_TEST_FLOPPY`. `replay-l3-brick-flash` therefore *built*
`build/batty-test-N.img` (the Makefile target honours the variable) and
then *read* the shared default — whatever a concurrent job had left
there.

Missed in the earlier 30-gate sweep because that sweep looked at
`scripts/test_*.py`. This is a harness the gates call into.

It presents as a game regression: `test-midgame-brick-replay` fails with
`unexpected PPM size 720x400` — text mode, i.e. the run never reached
graphics — and it fails the serial retry too, because the shared image is
still wrong. Run it alone a minute later and it passes.

The `exercise_*.py` and `visualise_levels.py` scripts still hardcode the
path. They are manual tools, never run by the suite, so they are left
alone.

### Full-suite baseline after the refactor

`--full`, 51 gates, 341.5s: **50 pass, 1 fail** — `test-bat-redraw-window`,
the known-flaky one (known-bugs #11).

Its two failures in that run are worth recording because they confirm
the diagnosis without any extra work: **234 px in parallel, 178 px on
the serial retry.** A real redraw defect gives the same count every
time; a magnitude that moves between runs is the bat landing at a
different X, which is exactly what the flakiness was traced to.

It also shows the runner's retry heuristic has a limit. "Failed alone
too" is meant to separate contention from real failures, and it does —
but a gate that fails ~60% of the time on its own fails the retry
often enough to be reported as believed. The heuristic distinguishes
*contention* from *not-contention*, not flaky from real.

### check_gate_greps does not see slice-scoped needles

`test_rocket_bonus.py` asserts that the no-ball guard is followed within
220 characters by the bat-explosion call:

    idx = compact.find(guard)
    if "play_bat_explosion(...)" not in compact[idx:idx + 220]:

`scripts/check_gate_greps.py` models top-level `X not in src` needles.
It does not model a needle scoped to a slice, so when the explosion call
moved behind a `lose_a_life()` helper it reported **PASS** while the gate
itself failed.

So a green `test-gate-greps` narrows where a rename can hurt; it does not
prove the source gates still pass. `make test-fast` runs the gates
themselves and is what actually answers that — the grep check is a
one-second early warning, not a substitute.

The checker now says so itself. A needle whose right-hand side is not a
plain name is counted as **position-scoped** and reported separately:

    PASS gate_greps: 34 source needles across 20 gates still match
      note: 2 of them assert a POSITION in the source (e.g. `not in
      compact[idx:idx+220]`).
      Only their existence was checked here; run the gate itself to
      confirm the position.

A tool that silently under-reports is worse than one that admits its
limit, because the whole point of it is to be trusted in a second.

**Second blind spot, same shape.** The checker accepted a needle that
matched only after whitespace normalisation. Most gates want that —
`test_death_sparks` writes its needles pre-compacted and compares
against a stripped copy of the source. But
`test_rocket_completion_no_ball` compares against the RAW text, so when
an extraction dedented a block, the checker passed and the gate failed.

It now looks at which haystack the gate compares against (`compact` and
friends versus anything else) and warns only for the raw-text ones:

    WARN: 1 needle(s) match only after whitespace normalisation —
    indentation moved under them.
      test_rocket_completion_no_ball.py:23
    A gate comparing against the raw source will FAIL on these.

Both blind spots had the same cause: the checker modelled the needle but
not the comparison. It now models enough of the comparison to know when
it cannot judge.

### Two probe keys with the same name

PROBE.TXT emitted `bonus_state=` twice: once as
`active%02X_type%02X_x%02X_y%02X_bomb%02X` and once as a raw
`%02X%02X%02X%02X%02X%04X` blob carrying the +400 marker too.

Both were consumed, and it worked only by accident. The regex gates
(`test-bonus-fall`, `-drop`, `-typepick`) anchor on `bonus_state=active`,
so they matched the first. `replay_harness.py` copies every `k=v` line
into a LIST, `state_probe.txt` therefore had two `bonus_state:` lines,
and `test_midgame_brick_replay`'s dict kept the last — the raw one,
which it then asserts is exactly 14 characters long.

Swap the two `fprintf`s and that gate fails with "malformed bonus_state
probe", pointing at the format rather than at the duplicate name. The
second key is now `bonus_pts_raw`.

### The replay harness reused a stale extraction

`run_port_state_probe` mcopies PROBE.TXT off the floppy with
`check=False`, then reads the file if it exists. A failed extraction
left the PREVIOUS run's file in place, so the gate asserted on stale
state — which is how renaming a probe key produced `''` for a key that
was present on the floppy. It now unlinks the target first.

### test-midgame-brick-replay is load-sensitive

Fails intermittently at high concurrency with

    ValueError: unexpected PPM size 720x400; expected 320x200 or 640x400

720x400 is TEXT mode: the screendump happened before the boot reached
graphics. `replay_harness.py` paces its captures with fixed sleeps, and
with seven QEMU instances competing those sleeps are sometimes short.

Seen twice at `--jobs 7`; passes standalone and at `--jobs 3`, and was
green in both full-suite runs. Distinct from the stale-floppy bug above,
which produced the same symptom for a different reason — that one is
fixed.

It also fails the serial retry often enough to be reported as believed,
the same limit noted for `test-bat-redraw-window`: the retry separates
contention from not-contention, not flaky from real.

The replay itself is not fully deterministic either — `random_number`
differs run to run (DAA5, F6E6). The gate does not assert on it, but
that is worth knowing before writing one that does.

It has two failure faces under load, both from the same cause. Either
the capture lands in text mode (`720x400`, above), or the run reaches
the probe without having destroyed anything and the gate reports
`bricks_quantity=FF` — FF being the gate's own default for a key it
could not read. Neither says "the replay was too slow", which is what
happened.

`ppm_inner_to_indices` now names this case instead of reporting it as an
unexpected image size. It cost twenty minutes to identify twice, because
`unexpected PPM size 720x400` reads like a rendering difference when it
means the guest never left text mode:

    guest still in 720x400 TEXT mode — the capture beat the boot to
    graphics. Check the gate's SLEEPs (or run it at lower --jobs); this
    is not a pixel difference.

### No gate drives more than one visual checkpoint

Every gate sets `BATTY_VISUAL_PROBE_FRAMES` to a single frame and
rebuilds the floppy when it wants another. So the multi-checkpoint path
— the delta between consecutive checkpoints, and resuming after one that
is not the last — is exercised only by `capture_frame_timeline.py`,
which is a tool, not a gate.

    python3 scripts/capture_frame_timeline.py --floppy build/batty-test.img \
        --frames 20,40,60 --wait-key --out build/tl_check

`test-visual-checkpoints` now gates the *resume* half of that path —
three checkpoints must each produce a capture, so a run that stops early
fails. Mutation-checked by stopping after the second.

It does **not** gate the delta arithmetic. `capture_frame_timeline.py`
names each file after the requested frame, not the one that fired, so a
delta treated as an absolute (20, 60, 120 instead of 20, 40, 60) still
writes all three files. Catching that needs motion in the scene plus an
expected image per checkpoint — a golden-capture gate. Until then the
hand-run above is still the check after touching the delta.

### test-bat-redraw-window: fixed, both halves

The flakiness and the defect it was hiding are both resolved
(known-bugs #11). The gate now parks the bat against the left clamp with
`sendkey left 800` rather than counting presses, so the two boots land
it at the same X, and compares the whole bat band rather than a window
around a position that used to wander.

Before: 5 failures in 8 runs, diff 143..232 px. After: 5/5 green, and
removing the fix fails it at exactly 6 px.

The earlier conclusion here — that it could not be both deterministic
and green — was right at the time and is now obsolete: it could not be,
until the defect underneath was fixed.

### First fully green full-suite run

`--full`, **54 gates, all green, 343.4s** (serial would be ~2106s;
6.1x on 14 cores, 7 at a time).

The two earlier full runs were 50/51 — the miss both times was
`test-bat-redraw-window`, which is now deterministic and green because
the defect it was hiding (known-bugs #11) is fixed.

Gate count went 51 -> 54 today: `test-blast-dirty-redraw`,
`test-game-over`, `test-stuck-ball-offset` and `test-visual-checkpoints`
were added to cover paths nothing reached — a blast frame across the
dirty/full boundary, the game-over sequence, the stuck-ball offset
invariant, and the multi-checkpoint probe.

Wall time is still set by `test-levels-sweep` at 267s, so the suite
cannot drop below ~4.5 minutes without splitting that gate.

## Mutation sampling as its own activity (2026-08-10)

The gates are written per feature, so they cover what someone thought to
assert. Sampling mutations over already-tested code covers what nobody
did.

One afternoon's pass over `src/physics.cpp` and `src/bricks.cpp`, seven
mutations, four survivors:

- `laffc_sweep`'s left clamp — genuinely equivalent (unreachable through
  either caller), characterised rather than gated;
- its row-scan wrap test, its left/right direction gate, and its corner
  tie-break — all real, all pinned, all confirmed against the
  disassembly first.

Two mutations were caught immediately (`laffc_bounce`'s face mask, the
brick shadow row), which is the useful control: the pass is not just
finding weak spots everywhere.

### A mutation that fails to COMPILE reports as "caught"

`mutate.py` runs a make target and reads its exit status. A replacement
string that does not compile fails the target, and that is
indistinguishable from a test detecting the change.

This bit on 2026-08-10. Driving batches from a shell loop, `&&` inside
the replacement was written `\&\&`, which survived the quoting and went
into the file. Three results came back "caught" and two of them were
compile errors. One — `bricks.cpp`'s `left_live = (col > 0) && ...` —
was reported as covered in a commit message and was not: re-run with
correct quoting, it survives, and `col >= 0` reads the byte BEFORE the
grid for row 0. Now fixed (`reset_column_zero`) and pinned.

**Fixed in the tool the same day.** `mutate.py` now inspects the
target's output and reports ERROR, not "caught", when the mutated source
did not compile. Verified three ways: a deliberately broken replacement
reports ERROR, a real detection still reports caught, and a QEMU gate
still reports caught — that last one matters because `make` prints
"Error 1" on any failure, so the markers are `error:` and `Error!`
rather than the bare word.

The exhaustive `>=` sweep predates the fix but is unaffected: it built
its replacements in Python, and `>=` -> `>` always compiles.

The habit stands anyway — a green target is not evidence until you know
the binary changed, which is the same trap as `make | grep -i error`
matching the word inside "no errors".

**The method that works**, learned the hard way twice in that pass:

1. mutate, run, and take "SURVIVED" as a question rather than a verdict;
2. decide equivalent-or-gap by reading the DISASSEMBLY, not the port —
   three of four survivors had the port already correct;
3. get the test's input from a DIFFERENTIAL dump (build both variants,
   diff their output over a neighbourhood), never by deriving it or by
   reading the function's own output;
4. re-run the same mutation against the new test. Two of the four tests
   passed while the mutant still survived, and only this step said so.

Step 3 and step 4 are the ones that were skipped and had to be redone.

### Second pass: enemies, scoring, rng, hud, weapons

Six more mutations, two survivors, and the two are worth telling apart.

`rng.cpp`'s `addr < 0x8000 ? u16(addr | 0x8000) : addr` mutated to `<=`
is **equivalent by arithmetic**: the only value it changes is
`addr == 0x8000`, and `0x8000 | 0x8000` is `0x8000`. Nothing to gate,
nothing to write down beyond this sentence.

`weapons.cpp`'s `bullet_y >= FIELD_Y0` mutated to `>` is equivalent
**in practice, by two constants**: bullets launch at `bat.y - 1` = 172
and step 6, and 172 = 4 (mod 6) while FIELD_Y0 = 32 = 2 (mod 6), so a
bullet never lands on the band's top scanline. That is a fact about the
bat row and BULLET_SPEED, either of which could change, so the boundary
is asserted directly in `bullet_band_includes_top_row` rather than left
to arithmetic nobody would re-check.

**"Equivalent" is not one category.** Equivalent by arithmetic needs a
sentence; equivalent because two unrelated constants happen not to line
up needs a test, because the next person to change one of them will not
know they were load-bearing.

The other four were caught: the enemy's turn half-plane, the extra-life
score threshold, the RNG's wrap mask and the HUD's glyph bound.

### Third and fourth passes: bricks, objects, enemies

Four more real gaps, and a pattern in all of them — every one is an
INCLUSIVE comparison whose boundary value the tests never produced, and
in every case the port was already right:

| line | boundary | why it is ordinary, not a corner |
|---|---|---|
| `reset_destroyed_cell_attrs`: `cr + 1 >= cr0` | destroyed cell one row above a PARTIAL window | the interlock behind known-bugs #18; every test used the full window |
| `object_step_animation`: `a >= 0x40` | cadence counter at exactly $40 | reached by counting — $80 becomes $40 becomes 0 |
| `enemy_home_step`: `t.x < 0x10` | target x at $0F | `CP $10 / JR NC` clamps it and writes it back |
| `laffc_sweep`: three, see laffc-decode.md | | |

### The exhaustive sweep, and what a survivor count is worth

Sampling found six real gaps, so the class was worth attacking whole.
Every unique non-loop `>=` / `<=` in `src/*.cpp` outside `main.cpp` —
51 of them — mutated to its strict form against `make test-fast`.

**16 caught, 35 survived.** That number is a coverage MEASUREMENT, not a
bug count, and the difference matters: three triaged immediately were
equivalent or unreachable —

- `bat_court_clamp_2`'s `bat_x >= 0x80`: both branches return `0x80` at
  the boundary. Equivalent by construction.
- `delta_to_dir`'s two quadrant tests: the function has no production
  consumer at all (known-bugs #8), so nothing it does is reachable.

— while the two `blit_masked_sprite` clipping guards were real, and are
now covered by `sprite_blit_clips`. `>` there lets a sprite straddling
the right edge write one pixel past the row, onto the next row's first
pixel, or past the buffer on the last row.

By module: `zxvga.cpp` 15, `physics.cpp` 12, `bricks.cpp` 4,
`sound.cpp` 2, `hud.cpp` 2. The `zxvga` concentration is expected —
that layer is mostly gated by QEMU screendumps rather than host tests —
and it is precisely why the sprite-blit guards had no host cover.

**Triaged so far (2026-08-10):** 10 of the 35, and `physics.cpp` is now
measured rather than estimated.

Four physics lines went from survivor to covered: the zone walk
(`bat_zone_boundary_above`), the column walk, and both straddle tests —
the last two by one test, because a ball on a column edge exercises all
three.

Re-running the twelve physics lines still standing:

- **3 are dead code.** `delta_to_dir`'s two quadrant tests and its angle
  test; the function has no production consumer (known-bugs #8).
- **1 is equivalent by construction.** `bat_court_clamp_2`.
- **1 is `laffc_sweep`'s bottom-edge face** (`cell_y >= FIELD_Y_LAST`).
- **7 were `brick_sweep`'s cluster — now resolved.** Three real, four
  equivalent, and the four are equivalent for a reason worth keeping:

  | line | verdict |
  |---|---|
  | `top >= FIELD_Y_END` | equivalent — `row0` then exceeds `row1`, so the scan loop never runs |
  | `left >= x_end` | equivalent — same, via `col0 > col1` |
  | `col1 = (right >= x_end) ? ...` | equivalent — the wider bound scans one extra column and `standing()` returns false out of range |
  | `row1 = (bottom >= FIELD_Y_END) ? ...` | equivalent — same |
  | both came-from tests | REAL, fixed |
  | `overlap_y <= overlap_x` | REAL, fixed |

  `BrickField::standing` bounds-checks — *"Out of range counts as
  gone"* — and that one line is what makes half this cluster harmless.
  A defensive guard three files away decides whether a boundary here is
  a bug or a no-op.

That leaves the honest count for physics: **1 open**, plus 3 dead and
1 equivalent-by-construction.

**`bricks.cpp` resolved (2026-08-10):** 4 survivors, 3 real, 1
equivalent.

| line | verdict |
|---|---|
| `row >= FIELD_ROWS` | REAL — a 15-byte overread on every full-window call |
| `left_live = (col > 0)` | REAL — reads before the array on row 0 |
| `cr >= cr0` | REAL — the window's own first row stops writing |
| `r0 - 1 >= 0` | REAL — row 0's shadow lost when the window starts at row 1 |
| `cr0 <= 4 + (r1 + 1)` | equivalent — callers set `cr0 = 4+R0`, `r1 = R1`, `R0 <= R1`, so both forms hold for every reachable call |

Two of the four were MEMORY guards, which is worth noting: this class of
mutation is usually read as an off-by-one in behaviour, and half of what
it found here was an out-of-bounds access instead.

**`sound`, `hud` and `replay_parse` resolved (2026-08-10):** 5
survivors, 4 real, 1 equivalent.

- `sound`: MAGNET's counter LANDS on its terminator ($18 + 4k reaches
  $78 at k = 24) so `>=` and `>` differ by a frame — REAL, pinned by
  length. TRIPLE_BALL's does not ($10 + $0Bk never equals $B6) —
  equivalent, and its length is now asserted anyway so the difference
  between the two is visible rather than inferred.
- `hud`: both markup range ends. $2A is the last glyph and $40 the first
  attribute; the shipped markup uses neither, so no existing test
  produced one.
- `replay_parse`: the lowercase hex range. The existing blob was
  `01A2b3FF` — a lowercase `b`, but no `a` or `f`, so the range ENDS
  went untested while the range itself looked covered.

**`zxvga.cpp` (13 survivors), partially resolved:** the attr blit's
COLUMN clamp is real and now pinned (`attr_blit_clamps`). Getting there
took two attempts for a reason worth repeating: `>=` and `>` differ at
EXACTLY the bound, and a first rect that overshot to `col_hi = 38`
clamped under both forms. The test now lands `col_hi` on `ATTR_COLS`
precisely.

Its sibling, the ROW clamp, is **knowingly untested**. `row_hi ==
ATTR_ROWS` writes `attr_buff[24 * ATTR_COLS + c]` — past the end of a
768-byte global — so nothing inside the buffer changes and no assertion
on it can tell. Catching it needs a sanitiser build or a guard page,
neither of which this suite has. Recorded here rather than left as an
apparently-covered line.

Both are unreachable through today's callers (every one passes
`clip_right_px = PLAYFIELD_W - 8`, capping `col_hi` at 30), which is
precisely why they are worth asserting: that is a fact about the
callers, not the clamps.

Also real and now pinned: `blit_masked_to_scr_buff`'s `right >= 0`. A
sprite straddling the LEFT edge at an unaligned x has `left = -1`
(dropped) and `right = 0` — the row's first byte, which must still be
written. Two things had to be got right to see it: `x = -8` does not
work (shift 0 takes the aligned branch, so use -7), and the sprite must
be ONE byte wide, because a second column writes byte 0 through the
`left` guard and masks the one under test.

**The remaining six are all EARLY-OUTS, and all equivalent in output**
— `y0 >= y_end`, `y_start >= end`, `width <= 0`, `x_start >= x_end`,
`h <= 0`, `x1 <= x0`. The argument is one sentence and covers all of
them: relaxing an early-out can only cause MORE work, never less. A
degenerate rect then either runs a zero-trip loop, or widens a dirty
range, or bumps a profiling counter — it cannot skip a pixel that
should have been drawn.

And the output claim is not just reasoning: `dirty_flush_equiv_full`
compares a dirty flush against a full repaint, so a mutation that
NARROWED the refresh would fail it. All six pass it, which is what
"equivalent in output" means here.

**That closes the sweep.** 51 candidates, 16 caught outright, 35
survivors triaged: 17 real and fixed, 1 knowingly untestable (the attr
row clamp writes past a global), 17 equivalent with a derivation each.

### The STRICT half, and why it yields much less

The symmetric sweep — every `<` and `>` mutated to `<=` and `>=` — has
93 candidates. 16 caught, 70 survived, 7 errors.

**The 70 is mostly an artefact of the operator.** Classifying them:

| class | count | why it cannot be caught |
|---|---|---|
| clamp idiom `if (x < N) x = N` | 19 | relaxing to `<=` re-assigns N when x is already N |
| min/max ternary `(a < b) ? a : b` | 11 | same value either way at equality |
| preprocessor / not host-built | 5 | never compiled by the host suite |
| everything else | 35 | worth triaging |

So 30 of 70 are equivalent BY CONSTRUCTION — the mutation is a no-op on
the dominant idiom for `<` in this codebase. For a clamp, the meaningful
mutation is the CONSTANT (`< N` -> `< N-1`), which is how
`enemy_home_step`'s `t.x < 0x10` was found earlier; `<` -> `<=` on that
same line is equivalent.

**Choose the operator to suit the idiom.** The `>=` sweep had a real
hit rate of 17/35 because inclusive comparisons are boundaries by
nature. This one is padded with mutations that could never have failed.

Real so far from the strict half: `laffc_sweep`'s up/down direction
gate. `LAFFC_13` is `CP $20 / JR NC`, so dir $20 belongs to the UPWARD
half, and `<=` moves it. It is the vertical twin of the left/right gate
at dir $10 — both hinge on exactly one direction value, and both went
untested for the same reason.

**`repair_band_row_boundaries`' three row guards** are the other real
find, and all three are MEMORY guards:

    if (r1 + 1 < FIELD_ROWS) repaint_row_top_edge(cells, r1 + 1);
    if (r1 + 1 < FIELD_ROWS) repaint_row_attrs(cells, r1 + 1);
    if (r0 > 0) { ... cells[(r0 - 1) * FIELD_COLS + col] ... }

Relaxed, they read a row past the grid and a row before it. Every
existing caller passes an interior window, so none of the three had ever
had to fire in a test. `repair_row_guards` puts the grid inside a buffer
with a known row on each side and calls the full window, so the mutants
read something predictable and act on it while the correct code cannot
look at either.

**The three `x < 0` / `y < 0` clip guards** are the strict sweep's other
real find, and they fail in the opposite direction to everything above.
Relaxed to `<=`, a sprite blit silently stops drawing the playfield's
leftmost column and topmost row — it writes LESS.

That is why `sprite_blit_clips` missed them. It asserts the complement,
that nothing OUTSIDE the playfield is touched, and a mutation that draws
less can never violate that. The missing half is the positive claim: a
sprite at the origin must paint the origin.

Worth generalising: **a containment assertion cannot catch under-draw,
and an existence assertion cannot catch over-draw.** Both blits needed
one of each, and the two sweeps found them from opposite sides — the
`>=` sweep caught the over-draw guards, the `<` sweep the under-draw
ones.

That brings the memory guards found by mutation to SEVEN — four in
`bricks.cpp`, two in `zxvga.cpp`, one in `laffc_sweep`'s neighbourhood.
Boundary mutation reads as a hunt for off-by-one pixels; in this
codebase it has mostly been a hunt for reads and writes outside arrays.

### Third sweep: the CLAMP-BOUND operator

The second sweep's lesson was that `<` -> `<=` is a no-op on a clamp.
The operator that does bite is moving the BOUND: `if (x < N) x = N`
becomes `if (x < N-1) x = N`, leaving the boundary value unclamped.

21 clamp idioms, **6 caught, 15 survived** — against 0 caught by the
relational operator on the same lines. The lesson paid for itself in one
run.

And the first real finding is about a test, not the code.
`test_blit_stays_in_playfield` already called
`buff_to_vga_rect_bytes(-20, 400, -5, 99)` with a comment about
"deliberately out of range" arguments. Every one of those overshoots its
clamp by enough that a one-off bound still clamps it, so the call proves
the clamps EXIST and nothing about where they sit — all four survived.

Replaced by `(-1, PLAYFIELD_H + 2, -1, BYTES_PER_ROW)`: y0 and byte_lo
one BELOW their clamps, byte_hi and y_end one ABOVE theirs. One call,
four boundaries, all four mutants dead.

**Out-of-range test data has the same failure mode as an out-of-range
mutation: go far enough out and both sides of the boundary agree.** The
useful value is always the boundary itself, and "deliberately out of
range" is not a substitute for it.

**The clamp sweep, closed.** 21 idioms, 6 caught outright, 15 triaged:

| | |
|---:|---|
| 11 | real, fixed |
| 2 | were "knowingly untested" — both now caught, see below |
| 2 | equivalent, derived |

The last two: `asset_load_chunked`'s scratch bound is REAL — one step off
and every full chunk `fread`s `scratch_size + 1` bytes into a buffer
holding `scratch_size`. `test_chunked_matches_whole` could never see it,
because the extra byte is read AND copied so `dest` still matches a
whole-file load; only a guarded buffer shows it. And the fixture size
matters: `piece` exceeds `scratch_size` by exactly one only when the
remainder is `scratch_size + 1`, so the file is 257 bytes (64*3 + 65),
not a round 300 that never hits it.

`sound.cpp`'s `ticks > 60000` cap is equivalent: the longest single beep
in the game is `metal_brik` at 8557 us, so at the host suite's 1 MHz
clock the largest tick count is 8557 and the cap cannot be reached.

`mark_dirty_bytes`' clamps needed a different observation point.
`flush_dirty_slot_to_vga` loops `y = 0; y < PLAYFIELD_H`, so a row
marked at -1 or at PLAYFIELD_H is never READ back and the screen looks
perfect. But `dirty_min_byte` is `[DIRTY_SLOTS][PLAYFIELD_H]`, so a
write at `[slot][PLAYFIELD_H]` lands in the NEXT slot's row 0 — visible
by counting marked entries across the whole array rather than by
looking at pixels. `end > PLAYFIELD_H` and `byte_hi > 31` both die that
way.

`y_start < 0` does not, and the reason is worth recording: its stray
write goes to `[0][-1]`, one byte before the array, because slot 0 is
what an empty row picks. Only `[1][-1]` would land inside. **Knowingly
untested**, alongside the attr blit's `row_hi` — both are real
out-of-bounds writes that a host build cannot see without a sanitiser.

Three more clamps in `blit_sprite_attrs_to_buff_clipped` fell to the
same treatment, and the two CLIP ones matter most: they are what keeps
the blit off the side-frame's attribute cells, which is the whole reason
the function takes a clip pair. Real callers pass `clip_left = 8` and
`clip_right = PLAYFIELD_W - 8`, so columns 0 and 31 belong to the frame.
A rect starting at x = 7, one pixel left of the clip, recolours the left
frame cell if the clamp is off by one — and nothing was checking.

`col_lo`'s clamp is observable for a different reason: an unclamped -1
writes `attr_buff[r * ATTR_COLS - 1]`, which is the PREVIOUS row's last
cell. The rect has to start on row 1 for that to be inside the buffer at
all.

**Two more real, from shapes the filter was not even aiming at:**

- `object_step_animation`'s wrap, `((d >> 4) & 0x0F) < e`. `CP E / JR
  NC` skips the wrap at high >= E, so the frame EQUAL to the range high
  is shown and only the one past it wraps. `<=` wraps a frame early and
  the top frame of every animation is never drawn. Equality happens once
  per loop of every animation in the game.
- `render_markup`'s `while (p < markup_len)`. `<=` reads the byte after
  the stream and renders it — a `while` bound, which the filter should
  have excluded alongside `for (...)` and did not. Excluding it would
  have lost a real finding, which is an argument for a LOOSER filter and
  more triage rather than the reverse.

**The strict sweep, closed.** 93 candidates, 16 caught, 7 build errors
(the `#include <...>` lines — flagged as ERROR rather than silent junk
only because `mutate.py` now checks for build failure, the fix from
earlier the same day validated by accident), 70 survivors:

|  |  |
|---:|---|
| 6 | real, fixed — the up/down gate, three row guards, the animation wrap, the markup bound |
| 3 | real, fixed — the under-draw clip guards |
| 30 | equivalent BY CONSTRUCTION — clamps and min/max ternaries |
| 5 | not host-built |
| 26 | equivalent: dirty-rect bookkeeping that widens rather than narrows, settled by the same early-out argument as the first sweep and by `dirty_flush_equiv_full` |

The `hud` attribute one needed two attempts and the reason generalises:
"code $40 draws nothing" is satisfied by an UNRECOGNISED code too, since
both fall through. What separates them is that an attribute does
`x -= 8` and consumes no column. **When a branch's effect is "do
nothing visible", assert what it does INSTEAD, not what it does not
do.**

| line | verdict |
|---|---|
| `bat_court_clamp_2`: `bat_x >= 0x80` | equivalent — both branches return `0x80` |
| `delta_to_dir`: two quadrant tests | unreachable — no production consumer |
| `blit_masked_sprite`: `x >=`, `y >=` | REAL, fixed (`sprite_blit_clips`) |
| `bat_deflect_dir`: `offset >= zones[i]` | REAL, fixed (`bat_zone_boundary_above`) |

The zone walk is the one worth reading twice. `LAB1F_6` is
`CP (HL) / JR C`, so the walk continues while `offset >= boundary` and
an offset landing exactly ON a boundary belongs to the zone ABOVE it.
With `>` it deflects as though it were one pixel to the left. The
captured hardware cases sit at offsets -3, 5, 13, 21 and 29 — not one of
them on a boundary — so the table this port was built from could not
have caught it.

**The rest are a triage backlog, not a defect list.**
Working through them means, per line: decide equivalent-or-real from the
disassembly, and for real ones build the input from a differential dump.
That is minutes each, which is why this note records the list's shape
rather than pretending the sweep finished the job.

### Why boundaries in particular

**`>=` and `>` differ at one value, and a test written from a scenario
rarely lands on it.** Scenario tests pick
positions that are interesting to a player; boundaries are interesting
to a compiler. Mutation is what connects the two, and the disassembly
is what says which side of the boundary is right — all four had a
`JR NC` or a `JR C` settling it in one line.


## AddressSanitizer (`make test-asan`)

Three mutation sweeps found nine memory-safety defects, every one caught
by a hand-built guarded fixture — a grid with a phantom row on each
side, a scratch buffer declared larger than it is passed. Two could not
be caught at all: `mark_dirty_bytes`' `y_start < 0` writes one byte
BEFORE a static array, and the attr blit's `row_hi` writes past
`attr_buff`. Neither is observable from a normal host build.

`make test-asan` rebuilds every host suite with
`-fsanitize=address,undefined` and runs them. Both mutants now die, and
no fixture was needed for either — only a rect that reaches the boundary
(`y = 190, h = 8` puts `row_hi` at exactly ATTR_ROWS).

**It found a real bug on its first run**, before any of that:
`replay_parse_hex_bytes` read both nibbles of a pair before checking
either, so a spec ending mid-pair — including the empty string — read
one byte past the terminator. The values come from `BATTY_*` environment
variables, so a short one reaches it. Fixed by checking the high nibble
first.

Not part of `test-fast`: it needs its own build of every suite, so it
doubles the compile. Run it before touching anything that indexes a
buffer.

### It also broke mutate.py's build-failure check

ASan prints `ERROR: AddressSanitizer:`, which contains the `error:`
marker the build-failure detector looks for — so the first sanitiser
catches were reported as "the mutated source did not build". The
detector now tests for sanitiser markers FIRST and treats them as a
detection.

A nice illustration of the same hazard the detector exists for: a signal
that looks like failure can come from anywhere, and "which kind of
failure" has to be decided explicitly.