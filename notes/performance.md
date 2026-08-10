# Rendering performance

**Rendering is at its floor**, measurement-backed. Both realistic profiles
show minimal avoidable work:

- `profile-ballbricks` (ball bouncing inside the brick band, emergent
  bonuses): 88 ball-only / 73 ball-object / 19 full-dynamic of 180 frames.
- `profile-multiball` (two extra balls in play): 0 / 178 / 2.

Every moving element has a dirty tier — ball (moving, stuck, multi, big), bat
including the fire animation, enemy, bonus, +400, bullets, blasts, bomb — a
brick hit costs one scoped band-rebuild frame, and both per-frame scan loops
are bounded to the dirty row span. The realistic scenarios started at ~100%
full-dynamic.

What is left is intrinsic, not headroom: `buff_to_vga` (1bpp -> 8bpp per
changed pixel), the per-frame object composition (proportional to what is on
screen), the restore `memcpy` (proportional to moving-sprite area), and a
handful of genuinely necessary full-dynamic frames (level entry, the scoped
band rebuild on a destroy).

If perf work resumes, the untouched lever is a DIFFERENT dimension: LOAD TIME.
Batching the small asset `fread`s helps a real floppy even though QEMU's fast
disk hides it (PLAN.md WS8.2). Further rendering changes would be marginal and
carry dirty-redraw gate risk.

## Profiling workflow

```sh
make profile-auto           # deterministic headless QEMU run
make profile-bricks         # laser bat + BATTY_AUTO_FIRE: brick destruction
make profile-ballbricks     # ball seeded INSIDE the band: normal play
make profile-multiball      # two extra balls
make profile-86box          # manual 86Box spot check, then: make read-profile
```

`profile-auto` injects `BATTY_RENDER_PROFILE=1`, `BATTY_START_LEVEL=1`,
`BATTY_LEVEL=$(PROFILE_LEVEL)` and
`BATTY_PROFILE_AUTO_FRAMES=$(PROFILE_FRAMES)` into `AUTOEXEC.BAT`. The game
starts in the level with a seeded moving ball, exits after that many rendered
frames, and writes `PROFILE.TXT`. Defaults: level 1, 180 frames.
`BATTY_RENDER_PROFILE` also disables sound, so PC-speaker I/O cannot hide
renderer cost.

## What the campaign taught

**Profile-driven, not assumption-driven.** Scoping the brick-band cache
rebuild to the dirty rows cut rows rebuilt 98 -> 28 as designed, and the wall
clock did not move: `build_static_brick_band_cache` is only ~4% of frame time.
The real cost of a brick-change frame was that `cache.band_dirty` raises
`BALL_DIRTY_BLOCK_STATIC` and blocks the ball-only fast path, so the whole
frame re-composited. The scoping is a
correct 3.5x reduction and is kept, but it was not the lever.

**Pick a realistic scenario before declaring a plateau.** The claim rested on
an open-bounce profile at 178/180 ball-only frames — a scenario with no enemy
that never multiballs. `profile-ballbricks` showed real play is
object-tier-dominated, and multi-ball forced 89 of 180 frames full-dynamic.

**The blockers came off one at a time, each with an A/B pixel gate** against a
`BATTY_FORCE_BALL_FULL_REDRAW` baseline mid-motion: bullets and blasts, the
bomb, the bat fire animation (so the laser worst case went 124 -> 21
full-dynamic frames), extra balls (~178 -> 2), big-ball and the stuck ball. A
bullet travels 6 px per frame — the fastest dirty sprite — so
`test-bullet-dirty-redraw` is the tightest of them.

**Deterministic harnesses, not emergent ones.** `BATTY_REPLAY_MULTIBALL`
spawns the extras BELOW the brick band with no bonus catch, which is
pixel-exact and repeatable; the first scenario relied on an emergent
MULTI_BALL catch entangled with the +400 popup.

## The three stale-VGA defects (known-bugs #1/#2)

User-visible leftovers — glitches after brick destruction, inverted-colour
residue after enemy fly-overs — were three cache-pollution defects in the
dirty system. Gated by `test-enemy-brick-residue`: 80 frames of the L3
brick-flash scenario with RNG and counter pinned, dirty path against a
`BATTY_FORCE_FULL_FLUSH_EACH_FRAME` baseline.

**Note the oracle choice.** The older dirty-vs-`FORCE_BALL_FULL_REDRAW` gates
CANNOT see this class: both sides flush by the same dirty ranges, so identical
staleness cancels out. Only the full-flush baseline pins VGA == buffers every
frame.

1. **Attr clash needs cell-granular flushing.** The enemy and blast attr blits
   recolour whole 8x8 cells while the dirty mark covered only the sprite's
   pixel rows, so boundary cells flushed mid-pass kept the clash colour.
   `mark_dirty_cell_rect_px` expands the rect to cell boundaries in Y (X is
   already byte == cell granular).

2. **The incremental band rebuild never flushed its own writes.** It rewrites
   whole band rows but relied on the brick flash's 18x10 rect, so the shadow
   attrs on the row below, the left dim and the neighbouring cells all went
   stale on VGA. The rebuild marks its own window dirty now, with
   `clear_dirty_ranges` moved ahead of the static branch so the marks survive.

3. **The rebuild window left boundary rows non-canonical**, polluting the band
   CACHE itself, which restores then spread — which is why leftovers appeared
   "when aliens fly over". Four interlocks: char row cr0 doubles as row r0-1's
   shadow row and cr1 as row r1+1's cell row, so the `level_attrs` base copy
   resurrected live attrs on destroyed boundary cells; vertically adjacent
   bricks write their top/bottom edges one pixel row into each other's cells;
   `brik_shadow_c(r1)` dims row r1+1's live cells, which only that row's own
   print re-brightens; and the background repaint erased the inner border line
   columns. Fix: recomposite `[lo-1 .. hi+1]`, guard the destroyed-reset
   writes to the base-copied char-row range, apply four edge fix-ups and
   re-clear the inner border line. Every destroyed row's full neighbourhood is
   re-derived, and the two outermost shared edge rows are patched per the full
   ascending paint order's overwrite semantics.

Cost: the destroy-frame flush grew from a flash rect to a band window, on
rebuild frames only, and the enemy's dirty rect is up to 14 rows taller while
an enemy is on screen. Steady-state frames are unchanged.
