# Performance Notes

## Current Profiling Workflow

Use the render-only profile run when comparing renderer cost:

```sh
make profile-86box
```

Exit the game cleanly so `PROFILE.TXT` is written to the DOS floppy image,
then read and analyze it:

```sh
make read-profile
```

`profile-86box` injects `BATTY_RENDER_PROFILE=1` into `AUTOEXEC.BAT`.
The game treats that as a render-only profiling mode and disables sound, so
PC-speaker I/O does not hide renderer costs.

## Latest Render-Only Result

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
- `make run` and `make run-86box` repack the floppy image every time so
  env-driven profile/sound flags do not go stale.

## Next Likely Wins

1. Localize lives changes the same way score changes are localized.
2. Split magnet rendering from top HUD rows so score localization is safe on
   magnet levels too.
3. Update destroyed brick cells directly in `bg_scr_buff` / `bg_attr_buff`
   instead of relying on score-triggered static rebuilds.
4. Reduce HUD/lives work inside dynamic frames, especially top-row restores.
5. Continue optimizing `buff_to_vga_rect_bytes` if VGA bytes/frame remains
   high after the composition work is reduced.
