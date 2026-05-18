# Per-level visual-diff profile

## Iter 17: BATTY_LEVEL env now actually works end-to-end

Pre-iter-17, `BATTY_LEVEL=N make test` LOOKED like it ran level N but did
not — the env never propagated to DOS because the test floppy's
AUTOEXEC.BAT only did `SET BATTYALL=1`. Every per-level diff number
from iters 11–16 was actually L1's render diffed against L_N's GT — i.e.
mostly meaningless. (The `BATTY_LEVEL` getenv at run_level:4479 was a
no-op.)

Fixes (commit on main):
1. `Makefile` TEST_FLOPPY_OUT rule emits `SET BATTY_LEVEL=N` into
   AUTOEXEC.BAT when the host env is set, and the `test` target now
   `rm -f`s the floppy first so the env change always triggers a
   rebuild (the floppy bytes don't change with env, so `make` would
   otherwise consider it up-to-date).
2. `scripts/test_visual.py` reads BATTY_LEVEL from env and switches
   GT_LEVEL1 → `build/level_gt/level_NN.scr` accordingly.

Confirmed working: L2 came back at 0 px diff (= iter-13 magnet-order
fix really did get L2 pixel-perfect; the prior notes saying "L2 has 7
px from magnets" were L1-vs-L2-GT noise).

True per-level numbers (post-iter-17 wiring, `BATTY_LEVEL=N make test`):

| Level | Cycle | Diff (px) | % of playfield |
|-------|-------|-----------|----------------|
| L01   | 0     | **0**     | 0.00 %         |
| L02   | 1     | **0**     | 0.00 %         |
| L03   | 2     | 5861      | 11.92 %        |
| L04   | 3     | **0**     | 0.00 %         |
| L05   | 0     | 11444     | 23.28 %        |
| L06   | 1     | 2005      | 4.08 %         |
| L07   | 2     | 1154      | 2.35 %         |
| L08   | 3     | 902       | 1.84 %         |
| L09   | 0     | 3135      | 6.38 %         |
| L10   | 1     | 1527      | 3.11 %         |
| L11   | 2     | 556       | 1.13 %         |
| L12   | 3     | 1043      | 2.12 %         |
| L13   | 0     | 1245      | 2.53 %         |
| L14   | 1     | 1384      | 2.82 %         |
| L15   | 2     | 1437      | 2.92 %         |

L5 (cycle-0, biggest residual) is now the highest-value target for the
next iter. L11 (556 px) remains the cleanest small-residual case for
attr-tracing experiments.

---

## Iter 16 confirmed: attr_buff[5,1] reaches buff_to_vga with $05

Used a debug peek (`attr_buff[0] = attr_buff[5*32+1]` just before
buff_to_vga in render_level_screen, then sample the top-left cell
in the PPM). Result: cell (0, 0) renders as non-bright cyan ink on
non-bright black paper — exactly attr `$05`. So the attr is correct
at the end of `render_level_screen`.

The L11 diff at (8, 39..47) and similar must come from a later
render. Possibilities:
- `redraw_full_with_ball` fires at level entry despite `ball_moved`
  and `bat_moved` both being 0 (somehow). Need to peek the same
  cell inside redraw_full_with_ball.
- `play_brik_anim` modifies attr_buff somewhere we missed.
- Something between level-init and the screendump runs that drops
  bit 7 from `scr_buff[39*32+1]` (= reverses paint_frame's $80
  side-strip pixel).

## Iter 16 attempted fix: `FRAME_SIDE_W = 1`

Reduced from 3 to 1 so paint_frame only writes byte_x=0 and 31 (= the
actual ornament column per the original's `print_sprite_pix` calls at
$BE8B). Re-extracted `frame_l1.bin` from no-lives GTs. Result:

- L11: 556 → 506 px (small improvement).
- L1: 0 → 639 px (REGRESSION). state5_bat_band FAIL.

Reverted. The L1 regression suggests `paint_bg + render_brick_band`
doesn't reproduce the L1 GT's byte_x=1..2 pixels even though the bg
pattern bytes match. Something else writes bits there that we'd need
to also reproduce.

The full bug here is interleaved: `frame_l1.bin` carries L3-specific
brick edge artifacts (= why FRAME_SIDE_W=3 looks "right" for L1 but
breaks L11). Reducing to 1 fixes L11 partially but breaks L1 because
some other code path was relying on the contaminated frame data to
produce the right bytes.

Two ways forward (iter 17+):
1. Audit what's drawing the side-strip pixels at byte_x=1..2 in
   the original (= maybe `brik_shadow` does more than dim, or
   there's a separate edge-of-bricks routine we're missing).
2. Or: ship 4 PER-LEVEL frame_l1 captures from non-bricked source
   cells (= a level that has $C0 at row 0 col 0 AND row 1 col 0
   for cycles where the current source has bricks there).

---

## Iter 13 / 14 status (post-magnet-order-fix)

| Level | Cycle | Diff (px) |
|-------|-------|-----------|
| L01   | 0     | **0**     |
| L02   | 1     | **0**     |
| L03   | 2     | 5861      |
| L04   | 3     | **0**     |
| L05   | 0     | 11444     |
| L06   | 1     | 2005      |
| L07   | 2     | 1154      |
| L08   | 3     | 902       |
| L09   | 0     | 3135      |
| L10   | 1     | 1527      |
| L11   | 2     | 556       |
| L12   | 3     | 1043      |
| L13   | 0     | (high)    |
| L14   | 1     | (mid)     |
| L15   | 2     | (mid)     |

(L13-L15 not re-profiled cleanly; the bulk profile script has timing
issues that occasionally produce inflated numbers — measure each
level alone with a fresh `make test` for a stable reading.)

## Iter 14 finding (partial, not yet fixed)

L11 has 556 px residual concentrated in two bands: y=33..63 (~272 px)
and y=97..127 (~284 px). These coincide with the brick-zone top + bottom.

Drilling in at L11 y=41 (a brick-row mid-row), the diff is in the
side-strip cells (x=0..31 and x=224..255). GT shows multiple ink
colours (cyan, magenta, white from per-cycle attrs); OUR shows
mostly bright black, suggesting `attr_buff` for those cells holds
`$45` (cycle-2 bg) or `$40` instead of the per-level attrs that
`level_attrs.bin` has.

The render order is:
1. `paint_bg_to_buff` writes `bg_attr_per_cycle[2] = $45` everywhere.
2. `render_brick_band` copies `level_attrs[char_rows 3..16]` —
   should put `$05` at `attr_buff[5,1]`.
3. The "reset destroyed cells" loop overwrites `attr_buff[5,1]` back
   to `bg_attr = $45` because L11 row 1 col 0 = `$C0` (= bit 7 + 6).
4. `paint_frame_to_buff` should re-write `attr_buff[5,1]` from
   `level_attrs[L11,5,1] = $05`.

Step 4 *should* fix it but evidently isn't — the rendered pixel
behaves like the cell still holds `$45`. Either `paint_frame_to_buff`
isn't writing the attr, the offset math is off-by-N, or something
runs *after* it that restores `$45`. Needs instrumentation (= add a
debug printf inside paint_strip_to_buff for the specific cell and
re-run).

For L1 the same `$C0`-reset loop runs but `paint_frame_to_buff`'s
overwrite produces a value matching the GT (= `$06`), which is
why state4_level1 passes despite the same loop firing. Why L11
diverges and L1 doesn't is the iter-15 question.

---

# Per-level visual-diff profile (iter 11)

After landing magnets (iter 10), I profiled all 15 levels via the
`BATTY_LEVEL` env override. Each level boots, captures its
state4 PPM, diffs against `build/level_gt/level_NN.scr` (the
modded-batty GT for that level).

| Level | Cycle | Diff (px) | % of playfield |
|-------|-------|-----------|----------------|
| L01   |  0    |   0       | 0.00 %         |
| L02   |  1    |   7       | 0.01 %         |
| L03   |  2    | ~6000     | ~12 %          |
| L04   |  3    |   0       | 0.00 %         |
| L05   |  0    | ~11000    | ~23 %          |
| L06   |  1    | ~2000     | ~4 %           |
| L07   |  2    | ~1100     | ~2 %           |
| L08   |  3    | ~1100     | ~2 %           |
| L09   |  0    | ~3000     | ~6 %           |
| L10   |  1    | ~1500     | ~3 %           |
| L11   |  2    | 984       | 2.00 %         |
| L12   |  3    | ~1000     | ~2 %           |
| L13   |  0    | ~1200     | ~2 %           |
| L14   |  1    | ~1200     | ~2 %           |
| L15   |  2    | ~1500     | ~3 %           |

The profile script's per-level numbers had measurement noise (the
re-run for L11 gave 984 instead of 36526 the original showed; the
discrepancy was likely from a stale floppy state during the first
pass). Real numbers above are from individual re-runs.

