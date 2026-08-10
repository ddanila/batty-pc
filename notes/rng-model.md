# RNG model — the per-frame tick

The port reproduces the original's random sequence byte-exactly.
`BATTY_RNG_PERFRAME` defaults ON; `=0` reverts to the old advance-on-read
model. `make test-rng-walk` pins the walk.

## The function

`random_generate` and the port's `next_random()` compute the same step
from a ROM-walk seed:

```
E += (seed) + $05 + ctrl_btns_pressed
D += ~(seed) + $16 + L
random_number = DE
seed = (seed + 1) & $9FFF      ; wraps into [$8000,$9FFF]
```

`random_number` low byte = `random_e`, high = `random_d`. The `$8000-$9FFF`
source ships as `assets/random_seed.bin` (8 KB, indexed `& $1FFF`).

## The model

The original ticks the RNG ONCE PER FRAME at the main-loop top
(`LB9E8_2`), independent of any consumer; consumers then mostly READ
`random_number` without advancing. A few advance first.

So the port ticks once per frame at the play-loop top, and `rng_sample()`
reads without advancing where the original reads. Getting this wrong in
either direction desyncs everything downstream — a per-frame tick without
the consumer conversion just consumes the sequence faster.

### Which consumers read, and which advance

Read-current (`rng_sample()` — the original has no preceding
`CALL random_generate`):

- enemy target: the arrival re-pick `LAA7D_1` and `enemy_pick_new_target`
- enemy spawn X (`enemy_prepare` $9EAA: `LD A,(random_number) / AND $03`)
- bonus DROP CHANCE (`brik_value`: `AND $0F / CP $05 / CALL C,set_bonus`)
- `bomb_appear` ($A989) reads both bytes — it runs every alien frame, so
  this was the main per-frame polluter of the enemy's sequence
- the +400 marker's X drift (`LA67B`: `AND $01 / RL B`)
- the magnet TOGGLE gate (`random_d == $99`, sampled before the tick)

Advance-then-read (`next_random()` — the original `CALL`s first):

- bonus TYPE pick (`generate_new_bonus` re-calls per rejection retry)
- the magnet ON/OFF coin in `print_magnets`: `CALL random_generate /
  LD A,(random_number) / RRA` per magnet, ON when bit 0 == 1. Advances
  once per magnet at level paint, so magnet levels walk further than
  non-magnet ones. See `notes/magnets.md`.
- `print_one_magnet`'s slot pick (advance per rejection retry; returns
  before any RNG use when the count is 0, so non-magnet levels never
  perturb the walk)
- the alien-blast noise tone. The PC-speaker layer is an approximation
  anyway and it only fires on a kill.

## Two byte-order traps, both of which cost a false "diverges"

`random_number` lives at **$8D48** — `random_number: DEFW $8E17` declares
a VARIABLE whose INITIAL VALUE is $8E17, not a variable at $8E17. Probing
$8E17 shows a constant and reads as "the original never ticks".

And the seeds are little-endian 16-bit values:

| value | at | frame 0 of the L3 state |
|---|---|---|
| `random_number` | $8D48 | `BATTY_REPLAY_RANDOM=3793` (D:E, **not** `9337`) |
| `random_seed` | $8D4A | `BATTY_REPLAY_RANDOM_SEED=962A` (**not** `2A96`) |

`random_seed` increments +1/frame. With both seeded correctly the walk is
byte-identical: f0..f4 = 3793 / BB53 / 460D / 0990 / 6A76. A swapped
`9337` looks like a real algorithmic divergence — both output bytes move.

Bare `BATTY_REPLAY_RANDOM=8E49` still appears in several port-only gates
and is fine there: it seeds `random_number` deterministically and those
gates never compare against the original's sequence. What it does NOT do
is set the ROM-walk seed address, which is what
`BATTY_REPLAY_RANDOM_SEED` is for.

## What the correct walk bought

The enemy's arrival re-pick reads the right `random_number`, so the
steering matches the ground truth (`dir` climbs 0x11/0x12/0x13 at
f16/f20/f24 instead of falling the wrong way to 0x0E/0x0C). See
`notes/enemy-movement.md`.

One residual: `pit_frame_counter` counts from BOOT, and cadences gate on
its low bits, so which play-frame a re-pick lands on varies with boot
length. That is faithful — the original's `counter_misc` is global too —
and deterministic per binary, so it is a test-comparison problem, not a
play bug. `BATTY_REPLAY_COUNTER` pins the phase for gates
(known-bugs #17).
