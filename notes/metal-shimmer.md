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

## L3 frame-step measurement (the residual is shimmer render phase)

`make capture-timeline-both` (port QEMU vs ZEsarUX, brick ROI, max-diff 0):

```
frame 0: 0/23040 px   [PASS]   (aligned start, perfect)
frame 1: 188/23040 px [FAIL]   bounds ~(104,72,127,86)
frame 3: 188/23040 px [FAIL]
frame 5: 184/23040 px [FAIL]
```

Pixel-level, the residual is dominated by a **white(15) <-> cyan(11) swap**
(~93 px) in the freshly-hit brick cells (cols 5-7, rows 4-7) — i.e. at a
given frame the port draws a DIFFERENT `anim_brik` frame than the original
(the frames are cyan/white brick variants, so a 1-frame offset reads as a
white/cyan swap). The frame->sprite MAPPING is verified correct: port
`frame_idx = (tick-1)>>1` matches original `((counter+1)&$FE)/2-1`, both 2
ticks/frame over the same `{2,6,3,7,4,5,5,1}` order. So this is NOT a wrong
mapping; it's a one-frame phase/order offset (when the slot's counter is
sampled vs advanced within the frame, relative to the original's
`LAFFC`-register-then-`fill_briks_data`-render order) and/or a brick
sprite-data difference. Frame 0 being 0 px shows the aligned start and the rest of the brick band
are exact.

### Per-cell pixel diff (done): NOT a pure phase offset

Compared one differing cell (6,5) across the captured frames:

```
f0: port == orig   {11:107 cyan, 15:21 white}  (static brick, matches)
f1: port {5:47, 8:40, 13:41}   orig {0:14, 5:50, 8:39, 13:25}
```

At f1 the cell flips to a damaged-brick pattern on BOTH sides, but the
port's pattern matches NO captured original frame (so it's not just a
1-frame phase slip), and the original has **14 black(0) pixels the port
lacks** (plus different magenta/cyan counts). So this cell is a
*destructible* brick mid-hit whose damaged-state RENDER differs in pixel
detail — the port's damaged/half-state (and/or the revealed background
tile) is missing the cracks/black the original shows.

### Sprite data verified identical; step-order phase fix tried + reverted

- The port's `brik_anim_sprites[1..7]` are **byte-identical** to the
  original `spr_brik_1..7` (`original/disasm/gfx/briks.asm`). So the
  residual is NOT a sprite-data difference.
- Hypothesis: the port advances (`step_brick_hit_anim`) BEFORE drawing,
  while the original draws-then-advances, so the shimmer runs one frame
  ahead. Tried `frame_idx = (tick>>1)-1` instead of `(tick-1)>>1`.
  Re-measured: frame 1 unchanged (188), frame 3 188->184, **frame 5
  184->261 (worse)**. Net worse, so REVERTED. The residual is therefore
  NOT a simple step-order off-by-one.
- Most likely remaining cause: the shimmer COUNTER PHASE is unsynced
  between port and original because the L3 seed does not capture the
  `briks_data` / `brick_hit_anim` state — the bricks were hit at
  different relative times before/within the window, so each side's
  shimmer counter sits at a different point in the 8-frame cycle. Syncing
  that would require seeding the shimmer slots + counters, which the seed
  doesn't do. So this residual is intrinsic to the seed-based comparison,
  not a port render bug.

Conclusion: the ~188 px L3 residual is a MIX — metal-shimmer phase on
undestructible cells, plus damaged-brick / background render detail on
freshly-hit destructible cells. It is small, cosmetic (no gameplay
effect), and spread across several per-cell render details rather than one
fixable bug; matching it pixel-for-pixel is deep cosmetic work with
diminishing returns. The gameplay-relevant parity (ball/collision/bat,
frame 0 = 0 px, the whole brick band exact except the transient hit
render) is achieved.

The earlier "720x400 harness issue" was a false alarm: it was a stale
replay-seeded `build/batty-test.img` left over from shimmer-validation
builds (its AUTOEXEC booted into a replay state, so the test's menu
navigation never reached L3 graphics and the screendump caught text mode).
`test_brick_flash.py` now force-rebuilds a clean floppy (env stripped of
`BATTY_*`) before capturing, so it's robust to that. Remaining (nice to
have): a frame-step pixel-diff of a *shimmering* metal brick vs ZEsarUX to
confirm the per-frame sprite sequence, not just the absence of artifacts.

## CORRECTION (2026-06-05): the L3 residual is NOT a shimmer phase artifact

Read the original's `briks_data` ($B6F4, 5×7-byte slots) at the L3 `$BA83`
start via ZEsarUX: **all 5 slots are EMPTY** (00…). So there is NO active
metal-shimmer at the L3 capture point — both the original and the port
start the shimmer system fresh (empty slots). The earlier "the residual is
a seed-state shimmer-phase artifact (the snapshot doesn't capture the
mid-animation `briks_data`)" hypothesis is therefore **wrong** — there is
no mid-animation state to mismatch.

What the `capture-timeline-both` ~188 px residual actually is: the L3
`l3-brick-flash` scenario destroys a **destructible** brick, so the
residual on the freshly-hit cells is the transient **brick-destruction
render** — the destruction flash + the revealed-background detail during
the few ticks after the hit (frame 0 = 0 px aligned; ~188 px at f1/3/5;
converges afterward). It is NOT the metal-shimmer animation.

Also confirmed this pass: the original `print_line_briks` renders every
non-destroyed brick with a single sprite gated only on bit 7 (destroyed) —
it does NOT pick a "cracked" sprite for bit 4 (half-damaged). So the port's
single-`spr_brik_1`-per-brick render matches; there is **no damaged-brick
crack-render gap** either.

Net: the metal-shimmer LOGIC is correct (and not even exercised at L3), and
the residual is a transient destruction-flash/bg-reveal render detail — a
brief diagnostic-only difference on destroyed cells (gameplay is correct:
the brick is destroyed, points awarded, bg revealed). Pixel-matching the
exact flash is low-value cosmetic, not a shimmer or shipped-game bug.

## MEASURED (2026-06-05): residual down to f5=85px after the seed fix

Ran `make capture-timeline-both` (port-QEMU vs ZEsarUX, ROI 8,32,248,128,
max-diff 0) with the corrected L3 seed now wired into `L3_SEED_ENV`:

    frame 0:  0/23040 px  [PASS]
    frame 1:  0/23040 px  [PASS]   <- was ~188 px
    frame 3:  4/23040 px  bounds=(95,67,101,70)
    frame 5: 85/23040 px  bounds=(92,64,139,72)

The **f1 residual dropped from ~188 px to 0** — the byte-correct seed
(`BATTY_REPLAY_RANDOM=3793 BATTY_REPLAY_RANDOM_SEED=962A`) stops the port
dropping the spurious SLOW bonus (its falling-letter pixels were polluting
the brick-band ROI). So much of the old "~188 px shimmer residual" was
actually the seed-mismatch spurious bonus, not the shimmer or the
destruction render.

What remains (f3=4, f5=85 px) is localized to the freshly-hit brick row
(x≈92–139, y≈64–72) — the transient brick-destruction render (revealed
background / the few-tick destroy edge) on cells the ball breaks between
f3 and f5. Gameplay is correct (bricks destroyed + scored); the diff is a
sub-100-px, few-tick cosmetic detail on the destroyed cells, confined to
the destruction moment. This is the last frame-step residual — a captured-
bg / destroy-edge render detail, low value to pixel-chase.
