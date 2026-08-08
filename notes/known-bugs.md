# Known bugs (user-reported, unfixed)

Concrete defects observed by the user that aren't yet caught by the
visual-regression test (= mid-game state we don't snapshot). Listed
here so the next iter has a target. When fixing, add a section to
`per-level-profile.md` or the relevant area doc; remove from here.

(none currently)

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


## 10. The bullet animation phase depends on which redraw path ran

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
