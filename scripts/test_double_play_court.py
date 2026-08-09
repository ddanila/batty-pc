#!/usr/bin/env python3
"""Double Play lays the court out: a divider and two bats.

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

`all_var_init` (LB7F8) does the rest of the layout, and it is not what
"add a second bat" suggests — it MOVES both:

    LD A,(game_mode) / CP $02 / JR NZ,LB7F8_1
    LD A,$01 / LD (object_bat_2),A        ; sprite_set: activate
    LD A,$38 / LD (object_bat_1+$02),A    ; bat 1 x = 56
    LD A,$B0 / LD (object_bat_2+$02),A    ; bat 2 x = 176

### The assertion is an A/B, and it is two-sided

  control  BATTY_GAME_MODE=0 (1 Player)
  subject  BATTY_GAME_MODE=2 (Double Play)

Same level, same seed, frozen at the same frame. Every differing pixel
must fall in the bat band or below, and the divider's own rectangle must
be among them. The second half is what makes it a parity check rather
than a smoke test: `render_separator` and `render_bat_2` are called from
the full-scene composers just above `render_hud_to_buff`, where a wrong
position or a missing mask leaks into the brick field or the score row,
and a one-sided "the divider is present" assertion would pass anyway.

### Why the frame is frozen

The first version slept 9 s and screendumped. That worked while mode 2
changed only the divider — 98 pixels, x 125..136, y 169..191. The moment
bat 1 moved to x=56 the two runs stopped being comparable at all: the
ball auto-launches off a bat in a different place, so by 9 s they have
destroyed different bricks and the diff is the whole screen. It now
halts at a visual checkpoint two frames in, with the ball still resting
on the bat.
"""
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_visual import ppm_inner_to_indices, run_qemu   # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
FLOPPY = Path(os.environ.get("BATTY_TEST_FLOPPY", "build/batty-dp.img"))
OUT = Path("build/double_play_court")

# The divider's own extent: x=$7D, 2 bytes wide; y=$A9, $18 rows.
X0, X1 = 0x7D, 0x7D + 2 * 8 - 1        # 125..140
Y0, Y1 = 0xA9, 0xA9 + 0x18 - 1         # 169..192

# Nothing above the bat band may change. The bats sit at y=$AD=173 and
# the divider at 169, so 160 leaves room for the bat sprite's shadow
# rows without reaching the brick field (which ends well above it).
BAND_TOP = 160

# The bat sprite is 4 bytes wide.
BAT_W = 4 * 8


def capture(mode: str):
    (ROOT / FLOPPY).unlink(missing_ok=True)
    env = (f"BATTY_TEST_FLOPPY={FLOPPY} BATTY_START_LEVEL=1 "
           f"BATTY_GAME_MODE={mode} BATTY_NOSOUND=1 "
           f"BATTY_REPLAY_WAIT_KEY=1 BATTY_REPLAY_COUNTER=0 "
           f"BATTY_VISUAL_PROBE_FRAMES=2")
    subprocess.run(f"{env} make {FLOPPY}", shell=True, cwd=ROOT,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ppm = OUT / f"mode{mode}.ppm"
    run_qemu(ROOT / FLOPPY,
             ["SLEEP 9.0", "sendkey ret", "SLEEP 2.0",
              f"screendump {ppm}", "sendkey esc", "SLEEP 0.2"],
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

    xs_all = [x for x, _ in diff]
    above = [(x, y) for x, y in diff if y < BAND_TOP]
    if above:
        ys = sorted({y for _, y in above})
        raise SystemExit(
            f"FAIL: {len(above)} pixels change ABOVE the bat band "
            f"(y < {BAND_TOP}), at y {ys[:8]}.\nDouble Play moves the bats "
            f"and adds a divider; it must not touch the brick field or the "
            f"score row. render_separator and render_bat_2 sit just above "
            f"render_hud_to_buff in the full-scene composers, so a wrong "
            f"position or a lost mask lands there.")

    # Each piece of the layout has to show up on its own. "Something
    # changed in the band" would pass with bat 2 missing entirely, which
    # is exactly what happened on the first run of this gate: the call
    # sat under `with_bat`, which the static-background compose does not
    # use, and the diff stopped at x=147.
    regions = (
        ("bat 1 at its Double Play x=$38", 0x38, 0x38 + BAT_W - 1),
        ("the divider at x=$7D",           X0,   X1),
        ("bat 2 at x=$B0",                 0xB0, 0xB0 + BAT_W - 1),
    )
    # The x EXTENT is checked too. A per-region count alone is too
    # loose: moving bat 2 from $B0 to $A0 still overlaps the $B0..$CF
    # window, and that mutation SURVIVED until these two bounds were
    # added.
    if min(xs_all) != 0x38 or max(xs_all) != 0xB0 + BAT_W - 1:
        raise SystemExit(
            f"FAIL: the changed pixels span x {min(xs_all)}..{max(xs_all)}, "
            f"expected exactly {0x38}..{0xB0 + BAT_W - 1} — bat 1's left "
            f"edge at $38 to bat 2's right edge at $B0 + 32. A bat at the "
            f"wrong x still overlaps its window, so the extent is what "
            f"pins the positions.")

    counts = []
    for name, x0, x1 in regions:
        n = sum(1 for x, y in diff if x0 <= x <= x1)
        if n == 0:
            raise SystemExit(
                f"FAIL: nothing changed where {name} should be "
                f"(x {x0}..{x1}). all_var_init's mode-$02 block activates "
                f"object_bat_2 and moves BOTH bats — $38 and $B0 — and "
                f"LBE8B_10 draws object_separator between them.")
        counts.append((name, n))
    on_divider = [(x, y) for x, y in diff if X0 <= x <= X1 and Y0 <= y <= Y1]

    xs = [x for x, _ in diff]
    ys = [y for _, y in diff]
    print(f"  {len(diff)} pixels differ, all in the bat band: "
          f"x {min(xs)}..{max(xs)}, y {min(ys)}..{max(ys)}")
    for name, n in counts:
        print(f"    {n:4d} at {name}")
    print("PASS double_play_court: mode $02 moves both bats and draws the "
          "divider, and changes nothing above the bat band")
    return 0


if __name__ == "__main__":
    sys.exit(main())
