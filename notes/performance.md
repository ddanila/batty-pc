# Performance Notes

## Current Profiling Workflow

Use the deterministic headless profile run when comparing renderer cost:

```sh
make profile-auto
```

`profile-auto` builds a normal floppy with `BATTY_RENDER_PROFILE=1`,
`BATTY_START_LEVEL=1`, `BATTY_LEVEL=$(PROFILE_LEVEL)`, and
`BATTY_PROFILE_AUTO_FRAMES=$(PROFILE_FRAMES)` injected into
`AUTOEXEC.BAT`. The game starts directly in the level with a seeded
moving primary ball, exits after the requested number of rendered gameplay
frames, writes `PROFILE.TXT`, and the Make target extracts it plus
`build/profile-summary.json`. Defaults: level 1, 180 rendered frames,
25 seconds of QEMU wall-clock time.

Use `PROFILE_LEVEL=3` for the busier brick/special-object stress case.

Use the manual 86Box run for spot checks on the IBM XT + VGA profile:

```sh
make profile-86box
```

Exit the game cleanly so `PROFILE.TXT` is written to the DOS floppy image,
then read and analyze it:

```sh
make read-profile
```

Both profile modes inject `BATTY_RENDER_PROFILE=1`. The game treats that
as a render-only profiling mode and disables sound, so PC-speaker I/O
does not hide renderer costs.

## Latest Automated Render-Only Result

Captured on May 26, 2026 with the default deterministic QEMU profile:

```sh
make profile-auto
```

```text
Profiling Report over 180 frames:
  paint_bg_to_buff:     13760 (29%)
  paint_frame_to_buff:  250 (0%)
  HUD / Lives:          4394 (9%)
  render_brick_band:    12254 (26%)
  buff_to_vga:          16448 (34%)
  static rebuilds:      2
  full dynamic frames:  61
  ball-only frames:     50
  ball-object frames:   69
  ball block bat:       0
  ball block static:    2
  ball block HUD:       3
  ball block objects:   3
  ball block bricks:    3
  ball block balls:     57
  ball block bat FX:    0
  VGA rect flushes:     457
  VGA bytes written:    390576
  sound disabled:       1
  Total PIT ticks sum:  47106
```

Analyzer summary:

- Top bucket: `buff_to_vga` at 34.9%.
- Background restore: 29.2%.
- Full dynamic redraw now covers 61 frames (33.9%).
- Ball-only redraw path covered 50 frames (27.8%).
- Ball+simple-object redraw path covered 69 frames (38.3%).
- Remaining ball dirty blockers are mostly multi-ball / big-ball style
  ball state (57 frames).
- VGA output averages 2.54 rect flushes/frame and 2170 bytes/frame.

## Latest Manual 86Box Render-Only Result

Captured on May 24, 2026 after the dirty-rectangle and brick-animation
passes, using the default `make profile-86box` 86Box target (`ibmxt`,
`vga`):

```text
Profiling Report over 332 frames:
  paint_bg_to_buff:     5262692 (28%)
  paint_frame_to_buff:  88298 (0%)
  HUD / Lives:          5622072 (30%)
  render_brick_band:    3715480 (19%)
  buff_to_vga:          3919476 (21%)
  static rebuilds:      1
  VGA rect flushes:     1147
  VGA bytes written:    728472
  sound disabled:       1
  Total PIT ticks sum:  18608018
```

Interpretation:

- VGA output is no longer the only dominant cost.
- HUD/lives composition and background restore are now the top buckets.
- Static rebuild count is low in this run, so the remaining cost is mostly
  per-frame restore/composition, not repeated full static rebuilds.
- VGA output is still significant at about 2.2 KiB/frame in this sample.

## Implemented Speed Passes

- Dynamic redraws now track byte ranges instead of full-width dirty rows.
- Dynamic redraws support two dirty intervals per row.
- The top HUD/frame is no longer flushed every moving-object frame.
- Brick flash and hard-brick hit animation no longer force full static
  background rebuilds every animation frame.
