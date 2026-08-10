# batty-pc

DOS re-creation of **Batty** (Elite Software / Hit-Pak, 1987, ZX Spectrum
48K). VGA mode 13h (320x200x256), 1:1 pixel-faithful inside the 256x192
playfield, border around it. Built with Open Watcom v2, boots in QEMU.

Targets a **386 in 32-bit protected mode**: `batty.exe` is an LE image loaded
by the DOS32A extender, which ships alongside it on the floppy. The flat
memory model means no segmentation and no 64 KB limits — see
[`notes/toolchain.md`](notes/toolchain.md).

## Status

Playable end-to-end in all three modes — 1 Player, 2 Players and Double Play:
title → menu → hi-score teaser → all 15 levels → game-over → 3-letter
initials entry → back to title.

**Static parity.** `make test` headlessly diffs five checkpoints against ZX
ground-truth snapshots, all pixel-identical: title, menu, hi-score, level
entry, and the bat band as its own ROI. `BATTY_LEVEL=N make test` re-runs the
level checkpoint for N = 1..15, and **all 15 levels are pixel-perfect** —
FAIL-gated for every level by `make test-levels-sweep`.

**Gameplay frame parity.** The ball's motion and brick collision are
*byte-exact with the Spectrum*: the ported `handling_ball` (exact 64-direction
q8.8 motion) plus the `LAFFC` brick collision match the original's ball object
— x, fraction, y, direction, hit cell, bounce axis — frame for frame over
level 3's first 150 frames and dozens of bounces, verified against ZEsarUX
through the frame-step harness. `BATTY_LEGACY_COLLISION=1` reverts to the
older approximate collision. Bat deflection, the RNG walk, enemy motion and
animation, the bonus economy, scoring and every per-frame animation are
byte-exact and gate-locked too.

109 gates hold all of it. Full detail:
[`notes/parity-status.md`](notes/parity-status.md), open gaps in
[`notes/parity-gaps.md`](notes/parity-gaps.md), the roadmap in
[`PLAN.md`](PLAN.md).

Everything the game does is implemented: bricks (1-hit, multi-hit and
undestructible), the 50 Hz game loop, bat resize and side fillers, multi-ball,
all 10 bonus types, the laser, the rocket clear, UFO and bird enemies with
bombs, the L4 spark enemy, magnets, the HUD, pause, high-score persistence
(`HISCORE.DAT`), and the Kinnock easter egg. Every loaded asset is generated
from the tape at build time (`test-asset-provenance`).

## Quick build / run

```sh
brew install mtools qemu
make floppy       # builds build/batty.exe + assets, packs build/batty.img
make run          # boots in QEMU
make run-dosbox   # boots in DOSBox-X (brew install dosbox-x)
make run-86box    # boots in 86Box (set BOX86_MACHINE to a 386-class machine)
make test         # headless visual regression
make test-fast    # host suites + source gates, no emulator, seconds
make parity-check # test + the byte-exact collision gate
```

`make run-dosbox` BOOTS the image rather than mounting it and running
`BATTY.EXE`. That matters: the build-time `BATTY_*` switches live in the
floppy's `AUTOEXEC.BAT`, and DOSBox-X's own shell never runs a mounted image's
AUTOEXEC — so the shorter invocation is the one that silently drops every
switch you set. It ignores your personal `dosbox-x.conf` by default
(`DOSBOX_CONF` overrides) so a conf pinning a 286 or CGA cannot fail in a way
that looks like the port's fault. Cycles default to `max`; the game paces
itself off its own 50 Hz PIT programming, so cycles buy headroom inside a
frame rather than speed. `DOSBOX_CYCLES=12000` is roughly a 386DX-40 if you
want period-representative timing.

`make run-86box` writes its VM config under `build/86box/`, uses 86Box's `vga`
card and mounts `build/batty.img` as drive A:. **`BOX86_MACHINE` defaults to
`ibmxt`, which cannot run this build** — it has been 386-only protected mode
since 2026-08-07. Pass a 386-class machine id from your own 86Box
(`86Box --help` lists them); the default is left visibly wrong rather than
replaced with an id nobody here can verify. `BOX86_BIN`, `BOX86_ROMPATH`,
`BOX86_MACHINE`, `BOX86_GFXCARD` and `BOX86_FDD_TYPE` all override, and the
defaults point at a Linux workstation's local build and ROM checkout.

The remaining targets (`make regions`, `make candidates`,
`make run-original`, `make snapshot`) drive RE tooling against the original
tape and additionally need `z80dasm` + `srecord`
(`brew install z80dasm srecord`). They are only needed when rediscovering
something not already captured in `original/disasm/` or `notes/` — see
**Approach**.

## Repo layout

