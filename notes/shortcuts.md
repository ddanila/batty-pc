# Shortcuts — technical debt to repay before we drown in the original code

The level-1 static render is pixel-identical against the GT capture,
but a lot of what gets to that pixel happens via *shipped artifacts*
rather than *ported logic*. This file is the running list of those
shortcuts so we can find our way back when it's time to do them
properly.

Rule of thumb: each shortcut here MUST be repaid before its area
becomes dynamic. Static scenes tolerate "ship the pixels"; gameplay
requires real rendering.

## 1. Brick rendering bypassed (biggest cheat)

**What we ship**: `assets/brick_bitmaps.bin` — 43 200 B = 15 levels ×
180 cells × 16 bytes/cell. The **final composited per-cell bitmap**
extracted from each level's GT capture (via
`scripts/extract_brick_bitmaps.py`).

**What the original does**: `sub_b765h` runs a multi-pass
neighbour-aware compositor:

- `IX` walks a list at `0xAF6F` of 16-bit source-pointer values:
  `0xAF0F`, `0xAF4F`, `0xAF1F`, `0xAF5F`, `0xAF2F`, `0xAF3F`,
  `0xAF3F`, `0xAEFF`. Each entry's value is a pointer to a 16-byte
  sprite.
- Per IX entry: `sub_ad8fh` paints all 180 cells with that one
  sprite (via `sub_adach` / `sub_adbch`), skipping cells where
  `(IY+0) & 0x90 != 0`.
- When the IX value equals the sentinel `0xAF3F`, a side pipeline
  runs: `lb73fh` walks the cell list at `(0x9789)` looking for the
  first un-skipped cell, then calls `sub_c101h` with `IX = 0xC0B8`
  to paint via a different mechanism (SMC-toggled).
- The eight passes compose into the final brick shape; cells with
  bit-4 set get their pixels only from the sentinel pipeline.

**Why we cheated**: the multi-pass compositor + SMC + the `0xC0B8`
side pipeline is days of Z80 reverse-engineering. Pixel-perfect
rendering was achievable in one session by extracting the
composited result.

**When to repay**: BEFORE Phase E (motion / gameplay). The same
compositor runs per-frame during play to redraw destroyed bricks,
spawn power-ups, etc. We can't fake bricks once they're changing.

**Proper port**:
1. Map every cache slot the IX list points into; identify each
   16-byte sprite.
2. Port `sub_adbch` — the 16x8 blitter that consumes (IX) source
   pointer and IY skip flags.
3. Port `sub_ad8fh` / `sub_adach` — the 12×15 grid walker.
4. Port `sub_b765h` — the multi-pass driver with sentinel handling.
5. Port `sub_c101h` + IX=0xC0B8 — the bit-4 cell side pipeline.

## 2. Frame / bat / lives are L1-specific raw-pixel composites