- Score/HUD changes are localized on non-magnet levels.
- Intro brick shimmer flushes only brick-field byte columns.
- PC speaker effects no longer busy-wait in the 50 Hz frame body.
- Brick destruction refreshes only the brick-band slice of the static
  background cache instead of rebuilding the whole static level image.
- VGA byte expansion now uses an 8086-safe precomputed attribute/nibble
  table: each Spectrum byte is emitted as four `stosw` writes instead of
  per-pixel shift/mask logic. The table covers all non-FLASH attributes
  in 8 KiB and intentionally avoids 386-only dword copies.
- Moving-object redraws no longer flush the full bat footprint every
  frame when the bat is stationary. The bat is still composed into
  `scr_buff`, but VGA output is limited to the running-dot row unless
  the bat position, size, laser/gun frame, or fire animation changes.
- Plain primary-ball motion can now use a ball-only dirty redraw: it
  restores the previous dirty ball ranges, redraws only the new ball and
  the bat running-dot row, and falls back to the full dynamic compose for
  brick hits, HUD changes, bonuses, enemies, bullets, rockets, extra
  balls, bat movement, and cache invalidations.
- Primary-ball motion with simple moving objects can now also avoid full
  dynamic compose. The dirty-object path redraws the primary ball, bat
  running-dot row, enemy, falling bonus, and +400 marker, while still
  falling back for bombs, rockets, bullets, brick animations, extra balls,
  HUD changes, bat motion, and cache invalidations.
- Static intro/title screens clear only the mode-13h border before
  blitting the 256x192 asset, and read screen assets in 16-row chunks
  instead of 192 single-row DOS reads. The 8 KiB random source table is
  loaded into far heap so the 4 KiB chunk buffer does not overflow the
  small-model DGROUP limit.
- `make run` and `make run-86box` repack the floppy image every time so
  env-driven profile/sound flags do not go stale.