```
src/                The DOS implementation — 15 separately compiled modules
                    (Open Watcom v2, 386 32-bit protected mode)
tests/              Host-native test suites (compile the real sources)
notes/              Project knowledge — read these first
original/
  batty.tap         Original tape image
  disasm/           Authoritative RE reference: the full annotated
                    disassembly (submodule; consult before touching
                    anything semantically tricky)
  blocks/           TAP-decoded per-block dumps
assets/             Binary assets extracted from the tape and consumed at
                    build time (font, sprites, level data, ...)
replays/            Replay / seeded-scenario specs for the parity harness
scripts/            Python tooling — asset extraction, RE, test harness
build/              Build outputs (gitignored)
vendor/             Vendored toolchain (Open Watcom v2, MS-DOS floppy)
tools/              Build-from-source dependencies (ZEsarUX submodule)
```

## Toolchain

Everything the build needs is in the repo. Host tools: `mtools` and
`qemu-system-i386` (`brew install mtools qemu`). `z80dasm` and `srecord` are
only needed for the RE-only targets.

**Vendored, ready to use.** `vendor/openwatcom-v2/current-build-<date>/` is a
trimmed Open Watcom v2 daily snapshot (`wcc386`, `wpp386`, `wlink`, `wdis`,
`h/`, the 32-bit DOS libraries and the DOS32A extender), with per-host
binaries for macOS arm64 / x64 and Linux x64; refresh with
`scripts/vendor_openwatcom.sh`. `vendor/msdos/floppy-minimal.img` is a 1.44 MB
MS-DOS 4.0 boot floppy; refresh with `scripts/vendor_msdos.sh`. Both need
`gh`.

**Submodule, build once.** `tools/zesarux/` is only needed for the RE targets
(`make run-original`, `make snapshot`, and the
`scripts/capture_levels*.py` / `scripts/trace_blitter.py` /
`scripts/run_original_cheat.py` tools):

```sh
git submodule update --init tools/zesarux
cd tools/zesarux/src && ./configure --enable-sdl2 && make
```

The Makefile defaults to `tools/zesarux/src/zesarux`; override with
`ZESARUX=/path/to/zesarux`.

`make run-original` defaults to SDL video and audio at double the normal
Spectrum window scale, equivalent to
`ZESARUX_VO=sdl ZESARUX_AO=sdl ZESARUX_RUN_OPTS="--zoom 4"`. Check what your
local build actually has before choosing other overrides:

```sh
tools/zesarux/src/zesarux --help | sed -n '/--vo driver/,/--version/p'
```

A minimal Linux build may list only `fbdev stdout simpletext null` video and
`dsp onebitspeaker null` audio. `onebitspeaker` needs PC-speaker I/O
permissions, so use `null` for smoke tests and RE automation
(`ZESARUX_VO= ZESARUX_AO=null`). If `sdl` or `xwindows` is not listed, that
binary cannot open a window with it — install the matching development
headers, rebuild, and then name the enabled drivers explicitly.

## Approach

**The hard RE is done.** `original/disasm/batty.asm` (from
[`CityAceE/Batty`](https://github.com/CityAceE/Batty), vendored as a
submodule) is a complete, named, annotated Z80 disassembly — every routine
that matters already has a label and a comment. It is also build-verified: an
assembled copy differs from the reference binary in exactly the bytes we patch
(`notes/per-level-profile.md`). Treat it as the source of truth.

Workflow for a new port:

1. Find the routine in `original/disasm/batty.asm` by its label
   (`handling_ball`, `print_briks`, `enemy_prepare`, ...).
   `scripts/disasm.py <label>` prints it.
2. Port it into the right `src/` module, citing the routine name in a code
   comment so the lineage is searchable.
3. Add a one-paragraph entry in `notes/<topic>.md` only if the routine exposes
   a non-obvious invariant (encoding, RAM layout, timing quirk). Otherwise the
   disassembly plus the code comment is enough.

**Read past the label.** A routine boundary is a label, not an end: several
wrong conclusions here came from listings that stopped one instruction short.
`scripts/disasm.py` prints a FALLS THROUGH warning for it, and
[`notes/lessons.md`](notes/lessons.md) collects the rest of the traps.

You should not need to disassemble anything from `original/batty.tap`
yourself. The RE-only tooling earns its keep only when chasing **dynamic or
SMC behaviour** the static disassembly cannot resolve (computed jumps,
self-modifying loops — use ZRCP single-stepping against ZEsarUX), or when
**rediscovering a routine** not yet named in `original/disasm/`, which is rare
enough to be worth flagging upstream. Otherwise: read the disassembly, port
the routine, gate it.

## Original assets

`original/batty.tap`, `original/BATTY.TZX`, `original/Batty.scr` and
`original/Batty.txt` are © Elite Systems / Hit-Pak, 1987. Vendored for
personal reverse-engineering reference only. See PLAN.md WS9 before
distributing anything built from them.

## License

The MS-DOS recreation (everything under `src/`, `tests/`, `scripts/`,
`notes/`, the `Makefile`) is MIT.
