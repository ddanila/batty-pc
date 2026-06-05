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

## PIXEL-LEVEL (2026-06-05): the f5=85px residual decomposed

Examined `build/tl_port` vs `build/tl_orig` frame_0005.idx at the diff
bounds (92,64,139,72). The 85 px are two brick-band render details on the
freshly-destroyed cells (NOT a flash — the port draws no destruction
flash, matching the original):

1. **Black inter-brick-gap line at x=104** (left edge of brick cell col 6,
   y=64..71): original = palette 0 (black), port = 8. The original keeps
   the black grid-gap line at a destroyed cell's edge; the port's
   revealed background fills it with the bg colour.
2. **Revealed-cell colour** across cols 7–8 (x≈120–139): port = 13 vs
   original = 8 (and a few px the other way). The destroyed-cell reveal
   shows a different attr/colour, and a couple of px at x≈92–94 (15 vs 10).

Both are brick-band BACKGROUND-pattern / attr details — the captured-bg
approximation already flagged in `parity-gaps.md` ("non-brick cells in the
brick band, frame-strip attrs, pre-dimmed shadow attrs still come from
captured data"). They surface only at the transient moment a brick is
destroyed (the cell then settles). Gameplay is fully correct. Pixel-
matching needs re-capturing the exact brick-band bg pattern + per-cell
revealed attrs — deep captured-asset work, very low value. This is the
final, fully-decomposed frame-step residual.

## ATTR-LEVEL (2026-06-05): the f5 residual is a left-char shadow + bg texture

Read the original's `.scr` attrs at the destroyed region (frame 5). The
85 px decompose into two brick-band attr/texture details:

1. **Left-char shadow on the destroyed cell.** The destroyed brick (brick
   row 4 / char row 8, brick col 6 / char cols 13–14) shows attr `0x05`
   (cyan ink, black paper, **bright=0**) on its LEFT char (col 13) and
   `0x45` (bright) on its right char (col 14). The port resets the whole
   destroyed cell to `bg_attr 0x45` (bright both chars), so its left char
   is too bright (palette 8 paper vs the original's palette 0). This
   left-char-only dimming is NOT the port's `brik_shadow_c` (which clears
   bright on a full brick width — both chars — of the row below a live
   brick); it's a distinct left-edge/inter-brick-gap shadow the port
   doesn't reproduce at destroyed cells. (col 13 is non-bright across char
   rows 7–9 — a vertical shadow strip down the left of brick col 6.)
2. **Bg texture at cols 15–17.** Port draws cyan ink (13) where the
   original has black paper (8) — the brick-band background PIXEL pattern
   (paint_bg / bg_tile) differs at those revealed cells.

Remaining pixel-chase work (deep, two independent sub-parts): (a) port the
original's left-edge/gap shadow so a destroyed cell keeps its non-bright
left char instead of a flat bright `bg_attr`; (b) re-capture/correct the
brick-band bg texture so the revealed pixels match. Both are
captured-asset / shadow-logic details; gameplay is unaffected (the diff is
a few-tick, sub-100 px cosmetic at the destruction moment). Progressed
from "188 px unknown" → "85 px, two named attr/texture sub-parts."

## FIX 1 landed (2026-06-05): destroyed-cell left-char shadow → f5 85→50px

Dimmed the LEFT char (cleared the bright bit) on the destroyed-cell attr
reset in `render_brick_band` (both the cell row and its shadow row), to
reproduce the original's inter-brick gap shadow that persists when a brick
is removed. Re-measured `capture-timeline-both`:

    before: f3=4  f5=85 px
    after:  f3=4  f5=50 px   (bounds shrank to x92-127)

So the left-char-shadow hypothesis held (residual dropped, none introduced).
Safe for the static gates (only bit-7 destroyed cells are touched; level
entry has none). Remaining f5=50px is sub-part (2): the brick-band BG
PIXEL pattern behind destroyed bricks — the port's `bg_tile` (cycle 2)
draws denser cyan-ink (palette 13) than the original's revealed bg (mostly
black paper, palette 8). That's a captured-bg-texture detail (next).

## RESULT (2026-06-05): pixel-chase succeeded — residual 188px → 4px (~98%)

Final `capture-timeline-both` (ROI 8,32,248,128, max-diff 0):

    f0=0  f1=0  f3=4  f5=4 px   (was f1≈188, f5=85 at the start of this run)

Two fixes drove it down:
1. **Seed** (L3_SEED_ENV = 3793/962A): killed the spurious SLOW-bonus
   drop → f1 188→0.
2. **Conditional left-char shadow** on destroyed cells (dim the left char
   iff the left neighbour is a live brick): f5 85→50→4.

