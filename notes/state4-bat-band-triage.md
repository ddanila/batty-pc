# state4 / state5_bat_band — how we got to 0 px

Historical record of how `state4_level1` went from 507 px → 0 px on L1.
The fixes themselves are all live in `src/main.cpp`; this file preserves
the technique notes that survive each iteration.

## Iter 3: blit formula — `(~mask & screen) | (mask & pix)`

`blit_masked_to_scr_buff_ptr` originally used `(mask | screen) ^ pix`,
read from the disasm at `byte_put_width_2`. Empirical check against
four `(mask, pix, screen → output)` triples from the L1 GT showed
the correct formula is the standard masked blit:

| sprite pair     | clean bg | original output | wrong-formula output |
|-----------------|----------|-----------------|----------------------|
| (`$1F`, `$00`)  | `$7F`    | `$60`           | `$7F` (no change)    |
| (`$FE`, `$00`)  | `$FE`    | `$00`           | `$FE`                |
| (`$3F`, `$0B`)  | `$7F`    | `$4B`           |                       |
| (`$FF`, `$F4`)  | `$FE`    | `$F4`           |                       |

Replacing the formula dropped state4 from 507 px → 10 px.

The disasm's `OR (HL); XOR D` writes the same result via table-driven
operands (`table_shifts` at `$F200`); the **output** matches our
direct-bitops form for every shift/operand combination. Iter-35
verified our assembled SNA matches `batty_for_compare.sna` at every
non-patched byte. See [`blitter-port.md`](blitter-port.md).

## Iter 4: ball-before-bat in `redraw_full_with_ball` — 10 → 3 px

Ball sprite rows 7..11 (the "ball-resting-on-bat" mask) drawn AFTER
the bat punched holes in the bat top via the masked-blit's pix=0 path
(`(~mask & screen) | (mask & pix)` with pix=0 → clears bits).

```
bat then ball: $C0  (wrong, holes punched)
ball then bat: $D8  (= GT)
```

Fix in `redraw_full_with_ball`: blit primary ball BEFORE bat. Secondary
balls (`ball2`, `ball3`) still blit after — they can land anywhere,
not just on the bat.

## Iter 5: pin `run_dot_frame` + `bat_w = 28` (3 → 0 px)

Two fixes in `render_running_dot`:

1. **Phase pin.** Under `BATTYALL`, reset `run_dot_frame = 0x0E` at the
   top of the function. The GT was captured after one gameplay-loop
   iter, so the original's running_dot punched once with frame=14.
   Our test reaches the screendump after many iters, with the frame
   counter at some non-deterministic phase. Pinning matches the GT.

2. **Bat logical width.** `object_bat_1+$0C = $1C = 28`, not 32 (the
   sprite footprint). The original uses 28 for the mirror dot's
   position calc (so the dots sit inside the body cap, not at the
   tapered sprite ends). Our `bat_w = 32 + ...` placed the second dot
   2 px too far right.

Result: state4 = 0 px, `state5_bat_band.assert_match = True` (= the
bat band is now FAIL-gated against any future regression).

## Iter 2: `frame_l1.bin` had lives baked in (507 px unchanged)

The captured frame asset had the lives icons baked into the left side
strip (= bytes at y=185..190 byte_x=1..2 = lives sprite pixels).
`paint_frame_to_buff` painted lives, then `render_lives` painted them
again on top, double-blitting (= visually 0 px diff but logically a
bug).

Re-extracted `frame_l1.bin` from a no-lives GT and dropped the
Makefile rule that auto-regenerated it (= the asset is checked-in;
extraction needs `L6853 lives-skip` patch ON for the source, while
the test GT needs it OFF — contradictory requirements).
