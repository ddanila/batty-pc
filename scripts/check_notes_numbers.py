#!/usr/bin/env python3
"""The plan's status block must state numbers that are still true.

`notes/refactor-plan.md` opens with "## Where this stands" — the section
that exists to be CURRENT. Every other section is a narrative of what was
done and when, so "51 gates" in a paragraph about an earlier session is
correct and must stay. This gate therefore reads the status block ONLY.

It exists because those numbers went stale three times, and the last time
was worse than stale: the line count quoted Watcom's `N lines` report
instead of `wc -l`. Those two agreed (within 1) when the baseline was
recorded and have since diverged by a constant 96 for this file — adding
10 lines moves both by 10, so it is an offset, not a scaling. The cause
is not established. Comparing a Watcom count against a `wc` baseline
mixed two measures and understated the reduction. `wc -l` is the measure
now, and this gate pins it.

Nobody notices a wrong number in a document. Reviewers trust it, and a
line count is the one claim a refactor's notes are actually judged on.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLAN = ROOT / "notes" / "refactor-plan.md"
MAIN = ROOT / "src" / "main.cpp"
RUNNER = ROOT / "scripts" / "run_gates_parallel.py"

SECTION = "## Where this stands"


def status_block() -> str:
    text = PLAN.read_text()
    if SECTION not in text:
        raise SystemExit(
            f"FAIL: {PLAN.name} has no '{SECTION}' section — this gate reads "
            f"that section and nothing else. Rename it back, or point this "
            f"gate at whatever now holds the current numbers.")
    body = text.split(SECTION, 1)[1]
    nxt = body.find("\n## ")
    return body if nxt < 0 else body[:nxt]


def gate_count() -> int:
    src = RUNNER.read_text()
    total = 0
    for name in ("PARITY_CHECK_GATES", "FULL_EXTRA_GATES"):
        start = src.index(name + " = [")
        block = src[start:src.index("]", start)]
        total += len(re.findall(r'"[a-z0-9-]+"', block))
    return total


def main() -> int:
    block = status_block()
    real_lines = len(MAIN.read_text().splitlines())
    real_gates = gate_count()
    bad = []

    # "`main.cpp`: 7,747 -> 6,762 lines"  — the arrow may be -> or an en/em dash
    m = re.search(r"`main\.cpp`:\s*([\d,]+)\s*(?:->|→|—)\s*([\d,]+)\s*lines", block)
    if not m:
        bad.append("the status block no longer states a `main.cpp`: A -> B "
                   "lines figure; this gate cannot check what is not claimed")
    else:
        claimed = int(m.group(2).replace(",", ""))
        if claimed != real_lines:
            bad.append(f"status block says main.cpp is {claimed:,} lines; "
                       f"wc -l says {real_lines:,} "
                       f"(NB: Watcom's 'N lines' is NOT this number)")

    claims = set(int(n) for n in re.findall(r"(\d+)\s+gates", block))
    claims |= set(int(a) for a, b in re.findall(r"\b(\d+)/(\d+)\b", block)
                  if a == b and int(a) > 20)
    if not claims:
        bad.append("the status block states no gate count at all")
    for n in sorted(claims):
        if n != real_gates:
            bad.append(f"status block claims {n} gates; the runner defines "
                       f"{real_gates} (PARITY_CHECK_GATES + FULL_EXTRA_GATES)")

    if bad:
        print("FAIL: the plan's status block has gone stale\n")
        for b in bad:
            print(f"  - {b}")
        print("\nFix the numbers in 'Where this stands'. Historical figures "
              "elsewhere in the file are past tense and correct as written — "
              "this gate deliberately does not read them.")
        return 1

    print(f"PASS notes_numbers: status block agrees with reality "
          f"(main.cpp {real_lines:,} lines, {real_gates} gates)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
