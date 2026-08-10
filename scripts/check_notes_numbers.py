#!/usr/bin/env python3
"""The plan's status block must state gate counts that are still true.

`notes/refactor-plan.md` opens with "## Where this stands" — the section
that exists to be CURRENT. Every other section is a narrative of what was
done and when, so "51 gates" in a paragraph about an earlier session is
correct and must stay. This gate therefore reads the status block ONLY.

Nobody notices a wrong number in a document; reviewers trust it. Every
count checked here is DERIVED from the Makefile and the parallel runner,
so the document is compared against the thing it describes rather than
against a second hand-maintained copy.

This gate used to pin `main.cpp`'s line count too. That check is gone
with the metric: a line count moved every time a comment was added or
removed, said nothing about whether the code improved, and cost an edit
to this repo's notes on sweeps that changed no behaviour at all. What
replaced it in the plan is the module/suite mapping, which
`test-host-tests-wired` already keeps honest.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLAN = ROOT / "notes" / "refactor-plan.md"
TESTING = ROOT / "notes" / "testing.md"
TESTS_DIR = ROOT / "tests"
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


def gate_counts() -> dict:
    """Every gate count the status block may legitimately state.

    The block names four: the QEMU suite, the emulator-free source
    gates, the ZEsarUX-oracle ones, and the total. Prose cannot tell the
    gate WHICH a given number is, so the rule is that every "N gates"
    claim must equal one of them — a wrong number matches none.

    The limit, stated because a green run should not be over-read:
    swapping two VALID counts (writing 59 where 71 belongs) is not
    caught. Catching that needs the gate to parse the sentence, which it
    cannot.

    Two earlier versions were worse. One knew only the runner's list and
    failed a status block that was more accurate than the gate. The next
    required the number to sit immediately before "gates", so it never
    saw "59 QEMU gates" at all — mutating 59 to 60 PASSED. Same three
    sources as check_gate_index.py."""
    src = RUNNER.read_text()
    all_gates: set[str] = set()
    for name in ("PARITY_CHECK_GATES", "FULL_EXTRA_GATES"):
        start = src.index(name + " = [")
        all_gates |= set(re.findall(r'"([a-z0-9-]+)"',
                                    src[start:src.index("]", start)]))
    mk = (ROOT / "Makefile").read_text()
    def recipe(target: str) -> set:
        m = re.search(r"^" + re.escape(target) + r":.*?\n((?:\t.*\n|\s*\n)*)",
                      mk, re.M)
        return set(re.findall(r"\$\(MAKE\) (test-[a-z0-9-]+|test)\b",
                              m.group(1))) if m else set()

    # Named "qemu" because for a long time it was exactly that: the
    # gates run_gates_parallel.py boots an emulator for. Since
    # 2026-08-10 the list also carries `test-asan`, which is host-only,
    # so read this bucket as "the full parallel sweep" — that is what
    # the notes now call it. The count stays derived from the runner,
    # which is the point; only the word is approximate.
    qemu_set = all_gates
    source = recipe("test-source-gates")
    # The ZEsarUX-only ones: named by parity-check-full and by nothing
    # else. Computed as a set difference, not by subtracting counts —
    # the groups OVERLAP (parity-check-full re-runs most of the QEMU
    # suite), and an earlier version that subtracted got 0.
    oracle = recipe("parity-check-full") - qemu_set - source
    total = qemu_set | source | oracle
    return {"qemu": len(qemu_set), "source": len(source),
            "oracle": len(oracle), "total": len(total)}


def main() -> int:
    block = status_block()
    counts = gate_counts()
    bad = []

    # A line count is not checked here, and must not come back: see the
    # module docstring. If one reappears in the status block, say so —
    # silently tolerating it is how the metric would creep back.
    if re.search(r"`main\.cpp`:\s*[\d,]+\s*(?:->|→|—)\s*[\d,]+\s*lines",
                 block):
        bad.append("the status block states a main.cpp line count again; "
                   "that metric was dropped deliberately (see this gate's "
                   "docstring) — state what the structure is, not how many "
                   "lines it takes")

    # Up to THREE words between the number and "gates". The block says
    # "71 gates", "59 QEMU gates" and "12 emulator-free source gates",
    # and each widening was forced by a mutation that PASSED: first
    # 59 -> 60 (one intervening word), then 12 -> 11 (two). Both were
    # cases of the gate appearing to check a number it could not see.
    claims = set(int(n) for n in
                 re.findall(r"(\d+)\s+(?:[A-Za-z-]+\s+){0,3}gates", block))
    claims |= set(int(a) for a, b in re.findall(r"\b(\d+)/(\d+)\b", block)
                  if a == b and int(a) > 20)
    if not claims:
        bad.append("the status block states no gate count at all")
    for n in sorted(claims):
        if n not in counts.values():
            bad.append(f"status block claims {n} gates, which is none of the "
                       f"real counts: {counts['qemu']} QEMU, "
                       f"{counts['source']} source, {counts['oracle']} "
                       f"ZEsarUX-oracle, {counts['total']} total")

    # PLAN.md's definition-of-done table is the THIRD place, and the
    # one a reader meets first. Only the table is scanned, not the whole
    # plan: the workstream sections are full of past-tense figures
    # ("all 59 QEMU gates passed with it broken") that are correct as
    # history, which is the same reason the status block above is read
    # in isolation. The table is present tense by construction.
    plan = (ROOT / "PLAN.md").read_text()
    tm = re.search(r"^\| # \| Criterion \| Today \|\n(?:\|[^\n]*\n)+", plan,
                   re.M)
    if not tm:
        bad.append("PLAN.md has no definition-of-done table; if it moved, "
                   "point this gate at it")
    else:
        for n in set(int(x) for x in
                     re.findall(r"(\d+)\s+(?:[A-Za-z-]+\s+){0,3}gates",
                                tm.group(0))):
            if n not in counts.values():
                bad.append(f"PLAN.md's status table claims {n} gates, which "
                           f"is none of the real counts: {counts['qemu']} "
                           f"QEMU, {counts['source']} source, "
                           f"{counts['oracle']} oracle, {counts['total']} "
                           f"total")

    # notes/testing.md's opening blockquote is the other place a reader
    # meets these numbers, and it is the FIRST thing in the file. It
    # stated "make test-video is the one gate here that needs no
    # emulator" long after that stopped being true, so it gets the same
    # treatment as the plan's status block.
    # Fold the blockquote to one line before matching: the numbers wrap
    # across lines behind "> " markers, and a line-oriented regex misses
    # them. Two mutations passed before this was added.
    intro_raw = TESTING.read_text().split("\n## ", 1)[0]
    intro = " ".join(l.lstrip("> ").strip() for l in intro_raw.split("\n"))
    suites = len(list(TESTS_DIR.glob("test_*.cpp")))
    for n in set(int(x) for x in
                 re.findall(r"(\d+)\s+(?:[A-Za-z-]+\s+){0,3}gates", intro)):
        if n not in counts.values():
            bad.append(f"notes/testing.md's intro claims {n} gates, which is "
                       f"none of the real counts: {counts['qemu']} QEMU, "
                       f"{counts['source']} source, {counts['oracle']} "
                       f"oracle, {counts['total']} total")
    for n in set(int(x) for x in
                 re.findall(r"(\d+)\s+host\s+(?:[A-Za-z-]+\s+){0,2}suites",
                            intro)):
        if n != suites:
            bad.append(f"notes/testing.md's intro claims {n} host suites; "
                       f"tests/ has {suites}")

    if bad:
        print("FAIL: a status/intro block has gone stale\n")
        for b in bad:
            print(f"  - {b}")
        print("\nFix the numbers in 'Where this stands'. Historical figures "
              "elsewhere in the file are past tense and correct as written — "
              "this gate deliberately does not read them.")
        return 1

    print(f"PASS notes_numbers: plan + testing.md intro agree with reality "
          f"({counts['qemu']} QEMU + {counts['source']} source + "
          f"{counts['oracle']} oracle = {counts['total']} gates)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
