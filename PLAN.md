# PLAN — road to a 100% functional Batty port

Goal of the repo: a **fully functional MS-DOS/8086 port of Batty**
(Elite/Hit-Pak, 1987, ZX Spectrum 48K) — everything the original game
does, the port does, verified against the original wherever it can be
measured. This file is the roadmap to that goal; status lives in
`notes/parity-status.md`, open fidelity gaps in `notes/parity-gaps.md`.

Last updated: 2026-07-06.

## Definition of done

The port is "100%" when all of the following hold:

| # | Criterion | Today |
|---|-----------|-------|
| 1 | All three game modes work: 1 Player, 2 Players (alternating), Double Play (simultaneous split-court co-op) | 1P only; modes 2/3 selectable but inert |
| 2 | Menu semantics match the original (0/ENTER starts the selected game directly; A/B input-device cycling affects play) | Start routes through the attract chain; device choice unused |
| 3 | Core gameplay byte-exact where an oracle exists (ball, bat, collision, RNG, enemy, bonuses, scoring) | **Done** — regression-locked by ~60 gates |
| 4 | Full game FLOW gated end-to-end: level-clear → next, life-loss → respawn, game-over → initials, level wrap | Sustained-play soak only; transitions unverified |
| 5 | Sound faithful to the original's 4-slot beeper queue (envelope/timing, not just effect IDs) | PIT-tone approximation of 13 effect IDs |
| 6 | All assets derived from the tape at build time; no captured emulator blobs | `frame_l1.bin`, parts of `level_attrs.bin`, menu/hi-score screens still captured |
| 7 | Runs correctly on real-hardware-representative targets (XT-class + 386) | QEMU + 86Box `ibmxt` verified; real iron untested |
| 8 | Historical completeness: Kinnock easter egg, pause semantics, hi-score behaviour | **Done** — pause, hi-score, and the easter egg (`BATTY_KINNOCK=1`) |

Byte-exactness of the already-achieved core (criterion 3) is a floor,
not a ceiling to re-litigate: every workstream below must keep
`make parity-check` green, and milestone-level work runs
`parity-check-full` before merge.

## Where we are (July 2026)

Playable end-to-end in 1-player mode: title → menu → hi-score → all 15
levels → game-over → initials → title. All 15 levels pixel-perfect at
entry; ball motion, LAFFC brick collision, bat deflection, RNG walk,
enemy motion/steering/animation, bonus economy, scoring, and all
per-frame animations are byte-exact vs the Spectrum and gate-locked.
Rendering perf is concluded ("at its floor"); an optional 386 build
exists. The hard RE is done — `original/disasm/` (CityAceE) is a
complete, named, build-verified disassembly; the port workflow is
"read the disasm, port the routine, gate it."

What follows is ordered by value. Workstreams 1–4 make the port
*functionally complete*; 5–7 close the remaining *fidelity* gaps;
8–9 are infrastructure and polish.

---

## WS1 — Menu start semantics (small, do first)

