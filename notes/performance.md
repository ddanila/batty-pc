# Performance Notes

## Current Profiling Workflow

Use the deterministic headless profile run when comparing renderer cost:

```sh
make profile-auto
```

`profile-auto` builds a normal floppy with `BATTY_RENDER_PROFILE=1`,
`BATTY_START_LEVEL=1`, `BATTY_LEVEL=$(PROFILE_LEVEL)`, and
`BATTY_PROFILE_AUTO_FRAMES=$(PROFILE_FRAMES)` injected into
`AUTOEXEC.BAT`. The game starts directly in the level, exits after the
requested number of rendered gameplay frames, writes `PROFILE.TXT`, and
the Make target extracts it plus `build/profile-summary.json`. Defaults:
level 3, 180 rendered frames, 25 seconds of QEMU wall-clock time.

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

Captured on May 25, 2026 with the default deterministic QEMU profile:

```sh
make profile-auto
```

```text
Profiling Report over 180 frames:
  paint_bg_to_buff:     17142 (27%)
  paint_frame_to_buff:  1296 (2%)
  HUD / Lives:          12258 (19%)
  render_brick_band:    9636 (15%)
  buff_to_vga:          21536 (34%)
  static rebuilds:      1
  VGA rect flushes:     665
  VGA bytes written:    498176
  sound disabled:       1
  Total PIT ticks sum:  61868
```

Analyzer summary:

- Top bucket: `buff_to_vga` at 34.8%.
- Background restore: 27.7%.
- HUD/lives: 19.8%.
- VGA output averages 3.69 rect flushes/frame and 2768 bytes/frame.

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
- `make run` and `make run-86box` repack the floppy image every time so
  env-driven profile/sound flags do not go stale.

## Next Likely Wins

1. Reduce `buff_to_vga` bytes/frame and rect count. The automated profile
   now makes this the top measured bucket.
2. Split background restore further so the profiler can distinguish dirty
   cache copy, top-frame repair, and moving-object composition.
3. Localize lives changes the same way score changes are localized.
4. Split magnet rendering from top HUD rows so score localization is safe on
   magnet levels too.
