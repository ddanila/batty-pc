#!/usr/bin/env python3
"""Double Play draws a court divider, and nothing else changes.

`LBE8B_10`, in `game_screen_draw_to_buffer`, immediately before the
1UP/HI/2UP sprites:

    LD A,(game_mode) / CP $02 / JR NZ,LBE8B_11
    LD IX,object_separator / CALL ix_buf_addr_calc / CALL print_obj_to_buff

and `object_separator` ($9BDC) carries the rest:

    DEFB $03,$05,$7D,$00,$A9,...
          set  spr   x     y

sprite set 3 (`gfx_screen_elements`) index 5 = `spr_separator` ($7A2A),
2 bytes wide and $18 rows, at x=$7D=125, y=$A9=169. It sits down in the
bat band — it is a marker between the two bats' halves, not a
full-height wall, which is not what "court split" suggests.

### The assertion is an A/B, and it is two-sided

  control  BATTY_GAME_MODE=0 (1 Player)
  subject  BATTY_GAME_MODE=2 (Double Play)

Same level, same seed. Every differing pixel must fall inside the
sprite's own rectangle, and there must BE some. The second half is what
makes it a parity check rather than a smoke test: `render_separator` is
called from the full-scene composers just above `render_hud_to_buff`,
where a wrong position or a missing mask would leak into the bat band
or the HUD, and a one-sided "the divider is present" assertion would
pass anyway.

Measured on the port: 98 differing pixels, x 125..136, y 169..191. The
sprite is 16 px wide but its right 4 columns are blank in the mask, and
the last row is empty — hence 136 and 191 rather than 140 and 192.
"""
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_visual import ppm_inner_to_indices, run_qemu   # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = Path(os.environ.get("BATTY_TEST_FLOPPY", "build/batty-sep.img"))
OUT = Path("build/double_play_sep")

# spr_separator at object_separator's coordinates: x=$7D, 2 bytes wide;
# y=$A9, $18 rows. The window below is the sprite's full extent, and
# nothing outside it may differ.
X0, X1 = 0x7D, 0x7D + 2 * 8 - 1        # 125..140
Y0, Y1 = 0xA9, 0xA9 + 0x18 - 1         # 169..192


def capture(mode: str):
    (ROOT / FLOPPY).unlink(missing_ok=True)
    env = (f"BATTY_TEST_FLOPPY={FLOPPY} BATTY_START_LEVEL=1 "
           f"BATTY_GAME_MODE={mode} BATTY_NOSOUND=1")
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ppm = OUT / f"mode{mode}.ppm"
    run_qemu(ROOT / FLOPPY, ["SLEEP 9.0", f"screendump {ppm}"],
             OUT / f"mode{mode}.log")
    return ppm_inner_to_indices(ppm)


def main() -> int:
    import shutil
    shutil.rmtree(OUT, ignore_errors=True)
    OUT.mkdir(parents=True, exist_ok=True)

    one = capture("0")
    two = capture("2")
    if len(one) != len(two) or not one:
        raise SystemExit("FAIL: captures differ in size or are empty")

    diff = [(i % 256, i // 256) for i in range(len(one)) if one[i] != two[i]]
    if not diff:
        raise SystemExit(
            "FAIL: Double Play looks identical to 1 Player. LBE8B_10 draws "
            "object_separator when game_mode == $02; render_separator is "
            "either not called or its game_mode test is wrong.")

    outside = [(x, y) for x, y in diff
               if not (X0 <= x <= X1 and Y0 <= y <= Y1)]
    if outside:
        xs = sorted({x for x, _ in outside})
        ys = sorted({y for _, y in outside})
        raise SystemExit(
            f"FAIL: {len(outside)} pixels change OUTSIDE the divider's "
            f"rectangle x {X0}..{X1}, y {Y0}..{Y1} — at x {xs[:8]}, "
            f"y {ys[:8]}.\nThe separator is drawn just above the HUD in "
            f"the full-scene composers, so a wrong position or a lost "
            f"mask leaks into the bat band or the score row.")

    xs = [x for x, _ in diff]
    ys = [y for _, y in diff]
    print(f"  {len(diff)} pixels differ, all inside the divider: "
          f"x {min(xs)}..{max(xs)}, y {min(ys)}..{max(ys)}")
    print("PASS double_play_separator: mode $02 draws the court divider "
          "and changes nothing else")
    return 0


if __name__ == "__main__":
    sys.exit(main())