**What:** Pressing 0/ENTER in the menu currently falls through the
attract chain (`src/main.cpp` ~L4006 "would start a game; advance for
now") instead of starting the selected game directly. Port the
original `main_menu` dispatch (see `routines/main_menu.asm` in the
disasm): 0/ENTER → start game in `selected_mode`, and make
`p1_dev`/`p2_dev` actually select the input source.

**Input-device adaptation (decision):** the original's device list
(Kempston / Sinclair / cursor joysticks) is Spectrum hardware — a
literal port makes no sense on PC. Adapt, don't transcribe: keep the
menu's A/B cycling and layout, but offer PC-meaningful devices —
**keyboard set 1**, **keyboard set 2** (a second key cluster, needed so
two players can share one keyboard in Double Play), and the **game-port
analog joystick** (port 0x201 / INT 15h AH=84h) where present, with a
graceful "not detected" fallback. Menu strings change accordingly; this
is a deliberate, documented deviation from pixel-parity on the menu
screen (gate the menu checkpoint against an updated reference, or keep
original strings and remap meaning — decide when implementing;
recommendation: update the strings, honesty beats fake parity here).

**Why first:** It's the entry gate for WS2/WS3 — mode wiring is
meaningless while mode selection can't start a game. Small, sharply
scoped, and it converts a documented stub into original behaviour.

**Exit:** menu 0/ENTER boots straight into the round-1 banner of the
chosen mode; a source-level gate pins the dispatch; attract-timeout
behaviour unchanged.

**Dispatch DONE (2026-08-09).** Key 0 now returns ST_LEVEL, which runs
`new_game_reset` and enters the round-1 banner; unrecognised keys re-poll
instead of leaving the menu; the attract timeout is untouched. Gated by
`test-menu-start`.

Two corrections to what this section said:

- It is **0 only**, not "0/ENTER". The original's tail is
  `LD A,$EF / CALL in_a_fe / AND $01 / RET NZ` on the $EFFE row and
  there is no ENTER in `main_menu` at all. ENTER is kept as the port's
  own attract-chain affordance, and that is load-bearing: `test-visual`
  walks title -> menu -> hi-score -> level by sending ENTER at each
  state, so making ENTER start a game would have turned its
  `state3_hiscore` checkpoint into a level capture — and since it diffs
  each checkpoint against its own reference, the failure would have read
  as a rendering regression on the wrong screen.
- "boots into the banner of the chosen mode" is only true for mode 1.
  Modes 2 and 3 are still inert, so picking one and pressing 0 starts a
  1-player game. Recorded here rather than papered over in the code.

Still open in WS1: `p1_dev`/`p2_dev` select nothing, and the
input-device adaptation below is undone.

## WS2 — 2 Players mode (alternating)

**What:** Classic turn-taking (research-confirmed: on the Spectrum,
2 PLAYERS alternates turns, unlike the C64 version). Per-player state
swap on life loss — score, lives, round, brick field — plus 1UP/2UP HUD
labels, the round-banner player digit (hardcoded `$01` today,
`src/main.cpp` ~L7302), and per-player hi-score entry. The object model
already carries `object_bat_2` ($9B3E) and the original's per-player
data structures are in the disasm (`preparation`/`record_table`
routines, `txt_player_N` strings).

**Approach decision:** follow the disasm's swap mechanism rather than
inventing a "two game structs" C-side abstraction — the original keeps
one live playfield and banks the waiting player's state; mirroring that
keeps parity reasoning (and any future oracle capture) tractable.

**Exit:** full 2P game playable; a transition gate proves the swap
(P1 dies with distinct score/round → P2's banked state restores
exactly); 1P gates untouched.

## WS3 — Double Play mode (simultaneous co-op)

**What:** The distinctive mode: both bats on screen at once, court
split into halves with a divider, each bat confined to its half, one
shared ball, shared pool of 3 lives. Needs: bat_2 render + input
(second device from `p2_dev`), per-bat margin clamps at the divider,
`handling_ball`'s bat-2 deflection branch, bonus ownership
(`object_bat_2+$14` paths already partially maintained in the port),
and the `LBC10_4` death-spark branch (shift 5 sparks by
`bat_2.x - bat_1.x`) that `parity-status.md` explicitly parked as
"out of scope (port is 1P)".

**Why after WS2:** WS2 builds the per-player state plumbing (HUD,
banner, hi-score) that Double Play reuses; Double Play then adds the
simultaneous-play mechanics on top.

**Exit:** co-op playable with two input devices; invariant gates
(bat confinement to halves, shared-lives accounting) + the spark-shift
branch gated; ideally one oracle capture of a Double Play frame for a
static checkpoint.

## WS4 — Game-flow transition gates

**What:** The top-listed test gap (`parity-gaps.md`): level-clear →
next-level, life-loss → respawn, game-over → initials entry, and the
level wrap after L15 (`round_number` keeps incrementing, `% 15` selects
the layout — matches `increment_round_number` $BBE0; gate it so it
stays true) are not end-to-end verified. The deterministic serial
harness (`BATTY_SERIAL_PROBE` + `WAITSERIAL`) and the replay/bake hooks
make these gateable without ZEsarUX.

**Why now:** WS1–3 all mutate exactly these flows (game start, player
swap on death, mode-specific game-over). Landing the transition gates
first-or-alongside means the mode work is born protected instead of
retro-gated.

**Exit:** a `test-flow-*` family in `parity-check-full` covering the
four transitions (plus mode-specific variants as WS2/WS3 land).

## WS5 — Sound: faithful beeper-queue port

**What:** Close the one openly approximate subsystem. The original has
**no music** — a 4-slot, 7-bytes-per-slot effect queue
(`routines/sound.asm`), frame-synced under IM 1, driving the 1-bit
beeper. The port currently maps the 13 effect IDs onto PIT-channel-2
tones with approximated envelopes (and `SND_MAGNET` as a blocking
sweep). Both machines are 1-bit toggles (Spectrum port $FE bit 4 vs PC
port 0x61 bit 1), so the faithful path is **direct-gate toggling with
PIT-calibrated delay loops**, porting each `play_sound_<id>` envelope's
period/step structure rather than re-synthesizing it. Prior art says
this is enough: the Warajevo DOS emulator reproduced Spectrum beeper
audio (incl. polyphony) on PC speaker; Batty's queued mono effects are
far simpler.

**Decision (recommended):** target *envelope-faithful*, not
cycle-exact — port the per-slot frequency walks and durations,
timed by PIT so it's CPU-speed independent; accept timbre differences
from bus/ISR jitter. Cycle-exact beeper emulation stays a non-goal
(below).

**Verification:** audio can't be pixel-diffed. Add a `BATTY_SOUND_LOG`
hook that records gate-toggle (period, duration) sequences per effect
and pin them source-level against tables derived from the disasm;
final judgement by ear vs ZEsarUX (`make run-original`).

**Exit:** all 13 effects (incl. a non-blocking magnet sweep) ported
from their `$C0F3+` handlers; toggle-log gates green; the
parity-gaps "sound envelopes are approximate" section rewritten to
"faithful, not cycle-exact".

## WS6 — Remaining gameplay-parity residuals

In priority order (all pre-scoped in `parity-gaps.md` / notes):

1. **Enemy vs bricks:** the original's bird runs `LAFFC` brick
   collision and exact `check_margins`; the port's bird doesn't
   (`enemy-movement.md` marks this the "next step"). Port both; gate
   with a seeded flight into a brick.
   *Brick half DONE (2026-08-09):* traced, ported and gated. The walk
   is `enemy_home_step` (LAA44, host-tested); the detection is
   `enemy_brick_reaction`, which reuses `laffc_sweep`/`laffc_bounce` and
   does the alien's half of `LAFFC_30` — keep the reflected dir, latch
   the snap point, leave the alien put, re-target off `flag_2` — without
   ever calling `brick_hit_resolve`. Gated by `test-enemy-brick-walk`,
   which needed a new `enemy_home` probe word because the reaction
   leaves no trace on screen.
   *`check_margins` DONE too (2026-08-09):* it is three clamps and
   nothing else — the port's reflect-and-re-aim was an invention, since
   `LAA7D` never looks at the alien's position. Gated by
   `test-enemy-margin-clamp`. **This item is closed**; the only residue
   is the original's 8-bit overflow in `check_right_margin`, reproduced
   and documented rather than fixed. See notes/enemy-movement.md.
2. **MAGNET catch for secondary balls:** the primary-ball stuck system
   spans ~32 sites; catching the unified secondaries means mirroring it
   per-ball. Deliberately deferred as substantial new code for the
   niche MAGNET+TRIPLE case — schedule it, don't ignore it.
3. **Seeded destroyed-cell mismatch:** `replay-l3-brick-flash-both`
   still shows the port and original destroying different *cells*
   (same count); the moving-object/destroyed-cell rows are INFO-only.
   Tighten the LAFFC neighbour-mask/cell-priority port until those rows
   can be promoted to required equality.
4. **Byte-exact enemy target gating:** blocked on a boot-phase-
   normalized comparison harness (test-infra, not port code). Do it if
   and when WS8's harness work makes it cheap.
   *Partly sidestepped (2026-08-09):* `test-enemy-descend` needed the
   phase and did not get the harness — it reads `enemy_repicks` out of
   the probe and asserts the implication (`turns == 0 -> target $10`,
   `turns == 1 -> target $29`) instead. Where a capture already says
   which outcome happened, no phase normalisation is needed.
   known-bugs #17.

**Exit:** items 1–3 gated; parity-gaps.md's "behavioral" section
reduced to the accepted-residuals list below.

## WS7 — Asset self-sufficiency (de-capture)

**What:** Replace the remaining captured-from-emulator blobs with
runtime generation from tape-derived data, so the tape is the *only*
original artifact:

1. **Frame ornament** — port the `spr_bord_horiz_*`/`spr_bord_left/
   right_*` compositor (disasm ~L6940) and drop `frame_l1.bin`
   (also kills the manual L6853 re-extract procedure in
   `modded-batty.md`).
2. **`level_attrs.bin` residue** — brick-body attrs are already
   computed; port the writer for frame-strip columns and pre-dimmed
   shadow attrs. This is also the root of the accepted 4px frame-step
   residual, which may fall out for free.
3. **Menu / hi-score screens** — `main_menu.bin`/`hi_score.bin` are
   full-screen snapshot dumps; the markup+font pipeline
   (`notes/menu.md`, `notes/encoding.md`) already decodes these —
   finish generating them.

**Unlock:** deterministic builds from `batty.tap` alone, per-level
frame variations the captured blob can't cover, and a much cleaner
distribution posture (WS9): the repo could stop shipping any
emulator-derived original imagery.

**Exit:** `assets/` contains only tape-extracted data; all visual
gates stay pixel-identical (they are the proof the generation is
right).

## WS8 — Infrastructure: CI, real hardware, load time

1. **Re-test KVM on hosted CI.** The "hosted runners have no KVM"
   conclusion predates GitHub enabling nested virtualization
   (`/dev/kvm`) on Linux runners (2024). If `qemu-system-i386 -accel
   kvm` works there, restore at least the QEMU smoke (the deterministic
   serial harness already removed the wall-clock flakiness). If not:
   self-hosted runner, or accept local-only (documented dead end for
   TCG stands — don't retry TCG).
2. **Real hardware smoke.** `make floppy` produces a bootable 1.44 MB
   image; one verified boot each on a real XT-class (8086 build) and a
   386 (`batty386.exe`) would retire criterion 7. Follow-ups already
   scoped in `performance.md`: batch the small asset `fread`s (floppy
   load time — the one untouched perf lever) and optionally a single
   auto-dispatching binary (runtime 386 detect) instead of two EXEs.
3. ~~**Boot-phase-normalized harness**~~ — for the PORT side this was
   already built and I had not noticed: `BATTY_REPLAY_COUNTER` pins
   `pit_frame_counter` at the aligned start (`pin_replay_frame_counter`,
   called from `enter_level`). Set it and every counter-phase decision is
   deterministic. Now used by the three enemy gates. What remains for a
   byte-exact comparison against the ORIGINAL is aligning the port's pin
   with the original's `counter_misc` at the same moment — that part is
   still open. known-bugs #17.

## WS9 — Polish, history, distribution

1. **Kinnock easter egg: DONE (2026-08-09).** Behind `BATTY_KINNOCK=1`,
   off by default; gated by `test-kinnock`, which parses the expected
   text out of `txt/txt_kinnock.asm` instead of copying it. Two
   surprises worth recording: it fires at the start of every LEVEL, not
   once per game (`print_kinnock` is called from the per-level entry
   `LB9E8_1`), and it is up for only ~0.3 s (`pause_short` with D=0 is
   ~1.05M T-states). See notes/shortcuts.md.
2. **Docs hygiene: pass done 2026-08-09.** Both named items fixed, plus
   three more the pass turned up:
   - `parity-gaps.md`'s priority list led with two struck-through CLOSED
     items, burying the two actually open. Reordered to OPEN first.
   - its "Some motion is approximate" section had six DONE entries under
     that heading, and the enemy bullet said "still approximate" one
     line above the list of four routines the port matches.
   - its closing paragraph had been edited in place three times and read
     "...end-to-end coverage. and the byte-exact frame-step oracle...".
   - `rng-model.md`'s "remaining work" paragraph outlived both its items
     by two months.
   - **known-bugs #16 asserted the opposite of the code** in the present
     tense, naming a routine deleted the same day.
   New tool from it: `scripts/notes_symbols.py` lists identifiers the
   notes cite and the tree no longer defines. A report, not a gate —
   see notes/testing.md for why.
3. **Distribution decision (user call, before any publicity):** Elite
   Systems still actively monetizes Batty (official iOS/macOS app) and
   historically denied archive distribution; the CityAceE disasm repo
   has no license. The repo currently vendors the tape, TZX, and
   captured screens. Before making the repo public or shipping
   binaries, decide between: (a) keep private (status quo, zero risk),
   (b) publish code only, with originals user-supplied at build time
   (WS7 makes this clean — the build already extracts assets from the
   tape), or (c) ask Elite. Until decided: don't distribute built
   images containing original assets.

---

## Suggested sequencing

```
WS1 menu start ──► WS2 2-players ──► WS3 double play
      │                  ▲
      └── WS4 flow gates ┘   (land alongside; gates protect the mode work)

WS5 sound          — independent, any time
WS6 parity residuals — independent, any time (item 1 first)
WS7 de-capture     — independent; prerequisite for WS9.3(b)
WS8 infra          — KVM re-test is a one-hour experiment; do early
WS9 polish         — docs pass early; distribution decision before publicity
```

A reasonable milestone cut:
- **M1 "feature-complete":** WS1 + WS4 + WS2 + WS3 → all three modes,
  transitions gated.
- **M2 "faithful":** WS5 + WS6.1–3 → sound + last behavioural parity.
- **M3 "self-contained & proven":** WS7 + WS8 → tape-only assets, CI
  restored, real-hardware boot.

## Accepted residuals (explicit non-goals — do not reopen)

- **Cycle-exact sound timbre** (Z80-clock-timed duty cycles, ISR
  jitter) — envelope-faithful (WS5) is the target.
- **The 4px L3 frame-step brick-edge nuance** — unless it falls out of
  WS7.2 for free.
- **Metal-shimmer 1-frame phase offset** — root-caused, cosmetic,
  both sides render out of phase.
- **Rocket-tally pacing** — the original's `pause_short` busy-wait is
  Z80-clock-bound; 1 brick/PIT-tick is the port's documented answer.
- **Big-bat resize as a literal bit-gated state machine** — visually
  matched; revisit only if a defect surfaces.
- **QEMU-under-TCG in CI** — calibrated dead end; KVM or nothing.

## Standing constraints (hard-won — see notes/lessons.md before touching)

Every workstream inherits: `make parity-check` green per change,
`parity-check-full` before milestones; disasm is the oracle (never
"fix" it — patch via `build_modded_batty.py` PATCHES); moving objects
blit pixels only (never attrs); no invented input short-circuits in
blocking sequences; pin `BATTY_REPLAY_COUNTER` for anything gated on
frame-counter low bits; ZEsarUX gates stay serial; the capture pipeline
(`notes/modded-batty.md`) is settled — don't redesign it.
