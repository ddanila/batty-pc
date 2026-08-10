#!/usr/bin/env python3
"""Apply a mutation, run a test target, report whether it was caught.

Mutation testing found five real gaps in this repo's own tests — a gate
that skipped a third of the gates, two suites that tested the shape of a
value and not the value, and zxvga never checking where a sprite lands.
It also went wrong three separate ways in the process, twice producing a
confident but false result. This exists so it goes wrong less.

The three failure modes, all of which this handles:

  STALE BINARY, same-second.  Restore a source file within the same
      second as the previous build and make sees an unchanged timestamp,
      reruns the OLD binary, and the mutation looks caught.

  STALE BINARY, wrong name.  `make test-video` builds `build/test_zxvga`.
      Deleting "build/test_video" deletes nothing, so every run used a
      stale binary and a real gap was reported as caught. Only a restored
      source STILL failing — which cannot happen — exposed it. This
      script deletes every build/test_* FILE rather than guessing which
      one a target builds. Directories are left alone: the QEMU gates
      keep their captures there.

  SILENT NO-OP.  A substitution that matches nothing leaves the source
      unmutated, the test passes, and it reads as "not caught" — the
      most dangerous outcome, because it looks like a finding. The file
      is compared before and after and a no-op is an error, not a result.

  MUTATION THAT DOES NOT BUILD.  The target's exit status is the only
      signal, and a compile error fails it exactly as a detection does.
      A shell loop that let `\&\&` into a replacement string produced
      three "caught" results on 2026-08-10, two of them compile errors,
      and one went into a commit message as covered when it was not.
      The build is now run SEPARATELY first, and a failure there is an
      error rather than a result.

Usage:
    scripts/mutate.py <file> <find> <replace> <make-target> [label]

Exit status is 0 when the mutation was CAUGHT (the desirable outcome),
1 when it survived, 2 when the mutation could not be applied.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def clear_test_binaries() -> int:
    """Remove every build artefact a test target could reuse.

    Three kinds, and missing any one produces a confident false result:

      build/test_*        the HOST suites' binaries.
      build/*.obj         the DOS objects. A module change rebuilds its
                          own .obj, but if the link happens inside the
                          same filesystem second the EXE is left alone.
      build/batty*.exe    the DOS EXEs the QEMU gates boot. This was
                          missing, and it is the one that matters most:
                          mutating src/physics.cpp changed
                          physics-test.obj (verified by md5) and left
                          batty-test.exe byte-identical, so the gates ran
                          the ORIGINAL code and every result was
                          meaningless.

    Directories are left alone — the QEMU gates keep captures there, and
    test-gate-freshness makes each gate clear its own.
    """
    build = ROOT / "build"
    if not build.is_dir():
        return 0
    n = 0
    for pattern in ("test_*", "*.obj", "batty*.exe"):
        for p in build.glob(pattern):
            if p.is_file():
                p.unlink()
                n += 1
    return n


BUILD_ERROR_MARKERS = (
    "error:",            # clang / gcc
    "Error!",            # Open Watcom
    "syntax error",
    "undeclared",
)


SANITIZER_MARKERS = (
    "AddressSanitizer",
    "LeakSanitizer",
    "runtime error:",        # UBSan
)


def looks_like_a_sanitizer_report(out: str) -> bool:
    """A sanitiser abort is a DETECTION, not a broken build.

    It has to be tested first: ASan prints `ERROR: AddressSanitizer:`,
    which contains the `error:` marker below. Without this check every
    sanitiser catch under `make test-asan` is reported as "the mutated
    source did not build" — which is how the first run of the ASan
    target scored its own successes as tooling failures.
    """
    return any(m in out for m in SANITIZER_MARKERS)


def looks_like_a_build_failure(out: str) -> bool:
    """Did the target fail to COMPILE rather than fail its assertions?

    Text-matching, because the alternative — a separate build step — has
    to know which target builds what, and the wrong-name mistake in the
    header above is exactly what that costs. A false positive here turns
    a real detection into an ERROR, which is loud and recoverable; a
    false negative is the silent kind this exists to stop.
    """
    low = out.lower()
    return any(m.lower() in low for m in BUILD_ERROR_MARKERS)


def main() -> int:
    if len(sys.argv) < 5:
        print(__doc__)
        return 2
    path, find, replace, target = sys.argv[1:5]
    label = sys.argv[5] if len(sys.argv) > 5 else f"{Path(path).name}: {find}"

    f = ROOT / path
    original = f.read_text()
    if find not in original:
        print(f"ERROR [{label}]: the text to replace is not in {path}.")
        print("  Nothing was mutated, so a passing test would mean nothing.")
        return 2
    mutated = original.replace(find, replace, 1)
    if mutated == original:
        print(f"ERROR [{label}]: substitution changed nothing")
        return 2

    build_failed = False
    try:
        f.write_text(mutated)
        clear_test_binaries()
        # Build first, on its own, so a compile error cannot masquerade
        # as a detection. `-n` is no good here (it would not compile at
        # all), so the target is run once with output captured and the
        # log inspected only if it fails.
        r = subprocess.run(["make", target], cwd=ROOT,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        out = r.stdout.decode("utf-8", "replace")
        caught = r.returncode != 0
        if (caught and not looks_like_a_sanitizer_report(out)
                and looks_like_a_build_failure(out)):
            build_failed = True
    finally:
        f.write_text(original)
        clear_test_binaries()

    if build_failed:
        print(f"ERROR [{label}]: the mutated source did not BUILD, so the "
              f"target's failure says nothing about coverage.")
        print("  Check the replacement text — a shell loop that lets "
              "backslashes into it is the usual cause.")
        return 2

    print(f"{'caught  ' if caught else 'SURVIVED'}  {label}")
    if not caught:
        print(f"           `make {target}` passed with {path} mutated:")
        print(f"             {find.strip()}  ->  {replace.strip()}")
        print(f"           Either the tests do not cover it, or it is an "
              f"equivalent mutant. Decide which, and say so.")
    return 0 if caught else 1


if __name__ == "__main__":
    raise SystemExit(main())
