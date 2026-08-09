# Known bugs

## Status

| # | what | state |
|---|------|-------|
| #3 | multi-hit bricks shimmer continuously | fixed 2026-06-11; **now gated** by `test-shimmer-one-pass` — until 2026-08-09 nothing guarded it and all 59 QEMU gates passed with it broken |
| #8 | multiball direction convention | no gameplay effect — the mirrored values were written to fields nothing read |
| #9 | blast dirty rect 16x8 vs 16x12 | fixed; `test-blast-dirty-redraw` |
| #10 | bullet animation phase depends on the redraw path | fixed; host tests |
| #11 | narrow bat redraw loses the inner border line | fixed; `test-bat-redraw-window` |
| #12 | stuck ball snapped to the default offset | fixed; `test-stuck-ball-offset` |
| #13 | extra balls write the primary's sign cache | fixed; `test-ball-sign-cache-owner` |
| #14 | the sign cache mixed signs and speeds | **fixed** — the open half was the multiball spawn reading a reconstructed dir instead of the primary's dir byte; settled from the disassembly |
| #15 | `bios_ticks()` frozen during gameplay | fixed; `test-frozen-clock` |
| #16 | the port bounces and re-aims an alien at a wall; the original only clamps | **open by design decision**, not by unknown — measured against the original and the disassembly, see below |

**#14 is the only open item**, and it is open for a reason that cannot be
closed from the port: `delta_to_dir` selects its angle by MAGNITUDE, and
now that the cache provably holds signs, the `0x08` angle is unreachable
from its only production caller. Whether the ORIGINAL derives the extra
balls' launch angle from real velocity needs hardware. See the end of
that section.

This file originally read "user-reported, unfixed" and "(none
currently)". Both stopped being true: #8-#15 were surfaced by the
refactor rather than reported, and #14 is open. The header said
otherwise for long enough that someone scanning it would have concluded
there was nothing outstanding.

When fixing one, add a section to `per-level-profile.md` or the relevant
area doc, and update the row above rather than only the section below.

(The bird/UFO render-parity program is fully closed as of 2026-06-12:
the LAAD2 anim stepper is ported, both compose paths follow the $9AD0
slot-paint order, and the sprite-encoding decode (gfx_inverse + the
bird_4-overrun corruption of spr_bird_5) is reproduced at asset level;
the f24 "rings"/38px were HUD score+blink vs the scoreless test HUD,
not a render gap. Full story in notes/bird-render-parity.md.)

(Resolved from the same triage (all fixed 2026-06-12, gate
`make test-enemy-flyover-redraw` + the dirty-redraw A/B family green):
the "~21 px in-flight delta" was the simple and full compose paths
drawing the bomb and enemy in OPPOSITE order (visible while a fresh
bomb still overlaps its parent UFO; both paths now follow the
original's $9AD0 slot-paint order — balls < bullets <
bomb/bonus/pts400 < enemy < rocket); the frame-12 top-band 247 px diff
was restore_top_frame_center running AFTER the object compose; the
frame-100 713 px diff was the multi-checkpoint counter-phase
measurement artifact (lessons.md).)

---

Resolved history:

(bug #6 "ball teleported through red bricks" resolved 2026-06-17: NOT
tunneling by speed — max ball step is ~6 px/frame vs an 8x16 cell, so the
bbox always overlaps. It was an INVERTED open-face edge test in the port's
`laffc_collision` phase-4 mask. The original (LAFFC) starts D=$0F (all
faces open) and CLEARS a face only when its neighbour is solid AND the cell
is NOT against that playfield boundary — so a brick against a boundary
keeps that boundary face OPEN. The port had `Lx!=$08 && EMPTY`,
`Hy>=$21 && EMPTY`, etc. — the negation, leaving a boundary brick's edge
face CLOSED. A ball straight down (dir $10) into a row-0 (Hy=$20) METAL
brick whose right neighbour was empty then took a no-op horizontal "bounce"
(dir ($10^$1F)+1 = $10, unchanged) and fell straight through; metal can't
be destroyed so it showed as a clean pass-through (destructible bricks
masked it by breaking regardless of the bounce axis). Fix: the four mask
conditions now match the disasm (`Lx==$08 || EMPTY`, `Hy<$21 || EMPTY`,
`Hy>=$78 || EMPTY`, `Lx==$E8 || EMPTY`). INTERIOR cells are unchanged
(edge term false), so L3 byte-exact ball parity holds (test-laffc-ball-frame1
green at f1/5/40/100/150). Found + gated by the new collision-invariant
sweep `make test-ball-no-tunnel` (seeds a ball one step from a solid brick
across levels x approaches x speeds; FAILs if it crosses a still-solid
brick unhit). Full root cause in notes/laffc-decode.md.)

