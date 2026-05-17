# state4 / state5 bat-band diff — triage

After bumping the modded-batty spin trap to PC 0xBB61 (post-paint, see
[`modded-batty.md`](modded-batty.md)) and then removing the L6853
lives-skip patch (so the GT *also* paints the lives indicator),
state4 / state5_bat_band report **507 px** of diff. All of it sits
in the bat band (y=160..192). This note decomposes those 507 pixels
into specific causes and what to do about each.

## Per-region breakdown

Measured directly against `build/level_gt/level_01.scr` and
`build/test_visual/state4_level1.ppm`:

| Region                              | Diff   | What it is |
|-------------------------------------|--------|------------|
| Lives icons (x=0..40)               | 160 px | **Both** render now. Our `render_lives` is missing the top + bottom edge rows of each indicator sprite — same off-by-Y shape as the bat-top issue. |
| Left bg (x=40..104)                 |   0 px | Clean. |
| Bat zone (x=104..160)               | 347 px | The bat sprite + ball + shadow rows. Real bat-render drift. |
| Right bg + side strip (x=160..256)  |   0 px | Clean. |

## Bat-zone detail

Per-row diff inside x=104..160:

```
y=166..172:  3-7 px each   (37 total)  — ball-on-bat position
y=173..182: 25-30 px each (281 total)  — bat body
y=183..185: 12-14 px each  (39 total)  — bat shadow rows
```

The bat is at the **same X position** as the GT (BAT_X_INIT = `$74`
= 116, body spans x=116..148). The bat top **starts at the same y
(173)** in both — but the *interior pixels* differ across the entire
body width and height. Side-by-side at y=173 (the top body row), the
GT has 24 px of solid ink (the curved bat-cap); our render has
nothing solid there.

The sprite data in `assets/sprites.bin` at offset `$3AC` is byte-
identical to `original/disasm/gfx/sprites_with_masks_2.asm:389`
(`spr_bat_normal`, header `$04,$0D` = 4 bytes wide × 13 rows). So:

- The sprite asset is correct.
- The bat position is correct.
- Either `blit_masked_to_scr_buff_ptr` (`src/main.c:1097`) is applying
  the `(mask | screen) ^ pixel` formula slightly wrong for shifted-X
  blits, or the order in which we composite the bat against the bg
  hex pattern differs from the original's compose order. The bat at
  BAT_X_INIT = 116 has shift = 4 (116 & 7), so this is a non-byte-
  aligned blit — exactly where shift-handling bugs would show up.

Reproduce locally:

```sh
make test                          # regenerates state4_level1.ppm
python3 - <<'PY'                   # ASCII-scan the top bat row
from pathlib import Path
import sys
sys.path.insert(0, 'scripts')
from test_visual import expected_from_scr, ppm_inner_to_indices, PALETTE_RGB
exp = expected_from_scr(Path('build/level_gt/level_01.scr'))
act = ppm_inner_to_indices(Path('build/test_visual/state4_level1.ppm'))
W = 256
def k(rgb):
    r,g,b = rgb
    return ' ' if r>200 and g>150 and b<100 else ('.' if r>150 else '#')
def scan(idx, y):
    return ''.join(k(PALETTE_RGB[idx[y*W + x]]) for x in range(100, 160))
print('y=173 GT :', scan(exp, 173))
print('y=173 OUR:', scan(act, 173))
PY
```

`build/test_visual/_bat_zone_8x.png` (regenerable from the snippet in
the section "Visual reference" below) shows the two bats at 8× scale
for eyeball comparison.

## Lives-indicator: regression now visible (160 px)

Done (loop iter 1): removed the L6853 lives-skip patch from
`scripts/build_modded_batty.py`. The modded-batty GT now paints the
lives indicator the same way our `render_lives` does. Test went from
427 px to 507 px — a +80 net increase because the original's lives
icons and ours disagree by ~160 px (where before, we were only
"54 px extra" against an empty GT).

Same shape as the bat-top problem: in both the lives icons and the
bat body, the top row + bottom row of the sprite render as blank in
our output, where the GT shows solid ink edges. Middle rows render
but with the interior ink/paper pattern slightly off. Sprite asset
bytes (`assets/sprites.bin` at `SPR_LIVES` = `0x070`, `SPR_BAT_NORMAL`
= `0x3AC`) are byte-identical to the upstream disasm
(`spr_lives_indicator` at `$7AFC`, `spr_bat_normal` at `$7E38`).

This is almost certainly a single bug in
`blit_masked_to_scr_buff_ptr` (`src/main.c:1097`) affecting at least
sprite row 0 and row h-1 — possibly an off-by-one in `for (row = 0;
row < h; row++)` or in how the row's first/last byte-pair is OR-merged
against existing scr_buff bytes. Fix this once and BOTH the lives
icon and bat-top regressions resolve together.

## Plan for the bat-body 347 px

Separately tracked from this audit:

1. Confirm `blit_masked_to_scr_buff_ptr` shift handling against the
   original's `print_obj_to_buff` (sub at `$9374`) for at least one
   shift = 4 case (e.g., spr_bat_normal at x=116). Hand-trace one
   row to verify the byte placement matches.
2. If the bit ops are correct, audit compose order: bat is painted
   AFTER paint_bg_to_buff fills the bg hex pattern, AFTER
   paint_frame_to_buff. The original does the same order — but
   subtle differences in *which cells get their attrs forced to
   bg_attr* could cause the bat body to display with attrs that hide
   ink. Cross-check `blit_sprite_attrs_to_buff` cell coverage.
3. Once those agree, the residual is genuine sprite-data drift — at
   that point the bat-band diff should drop into single digits.

Once the bat-body diff is gone, flip state5_bat_band's `assert_match`
to `True` so any future regression in the bat / ball / lives region
fails the build.
