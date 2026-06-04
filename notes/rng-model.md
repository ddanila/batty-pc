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

## RESOLVED (2026-06-04): random_number is at $8D48, and DOES tick/frame

A wrong-address scare, now corrected. `random_number: DEFW $8E17` makes
`random_number` a **variable** (in the data block at $8D46: `counter_misc`
$8D46, `random_number` **$8D48**, `random_seed` $8D4A) whose *initial
value* is $8E17 — it is NOT located at address $8E17. I had been
probing/seeding $8E17 (the init value), which is why it looked constant.

Probing the REAL address `$8D48` shows `random_number` **changes every
frame** (low bytes 0x53,0x13,0x90,0x76,0x8E,0x99,…). So:

- `random_generate` IS called once per frame (`LB9E8_2`), confirming the
  **per-frame-tick model (`BATTY_RNG_PERFRAME`) is correct**. The earlier
  "constant / do not flip" caveat was an artifact of the wrong address.
- A memory-write trace (`scripts/trace_enemy_target.py`, breakpoint
  `MWA=9BAAH`) confirms the enemy target is written by `LAA7D_1` at
  `$AA9C` with values that vary frame-to-frame (0x2C,0x36,0x03,0x1E,…) —
  i.e. `random_number($8D48) & $3F`, exactly the `LAA7D_1` decode. The
  decode was right; the contradiction was the address.
- **Seed bug:** the replay's `BATTY_REPLAY_RANDOM` writes `$8E17`, which is
  NOT the RNG variable, so it does NOT seed the original's RNG (the
  original runs from its snapshot `$8D48` value). Harmless for the
  byte-exact ball gate (RNG-independent), but it means RNG-dependent
  validation must seed/compare `$8D48`, not `$8E17`.

Path to byte-exact enemy/bonus RNG (now clear): (1) keep the per-frame
tick (flag ON is the right model); (2) seed the port's `random_d/e` to the
original's `$8D48` value at frame 0 (and fix the replay to poke `$8D48`);
(3) verify the port's `next_random` walk matches the original's frame by
frame against `$8D48`. The consumer read-current/advance classification
already done stands.

## VALIDATED end-to-end (2026-06-04): port RNG reproduces the original

Two fixes made this testable:
- The Makefile did NOT pass `BATTY_RNG_PERFRAME` / `BATTY_REPLAY_RANDOM_SEED`
  into the DOS `AUTOEXEC`, so the flag NEVER reached the build — every
  earlier "flag-on" test actually ran flag-OFF (no tick). Fixed the
  passthrough. (This invalidates the earlier flag-on enemy "divergence".)
