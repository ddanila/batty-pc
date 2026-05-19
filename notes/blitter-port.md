# Blitter architecture — scr_buff + attr_buff pipeline

We mirror the original game's frame-paint architecture:

1. **scr_buff** (6144 B) — 1-bit pixel data, 32 bytes × 192 rows. Mirror
   of the original's offscreen pixel buffer at `0xDA00..0xF1FF` (which
   the original copies to `0x4000` = ZX screen pixels).
2. **attr_buff** (768 B) — per-cell attrs, 32 × 24. Mirror of `0xD700..0xD9FF`
   (copied to `0x5800` = ZX screen attrs).
3. **buff_to_vga** — single final pass that walks scr_buff bits and emits
   each pixel via the surrounding char cell's attr_buff entry (ink for
   set bit, paper for clear).

Going through scr/attr buffers (rather than direct-to-VGA) is what
makes per-cell bg-attr inheritance automatic and the mask=0/pix=1 XOR
case (bat shadow band) work uniformly across every renderer.

## The masked-blit primitive

Our C-side `blit_masked_to_scr_buff_ptr` uses:

```c
screen' = (~mask & screen) | (mask & pix)
```

Standard "where mask=1 take pix bit, else preserve screen". Truth table:

| mask | pix | result          | semantic              |
|------|-----|-----------------|-----------------------|
| 1    | 0   | bit = 0         | sprite paper (= cleared) |
| 1    | 1   | bit = 1         | sprite ink (= set)       |
| 0    | x   | bit = screen    | transparent (preserve)   |

The original Z80 binary uses the equivalent `(mask | screen) ^ pix`
arrangement in `byte_put_width_N` ($99EB) and a table-driven shifted
variant in `byte_put_width_shift_N`. The shifted variant indexes
pre-shifted entries from `table_shifts` (built at boot at `0xF200`),
so its `OR`/`XOR` operands are transformed — the **output** matches
our direct-bitops formula, the intermediate operands don't. Verified
empirically against per-level GTs.

`blit_masked_to_scr_buff_ptr` in `src/main.c` handles non-byte-aligned
x with a per-row shift across two destination bytes — same shape as
the original's table-driven path, computed at runtime in C.

## Compose order

`render_level_screen` in `src/main.c` does:

```
paint_bg              -> bg pattern + bg_attr in attr_buff
render_magnets        -> ON sprite always, OFF overlay for slots 2-3
inner_border_line_c   -> 1 px inner border at byte 1 / byte 30
render_brick_band     -> level_attrs copy + print_briks_c + print_border_shadow_c
render_brick_flash    -> brick-destroyed flash (mid-game only)
paint_frame_to_buff   -> HUD strip + side strip (from frame_l1.bin)
render_bat            -> bat at BAT_X, BAT_Y_PX
render_lives          -> lives indicator at y=185
buff_to_vga           -> final scr/attr → VGA
```

`paint_frame_to_buff` runs AFTER `render_brick_band` so its side-strip
attrs override the leftmost / rightmost brick's body attrs at the
side-strip cells, and BEFORE sprites so they OR-blit cleanly over
the frame. Bat + enemy cells force `bg_attr` via
`blit_sprite_attrs_to_buff` so the sprite stays bg-coloured even when
its bbox overlaps frame side-strip cells.

During gameplay redraws, the VGA page is not cleared before
`buff_to_vga`. The original loop saves/restores object regions in
`scr_buff` / `attr_buff`, prints active objects into those buffers,
then copies the ZX-format buffers to screen (`buff_to_screen_pixs` and
`buff_to_screen_attrib`). Clearing VGA between frames is a port-only
artifact and causes visible flicker, especially while a falling bonus
forces full-frame redraws.

The original also does not draw persistent HUD letters for active
bonuses. `get_bonus` updates object state, score, sound, bat/ball
state, and changes the caught bonus object into the floating points
sprite; any separate current-bonus letter overlay is port-only.

