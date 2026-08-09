#!/usr/bin/env python3
"""known-bugs.md's status table must agree with its own sections.

The file opens with a one-row-per-bug table and then documents each bug
at length below. Both drift, and they drift apart:

  #16  the row said "open by design decision" for three weeks after
       check_margins was ported literally and gated — the section itself
       said "Closed 2026-08-09" in its own body
  #17  had a full section and no row at all
  prose "#14 is the only open item", two lines under a #14 row reading
       "**fixed**"

A status table that contradicts its own prose is worse than no table:
a reader takes whichever half they read first.

Two checks:

  COVERAGE   every `## #N` section has a table row, and every row has a
             section. A bug documented but unlisted is invisible to
             anyone reading the summary; a row with no section is a
             claim with no evidence.

  STATE      a row calling a bug open, while its section says it was
             fixed or closed on a date, is the #16 failure exactly.

What this cannot check is whether "fixed" is TRUE — that is what the
gates named in each row are for.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOC = ROOT / "notes/known-bugs.md"

OPEN_WORDS = re.compile(r"\bopen\b|\bunfixed\b|\bnot fixed\b", re.I)
CLOSED_IN_BODY = re.compile(r"\b(?:FIXED|Closed|RESOLVED)\s+\d{4}-\d{2}-\d{2}")


def main() -> int:
    text = DOC.read_text()

    tm = re.search(r"^\| # \| what \| state \|\n\|[-| ]+\|\n((?:\|[^\n]*\n)+)",
                   text, re.M)
    if not tm:
        raise SystemExit("FAIL: no status table in notes/known-bugs.md; if it "
                         "moved, point this gate at it")
    rows = {}
    for line in tm.group(1).strip().split("\n"):
        cells = [c.strip() for c in line.strip("|").split("|")]
        m = re.match(r"#(\d+)", cells[0])
        if m:
            rows[int(m.group(1))] = cells[-1]

    # Two heading styles coexist: the older `## 8. Extra balls ...` and
    # the newer `## #13 — ...`. Matching only the newer reported six
    # false "row but no section" failures the first time this ran.
    sections = {}
    for m in re.finditer(r"^## #?(\d+)[.\s]", text, re.M):
        n = int(m.group(1))
        end = text.find("\n## ", m.end())
        sections[n] = text[m.start():end if end > 0 else len(text)]

    bad = []
    for n in sorted(set(sections) - set(rows)):
        bad.append(f"#{n} has a section but no row — invisible to anyone "
                   f"reading the summary")
    # A row with no section is allowed ONLY if it names the gate that
    # holds the fix — #3's fix predates this file's section format, and
    # `test-shimmer-one-pass` is better evidence than prose would be.
    # Not a silent exemption: a row with neither a section nor a gate is
    # a claim with nothing behind it.
    for n in sorted(set(rows) - set(sections)):
        if not re.search(r"`test-[\w-]+`", rows[n]):
            bad.append(f"#{n} has a row but neither a section nor a named "
                       f"gate — a claim with nothing behind it")

    for n in sorted(set(rows) & set(sections)):
        state = rows[n]
        if OPEN_WORDS.search(state) and CLOSED_IN_BODY.search(sections[n]):
            hit = CLOSED_IN_BODY.search(sections[n]).group(0)
            bad.append(f"#{n}'s row says {state!r} but its section says "
                       f"{hit!r}")

    if bad:
        print("FAIL: known-bugs.md's table disagrees with its sections\n")
        for b in bad:
            print(f"  {b}")
        return 1

    print(f"PASS known_bugs_table: {len(rows)} rows, {len(sections)} sections, "
          f"none claiming open over a section that records a fix")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