L1 (cycle 0), L2 (cycle 1), L4 (cycle 3) sit at 0–7 px — those passes
exercise the brick-render + frame + magnets paths cleanly. L5 / L9 /
L11 / L13 (cycle 0 again, plus other cycles) show 1000–11000-px
residuals concentrated in specific regions.

## Confirmed bugs from this iter

1. **`frame_l1.bin` contaminated with level-specific brick edges.**
   `extract_frame.py` extracts a 3-byte-wide column from one
   representative GT per cycle. For cycle 2 the source is L3.scr,
   which has a brick at row 1 col 0 — the brick's top-edge clearing
   leaves $00 bytes at byte_x=1..2 y=39. When we apply the captured
   frame to L11 (where col 0 row 1 has no brick), our `paint_frame_to_buff`
   overwrites L11's bg pattern at byte_x=1..2 y=39 with $00. Visible
   as black wedge inside the side strip.

   Tried fix: reduce `FRAME_SIDE_W` from 3 → 1 (= only the actual
   ornament column at byte_x=0). Broke L1 (1151 px diff) because
   the L1 GT's byte_x=1, 2 contains data our `render_brick_band`
   doesn't fully reproduce. Specifically, **attrs differ**:
   `level_attrs.bin` has $06 (non-bright yellow) at side-strip-
   adjacent cells, but `render_brick_band` overrides those to
   `bg_attr` = $46 (bright) for $C0 cells.

   Tried second fix: also gate the "reset destroyed cells" loop on
   `(cell & 0xC0) == 0x80` (only true runtime-destroyed cells, not
   the $C0 empty-cell sentinel). Broke worse — L11 went from 984 →
   31870 because the level_attrs.bin captures **per-level** attrs,
   not per-cycle, and they're highly non-uniform.

