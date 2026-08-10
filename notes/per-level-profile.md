# Per-level visual parity

`BATTY_LEVEL=N make test` (N = 1..15) diffs `state4_level1` against
`build/level_gt/level_NN.scr`. **All 15 levels are pixel-perfect**,
FAIL-gated for every level by `make test-levels-sweep` (15 QEMU boots, in
`parity-check-full`). `make test` alone gates the default L1.

The GTs come from the modded-tape capture pipeline
(`notes/modded-batty.md`).

## The blind spot that made the sweep necessary

For an unknown stretch, non-default levels were INFO-only, and L3 and L9
sat at ~186 px each while every L1-gated suite stayed green. The residual
turned out not to be render drift at all.

The diff was a single 24x16 box at x 64..87, y 9..23 — read as "scattered
across the whole playfield" from a thumbnail, which cost the first pass.
Decisive facts, in the order they landed:

- an identical 191 px footprint on both levels, colours tracking each
  level's cycle ink: one artefact, two levels;
- GT L3 == GT L7 in that region, ours-L3 != both, while ours-L7 passes —
  so the artefact is drawn at RUNTIME, not by the static render;
- 24x16 at x=64 is exactly the UFO sprite (`prop_even` w=$18 h=$10) at the
  spawn slot `prop_x_coord[0] = $40`;
- `enemy_prepare` spawns only when bricks-remaining < $2C (44). Starting
  destructible counts: L3=26, L5=9, L9=7 — and L5 is the one level the
  original exempts from enemies entirely. So {immediate-spawn levels} minus
  {L5} = exactly {L3, L9};
- the state4 dump lands ~1.5 s after the level-entry ENTER, about gameplay
  frame 10 (the 1.2 s banner and 0.32 s shimmer eat the rest), and a
  probe-halt timeline of L3 entry shows the descending UFO occupying
  exactly 64,9..87,23 at frame 10, with frames 40 and 60 pixel-clean;
- the GTs are alien-free by construction, so a live alien anywhere in frame
  is an automatic "drift".

**Fix:** `enemy_prepare` returns early under `BATTYALL`, so natural alien
spawns are pinned off in test mode — the same philosophy as the menu-blink,
running-dot and magnet-toggle pins. Tests that need an alien seed one via
`BATTY_REPLAY_ENEMY_OBJECT`, which bypasses the spawner; the RNG walk is
unchanged, since `enemy_prepare`'s reads are `rng_sample` and advance
nothing.

Two things worth carrying forward. An earlier `FORCE_FULL_FLUSH_EACH_FRAME`
run that looked "clean" and suggested stale VGA was itself a timing
artefact: full-flushing slows level entry enough that the dump lands before
the spawn. And the informational `gate-laffc-long` now lacks the port-side
alien that the ZEsarUX side still spawns, so its frame-40 residual gains
about a sprite of diff — re-baseline if it is ever gated.

## Magnet ON/OFF at level paint

`render_magnets` draws each level's magnets per `magnets_per_level[]`. Per
`print_magnets` ($8D4C) and `gfx_screen_elements` at $77F0:

- sprite `$06` = `spr_magnet_circle_ON` (w=4, h=30 with the SMC) — the
  lightning sprite;
- sprite `$07` = `spr_magnet_circle_OFF` (w=3, h=23) — the bare outline.

The original draws ON unconditionally, then conditionally overlays OFF, so
the outline punches holes in the lightning. In test mode (`BATTYALL`) the
coin is pinned: slots 0 and 1 skip OFF (pure lightning, ~70% set), slots 2+
draw OFF on top (~43% set). That matches every modded-batty GT surveyed.

Getting the two sprite ids backwards — treating `$06` as OFF — inverted the
conditional skip and cost 1383 -> 660 px of residual to undo. **Read the
sprite-ID table before writing per-slot draw logic**: the names in
`gfx_screen_elements` are ordered by entry, not by sprite-data layout.

The runtime toggle and the ball physics are in `notes/magnets.md`.

## Two resolved residual classes

**L6 / L12: a magnet overlapping the HUD.** L6's magnet 1 is at (116, 16),
L12's at (116, 8) — char rows 2 and 1, the HUD score and label rows. The
port's `paint_frame_to_buff` runs AFTER the magnet draws and overwrote
their top rows. Reordering it to match the original's
`game_screen_draw_to_buffer` changed nothing measurable and was reverted.
What actually fixed it: `inner_border_line_c` was clearing the top-frame
inner edge AFTER `paint_frame_to_buff`, while the original clears that
vertical line BEFORE drawing the top border. Matching the original's net
final image removed the stale top-border holes and the L6/L12 residuals
together.

**L3 / L9: a top-frame centre residual** in char cells cr 0..2, cc 8..10 —
the gameplay redraw path leaving stale top-frame pixels after the static
background cache was introduced. `restore_top_frame_center` restores those
cells at the end of `redraw_full_with_ball` and marks the top frame dirty.
Deliberately narrow: repainting the whole HUD after magnets would re-open
the L6/L12 case. It also has to run BEFORE the object compose — running
after it erased any sprite slice overlapping x 64..87, y<24, which showed as
alien and ball flicker on full-path frames.

## How `BATTY_LEVEL=N` reaches DOS

- the Makefile injects `SET BATTY_LEVEL=N` into the test floppy's
  `AUTOEXEC.BAT`;
- `make test` `rm -f`s the floppy first, because the image bytes do not
  change with the env and `make` would otherwise consider it up to date;
- `getenv("BATTY_LEVEL")` in `run_level` sets `round_number = N-1`;
- `scripts/test_visual.py` switches `state4`'s expected snapshot to
  `build/level_gt/level_NN.scr`.

`BATTY_START_LEVEL=1` is a different knob: it starts the port directly in
`ST_LEVEL` after asset load, for the replay targets.

## The disassembly matches the binary

`build/modded_batty/batty.sna` differs from
`original/disasm/tools/batty_for_compare.sna` — the reference binary the
disassembly was generated against — in exactly 35 bytes, all at addresses
listed in `PATCHES` in `scripts/build_modded_batty.py`. So the non-patched
code is byte-perfect and the disassembly is fully consistent with what runs
in the emulator.

That killed an early hypothesis that "the disasm doesn't match the binary".
The real indirection was `table_shifts` pre-shifting the blit operands —
see `notes/blitter-port.md`.
