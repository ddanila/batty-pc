# Parity pass 2026-05-20

Scope: identify three remaining visible/behavioral mismatches against
the original Batty disassembly and fix them in the DOS recreation.

## Findings

1. Falling bonuses used a constant `+2 px/frame` fall. The original
   `handling_bonus` enters `LA55A_0` with `DE=$0008, B=$02`, so the
   object accelerates from rest to a capped 2 px/frame speed.

2. Alien bombs also used constant `+2 px/frame`. In the original,
   bombs are `object_bonus` with `sprite_num=$0A`, so they share the
   same `LA55A_0` movement as normal falling bonuses.

3. The `+400` marker used a short fixed lifetime and a simple every-
   other-frame Y increment. The original `handling_400pts` applies X
   drift, then jumps into `LA55A_0` with `DE=$0028, B=$80`; it remains
   active until its Y coordinate reaches `$C0`.

## Result

- Added a small `LA55A_0`-style fixed-point motion helper.
- Routed falling bonuses and bombs through the `DE=$0008, B=$02`
  accelerator and reset their accumulator on spawn.
- Routed the `+400` marker through the `DE=$0028, B=$80` accelerator,
  including the original signed initial high-byte shape selected from
  frame parity, and made it expire at the playfield bottom instead of
  by a fixed timer.

The broader RNG and triple-ball direction-table gaps remain documented
in `parity-gaps.md`.

## Verification

- `make all`
- `make test-hud`
- `make test` (at the time, same accepted `state4_level1` 12-pixel drift;
  later fixed by matching the original's top-border/inner-line net order)
- `python3 scripts/exercise_bonus.py` ran successfully; that natural
  smoke path did not spawn a falling bonus during its 30 sampled frames.
