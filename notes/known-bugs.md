# Known bugs (user-reported, unfixed)

Concrete defects observed by the user that aren't yet caught by the
visual-regression test (= mid-game state we don't snapshot). Listed
here so the next iter has a target. When fixing, add a section to
`per-level-profile.md` or the relevant area doc; remove from here.

4. **Initial hard-block shimmer plays much too fast** (the on-hit shimmer
   is fine). The pre-round all-metal-bricks animation
   (`all_metal_briks_animation_snd` $B765) shows each of the 8 anim_brik
   frames for exactly TWO 50Hz interrupts (`EI/HALT/EI/HALT/DI`, ~40ms)
   and is NOT interruptible by input. The port's `play_brik_anim`:
   (a) waits `pit_ticks() - t < 2` from a mid-tick sample = 1..2 ticks
   (~30ms avg, 25% fast), and (b) — the big one — ABORTS the whole
   animation on any buffered keypress (a port invention). Keys held /
   typematic-repeating at level entry (very common: moving the bat into
   position, pressing FIRE) skip the animation almost entirely.
   FIX: wait two full tick edges per frame (HALT-equivalent), draw after
   the wait, and don't consume/abort on non-ESC keys.

5. **Magnets don't affect the ball** (they should bend its trajectory
   while ON). The port renders magnets at level paint but the entire
   runtime system is missing: (a) the per-frame random toggle
   (`LB9E8_2`: when `random_number+1 == $99`, `print_one_magnet` $8E72
   picks a random magnet, flips its $06/$07 ON/OFF sprite state, redraws
   the circle + plays a sound); (b) the ball capture/curve physics at the
   top of `handling_ball` (LA27E_0..11): while a ball overlaps an ON
   magnet's 15×14 box (slot coords = paint x0+5,y0+5, obj_compare $AC22),
   its direction rotates ±1/64 per frame (sign from the dir quadrant and
   ball-above/below-centre test), and it releases with a quantized exit
   dir `(dir+2)&$3C` (±4 nudge off the pure axes) + a 2-frame
   re-capture cooldown when the magnet turns OFF or the ball leaves the
   box. Also found while decoding: the port's initial coin-flip is
   read-current (`rng_sample`) and INVERTED — the original `print_magnets`
   does `CALL random_generate` per magnet (advances!) and keeps the
   magnet ON when bit0==1 (`RRA / JR C,skip-OFF`); the port draws OFF
   when bit0==1 and gives every magnet on the level the same coin.

---

Resolved history:

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