The normal DOS build draws the in-game score HUD (`1UP`, `HI`, `2UP`
and their six-digit counters) into `scr_buff` before `buff_to_vga`, so
it follows the same ZX-format buffer path as the rest of gameplay. The
visual-test executable is compiled with `BATTY_SCORELESS_HUD` because
the GT capture pipeline intentionally NOPs the original score block.

`assets/sprites.bin` must include the full `gfx_bonuses` tail through
`spr_bonus_triple_ball` (`$8CEA..$8D46`). A shorter extraction leaves the
triple-ball sprite truncated near the end of the blob, making its
falling bonus render from partial data plus zero-filled memory.

(Note: the original game's order is `paint_bg -> paint_frame ->
inner_border -> ... -> magnets -> bricks`. Our order is functionally
equivalent for the test GT moment because `paint_frame_to_buff` only
overwrites the side-strip cells and the HUD area — and the test GT
captures a frame where neither needs the original order's invariants
to hold. The HUD-area magnet residuals on L6/L12 hint at a case where
reordering might help, but iter-37 verified it doesn't change the
captured PPM in practice — likely a timing / sentinel-write quirk
explored in `per-level-profile.md`.)

## Brick types (1-hit / multi-hit / undestructible)

Cell encoding in `assets/levels.bin` (180 B/level, 12 rows × 15 cols):

| bit | meaning                                        |
|-----|------------------------------------------------|
| 7   | no brick (`$80`) OR empty sentinel (`$C0`)    |
| 5   | undestructible (e.g. `$2B`, `$2C`, `$2E`)      |
| 4   | "this hit destroys" — 1-hit OR multi-hit's     |
|     |  final hit (e.g. `$11`..`$15`)                 |
| 0–3 | low nibble = colour index into `briks_colors[]` |

Collision flow (`brick_collision` in `src/main.c`):

- bit 7 set: pass through (skip).
- bit 5 set: bounce, no destruction.
- bit 4 set: destroy (score + sound + set bit 7).
- otherwise (= fresh multi-hit): SET bit 4, bounce — next hit's
  bit-4 branch destroys.

L1 row 3's `$13`/`$14` (bit 4 set) are 1-hit destructible; L1 row 0's
`$07` cells (= bit 4 clear) are 2-hit multi-hit; L5 `$2E` (bit 5 set)
is undestructible metal.

### Runtime destroyed-cell attr cleanup

`level_attrs.bin` is captured with all bricks alive, so the brick band
starts each repaint with body attrs and shadow attrs already present.
For runtime-destroyed cells (`cell & 0xC0 == 0x80`), `render_brick_band`
resets both the body attr cells and the row directly below to the level
background attr before calling `print_briks_c`.

That shadow-row reset is unconditional. If a live brick sits below the
destroyed cell, `print_briks_c` writes that lower brick's body attr
afterward. If the row below is empty or sentinel-only, the stale dimmed
shadow attr disappears with the destroyed brick instead of tinting the
background.

## Original source references

- `byte_put_width_N` at $99EB — the unshifted masked-blit primitive.
  Opcode pattern `POP DE; LD A,E; OR (HL); XOR D; LD (HL),A; INC L`
  (verified against `original/disasm/tools/batty_for_compare.sna`).
- `byte_put_width_shift_N` at $99FE — the shifted-blit primitive. Uses
  `table_shifts` lookups for pre-shifted mask/pix.
- `print_obj_to_buff` at $A35C — top-level masked-sprite blitter that
  dispatches via the patched `JP put_byte_N` to the right width.
- `game_screen_draw_to_buffer` at $BE6B — the initial paint pipeline.
- `print_magnets` at $8D4C — magnet rendering ($06 = ON, $07 = OFF
  per `gfx_screen_elements`).
- `print_briks` at $ADE1 — brick rendering.
- `print_border_shadow` at $BFCF — dims cc 1 of cr 1..23 and cr 1 cc
  2..30 (= clears bit 6 / bright). Our `print_border_shadow_c`.
- `table_shifts` at $F200 — pre-shifted sprite-byte lookup table.
