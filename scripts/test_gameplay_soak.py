#!/usr/bin/env python3
"""Long-run gameplay invariant soak (D4): sustained play must never violate
the basic rules, across many frames and several levels.

Single-frame gates catch a specific instant; this drives REAL play — an
in-flight ball bouncing through the brick field — for a long horizon on
multiple levels and asserts, at sampled checkpoints, that the invariants
hold throughout:

  per checkpoint:
    - the ball's CENTRE is never inside a solid brick cell (no tunnel /
      stuck-in-brick),
    - the ball never escapes the playfield walls (x in [8,244], y in [8,191]);
  across checkpoints (monotonicity):
    - bricks_quantity only ever DECREASES (a brick can't un-break),
    - score only ever INCREASES (no negative scoring / rollover bug).

These hold whether the ball is bouncing or has dropped+respawned, so the
soak is robust without pinning the ball in play. Oracle-free. Catches
accumulation/over-time regressions a single capture can't.

    make test-gameplay-soak
"""
from __future__ import annotations

import concurrent.futures
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_visual import run_qemu, boot_until_gameplay

FLOPPY = os.environ.get("BATTY_TEST_FLOPPY", "build/batty-test.img")
OUT = Path("build/test_gameplay_soak")
TAIL = "020CEEF008076C4E020C0000008C"
BAT_OBJECT = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
BOOT_WAIT = os.environ.get("BATTY_BOOT_WAIT", "8")
INNER_JOBS = int(os.environ.get("BATTY_INNER_JOBS",
                                str(min(8, os.cpu_count() or 4))))

LEVELS = [1, 3, 5, 9]                       # variety, incl. edge-metal L5/L9
CHECKPOINTS = [30, 60, 90, 120, 150]        # ball stays in play through ~150f
# In-flight ball near field centre (x=0x78=120, y=0x50=80) up-right
# (dir=0x38) at speed 4 — bounces through the bricks/walls for a sustained
# run. Bytes: set=02 num=00 x=78 xhi=00 y=50 yhi=00 dir=38 spd=04 + tail.
BALL = "0200780050003804" + TAIL


def case_floppy(idx: int) -> str:
    stem = FLOPPY[:-4] if FLOPPY.endswith(".img") else FLOPPY
    return f"{stem}-c{idx}.img"


def probe(level: int, frame: int, idx: int) -> dict:
    floppy = case_floppy(idx)
    serial = OUT / f"serial{idx}.txt"
    Path(floppy).unlink(missing_ok=True)
    # BATTY_SERIAL_PROBE: the port emits a COM1 marker at the $BA83 pause and
    # at the FRAME_PROBE quit, so the wake/read are deterministic even when
    # the parallel fan-out oversubscribes cores (wall-clock waits missed the
    # wake here, reading the pre-gameplay seed state -> false violations).
    env = (f"BATTY_LEVEL={level} BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
           f"BATTY_REPLAY_PROBE=1 BATTY_SERIAL_PROBE=1 BATTY_REPLAY_RANDOM=8E49 "
           f"BATTY_REPLAY_BAT_OBJECT={BAT_OBJECT} BATTY_REPLAY_BALL_STUCK=0 "
           f"BATTY_REPLAY_BALL_OBJECT={BALL} BATTY_FRAME_PROBE={frame}")
    subprocess.run(f"BATTY_TEST_FLOPPY={floppy} {env} make {floppy}", shell=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)

    def drive():
        subprocess.run(["mdel", "-i", floppy, "::PROBE.TXT"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        run_qemu(Path(floppy),
                 [f"SLEEP {BOOT_WAIT}", "WAITSERIAL 1 90",   # boot done, at pause
                  "sendkey ret", "WAITSERIAL 2 90"],         # frame-N quit marker
                 OUT / f"q{idx}.log", serial_path=serial)
    p = boot_until_gameplay(Path(floppy), drive, label=f"L{level} f{frame}")
    return {
        "level": level, "frame": frame,
        "ball": bytes.fromhex(p.get("object_ball_1", "")),
        "grid": bytes.fromhex(p.get("current_level_copy", "")),
        "bricks": int(p.get("bricks_quantity", "FF"), 16),
        "score": int((p.get("score", "0") or "0").lstrip("0") or "0"),
    }


def centre_in_solid(ball: bytes, grid: bytes):
    if len(ball) != 22 or (ball[0] & 0x80):
        return None
    cx, cy = ball[2] + 4, ball[4] + 3
    if cy < 32 or cy >= 32 + 12 * 8 or cx < 8 or cx >= 8 + 15 * 16:
        return None
    r, c = (cy - 32) // 8, (cx - 8) // 16
    v = grid[r * 15 + c]
    return (r, c, v) if (v & 0x80) == 0 else None


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    subprocess.run(["make", "build/batty-test.exe"],
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT, check=True)

    work = [(lv, f) for lv in LEVELS for f in CHECKPOINTS]
    results = [None] * len(work)
    with concurrent.futures.ThreadPoolExecutor(max_workers=INNER_JOBS) as ex:
        futs = {ex.submit(probe, lv, f, i): i for i, (lv, f) in enumerate(work)}
        for fut in concurrent.futures.as_completed(futs):
            results[futs[fut]] = fut.result()

    fails = []
    by_level = {lv: [] for lv in LEVELS}
    lvl_fail = {lv: 0 for lv in LEVELS}
    for r in results:
        if not r or len(r["grid"]) != 180 or len(r["ball"]) != 22:
            lv = r["level"] if r else 0
            fails.append(f"L{lv} f{r['frame'] if r else '?'}: bad probe")
            lvl_fail[lv] = lvl_fail.get(lv, 0) + 1
            continue
        by_level[r["level"]].append(r)
        b = r["ball"]
        hit = centre_in_solid(b, r["grid"])
        if hit:
            fails.append(f"L{r['level']} f{r['frame']}: ball centre in SOLID "
                         f"r{hit[0]} c{hit[1]} (0x{hit[2]:02X})")
            lvl_fail[r["level"]] += 1
        if not (b[0] & 0x80) and (b[2] < 8 or b[2] > 244 or b[4] < 8 or b[4] > 191):
            fails.append(f"L{r['level']} f{r['frame']}: ball ESCAPED "
                         f"x={b[2]} y={b[4]}")
            lvl_fail[r["level"]] += 1

    # monotonicity per level (sorted by frame)
    for lv, rs in by_level.items():
        rs.sort(key=lambda r: r["frame"])
        for a, b in zip(rs, rs[1:]):
            if b["bricks"] > a["bricks"]:
                fails.append(f"L{lv}: bricks rose {a['bricks']}->{b['bricks']} "
                             f"(f{a['frame']}->f{b['frame']})")
                lvl_fail[lv] += 1
            if b["score"] < a["score"]:
                fails.append(f"L{lv}: score fell {a['score']}->{b['score']} "
                             f"(f{a['frame']}->f{b['frame']})")
                lvl_fail[lv] += 1
        if rs:
            print(f"  L{lv}: {len(rs)} checkpoints, bricks "
                  f"{rs[0]['bricks']}->{rs[-1]['bricks']}, "
                  f"score {rs[0]['score']}->{rs[-1]['score']} "
                  f"[{'PASS' if lvl_fail[lv] == 0 else 'FAIL'}]")

    print()
    if fails:
        print(f"FAIL gameplay_soak: {len(fails)} invariant violation(s)")
        for f in fails[:20]:
            print(f"    {f}")
        return 1
    print(f"PASS gameplay_soak: {len(work)} checkpoints over {len(LEVELS)} "
          f"levels — ball never in a brick / never escaped, bricks only fell, "
          f"score only rose")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
