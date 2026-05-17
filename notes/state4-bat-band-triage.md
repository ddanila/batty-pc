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

Done (loop iter 1): removed the L6853 lives-skip patch — modded-batty
GT now paints lives the same way our `render_lives` does. Test went
from 427 → 507 px because our `render_lives` output disagrees with
the original's by ~160 px.

## Iter 2 finding: `assets/frame_l1.bin` had lives baked in

The original captured frame asset had the lives icons baked into the
left side strip (bytes at y=185..190 byte_x=1..2 were the lives-icon
pixel pattern from whatever GT it was extracted from). So
`paint_frame_to_buff` painted the lives, then `render_lives` painted
them again on top, double-blitting.

Re-extracted `frame_l1.bin` from a no-lives GT (temporarily re-apply
the L6853 patch → re-run `capture_levels_modded.py` → re-run
`extract_frame.py` → remove L6853 patch → re-run
`capture_levels_modded.py` to restore the lives-included test GT).
The clean frame_l1 at y=185..190 byte_x=0..2 is now `$9F $7F $FE`
(uniform side-strip ornament + hex bg, no lives icon pixels).

Also dropped the Makefile rule that auto-regenerated frame_l1.bin from
`build/level_gt/level_01.scr` — the asset is checked-in now (those two
pipelines have contradictory requirements: extraction needs the
lives-skip patch on, test GT needs it off).

## Iter 2 finding: lives-blit still off (507 px unchanged)

With a clean frame_l1.bin the diff stayed at 507 px. So the residual
isn't double-blit; it's that `render_lives` itself produces different
scr_buff bytes than the original's `print_obj_to_buff`.

Empirically derived from the GT:

| sprite pair      | clean bg | original output | our output      |
|------------------|----------|-----------------|-----------------|
| (`$1F`, `$00`)   |   `$7F`  |     `$60`       | `$7F` (unchanged) |
| (`$FE`, `$00`)   |   `$FE`  |     `$00`       | `$FE` (unchanged) |
| (`$3F`, `$0B`)   |   `$7F`  |     `$4B`       |   ?             |
| (`$FF`, `$F4`)   |   `$FE`  |     `$F4`       |   ?             |

Formula `(~mask & screen) | (mask & pix)` reproduces every original
output exactly. That's the standard "where mask=1 take pix, else
preserve screen" sprite blit. Our `blit_masked_to_scr_buff_ptr` uses
`(mask | screen) ^ pix` — which is what `notes/blitter-port.md`
claims, but doesn't match empirically.

Reconcile with the disasm: `byte_put_width_2` at `original/disasm/
batty.asm:1087` is `LD A,E; OR (HL); XOR D`, i.e. `(E | screen) ^ D`.
The mapping of E/D back to "mask/pix" needs another look — the
DEFB ordering vs POP DE little-endian convention may be inverted from
what we assumed in our blit.

## Iter 3: blit formula fixed — diff 507 → 10 px

Replaced the inner-line of `blit_masked_to_scr_buff_ptr`
(`src/main.c:1126,1130`) from `(mask | screen) ^ pix` to
`(~mask & screen) | (mask & pix)` — the standard "where mask=1 take
pix, else preserve screen" sprite blit, matching all four empirical
GT data points.

state4 went from 507 px → 10 px. state5_bat_band from 6.19% to 0.12%.
states 1-3 still pixel-identical (no regression in the menu/hiscore
paths, which also use this blit for the markup text).

Old `blitter-port.md` documented the formula as `(mask | screen) ^
pix` — that doc was wrong. The disasm's `byte_put_width_2` is
`LD A,E; OR (HL); XOR D`, but mapping E/D back to "mask"/"pix" via
POP DE little-endian doesn't reproduce the empirical output either —
something about the original's SP setup must shift which byte gets
treated as which role. Empirics > disasm-reading-by-hand in this case.

## Iter 4: ball/bat blit order swap — diff 10 → 3 px

The 7-px diagonal smudge at (x=139..142, y=174..177) was the ball
sprite's lower 5 rows (rows 7..11 of the 12-row sprite) **punching
holes through the bat top**. Those rows are the "ball-resting-on-bat"
mask: drawn AFTER the bat, they clear the bat's ink bits via
`(~mask & screen) | (mask & pix)` where `pix=0` → bits go to 0.

Empirically verified: blitting ball BEFORE bat reproduces the GT byte
exactly:

```
bat then ball: $C0 (our actual before fix)
ball then bat: $D8 (GT)
```

Fix: swap the order in `redraw_full_with_ball` (`src/main.c:3675-3678`)
so the ball blits first and the bat overlays it. Secondary balls
(`ball2`, `ball3`) still render after the bat — they can land
anywhere, not just sitting on the bat.

## Iter 5: pin running_dot phase + fix bat_w (3 → 0 px)

Two fixes, both in `render_running_dot` (`src/main.c:1567+`):

1. **Phase pin.** Under `BATTYALL`, reset `run_dot_frame = 0x0E` at the
   top of the function. The GT was captured after one gameplay-loop
   iter, so the original's running_dot punched once with frame=14.
   Our test reaches the screendump after many iters, with the frame
   counter at some non-deterministic phase. Pinning matches the GT's
   single-iter capture.

2. **Bat logical width.** Object_bat_1+$0C = `$1C` = **28**, not 32.
   The bat sprite is 32 px wide but the original uses 28 for the
   mirror dot's position calc (so the dots sit inside the body cap,
   not at the tapered sprite ends). Our `bat_w = 32 + 2 *
   bat_extra_px` placed the second dot at `BAT_X + 32 - 14 - 1 = 133`
   instead of `BAT_X + 28 - 14 - 1 = 129`. Fixed to `bat_w = 28 + 2 *
   bat_extra_px` (and 28 instead of 32 for big-bat too).

Result: state4 0 px. `state5_bat_band.assert_match` flipped to True
— the bat band is now FAIL-gated against any future regression.

## Journey from 507 → 0 px

| Iter | Fix                                              | px diff |
|------|--------------------------------------------------|---------|
|  0   | Initial (after rev'ing modded-batty for bat GT)  | 507     |
|  2   | Re-extract clean frame_l1.bin (no baked-in lives)| 507     |
|  3   | Blit formula: `(~mask & screen) | (mask & pix)`  | 10      |
|  4   | Render ball BEFORE bat in `redraw_full_with_ball`| 3       |
|  5   | Pin run_dot_frame + bat_w = 28 (not 32) in dots  | 0       |

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
