#!/usr/bin/env python3
"""Enemy/sprite attribute-parity gate (known-bugs #7).

The original draws moving objects with print_obj_to_buff ($B82C), which
blits sprite PIXELS only -- it never calls print_sprite_attrib ($B656),
so a flying enemy leaves every cell's ATTRIBUTE untouched. Over open
texture the cell keeps bg_attr; over a brick it keeps the BRICK's attr, so
the sprite renders in ZX colour-clash with whatever is underneath (the
bird over a red brick shows red, not the playfield background). Verified
against the ZEsarUX oracle: the bird's cell attrs are byte-identical to the
static GT across the fly-over (build/orig_flyover, notes/bird-render-parity.md).

The port used to force bg_attr onto the enemy's whole bounding box
(blit_sprite_attrs_to_buff(enemy..., bg_attr)) -- recolouring the brick
cells the bird flew over to the playfield background. This gate locks the
original's behaviour with a pure port-internal invariant (no ZEsarUX
needed):

    under a flying enemy, attr_buff == bg_attr_buff (the static-background
    attr snapshot) for every cell in the sprite's footprint.

It reuses the deterministic L3 fresh-bird descend (test_enemy_descend): the
alien spawns at x=168, y=1 and slides down 1 px/frame, so by the probed
frames its 24x16 footprint overlaps L3's row-0/1 brick cells (attr 0x05),
where a bg_attr recolour (0x45) is plainly wrong.

    make test-enemy-attr-parity
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = "build/batty-test.img"

FRESH_ENEMY = "0900A80001001001030FDA35180CA801030FF0701000"
BAT_OBJECT  = "01017400AD000000040DEFAE1C0A74AD040DF0008380"
BALL_OBJECT = "02006C004E001F03020CEEF008076C4E020C0000008C"

# Probe frames during the descend; at frame f the bird is at y=f+1, so its
# 16-px-tall footprint overlaps the L3 brick rows 0/1 (y 32..47 -> char
# rows 0/1) once its top is high enough. Both frames keep it over bricks.
FRAMES = [3, 6]
ENEMY_W, ENEMY_H = 24, 16


def probe(frame: int) -> dict:
    Path(ROOT / FLOPPY).unlink(missing_ok=True)
    rep = ROOT / "build/PROBE_attr.txt"
    rep.unlink(missing_ok=True)
    env = (
        f"BATTY_LEVEL=3 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 "
        f"BATTY_REPLAY_PROBE=1 BATTY_REPLAY_RANDOM=8E49 "
        f"BATTY_REPLAY_BAT_OBJECT={BAT_OBJECT} BATTY_REPLAY_BALL_STUCK=0 "
        f"BATTY_REPLAY_BALL_OBJECT={BALL_OBJECT} "
        f"BATTY_REPLAY_ENEMY_OBJECT={FRESH_ENEMY} "
        f"BATTY_VISUAL_PROBE_FRAMES={frame}"
    )
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    subprocess.run([sys.executable, "scripts/capture_frame_timeline.py",
                    "--floppy", FLOPPY, "--frames", str(frame), "--wait-key",
                    "--out", "build/tl_attr"], cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-n", "-i", FLOPPY, "::PROBE.TXT", str(rep)],
                   cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not rep.exists():
        return {}
    txt = rep.read_text()
    out = {}
    for key in ("object_enemy", "attr_buff", "bg_attr_buff"):
        m = re.search(rf"{key}=([0-9A-Fa-f]+)", txt)
        if m:
            out[key] = m.group(1)
    return out


def footprint_cells(en: bytes):
    ex, ey = en[2], en[4]
    c0, c1 = ex // 8, (ex + ENEMY_W - 1) // 8
    r0, r1 = max(ey // 8, 0), (ey + ENEMY_H - 1) // 8
    return [(r, c) for r in range(r0, min(r1, 23) + 1)
            for c in range(c0, min(c1, 31) + 1)]


def main() -> int:
    fails = 0
    for frame in FRAMES:
        p = probe(frame)
        if not {"object_enemy", "attr_buff", "bg_attr_buff"} <= p.keys():
            print(f"  frame {frame}: missing probe rows [FAIL]")
            fails += 1
            continue
        en = bytes.fromhex(p["object_enemy"])
        ab = bytes.fromhex(p["attr_buff"])
        bg = bytes.fromhex(p["bg_attr_buff"])
        if en[0] != 0x09:
            print(f"  frame {frame}: enemy not the seeded bird "
                  f"(set=0x{en[0]:02X}) [FAIL]")
            fails += 1
            continue
        diffs = [(r, c, ab[r * 32 + c], bg[r * 32 + c])
                 for (r, c) in footprint_cells(en)
                 if ab[r * 32 + c] != bg[r * 32 + c]]
        over_bricks = any(bg[r * 32 + c] not in (0x45, 0x47)
                          for (r, c) in footprint_cells(en))
        tag = "PASS" if not diffs else "FAIL"
        print(f"  frame {frame}: bird x={en[2]} y={en[4]} "
              f"footprint over bricks={over_bricks}  "
              f"attr!=bg cells={len(diffs)} [{tag}]")
        for (r, c, a, b) in diffs[:8]:
            print(f"      r{r} c{c}: attr=0x{a:02X} expected(bg/brick)=0x{b:02X}")
        if diffs:
            fails += 1

    print()
    if fails:
        print(f"FAIL enemy_attr_parity: a flying enemy recoloured "
              f"{fails} frame(s)' cells (should leave brick/bg attrs intact)")
        return 1
    print("PASS enemy_attr_parity: flying enemy leaves cell attrs unchanged "
          "(ZX colour-clash, matches the original)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
