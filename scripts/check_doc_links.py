#!/usr/bin/env python3
"""A file path cited in a comment or note must exist.

Three citations had rotted:

  notes/magnets-missing.md   folded into notes/magnets.md by a notes
                             audit; the comment citing it kept the old
                             path, so the 271-px measurement that
                             justifies painting both magnet sprites led
                             nowhere.
  notes/laser.md             cited for "the long note", and never
                             written at all.
  scripts/repro_enemy_flyover_trail.py
                             deleted once the bug it reproduced was
                             fixed, still cited as a thing to run.

None of these break a build, which is why they lasted. They cost a
reader's time instead — a provenance comment whose evidence cannot be
found is worse than one that states the fact inline, because it looks
like the fact is documented somewhere.

The fixes were not "delete the citation": in each case the thing the
citation pointed AT was worth keeping, so it moved into the comment.

### The self-reference trap

Explaining that a file is gone means naming it, and naming it in path
form trips this check — the same way the frozen-clock gate tripped on
its own comment and the gate-index checker swept names out of its own
prose. The convention adopted here is to name a dead file WITHOUT its
directory or extension (`a `laser` note`), which reads as history rather
than as a live pointer. This gate therefore only matches things that
look like real paths: a known top-level directory, a slash, and an
extension.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCAN = ["src/*.cpp", "src/*.h", "notes/*.md", "tests/*.cpp"]
PATH_RE = re.compile(
    r"\b((?:notes|src|scripts|tests|replays)/[A-Za-z0-9_./-]+\.[a-z]{1,4})\b")


def main() -> int:
    broken: list[tuple[str, str, int]] = []
    seen = 0
    for pattern in SCAN:
        for f in sorted(ROOT.glob(pattern)):
            for n, line in enumerate(f.read_text().split("\n"), 1):
                for m in PATH_RE.finditer(line):
                    seen += 1
                    if not (ROOT / m.group(1)).exists():
                        broken.append((m.group(1),
                                       str(f.relative_to(ROOT)), n))

    if not seen:
        raise SystemExit("FAIL: no file citations matched at all — the "
                         "pattern or the layout changed, so this gate is "
                         "checking nothing")

    if broken:
        print("FAIL: a cited file does not exist\n")
        for target, where, line in broken:
            print(f"  {where}:{line}")
            print(f"    cites {target}, which is not there")
        print("\nEither fix the path, or move what the citation pointed at "
              "into the text. If you are recording that a file was REMOVED, "
              "name it without its directory and extension — a `laser` note "
              "— so it reads as history and does not look like a live "
              "pointer.")
        return 1

    print(f"PASS doc_links: {seen} file citations, all resolve")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
