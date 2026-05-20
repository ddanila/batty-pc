# Parity pass 2026-05-20: RNG and ball effects

Scope: skip sound and fix three more gameplay parity differences
against the original disassembly.

## Findings

1. `next_random` used a generic C LCG. The original `random_generate`
   at `$8EB4` keeps two bytes initialized to `$8E17`, walks source
   bytes from `$8000..$9FFF`, and folds in `ctrl_btns_pressed`.

2. Bonus spawning used the low byte for both the drop gate and table
   index, and the late-level rocket rarity check consumed a separate
   RNG value. The original uses `random_number+$01` for the drop gate
   and table index, then uses the low byte from the same generated
   value for the rocket rarity check.

3. Triple-ball fan-out mirrored/invented integer velocities. The
   original derives ball 2 and ball 3 directions from ball 1's 6-bit
   direction low nibble: `$04 -> $0C/$08`, `$08 -> $0C/$04`, otherwise
   `$08/$04`, preserving the quadrant bits.

## Result

- Added `assets/random_seed.bin`, the original `$8000..$9FFF` source
  window used by `random_generate`, and ship it on both DOS floppies.
- Replaced the LCG with the original two-byte RNG update shape and
  changed bonus selection to consume the high/low bytes in the same
  places as the Spectrum code.
- Changed triple-ball spawn to apply the original direction split.
- Tightened SMASH duration to the original `$F8` half-rate counter
  instead of an approximate 500-frame countdown.

Remaining non-sound gaps are still listed in `parity-gaps.md`.
