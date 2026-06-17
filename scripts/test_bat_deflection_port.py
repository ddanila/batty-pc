#!/usr/bin/env python3
"""Port-side bat-deflection regression (LAB1F port in step_ball).

Seeds the DOS port with a coherent ball dropped just above the bat
(y=0x96) heading down at a chosen incoming direction, steps it to a
post-contact frame, and asserts the deflected direction matches the
ground truth captured from the original (ZEsarUX) by
scripts/capture_bat_deflection.py. See notes/bat-deflection.md for the
decode and the captured tables.

Because the port's ball motion is byte-exact, seeding the same descending
ball reaches the same contact pixel and so must produce the same outgoing
dir as the Spectrum. This is the end-to-end gate on the LAB1F port. The
cases span multiple incoming dirs (different dir_to_dxdy quadrants) and
bat positions (different threshold zones / LAC0A columns, incl. the bit2
double-reflect zones).

ZEsarUX-free (QEMU only). Exit 0 = all PASS.
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import boot_until_gameplay

FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-test.img")
# Normal bat at x=116 width 28; ball 8 wide, dropped at y=0x96.
BAT_OBJECT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
BALL_BASE = "0200800096000C03020CEEF008078096020C0000008C"

# (incoming_dir, start_x, probe_frame, expected_out_dir) — the captured
# Spectrum ground truth (scripts/capture_bat_deflection.py). probe_frame
# is a few frames past contact; the dir is stable afterward. NO-FLIP
# (ball misses the bat) seeds are omitted.
CASES = [
    # dir 0x0C (down-right, q=0x00): zones across the bat (incl. reflect).
    (0x0C, 104, 8, 0x28),
    (0x0C, 112, 8, 0x2C),
    (0x0C, 120, 8, 0x34),
    (0x0C, 128, 8, 0x38),
    (0x0C, 136, 9, 0x38),
    # dir 0x08 (down-right 45, q=0x00, LAC0A column 1).
    (0x08, 100, 10, 0x28),
    (0x08, 108, 10, 0x38),
    (0x08, 116, 10, 0x38),
    (0x08, 124, 10, 0x3C),
    # dir 0x14 (down-left, q=0x10, LAC0A column 3).
    (0x14, 124, 10, 0x28),
    (0x14, 132, 10, 0x2C),
    (0x14, 140, 10, 0x34),
    (0x14, 148, 10, 0x38),
]


def ball_object_hex(start_x: int, indir: int) -> str:
    a = bytearray(bytes.fromhex(BALL_BASE))
    a[2] = start_x        # x pixel
    a[6] = indir          # incoming direction
    a[14] = start_x       # prev-x (avoid stale-erase glitch)
    return a.hex().upper()


# MAGNET/CATCH bonus ($03 at bat +$14): a normal bat catches the ball at
# a quantized offset. Bat hex below has +$14 (byte 20) = 0x03. The caught
# ball must rest at x = bat_x(116) + (ball_x-116 & 0xFC clamped 0x18).
# Captured: center drop -> caught x=132 (offset 0x10).
BAT_OBJECT_CATCH = "01017400AD000000040DEFAE1C0A74AD040DF0000380"
CATCH_CASES = [
    # (start_x, incoming_dir, probe_frame, expected_rest_x)
    (126, 0x0C, 11, 132),
]


import concurrent.futures

# Cases are independent, so run them concurrently — each on its OWN floppy
# (derived from FLOPPY) so the per-case builds/boots don't collide. This is
# the dominant gate's wall-clock (14 boots); intra-gate parallelism cuts it
# from ~serial to ~serial/INNER_JOBS. Capped modestly so it doesn't badly
# oversubscribe when this gate itself runs under run_gates_parallel.py.
# High concurrency standalone; modest under run_gates_parallel.py (where
# BATTY_TEST_FLOPPY is set and many gates already run concurrently) to avoid
# oversubscribing cores — too many concurrent QEMUs run slower than real
# time, which breaks the harness's wall-clock frame waits.
INNER_JOBS = int(os.environ.get(
    "BATTY_INNER_JOBS",
    "2" if os.environ.get("BATTY_TEST_FLOPPY") else str(min(7, os.cpu_count() or 4))))


def _case_paths(idx):
    stem = FLOPPY[:-4] if FLOPPY.endswith(".img") else FLOPPY
    return f"{stem}-c{idx}.img", f"build/tl_bat_{idx}"


def _probe_ball(start_x, indir, frame, bat_obj, floppy, outdir):
    """Boot a per-case floppy, wake, read object_ball_1 — with a retry if the
    wake key was missed (probe_phase=init seed write, slow-boot race);
    boot_until_gameplay re-boots until it sees a probe_phase=play checkpoint."""
    Path(floppy).unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_LAFFC=1 BATTY_REPLAY_PROBE=1 BATTY_REPLAY_RANDOM=8E49 "
        f"BATTY_REPLAY_BAT_OBJECT={bat_obj} BATTY_REPLAY_BALL_STUCK=0 "
        f"BATTY_REPLAY_BALL_OBJECT={ball_object_hex(start_x, indir)} "
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    # BATTY_TEST_FLOPPY makes the Makefile's TEST_FLOPPY_OUT == this per-case
    # path (and derive a unique AUTOEXEC scratch), so `make {floppy}` builds it.
    subprocess.run(f"BATTY_TEST_FLOPPY={floppy} {env} make {floppy}", shell=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def drive():
        subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                        "--floppy", floppy, "--frames", str(frame), "--wait-key",
                        "--out", outdir], stdout=subprocess.DEVNULL)

    probe = boot_until_gameplay(floppy, drive,
                                label=f"bat start_x={start_x} in=0x{indir:02X}")
    hexs = probe.get("object_ball_1", "")
    return bytes.fromhex(hexs) if hexs else None


def _run_dir_case(idx, indir, sx, frame, exp):
    floppy, outdir = _case_paths(idx)
    b = _probe_ball(sx, indir, frame, BAT_OBJECT, floppy, outdir)
    got = b[6] if b else -1
    ok = got == exp
    return ok, (f"  in=0x{indir:02X} start_x={sx:3d} frame={frame}: "
                f"out_dir=0x{got:02X} [{'PASS' if ok else f'FAIL exp 0x{exp:02X}'}]")


def _run_catch_case(idx, sx, indir, frame, exp_x):
    floppy, outdir = _case_paths(idx)
    b = _probe_ball(sx, indir, frame, BAT_OBJECT_CATCH, floppy, outdir)
    got_x = b[2] if b else -1
    ok = got_x == exp_x
    return ok, (f"  catch in=0x{indir:02X} start_x={sx}: rest_x={got_x} "
                f"[{'PASS' if ok else f'FAIL exp {exp_x}'}]")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dirs", default="",
                    help="comma-separated incoming dirs to verify "
                         "(hex ok); default = all")
    args = ap.parse_args()
    want = {int(t, 0) for t in args.dirs.split(",") if t.strip()}
    cases = [c for c in CASES if not want or c[0] in want]

    # Pre-build the shared TEST_EXE once so the concurrent per-case `make`
    # calls don't race on build/main-test.obj (they then just assemble their
    # own floppy against the up-to-date EXE).
    subprocess.run(["make", "build/batty-test.exe"],
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT, check=True)

    # Build the parallel work list: dir cases + (when unfiltered) the catch.
    work = []
    for (indir, sx, frame, exp) in cases:
        work.append(("dir", (indir, sx, frame, exp)))
    if not want:
        for (sx, indir, frame, exp_x) in CATCH_CASES:
            work.append(("catch", (sx, indir, frame, exp_x)))

    results = [None] * len(work)
    with concurrent.futures.ThreadPoolExecutor(max_workers=INNER_JOBS) as ex:
        futs = {}
        for idx, (kind, payload) in enumerate(work):
            if kind == "dir":
                futs[ex.submit(_run_dir_case, idx, *payload)] = idx
            else:
                futs[ex.submit(_run_catch_case, idx, *payload)] = idx
        for fut in concurrent.futures.as_completed(futs):
            results[futs[fut]] = fut.result()

    fails = 0
    for ok, line in results:
        print(line)
        if not ok:
            fails += 1

    if fails == 0:
        print(f"PASS bat_deflection: port LAB1F matches Spectrum ground "
              f"truth at {len(work)} cases (incl. MAGNET catch)")
    else:
        print(f"FAIL bat_deflection: {fails}/{len(work)} cases diverged")
    return fails


if __name__ == "__main__":
    sys.exit(main())
