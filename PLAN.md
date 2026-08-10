# PLAN — road to a 100% functional Batty port

A **fully functional DOS port of Batty** (Elite/Hit-Pak, 1987, ZX Spectrum
48K): everything the original does, verified against it wherever it can be
measured.

**The minimum target is a 386** — 32-bit protected mode under DOS32A, to
escape the 64 KB segment ceiling. There is no 8086 build.

Status detail lives in `notes/parity-status.md`, open fidelity gaps in
`notes/parity-gaps.md`.

## Definition of done

| # | Criterion | Today |
|---|-----------|-------|
| 1 | All three modes: 1 Player, 2 Players (alternating), Double Play | **Done** — including the court, both bats, split-keyboard input, catches, scoring and per-bat bonuses |
| 2 | Menu semantics match the original | Key 0 starts the game (`test-menu-start`); the device byte is per-player state but selects nothing |
| 3 | Core gameplay byte-exact where an oracle exists | **Done** — regression-locked by 110 gates |
| 4 | Full game FLOW gated end-to-end | **Done** — `test-level-advance`, `test-life-loss`, `test-life-respawn`, `test-game-over-visual`, `test-name-entry-visual` |
| 5 | Sound faithful to the 5-slot beeper queue | ids, slot count, pitches and envelope arithmetic faithful; durations still round to 20 ms |
| 6 | All assets derived from the tape at build time | **Done** — all 14 loaded assets build from `original/blocks/`, held by `test-asset-provenance` |
| 7 | Runs on real-hardware-representative targets (386+) | QEMU and DOSBox-X verified; real iron untested |
| 8 | Historical completeness | **Done** — pause, hi-score, and the Kinnock egg (`BATTY_KINNOCK=1`) |

*Table refreshed 2026-08-10.* `test-plan-table-fresh` fails when a workstream
records a date newer than this one, so the table has to be re-read on the day
work lands.

Every workstream keeps `make parity-check` green, and milestone work runs
`parity-check-full` before merge. **WS2** (2 Players), **WS3** (Double Play —
`notes/double-play.md`), **WS4** (game-flow transitions) and **WS7** (asset
self-sufficiency) are complete; their scope is closed.

## Where we are

Playable end-to-end in all three modes: title → menu → hi-score → all 15
levels → game-over → initials → title. 2-player hands the turn over on each
life loss and on a player running out; Double Play runs two bats on a split
court with per-bat bonuses and side-attributed scoring.

All 15 levels are pixel-perfect at entry. Ball motion, LAFFC brick collision,
bat deflection, the RNG walk, enemy motion and animation, the bonus economy,
scoring and every per-frame animation are byte-exact against the Spectrum and
gate-locked. Rendering performance is at its floor. `original/disasm/` is a
complete, named, build-verified disassembly, so the workflow is "read the
disasm, port the routine, gate it."

## WS1 — Menu start semantics

**Done:** key 0 returns `ST_LEVEL`, which runs `new_game_reset` and enters the
round-1 banner; unrecognised keys re-poll. Gated by `test-menu-start`. It is 0
only, not 0/ENTER — ENTER belongs to the attract chain.

**Open — a decision, not code.** `p1_dev`/`p2_dev` are per-player state that
selects nothing. The original's device list (Kempston / Sinclair / cursor
joysticks) is Spectrum hardware, so a literal port makes no sense on a PC. The
proposal is to keep the menu's A/B cycling and offer keyboard set 1, keyboard
set 2 and the game-port joystick (0x201 / INT 15h AH=84h), with a "not
detected" fallback — which means changing the menu strings, a deliberate
deviation from pixel parity. Recommendation: change them; honesty beats fake
parity here.

Two things wait on this decision: both of the original's split-keyboard
readers bail to the per-device poll unless BOTH players are on `ctrl_type` 0,
and player 2 has no say yet in which keys they would rather use
(`notes/double-play.md`).

## WS5 — Sound

