# Brick-hit shimmer, and the frame-step floor

The cyan animation a brick plays when the ball hits it, plus the L3
frame-step residual it was long blamed for. Both are settled; the
shimmer is a literal port and the residual is a measurement artefact.

## The shimmer is ONE ~15-tick pass per hit

Registered in `LAFFC`'s undestructible / first-contact-multi-hit paths
(`LAFFC_34..37`): find a free `briks_data` slot (5 max, `+$00 == 0` means
free), store the screen and buffer addresses and the cell pointer, `INC`
the counter to 1, queue the metal-hit sound.

`fill_briks_data` ($B694) runs once per frame and calls
`metal_brik_anim` ($B6A9) per ACTIVE slot:

    LD A,(IY+$00) / AND A / CALL NZ,metal_brik_anim   ; counter 0 == free

    metal_brik_anim:
      if BIT 7 of the cell (destroyed) -> free the slot     ; early out only
      idx    = (counter + 1) & $FE                          ; 2 ticks per frame
      sprite = anim_brik[idx/2]                             ; {2,6,3,7,4,5,5,1}
      draw to screen AND buffer
      counter = (counter + 1) & $0F                         ; 1..15, wrap frees

The counter DOUBLES AS the free/active flag, so `(15 + 1) & $0F == 0`
ends the animation: eight sprites at two ticks each, then stop. Metal and
multi-hit bricks alike — nothing shimmers permanently. Each frame is
drawn before the increment, so the last frame persists on a surviving
brick (the "hit" look), and `anim_brik[7] == spr_brik_1` is the normal
brick, which is why expiry is seamless against the static band cache.

The original does NOT dedupe by brick: a re-hit mid-animation takes a
second slot.

`step_brick_hit_anim` ports the counter literally (`ticks = (ticks + 1) &
0x0F`) and renders `frame_idx = (tick - 1) >> 1`; `brick_hit_anim_spawn`
mirrors `LAFFC_35/36`, first free slot, no dedupe. `test-shimmer-one-pass`
holds it. Reading the wrap as "shimmers forever" once cost a
user-visible defect (known-bugs #3): a two-hit brick animated
continuously.

## The level-intro pass is a different routine

`all_metal_briks_animation_snd` ($B765) runs once in the round-intro
sequence, not per frame. It waits TWO 50 Hz interrupts per animation
frame (`EI/HALT/EI/HALT/DI`, draw AFTER the wait) and reads no input — 8
frames ≈ 16 edges ≈ 320 ms, uninterruptible.

`play_brik_anim` matches that: two full `do {} while (pit_ticks() == t)`
edge waits per frame, draw after. Non-ESC keys are PEEKED
(`_bios_keybrd(_KEYBRD_READY)`) and left in the BIOS buffer; only ESC is
consumed. Both halves were once inventions — a mid-tick `< 2` sample ran
~25% fast, and "abort on any buffered keypress" meant a player holding a
bat key at level entry skipped the animation entirely.

`test-brik-anim-pace` gates it. The floppy is built with
`BATTY_TEST_KEY_BEFORE_ANIM=1` so the port stuffs one ENTER into the BIOS
buffer just before the animation; the pass must report `brik_anim_ticks`
>= 16 AND the key must survive to release `BATTY_REPLAY_WAIT_KEY`. With
the abort bug the key dies at frame 0 and the probe reads ~0 ticks.

## The frame-step floor: 0,0,0,4,0,4,1

`make test-frame-step` (in `parity-check-full`) re-runs the L3 capture
over frames 0-6 and asserts those per-frame budgets in the brick ROI
exactly. Two things drove the residual down from ~188 px:

1. **The byte-correct L3 seed** (`BATTY_REPLAY_RANDOM=3793`
   `BATTY_REPLAY_RANDOM_SEED=962A`). The stale seed made the port drop a
   spurious SLOW bonus whose falling letters polluted the ROI — f1 went
   188 -> 0. Most of the old "shimmer residual" was this.
2. **The conditional left-char shadow** on destroyed cells (dim the left
   char iff the left neighbour is a live brick), reproducing the
   original's inter-brick gap shadow — f5 went 85 -> 50 -> 4.

**The last 4 px are a capture-phase offset, not a render bug.** The
differing pixels share an identical char attr (`$57`) on both sides, so
it is a bitmap difference; and the wedge is byte-for-byte
`PORT frame 3 ≡ ORIG frame 5`. The port's visual probe halts AFTER the
frame's update and redraw; ZEsarUX breaks at `$BA83`, the TOP of the main
loop. So port frame N samples roughly orig frame N+1 by construction.
Same class as cycle-exact sound: accepted (PLAN.md).

The shimmer is not even exercised at L3 — reading the original's
`briks_data` ($B6F4) at the `$BA83` start shows all five slots empty, so
both sides start the system fresh.

## `BRICK_FLASH_TICKS` must stay 2

A perf commit cut it 2 -> 1, reasoning that the destroyed cell's second
full-dynamic frame is redundant because `carry_dirty_with_previous`
re-flushes the rect next frame. That holds for the single-buffer ERASE —
so `test-brick-flash` still passed — but the carry does not reproduce a
full-dynamic band REBUILD of the destruction transient, and f5 regressed
to 88-134 px. Reverted.

**Any change to the brick-band / dirty-redraw / flash path must re-run
the frame-step gate**, which is why that gate exists: `test-brick-flash`
checks erase cleanup, not the destruction transient's render.
