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


TRIPLE_D = '"' * 3
TRIPLE_S = "'" * 3


def strip_prose(path, text: str) -> str:
    """Drop comments and docstrings, so PROSE cannot define a symbol.

    Added 2026-08-10, and it is the difference between this tool working
    and not. The corpus used to be raw text, so a name DELETED from the
    code but still present in a comment counted as defined — and the
    name that survives in a stale comment is exactly the name a stale
    note is about.

    Measured that day: `bounce_enemy_off_margins`, the deletion that
    prompted this tool to exist, was masked by the comments mentioning
    it. So was `primary_ball_launch_from_bat`, renamed hours earlier.
    Stripping takes the report from 36 names to 58, and the extra 24
    include both.

    Crude regexes on purpose. This is a report a human triages, so a
    docstring that survives a bad match costs nothing, while a real
    parser would cost a dependency and an afternoon.
    """
    if path.suffix in (".cpp", ".h"):
        text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
        return re.sub(r"//[^\n]*", " ", text)
    if path.suffix == ".py":
        text = re.sub(TRIPLE_D + r".*?" + TRIPLE_D, " ", text, flags=re.S)
        text = re.sub(TRIPLE_S + r".*?" + TRIPLE_S, " ", text, flags=re.S)
        return re.sub(r"#[^\n]*", " ", text)
    return text


def known_identifiers() -> set:
    known = set()
    for pattern in SOURCES:
        for f in ROOT.glob(pattern):
            known |= set(IDENT.findall(
                strip_prose(f, f.read_text(errors="ignore"))))
            # A script called notes_symbols.py DOES define the name
            # `notes_symbols`, and notes cite scripts that way. Without
            # this, stripping docstrings reports every tool in scripts/
            # as missing.
            known.add(f.stem)
    for f in ROOT.glob("original/disasm/**/*.asm"):
        known |= set(IDENT.findall(f.read_text(errors="ignore")))
        known.add(f.stem)
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
