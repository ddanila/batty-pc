# Known bugs (user-reported, unfixed)

Concrete defects observed by the user that aren't yet caught by the
visual-regression test (= mid-game state we don't snapshot). Listed
here so the next iter has a target. When fixing, add a section to
`per-level-profile.md` or the relevant area doc; remove from here.

(none currently)

(Not user-reported, but pending: **~21 px in-flight compose delta
between the dirty and full render paths next to a flying alien over
textured background** — what's left of the 2026-06-11 "trailing
residue" finding after the 2026-06-12 follow-up. NOT persistent:
single-checkpoint A/B captures at frames 60/70/80/100 after the
fly-over are 0 px — the screen heals completely; the delta exists only
while the sprite is in flight (frame-50 capture, UFO at y 51..66,
black-vs-texture at rows 49..57 of its column). One of the two paths
mis-handles the sprite's XOR-shadow over the bg texture; deciding WHICH
needs the original as oracle (a ZEsarUX A/B with a seeded alien), not
the dirty-vs-full comparison. Repro:
`python3 scripts/repro_enemy_flyover_trail.py` (expected-FAIL, 21 px).
The other two findings from that harness are resolved: the frame-12
top-band 247 px diff was a real full-path bug (restore_top_frame_center
ran AFTER the object compose, erasing sprite slices over the frame
centre — fixed 2026-06-12, A/B now 0 px at f12); the frame-100 713 px
diff was a measurement artifact (multi-checkpoint timelines re-randomize
the counter phase at every halt — see the lessons.md entry).)

---

Resolved history:

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
