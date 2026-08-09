#!/usr/bin/env python3
"""Verify that every source-grep gate's needles still exist in src/.

Several gates assert on the SHAPE of the port's source: that a constant is
still $1B, that a guard still excludes rocket_active, that a dirty rect
still covers 18x10. They do it by searching src/*.cpp for a literal
string. That works right up until the code moves or a variable is
renamed, at which point the gate fails for a reason that has nothing to
do with what it is guarding -- and it fails only when someone runs it.

Twice in one session that took six commits and a whole refactor to
notice. This finds it in about a second:

    make test-gate-greps

It is deliberately conservative. A literal counts as a needle only if it
looks like C (contains a brace, paren, semicolon, operator or a known
identifier shape) and is long enough not to match by accident. False
negatives are fine; false alarms would train people to ignore it.
"""
from __future__ import annotations

import ast
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
SCRIPTS = ROOT / "scripts"

MIN_LEN = 12


def source_text() -> str:
    return "".join(p.read_text() for p in sorted(SRC.glob("*.cpp")) + sorted(SRC.glob("*.h")))


def _iterates(tree, var: str, container: str) -> bool:
    """True if `var` is a comprehension/loop variable ranging over `container`."""
    for node in ast.walk(tree):
        gens = getattr(node, "generators", None)
        if gens:
            for g in gens:
                if (isinstance(g.target, ast.Name) and g.target.id == var
                        and isinstance(g.iter, ast.Name) and g.iter.id == container):
                    return True
        if (isinstance(node, ast.For) and isinstance(node.target, ast.Name)
                and node.target.id == var
                and isinstance(node.iter, ast.Name) and node.iter.id == container):
            return True
    return False


def needles(path: Path):
    """Literals the gate searches for INSIDE the source.

    The signature is a required-containment test -- `"..." not in src`
    guarding a failure -- which is how every source-grep gate here asserts
    the port still says something. A
    literal used any other way is a path, a message or a regex, and has
    nothing to do with whether the source still says what the gate
    expects. Matching on shape instead flagged 194 of those; a check that
    cries wolf is worse than no check.

    Literals reached through a variable (`guard = "..."; if guard not in
    src`) are picked up too, by resolving simple string assignments.

    Yields (text, line, scoped). `scoped` marks a needle whose
    right-hand side is not a plain name -- `needle not in
    compact[idx:idx + 220]` asserts a POSITION, and all this checker can
    confirm is that the text still exists somewhere. Those are counted
    and reported separately so a green run does not claim more than it
    checked.
    """
    try:
        tree = ast.parse(path.read_text())
    except SyntaxError:
        return

    # `name = "literal"` and `name = ["a", "b"]` bindings, so needles reached
    # through a variable or a list comprehension still resolve. A gate that
    # writes `[n for n in required if n not in src]` is doing exactly what
    # this checks; missing it defeats the purpose.
    bound = {}
    listed = {}
    for node in ast.walk(tree):
        if not (isinstance(node, ast.Assign) and len(node.targets) == 1
                and isinstance(node.targets[0], ast.Name)):
            continue
        name = node.targets[0].id
        val = node.value
        if isinstance(val, ast.Constant) and isinstance(val.value, str):
            bound[name] = (val.value, node.lineno)
        elif isinstance(val, (ast.List, ast.Tuple)):
            items = [(e.value, node.lineno) for e in val.elts
                     if isinstance(e, ast.Constant) and isinstance(e.value, str)]
            if items:
                listed[name] = items

    for node in ast.walk(tree):
        if not isinstance(node, ast.Compare):
            continue
        # Polarity matters. `if needle not in src: FAIL` means the needle
        # is REQUIRED, and that is what goes stale when code moves.
        # `if needle in src: FAIL` means it is FORBIDDEN -- absence is the
        # correct state and there is nothing to verify.
        if not any(isinstance(op, ast.NotIn) for op in node.ops):
            continue
        left = node.left
        rhs = node.comparators[0] if node.comparators else None
        scoped = not isinstance(rhs, ast.Name)
        # Which haystack: a gate comparing against a whitespace-stripped
        # copy writes its needles pre-compacted on purpose. One comparing
        # against the raw text needs the indentation to match.
        rhs_name = rhs.id if isinstance(rhs, ast.Name) else ""
        # An INLINE `"".join(x.split())` is a compacted haystack just as
        # much as a variable named `compact`. Without this, a needle
        # written pre-compacted against an inline expression was checked
        # raw and reported as "matches only after whitespace
        # normalisation" — a warning about the gate's own classifier,
        # not about the source.
        inline_compact = (rhs is not None
                          and "split" in ast.dump(rhs)
                          and "join" in ast.dump(rhs))
        raw = (rhs_name not in ("compact", "compact_direction",
                                "physics_compact")) and not inline_compact
        if isinstance(left, ast.Constant) and isinstance(left.value, str):
            text, line = left.value, node.lineno
        elif isinstance(left, ast.Name) and left.id in bound:
            text, line = bound[left.id]
        elif isinstance(left, ast.Name):
            # a comprehension/loop variable: yield every candidate it ranges
            # over, e.g. `for needle in required if needle not in src`
            for src_name, items in listed.items():
                if src_name in {n.id for n in ast.walk(node) if isinstance(n, ast.Name)} \
                   or _iterates(tree, left.id, src_name):
                    for t, ln in items:
                        if len(t) >= MIN_LEN:
                            yield t, ln, scoped, raw, rhs_name
            continue
        else:
            continue
        if len(text) >= MIN_LEN:
            yield text, line, scoped, raw, rhs_name