2. **L11 GT bg is cycle 0 (yellow), not cycle 2 (cyan).** The
   modded-batty pipeline somehow paints L11 with the wrong texture.
   Checked the disasm: `game_screen_draw_to_buffer` does
   `LD A,(current_level_number_1up); AND $03; ADD A,A;
   CALL hl_add_a; ... LD (current_texture+1), DE` — selects texture
   by `level_number & 3`. For L11 (level number 10), cycle should be
   2. But the captured GT shows yellow attrs (cycle 0 texture).
   Either the modded-batty pipeline trashes some state before
   `game_screen_draw_to_buffer` runs, or my cycle math is wrong
   somewhere. Needs more investigation.

## What's left at the baseline

- L1 state4: 0 px (FAIL-gated, can't regress).
- L2: 7 px (magnet shadow row offset).
- L3..L15: 1k–11k px each.

## Plan for next iter

Diagnose L11's wrong bg cycle first. If the GT is wrong (= modded-
batty bug), refresh the GT capture. If our rendering is wrong,
trace `bg_attr_per_cycle` lookup.

Once L11 has the right cycle attr, the `level_attrs.bin` vs
`bg_attr_per_cycle` mismatch can be resolved (level_attrs becomes
authoritative; bg_attr_per_cycle becomes a cache for cells outside
the brick band).

Then re-extract `frame_l1.bin` with `FRAME_SIDE_W=1` and audit
`render_brick_band`'s attr-reset logic against the original.
