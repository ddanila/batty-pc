# Plan — gameplay recreation

The menu phases are done (TITLE → MENU → HISCORE, all checkpoints
pixel-identical). This is the plan for porting the actual game.

Original [`plan.md`](plan.md) covered the menu phase and is now
historical reference.

> **Technical debt warning**: the L1 static render (state4_level1)
> is pixel-identical via shipped-pixel-data shortcuts. Before each
> phase touches a shortcut's area, the proper port must land first.
> See [`shortcuts.md`](shortcuts.md) for the full list and priority
> matrix. In particular: the brick compositor (shortcut #1) is a
> blocker for Phase E gameplay; the per-level frame / bat /
> dynamic scores (#2, #4) need repaying as we move levels and
> introduce motion.

## Phase A — Sprite system trace

Goal: understand how anything moving gets to VRAM.

The disassembly's single biggest unknown is the routine that paints
the runtime sprite cache at `0xE400..0xF1FF` into screen RAM each
frame, plus the `0xF200` shift table the cache is keyed against.
Until this lands, nothing dynamic can be reproduced.

- [ ] **A1.** Find the blitter via ZEsarUX watchpoint on writes to
      `0x4000..0x57FF` at snap3's PC. The PC at the moment the
      watchpoint fires is inside the blitter.
- [ ] **A2.** Decode the sprite cache at `0xE400..0xF1FF` (14 pages
      = 3.5 KB). Likely pre-shifted variants of each base sprite, one
      per fractional X offset.
- [ ] **A3.** Decode the shift table at `0xF200`. Almost certainly an
      index `(sprite_id, x_subpixel) → cache_offset` or similar.
- [ ] **A4.** Extract every distinct sprite as a PNG; have the user
      visually verify before locking in the format.
- [ ] **A5.** Write `notes/sprites.md`.

**Exit:** every sprite in snap3 traceable to a RAM source + shift
index. The blitter's pseudocode in the notes file.

## Phase B — Static level-1 render

Goal: reproduce snap3 (level 1, just started) pixel-identical in C.

- [ ] **B1.** Extract level / brick layout from snap3 RAM (location
      TBD; trace from blitter callsites that draw bricks).
- [ ] **B2.** Extract bat + ball + status overlays (lives, score,
      level number).
- [ ] **B3.** Port a sprite blitter to C; render snap3 statically.
- [ ] **B4.** Add `state4_level1` checkpoint to `scripts/test_visual.py`.

**Exit:** `state4_level1` PASS pixel-identical (49 152 px).

## Phase C — Replay infrastructure

Goal: deterministic input feed so gameplay regression-tests work.

Decision: build *before* Phase D so every later moving element gets
regression-tested for free. Without this we'd be back to eyeballing
PNGs the way the early hi-score work did, except now the screens are
in motion.

- [ ] **C1.** Define `replay.txt` format: `<frame_N> <action>` per
      line, where action is `key_down K`, `key_up K`, or `assert
      snapshot_NN`.
- [ ] **C2.** `make capture-replay`: drive ZEsarUX via ZRCP from a
      replay file; snapshot RAM + screen at every `assert` checkpoint.
      Produces `replays/<name>/frame_NN.{scr,ram}` files.
- [ ] **C3.** Extend `scripts/test_visual.py` to consume a replay
      against our DOS build: drive QEMU via `sendkey` timed to the C
      frame counter (see Phase D); pixel-diff at every checkpoint.
- [ ] **C4.** First replay: 10 frames of "do nothing" on the menu —
      proves clock-sync works before any gameplay-specific code lands.

**Exit:** "do nothing for 10 frames" replay produces pixel-identical
frames between ZX and our DOS recreation.

## Phase D — Game loop skeleton + 50 Hz timing

Goal: run an actual frame-by-frame loop at the right rate.

Decision: reprogram the PIT to 50 Hz, chain the BIOS INT 8 handler.
~30 lines of inline asm in `src/main.c`. The alternative (18.2 Hz
BIOS ticks) would run everything ~2.7× slower than the original and
make replays uncomparable.

- [ ] **D1.** Find main game loop entry from snap3 PC trace.
- [ ] **D2.** Identify per-frame routine list (input → physics →
      render → wait).
- [ ] **D3.** PIT timer 0 reprogrammed to 50 Hz divisor 23864; chain
      original BIOS INT 8 every (50/18.2) ≈ 3rd tick so the BIOS
      clock stays correct.
- [ ] **D4.** C-side `frame_counter` exposed; `wait_frame()` blocks
      on the PIT-driven flag.
- [ ] **D5.** With nothing animating yet, the "do nothing" replay
      from Phase C still passes.

**Exit:** game runs at 50 Hz wall-clock; snap3-equivalent capture
still pixel-identical at frame 0; Phase C's "do nothing" replay
still PASS.

## Phase E — Bat (first moving element)

- [ ] **E1.** Find bat position byte + render callsite from snap3
      RAM walk + blitter trace.
- [ ] **E2.** Find input → bat movement code (look for `LD A, 0xnn`
      followed by `sub_97a7h` for the keys mapped to bat L/R).
- [ ] **E3.** Port to C; capture + replay "press right for 10
      frames" — pixel-identical against ZX per frame.

**Exit:** `replays/bat-right-10/` PASS on both sides.

## Phase F — Ball physics

- [ ] **F1.** Find ball state (position, velocity, on-bat flag).
- [ ] **F2.** Find collision routines (wall, bat, brick) — the
      physics core. Watchpoint on the ball position bytes to find
      every writer.
- [ ] **F3.** Port; replay "ball on bat → release → first wall
      bounce" pixel-identical per frame.

**Exit:** `replays/first-bounce/` PASS on both sides.

## Phase G — Bricks + scoring

- [ ] Collision -> brick destruction -> score increment -> per-brick
      colour / score table.

## Phase H — Power-ups, levels 2+

- [ ] Power-up state machine (sticky bat, multi-ball, expand, etc.)
      Needs new ZX snapshots — none of our current 3 captures it.
- [ ] Level progression / level data format.

## Phase I — Game over / hi-score entry

- [ ] Game-over flow.
- [ ] Name-entry on a new high score — the snap1 hi-score table
      becomes actually achievable from gameplay.

## Phase J — Sound

Deferred to last. Not on the critical path for visual parity, and
will need a separate Adlib/PC-speaker decision (the original is
ZX-beeper). Adlib emulator already in `~/fun/adlib-rng` as reference.

## Risks / unknowns to call out now

- **Sprite cache format at `0xE400..0xF1FF`.** Biggest unknown. The
  shift table at `0xF200` is the key. Until A1-A3 land we can't even
  estimate the rest of gameplay's complexity.
- **50 Hz timing in DOS.** PIT reprogramming is well-trodden but
  fiddly; we must chain BIOS INT 8 correctly or the system clock
  drifts. Test very early (Phase D).
- **Power-up state machine.** Not in any current snapshot. We'll
  need to play far enough into the original to capture it (probably
  several runs through level 1 first).
- **Floppy-image size budget.** The current floppy is 1.44 MB
  minimal MS-DOS 4.0; assets are ~10 KB. Should be fine through
  Phase I; double-check after Phase H.

## Tooling we need to build

- `scripts/capture_replay.py`: drives ZEsarUX via ZRCP from a replay
  file, snapshots at each checkpoint.
- `scripts/test_visual.py` replay-consumer mode (Phase C3).
- Frame-counter / PIT helpers in `src/timing.h` (Phase D3).
- Optional: `scripts/decode_sprite_cache.py` once A2 lands.

## Snapshots we'll need

- Multiple consecutive gameplay frames (for Phase B + C calibration).
- "Ball just hit bat" mid-trajectory.
- A power-up sprite active.
- Game-over screen.
- Hi-score entry flow (cursor on letter grid).
