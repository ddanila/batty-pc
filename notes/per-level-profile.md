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