**Status**: partially repaid — the FRAME now uses per-level attrs
from the extended `level_attrs.bin` (24 char-rows). Bg tile per-
cycle (yellow/green/cyan/white). Bat and lives still use the L1
bg attr (which IS the level's bg attr, so colour is right) but
the pixel-block is L1-only — wrong-position for L2..L15 where the
bat might start elsewhere.

**What we ship**:
- `frame_l1.bin` (1764 B) — top + 3-col left + 3-col right strips
  lifted from L1.scr. Pixel bits used as-is; attrs sourced per-level
  from `level_attrs` at render time.
- `bg_tile.bin` (128 B = 4 cycles × 32 B) — hex pattern tile, one
  bitmap per 4-level colour cycle.
- `bat_l1.bin` (95 B) — 5×19 px block including bat + ball + shadow.
- `lives_l1.bin` (16 B) — the second life indicator (2×8 px).
- All rendered via `paint_block()` / `paint_strip()` byte-for-byte.

**What the original does**: the frame is drawn by routines we haven't
traced; the bat is painted by `sub_b5f8h`-style rectangle blitter
using a position pair (analogous to `(l92BDh)` / `(l92BFh)` for the
menu indicators). The lives indicator follows the same pattern with
position from the lives counter byte.

**Why we cheated**: cheap pixel-perfect win for the static L1 view.
The "frame" we extracted from GT also captures the HUD scores and
side-edge attributes; treating it as one raw blob saved us decoding
each sub-element.

**When to repay**:
- Other levels — if `make run`'s L2..L15 cycle shows broken frame
  colours, we need per-level frame data OR a proper frame painter
  that uses per-level attrs.
- Bat motion — needs the bat sprite separated from the ball-on-bat
  composite, plus a position state byte.
- Lives counter — needs to vary with the `(0xB7E6)` / equivalent
  counter byte.

**Proper port**:
1. Find the level-init frame painter in the 0xBA4C call chain.
2. Find the bat sprite source (probably one or two cache slots).
3. Find the bat position state and the per-frame bat update.
4. Find the lives counter source and the lives-indicator painter.

## 3. Per-level attribute band lifted from GT

**What we ship**: `assets/level_attrs.bin` — 5760 B (15 × 12 char-rows
× 32 cols) of ZX attribute bytes pulled from char rows 2..13 of
each `level_NN.scr`.

**What the original does**: an LDIR at `0xBA78` copies `0x8B` (139)
bytes from `0xD90B` to `0x5A0B`. `0xD90B` is RAM populated by an
earlier step in the level-init chain. There's surely a per-level
palette/pattern table in the blob that drives this.

**Why we cheated**: same pattern — extracting from GT was the
fastest path to colour-correct rendering.

**When to repay**: when we port the level-init chain at `0xBA4C`,
the attribute source becomes obvious (probably a small per-level
palette + a row-pattern table).

## 4. HUD scores are frozen at L1-start values

**What we ship**: the `frame_l1.bin` asset includes the rendered
`1UP 000000 / HI 100000 / 2UP 000000` text. When we move to other
levels via `make run`, the scores are still these L1-start values
because the frame is L1-only.

**What the original does**: scores live in RAM (probably `(0x...)`
addresses we haven't located yet), and a markup-rendered overlay
draws them every frame via `sub_b796h` or similar.

**Why we cheated**: static rendering is what state4_level1 tested
against; the scores happened to be the L1-start values in our GT
capture so they "matched" trivially.

**When to repay**: as soon as gameplay starts. The score increments
on brick hits, so frozen scores would visibly fail.

**Proper port**:
1. Find the score state bytes.
2. Render via the same markup pipeline the menu/hi-score uses
   (we already have that code from earlier work; just need the
   right records and source state).

## 5. Bat position hardcoded

**Hardcode**: `BAT_X_PX = 112`, `BAT_Y_PX = 167` (L1 starting
position).

**Original**: bat position byte(s) in RAM, updated per frame by
the input handler.

**When to repay**: first thing in Phase E (bat motion). The bat
must move with input.

## 6. Sprite cache is now dead weight  ✅ RESOLVED

Dropped in the per-cycle-tile / per-level-attrs commit. 3.5 KB
freed; no code reads from it.

## 7. Frame's "shadow" band is conflated with the frame

The 3rd column of each side strip (col 2 left, col 29 right) is the
shadow the original casts inside the frame edge. We stuffed it
into the same `frame_l1.bin` asset for convenience. Strictly the
shadow is a separate rendering effect (probably driven by an
attribute band, not pixels). Same for the brick-row drop shadows
the user flagged — those are pixel-baked into our `brick_bitmaps`
asset rather than computed.

**When to repay**: when we touch frame / bricks in gameplay
(per-frame brick destruction must update the shadows).

## Priority matrix

| Shortcut                                  | Effort  | Blocking what?                  |
|-------------------------------------------|---------|---------------------------------|
| 1. Brick compositor                       | Days    | Phase E (gameplay) brick hits    |
| 2a. Per-level frame (attrs ✅, pixels ❌)  | Hours   | L2..L15 visual correctness       |
| 2b. Bat sprite + position byte            | Hours   | Phase E (motion)                 |
| 2c. Lives counter wiring                  | Hours   | Lives changing during play       |
| 3. Attribute source                       | Hours   | Bundled with #1 cleanup          |
| 4. Dynamic scores                         | Hour    | First brick hit during play      |
| 5. Bat position runtime                   | Bundled with 2b                            |
| 6. Drop sprite_cache  ✅                  | done    | -                                |
| 7. Frame / brick shadow theory            | Bundled with 1 + 2a                        |

## L2..L15 sweep status

Headline diff numbers per level (full-playfield RGB diff), progress
across the visual-parity grind:

```
              raw     per-cycle  per-half  mid-band  full-clean
                     tile+attrs   attrs    cleaning
L 1:  0.00%   0.00%   0.00%      0.00%     0.00%     0.00%
L 2: 24.93%   5.72%   5.59%      6.03%     2.66%
L 3: 22.95%   7.27%   6.61%      6.91%     3.86%
L 4: 28.35%  14.37%  13.07%      8.18%     5.44%
L 5:  9.01%   7.69%   7.69%      4.83%     2.76%
L 6: 28.55%  12.19%  12.19%     10.10%     4.70%
L 7: 28.53%  13.24%  11.16%      5.54%     2.85%
L 8: 25.13%  11.05%   9.62%      7.08%     3.98%
L 9:  7.83%   6.95%   3.82%      2.87%     1.77%
L10: 26.80%   7.65%   6.48%      5.98%     3.00%
L11: 30.41%  15.47%  12.74%      5.49%     2.94%
L12: 24.89%  10.31%   8.75%      6.65%     3.84%
L13:  8.45%   7.18%   7.18%      6.07%     2.28%
L14: 29.02%  11.40%   9.71%      7.94%     4.81%
L15: 31.39%  17.52%  12.83%      7.87%     4.09%
```

Max diff: 31% raw → 5.44% after full clean. Average: from ~22%
raw to ~3% after full clean.

Residual breakdown (verified on L4 / L6 per-region counts):

- **Brick drop-shadow bands**: 1-px dark lines below each brick row
  rendered by a separate pass in the original. Our brick_bitmaps
  capture only the 8-px brick body, not the shadow row that the
  original paints between rows.
- **Bat-X drift**: snap3's bat X varies slightly between captures
  because the level-init at 0xBA4C doesn't reset the bat position;
  the cleaning replaces snap3's bat region with L1's bat, but the
  actual GT capture had it at a slightly different X.
- **A handful of fine brick boundary pixels** where our per-half
  attr split still under-captures the original's painting order.

The patched-capture method's residual artifact zone is now well-
characterised; pushing below ~3% / level requires either:
  (a) a real playthrough capture of each level (no more
      patched-capture), or
  (b) porting the brick drop-shadow pass from the original
      (bundled with shortcut #1 - the multi-pass brick
      compositor).

## What's NOT a shortcut

For the record, these parts ARE properly ported:
- ZX → VGA palette mapping (15-colour bright ZX directly to VGA DAC).
- Hex-tile background tiling (16×16 1bpp tile applied with per-level
  attr — same byte source the original uses).
- The level data table itself (`0x6CBD` pointer table → 180 B raw
  cells per level; we extract and use the actual ROM data).
- Menu / title / hi-score rendering — all driven by the same markup
  records the original uses (`MENUMARK.BIN` / `MARKUP.BIN`).
- The font (extracted from blob 0x6A15+ as 6×8 glyphs).
- 50 Hz timing approach is documented but not yet implemented
  (`notes/plan-gameplay.md` Phase D).