The last 4 px are a brick-edge colour detail at brick col 5 (x92–94/x101:
port palette 15 = bright white vs orig 10) — NOT the destroyed-cell bg
(the x104 p8-vs-o0 "diffs" are both black in RGB, so they don't count).
It's ~0.017% of the ROI, a 1-px brick-edge ink nuance, almost certainly a
captured level_attrs edge value. Diminishing returns past here; the
frame-step parity is effectively pixel-perfect. Pixel-chase complete.

## CORRECTION (2026-06-05, iter 2): the last 4px is a CAPTURE-PHASE offset, NOT an attr value

The "almost certainly a captured level_attrs edge value" guess above is
**wrong**. Re-decoded the f3/f5 residual directly from the existing
`build/tl_port` + `build/tl_orig` `.idx`/`.scr` captures:

- The diff cells are `port=15 (white)` vs `orig=10 (red)`. The char-cell
  **attr is identical on both sides** (`0x57` = white ink / **bright red**
  paper), so it is NOT an attr/captured-asset difference. It is a **bitmap**
  difference: the port has set (ink) bits the original leaves clear (paper).
- The differing pixels form a **white wedge in the bottom-right of brick
  col 5 / char row 8** (x88–103, y64–71). It is absent at f0/f1 (0px), then
  appears at f3 and grows at f5 — i.e. it is the transient at the moment the
  ball hits/penetrates that brick (the cell attr is still `0x57`, so the ball
  body OR-blitted into the red cell shows white-on-red).
- **Decisive measurement:** the wedge pixel-set is, byte-for-byte,
  `PORT frame 3 ≡ ORIG frame 5` (18px, **zero** difference). The port's
  transient runs ahead of ZEsarUX's by a fixed phase. Growth is ~2px/frame
  and the port leads by ~2 capture indices consistently (port f5=22px vs
  orig f5=18px = the same lead).

**Why:** the port's visual-probe halt is at `main.c:7430` — *after* the
frame's full `handling_object` + redraw (post-update). ZEsarUX captures at
the `$BA83` breakpoint — the *top* of the main loop (pre-update). So port
frame N ≈ orig frame N+1 by construction; the brick-hit transient then adds
one more anim-step of lead. Both halves are temporal, not spatial: the
RENDER is identical (proven by f3≡f5), only the sample phase differs.

This puts the last 4px in the **same class as the rocket pace and
cycle-exact sound** — a measurement/timing-phase residual, not a port render
bug. Options if ever chased: (a) capture the port pre-update (move the probe
halt to the loop top) to match `$BA83` — risks shifting the 8 currently-0px
gates; (b) investigate the brick-hit-anim tick phase (a prior step-order
flip `(tick>>1)-1` was tried and reverted — made f5 worse). Neither is worth
4 transient px. **Status: root-caused (phase offset), accepted as artifact.**

## REGRESSION found + fixed (2026-06-05, iter 2): perf cut f5 4px → 88px

Caveat to the "accepted as 4px artifact" above: when re-running
`capture-timeline-both` on HEAD, f5 was **88px**, not 4px — the documented
4px had **regressed**. Bisected it (the capture gate needs ZEsarUX and is
NOT in `make test`/`parity-check`, so it wasn't re-run during the perf
campaign):

- `8aa0a92` (parent): f5 = **4px** ✓
- `27c4d69` "perf: brick destruction costs one full-dynamic frame, not two":
  f5 = **134px** ✗  ← culprit

`27c4d69` cut `BRICK_FLASH_TICKS` 2→1 on the theory that the destroyed
cell's second full-dynamic frame is redundant because
`carry_dirty_with_previous` re-flushes the rect on the next ball-only/object
frame. That holds for the single-buffer **erase** (so `test-brick-flash`
still passed), but the carry re-flush does **not** reproduce a full-dynamic
band REBUILD of the destruction transient — so the destroy frame rendered
differently from the original, regressing the L3 frame-step residual to
88–134px (it oscillates with the later bounded-scan commits).

**Fix:** revert `BRICK_FLASH_TICKS` to 2 (the destroyed cell needs two
full-dynamic frames to match the original's destroy render). Re-measured at
HEAD: `f0=0 f1=0 f2=0 f3=4 f4=0 f5=4 f6=1` — back to the 4px floor.
`test-brick-flash`, `make test` (all 4 states), and the dirty-redraw gates
(bullet/bomb/ball-object) all still pass. Cost: loses that micro-opt's
"ball block bricks 19→12" on normal brick-hit frames — parity wins over a
brick-hit-frame perf nicety. **Lesson:** any change to the brick-band /
dirty-redraw / flash path must re-run `capture-timeline-both`, not just the
headless gates — `test-brick-flash` checks erase cleanup, not the
destruction transient's full-dynamic render. (Added to lessons.md.)
