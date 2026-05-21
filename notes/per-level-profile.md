# Per-level visual-diff profile

`BATTY_LEVEL=N make test` (N = 1..15) diffs `state4_level1` against
`build/level_gt/level_NN.scr`. **13 of 15 levels are pixel-perfect**;
the remaining two sit at a combined 611 px residual out of 49152.

## Current per-level numbers

| Level | Cycle | Diff (px) | Residual location                                |
|-------|-------|-----------|--------------------------------------------------|
| L01   | 0     | **0**     | PASS                                            |
| L02   | 1     | **0**     | PASS                                            |
| L03   | 2     | 305       | HUD / top-frame residual                          |
| L04   | 3     | **0**     | PASS                                            |
| L05   | 0     | **0**     | PASS                                            |
| L06   | 1     | **0**     | PASS                                             |
| L07   | 2     | **0**     | PASS                                            |
| L08   | 3     | **0**     | PASS                                            |
| L09   | 0     | 306       | HUD / top-frame residual                          |
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

## L3 / L9: HUD bright-bit anomaly (cr 1 cc 8..11)

L3 (cycle 2) and L9 (cycle 0) end up with `attr_buff[1*32+8..11]`
holding the bright form (`$45` / `$46`) instead of the GT's non-bright
form (`$05` / `$06`). The bright bit survives despite:

1. `paint_frame_to_buff` writing `lattr[1*32+8] = $05` (verified — the
   file has $05 at the right offset).
2. `print_border_shadow_c` clearing bit 6 of `cr 1 cc 2..30` (verified
   — runs inside `render_brick_band`).
3. No known code path writing to `cr 1 cc 8` after `paint_frame_to_buff`.

L7 / L11 / L15 (also cycle 2 with same `lattr`) render the cell
correctly. L13 (cycle 0, same `lattr` as L9) also renders correctly.
The cycle-and-data identical levels diverging by level number is the
mystery.

Iters 19, 20, 22, 23, 24, 25, 30, 36 all attempted sentinel-write
debugging from `render_level_screen`. **The sentinel writes never
appear in the captured PPM** — even via volatile pointers, even via
chained write+read+vga-write — strongly suggesting Open Watcom's
`-os` optimization eliminates writes to static-near arrays at the
END of `render_level_screen`. Without DOS-side instrumentation that
the compiler can't elide (file I/O, BIOS interrupt, serial output),
this debug avenue is closed.

Possible angles for the future:
- Compile `render_level_screen` with `-od` (no optimization) and
  re-test — see if sentinel writes survive then.
- Mark `attr_buff` / `scr_buff` as `volatile` globally (perf cost,
  but unblocks debugging).
- Port `random_generate` deterministically and trace the actual
  modded-batty pipeline state at the trap moment to see if there's
  an extra write to cr 1 we haven't replicated.

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
