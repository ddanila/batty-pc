# Visual regression test

> **`make test-fast` needs no emulator and runs in seconds** — 14 host
> test suites plus 30 source gates. Start there; it is also exactly what
> CI runs. `make test-video` is one of those suites: it compiles the
> video engine (`src/zxvga.cpp`) with the host compiler and checks the
> ZX attribute/colour-clash model exhaustively — every attr x every byte
> — in milliseconds. See [`video-engine.md`](video-engine.md).
>
> The QEMU gates below cost ~10 s per boot; `make parity-check-parallel`
> runs all 79 gates of the full sweep in about six minutes (78 QEMU
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
- `test-life-respawn` — a death gives back a centred bat, a fresh ball
  on it and no leftover bonuses (`test-life-loss` covers only the life
  being taken).
- `test-respawn-redraw` — after a death the bat and the magnets are
  drawn the same whether or not the life count changed. The post-death
  repaint used to ride on `lives_dirty`; with `BATTY_INFINITE_LIVES` the
  bat came back in fragments (known-bugs #21).
- `test-hud-patch-extent` — the in-place HUD patch covers every row the
  score digits occupy. A SOURCE gate, and not by preference: the visual
  executable is built `-dBATTY_SCORELESS_HUD`, so `render_hud_to_buff` is
  empty in every QEMU gate and no screendump in this repo has a score on
  it. The digits have no visual coverage at all (known-bugs #22).
- `test-host-tests-wired` — every host suite runs under `make test-fast`.

**CI is a second compiler, and it is authoritative for one thing.**
`.github/workflows/parity-check.yml` runs `make test-fast` and
`make test-asan` on ubuntu-latest, where `c++` is g++. Nothing local
can stand in for it: Apple clang and g++ disagree about uninitialized
analysis, and that gap kept `main` red for 163 runs (2026-08-09 06:25
to 2026-08-10) while every local sweep was green. **Read the run after
you push** — `gh run list --limit 3` is enough. A green local sweep is
not the same claim as a green CI.
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

`test-visual`'s checkpoint table has an `assert_match` flag. False keeps
the row — it still captures, diffs and prints "pixel-identical" — but it
fails nothing. That is the right tool for a known residual and the wrong
one to leave lying around: three rows sat at False for residuals long
since fixed. The table is linted now — a bare `False` fails, `False,  #
INFO: <why>` passes — because the row itself will never tell you it has
gone stale.

If a residual is explained by "the GT can't show this region", that is a
blind spot, not a floor: recapture the GT, or split the region into its
own ROI with its own number.
