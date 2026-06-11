# Performance Notes

## STATUS: rendering perf at its floor (2026-06-05)

The render-cost campaign is comprehensively complete and measurement-backed.
Both realistic profiles confirm minimal avoidable work:
- `profile-ballbricks` (ball through bricks, emergent bonuses): **88 ball-only
  / 73 ball-object / 19 full-dynamic** of 180 frames.
- `profile-multiball` (extra balls in play): **0 / 178 / 2 full-dynamic**.

Arc: the realistic scenarios started at ~100% full-dynamic (full recompose
every frame); every moving element now has a dirty tier (ball — moving/
stuck/multi/big — bat incl. fire-anim, enemy, bonus, +400, bullets, blasts,
bomb), brick hits cost one scoped-band-rebuild frame, and both per-frame
scan loops (restore + carry) are bounded to the dirty row span.

What remains is **intrinsic or measured-marginal**, not headroom:
- `buff_to_vga` (~31%) — 1bpp→8bpp per changed pixel, 16-bit stosw; tight.
- `render_brick_band` bucket (~38%) — the per-frame OBJECT/full composition
  (e.g. compositing 3 balls in multiball); proportional to what's on screen.
- The per-frame restore memcpy — proportional to moving-sprite area.
- The few remaining full-dynamic frames are NECESSARY (level entry, the
  scoped band rebuild on a brick destroy — the full path is already
  localized for these, so a dedicated tier wouldn't help; verified).

Further rendering-perf changes would be marginal and carry dirty-redraw
gate-risk. Higher-value future perf would be a DIFFERENT dimension (asset/
level load time, sound path) — but those are one-time/low-value or hard to
measure under QEMU's fast disk.

**The perf loop was concluded here (2026-06-05).** Rendering is at its
floor. If perf work resumes, the untouched lever is LOAD TIME (the asset/
level `fread`s in src/main.c around the file-load helpers — batching small
reads helps the real DOS/floppy target even though QEMU's fast disk hides
it); that was identified but not pursued. The render-cost work is complete.

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
- **Multi-ball dirty tier — LANDED (2026-06-05), the biggest win.** Extra
  balls (ball2/ball3) now route to the simple-object tier and are rendered +
  dirty-marked there instead of forcing a full recompose every frame.
  Measured (`profile-multiball`, two extra balls in play): full-dynamic
  frames **~178 → 2**, `ball block balls` → 0, 178/180 frames now on the
  ball-object tier; total PIT 27214. Multi-ball play used to be a worst
  case (every frame full-dynamic); it's now tiered like ordinary play.
  Verified by `test-multiball-dirty-redraw` — and the key was a DETERMINISTIC
  harness: the prior MULTI_BALL-catch scenario was flaky (emergent 3-ball +
  bonus cascade) and entangled with the +400 popup. The new
  `BATTY_REPLAY_MULTIBALL` bake hook spawns ball2/ball3 BELOW the brick band
  (no bonus catch → no popup; a few-frame probe stays clear of bricks for
  any direction), giving a pixel-exact, repeatable comparison (passed 2/2
  runs). The +400-popup simple-tier coupling is handled by routing
  pts_400-active frames to the full path (brief + low-frequency; the popup
  always co-occurs with the catch anyway). big-ball still stays full-dynamic
  (its wider sprite needs a wider dirty rect — a small separate follow-up).

- **big-ball dirty tier — LANDED (2026-06-05).** The last BALLS
  full-dynamic driver. It turned out trivial: SPR_BIG_BALL and
  SPR_BALL_NORMAL are BOTH 16×12 (2 bytes × 12 rows — verified from the
  sprite blob), and `render_ball_to_buff` already draws the big-ball sprite
  in every tier, so the primary's existing 16×12 dirty mark covers it. The
  big-ball blocker was purely conservative — removed it (no rect change).
  `test-bigball-dirty-redraw` (new, via a deterministic `BATTY_REPLAY_BIGBALL`
  hook) confirms pixel-exact vs the full baseline. **Every ball state —
  normal, multi-ball, big-ball — is now on the dirty path; `ball block
  balls` only fires for a genuinely-absent primary (hidden/stuck) now.**

- **Stuck-ball tiered (2026-06-05).** Re-profiling `profile-ballbricks`
  after big-ball showed `ball block balls` STILL 89 — now from `ball_stuck`,
  not big-ball (the emergent scenario catches a MAGNET bonus → the ball
  rides the bat, and the auto-profile never presses SPACE to launch). But a
  stuck ball is visible and rides the bat at a known position (BALL_X/Y set
  each frame), so it redraws fine on the dirty path — it was force-routed to
  full only by the `ball_stuck` half of the BALLS blocker. Dropped that half
  (kept `!BALL_VISIBLE`, which genuinely has nothing to draw). Measured:
  full-dynamic **107 → 19**, `ball block balls` → 0, 88 frames now ball-only.
  Helps the real MAGNET-hold + pre-launch states. `test-stuck-ball-dirty-
  redraw` (new) confirms pixel-exact. **`ball block balls` is now fully
  eliminated** — every ball state (moving, stuck, multi-ball, big-ball) is on
  the dirty path; BALLS only fires for a truly hidden primary.

- **Per-frame restore scan bounded to the dirty rows (2026-06-05).** With
  every moving object now tiered, the realistic profile's cost shifted to
  the EVERY-FRAME floor: `restore_prev_dirty_from_static_cache` (in the
  `paint_bg_to_buff` bucket) scanned all 192 pixel rows each frame even when
  only ~12 were dirty (the common ball-only case = 88/180 frames). Now
  `carry_dirty_with_previous` records the pixel-row span of the new
  prev_dirty (free — it already scans all rows), and the restore iterates
  only `[prev_dirty_y_lo, prev_dirty_y_hi]`. Measured (profile-ballbricks):
  `paint_bg_to_buff` 8686 → 7570 (the scan overhead removed). All 8
  dirty-redraw gates + the visual states pass (a too-narrow range would
  leave a trail — none did). Init range is full so the first restore
  (pre-carry) is safe; empty range (hi<lo) scans nothing.

- **Per-frame carry scan bounded too (2026-06-05).** Symmetric to the
  restore-scan bound: `carry_dirty_with_previous` also scanned all 192 rows
  every frame (it lives in the `buff_to_vga` bucket). It only needs the rows
  where the current OR last-prev set is dirty, so it now scans
  `[min(cur,prev)_y_lo, max(cur,prev)_y_hi]`. The current span is tracked in
  `mark_dirty_bytes` + `mark_all_dirty` (the only pre-carry current-mark
  paths) and reset in `clear_dirty_ranges` for the current set; the prev span
  comes from the previous carry. Measured (profile-ballbricks):
  `buff_to_vga` 8760 → 7792. The bound is the UNION of current+prev so it
  can never be too narrow (it changes the scan range only, not the flushed
  output — so it can't introduce a trail); all 8 dirty-redraw gates + visual
  states pass. (Note: `test-ball-object-dirty-redraw` flaked once then passed
  twice — guessed at the time to be a SLEEP-timed-screendump capture flake;
  root-caused 2026-06-11 as the counter_misc PHASE: the enemy steer gates on
  the global counter's &3, so the test's dirty and full boots landed on
  random phases and the enemy's 12-frame path differed by 1-2 px — a
  constant ~145px sprite XOR, passing only when both boots shared a phase.
  Fixed by pinning BATTY_REPLAY_COUNTER=0 in the test. Unrelated to the
  scan range either way.) With both scan loops bounded, the every-frame
  floor is now just the restore memcpy + the VGA flush — both intrinsic.

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
