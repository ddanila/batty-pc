# Blitter architecture — the scr_buff + attr_buff pipeline

The port mirrors the original's frame-paint architecture rather than drawing
straight to VGA:

1. **scr_buff** (6144 B) — 1-bit pixel data, 32 bytes x 192 rows. Mirror of
   the original's offscreen buffer at `0xDA00..0xF1FF`, which it copies to
   `0x4000` (ZX screen pixels).
2. **attr_buff** (768 B) — per-cell attributes, 32 x 24. Mirror of
   `0xD700..0xD9FF`, copied to `0x5800`.
3. **buff_to_vga** — one final pass that walks scr_buff's bits and emits
   each pixel via the surrounding char cell's attr_buff entry: ink for a set
   bit, paper for a clear one.

Going through the two planes is what makes per-cell background-attribute
inheritance automatic and the mask=0/pix=1 case (the bat shadow band) work
uniformly across every renderer. The engine itself is `src/zxvga.cpp` —
see `notes/video-engine.md`.

## The masked-blit primitive

`blit_masked_to_scr_buff` uses:

```c
screen' = (~mask & screen) | (mask & pix)
```

Standard "where mask=1 take the pix bit, else preserve screen":

| mask | pix | result | semantic |
|------|-----|--------|----------|
| 1 | 0 | bit = 0 | sprite paper |
| 1 | 1 | bit = 1 | sprite ink |
| 0 | x | bit = screen | transparent |

The original uses the equivalent `(mask | screen) ^ pix` in
`byte_put_width_N` ($99EB), and a table-driven shifted variant in
`byte_put_width_shift_N` ($99FE) that indexes pre-shifted entries from
`table_shifts` (built at boot at `0xF200`, see `notes/init.md`). So its
OR/XOR operands are pre-transformed while the OUTPUT matches the direct
bit-ops formula. The port shifts per row at runtime instead.

**The two agree on live data, not on tape data.** `gfx_inverse` XORs every
sprite's pix bytes with its mask at boot, so the original's blit operates on
`tape_pix XOR mask` — which is exactly why `(~m & d) | (m & p)` on TAPE
bytes is bit-identical to `(m | s) ^ pix` on LIVE bytes. Comparing the two
formulas on the same bytes produces a phantom mismatch;
`notes/bird-render-parity.md` has the full algebra and the two real bytes
where it does not hold.

## Compose order

`render_level_screen`:

```
paint_bg              -> bg pattern + bg_attr in attr_buff
paint_frame_to_buff   -> HUD strip + side strips (generated, not captured)
render_bat            -> bat at BAT_X, BAT_Y_PX
render_lives          -> lives indicator at y=185
render_hud_to_buff    -> 1UP / HI / 2UP labels and score digits
render_magnets        -> ON sprite always, OFF overlay per slot state
inner_border_line_c   -> 1 px inner border at byte 1 / byte 30
render_brick_band     -> base band + paint_bricks + print_border_shadow_c
render_brick_flash    -> destroyed-cell dirty marking (mid-game only)
buff_to_vga           -> scr/attr -> VGA
```

`paint_frame_to_buff` runs BEFORE the sprites so they blit cleanly over the
frame. The VGA expansion precomputes `attr & $7F` plus 4-bit pixel nibbles
into `vga_attr_nibble_dwords` and emits each Spectrum byte as **two aligned
32-bit stores** in mode 13h. FLASH is ignored.