def source_haystacks(text: str) -> set:
    """Variables in a gate script that hold C source text.

    A name qualifies if it is assigned from an expression mentioning a
    .cpp/.h path or the shared source_text() helper, or from a transform
    of a name that already qualifies (`compact = "".join(src.split())`).
    """
    import ast as _ast
    tree = _ast.parse(text)
    good: set = set()
    for _ in range(3):                    # let transforms chain
        for node in _ast.walk(tree):
            if not isinstance(node, _ast.Assign) or len(node.targets) != 1:
                continue
            tgt = node.targets[0]
            if not isinstance(tgt, _ast.Name):
                continue
            expr = _ast.dump(node.value)
            names = {n.id for n in _ast.walk(node.value)
                     if isinstance(n, _ast.Name)}
            if (".cpp" in expr or ".h'" in expr or "source_text" in expr
                    or "SRC" in names or names & good):
                good.add(tgt.id)
    return good


def main() -> int:
    # Only gates that actually read the source can have stale needles.
    #
    # Detecting that by the literal path `src/main.cpp` was wrong twice
    # over. Gates that build the path with pathlib segments —
    # `ROOT / "src" / "main.cpp"` — never matched, so FOUR were skipped
    # silently: test_stuck_ball_offset, test_invariant_owners,
    # test_game_over and test_ball_sign_cache_owner, which are precisely
    # the invariant gates. And the glob missed check_*.py, some of which
    # carry needles too. Found by renaming rest_ball_on_bat, which
    # test_stuck_ball_offset greps for by name: this gate passed.
    #
    # The rule now is "mentions a C source filename", which both styles
    # satisfy.
    gates = [p for p in sorted(list(SCRIPTS.glob("test_*.py"))
                               + list(SCRIPTS.glob("check_*.py")))
             if p.name != "check_gate_greps.py"
             and re.search(r'\b\w+\.(cpp|h)\b', p.read_text())]
    src = source_text()
    compact = "".join(src.split())

    stale = []
    loose = []
    checked = 0
    scoped_count = 0
    for gate in gates:
        src_vars = source_haystacks(gate.read_text())
        for text, line, scoped, raw, rhs_name in needles(gate):
            # A needle is only checkable against src/ if the thing it is
            # compared AGAINST came from a C source file. check_notes_numbers
            # reads main.cpp for a line count but greps the PLAN document;
            # judging by filename alone reported its `## Where this stands`
            # as a stale source needle.
            if rhs_name and rhs_name not in src_vars:
                continue
            checked += 1
            if scoped:
                scoped_count += 1
            if text in src:
                continue
            # It only matches once whitespace is normalised. Gates that
            # compare against `compact` are fine; gates that compare
            # against the raw source are NOT, and will fail. This
            # checker cannot tell which, so it says so rather than
            # counting the needle as verified.
            if "".join(text.split()) in compact:
                # Only a problem when the gate wanted the raw text.
                if raw:
                    loose.append((gate.name, line, text))
                continue
            stale.append((gate.name, line, text))

    if stale:
        print(f"FAIL: {len(stale)} gate needle(s) no longer match src/\n")
        for name, line, text in stale:
            shown = text if len(text) <= 88 else text[:85] + "..."
            print(f"  {name}:{line}")
            print(f"    {shown}")
        print("\nThe code moved or was renamed. Update the gate, or the next")
        print("person to run it will debug the wrong thing.")
        return 1

    if loose:
        print(f"WARN: {len(loose)} needle(s) match only after whitespace "
              f"normalisation — indentation moved under them.\n")
        for name, line, text in loose:
            shown = text if len(text) <= 88 else text[:85] + "..."
            print(f"  {name}:{line}")
            print(f"    {shown!r}")
        print("\nA gate comparing against the raw source will FAIL on these.")
        print("Run it: make test-fast.\n")

    print(f"PASS gate_greps: {checked} source needles across {len(gates)} gates still match")
    if scoped_count:
        print(f"  note: {scoped_count} of them assert a POSITION in the source "
              f"(e.g. `not in compact[idx:idx+220]`).")
        print("  Only their existence was checked here; run the gate itself "
              "to confirm the position.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
