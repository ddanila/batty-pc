# Plan — main menu Phase B and beyond

The hi-score screen was easy because it's static. The main menu isn't:

1. It's **animated** — at least the prompt-style elements probably cycle
   on a frame counter (the `0x8D46` counter ticks every IM-1 fire).
2. It's **interactive** — `A` and `B` toggle a control-config state
   visible on screen (e.g. "KEYBOARD / KEMPSTON / CURSOR").
3. The snap2 PPM is a single frame; whatever happened to be invisible at
   that exact moment is missing from `MAINMENU.BIN`. The COPYRIGHT line
   we found at `0x95EB` was off-screen / not painted yet at snap2.

## Already confirmed (initial probe)

- **Markup encoding is reused.** snap2's RAM at `0x8FD1..` is still the
  stale hi-score buffer byte-for-byte; the menu's render path uses a
  *different* source buffer.
- **Menu strings live in the blob, not dynamically composed.** Hits
  found in snap2 at `0x95Ax..0x9620`: DOUBLE PLAY, START GAME,
  KEYBOARD, KEMPSTON, CURSOR, COPYRIGHT HIT PAK 1987, BATTY (the title
  proper).
- **Marker byte / 8 = X column.** The hi-score `0x38 / 0x50 / 0x58`
  markers + the menu's new `0x60` / `0x68` all fit `col = marker / 8`:
  0x30→6, 0x38→7, 0x40→8, 0x50→10, 0x58→11, 0x60→12, 0x68→13.
  This makes our `x_for_marker` switch unnecessary and any future
  marker auto-decodable.

So we go data-driven for the menu the same way we did for hi-score, but
have to find the dynamic bits too.

## Stage 1 — find the menu's markup buffer + walk it

The menu strings live at `0x95A0..~0x9620` in the blob, but we still
need the *records* that wrap them (`<marker> <Y> <attr> <count>
<payload>`). Probe outward from the known string hits.

- [ ] Dump `0x9580..0x9650` from snap2 RAM and parse with our existing
      record schema, treating any `0x?8 / 0x?0` byte with sensible
      `Y / attr / count` following as a record start.
- [ ] Refactor `render_header_like` and `x_for_marker` to use the
      `col = marker / 8` formula. One code path handles all of
      0x30/0x38/0x40/0x50/0x58/0x60/0x68/…
- [ ] Bundle the menu's record range as `assets/main_menu_markup.bin`.
      Add a new state in `main.c` cycle: state 4 = `demo_menu()` =
      render menu from this markup with the same renderer hi-score
      uses.
- [ ] Test: snap2 may still mismatch px in dynamic regions; accept and
      triage by mismatch *location* — those are the animated bits.

## Stage 2 — find the dynamic / animated elements

The menu has *some* element that changes per frame. To find it:

- [ ] Take **two or three snap2-like snapshots** spaced a few seconds
      apart while sitting on the main menu untouched. Diff their RAMs
      with `analyze_snapshots.py`; anything that changes is candidate
      animation state (a flash counter, a sprite frame index, a cycling
      colour byte).
- [ ] Diff each snapshot's VRAM with our reverse-search
      (`vram_back_to_ram.py`) — finds which RAM bytes are being painted
      onto screen each frame. Those are the live render sources.
- [ ] Cross-reference the changing bytes against the disassembly: any
      `LD (xxxx), A` instruction whose target is in the diff set is the
      animator. Likely candidates: `0x8D46` (frame counter, already
      identified) drives a `frame_counter & N`-style cycle.

## Stage 3 — find the A / B input handler

`A` is bit 0 of keyboard row `0xFD`, `B` is bit 4 of row `0x7F`.
`sub_97a7h` (the keyboard half-row read helper) is called with `A`
preloaded with the row mask, so we can grep for callers.

- [ ] `grep -nE 'ld a,0fdh|ld a,07fh' original/blocks/03_main.asm` —
      finds every `LD A,row` immediately before a `call sub_97a7h`.
      Each hit is a key-poll location.
- [ ] For each hit, look at the surrounding routine: which state byte
      does it modify? That byte is the control-config variable
      ("which input device the player uses"). Probably 1-byte at a
      fixed address; values 0 / 1 / 2 / … for the available options.
- [ ] Confirm by snapshotting **after pressing A**, then **after
      pressing B**, and diffing against the idle menu snapshot. The
      byte that flips is the config state.

## Stage 4 — render the menu with state, in C

Once stages 1–3 land:

- [ ] C side gets an `enum input_device { KEYBOARD, KEMPSTON, … }`
      mirroring the original byte values.
- [ ] The render path either:
      - composes the same way the original does (we port `sub_b61ch`
        — the live-state → markup composer at `0xBF00 → 0x8FD1`), or
      - has a hand-written render call that draws the current option
        directly using our `draw_glyph`. Composer port is cleaner long
        term; direct call is faster to ship.
- [ ] A and B in the C `getch` loop modify the `current_device` var;
      next frame the renderer reflects it.

## Stage 5 — make the test catch the dynamic stuff

A single-frame PPM diff catches static content but is brittle for
animations. Two upgrades:

- [ ] **Multi-frame captures.** Boot, sleep, screendump, sleep,
      screendump, … N times. For each frame, the expected is taken
      from `make snapshot` (the ZEsarUX side) at the matching
      logical tick. Requires deterministic frame timing on both sides.
- [ ] **State-driven captures.** `make test` advances the state
      machine through `(no input → press A → press B → start)` and
      diffs each post-action frame against a corresponding ZEsarUX
      snapshot taken under the same input sequence. Doesn't need
      frame-perfect timing — it tests *what the state is*, not *what
      tick we're on*.

## Stage 6 — replay file (groundwork for gameplay tests)

This is the foundation everything dynamic depends on:

- [ ] Define `replay.txt` format: `<tick_or_frame> <action>` per line,
      where action is a key press / hold / release. Same file feeds
      both the ZEsarUX run (via ZRCP) and the QEMU run (via `sendkey`).
- [ ] Add `make capture-replay` that drives ZEsarUX from a replay file,
      snapshotting RAM + screen at every checkpoint specified in the
      file. Produces `replays/<name>/frame_NN.scr` files.
- [ ] Extend `make test` to consume a replay: run the same replay
      against our DOS recreation, screendump at the same checkpoints,
      pixel-diff per frame. Now we can regression-test any gameplay
      sequence we can record once in the original.

## Notes for future sessions

- The disassembly's biggest remaining unknown is the **screen sprite
  blitter** — the routine that paints the runtime sprite cache at
  `0xE400..0xF1FF` into VRAM each frame, using the `0xF200` shift
  table. It must touch both. Find it via the existing trace tool plus
  watchpoint trick: in ZEsarUX, break on writes to `0x4000..0x57FF`
  while the playfield is rendering (snapshot 3 PC). The PC at that
  moment is inside the blitter.
- `sub_9231h` (the menu setup that paints the red frame) is also
  unanalysed; once we trace it we can ditch our hand-written
  `draw_red_frame` and use the same per-screen markup the original
  does for the frame.
- The 14-page region at `0xE400..0xF1FF` looked like "distorted
  playfield" — likely a custom de-interleave or pre-shifted scroll
  cache. Once we find the blitter the format will be obvious.
