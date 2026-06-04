# batty-pc

MS-DOS / 8086 re-creation of **Batty** (Elite Software / Hit-Pak, 1987,
ZX Spectrum 48K). VGA mode 13h (320×200×256), 1:1 pixel-faithful inside
the 256×192 playfield, border around it. Built with Open Watcom v2,
boots in QEMU.

## Status

The game is playable end-to-end: title → menu → hi-score teaser →
all 15 levels → game-over → 3-letter initials entry → back to title.
`make test` headlessly diffs five checkpoints against ZX GT snapshots:

```
PASS state1_title       (pixel-identical)
PASS state2_menu        (pixel-identical)
PASS state3_hiscore     (pixel-identical)
PASS state4_level1      (pixel-identical, default = L1; FAIL-gated since iter-34)
PASS state5_bat_band    (pixel-identical, FAIL-gated)
```

**Gameplay frame parity.** Beyond the static checkpoints, the primary
ball's *motion and brick collision are byte-exact with the Spectrum* —
the ported `handling_ball` (exact 64-direction q8.8 motion) + `LAFFC`
brick collision match the original's ball object (x / fraction / y /
direction, hit cell, bounce axis) frame-for-frame over level 3's first
~150 frames (dozens of bounces). Gated by `make test-laffc-ball-frame1`;
`make parity-check` runs the static + collision gates together. Verified
against ZEsarUX via the frame-step harness (`notes/replay-harness.md`,
`notes/laffc-decode.md`). The one remaining frame-by-frame difference is
the cosmetic metal-brick light shimmer (an animation the static GT
doesn't capture and both sides render out of phase); it has no gameplay
effect. `BATTY_LEGACY_COLLISION=1` reverts to the older approximate
collision.

### Per-level pixel parity (`BATTY_LEVEL=N make test`)

`BATTY_LEVEL=N` (N = 1..15) re-runs `state4` against
`build/level_gt/level_NN.scr`. **All 15 levels are pixel-perfect**.

| Level | Diff | What's left                                         |
|-------|------|-----------------------------------------------------|
| L1    | **0**  | PASS                                              |
| L2    | **0**  | PASS                                              |
| L3    | **0**  | PASS                                              |
| L4    | **0**  | PASS                                              |
| L5    | **0**  | PASS                                              |
| L6    | **0**  | PASS                                              |
| L7    | **0**  | PASS                                              |
| L8    | **0**  | PASS                                              |
| L9    | **0**  | PASS                                              |
| L10   | **0**  | PASS                                              |
| L11   | **0**  | PASS                                              |
| L12   | **0**  | PASS                                             |
| L13   | **0**  | PASS                                              |
| L14   | **0**  | PASS                                              |
| L15   | **0**  | PASS                                              |

The resolved parity history is documented in
[`notes/per-level-profile.md`](notes/per-level-profile.md).

| Stage                                       | State |
|---------------------------------------------|-------|
| Mode-13h toolchain + QEMU                   | ✓ |
| ZX `.SCR` decoder + GT capture pipeline      | ✓ |
| TAP parser / block extraction / disasm trace | ✓ |
| Init chain, menu refresh, markup encoding    | ✓ `notes/init.md`, `notes/menu.md`, `notes/encoding.md` |
| Title / menu / hi-score screens             | ✓ pixel-identical |
| ZX-style `scr_buff` + `attr_buff` pipeline   | ✓ `notes/blitter-port.md` |
| Bricks: 1-hit / multi-hit / undestructible   | ✓ ported `print_briks` semantics + damage dim |
| 50 Hz game loop (PIT IRQ + INT 9 polling)    | ✓ |
| Bat: input, resize ramp, side-fillers        | ✓ |
| Ball: walls, bricks, 5-zone bat deflection   | ✓ |
| Multi-ball secondary (BIG_BALL aware)        | ✓ |
| Bonus drops + 10 bonus types                 | ✓ LIFE / SLOW / BIG_BAT / BIG_BALL / KILL_ALIENS / CATCH / ROCKET / SCORE_5K / LASER / MULTI_BALL |
| Laser: gun bat + 4-frame fire anim + bullet  | ✓ + bullet-impact blast |
| Rocket: flying sprite, chain destruction     | ✓ 2-frame flame, brick flash on each hit |
| Aliens: UFO / bird / 5-frame blast          | ✓ + bombs drop |
| L4 spark enemy (`handling_spark` port)       | ✓ 5-frame decay |
| HUD: live score / hi-score / power-up chips  | ✓ chips blink in last second of timed effects |
| High-score persistence with 3-letter name    | ✓ `HISCORE.DAT` (v2 = 7 bytes; v1 = 4 bytes auto-defaults to `AAA`) |
| Pause banner (P toggles, ENTER dismisses)    | ✓ |
| Magnets (level decorations + ON/OFF blink)   | ✓ `render_magnets` (`notes/per-level-profile.md`) |
| Headless visual regression (5 checkpoints + per-level) | ✓ `make test`, `BATTY_LEVEL=N make test` (`notes/testing.md`) |
| Frame-step parity gate (port vs ZEsarUX, frame-by-frame) | ✓ `make capture-timeline-both` (`notes/replay-harness.md`) |
| Real `handling_ball` 64-direction q8.8 motion | ✓ exact `dir_to_dxdy` (LAD69 X/Y cross + fraction); byte-exact vs Spectrum (probed) |
| `LAFFC` brick collision (cell / axis / position) | ✓ **default** (primary ball); byte-exact vs Spectrum over L3's 150-frame trajectory, `make test-laffc-ball-frame1`; `BATTY_LEGACY_COLLISION=1` reverts to `brick_collision` (`notes/laffc-decode.md`) |
| Brick-hit shimmer frame-exactness            | open — collision is byte-exact; the cyan hit-shimmer pixel pattern still differs (cosmetic, shared by both collision paths) |
| 2-player mode (`game_mode == 2`)             | open — selectable from menu, not wired into run loop |
| Frame ornament from `spr_bord_*` primitives  | open — currently bundles a captured `frame_l1.bin` |

## Quick build / run

```sh
brew install mtools qemu
make floppy   # builds build/batty.exe + assets, packs build/batty.img
make run      # boots in QEMU
make run-86box # boots in 86Box as an IBM XT with an ISA VGA card
make test     # headless visual-regression (boots, drives keys, pixel-diffs)
make parity-check # test + byte-exact LAFFC collision gate (gameplay frame parity)
```

`make run-86box` defaults to the locally built SDL frontend at
`/home/ddanila/.local/86box/bin/86Box` and writes its VM config under
`build/86box/`. It uses 86Box's `ibmxt` machine, `vga` video card, and
mounts `build/batty.img` as drive A:. ROMs are expected at
`/home/ddanila/fun/86Box-roms` from the upstream 86Box ROM checkout.
Override `BOX86_BIN`, `BOX86_ROMPATH`, `BOX86_MACHINE`,
`BOX86_GFXCARD`, or `BOX86_FDD_TYPE` when testing another local
emulator install or ROM set.

The remaining Makefile targets (`make regions`, `make candidates`,
`make run-original`, `make snapshot`) drive RE tooling against the
original tape and additionally need `z80dasm` + `srecord`
(`brew install z80dasm srecord`). They're only needed when rediscovering
something not already captured in `original/disasm/` or the `notes/`
— see the **Approach** section below for when that's worth doing.

## Repo layout

```
src/main.c          The entire DOS implementation (Open Watcom v2, 16-bit)
notes/              Project knowledge (read these first)
original/
  batty.tap         Original tape image
  disasm/           Authoritative reverse-engineering reference
                    (full annotated disassembly — consult before
                     touching anything semantically tricky)
  blocks/           TAP-decoded per-block dumps
assets/             Binary assets extracted from the tape and consumed
                    at build time (font, sprites, level data, etc.)
scripts/            Python tooling — asset extraction + RE + test harness
build/              Build outputs (gitignored)
vendor/             Vendored toolchain bits (Open Watcom v2, MS-DOS floppy)
tools/              Build-from-source dependencies (ZEsarUX submodule)
```

## Toolchain

Everything the build needs lives in this repo. Host tools:
`mtools` and `qemu-system-i386` (`brew install mtools qemu` on
macOS). `z80dasm` and `srecord` are only needed for the RE-only
targets — install them on demand.

### Vendored — ready to use

- `vendor/openwatcom-v2/current-build-<date>/` — trimmed Open Watcom v2
  daily snapshot (`wcc`, `wlink`, `h/`, `lib286/dos/clibs.lib`), per-host
  binaries for macOS arm64 / x64 and Linux x64. Refresh with
  `scripts/vendor_openwatcom.sh` (needs `gh`).
- `vendor/msdos/floppy-minimal.img` — 1.44 MB MS-DOS 4.0 boot floppy.
  Refresh with `scripts/vendor_msdos.sh` (needs `gh`).

### Submodule — build once

- `tools/zesarux/` — ZEsarUX source (only needed for the RE targets:
  `make run-original`, `make snapshot`, the `scripts/capture_levels*.py`
  / `scripts/trace_blitter.py` / `scripts/run_original_cheat.py`
  tools). Bring it up with:

  ```sh
  git submodule update --init tools/zesarux
  cd tools/zesarux/src && ./configure --enable-sdl2 && make
  ```

  The Makefile and scripts default to `tools/zesarux/src/zesarux`;
  override with `ZESARUX=/path/to/zesarux` if you have it installed
  elsewhere.

  On Linux, `make run-original` defaults to SDL video/audio and doubles
  ZEsarUX's normal Spectrum window scale:

  ```sh
  make run-original
  ```

  This is equivalent to:

  ```sh
  make run-original ZESARUX_VO=sdl ZESARUX_AO=sdl ZESARUX_RUN_OPTS="--zoom 4"
  ```

  Check the drivers compiled into your local ZEsarUX before choosing
  other overrides:

  ```sh
  tools/zesarux/src/zesarux --help | sed -n '/--vo driver/,/--version/p'
  ```

  On a minimal Linux build, this may list only `fbdev stdout simpletext
  null` video and `dsp onebitspeaker null` audio. `onebitspeaker` needs
  PC speaker I/O permissions; use `null` for smoke tests and RE
  automation. If `xwindows` or `sdl` is not listed, the current binary
  cannot open a window with that driver. Install the matching development
  headers, rebuild ZEsarUX, and then select the enabled drivers
  explicitly:

  ```sh
  # X11 window + ALSA audio, if both drivers are listed by --help:
  make run-original ZESARUX_VO=xwindows ZESARUX_AO=alsa

  # SDL window + PulseAudio/PipeWire; this is the tested desktop path
  # on the current Linux workstation:
  make run-original ZESARUX_VO=sdl ZESARUX_AO=pulse

  # Headless smoke test / RE automation:
  make run-original ZESARUX_VO= ZESARUX_AO=null
  ```

## Approach

**The hard RE is done.** `original/disasm/batty.asm` (from
[`CityAceE/Batty`](https://github.com/CityAceE/Batty), vendored as
a submodule at `original/disasm/`) is a complete, named, annotated
Z80 disassembly — every routine that matters already has a label and
a comment explaining what it does. Treat it as the source of truth.

Workflow for a new port:

1. Find the routine in `original/disasm/batty.asm` by its named label
   (`handling_ball`, `print_briks`, `enemy_prepare`, …).
2. Port it into `src/main.c`, citing the routine name in the code
   comment so the lineage is searchable.
3. Add a one-paragraph entry in `notes/<topic>.md` if the routine
   exposes a non-obvious invariant (encoding, RAM layout, timing
   quirk). Otherwise skip — the disasm + code comment is enough.

You should not need to disassemble anything from `original/batty.tap`
yourself. The RE-only tooling (`make regions`, `make candidates`,
`make run-original`, `make snapshot`, the `scripts/trace_*.py` /
`scripts/scan_*.py` helpers) only earns its keep when:

- chasing **dynamic / SMC behaviour** the static disasm can't resolve
  (computed jumps, self-modifying loops) — use ZRCP single-step
  against ZEsarUX, or
- **rediscovering a routine** that isn't yet named in
  `original/disasm/` (rare — flag it back upstream if so).

Otherwise: read the disasm, port the routine, move on.

## Original assets

`original/batty.tap`, `original/BATTY.TZX`, `original/Batty.scr`,
`original/Batty.txt` are © Elite Systems / Hit-Pak, 1987. Vendored
for personal reverse-engineering reference only.

## License

The MS-DOS recreation (everything under `src/`, `scripts/`, `notes/`,
the `Makefile`, etc.) is MIT.