- Added `BATTY_REPLAY_RANDOM_SEED` to seed `random_seed_addr` (the ROM-walk
  position = the original's `random_seed` at `$8D4A`).

Offline, `next_random` against the shipped ROM window (`random_seed.bin`,
8 KB, `& $1FFF`) reproduces the original's `$8D48` sequence EXACTLY from
the f1 state (0x460D, seed 0x962C). In the BUILT port (flag ON, seeded
`BATTY_REPLAY_RANDOM=460D BATTY_REPLAY_RANDOM_SEED=962C`):

```
        port              original
  f1:   0990 / 962D       460D / 962C
  f2:   6A76 / 962E       0990 / 962D
  f3:   9A8E / 962F       6A76 / 962E
  f4:   D899 / 9630       9A8E / 962F
```

The port reproduces the original's exact RNG values, **offset by one
frame**: the original has `f0 == f1` (no tick on its first frame boundary
— a frame-step setup artifact), while the port ticks immediately. So the
RNG model is byte-exact; the lone remaining alignment is that one-frame
head start.

### Ready to flip — but gated on an RNG-behaviour test

The L3 ball gate stays **byte-exact with the flag ON** (f40/f100/f150 all
match), so per-frame ticking + the descending (now correctly-moving)
enemy don't perturb the ball. And the port's RNG init (`random_e=$17`,
`random_d=$8E` → `$8E17`; `random_seed_addr=$8000`) MATCHES the original's
`random_number`/`random_seed` init, so flag-on would track the original
from a clean start too. So flag ON is the validated-correct model and
flipping the default is the natural culmination (BATTY_LAFFC pattern).

NOT flipped yet, deliberately: the ball gate is RNG-*independent*, so it
cannot prove the RNG-*dependent* behaviour (enemy targets, bonus drops,
magnets). The discipline is validate-then-flip; the last risky flip
(shimmer phase) regressed. The flip is ready once an RNG-dependent gate
exists — e.g. a flag-on test asserting the enemy target reaches `0x2C`
(its captured value) from the seeded L3 state. Building that gate (around
the documented one-frame offset) is the next concrete step before the
default flips. Resolving it (skip the port's first-frame tick, or seed to the
original's f0 pre-tick value) would make RNG-dependent reads — enemy
targets, bonus drops — frame-exact. Note the L3 seed value `0x460D` is the
snapshot's, not the env `8E49` (which wrote the wrong address `$8E17`).

## Status: staged foundation landed (flag OFF by default)

`BATTY_RNG_PERFRAME` + `rng_sample()` are in (the `BATTY_LAFFC` staging
pattern). Flag OFF: `rng_sample()` ≡ `next_random()` and no per-frame
tick, so behaviour is byte-identical — verified, the L3 ball gate and all
`make test` states stay green. Flag ON: a per-frame `next_random()` tick
runs at the play-loop top, and read-current consumers sample without
advancing. Done so far: the per-frame tick; the enemy consumers
(`enemy_turn_towards_target` reads `random_e`, `enemy_pick_new_target`
uses `rng_sample()`); the magnet on/off coin-flip (the original
`print_magnets` reads `random_number+$01` without advancing). Flag-ON
smoke: boots fine and the (RNG-independent) ball stays byte-exact.

**Validation is gated on FINISHING the conversion.** Read-current
consumers can't be validated piecemeal: any consumer still calling
`next_random()` advances the shared `random_number` between frames, so a
converted consumer's sampled value won't match the original until the
others are converted too. (Concretely: the enemy repick can't match its
`0x2C` ground truth while L3's bonus-drop consumers still advance — see
notes/enemy-movement.md.) So the remaining read-current consumers must all
be converted before the flag-on acceptance tests pass; each conversion is
flag-OFF byte-identical, so they land safely meanwhile.

### Consumer classification (against the original)

Converted to `rng_sample()` (read-current — original reads `random_number`
with no preceding `CALL random_generate`):
- enemy target (`enemy_turn_towards_target`, `enemy_pick_new_target`)
- magnet on/off coin-flip (`print_magnets`)
- bonus DROP CHANCE (`brik_value`: `LD A,(random_number+$01)/CP $05/
  CALL C,set_bonus`)
- `bomb_appear` (`$A989`: reads both bytes, no advance) — runs every alien
  frame, so this was the main per-frame polluter of the enemy sequence

Kept on `next_random()` (advance — original `CALL random_generate` first):
- bonus TYPE pick (`generate_new_bonus`: re-`CALL`s `random_generate` each
  retry, so each iteration advances)

- `enemy_prepare` spawn X (`$9EAA`: `LD A,(random_number)/AND $03`, no
  advance) — read-current. (Same edit also fixed the spawn dir/target to
  $10, see notes/enemy-movement.md.)

- 400pts marker X-drift (`LA67B`: `LD A,(random_number)/AND $01/RL B`, no
  advance) — read-current -> `rng_sample()`.

Left on `next_random()` (advance): alien-blast sound noise tone (`$C1A8`
region). The PC-speaker layer is an approximation, not a byte-exact beeper
port (see parity-gaps.md), and it only fires on an alien kill — outside
the enemy-flight window — so it neither needs nor can use a byte-exact RNG.

**The enemy-relevant consumer set is now complete:** every consumer that
fires during normal enemy flight (per-frame `bomb_appear`, per-brick bonus
drop-chance, enemy target/spawn, magnet, 400pts) reads-current; the bonus
TYPE pick correctly advances. So with `BATTY_RNG_PERFRAME=1` the enemy
target sequence should track the original (the seed-walk is aligned by
design: the port ships the $8000-$9FFF ROM window and starts the walk at
$8000). Acceptance test (next): seed the enemy to the L3 state
(`BATTY_REPLAY_ENEMY_OBJECT`, dir/target=$10, x=168, y=1, spd=1), run
flag-on with RNG 8E49, and confirm the target repicks to 0x2C at frame
~10 (notes/enemy-movement.md ground truth).

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
