# Parity pass 2026-05-20: rocket clear

Scope: skip sound and tighten three rocket-clear differences against
the original disassembly.

## Findings

1. Rocket Y movement used a constant `3 px/frame`. The original
   `handling_rocket` uses the `LA8CF/LA8D1` fixed-point accumulator:
   it begins with a slow fractional lift, then persists `-0x20` into
   the accumulator once `counter_misc >= $38`.

2. The rocket brick sweep used a narrow `16x14` placeholder body. The
   original rocket sprite is 3 bytes wide and `get_rocket` patches the
   first frame height to `$1B`, so the visible footprint is 24x27.
   Before pickup, startup / `all_var_init` patch the same sprite height
   byte to `$0C`, so the falling next-level bonus is the compact rocket
   pack, not the full flame trail.

3. Other objects and player actions continued during the rocket clear.
   The original `LBAED_6` hides the normal object set and enters a
   rocket-specific loop, so balls, bullets, aliens, bombs, and marker
   sprites do not keep playing underneath the clear sequence.

4. The rocket was flying independently of the bat. Original
   `handling_rocket` writes `rocket_y - 6` into both bat objects each
   tick, keeping the rocket attached while the bat flies upward.

5. When the rocket leaves the playfield, original `LBB97` goes straight
   to `LBBFB` / level clear. The temporary no-ball state must remain
   protected for that transition frame too.

## Result

- Replaced constant rocket motion with the original accumulator shape.
- Switched the rocket sweep box to the sprite footprint and restored
  the original `$0C` falling-bonus / `$1B` caught-rocket height patch.
- Cleared/hid active balls, bullets, enemy, bomb, and point marker when
  the rocket starts, and froze bat movement / laser firing while the
  rocket is active.
- Attached the bat Y coordinate to the rocket during clear, matching
  `handling_rocket`.
- Kept the rocket-complete transition on the level-clear path instead
  of falling into the no-ball bat-death path.

The remaining documented non-sound behavioral gap is enemy steering /
collision detail.
