#!/usr/bin/env python3
"""List identifiers the notes cite that nothing in the tree defines.

A REPORT, not a gate. Run it during a docs-hygiene pass:

    scripts/notes_symbols.py

### What it is for

Notes rot in a specific way here: a routine is renamed or deleted, and
prose that was true keeps naming it. `check_doc_links` catches dead
FILE citations, and nothing caught dead SYMBOL citations. The case that
prompted this: `bounce_enemy_off_margins` was deleted on 2026-08-09 and
three notes still named it — one of them, known-bugs #16, in the present
tense, asserting the exact opposite of the code ("The port does all
three. `bounce_enemy_off_margins` clamps to ...").

### Why it is not a gate

Run against the tree as it stands, it reports ~47 identifiers, and most
are legitimate:

  - past-tense history. This repo deliberately keeps the record of what
    a thing used to be called, so `static_bg_dirty` appearing in the
    sentence that describes its rename to `static_bg_cache_dirty` is
    correct, not stale.
  - partial names — `all_metal_briks` where the symbol is
    `all_metal_briks_animation_snd`.
  - probe-field VALUES rather than identifiers (`active01_type04`).

Telling those apart from a rotted present-tense claim needs a reader.
Making this fail the build would mean either an allowlist that goes
stale the same way the notes do — the exact problem — or pressure to
delete history to get green. So: a tool for the hygiene pass, and the
judgement stays with the person running it.

Exit status is always 0. It reports; it does not decide.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Where a symbol may legitimately live: the port, its tests and gates,
# the build, and the disassembly (notes cite Z80 labels constantly).
SOURCES = ("src/*.cpp", "src/*.h", "tests/*.cpp", "scripts/*.py",
           "scripts/*.sh", "Makefile")

IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]{3,}")
# Only backticked snake_case: `foo_bar`. Prose words and CamelCase are
# too noisy to be worth reporting.
CITED = re.compile(r"`([a-z][a-z0-9]*(?:_[a-z0-9]+)+)`")


def known_identifiers() -> set:
    known = set()
    for pattern in SOURCES:
        for f in ROOT.glob(pattern):
            known |= set(IDENT.findall(f.read_text(errors="ignore")))
    for f in ROOT.glob("original/disasm/**/*.asm"):
        known |= set(IDENT.findall(f.read_text(errors="ignore")))
    return known


def main() -> int:
    known = known_identifiers()
    missing: dict = {}
    for f in sorted((ROOT / "notes").glob("*.md")):
        text = f.read_text()
        for n, line in enumerate(text.split("\n"), 1):
            for tok in CITED.findall(line):
                if tok not in known:
                    missing.setdefault(tok, []).append(f"{f.name}:{n}")

    if not missing:
        print("no notes cite an identifier the tree does not define")
        return 0

    print(f"{len(missing)} identifier(s) cited in notes/ that nothing in "
          f"src/, tests/, scripts/, the Makefile or the disassembly "
          f"defines.\n")
    print("Most will be deliberate history — a rename recorded in past "
          "tense is correct.\nWhat to look for is a PRESENT-TENSE claim "
          "about what the port does.\n")
    for tok, where in sorted(missing.items()):
        print(f"  {tok}")
        for w in where:
            print(f"      {w}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
