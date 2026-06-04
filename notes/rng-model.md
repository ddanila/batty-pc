# RNG model — per-frame tick vs advance-on-read

This is the blocker for byte-exact parity of every RNG-dependent
behaviour: enemy target headings (see `notes/enemy-movement.md`), bonus
drop type/chance, enemy/bomb spawn timing, the magnet coin-flip, death
sparks. The RNG *function* is already an exact port; only the *calling
model* differs.

## The function (matches exactly)

`random_generate` ($8E…, disasm line 114) and the port's `next_random()`
compute the same step from a ROM-walk seed:

```
E += (seed) + $05 + ctrl_btns_pressed
D += ~(seed) + $16 + L
random_number = DE
seed = (seed + 1) & $9FFF      ; wraps into [$8000,$9FFF]
```

`random_number` low byte = `random_e`, high byte = `random_d`. Confirmed
identical; the byte-exact L3 ball gate relies on it via the seed override.

## The model (differs — this is the gap)

**Original.** `random_generate` ADVANCES `random_number`. It is called:

- **once per frame** at the main-loop top `LB9E8_2` (disasm line 6173) —
  the per-frame tick, independent of any consumer;
- plus on specific events: `generate_new_bonus` (line 1699), and two more
  sites (6293, 6461).

Consumers then READ the current value without advancing, e.g.
`LD A,(random_number)` / `LD A,(random_number+$01)` (lines 1700, 1766,
1878, 2785, 3031, 3397, 3557, 4844, 6163, …). So most consumers sample
the once-per-frame-ticked value; a few (bonus gen) advance first.

**Port.** `next_random()` ADVANCES and RETURNS in one call; every consumer
(8 sites) advances on read; there is NO per-frame tick. So the RNG
sequence the consumers see is desynced from the original's frame-by-frame.

## Status: staged foundation landed (flag OFF by default)

`BATTY_RNG_PERFRAME` + `rng_sample()` are in (the `BATTY_LAFFC` staging
pattern). Flag OFF: `rng_sample()` ≡ `next_random()` and no per-frame
tick, so behaviour is byte-identical — verified, the L3 ball gate and all
`make test` states stay green. Flag ON: a per-frame `next_random()` tick
runs at the play-loop top, and read-current consumers sample without
advancing. Done so far: the per-frame tick; the enemy consumers
(`enemy_turn_towards_target` reads `random_e`, `enemy_pick_new_target`
uses `rng_sample()`). Flag-ON smoke: boots fine and the (RNG-independent)
ball stays byte-exact. Remaining consumers below are converted +
validated incrementally before the default can flip.

## Alignment plan (deliberate; not a single safe edit)

1. Add a per-frame tick: one `next_random()` at the main-loop top,
   mirroring `LB9E8_2`.
2. Convert the consumers that the original READS (no advance) to read
   `random_e`/`random_d` directly instead of calling `next_random()`.
   (`enemy_turn_towards_target` already reads `random_e & $3F`.)
3. Leave consumers that the original ADVANCES-then-reads
   (e.g. `generate_new_bonus`) calling `next_random()` first.
4. Match the secondary call sites (6293, 6461) once identified.

## Why it isn't a 5-minute change

- It touches ~8 RNG consumers, each needing the original's advance-vs-read
  semantics decided individually (no ground truth for most).
- It risks the green `make test`: any RNG-dependent *rendering* at a
  captured frame can flip. The magnet coin-flip is the obvious one —
  though L1 (the level in `make test`) has no magnets, so the level/bat
  states may be RNG-independent; this needs checking, not assuming.
- A partial change is worse than none: the per-frame tick WITHOUT the
  consumer conversion just adds an extra advance per frame, desyncing the
  consumers further. Tick and conversion must land together.

So this is a deliberate, separately-validated refactor (ideally staged
behind a `BATTY_RNG_PERFRAME`-style flag like `BATTY_LAFFC` was), with
`scripts/capture_enemy_flight.py` and a bonus-drop capture as the
acceptance checks. The ball physics gates are unaffected (the ball is
RNG-independent), so they stay green throughout.
