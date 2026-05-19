# Known bugs (user-reported, unfixed)

Concrete defects observed by the user that aren't yet caught by the
visual-regression test (= mid-game state we don't snapshot). Listed
here so the next iter has a target. When fixing, add a section to
`per-level-profile.md` or the relevant area doc; remove from here.

## 1. Triple-bonus (multi-ball / MULTI_BALL) power-up renders wrong

The MULTI_BALL bonus letter sprite renders incorrectly. Specific
nature of the mis-render not yet captured — needs side-by-side
screenshot vs the original.

Likely site: `render_bonus_to_buff` in `src/main.c` uses
`spr_for_bonus(bonus_type)` to pick the letter sprite. The bonus
sprites live in `assets/sprites.bin` at offsets computed from the
disasm. If the MULTI_BALL offset or the bonus_type → sprite_num map
is off-by-one, we'd render an adjacent letter.

To verify: catch a MULTI_BALL drop in `make run` and screenshot the
falling sprite. Compare against `original/batty.tap` boot in
ZEsarUX. The original's bonus letters are at `gfx_bonuses` in
`original/disasm/routines/sprites_search.asm`.

## 2. `make run-original` doesn't work

The RE target that boots the original game in ZEsarUX for side-by-
side comparison. Symptom not yet detailed (= what error / what
happens on invocation).

To investigate:

```sh
make run-original
```

Likely causes:
- ZEsarUX not built. Bring it up via
  `git submodule update --init tools/zesarux` then
  `cd tools/zesarux/src && ./configure && make`.
- Path mismatch — the Makefile defaults to
  `tools/zesarux/src/zesarux`; override with
  `ZESARUX=/path/to/zesarux` if installed elsewhere.
- Missing tape file at `original/batty.tap`.

If the binary builds and tape is present but the target still fails,
check the Makefile recipe and the ZESARUX command-line args it
passes (`--noconfigfile --machine 48k ...` per line 248+ in the
Makefile).
