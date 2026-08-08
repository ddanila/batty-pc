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

Not fixed, because it is not yet known which side is right. The primary
ball's convention is hardware-verified (dir `$1F` moves left, probed via
`capture_frame_timeline_original.py --probe-ball`); `dir_to_delta` has no
such anchor. Changing it would alter multiball trajectories, so it needs
an oracle capture of the original's extra-ball motion first — the
`$B7EA` + `$BA24` poke recipe in notes/testing.md generalises to it.

Existing gates do not catch this: `test-ball-paths-no-tunnel` asserts
extra balls never tunnel through bricks or escape the walls, both of
which hold regardless of which quadrant convention is used.


## 9. The two redraw paths mark different rects for a bullet blast

`redraw_full_with_ball` marks each live blast slot as `16 x 8`; the
dirty path's `render_simple_objects_to_buff_and_mark` marks the same
slot as `16 x 12`. Everything else about the two blocks is identical.

Found while sweeping the two paths for shared code after
`render_falling_objects_to_buff` and `render_extra_balls_to_buff` were
unified. Those two were byte-identical and could be merged; this one
cannot, because the paths genuinely disagree and it is not yet known
which height is right.

The blast sprite's real height would settle it — `spr_blast_frames`
indexes into `sprites_blob`, whose per-sprite header carries the height,
so it is readable without an oracle capture. Until then neither number
is provably correct: 12 over-marks and costs a little flush bandwidth,
8 under-marks and would leave stale pixels if the sprite is taller than
8 rows.

No gate covers it. The blast is drawn on both paths, but no gate
compares a blast frame across the dirty/full boundary the way
`test-*-dirty-redraw` does for balls, bombs and bonuses. That comparison
is the test worth writing before touching either number.
