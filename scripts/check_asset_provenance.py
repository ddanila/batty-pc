#!/usr/bin/env python3
"""Every asset the game LOADS must be built from the tape.

PLAN.md WS7's exit condition is "assets/ contains only tape-extracted
data". This checks the half that matters: for each `.BIN` the port
loads, the Makefile rule that builds it must depend on
`original/blocks/*.dat.bin` — the tape's own blocks — and not on an
emulator snapshot or a screen capture.

Three assets failed this when it was written, and none of them had to:

  loading.bin           built from original/Batty.scr, which is a
                        byte-identical copy of tape block 02
  main_menu_markup.bin  cut out of a snapshot's RAM dump at $954D,
                        which is inside block 03
  markup.bin            no build rule AT ALL — 273 bytes checked in
                        with its provenance recorded nowhere

The last one is why this gate exists rather than a one-off audit. A
checked-in binary with no rule is invisible: nothing rebuilds it,
nothing explains it, and `make assets` does not mention it.

Assets that are only GATE REFERENCES — frame_l1.bin, level_attrs.bin,
main_menu.bin, hi_score.bin — are deliberately out of scope. They are
recordings of the original used to check the port's generators, and
recording them from an emulator is the point.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
MAKEFILE = ROOT / "Makefile"

LOADS = re.compile(r'asset_load\w*\(\s*"([A-Z0-9_]+\.BIN)"')
SHOWS = re.compile(r'show\(\s*"([A-Z0-9_]+\.BIN)"')
# `\s+`, not a single space: the recipes align the DOS names in a
# column, and a one-space pattern silently matched nothing for the
# three that are padded — reporting them as unbuildable rather than
# as fine.
COPIES = re.compile(r"mcopy[^\n]*-o (assets/[\w.]+)\s+::([A-Z0-9_]+\.BIN)")
TAPE = "original/blocks/"


def main() -> int:
    loaded = set()
    for f in sorted(SRC.glob("*.cpp")):
        text = f.read_text()
        loaded |= set(LOADS.findall(text)) | set(SHOWS.findall(text))
    if not loaded:
        raise SystemExit("FAIL: found no asset loads in src/")

    mk = MAKEFILE.read_text()
    dos_to_file = dict((dos, path) for path, dos in COPIES.findall(mk))

    problems = []
    for dos in sorted(loaded):
        path = dos_to_file.get(dos)
        if path is None:
            problems.append(f"{dos} is loaded but no mcopy line says which "
                            f"assets/ file it comes from")
            continue
        m = re.search(r"^" + re.escape(path) + r":([^\n]*)", mk, re.M)
        if not m:
            problems.append(f"{path} ({dos}) has NO build rule — a checked-in "
                            f"binary whose provenance is recorded nowhere")
            continue
        deps = m.group(1)
        if TAPE not in deps:
            problems.append(f"{path} ({dos}) is built from `{deps.strip()}` — "
                            f"not from the tape. Snapshots and screen "
                            f"captures are not sources for something the "
                            f"game loads.")

    if problems:
        print("FAIL: a loaded asset is not tape-derived\n")
        for p in problems:
            print(f"  {p}")
        print("\nPLAN.md WS7: assets/ contains only tape-extracted data.")
        return 1

    print(f"PASS asset_provenance: all {len(loaded)} loaded assets are built "
          f"from original/blocks/")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
