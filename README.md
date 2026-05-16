# batty-pc

MS-DOS / 8086 re-creation of **Batty** (Elite Software / Hit-Pak, 1987,
ZX Spectrum 48K). VGA mode 13h (320×200×256), 1:1 pixel-faithful inside
the 256×192 playfield, border around it. Built with Open Watcom v2,
boots in QEMU.

The toolchain pattern mirrors two sibling projects:

- [`generaly`](https://github.com/ddanila/generaly) — 1:1 Z80 reimpl
  of a different Speccy game.
- [`adlib-rng`](https://github.com/ddanila/adlib-rng) — Open Watcom
  + `mtools` + QEMU + AdLib boilerplate for 16-bit DOS work.

## Status

The game is playable end-to-end: title → menu → hi-score teaser →
all 15 levels → game-over → 3-letter initials entry → back to title.
`make test` headlessly diffs four checkpoints against ZX GT snapshots:

```
PASS state1_title       (pixel-identical)
PASS state2_menu        (pixel-identical)
PASS state3_hiscore     (pixel-identical)
INFO state4_level1      194 / 49 152 px differ (~0.4%)
```

The 194-px residual on state4 is the intentional bat + ball overlay
over a bat-free GT snapshot — the absolute floor without recapturing
the GT mid-render. Everything else matches.

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
| Headless visual regression (4 checkpoints)   | ✓ `make test` (`notes/testing.md`) |
| 2-player mode (`game_mode == 2`)             | open — selectable from menu, not wired into run loop |
| Real `handling_ball` 64-direction motion     | open — port uses integer dx/dy + 5-zone deflection |
| Frame ornament from `spr_bord_*` primitives  | open — currently bundles a captured `frame_l1.bin` |

## Quick build / run

```sh
brew install mtools qemu z80dasm srecord
make floppy   # builds build/batty.exe + assets, packs build/batty.img
make run      # boots in QEMU
make test     # headless visual-regression (boots, drives keys, pixel-diffs)
```

The remaining Makefile targets (`make regions`, `make candidates`,
`make run-original`, `make snapshot`) drive RE tooling against the
original tape — only needed when rediscovering something not already
captured in `original/disasm/` or the `notes/`.

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
vendor/             Symlinks to sibling repos (openwatcom-v2, msdos
                    from adlib-rng; zrcp.py from generaly)
```

## Toolchain

The build needs **Open Watcom v2** (16-bit, 8086) and an **MS-DOS 4.0
boot floppy** for QEMU. Both are vendored in
[`adlib-rng`](https://github.com/ddanila/adlib-rng); this repo's
`vendor/` is just symlinks into that sibling checkout. ZEsarUX (used
for the RE targets) is vendored similarly in
[`generaly`](https://github.com/ddanila/generaly).

## Approach

`original/disasm/batty.asm` is the authoritative annotated
disassembly — every routine that matters has been named and
documented there. New ports of game behaviour cite their source
routine in `notes/<topic>.md` and in code comments where helpful.
ZRCP single-step against ZEsarUX is still the fallback for SMC and
computed-jump questions the static disasm can't answer.

## Original assets

`original/batty.tap`, `original/BATTY.TZX`, `original/Batty.scr`,
`original/Batty.txt` are © Elite Systems / Hit-Pak, 1987. Vendored for
personal reverse-engineering reference only — the same pattern
[`generaly`](https://github.com/ddanila/generaly) uses with its
`original/generals.trd`.

## License

The MS-DOS recreation (everything under `src/`, `scripts/`, `notes/`,
the `Makefile`, etc.) is MIT.
