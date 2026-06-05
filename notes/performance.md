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

## Next Likely Wins

0. Add a brick-destruction profile scenario (ball or auto-fired bullets
   repeatedly hitting bricks) so the incremental band rebuild above can be
   measured, not just reasoned about. The current open-bounce profile
   destroys no bricks.

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
