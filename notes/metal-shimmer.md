# Metal-brick shimmer — decode (the last L3 frame-step residual)

The one persistent residual on the byte-exact L3 frame-step gate is the
undestructible ("metal") brick shimmer. Earlier notes guessed a
"sliding-window / constant animation"; the disasm shows it is actually a
**ball-hit-triggered, then permanent, per-brick shimmer**. Cosmetic (metal
bricks are indestructible regardless), but it is a real per-frame visual
difference, so it's the measurable parity residual.

## Trigger: ball hits an undestructible brick (LAFFC_34..37)

Inside `LAFFC` (brick collision), the undestructible path (`BIT 5,(IY+$00)`
set) registers the hit brick in a free `briks_data` slot (5 slots max):

```
LAFFC_34: find a free briks_data slot (IX, B=5, +0 == 0 means free)
LAFFC_36: (IX+$01/$02) = screen_addr_calc(brick xy)
          (IX+$03/$04) = scr_buff_addr_calc(brick xy)
          INC (IX+$00)               ; frame counter starts at 1
          (IX+$05/$06) = IY          ; pointer to the brick cell
LAFFC_37: queue the metal-hit sound ($02,$09,$B0)
```

So a metal brick only starts shimmering once the ball has hit it, and
only the first 5 distinct hit metal bricks get a slot.

## Per-frame animation (fill_briks_data / metal_brik_anim)

`fill_briks_data` runs once per frame (called at each main-loop site:
disasm 6200/6296/6464) and walks the 5 slots; each active slot
(`+0 != 0`) calls `metal_brik_anim`:

```
metal_brik_anim:
  if BIT 7 of the brick cell (*+5/+6) set (destroyed) -> free slot (+0=0)
  idx = (counter + 1) & $FE            ; 2 ticks per frame
  sprite = anim_brik[idx/2]            ; DEFW table, 2 bytes/entry
  draw sprite (8 rows x 2 bytes) to screen (+1/+2) AND buffer (+3/+4)
  counter = (counter + 1) & $0F        ; cycles 0..15, never "finishes"
```

`anim_brik` = 8 frames cycling the brick sprites:
`spr_brik_2, _6, _3, _7, _4, _5, _5, _1`. Because the counter only wraps
`& $0F` and a metal cell's bit 7 is never set, **a hit metal brick
shimmers forever** (cycling those 8 brick appearances, 2 ticks each). That
permanent cycle is what the earlier notes saw as a "constant animation".

## Port status + implementation path

The port has the scaffolding stubbed: `brick_flash_*` slot vars and
`render_brick_flash_to_buff()` — currently a no-op (`(void)` casts). To
close the residual:

1. Replace the single `brick_flash` slot with **5 shimmer slots** (col,
   row, counter), registered when the ball bounces off an undestructible
   brick (the `BIT 5` / undestructible branch in `brick_hit_resolve` /
   the LAFFC undestructible path).
2. Each frame, for each active slot, draw `anim_brik[(counter+1)&$FE >>1]`
   — i.e. the brick sprite `{2,6,3,7,4,5,5,1}[frame]` — at the brick's
   cell, then `counter = (counter+1) & $0F`. The port already renders
   brick sprites, so this reuses the existing brick blit at the cell xy.
3. Never free the slot (metal cell never destroyed); 5 slots cap matches
   the original (first 5 hit metal bricks shimmer).

This is a self-contained render feature (no gameplay effect, no RNG, no
collision change), so it won't touch the byte-exact ball gate; it only
needs its own visual check against a ZEsarUX L3 capture. Scoped as the
next implementation step.
