# Visual regression test

> **`make test-video` is the one gate here that needs no emulator.** It
> compiles the video engine (`src/zxvga.cpp`) with the host compiler and
> checks the ZX attribute/colour-clash model exhaustively — every attr x
> every byte — in milliseconds. See [`video-engine.md`](video-engine.md).
> Everything below drives QEMU or ZEsarUX and costs ~10 s per boot.

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

## CI (`.github/workflows/parity-check.yml`)

Hosted GitHub runners have no KVM, so QEMU runs under TCG emulation that is
SLOWER than real time — and the gate harness waits for game frames with
wall-clock sleeps (boot-wait + delta/fps). Under TCG the port hasn't
finished N frames when the capture fires, so EVERY QEMU gate lands on the
wrong frame and "diverges" (calibration run 27697521157: bat-deflection
14/14, laffc-ball 5/5, over a 21-min job). A bigger `BATTY_BOOT_WAIT` does
not fix it; it needs a frame-completion-aware harness or a KVM/self-hosted
runner. So CI runs only the fast, deterministic, emulator-free signal (build
the EXE via the in-tree linux-amd64 OpenWatcom + the source-level gates
l3-replay-seed / death-sparks / rocket-bonus, ~7 s). The QEMU suite runs
locally via `make parity-check-parallel`; ZEsarUX-oracle gates via
`make parity-check-full`.

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

The `exercise_*.py` and `sweep_levels.py` scripts still hardcode the
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
