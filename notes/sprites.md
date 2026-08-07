# Sprite system

The rendering port is documented in
[`blitter-port.md`](blitter-port.md). This file holds the few
ZX-specific facts about the original's sprite data that aren't
obvious from our C code.

## Sprite layout

Every sprite in the original blob shares the same header + body
format, which we read directly:

```
byte 0   width in bytes (rows are byte-aligned)
byte 1   height in rows
then h * w pairs of (mask_byte, pix_byte) per byte-column, row-major.
```

The blit semantics in our C port: `screen' = (~mask & screen) | (mask & pix)`
— standard "where mask=1 take pix, else preserve screen". The original
Z80's `byte_put_width_N` ($99EB) uses the equivalent `(mask|screen)^pix`
arrangement, and the shifted variant (`byte_put_width_shift_N` at $99FE)
indexes pre-shifted mask/pix from `table_shifts` ($F200) so the OR/XOR
operands are pre-transformed; the **output** matches our direct-bitops
formula. See [`blitter-port.md`](blitter-port.md) for the full
discussion + verification against `batty_for_compare.sna`.

## Pre-shift table at `0xF200..0xFFFF` — load-time

The original builds a 7-shift pre-shifted sprite cache at boot
(routine at `0x6800`). For each (input_byte, shift_count) pair it
stores 8 output bytes (= the input shifted by N bits, both halves
of the carry). This is what lets the blitter render sprites at
arbitrary X positions cheaply on Z80. Our port shifts per-row at
runtime in C — the table exists only as a ZX-side blitter shape
reference.

## Asset addresses

Sprite blob spans `0x7A8C..0x8F50` in the original (= our
`assets/sprites.bin`, 4724 B). All `SPR_*` offsets in `src/main.cpp`
are computed as `(absolute - 0x7A8C)`. To find a sprite's absolute
address, search `original/disasm/gfx/sprites_with_masks_2.asm` for
the `spr_*:` label and read the `Data block at NNNN` comment above
it.