**Done:** the 12 queued effect ids are table positions and byte-exact against
`play_sounds_list`; slot count, pitches and envelope ARITHMETIC are faithful;
`sound_beep_cont_d` and `sound_beep2_bd` compute real envelope lengths; the
`LC122` sweeps run their full length. `SND_MAGNET` sits deliberately outside
the table — the original never queues it.

**Open — a design call.** Durations round to 20 ms because the sound clock is
the 50 Hz frame counter. The original's beeper BLOCKS: `sound_beep` is DJNZ
spin loops around `OUT ($FE),A`, so the queue eats 3-9 ms of frame time and
the main loop branches on whether it fitted inside one interrupt. A PC needs
none of that — the PIT holds a tone with no CPU. So the choice is between
blocking like the original (faithful to the millisecond, and it imports the
frame-pacing behaviour) and keeping the latch (needs the stop scheduled from
an interrupt, and the port's timer is the same 50 Hz). Decode and both
options: `notes/sound.md`.

## WS6 — Gameplay-parity residuals

1. **Enemy vs bricks — done.** `enemy_home_step` (LAA44) and
   `enemy_brick_reaction` ported; `check_margins` is three clamps. Gated by
   `test-enemy-brick-walk` and `test-enemy-margin-clamp`.
2. **MAGNET catch for secondary balls — done.** The stuck state is per-ball,
   which was the same refactor bat 2's catch needed.
3. **Byte-exact enemy target gating — open.** Needs the port's counter pin
   aligned with the original's `counter_misc` at the same moment.
   `BATTY_REPLAY_COUNTER` makes the port side deterministic; the comparison
   against the ORIGINAL is what is missing. See `notes/rng-model.md`.

## WS8 — Infrastructure

1. **CI — done.** `ubuntu-latest` exposes `/dev/kvm`, and with the udev rule
   from GitHub's own docs the QEMU gates can run at local speed.
   `qemu-smoke-kvm` is a `continue-on-error` job that measures it; nothing
   depends on it yet. CI also runs `make test-asan`, the only place the suites
   are compiled by g++ rather than Apple clang. `notes/testing.md`.
2. **Real hardware — open.** `make floppy` produces a bootable 1.44 MB image;
   one verified boot on a real 386 or better retires criterion 7. The
   load-time lever that survives is batching the small asset `fread`s, which
   helps a real floppy even though emulated disks hide it.

## WS9 — Polish, history, distribution

1. **Kinnock easter egg — done** (`BATTY_KINNOCK=1`, `test-kinnock`).
2. **Docs hygiene — done.**
3. **Distribution — a user call, before any publicity.** Elite Systems still
   monetizes Batty (official iOS/macOS app) and historically denied archive
   distribution; the CityAceE disasm repo has no licence. This repo vendors
   the tape, TZX and captured screens. Choose between (a) keep private,
   (b) publish code only with originals supplied by the user at build time —
   WS7 makes this clean — or (c) ask Elite. Until then, do not distribute
   built images containing original assets.

## Accepted residuals — do not reopen

- **Cycle-exact sound timbre.** Envelope-faithful is the target.
- **The 4 px L3 frame-step brick-edge nuance**, root-caused to the capture
  phase: the port's probe halts after the frame's update, ZEsarUX breaks at
  the top of the main loop, so port frame N samples orig frame N+1.
  `notes/metal-shimmer.md`.
- **The rocket end-tally's pace.** The original's per-brick `pause_short` is a
  Z80-clock busy-wait; the port paces one brick per PIT tick. Score total,
  render and order are faithful.
- **Big-bat resize as a literal bit-gated state machine** — visually matched.
  This residual is not free: it owns `play_sound_bat_resize_1`'s `bonus_flag`
  guard, which the port lacks, so a WS5 sound divergence cannot be closed
  without it.

**When a new gap traces back to an accepted residual, update the residual.**
This list is the one entry nobody re-reads, so the trade-off recorded here has
to stay current.
