# Known bugs

All fixed. One open QUESTION about the ORIGINAL remains (#14); it needs a
Spectrum and is not a defect in the port. The table is the index; the
sections below keep only what the fix does not say for itself.

## Status

| # | what | state |
|---|------|-------|
| #3 | multi-hit bricks shimmer continuously | fixed; `test-shimmer-one-pass` |
| #8 | multiball direction convention mirrored in two quadrants | no gameplay effect — the mirrored values went to fields nothing read |
| #9 | blast dirty rect 16x8 vs 16x12 | fixed; `test-blast-dirty-redraw` |
| #10 | bullet animation phase depends on the redraw path | fixed; host tests |
| #11 | narrow bat redraw loses the inner border line | fixed; `test-bat-redraw-window` |
| #12 | stuck ball snapped to the default catch offset | fixed; `test-stuck-ball-offset` |
| #13 | extra balls write the primary's sign cache | fixed; `test-ball-sign-cache-owner` |
| #14 | the sign cache mixed signs and speeds | fixed; one question about the ORIGINAL left open below |
| #15 | `bios_ticks()` frozen during gameplay | fixed; `test-frozen-clock` |
| #16 | the port invented a bounce where the original only clamps | fixed; `test-enemy-margin-clamp` |
| #17 | `test-enemy-descend` failed two runs in three | fixed; the gate asserts the implication and `BATTY_REPLAY_COUNTER` pins the phase |
| #18 | the partial brick rebuild lost both boundary rows | fixed; `test-enemy-brick-residue` |
| #19 | ENTER steered bat 2, and ENTER starts every capture | fixed; ENTER dropped from the bat-2 cluster |
| #20 | destroying a TOP-row brick leaves a solid black line | fixed; `tests/test_bricks.cpp` |
| #21 | after a death the bat returns in fragments, magnets stay missing | fixed; `test-respawn-redraw` |
| #22 | the bottom five rows of every score digit are stale | fixed; `test-hud-patch-extent` |

## #3 — multi-hit bricks shimmer

The hit animation re-armed itself every frame instead of running once,
because the `(c+1) & $0F` wrap was read as "cycles forever" rather than as
the free-slot marker. `notes/metal-shimmer.md`. Gated by
`test-shimmer-one-pass`, written two months after the fix — until then the
whole QEMU suite passed with it broken.

## #8 — the extra balls' mirrored direction convention

`dir_to_delta` (extra balls) and `dir_to_dxdy` (the byte-exact primary)
disagreed in two of four quadrants. Harmless: the mirrored values went into
four `BallState` fields nothing read, while `step_extra_ball` moved the
extras with `dir_to_dxdy` like the primary. Found by following a
`(void)`-silenced parameter; a sweep of every state struct for the same
pattern found nothing else.

## #9 — the blast's dirty rect

The full path marked each blast slot `16 x 8`, the dirty path `16 x 12`. 8
is correct, per the frame heights in `assets/sprites.bin`.

## #10 — the bullet's animation phase followed the redraw path

The phase lived in a function-static incremented once per call, and the two
redraw paths call the renderer a different number of times. It lives in the
per-bullet `bullet_frame[]` now.

## #11 — the narrow bat redraw lost the inner border line

`inner_border_line_c` blacks out two pixel columns over three 28-row bands
from y=50. The narrow bat redraw restored the background across them
without putting the line back: 6 px at the left clamp, 2 px at the right.

## #12 — a held ball snapped to the default offset

`run_level`'s bat-only redraw repositioned a stuck ball with the constant
`BALL_X_OFFSET_ON_BAT` where every other path uses `ball.stuck_offset_x`,
the quantised offset the MAGNET/CATCH bonus recorded. One function decides
where a stuck ball sits now.

## #13 — extra balls wrote the primary's sign cache

`ball.dx`/`ball.dy` summarise the PRIMARY ball's direction as signs, and
`ball_lands_on_bat()` gates the bat hit on `ball.dy > 0`. Extra balls were
writing them, so a secondary moving upward could suppress the primary's bat
contact. Only the primary may write them now.

## #14 — the sign cache mixed signs and speeds

Four of six writers stored {-1, 0, +1} as documented; two stored
velocities. Fixed, and the multiball spawn reads the primary's `dir` byte
rather than reconstructing one from signs.

**The open question is about the ORIGINAL, not the port.** `delta_to_dir`
picks its angle by MAGNITUDE, and now that the cache provably holds signs,
its `0x08` angle is unreachable from its only production caller. Whether
the original derives the extra balls' launch angle from a real velocity
needs hardware. `test-multiball-source` holds the port's side.

## #15 — `bios_ticks()` does not advance during gameplay

The port reprograms the PIT to 50 Hz and never chains INT 8 to the BIOS
handler, so `$0040:006C` stops. Anything timing gameplay must use
`pit_ticks()`. Measured, not deduced: a probe latching both clocks at frame
1 and again at a checkpoint reported 0 BIOS ticks over 678 PIT ticks.
`test-frozen-clock` pins it.

## #16 — the port invented a bounce where the original clamps

`check_margins` is three CLAMPS and nothing else; the reflecting version
(`bounce_wall`) belongs to the ball and the sparks. The port had given the
alien ball-style bouncing plus a re-aim, and both inventions are gone.

The right clamp's 8-bit overflow — for `x >= $E8` the sum wraps below `$F9`
and the clamp does not fire, so an alien can run off the edge and reappear
— IS reproduced, deliberately. `notes/enemy-movement.md`.

## #17 — a gate that failed two runs in three

`test-enemy-descend` checked one arm of an implication whose antecedent was
not pinned, so it depended on the frame counter's phase at capture. The
gate asserts the implication now and `BATTY_REPLAY_COUNTER` pins the phase.

**The audit that followed matters more than the flake.** Every gate
asserting a derived value was checked for the same shape: assert an
implication, or pin the antecedent — never one arm and hope.

## #18 — the partial brick rebuild lost both boundary rows

The window's two boundary char rows are shared with the rows outside it,
and the base copy wiped both. It stayed invisible while the base band was a
capture taken with every brick alive, because that copy already carried the
neighbours' attrs; making the base band the EMPTY playfield turned it into
92 px of stale bright. `notes/performance.md`.

## #19 — ENTER steered bat 2

The original's right-hand cluster is K *or* Enter, and Double Play
transcribed both. ENTER is also this port's attract-chain key, pressed by
`--wait-key` to start every capture, so the harness nudged bat 2 by one
4 px step at a moment nothing controls.

**A gate written around a defect makes the defect permanent.** This same
race had already appeared in `test-double-play-input`, where the gate was
widened to accept both readings and a paragraph called that the honest
bound. It was not honest, it was accommodating — the behaviour was wrong
outside the harness too. When a measurement comes out two ways, the
question is which behaviour is right, not which bounds cover both.

## #20 — the top brick row leaves a line behind

Every standing brick zeroes the pixel row above its body — row 31 for brick
row 0. The incremental band rebuild deliberately does not erase the shared
top-edge row at the top of its window, because for an interior row that
pixel row belongs to the brick above. Row 0 has no brick above and the
window cannot widen past it, so nobody erased those zeros, and the rebuild
then CAPTURED row 31 into the static cache — which is what made the line
permanent rather than a flicker. `band_rebuild_window` in `src/bricks.cpp`
owns the paint window and the capture window as one decision.

**The existing test could not see it.**
`test_window_repaint_matches_full_at_its_edges` compares exactly these rows
on every level and was green throughout: it `memset`s the buffer to 0, and
the residue is made OF zeros. The new test paints on `0xFF`. Third time a
measurement here failed by picking a background the defect could hide in.

## #21 — the bat comes back in pieces after a death

Nothing `respawn_primary_ball` restores looks CHANGED to the per-frame
dirty tests: the bat returns to `BAT_X_INIT` with `prev_x` already equal to
it, same y, width and bonus byte, so `bat_changed()` is false and one row
of running dots gets flushed over a bat that is not on screen. Magnets
repaint only when they toggle, so one simply stays missing.

It was invisible for months because losing a life changes the life counter,
and `refresh_static_background` rebuilds on `lives_dirty` for the
indicators' sake. The repaint the explosion needed was arriving free from a
counter it has nothing to do with.
`invalidate_static_cache_after_death()` asks for it explicitly now.

**Accidental dependencies are invisible while the coincidence holds.**
`BATTY_INFINITE_LIVES` draws nothing; it removed the coincidence, and the
defect surfaced within minutes of someone PLAYING the build. No gate was
looking, and the suite was green.

## #22 — the bottom of every score digit is stale

The in-place HUD patch covered `FRAME_TOP_H_PX` (24) rows. The digits start
at y=`$15` and are 8 rows tall, so they reach row 28: rows 24..28 were
neither re-cached nor flushed and kept the previous score's pixels.
Reported from play as "very hard to tell", which is right — the top three
rows update normally and digits differ least at the bottom.
`HUD_PATCH_H_PX` is derived from the digit geometry now.

**The coverage hole is bigger than the bug.** The visual-test executable is
built `-dBATTY_SCORELESS_HUD` because the GT capture pipeline NOPs the
original's score block, so `render_hud_to_buff` is empty in every QEMU
gate. Not one screendump in the suite has a score on it, so the digits have
no visual coverage of any kind. `test-hud-patch-extent` holds the
arithmetic instead and says so itself; closing the hole means giving the
test build a HUD and re-baselining every visual gate. Not done.
