# Sprite data

The rendering port is in `notes/blitter-port.md` and the enemy-specific
encoding story in `notes/bird-render-parity.md`. This file holds the few facts
about the original's sprite data that are not obvious from the C.

## Layout

Every sprite in the blob shares one header + body format, which the port reads
directly:

```
byte 0   width in bytes (rows are byte-aligned)
byte 1   height in rows
then h * w pairs of (mask_byte, pix_byte) per byte-column, row-major
```

`Sprite` in `src/zxvga.h` wraps it, so call sites read `sprite.height()`
rather than `src[1]`.

Two things about heights are worth knowing. Some are SELF-MODIFIED at
runtime — `spr_magnet_circle_ON` is drawn at height `$1E` at level paint and
`$17` on a toggle (`notes/magnets.md`) — so a captured blob and live memory
legitimately disagree. And `spr_bird_4`'s header claims one row more than the
layout allots, which the boot-time XOR pass then overruns into `spr_bird_5`;
the `sprites.bin` Makefile rule patches the two resulting bytes.

**Sprites are drawn bottom-up.** `print_sprite_pix` moves to the PREVIOUS
buffer line each row, so the first data row lands at y and the rest stack
above. Laying the border sprites out top-down gave 58 bytes of 256; reading
the routine gave 256 of 256.

## Asset addresses

The blob spans `0x7A8C..0x8F50` in the original (= `assets/sprites.bin`,
4724 B), so every `SPR_*` offset in the port is `absolute - 0x7A8C`. To find a
sprite's absolute address, search
`original/disasm/gfx/sprites_with_masks_2.asm` for the `spr_*:` label and read
the `Data block at NNNN` comment above it.

`assets/sprites.bin` must include the full `gfx_bonuses` tail through
`spr_bonus_triple_ball` (`$8CEA..$8D46`) — a shorter extraction truncates the
triple-ball sprite at the end of the blob.

**Read the sprite-ID table before writing per-slot draw logic.** The names in
`gfx_screen_elements` are ordered by ENTRY, not by sprite-data layout: sprite
`$06` is `spr_magnet_circle_ON`, drawn first and unconditionally. Getting that
order backwards cost 1383 → 660 px of residual to undo.

## The pre-shift table is a ZX-side concern only

The original builds a 7-shift pre-shifted byte cache at `0xF200..0xFFFF` at
boot, which is what lets its blitter render at arbitrary X cheaply. The port
shifts per row at runtime in C, so the table exists here only as a reference
for the blitter's shape. Full decode in `notes/init.md`.
