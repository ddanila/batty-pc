#!/usr/bin/env python3
"""What the floppy carries and what the game loads must be the same set.

Two failures, one in each direction:

  MISSING   the port calls `asset_load("X.BIN", ...)` and no Makefile
            rule mcopies it. The build succeeds, the boot fails, and the
            first sign is a black screen under QEMU.

  DEAD      a Makefile rule ships an asset nothing loads. Harmless at
            runtime, which is why it survives: `MAINMENU.BIN` and
            `HISCORE.BIN` — 48 KB each — were on every floppy image
            built, unread, because the port started generating those
            screens from `MENUMARK.BIN` / `MARKUP.BIN` markup and the
            copy lines stayed.

The dead half is the reason this exists. Two full-screen captures on the
floppy is not just 96 KB: it is evidence, to anyone reading the build,
that the port still displays captured screens — which is what PLAN.md's
WS7 item 3 said, wrongly, long after it stopped being true.

Both floppy targets are checked, and they must agree with each other
too: a `make floppy` that carries an asset `make $(TEST_FLOPPY)` does
not is a gate that passes on an image the player never gets.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
MAKEFILE = ROOT / "Makefile"

# Any asset_load* variant: _variable and _chunked both exist, and a
# regex naming only the ones that existed when it was written reports
# a live asset as dead. RANDOM.BIN (asset_load_chunked) was the one
# that caught this.
LOADS = re.compile(r'asset_load\w*\(\s*"([A-Z0-9_]+\.BIN)"')
SHIPS = re.compile(r"mcopy[^\n]*::([A-Z0-9_]+\.BIN)")
# `show()` displays a full-screen image straight off the disk.
SHOWS = re.compile(r'show\(\s*"([A-Z0-9_]+\.BIN)"')


def main() -> int:
    loaded = set()
    for f in sorted(SRC.glob("*.cpp")):
        text = f.read_text()
        loaded |= set(LOADS.findall(text))
        loaded |= set(SHOWS.findall(text))
    if not loaded:
        raise SystemExit("FAIL: found no asset loads in src/ at all — the "
                         "call pattern changed and this gate is checking "
                         "nothing")

    mk = MAKEFILE.read_text()
    # One set per floppy recipe, so the two images can be compared.
    recipes = {}
    for m in re.finditer(r"^([\w./$()-]+):[^\n]*\n((?:\t[^\n]*\n|\s*\n)*)",
                         mk, re.M):
        names = set(SHIPS.findall(m.group(2)))
        if names:
            recipes[m.group(1)] = names
    if not recipes:
        raise SystemExit("FAIL: found no mcopy lines in the Makefile — the "
                         "floppy recipes changed and this gate is checking "
                         "nothing")

    problems = []
    for target, ships in sorted(recipes.items()):
        for name in sorted(loaded - ships):
            problems.append(f"{target} does not ship {name}, which the port "
                            f"loads — the boot will fail")
        for name in sorted(ships - loaded):
            problems.append(f"{target} ships {name}, which nothing in src/ "
                            f"loads — dead weight on the image")

    sets = list(recipes.values())
    if any(s != sets[0] for s in sets[1:]):
        for a in recipes:
            for b in recipes:
                if a < b and recipes[a] != recipes[b]:
                    diff = recipes[a] ^ recipes[b]
                    problems.append(f"{a} and {b} carry different assets: "
                                    f"{sorted(diff)}")

    if problems:
        print("FAIL: the floppy and the port disagree about assets\n")
        for p in problems:
            print(f"  {p}")
        return 1

    print(f"PASS floppy_assets: {len(loaded)} assets, loaded by the port "
          f"and shipped by all {len(recipes)} floppy recipes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
