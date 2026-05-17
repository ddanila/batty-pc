# Blitter architecture port — scr_buff + attr_buff pipeline

We mirror the original game's frame-paint architecture:

1. **scr_buff** (6144 B) holds 1-bit pixel data, 32 bytes × 192 rows.
   Mirror of ZX `0xDA00..0xF1FF` (which the original copies to `0x4000`).
2. **attr_buff** (768 B) holds per-cell attrs, 32 × 24. Mirror of
   `0xD700..0xD9FF` (copied to `0x5800`).
3. **buff_to_vga** is the single final pass that walks scr_buff bits
   and emits each pixel via the surrounding char cell's attr_buff
   entry (ink for set bit, paper for clear).

Going through scr/attr buffers (rather than direct-to-VGA) is what
makes per-cell bg-attr inheritance automatic and the mask=0/pix=1 XOR
case (bat shadow band) work uniformly across every renderer.

## The blit primitive

```
scr_buff' = (~mask & scr_buff) | (mask & pix)
```

Standard "where mask=1 take pix bit, else preserve screen" sprite blit.

| mask | pix | result          | semantic              |
|------|-----|-----------------|-----------------------|
| 1    | 0   | bit = 0         | sprite ink (= bit cleared; buff_to_vga renders cleared = ink for default attr ordering) |
| 1    | 1   | bit = 1         | sprite paper          |
| 0    | x   | bit = screen    | transparent (preserve)|

An earlier draft of this note (and the corresponding C code) said the
formula was `(mask | screen) ^ pixel`. That's incorrect — see
`state4-bat-band-triage.md` for the empirical derivation: the four
`(mask, pix, screen → output)` triples from the level-1 GT confirm
the `(~mask & screen) | (mask & pix)` form.

`blit_masked_to_scr_buff_ptr` in src/main.c handles non-byte-aligned
x with a per-row shift across two destination bytes. Mirrors the ZX
pre-shift table at $F200 (`table_shifts`) but computed at runtime.

## Compose order — see `redraw_full_with_ball`

The per-frame sequence is the obvious one: bg → bricks → frame →
sprites → buff_to_vga → HUD text on top. Read
`redraw_full_with_ball` directly; the only non-obvious bit is that
`paint_frame_to_buff` runs AFTER `render_brick_band` so frame
attrs override the leftmost / rightmost brick's body attrs at the
side-strip cells, and BEFORE sprites so they OR-blit cleanly over
the frame.

Bat + enemy cells force `bg_attr` via `blit_sprite_attrs_to_buff`
so the sprite stays bg-coloured even when its bbox overlaps frame
side-strip cells (whose attrs would otherwise tint the sprite).

## state4_level1 residual diff

state4 sits at ~228 px against the captured ZX GT, concentrated in
the bat+ball overlap region (y=160..191, x=112..151). The GT was
captured at a frame where neither the bat nor the ball was drawn,
so the residual is the bat/ball/lives indicator our renderer paints
over an empty bg in the same cells. Floor without recapturing GT
mid-render.

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

## Original source references

- `sub_94BC` — the inner blit (`(mask | screen) ^ pixel`).
- `print_obj_to_buff` at $9374 — SP-based fast masked-sprite blit
  that writes to scr_buff (mirror of our blit_masked_to_scr_buff_ptr).
- `print_sprite_pix` at $B539 and `print_sprite_attrib` at $B656 —
  separate pixel / attr blit drivers.
- `game_screen_draw_to_buffer` at $BE6B — the final buff-to-screen
  copy (our `buff_to_vga`).
- `table_shifts` at $F200 — pre-shifted sprite cache for non-aligned x.
