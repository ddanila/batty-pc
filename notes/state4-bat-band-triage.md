# state4 / state5 bat-band diff — triage

After bumping the modded-batty spin trap to PC 0xBB61 (post-paint, see
[`modded-batty.md`](modded-batty.md)), state4 / state5_bat_band report
**427 px** of diff. All of it sits in the bat band (y=160..192). This
note decomposes those 427 pixels into specific causes and what to do
about each.

## Per-region breakdown

Measured directly against `build/level_gt/level_01.scr` and
`build/test_visual/state4_level1.ppm`:

| Region                              | Diff   | What it is |
|-------------------------------------|--------|------------|
| Lives indicators (x=0..32)          |  54 px | We draw two lives icons; the modded-batty GT NOPs `print_lives_indicator`. |
| Left bg (x=32..104)                 |  26 px | Falloff: shadow-row pixels + ball stuck-on-bat in slightly different cells. |
| Bat zone (x=104..160)               | 347 px | The bat sprite itself + ball above it + shadow rows. Real bat-render drift. |
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

## Lives-indicator overhead

Our `render_lives` (`src/main.c:1611`) always paints the lives icons
at the bottom-left during gameplay. The modded-batty GT patches both:

- line 6853: `JR Z,LBE8B_10` → `JR LBE8B_10` (skip the lives draw
  gate), and
- the `LBE8B_11` region's three `CALL print_obj_to_buff` (1UP / 2UP /
  HI labels — also painted via this routine).

So the GT *deliberately* has no lives indicator while ours does. Two
ways to resolve:

1. Suppress `render_lives` when `BATTYALL=1` (test mode), same trick
   we already use for `test_mode_pin_blink`. Cheap. Catches genuine
   lives-drawing regressions when running `make run`, hides them
   under test. Tradeoff: same blind-spot pattern the testing.md
   lesson is warning against.
2. Recapture the GT with `render_lives` *enabled* by adjusting the
   modded-batty patches (remove the line 6853 patch, restore the
   LBE8B_11 lives-portion CALL). Cleanest — keeps test honest.

Recommend (2). The patch list in `scripts/build_modded_batty.py` is
small; the cost is one more modded-batty build + GT recapture cycle.

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
