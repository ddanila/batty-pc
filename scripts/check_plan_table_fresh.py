#!/usr/bin/env python3
"""PLAN.md's definition-of-done table must be newer than the work it grades.

The table has gone stale three times. Each refresh wrote a paragraph
about how bad that is and changed nothing structural, so it went stale
again — most recently claiming "bonus ownership and bat-2 catch remain"
after both landed the same day.

The failure is not that anyone believes the wrong thing. It is that work
lands in a WORKSTREAM section and nobody re-reads the TABLE, which lives
four hundred lines above it.

### What this can and cannot check

It cannot tell whether the prose is true. No gate can read "Done" and
know. What it CAN do is compare dates: if any workstream section records
a date newer than the table's own "Table refreshed YYYY-MM-DD", the
table has not been looked at since work landed and is presumed stale.

That is exactly the step being skipped, and it is decidable.

Bumping the date without reading the rows defeats it. That is deliberate
— the gate converts an omission into a choice, which is the most a
freshness check can honestly do.

### Why dates and not, say, gate counts

`test-notes-numbers` already covers every NUMBER in the table and has
never once been the thing that went stale; numbers are easy to check and
so they get checked. It is the prose that rots. Dates are the only
machine-readable proxy for "has anyone looked at this lately".
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLAN = ROOT / "PLAN.md"

DATE = re.compile(r"20\d\d-\d\d-\d\d")


def main() -> int:
    text = PLAN.read_text()

    m = re.search(r"Table refreshed (20\d\d-\d\d-\d\d)", text)
    if not m:
        print("FAIL: PLAN.md no longer states 'Table refreshed <date>' under "
              "the definition-of-done table.\n\nThat line is what this gate "
              "compares against. If the table moved or was reworded, put a "
              "dated refresh marker back — without one, nothing can tell "
              "whether the table has been read since the work it grades.")
        return 1
    refreshed = m.group(1)

    # Everything from the first workstream heading onward is the work.
    ws = text.find("\n## WS1")
    if ws < 0:
        print("FAIL: PLAN.md has no '## WS1' heading; this gate cannot tell "
              "the table from the workstreams any more.")
        return 1

    newer = sorted({d for d in DATE.findall(text[ws:]) if d > refreshed})
    if newer:
        print(f"FAIL: the definition-of-done table was refreshed "
              f"{refreshed}, but the workstreams record work dated "
              f"{', '.join(newer)}.\n")
        print("Re-read the table's rows against what actually landed, then "
              "move the 'Table refreshed' date. The table has drifted three "
              "times by exactly this route: work lands in a WS section and "
              "nobody scrolls back up.")
        return 1

    print(f"PASS plan_table_fresh: the definition-of-done table "
          f"({refreshed}) is at least as new as every dated workstream entry")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