- **Incremental brick-band cache rebuild (2026-06-05).** A brick hit used
  to invalidate the whole static brick-band cache: `build_static_brick_band_
  cache` re-painted the bg over all ~98 px-rows, ran `render_brick_band`
  (which re-composites **all** 180 cells via `print_briks_c`), and copied
  the whole band back. Now `mark_brick_row_dirty(row)` accumulates a dirty
  brick-ROW range `[lo,hi]`, and the rebuild scopes paint + render + copy to
  those rows (plus one below for the vertical inter-brick shadow) via
  `render_brick_band_rows` / `print_briks_rows_c`. A single hit re-composites
  ~3 rows (~45 cells + ~24 px-rows) instead of 180 cells + 98 px-rows —
  roughly **4× less rebuild work per brick destroyed**, the common cost in
  real (brick-heavy) play. The whole-band case (level entry, rocket clear)
  still uses the proven full path. Rendering whole rows (all columns) keeps
  the horizontal inter-brick shadow correct without per-cell tracking.
  Verified correct: `test-brick-flash`, `test-midgame-brick-replay`,
  `test-brick-scoring`, `test-ball-object-dirty-redraw`, and the visual
  states all pass; the open-bounce profile baseline is unchanged (no
  regression — that scenario destroys no bricks, so it can't show the win).
  Quantifying it needs a brick-destruction profile scenario (see below).

- **Measured the band-rebuild scoping (2026-06-05) — and it corrected the
  assumption.** Added `make profile-bricks` (laser bat + `BATTY_AUTO_FIRE`
  so bullets continuously destroy bricks) + band-rebuild counters (`band
  rebuilds` / `band rows rebuilt` / `band rebuild PIT` in PROFILE.TXT) + a
  `FULL_BAND=1` A/B toggle (`BATTY_FULL_BAND_REBUILD` forces the whole-band
  path). A/B over 180 frames, 7 brick-destroy rebuilds:
  - scoped:  28 rows rebuilt, band rebuild PIT 1364
  - full:    98 rows rebuilt, band rebuild PIT 1278
  The row count drops 3.5× (98→28) as designed, but the **wall-clock is
  flat** (within run-to-run noise). Why: `build_static_brick_band_cache` is
  only ~4% of frame time. The REAL cost of a brick-destroy frame is the
  **full-dynamic redraw** it forces — `static_bg_cache_dirty` is a
  ball-only-fast-path blocker (`BALL_DIRTY_BLOCK_STATIC`), so every
  brick-change frame falls back to `redraw_full_with_ball` (the mislabeled
  `render_brick_band` ~21% bucket + bg + vga). The row-scoping is still a
  correct ~3.5× reduction in cache-rebuild work and is kept, but it is not
  the lever. **Honest takeaway: profile-driven, not assumption-driven.**

- **Bullets/blasts joined the simple-object dirty tier (2026-06-05).** The
  brick-destruction profile turned out to be **bullet-dominated**, not
  brick-dominated: with auto-fire, a bullet is in flight almost every frame,
  and bullets were a hard full-dynamic blocker — `ball block objects: 124`
  of 180 frames. Extended `render_simple_objects_to_buff_and_mark` to render
  + dirty-mark bullets and impact blasts (and dropped the bullet rejection
  in `can_redraw_ball_with_simple_objects`), so in-flight bullets now use
  the cheaper ball-object tier. Measured (profile-bricks): full-dynamic
  frames **124 → 72**, total PIT 36400 → 35242 (~3% in this laser worst
  case; the remaining 72 are bullet frames that ALSO carry a flash / HUD /
  bat-fx blocker). A bullet travels 6 px/frame — the fastest dirty sprite —
  so `test-bullet-dirty-redraw` (new) compares the dirty path to the
  `FORCE_BALL_FULL_REDRAW` baseline mid-flight and confirms **no trail**
  (pixel-identical). Wired into `parity-check-full`.

- **Bomb joined the simple-object dirty tier (2026-06-05).** Same proven
  pattern as bullets: a falling enemy bomb (a single 16×16 sprite, common in
  normal play) was a hard full-dynamic blocker. Extended
  `render_simple_objects_to_buff_and_mark` to render + dirty-mark it (the
  bat-collision kill stays in step_bomb), dropped `bomb_active` from the
  `can_redraw_ball_with_simple_objects` rejection. `test-bomb-dirty-redraw`
  (new, reuses the `BATTY_REPLAY_BOMB` hook) confirms the dirty path is
  pixel-identical to the `FORCE_BALL_FULL_REDRAW` baseline mid-fall. Wired
  into `parity-check-full`.
- **Diminishing-returns note (2026-06-05).** The renderer is now
  well-optimized: the common case (open bounce) is 178/180 ball-only frames,
  and the `buff_to_vga` flush (the analyzer's top bucket) is already tight —
  one coalesced byte-aligned rect per moving object, with the 1bpp→8bpp
  conversion intrinsic per changed pixel. The remaining full-dynamic drivers
  are each a specialised tier with edge cases and modest payoff: brick-hit
  frames also carry a HUD (score) blocker and the full path already skips
  inactive objects (so a brick-flash tier saves little — note the flash
  renders NOTHING, it only schedules the destroyed cell's dirty rect); the
  bat fire-anim / resize (`bat_fx`) forces a full frame only to reflush the
  changed bat sprite. Worth doing if a specific scene needs it, but expect
  ~single-digit-% gains, not step changes.

- **Bat fire-animation joined the dirty path (2026-06-05).** The laser
  cannon fire-anim (and the bat sprite changing each of its ~8 ticks) was a
  full-dynamic blocker (`BALL_DIRTY_BLOCK_BAT_FX`) purely because the dirty
  tiers only marked the 1px running-dot row dirty, never the bat body. New
  `redraw_bat_dirty` helper: on a fire-anim frame it repaints + flushes the
  whole 13px bat body (otherwise just the running-dot row as before); the
  blocker now fires only on a resize TRANSITION (`bat_extra_px !=
  bat_extra_tgt`, which needs the vacated-area restore). Measured
  (profile-bricks): full-dynamic frames **72 → 21** (`ball block bat FX` 63
  → 0). Combined with the bullet tier, the laser worst case went **124 → 21**
  full-dynamic frames. `test-bat-fire-dirty-redraw` (new) confirms the
  dirty path is pixel-identical to the FORCE_BALL_FULL_REDRAW baseline
  mid-fire-anim. The remaining 21 are brick-hit frames (band + HUD blockers
  — the low-marginal tier per the note above).

- **Brick destruction: one full-dynamic frame, not two (2026-06-05).**
  `render_brick_flash` draws nothing — `brick_flash_ticks` only keeps the
  destroyed cell's rect marked dirty (a BRICKS full-dynamic blocker). It was
  2 ticks = 2 full-dynamic frames per hit, but the VGA is single-buffered
  (fixed 0xA0000000, no page flip) and `carry_dirty_with_previous` already
  re-flushes last frame's dirty rects (restoring them from the now-updated
  cache). So frame N rebuilds the band + flushes the cell (erased on the hit
  frame — no transient), and frame N+1 re-flushes that rect via the carry
  even as a ball-only/object frame. Cut to 1 tick. Measured (profile-bricks):
  `ball block bricks` 19 → 12 (the 7 redundant second-frames removed). In
  that laser scenario the full-dynamic TOTAL is unchanged (those frames also
  carry bullet/object blockers); the full→cheaper conversion lands in normal
  (no-laser) brick-hit play, where the second flash frame has no other
  blocker. Total-PIT is measurement-noisy run-to-run so the brick-blocker
  frame count is the reliable signal. Correctness: test-brick-flash (no
  stale cell vs L3 ref), midgame-brick-replay, brick-scoring, and the dirty
  gates all pass.
- **"Plateau" CORRECTED by a realistic profile (2026-06-05).** The
  plateau claim above rested on the open-bounce profile (178/180 ball-only)
  — which is unrealistic: it has no enemy and never multiballs. New
  `make profile-ballbricks` bakes a ball INSIDE the brick band (no laser) so
  it bounces through the bricks; this is normal-play. It revealed:
    1. **Real play is object-tier-dominated, not ball-only.** 0/180 ball-only
       frames — an enemy is on screen most of the time, so the ball-object
       tier is the common path (validating the bullet/bomb tier work).
    2. **Multi-ball is the biggest un-tiered full-dynamic driver.** A brick
       hit dropped a MULTI_BALL bonus → caught → `ball block balls: 89` of
       180 frames forced full-dynamic. Extra balls (ball2/ball3) and big-ball
       have no dirty tier, so multiball play — a common, deliberate state —
       recomposes the whole frame every frame.
  So the renderer is NOT at a plateau; the multi-ball/big-ball tier (long
  listed below as a Next Win) is a confirmed, large lever, not a
  speculative one.

## Next Likely Wins

0. **Multi-ball / big-ball dirty tier (CONFIRMED top lever).** Measured 89/180
   full-dynamic frames in `profile-ballbricks` are `ball block balls`.
   ball2/ball3 are full 16×12 moving sprites like the primary — extend the
   dirty path to render + dirty-mark them (and the big-ball sprite), same
   proven pattern as the bullet/bomb tiers. Verify with a multi-ball
   dirty-redraw comparison.

## Next Likely Wins

0. **Brick-change frames should NOT force a full-dynamic redraw.** This is
   the real lever the measurement exposed: a single destroyed brick
   invalidates `static_bg_cache_dirty`, which blocks the ball-only fast
   path, so the whole frame re-composites. Instead, after the (now cheap,
   scoped) band-cache rebuild, flush just the changed band rect + the ball
   via the dirty-rect path — like the ball-only tier but including the
   dirty brick rows. That removes the per-brick-hit full-frame recompose.

1. Reduce `buff_to_vga` bytes/frame and rect count. The automated profile
   now makes this the top measured bucket.
2. Add a multi-ball / big-ball dirty redraw tier. The current default
   profile still records 57 ball-state blockers after the simple-object
   path.
3. Split background restore further so the profiler can distinguish dirty
   cache copy, top-frame repair, and moving-object composition.
4. Localize lives changes the same way score changes are localized.
5. Split magnet rendering from top HUD rows so score localization is safe on
   magnet levels too.
