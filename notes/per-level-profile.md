# Per-level visual-diff profile

`BATTY_LEVEL=N make test` (N = 1..15) diffs `state4_level1` against
`build/level_gt/level_NN.scr`. **All 15 levels are pixel-perfect**.

## Current per-level numbers

| Level | Cycle | Diff (px) | Residual location                                |
|-------|-------|-----------|--------------------------------------------------|
| L01   | 0     | **0**     | PASS                                            |
| L02   | 1     | **0**     | PASS                                            |
| L03   | 2     | **0**     | PASS                                             |
| L04   | 3     | **0**     | PASS                                            |
| L05   | 0     | **0**     | PASS                                            |
| L06   | 1     | **0**     | PASS                                             |
| L07   | 2     | **0**     | PASS                                            |
| L08   | 3     | **0**     | PASS                                            |
| L09   | 0     | **0**     | PASS                                             |
| L10   | 1     | **0**     | PASS                                            |
| L11   | 2     | **0**     | PASS                                            |
| L12   | 3     | **0**     | PASS                                             |
| L13   | 0     | **0**     | PASS                                            |
| L14   | 1     | **0**     | PASS                                            |
| L15   | 2     | **0**     | PASS                                            |

## Magnet ON/OFF semantics (iter-21 + iter-34)

`render_magnets` in `src/main.c` draws each level's magnets per
`magnets_per_level[]`. Per the original `print_magnets` ($8D4C) and
the `gfx_screen_elements` table at $77F0:

- sprite_num **$06 = `spr_magnet_circle_ON`** (w=4, h=30 with SMC) —
  the "active" lightning sprite.
- sprite_num **$07 = `spr_magnet_circle_OFF`** (w=3, h=23) — the
  bare-outline sprite.

The original draws ON unconditionally, then conditionally overlays
OFF (= the bare outline punches holes in the lightning) on a 50%
random coin. In test mode (BATTYALL), we pin the coin: magnet slots
0 and 1 skip OFF (= pure lightning at ~70% set), slots 2+ draw OFF on
top (= ~43% set). This matches every modded-batty GT capture
surveyed (slots 0/1 always lit, slots 2+ outlined).

Iter-21 originally had this BACKWARDS (treating $06 as OFF and $07 as
ON, so the conditional-skip logic was inverted); iter-34 flipped it
and dropped total residual 1383 → 660 px.

## L6 / L12: magnet overlaps HUD area (resolved)

L6 magnet 1 is at `(116, 16)` — y=16 = char_row 2 = HUD score row.
L12 magnet 1 is at `(116, 8)` — y=8 = char_row 1 = HUD label row.

In our `render_level_screen` order (`render_magnets` → ... →
`paint_frame_to_buff`), the frame-paint pass writes y=0..23 from
`frame_l1.bin`'s cycle-N entry AFTER the magnet draws, silently
overwriting the magnet pixels at HUD rows. For L6 / L12 specifically,
the magnet's top rows (rows 0..7 / 0..15) get clobbered.

Iter-37 tried moving `paint_frame_to_buff` BEFORE `render_magnets` to
match the original's `game_screen_draw_to_buffer` order. Surprisingly,
the diff didn't change — possibly because `state4` captures during
`show_round_banner`'s 60-PIT wait when only the FIRST
`render_level_screen` has flushed, so even with the reorder the
second-render's effect doesn't reach the captured PPM. Reverted.

Resolved while chasing the L1 12-pixel residual: `inner_border_line_c`
was clearing the top-frame inner edge after `paint_frame_to_buff`, while
the original clears that vertical line before drawing the top border.
Matching the original's net final image removed the stale top-border
holes and also cleared the L6/L12 magnet/HUD residuals.

## L3 / L9: top-frame center residual (resolved)

L3 and L9 carried a top-center residual in char cells `cr 0..2,
cc 8..10`. The visible issue looked like a HUD bright-bit anomaly at
first, but the actual capture showed the gameplay redraw path leaving
stale top-frame pixels in the final VGA image after the static
background cache was introduced.

The fix is deliberately narrow: `restore_top_frame_center` restores
those cells from `frame_l1.bin` and `level_attrs.bin` at the end of
`redraw_full_with_ball`, then marks the top frame dirty. That keeps the
full original-captured top frame authoritative without repainting the
entire HUD after magnets, which would re-open the L6/L12 overlap case.

## How `BATTY_LEVEL=N` works

Pre-iter-17, the env var was a no-op — `getenv("BATTY_LEVEL")` ran in
DOS but the test floppy's AUTOEXEC.BAT only set `BATTYALL=1`, never
the level var. Per-level numbers from iters 11–16 were silently
L1's render diffed against L_N's GT (= mostly meaningless).

Iter-17 wired this through:
- Makefile injects `SET BATTY_LEVEL=N` into the test floppy's
  AUTOEXEC.BAT when the host env is set.
- `make test` `rm -f`s the floppy first so the env change always
  triggers a rebuild (the floppy bytes don't change with env, so
  `make` would otherwise consider it up-to-date).
- `scripts/test_visual.py` reads `BATTY_LEVEL` and switches `state4`'s
  expected snapshot to `build/level_gt/level_NN.scr`.

## Verification: assembled SNA matches reference binary

`original/disasm/tools/batty_for_compare.sna` is the reference binary
the disasm was generated against. Our `build/modded_batty/batty.sna`
differs from it in exactly 35 bytes — all at addresses listed in
`PATCHES` in `scripts/build_modded_batty.py`. So our build's non-
patched code is byte-perfect; the disasm is fully consistent with
what runs in QEMU.

This kills the iter-25 hypothesis that "the disasm doesn't match the
binary" — see [`blitter-port.md`](blitter-port.md) for the full
formula reconciliation between the original Z80's table-driven
shifted blit and our direct-bitops C port.