(bug #7 "bird's background looks black / wrong vs the original" resolved
2026-06-17: the premise was misdiagnosed. The port force-recoloured the
flying enemy's whole bounding box to `bg_attr`
(`blit_sprite_attrs_to_buff(enemy..., bg_attr)`). The original does NOT:
moving objects are drawn by `print_obj_to_buff` ($B82C), PIXELS only —
`print_sprite_attrib` ($B656) is called only 4 times, all in the static
`game_screen_draw_to_buffer`. So the bird keeps each cell's underlying attr
(bg over texture, the BRICK's attr over bricks) = ZX colour-clash. Verified
vs the ZEsarUX oracle: the bird's cell attrs are byte-identical to the
static L3 GT across the fly-over. Fix: dropped both enemy recolour calls +
the now-dead `blit_sprite_attrs_to_buff` helper; the enemy blits pixels
only like every other moving object. This also dissolves the bug-#2
stale-clash-attr class. Gate: `make test-enemy-attr-parity`. Full story in
notes/bird-render-parity.md.)


(the "L9 state4 drift 186 px" entry that used to sit here was resolved
2026-06-11: not render drift at all — the state4 screendump races the
LIVE alien on the only two levels (L3/L9) whose starting brick count is
under the $2C spawn gate (L5 is enemy-exempt). Fixed by pinning natural
alien spawns off under BATTYALL (enemy_prepare early-return; tests seed
via BATTY_REPLAY_ENEMY_OBJECT) and FAIL-gating all 15 levels via
`make test-levels-sweep`. Full triage trail in
notes/per-level-profile.md "RESOLVED (2026-06-11)".)

(bug #4 "initial hard-block shimmer very fast" resolved 2026-06-11: the
port's `play_brik_anim` aborted on ANY buffered keypress (a port
invention — keys held / typematic-repeating at level entry skipped the
animation almost entirely) and waited 1..2 ticks/frame from a mid-tick
sample instead of the original's two full `EI/HALT` interrupt edges
(`all_metal_briks_animation_snd` $B765). Now: two full edge waits per
frame, draw after the wait, non-ESC keys peeked and left buffered for
the main loop. Gate: `make test-brik-anim-pace` (stuffs a key into the
BIOS buffer before the anim; asserts brik_anim_ticks ∈ [16,40] AND that
the key survives to release WAIT_KEY). Full story in
`notes/metal-shimmer.md` "FIXED 2026-06-11: the LEVEL-INTRO shimmer
pace".)

(bug #5 "magnets don't affect the ball" resolved 2026-06-11: the entire
runtime magnet system was missing — the random ON/OFF toggle (LB9E8_2
`random_number+1 == $99` → print_one_magnet $8E72) and the ball
capture/curve/release physics at the top of handling_ball
(LA27E_0..11: ±1/64 dir rotation per frame inside an ON magnet's 15×14
box, quantized multiple-of-4 release dir, 2-frame re-capture cooldown).
Also fixed en route: the initial coin-flip was inverted, shared one
RNG sample across all magnets, and was misclassified read-current
(rng-model.md corrected — the original advances per magnet); moving the
coin into `magnet_level_init` also stops mid-game static-background
rebuilds from re-rolling the magnet look. Gate: `make test-magnet-ball`
(ON curves + releases quantized, OFF inert). Full decode + port design
in `notes/magnets.md`.)

(bugs #1 "background leftovers after a brick is destroyed" and #2
"inverted-colour leftovers after aliens fly over bricks" resolved
2026-06-11 — neither original hypothesis was right; both were
stale-VGA/flush-granularity defects, reproduced with the new
`make test-enemy-brick-residue` gate (dirty path vs a
FORCE_FULL_FLUSH_EACH_FRAME baseline; the existing dirty-vs-full-redraw
gates can't see these because both sides flush identically). Three
mechanisms, full story in `notes/performance.md` "Stale-VGA fixes":
  1. The enemy's colour-clash attr blit recolours whole 8x8 cells but
     only the sprite's own pixel rows were marked dirty — boundary-cell
     rows flushed during the fly-over kept the clash colour after the
     attrs were restored. Fixed: `mark_dirty_cell_rect_px`.
  2. The incremental band-cache rebuild rewrote whole rows + 32-byte
     attr rows but relied on the 18x10 brick-flash rect for flushing —
     everything outside went stale. Fixed: the rebuild marks its window.
  3. The rebuild window itself left boundary rows non-canonical
     (level_attrs base-copy resurrecting live attrs on destroyed
     boundary-row cells; interlocking brick top/bottom edge rows; the
     R1-row shadow dimming row R1+1's live cells; the erased inner
     border line) — polluting the band CACHE, which restores then spread
     to the screen. Fixed: window widened one brick row each side +
     guarded boundary resets + 4 edge fix-ups in
     `render_brick_band_rows`.
Gates: parity-check-full incl. test-frame-step at the documented floor,
plus the new test-enemy-brick-residue.)

(bug #3 "multi-hit bricks shimmer continuously" resolved 2026-06-11:
`step_brick_hit_anim` now mirrors the original's `(c+1) & $0F` literally —
the wrap to 0 frees the slot, one ~15-tick pass; the spawn dedupe
invention was dropped (LAFFC_35 takes the first free slot). Full decode +
fix in `notes/metal-shimmer.md`, "FIXED 2026-06-11". Gates: parity-check,
test-brick-flash, test-frame-step all green at the documented floor.)

(the `replay-l3-entry` "0 → 1885 px" entry was resolved
2026-06-11: the SCREEN had already healed back to 0/23040 px by the time
it was re-triaged, and the residual FAIL was the gate comparing
moment-dependent probe rows across different instants — the original
probed at the `$BA83` pause vs the port's PROBE.TXT rewritten by its ESC
handler at harness teardown. The "original=8E49" RNG reading was an echo
of the setup pin at the WRONG address `$8E17` (real `random_number` is
`$8D48` = 3793). Gate semantics fixed + green; full story in
`notes/replay-harness.md`, "Gate semantics corrected".)

## 8. Extra balls use a mirrored direction convention

**Status:** open, unverified against the original.

`dir_to_dxdy` (orig `hl_bc_calc_direction`, the byte-exact port that
drives the primary ball) and `dir_to_delta` (whole-pixel deltas, used
ONLY by the multiball extra balls in `step_extra_ball`) disagree on two
of the four quadrants:

| dir & 0x30 | `dir_to_dxdy` | `dir_to_delta` |
|-----------:|:--------------|:---------------|
| 0x00       | (+dx, +dy)    | (+dx, +dy)     |
| 0x10       | (-dx, +dy)    | (+dx, -dy)     |
| 0x20       | (-dx, -dy)    | (-dx, -dy)     |
| 0x30       | (+dx, -dy)    | (-dx, +dy)     |

So for half of all directions a secondary ball travels mirrored relative
to what the same `dir` byte means for the primary. Found while extracting
`src/physics.cpp`; `tests/test_physics.cpp`
`delta_vs_dxdy_conventions` pins the current behaviour so it stays
visible.

**It has no gameplay effect, established 2026-08-08.** `dir_to_delta`'s
only production consumer was `spawn_extra_ball`, which wrote its results
into `ball.extra2_dx/dy` and `extra3_dx/dy` — four fields that nothing
ever read. `step_extra_ball` moves the extras with `dir_to_dxdy(o->dir,
o->speed, ...)`, the same call the primary uses, from the object table.
The mirrored convention was computed and discarded.

The dead fields and the discarded call are now gone, which leaves
`dir_to_delta` with no production consumer at all — only
`tests/test_physics.cpp` exercises it, as a characterisation of a
faithful port.

So there is nothing to fix in the game, and no oracle capture is needed.
What remains is a naming trap: two functions that decode the same `dir`
byte disagree in two of four quadrants, and only one of them is wired
up. `delta_vs_dxdy_conventions_differ` keeps that visible.

That also explains why no gate ever caught it —
`test-ball-paths-no-tunnel` holds under either convention because
neither is in play.


## 9. The two redraw paths marked different rects for a bullet blast — resolved

`redraw_full_with_ball` marked each live blast slot as `16 x 8`; the
dirty path marked the same slot as `16 x 12`. Everything else about the
two blocks was identical.

**8 is correct.** The blast sprites are 8 px wide and at most 8 rows
tall — `sprites_blob` headers give frame heights 6/7/8/8, read straight
out of `assets/sprites.bin` at the `SPR_BULLET_BLAST_*` offsets.
`mark_dirty_rect_px` rounds X out to byte boundaries but takes Y
exactly (only `mark_dirty_cell_rect_px` rounds Y to cells), and a blast
writes no attrs — so the sprite's own 8 rows are the whole of what needs
flushing. The dirty path's 12 was over-marking by 4 rows.

Both paths now go through `render_bullet_blasts_to_buff_and_mark`, so
they cannot drift apart again.

`test-blast-dirty-redraw` is the gate that was missing. It compares a
blast frame across the dirty/full boundary the way the other
`test-*-dirty-redraw` gates do for balls, bombs and bonuses. Sensitivity
checked by mutation: marking `16 x 2` fails it with an 18 px diff.

By construction it catches under-marking (stale pixels) and not
over-marking (wasted flush bandwidth) — the two screens agree either
way when the rect is too large.


## 10. The bullet animation phase depended on which redraw path ran — fixed

`render_bullet_to_buff` increments a function-static `bullet_anim_tick`
on every call and picks `SPR_BULLET_1` / `SPR_BULLET_2` from its low
bit. The full redraw path calls it unconditionally; the dirty path
calls it only inside `if (any_bullet_active())`.

So on a frame with no bullets live, the full path advances the phase and
the dirty path does not. Which sprite the *next* bullet shows first
therefore depends on how many bulletless frames took the full path — a
render function carrying hidden per-frame state that only one of its two
callers ticks.

Not unified, because both directions change behaviour and neither is
anchored:

- Guarding the full path too makes the phase depend only on frames with
  bullets, which is probably what was intended.
- Ticking unconditionally on both paths makes it a free-running 25 Hz
  flicker independent of bullets.

The original's bullet flicker would settle it. Until then the safe part
was done: the two paths' *marking* loops are now one
`mark_live_bullets_dirty`, which is where the copy-paste risk was. The
guard difference is left visible rather than quietly resolved.

**What the two frames actually are** (2026-08-09), so an oracle capture
knows what to look for. Both records are 18 bytes at `$7DD2` and
`$7DE4`, 8 px wide by 8 rows:

    BULLET_1: 01 08 | 60 00 F0 40 F0 40 F0 40 | 60 00 F0 A0 F0 A0 F0 A0
    BULLET_2: 01 08 | 60 00 F0 40 F0 40 F0 40 | 60 00 F0 50 F0 50 F0 50
                     ^ data (identical)        ^ mask (differs)

The PIXELS are identical. Only the mask differs, and only in the low
nibble of three rows — `A0` = 1010, `50` = 0101. So the animation is a
one-pixel transparency shimmer: alternating which background pixels show
through the bullet, not a change of shape.

That matters for the capture. Do not look for the bullet moving or
changing outline; look at which background pixels show through it on
consecutive frames, and whether that alternation continues across frames
with no bullet in flight. A still frame cannot answer it — the phase is
only visible as a difference between two consecutive frames.

**The snapshot answers it without a capture** (2026-08-09). Searching
`build/snapshots/20260513T202101Z.sna` for references to the two sprite
addresses finds each exactly once, at `$77E6` and `$77E8` — consecutive,
i.e. a table. That table is itself entry 4 of the animation-table
pointer array at `$7780`, the same array whose other entries are the
bird and UFO frame lists:

    $7780: $7796    $7786: $77D0    $778C: $77F8
    $7782: $77BE    $7788: $77E6  <- bullet
    $7784: $782E    $778A: $77F2

And the list at `$77E6` is not two entries but three:

    $77E6: $7DD2   bullet frame 0
    $77E8: $7DE4   bullet frame 1
    $77EA: $7DF6   SPR_BULLET_BLAST_1

So the bullet's shimmer and its impact blast are ONE animation sequence,
indexed the way every other animated object is indexed: by that object's
own `sprite_num`. Not by a global per-frame counter.

That reframes the bug. Both repairs considered above are wrong — the
question "should the tick advance on bulletless frames?" only arises
because the port made the frame a shared static. The port models bullets
as parallel arrays (`bullet_x[]`, `bullet_y[]`, `bullet_active[]`) with
no per-bullet animation index, which is why it needed one.

**Fixed 2026-08-09.** `bullet_frame[N_BULLETS]` in `src/weapons.cpp`
gives each bullet its own index, advanced once per step inside
`bullet_advance` — the stepper, not the renderer — so the phase cannot
depend on how a frame was drawn. `bullets_clear` resets it, and firing
starts a bullet at frame 0.

Covered by two host tests in `tests/test_weapons.cpp`, not a QEMU gate:
the indices are independent (stepping one bullet does not move the
other's), and an inactive slot does not drift. Mutation-checked both
ways — advancing all slots on any step, and advancing before the
active check — each fails.

Writing the second test found a second thing: `bullets_clear` left the
old animation index behind, so a new bullet in a reused slot could start
mid-shimmer. It clears it now.

STILL DIVERGENT from the original, deliberately: `$77E6` runs the two
bullet frames and the blast frames as ONE sequence, so in the original a
bullet animates continuously into its impact. The port keeps blasts in
separate state (`bullet_blast_ticks`) with their own frame maths. That
is a bigger change and no evidence yet says the seam is visible.

Inferred rather than measured: that the indexing uses `sprite_num`. The
table's position and contents are read directly from the snapshot; the
indexing mechanism is the one the port already documents for
bird/UFO (`spr_bird_frames[sprite_num & 7]`,
`spr_ufo_frames[sprite_num % 10]`).

No gate covers it. `test-bullet-fly`, `test-laser-cadence` and
`test-bullet-dirty-redraw` all fire bullets promptly, so no run
accumulates the bulletless full-redraw frames that would separate the
two phases.


## 11. The narrow bat redraw lost the inner border line — fixed

Found by making `test-bat-redraw-window` deterministic (notes/testing.md).
Once both boots land the bat at the same X, the remaining difference is
reproducible: **6 px at the left clamp (x=8), 2 px at the right clamp
(x=247)**, 5/5 runs.

**One cause, both clamps.** `inner_border_line_c` blacks out exactly two
pixel columns, over three 28-row bands starting at y=50, 106 and 162:

    scr_buff[y * 32 + 1]  &= 0x7F;   /* x=8   */
    scr_buff[y * 32 + 30] &= 0xFE;   /* x=247 */

The bat band (rows 173..185) sits inside the third band, so both columns
are blacked there. `redraw_bat_dirty` repaints its window with
`paint_bg_window_to_buff` — the plain background tile — and never
re-applies those clears, so the column comes back as tile pixels. The
full path does not lose them because it restores from the static cache,
which was composed with `inner_border_line_c` already applied.

It only shows when the bat is clamped against a wall, because only then
does the bat's repaint window include byte 1 or byte 30. The rows that
differ are exactly those where the bat sprite's own pixels do not happen
to cover the column: at the left clamp rows 173, 174, 181, 182, 183, 185
lose it while 175..180 and 184 are masked by the bat.

Two earlier explanations in this file were wrong and are recorded here
so the reasoning is not repeated: the lives indicators (refuted — the
sprite covers rows 185..190 and rows 186..190 matched), and the
running-dot row reflush (refuted — that would not produce a difference
confined to a single column).

**Fixed 2026-08-09.** `restore_inner_border_line(y0, h, byte_lo,
byte_hi)` re-applies the two columns inside any window that was just
repainted from the tile, and returns immediately when the window reaches
neither byte 1 nor byte 30 — which is every frame away from a wall.

BOTH narrow paths needed it, not one. The first fix went into
`redraw_bat_dirty` only and the gate still failed at 6 px: with the ball
hidden the scenario takes `redraw_bat`, the bat-only path, which
repaints the same window with the same tile. An incomplete fix that
looked complete.

`test-bat-redraw-window` is the gate, rewritten to park the bat against
the left clamp — deterministic (5/5 green) where it used to fail 5 runs
in 8, and it reaches the columns because only a clamped bat's window
includes byte 1 or byte 30. Mutation-checked: removing the call from
`redraw_bat` fails it at exactly 6 px.

## 12. A CATCH-held ball snapped to the default offset on a bat-only redraw — fixed

`run_level`'s bat-only redraw branch repositions a stuck ball with

    BALL_X = BAT_X + BALL_X_OFFSET_ON_BAT;      /* the constant 16 */

while `rest_ball_on_bat` — used by every other path that places a stuck
ball — uses `ball.stuck_offset_x`, the quantised offset the MAGNET/CATCH
bonus recorded (`& 0xFC`, capped `0x18`, so 0, 4, 8, 12, 16, 20 or 24).

They agree only when the catch happened to land on 16. Catch the ball at
offset 0 and the two disagree by 16 px.

Whether it is visible: the branch runs only when `bat_moved` and NOT
`ball_moved`, i.e. a frame with no physics tick where the bat's drawn
position is stale. On a ticked frame `ride_stuck_ball_on_bat` sets
`ball_moved`, so the ball_moved branch wins. So the snap would last one
frame and be corrected on the next tick — a flicker, not a lasting
displacement.

It matters beyond the flicker because the launch direction is derived
from that offset (`launch_offset = ball.stuck_offset_x - 4`). The
displayed position and the direction the ball will leave in can
therefore disagree for a frame.

**Fixed 2026-08-09**: `redraw_frame` now calls `rest_ball_on_bat()`,
which is the one function that knows where a stuck ball sits.

No pixel gate could catch this — it needs a CATCH at a non-default
offset AND a frame where the bat moved without a physics tick, and
`test-magnet-ball` catches at an offset that happens to agree. So the
guard is `test-stuck-ball-offset`, a source gate on the invariant:
every path that repositions a stuck ball goes through
`rest_ball_on_bat`. It was written first and failed against the old
code, which is the evidence that it guards the thing it claims to.

`respawn_primary_ball` is the deliberate exception — it sets
`stuck_offset_x = BALL_X_OFFSET_ON_BAT` on the line above, so the two
agree by construction.

---

## #13 — the primary ball's sign cache is written from extra balls

**Found 2026-08-09. Fixed 2026-08-09.**

`ball.dx` / `ball.dy` hold the PRIMARY ball's direction reduced to signs
(-1/0/+1). They are not the physics — motion is q8.8 in the object
descriptor — they are a summary, and exactly two things read them:

  - `ball_lands_on_bat()` gates the bat hit on `ball.dy > 0`
  - `apply_multi_ball_bonus()` derives the extras' launch directions
    from `delta_to_dir(ball.dx, ball.dy)`

Four places refresh them, now all through `refresh_ball_motion_signs()`.
Two of those four take an `Object *` that may be an **extra** ball:

  - `laffc_collision(o, ...)` — called from `step_extra_ball` (and from
    `magnet_ball_frame`, which itself runs for extras)
  - `magnet_ball_frame(o, si)` — si 1 and 2 are the extras

Both write the primary's cache from whichever ball they were handed.

### Why nothing breaks today

Ordering, and nothing stronger:

  - `step_ball` refreshes both from the primary at the top of its move,
    before `ball_lands_on_bat` reads `ball.dy`. So the extras' write from
    the previous frame is always overwritten before the hot read.
  - `apply_multi_ball_bonus` early-returns when extras are already
    active, and while no extras exist nothing else can write the cache.

Neither is a property of the writing code. Both are properties of code
elsewhere that could reasonably change.

### The reachable scenario

`step_ball` has two early returns above the refresh — a stuck ball, and
a magnet-handled frame. So:

  1. extras are out, one bounces off a brick → cache = the extra's signs
  2. the primary is stuck on the bat (CATCH), so `step_ball` returns
     before refreshing
  3. the extras die
  4. the player catches MULTIBALL while the primary is still stuck

The new extras then launch from the dead extra ball's last direction
rather than the primary's. Narrow, but not impossible.

### The fix

`refresh_ball_motion_signs` now takes the object it is refreshing FROM
and returns early for anything that is not `OBJ_BALL_1`. Passing the
object through makes the bug unexpressible at a call site, rather than
merely absent from today's call sites — `laffc_collision` and
`magnet_ball_frame` hand it their own `o` and it does the right thing
for either ball.

The guard is `test-ball-sign-cache-owner`, written first and failed
against the code at `d7daa83`, which is the evidence it guards what it
claims to. It is structural for the reason above: no pixel gate reaches
four coincident conditions. Extracting it also pulled in a second,
separate in-place implementation of the same sign reduction inside
`primary_ball_launch_from_bat`, which briefly left non-sign values in
the cache; that now goes through the owner too.

The full 55-gate suite is unchanged by the fix, which is the expected
result — the scenario it corrects is one no gate reaches.

This is the same shape as #8: a convention that writes state belonging to
another object. #8 turned out to be harmless because nothing read the
fields. Here something does read them, and only the frame ordering saves
it.

---

## #14 — `ball.dx`/`ball.dy` mix signs and speeds, and one reader cares

**Found 2026-08-09. Units fixed 2026-08-09; the question the units
raised is still open — see the end.**

Found while fixing #13. The sign cache is documented as holding
{-1, 0, +1}, and four of its six writers do exactly that. Two do not:

```c
ball.dy = -BALL_SPEED;      /* catch_ball_on_bat, deflect_ball_off_bat */
```

`BALL_SPEED` is 2, so those store -2.

### Why that is not obviously harmless

`delta_to_dir` — which has exactly ONE production caller,
`apply_multi_ball_bonus(delta_to_dir(ball.dx, ball.dy))` — selects its
angle by MAGNITUDE:

```c
const u8 angle = (abs(dx) >= BALL_SPEED) ? 0x08 : 0x04;
```

It reads `dx`, and no writer ever puts a magnitude in `ball.dx` — only
`ball.dy` gets the -2. So today the angle is **always 0x04** and the
0x08 branch is unreachable from the only caller. Whether the original
derives the extras' launch angle from the ball's real velocity (in which
case reducing to signs before calling this destroys the input it wants)
is not established, and settling it needs the Spectrum, not the port.

`test_delta_roundtrip_quadrants` passes +-2, so it exercises the angle
production never selects and skips the one production always selects.
`test_delta_to_dir_sign_inputs` now pins the actual behaviour, so a
change to either side is a decision rather than an accident.

### The units are fixed

Three separate things put non-signs into the cache. All three are gone:

  - `deflect_ball_off_bat`'s `ball.dy = -BALL_SPEED;` was a **dead
    store** — the deflection immediately below rewrites the direction and
    calls `refresh_ball_motion_signs` unconditionally, with no return in
    between. Deleted.
  - `catch_ball_on_bat`'s copy is the one that SURVIVES, because a caught
    ball is stuck and `step_ball` early-returns above the refresh. Now
    stores `-1`.
  - `primary_ball_set_velocity` assigned its arguments straight through,
    and two of its three callers pass `-BALL_SPEED`. It now routes them
    through `refresh_ball_motion_signs`, which normalises. Fixing it in
    the setter rather than at each call site means the third caller —
    `BATTY_REPLAY_BALL_VEL`, where a gate can pass anything — cannot
    reintroduce the problem.

All behaviour-preserving, and provably so rather than by test: the sign
is the only thing any reader uses. `ball_lands_on_bat` tests
`ball.dy > 0`; `delta_to_dir` picks its quadrant on `dx >= 0` / `dy >= 0`
and its angle on `abs(dx)`. Confirmed by the full suite, 57/57.

Worth recording how the last one went: writing the normalisation inline
in the setter tripped `test-ball-sign-cache-owner`, the #13 gate, which
requires exactly one writer. It was right — that would have been a second
one. Routing through the owner was the fix, and the gate found it before
a human review would have.

### The open half, settled from the disassembly (2026-08-09)

The question was whether the original derives the extras' launch angle
from the ball's real VELOCITY, in which case reducing to signs destroys
the input it wants. `original/disasm/batty.asm` answers it at LA67B_8
(`$A67B`):

    LD A,(IY+$06)     ; the primary ball's DIR BYTE
    AND $0F           ; low nibble picks the branch
    LD DE,$080C  / CP $04 / JR Z,...
    LD DE,$040C  / CP $08 / JR Z,...
    LD DE,$0408       ; else
    LD A,(IY+$06) / AND $30 / OR E   -> ball2 dir
    LD A,(IY+$06) / AND $30 / OR D   -> ball3 dir

No velocity anywhere. It reads the dir byte and splits it: `$0F` for the
angle, `$30` for the quadrant.

`extra_ball_dirs` was already a faithful port of that table. The bug was
its INPUT: the port passed `delta_to_dir(ball.dx, ball.dy)` — a dir
RECONSTRUCTED from the {-1,0,+1} sign cache. `delta_to_dir` picks its
angle with `abs(dx) >= BALL_SPEED`, and a sign is never >= 2, so the low
nibble came out `$04` every single time. The port always took the FIRST
branch where the original varies with the primary's actual angle.

**Fixed**: `apply_multi_ball_bonus` now passes
`objects[OBJ_BALL_1].dir`, which is what `(IY+$06)` is. One line, and it
removes the round trip entirely — the sign cache is no longer involved
in the multiball spawn at all.

Full suite 59/59 green after the change: no gate reached a multiball
spawn from a primary whose low nibble was not `$04`, which is why this
survived. `delta_to_dir` now has no production caller.

---

## #15 — `bios_ticks()` does not advance during gameplay

**Found 2026-08-09. Fixed 2026-08-09.** Two wrong conclusions on the way,
both recorded, because the way this was got wrong is the useful part.

`play_game_over` held the game-over screen for 65 BIOS ticks (~3.6 s at
18.2 Hz), and `blink_phase()` was `(bios_ticks() >> 1) & 1`. The screen
never gave way on its own — captures every 2 s from 8 s to 40 s were
pixel-identical — but it yielded to a keypress instantly.

### First conclusion: right, but unproven

The original entry guessed `bios_ticks()` returns a constant, reasoning
that this hold is its only live user (every other `bios_ticks` timeout
goes through `TIMED_OUT`, gated on `auto_advance`, which is never
assigned), so nothing else would notice.

### Second conclusion: wrong, and confidently so

A `clocks=` line was added to PROBE.TXT and read from three runs:

```
wait= 6s  clocks=bios275796
wait=12s  clocks=bios275923
wait=20s  clocks=bios276142
```

The counter moved, so the guess was declared refuted.

**It was not.** Those are three separate BOOTS. QEMU seeds the BIOS
tick count from the host clock at power-on, so what those numbers
measured was host wall time passing between three runs — a rebuild plus
a boot each. The counter looked alive because a fresh one was handed out
every time. The entry even said a rate could not be derived from them,
and then treated them as proof anyway.

### What actually settled it

Two readings inside ONE run. Both clocks are now latched at the first
gameplay frame and reported again at the probe checkpoint:

```
FRAME_PROBE=200  probe_phase=play  clocks=bios289687_pit754_dbios0_dpit678
```

678 PIT frames — about 13.6 s at 50 Hz — with the BIOS counter advancing
by **zero**. During gameplay `bios_ticks()` is frozen.

The port reprograms PIT timer 0 to ~50 Hz and chains the original INT 8
on an accumulator overflow, specifically to keep the BIOS time-of-day at
$0040:$006C ticking at 18.2 Hz. That chaining is not working. Why it is
not is a separate question and is NOT answered here.

### The fix

Both live users now count PIT frames, which demonstrably advance:

  - the game-over hold: 65 BIOS ticks at 18.2 Hz = 3.57 s = 178 PIT
    frames at ~50 Hz
  - `blink_phase()`: 2 BIOS ticks = 0.110 s = 6 PIT frames, preserving
    the ~4.5 Hz half-period the original had

Confirmed: with no key sent at all, the game-over screen now gives way and
name entry is on screen by 9 s.

The BIOS chaining itself is left alone. It has one remaining consumer,
`TIMED_OUT`, which `auto_advance` keeps permanently false — so nothing
depends on it, and fixing a timer chain nothing reads is not worth the
risk to the 50 Hz frame clock everything else depends on.

### What this cost

The frozen clock made two things wrong that nobody had noticed: the
game-over screen waited forever, and the name-entry cursor never blinked
(`blink_phase()` returned a constant). Both are visible in normal play.
Neither was caught, because a player always presses a key.


---

## #16 — two enemy margin-escape angles aim out of the field

**Found 2026-08-09. NOT fixed, and deliberately so: it needs ground
truth this port does not have.**

`enemy_target_away_from_margins` exists so an alien near a wall stops
picking random targets and aims at a fixed angle that leads away — the
header says "so it cannot grind along an edge". Six cases, and four of
them do that.

Direction convention, measured from `dir_to_dxdy` (not assumed —
`dir_to_delta` is mirrored in two quadrants, known-bugs #8):

    $00 right   $08 down-right   $10 down   $18 down-left
    $20 left    $28 up-left      $30 up     $38 up-right

`$10` being straight down agrees with the note in `handling_bird_obj`
about an earlier port getting that wrong, so the convention is anchored.

| case | angle | direction | inward? |
|---|---|---|---|
| left edge, upper | `$08` | down-right | yes |
| left edge, lower | `$00` | right | yes |
| **right edge, upper** | `$38` | **up-right** | **NO** |
| right edge, lower | `$20` | left | yes |
| top edge, left half | `$08` | down-right | yes |
| **top edge, right half** | `$38` | **up-right** | **NO** |

At the right edge, aiming right is outward. At the top edge, aiming up
is outward. The top-right case is both at once.

`$18` (down-left) would be inward for both, and differs from `$38` by
one bit ($20).

### It is NOT a transcription slip — there is nothing to slip from

My first reading was that someone mis-copied `$18` as `$38` out of
LAA7D. `notes/enemy-movement.md` says otherwise, in its own open-items
list:

> **Margins.** The original's `check_margins` vs the port's
> `enemy_target_away_from_margins` is still an approximation.

So these six angles are the PORT's invention, not the original's table.
That makes the finding sharper, not weaker: the routine exists to aim
the alien away from a wall — its own header says "so it cannot grind
along an edge" — and two of its six cases aim into one. It is
self-inconsistent on its own terms, provable without any capture.

It also means the bounce that precedes it has just reflected the alien's
`dir` off that wall, so at the top-right the alien is moving left while
being steered up-right: back into the corner it just hit.

### Why it is still not fixed

Changing `$38` to `$18` would swap one approximation for another. It
would be more self-consistent, but "more sensible" is not "closer to the
original", and nothing here measures the original's margin behaviour at
all. The port has no gate pinning what the original does at an edge, so
the change would be unverifiable in the direction that matters.

### Settling it: what works and what does not (attempted 2026-08-09)

The oracle path RUNS here. `tools/zesarux/src/zesarux` is built,
`capture_enemy_flight.py` frame-steps the original's `object_enemy`, and
the target byte is readable: `--probe-ball 0x9BA8` puts `$9B96+$14` in
the printed `x=` field. Baseline on the unmodified L3 flight reads
target `$10` for the first frames and `$2C` later — `$10` matching the
fresh alien's documented target, which confirms the offset.

What does NOT work is poking the alien to the right edge via a replay's
`write_memory` SETUP ops. The L3 state spawns a FRESH alien during the
run, at x=168 y=1 (exactly as `test-enemy-descend` documents), so a
setup-time poke is overwritten before the first probe. Verified: with
`$9B98`/`$9B9A` set to `$F0`/`$04`, the capture still reads x=168 y=1.

`capture_frame_timeline_original.py` now takes
`--poke-at-frame FRAME:ADDR:BYTES`, which writes after reaching that
frame and before its probe. With it, the alien can be placed:

    --poke-at-frame 10:0x9B98:0xF0   x = 240
    --poke-at-frame 10:0x9B9A:0x04   y = 4
    --poke-at-frame 10:0x9B9C:0x00   dir = $00 (rightward)

### What the original actually did — and it is not what I expected

    frame 14  x=240 y=8 dir=$00   target=$2C
    frame 18  x=243      dir=$3F   target=$2C
    frame 22  x=247      dir=$3E   target=$2C
    frame 26  x=251      dir=$3D   target=$2C
    frame 30  x=255      dir=$3C   target=$2C

The target NEVER CHANGES. The original did not re-aim at all between
x=240 and x=255; `dir` simply walks toward the `$2C` it already had,
one step at a time, exactly as `enemy_turn_towards_target` does. And the
alien is not bounced or clamped — it travels to x=255.

So the question "does the original pick `$38` or `$18` at the right
edge?" has no answer at these coordinates, because the original picks
NOTHING here. The port re-aims where the original does not.

### What the original does at the edge, measured

Stepping past x=255 answers it:

    frame 29  x=254  dir=$3C   target=$2C
    frame 30  x=255  dir=$3C   target=$2C
    frame 31  x=8    dir=$3C   target=$2C
    frame 32  x=9    dir=$3C   target=$2C

The alien runs off the right, its x byte overflows past 255, and the
original's own LEFT clamp catches it at 8. Then it carries on rightward.

Three things the original did NOT do, across the whole run:

  - it never re-aimed: `target` is `$2C` from frame 10 to frame 44;
  - it never reflected `dir`: `$3C` across the wrap, unchanged;
  - it never clamped at the RIGHT edge at all.

The port does all three. `bounce_enemy_off_margins` clamps to
`x_max = 256 - 8 - w`, reflects `dir` with `(0x20 - dir) & 0x3F`, and
calls `enemy_target_away_from_margins`. None of that has a counterpart
here, which is consistent with `notes/enemy-movement.md` calling the
margin handling an approximation.

So #16's original question — `$38` or `$18`? — is the wrong question.
The angle table belongs to a re-aim the original does not perform.

### The disassembly explains all of it

`original/disasm/batty.asm` settles what the captures could only hint at.

    check_margins:                  ; called by handling_bird, handling_ufo
      CALL check_top_margin
      CALL check_left_margin
      JP   check_right_margin       ; ...three CLAMPS, and nothing else

    bounce_wall:                    ; called by handling_ball, handling_spark
      LD B,$3F / CALL check_top_margin  / CALL C,change_direction
      LD B,$1F / CALL check_left_margin / CALL C,change_direction
      CALL check_right_margin / RET C / JP change_direction

Two routines over the same three checks. The ENEMY gets the clamp-only
one; the reflecting one belongs to the ball and the sparks. So the
original never turns an alien at a wall, and never re-aims it — exactly
what the capture showed.

    check_right_margin:
      LD A,(IX+$0C)     ; width
      ADD A,(IX+$02)    ; + x        <-- 8-BIT ADD
      CP $F9
      RET C             ; no clamp if width + x < $F9
      LD A,$F8
      SUB (IX+$0C)
      LD (IX+$02),A     ; x := $F8 - width

The bird's width (`+$0C`) is 24, measured. So the clamp fires for
`x >= 225` and sets `x = $F8 - 24 = 224`. But the `ADD` is 8-bit: for
`x >= 232` the sum passes 255, WRAPS below `$F9`, and the clamp silently
does not fire. That is an overflow escape window in the original, and it
is why the poked alien at x=240 walked off the edge and reappeared at 8.

Predicted and confirmed: poking the alien to x=228 — inside the working
window — clamps it to exactly 224, with `dir` unchanged through the
clamp.

### Port versus original, complete

| | original | port |
|---|---|---|
| clamp value | `$F8 - width` = 224 | `x_max = 256 - 8 - w` = 224 — same |
| clamp range | `x` 225..231 only | every `x > 224` |
| `x >= 232` | overflow, NO clamp, runs off and wraps | clamped |
| reflect `dir` | never | `(0x20 - dir) & 0x3F` |
| re-aim target | never | `enemy_target_away_from_margins` |

The clamp VALUE matches. The other three rows are port inventions, and
the `$38` angle that opened this entry is a detail of the last of them.

### Why it is still not fixed

Now a design question rather than an unknown. Reproducing the original
means reproducing an 8-bit overflow that lets an alien traverse the
screen edge — faithful, and startling to watch. The port's version is
tidier and diverges. Neither is obviously right, and nothing anywhere
records that the choice was ever made; it looks like the port simply
reused ball-style bouncing for the enemy.

That decision belongs to whoever owns the port's fidelity goals, not to
a bug report. What this entry can do is make sure it is a decision.

The measurement is one artificial scenario: the alien was poked to
x=240, y=4, dir=`$00` and driven into the edge. It shows what the
original does there. It does not show that the original NEVER re-aims —
`check_margins` exists in the disassembly and may fire under conditions
this run did not reach.

And the port's clamp is not obviously wrong to have: an alien whose x
wraps to the far side of the screen is startling, and the port may have
added the clamp deliberately. That decision is not recorded anywhere,
which is the real gap.

What is now certain is that the port and the original differ at an
alien's right margin in three separate ways, and only one of them (the
angle table) was previously noticed.

Still not fixed, and now for a better reason: changing `$38` to `$18`
would refine a re-aim the original may not perform at all.

### What is in place meanwhile

`test_margins_aim_inward` now pins all six angles as a value table. So
if someone verifies the original and the values change, the test fails
and forces the decision to be explicit rather than silent. That is the
useful state for an unresolved question: the current behaviour is
recorded and cannot drift unnoticed.
