# Per-level visual-diff profile

`BATTY_LEVEL=N make test` (N = 1..15) diffs `state4_level1` against
`build/level_gt/level_NN.scr`. **All 15 levels are pixel-perfect**,
now enforced by the fail-gated `make test-levels-sweep` (in
parity-check-full). The 2026-06-11 "L9 drift" below was root-caused to
the screendump racing a LIVE alien, not render drift — see the RESOLVED
section.

## REGRESSION SPOTTED (2026-06-11): L9 drifted to 186 px — TODO, separate look

Found while regression-testing the magnet/shimmer fixes (known-bugs
#4/#5), NOT caused by them — verified byte-identical at clean HEAD
`aec8d43` before those changes landed:

    rm -f build/batty-test.img && BATTY_LEVEL=9 make test
    -> INFO state4_level1: 186/49152 px differ (0.38%)

It surfaced only as **INFO** because state4 is FAIL-gated on L1 alone;
non-default levels are informational — exactly the lessons.md trap
("INFO is for accepted drift, not unmeasured surface"). The 15/15
pixel-perfect table below dates from commit `73b3013` ("Fix level-entry
parity and brick flash cleanup"); nothing has re-run the sweep since,
so the drift's age is unknown. L1 (same cycle 0 as L9) is still 0 px.

Diff shape (build/test_visual/state4_level1_diff.png from the L9 run):
scattered 1–2 px clusters across the WHOLE playfield — frame-border
edges, brick band, and open field alike — NOT localized to the magnets
or a single band. That smells like a global render detail (bg-texture /
attr / shadow nuance or a stale GT), not a sprite bug.

### What to do (in order)

1. **Sweep all 15 levels** and rebuild the table:
   `for N in 1..15: rm -f build/batty-test.img; BATTY_LEVEL=$N make test`
   (or `scripts/sweep_levels.py` for a single-boot composite grid —
   note it cycles levels with ENTER rather than booting per level, so
   confirm a discrepancy with the per-boot command before trusting it).
   Key question: is the drift L9-only, cycle-0-wide (L5/L13 share
   cycle 0 — but L1 passes, so plain cycle-0 is unlikely), or
   magnet-count-related (L9 and L13/L14 have 4 magnets)?
2. **Localize before theorizing** (lessons.md): regenerate the L9 diff
   PNG and bucket the differing px by region — frame border vs brick
   band (y 32..127) vs open field vs the four magnet zones (L9 magnets
   at paint origins (0x40,0x3C), (0xA8,0x3C), (0x54,0x6C), (0x94,0x6C),
   each ~32x30 px). If the diff overlaps the magnet zones, FIRST
   suspect the GT side: lessons.md "audit post-processing" records
   `scripts/clean_gts.py` wiping the y=120..160 band including magnet
   pixels from GTs once before. Verify `build/level_gt/level_09.scr`
   is the pristine modded-tape capture (re-run the capture pipeline in
   `notes/modded-batty.md` if in doubt) before touching the renderer.
3. **Bisect** `BATTY_LEVEL=9 make test` over `73b3013..HEAD` (the
   claim commit → today). Prime suspects given the diff shape: the
   perf campaign's brick-band/dirty-redraw/static-cache commits — the
   documented `BRICK_FLASH_TICKS` regression (notes/metal-shimmer.md)
   already proved that family can regress render parity while every
   L1-gated suite stays green.
4. **Close the blind spot** (the structural fix, regardless of root
   cause): add a `test-levels-sweep` gate to `parity-check-full` that
   FAIL-gates state4 for ALL 15 levels at 0 px (or explicit per-level
   budgets if some drift is accepted after analysis). Until that gate
   exists, this table can rot silently again — per lessons.md, either
   recapture/fix to 0 or give each level its own gated number; INFO is
   an alibi.

## RESOLVED (2026-06-11): it was the screendump racing a LIVE alien

Executed the plan above; the trail led somewhere unexpected.

**Sweep** (step 1): only L3 (185 px) and L9 (186 px) drifted; the other
13 were 0 px. Not magnet-correlated (L3 has none), not cycle-correlated
(L3 cycle 2, L9 cycle 0; siblings pass).

**Localize** (step 2): the "scattered everywhere" reading of the diff
PNG was a thumbnail-scale misread (red diff vs grey context). The
actual diffs sat ENTIRELY in x 64..87, y 9..23 — a 24×16 box. Decisive
facts, in the order they landed:

- Identical 191-px footprint on both levels, colours tracking each
  level's cycle ink — one artefact, two levels.
- GT L3 == GT L7 in the region, ours-L3 != both, while ours-L7 passes —
  so the artefact is something drawn at RUNTIME, not static render.
- 24×16 at x=64 = exactly the UFO sprite (prop_even w=$18 h=$10) at the
  enemy spawn slot `prop_x_coord[0] = $40`.
- `enemy_prepare` spawns only when bricks-remaining < $2C (44). Starting
  destructible counts: L3=26, L5=9, L9=7 — and L5 is the one level the
  original exempts from enemies entirely. {immediate-spawn levels} −
  {L5} = **exactly {L3, L9}**.
- The state4 dump lands ~1.5 s after the level-entry ENTER ≈ gameplay
  frame ~10 (1.2 s banner + 0.32 s shimmer eat the rest) — and a
  probe-halt timeline of L3 entry shows the descending UFO occupying
  EXACTLY 64,9..87,23 at frame 10, with frames 40/60 pixel-clean vs GT
  (the dirty path erases the fly-over correctly; no stale ghost).
- The GTs are alien-free by construction (modded-batty trap fires
  before the alien is composed), so a live alien anywhere in frame is
  an automatic "drift".

So: no render regression, no GT corruption, nothing to bisect (step 3
moot — the race's outcome flipped with incidental entry-timing changes
across the campaign, e.g. shimmer-length fixes). The earlier
FORCE_FULL_FLUSH_EACH_FRAME=1 "clean" run that suggested stale VGA was
itself a timing artefact: full-flushing slows level entry enough that
the dump landed before the spawn.

**Fix:** `enemy_prepare` now returns early under BATTYALL
(`test_mode_pin_blink`) — natural alien spawns are pinned off in test
mode, same philosophy as the menu-blink / running-dot / magnet-toggle
pins. Every test that needs an alien seeds one via
`BATTY_REPLAY_ENEMY_OBJECT`, which bypasses the spawner; the RNG walk
is unchanged (enemy_prepare's reads are rng_sample, no advance). L3/L9
state4: pixel-identical after the pin. Caveat: the informational
`gate-laffc-long` (frames up to 40) now lacks the port-side alien the
ZEsarUX side still spawns — its frame-40 residual gains ~a sprite of
diff; it is not in any suite, re-baseline if it's ever gated.

**Blind-spot closed** (step 4): `make test-levels-sweep` boots all 15
levels and FAIL-gates state4 at 0 px each; wired into parity-check-full.

**Bycatch — a real bug found by the triage harness:** the A/B
dirty-vs-fullflush repro built for the (wrong) stale-VGA theory found a
genuine ~21 px black trailing residue at a descending UFO's upper edge
rows (frame-50 halt, RNG+counter pinned, measured (83..87, 49..57) on
L1). See `scripts/repro_enemy_flyover_trail.py` (expected-FAIL repro,
not yet wired) and the pending entry in notes/known-bugs.md.

UPDATE 2026-06-12 (follow-up session): the bycatch decomposed into
three. (1) NOT persistent — post-fly-over captures are 0 px, so there
is no lingering trail; the 21 px is an IN-FLIGHT compose delta between
the dirty and full paths next to the live sprite (still open, needs the
ZEsarUX oracle — see known-bugs.md). (2) A real full-path bug found and
FIXED: `restore_top_frame_center` ran after the object compose,
erasing any sprite slice overlapping the top-frame centre (x 64..87,
y<24) on full-path frames — alien/ball flicker; now runs before the
compose, frame-12 A/B 0 px, all gates green at the documented floor.
(3) A methodology trap: multi-checkpoint A/B timelines re-randomize the
counter phase at each halt (lessons.md, third instance) — the "frame
100 / 713 px" scare was that artifact, not a bug.

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
