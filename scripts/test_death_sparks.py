#!/usr/bin/env python3
"""Regression checks for the bat death spark fanout.

The original LBC10 routine rewrites ten object slots as anim_spark:
directions start at $1B and advance by $05, X starts at
bat_x + bat_body_width/2 - 12 and advances by 3 px per spark, Y is
$AE, speed is 2, and handling_spark reflects through bounce_wall.
"""
from pathlib import Path


SRC = Path("src/main.cpp")


def simulate_original(xs, frames=36, direction_start=0x03):
    """Small model of LBC10 + handling_spark, used to document the right
    wall reflection that was easy to get wrong in the port."""
    sin = [0xFF, 0xFD, 0xFA, 0xF4, 0xE6, 0xE0, 0xD4, 0xC5,
           0xB4, 0xA1, 0x8D, 0x78, 0x61, 0x4A, 0x31, 0x18, 0x00]

    def dxdy(direction, speed):
        q = direction & 0x30
        d = direction & 0x0F
        yy = sin[d]
        xx = sin[16 - d]
        if q == 0x00:
            x, y = xx, yy
        elif q == 0x10:
            x, y = yy, -xx
        elif q == 0x20:
            x, y = -xx, -yy
        else:
            x, y = -yy, xx
        return x * speed, y * speed

    out = []
    for start_x in xs:
        x = start_x << 8
        y = 0xAE << 8
        direction = direction_start
        bounced = False
        for _ in range(frames):
            dx, dy = dxdy(direction, 2)
            x = (x + dx) & 0xFFFF
            y = (y + dy) & 0xFFFF
            xi = x >> 8
            yi = y >> 8
            if yi >= 0xC0:
                break
            if xi < 0x08:
                xi = 0x08
                x = xi << 8
                direction = ((direction ^ 0x1F) + 1) & 0x3F
            elif xi + 0x08 >= 0xF9:
                xi = 0xF8 - 0x08
                x = xi << 8
                direction = ((direction ^ 0x1F) + 1) & 0x3F
                bounced = True
        out.append((x >> 8, direction, bounced))
    return out


def main() -> int:
    src = SRC.read_text()
    compact = "".join(src.split())

    if "#define DEATH_SPARK_COUNT 10" not in src:
        raise SystemExit("FAIL: LBC10 spark count must stay at ten object slots")
    if "#define DEATH_SPARK_BODY_W 0x08" not in src:
        raise SystemExit("FAIL: spark body width must mirror LBC10's IX+$0C = $08")
    if "unsigned char dir = 0x1B" not in src:
        raise SystemExit("FAIL: LBC10 spark direction seed $1B changed")
    if "dir = (unsigned char)((dir + 5) & 0x3F)" not in src:
        raise SystemExit("FAIL: LBC10 spark direction increment $05 changed")
    if "death_sparks[i].y_q88         = (long)0xAE << 8" not in src:
        raise SystemExit("FAIL: LBC10 spark Y spawn $AE changed")
    if "death_sparks[i].speed         = 2" not in src:
        raise SystemExit("FAIL: LBC10 spark speed $02 changed")
    if "death_sparks[i].x_q88         = (long)(x_start + i * 3) << 8" not in src:
        raise SystemExit("FAIL: LBC10 spark X spacing +3 changed")
    if "bc = L - 256" in src or "hl = L - 256" in src or "bc = C - 256" in src or "hl = C - 256" in src:
        raise SystemExit("FAIL: death spark direction math must negate table magnitudes, not subtract from 256")

    compact_direction = compact[compact.find("staticvoiddir_to_dxdy"):compact.find("#defineDEATH_SPARK_COUNT")]
    for needle in ("case0x10:hl=C;bc=-L;", "case0x20:hl=-L;bc=-C;", "default:hl=-C;bc=L;"):
        if needle not in compact_direction:
            raise SystemExit(f"FAIL: death spark direction quadrant does not mirror LAD13 magnitude negation: {needle}")

    spawn = "intbat_center=BAT_X+(int)objects[OBJ_BAT_1].w_body_px/2;"
    if spawn not in compact:
        raise SystemExit("FAIL: death sparks must spawn from bat body width, not sprite shadow width")
    if "intx_start=bat_center-12;" not in compact:
        raise SystemExit("FAIL: LBC10 spark X start must be bat_center - $0C")
    if "right_x=0xF8-DEATH_SPARK_BODY_W;" not in compact:
        raise SystemExit("FAIL: right wall clamp must be $F8 - IX+$0C")
    if "elseif(xp>right_x)" not in compact:
        raise SystemExit("FAIL: right wall bounce must trigger once x exceeds $F8 - width")
    if "death_sparks[i].dir=(unsignedchar)(((death_sparks[i].dir^0x1F)+1)&0x3F);" not in compact:
        raise SystemExit("FAIL: horizontal bounce must use change_direction with B=$1F")
    if "death_sparks[i].dir=(unsignedchar)(((death_sparks[i].dir^0x3F)+1)&0x3F);" not in compact:
        raise SystemExit("FAIL: top bounce must use change_direction with B=$3F")
    if "pause_longB=$03" not in compact or "pit_ticks()-death_pause_start<45UL" not in compact:
        raise SystemExit("FAIL: death path must keep LBC10's pause_long B=$03 after sparks expire")

    edge = simulate_original([236, 238, 240, 242], frames=4)
    if not all(x <= 240 for x, _, _ in edge):
        raise SystemExit(f"FAIL: original right clamp model exceeded x=240: {edge}")
    if not any(bounced for _, _, bounced in edge):
        raise SystemExit("FAIL: original right clamp model did not exercise a bounce")
    first = simulate_original([0x74], frames=2, direction_start=0x1B)[0]
    if first[:2] != (0x75, 0x1B):
        raise SystemExit(f"FAIL: original first-spark vector model drifted: {first}")
    print("PASS death_sparks_lbc10_spawn: count, X/Y seed, spacing, direction and speed mirror original")
    print("PASS death_sparks_direction_math: negative components mirror LAD13 magnitude negation")
    print("PASS death_sparks_bounce_wall: right/left/top reflections mirror bounce_wall")
    print("PASS death_sparks_post_pause: LBC10 pause_long B=$03 before respawn is preserved")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