The original's order is `paint_bg -> paint_frame -> inner_border -> ... ->
magnets -> bricks`. The port's differs and is equivalent for the captured
moment, because `paint_frame_to_buff` only overwrites the side-strip cells
and the HUD area. `inner_border_line_c` applies only the net visible lower
bands (y=50..77, 106..133, 162..189): the original clears the vertical inner
border line BEFORE drawing the top border and the border then restores
y=0..21, so clearing the top band after the port's combined top+side asset
punches 12 black holes in L1 at x=8 and x=247. `notes/per-level-profile.md`
has both residual classes this produced.

The gameplay static-background cache does one extra narrow top-frame restore
for cells cr 0..2, cc 8..10 immediately before dirty flushing, which matches
the original-captured final image for L3/L9 without repainting the whole HUD
after magnets.

## Dirty redraws

Gameplay caches the static level image in `bg_scr_buff` / `bg_attr_buff`.
Each moving object marks the exact pixel rows its sprite covers, and each
frame flushes the union of the previous and current dirty rows. **Sprite
heights come from the sprite headers, not from collision constants**, so
tall falling bonuses and the 28-row rocket animation do not leave stale
bottom rows.

The VGA page is NOT cleared before `buff_to_vga` during gameplay. The
original saves and restores object regions in its buffers, prints active
objects into them, then copies buffer to screen; clearing VGA between frames
is a port-only artefact and causes visible flicker, especially while a
falling bonus forces full-frame redraws.

Attribute-writing blits must expand their dirty rect to CELL boundaries in
Y, since they recolour whole 8x8 cells — `notes/performance.md` traces the
three stale-VGA defects that came from not doing so.

## Brick collision, in brief

`brick_collision` (the pre-LAFFC path, still the fallback):

- bit 7 set: pass through;
- bit 5 set: bounce, no destruction;
- bit 4 set: destroy (score + sound + set bit 7);
- otherwise a fresh multi-hit brick: SET bit 4 and bounce.

The default path is the literal `LAFFC` port — `notes/laffc-decode.md`.
Cell encoding and the type vocabulary are in `notes/levels.md`.

### Destroyed-cell attribute cleanup

For runtime-destroyed cells (`cell & 0xC0 == 0x80`), `render_brick_band`
resets the body attr cells and the row directly below to the level
background before calling `paint_bricks`. The shadow-row reset is
unconditional: if a live brick sits below, `paint_bricks` writes its body
attr afterwards; if the row below is empty, the stale dimmed shadow attr
disappears with the destroyed brick instead of tinting the background.

Note the base band is the EMPTY playfield's attributes, so the reset half of
that is a no-op — the shadow half is what earns its keep
(`notes/levels.md`).

## Assets

`assets/sprites.bin` must include the full `gfx_bonuses` tail through
`spr_bonus_triple_ball` (`$8CEA..$8D46`). A shorter extraction leaves the
triple-ball sprite truncated near the end of the blob, so its falling bonus
renders from partial data plus zero-filled memory.

The normal build draws the in-game score HUD (`spr_1up`, `spr_2up`,
`spr_hi`, `spr_score_digits`) into `scr_buff` before `buff_to_vga`. The
visual-test executable is compiled `-dBATTY_SCORELESS_HUD`, because the GT
capture pipeline NOPs the original's score block — which is also
known-bugs #22's coverage hole.

The original does NOT draw persistent HUD letters for active bonuses.
`get_bonus` updates object state, score, sound and bat/ball state, and turns
the caught bonus object into the floating-points sprite; any separate
current-bonus letter overlay would be port-only.

## Original source references

- `byte_put_width_N` $99EB — the unshifted masked-blit primitive. Opcode
  pattern `POP DE; LD A,E; OR (HL); XOR D; LD (HL),A; INC L`, verified
  against `original/disasm/tools/batty_for_compare.sna`.
- `byte_put_width_shift_N` $99FE — the shifted variant, via `table_shifts`.
- `print_obj_to_buff` $A35C — the top-level masked-sprite blitter,
  dispatching through a patched `JP put_byte_N` to the right width. Writes
  PIXELS only, never attributes.
- `game_screen_draw_to_buffer` $BE6B — the initial paint pipeline.
- `print_briks` $ADE1 — brick rendering.
- `print_magnets` $8D4C — magnets ($06 = ON, $07 = OFF).
- `print_border_shadow` $BFCF — clears bit 6 on cc 1 of cr 1..23 and cr 1 of
  cc 2..30. The port's `print_border_shadow_c`.
- `table_shifts` $F200 — the pre-shifted sprite-byte lookup table.

**A watchpoint gives you a PC, not a purpose.** `0xAD8F` was named "the
brick-field blitter" and cost days of theorising; it is
`all_metal_briks_frame`, an animation that happens to share an inner blit
with the level paint. Trace where a routine is CALLED FROM before naming it.
