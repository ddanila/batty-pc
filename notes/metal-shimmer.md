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

## Port status — FIXED (was: stopped after one pass)

The port already had the system (`brick_hit_anim`: 5 slots, the
`{2,6,3,7,4,5,5,1}` `anim_brik` order, per-frame render into `scr_buff`,
registered from the undestructible/multi-hit paths in `brick_hit_resolve`)
— so it was NOT a no-op (the no-op `render_brick_flash` is the separate
destruction marker). The bug was **timing**: `step_brick_hit_anim` freed
the slot once `tick > BRICK_HIT_ANIM_TICKS` (16), so the shimmer played a
single 8-frame pass and stopped. The original never stops: the counter
only wraps `(c+1) & $0F` and the slot is freed solely when the brick cell
gets bit 7 (destroyed).

Fixed `step_brick_hit_anim` to **cycle the counter forever** (wrap 1..16)
and free a slot only when its cell is destroyed (bit 7) or gone — so a hit
metal brick (never destructible) sparkles permanently, and a multi-hit
brick shimmers until its final hit, matching `metal_brik_anim`.

Self-contained render change (no gameplay/RNG/collision effect):
byte-exact L3 ball gate still byte-exact, `make test` still
pixel-identical, and **`make test-brick-flash` passes** (L3 brick
destruction cleanup, no stale-flash cells vs the original L3 reference) —
so the permanent shimmer introduces no stale-flash artifacts.

The earlier "720x400 harness issue" was a false alarm: it was a stale
replay-seeded `build/batty-test.img` left over from shimmer-validation
builds (its AUTOEXEC booted into a replay state, so the test's menu
navigation never reached L3 graphics and the screendump caught text mode).
`test_brick_flash.py` now force-rebuilds a clean floppy (env stripped of
`BATTY_*`) before capturing, so it's robust to that. Remaining (nice to
have): a frame-step pixel-diff of a *shimmering* metal brick vs ZEsarUX to
confirm the per-frame sprite sequence, not just the absence of artifacts.
