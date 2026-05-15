# Blitter architecture port — scr_buff + attr_buff pipeline

We now mirror the original game's frame-paint architecture:

1. **scr_buff** (6144 B) holds 1-bit pixel data, 32 bytes × 192 rows.
   Mirror of ZX `0xD A00..0xF1FF` (which the original copies to `0x4000`).
2. **attr_buff** (768 B) holds per-cell attrs, 32 × 24. Mirror of
   `0xD700..0xD9FF` (copied to `0x5800`).
3. **buff_to_vga** is the single final pass that walks scr_buff bits
   and emits each pixel via the surrounding char cell's attr_buff
   entry (ink for set bit, paper for clear).

## Why we did this

User asked: *"let's emulate drawing and blitter same as original has,
that would help us in other cases to make game identical to the
original"*. The direct-to-VGA approach we had before forced every
renderer to know its own ink/paper at call time, which:

- Broke bg-attr inheritance (sprites painted with fixed colours, not
  the surrounding cell's attr).
- Made the mask=0, pix=1 XOR case (the bat's textured shadow band)
  invisible, because the direct-VGA blitter collapsed it to "preserve".
- Diverged from the original wherever per-cell attrs varied in the
  brick band (the only zone with non-bg attr).

After the port: 38645 -> 286 px diff on state4_level1 vs the ZX GT
(99.3% closer to original). Remaining 286 px are concentrated at the
frame ornament side strips and the HUD — separate parity work.

## The blit primitive (port of sub_94BC)

```
scr_buff' = (mask | scr_buff) ^ pixel
```

Four cases per bit:

| mask | pix | result          | semantic                       |
|------|-----|-----------------|--------------------------------|
| 1    | 0   | bit = 1         | solid body (ink in buff_to_vga) |
| 1    | 1   | bit = 0         | internal texture (paper)        |
| 0    | 1   | bit toggled     | XOR shadow (flips bg)           |
| 0    | 0   | bit preserved   | transparent                    |

`blit_masked_to_scr_buff_ptr` in src/main.c handles non-byte-aligned
x with a per-row shift across two destination bytes. Mirrors the ZX
pre-shift table at $F200 (`table_shifts`) but computed at runtime.

## Render order (per frame)

```
paint_bg_to_buff(level_idx)        // bg tile -> scr_buff, bg_attr -> attr_buff
render_brick_band(level_idx)       // bricks -> scr_buff, brick attrs -> attr_buff
render_bat / render_lives          // bat + lives -> scr_buff (XOR shadow!)
buff_to_vga()                      // single pass: scr_buff + attr_buff -> VGA
render_frame(cycle, level_idx)     // frame ornament direct-VGA on top
```

For bat-only redraws (`redraw_bat`), the same pipeline runs on a
y-strip only: `paint_bg_strip_to_buff` + `render_bat` +
`render_lives` + `buff_to_vga_strip`.

## Migration status

All gameplay renderers now go through the scr_buff + attr_buff
pipeline. Direct-VGA only for the final overlay layer:

- bg tile + per-cell attrs (`paint_bg_to_buff`)
- bricks (via `print_briks_c`)
- bat + lives (`render_bat`, `render_lives`)
- ball (`render_ball_to_buff`)
- bomb, 400pts, alien (inline `blit_masked_to_scr_buff` calls in
  `redraw_full_with_ball`)
- bonus (`render_bonus_to_buff` + `blit_sprite_attrs_to_buff` for
  the per-type ink)

Direct-VGA on top of `buff_to_vga`:

- `render_hud_score` / `render_hud_powerups` (white text)
- `render_frame` (per-level attrs from level_attrs.bin)

## state4_level1 residual diff

After all the migrations, state4 vs the captured ZX GT sits at
194 px (down from 38645 pre-pipeline). The residual is entirely in
the bat+ball overlap region (y=160..191, x=112..151). Confirmed by
inspecting the GT snapshot: it was captured at a frame where neither
the bat nor the ball was drawn — pure bg pattern at the bat zone.

This means the test has reached the limit of comparability under the
current GT; lowering it further would require recapturing the GT
with the bat / ball / lives drawn in matching positions, separate
from the rendering-pipeline parity work.

## What this enables

Now that every renderer writes into scr_buff via the original
`(mask | screen) ^ pixel` blit:

- The XOR shadow case (mask=0, pix=1) works uniformly — visible on
  the bat shadow rows.
- Per-cell bg-attr inheritance is automatic for every sprite — no
  more passing `bg_attr` to renderers.
- Colour-clash artefacts (bonus tinting bg in its char cells) match
  the original.
- Future ports can write 1-bit pixel data and per-cell attrs and
  trust the single `buff_to_vga` pass to handle the rest.

## Frame ornament in scr_buff (compose order)

After porting `render_frame` to a buffer-pipeline variant
(`paint_frame_to_buff` + `paint_strip_to_buff`), the per-frame
compose order is:

```
paint_bg_to_buff      -> bg tile + bg_attr fill
render_brick_band     -> brick body pixels + per-cell attrs
                         (destroyed bricks reset their cell attrs
                          to bg_attr so they actually disappear)
paint_frame_to_buff   -> frame top + side strips (must run AFTER
                         render_brick_band so frame attrs override
                         the brick body attrs at the leftmost /
                         rightmost cell, and BEFORE sprites so
                         they OR-blit over the frame pixels)
render_bat / render_lives
render_ball_to_buff
render_bomb / render_400pts / render_alien   (all to scr_buff)
render_bonus_to_buff
buff_to_vga
render_hud_score / render_hud_powerups       (direct-VGA text)
```

Bat + enemy cells force `bg_attr` in attr_buff (via
`blit_sprite_attrs_to_buff`) so they stay bg-coloured even when
their bbox overlaps frame-strip cells or brick cells whose attrs
would otherwise tint the sprite.

## Brick types (1-hit / multi-hit / undestructible)

Cell encoding in the level data:

| bit | meaning                                        |
|-----|------------------------------------------------|
| 7   | no brick at this cell (skip, or already gone) |
| 5   | undestructible — bounce, never destroy        |
| 4   | "this hit destroys" (1-hit OR multi-hit's     |
|     |  final hit, already registered)               |
| 0–3 | low nibble = colour index into briks_colors[] |

Collision flow (`brick_collision` in src/main.c):

- bit 7 set: pass through.
- bit 5 set: bounce, no destruction.
- bit 4 set: destroy (score + sound + set bit 7).
- otherwise (multi-hit fresh): SET bit 4, bounce — next hit will
  match the bit-4 branch and destroy.

L1 cells like `$13` / `$14` (bit 4 set, bit 5 clear) are 1-hit
destructible; L1 row 0's `$07` cells (both bits clear) are 2-hit
multi-hit; L4 `$2C` (bit 5 set) is undestructible decoration.

## Key files

- `src/main.c::blit_masked_to_scr_buff_ptr` — the OR-blit primitive
- `src/main.c::paint_bg_to_buff` / `buff_to_vga` — the pipeline
- `src/main.c::paint_bg_strip_to_buff` / `buff_to_vga_strip` —
  partial-strip variants for `redraw_bat`
- `src/main.c::render_level_screen` / `redraw_bat` — the two
  callers that drive the buffer pipeline

## Original source references

- `sub_94BC` — the inner blit (`(mask | screen) ^ pixel`).
- `print_obj_to_buff` at $9374 — SP-based fast masked-sprite blit
  that writes to scr_buff (mirror of our blit_masked_to_scr_buff_ptr).
- `print_sprite_pix` at $B539 and `print_sprite_attrib` at $B656 —
  separate pixel / attr blit drivers.
- `game_screen_draw_to_buffer` at $BE6B — the final buff-to-screen
  copy (our `buff_to_vga`).
- `table_shifts` at $F200 — pre-shifted sprite cache for non-aligned x.
