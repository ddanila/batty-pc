#!/usr/bin/env python3
"""Headless visual regression test.

Boots build/batty.img in QEMU with `-display none -monitor stdio`,
captures the framebuffer at each checkpoint, decodes it back into
palette-index space, and diffs against the ZEsarUX snapshot decoded
through our ZX palette. Pixel-identical => PASS.

Returns exit code = number of failed checkpoints.
"""
import argparse
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

# 6-bit DAC values written by src/main.cpp (ZX_LO=56, ZX_HI=63).
# QEMU's mode-13h output scales them by plain shift-2 (no LSB replication):
# DAC 56 -> 0xE0 (224), DAC 63 -> 0xFF (255). DAC 0 -> 0.
ZX_LO = 56
ZX_HI = 63
ZX_LO_8 = 224
ZX_HI_8 = 255
SCREEN_W, SCREEN_H = 320, 200
PLAYFIELD_W, PLAYFIELD_H = 256, 192
BORDER_X, BORDER_Y = 32, 4


def build_palette_rgb():
    """16 ZX palette RGB triples in the 8-bit values QEMU emits."""
    out = []
    for level in (ZX_LO_8, ZX_HI_8):
        for ink in range(8):
            r = level if (ink & 2) else 0
            g = level if (ink & 4) else 0
            b = level if (ink & 1) else 0
            out.append((r, g, b))
    return out


PALETTE_RGB = build_palette_rgb()
PAL_LOOKUP  = {rgb: i for i, rgb in enumerate(PALETTE_RGB)}


def rgb_to_index(rgb):
    if rgb in PAL_LOOKUP:
        return PAL_LOOKUP[rgb]
    return min(range(16),
               key=lambda i: sum((a-b)**2 for a, b in zip(rgb, PALETTE_RGB[i])))


def parse_ppm(path: Path):
    """Parse a P6 binary PPM. Returns (w, h, raw_rgb_bytes)."""
    data = path.read_bytes()
    # Header is 3 ASCII tokens separated by whitespace (comments start with '#').
    i = 0
    def next_token():
        nonlocal i
        while i < len(data) and data[i:i+1] in b' \t\r\n':
            i += 1
        if i < len(data) and data[i:i+1] == b'#':
            while i < len(data) and data[i:i+1] != b'\n':
                i += 1
            return next_token()
        start = i
        while i < len(data) and data[i:i+1] not in b' \t\r\n':
            i += 1
        return data[start:i]
    magic  = next_token()
    w      = int(next_token())
    h      = int(next_token())
    maxval = int(next_token())
    if magic != b'P6':   raise ValueError(f'expected P6, got {magic!r}')
    if maxval != 255:    raise ValueError(f'expected maxval 255, got {maxval}')
    i += 1   # skip single-byte separator after maxval
    return w, h, data[i:]


def ppm_inner_to_indices(path: Path):
    """Extract the 256x192 playfield region from QEMU's PPM, map to palette.
    QEMU outputs mode 13h scaled 2x to 640x400 for aspect correction —
    detect the scale and sample one pixel per VGA pixel cell."""
    w, h, raw = parse_ppm(path)
    if (w, h) == (SCREEN_W, SCREEN_H):
        scale = 1
    elif (w, h) == (SCREEN_W * 2, SCREEN_H * 2):
        scale = 2
    else:
        raise ValueError(f'unexpected PPM size {w}x{h}; expected 320x200 or 640x400')
    out = bytearray(PLAYFIELD_W * PLAYFIELD_H)
    for y in range(PLAYFIELD_H):
        py = (BORDER_Y + y) * scale
        for x in range(PLAYFIELD_W):
            px = (BORDER_X + x) * scale
            off = (py * w + px) * 3
            out[y * PLAYFIELD_W + x] = rgb_to_index((raw[off], raw[off+1], raw[off+2]))
    return bytes(out)


def expected_from_scr(scr_path: Path):
    sys.path.insert(0, str(Path(__file__).parent))
    from extract_scr import decode
    return decode(scr_path.read_bytes())


def run_qemu(floppy: Path, script: list, log_path: Path, serial_path=None):
    """Drive QEMU via -monitor stdio. `script` is a list of:
      'SLEEP <secs>'        — wall-clock wait
      'WAITSERIAL <n> [to]' — wait until the port has emitted >= n 'PROBE'
                              markers on COM1 (deterministic frame-reached
                              signal; needs serial_path + a BATTY_SERIAL_PROBE
                              floppy). Optional timeout `to` secs (default 40).
      anything else         — a raw QEMU monitor command.
    serial_path: if given, COM1 is captured to that file (`-serial file:`)."""
    log = log_path.open('wb')
    cmd = [
        'qemu-system-i386',
        '-drive', f'if=floppy,format=raw,file={floppy}',
        '-boot', 'a',
        '-m', '4',
        '-display', 'none',
        '-monitor', 'stdio',
        '-no-reboot',
    ]
    if serial_path is not None:
        Path(serial_path).write_bytes(b'')   # truncate prior markers
        cmd += ['-serial', f'file:{serial_path}']
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=log, stderr=log)
    try:
        for step in script:
            if step.startswith('SLEEP '):
                time.sleep(float(step.split()[1]))
            elif step.startswith('WAITSERIAL '):
                parts = step.split()
                want = int(parts[1])
                timeout = float(parts[2]) if len(parts) > 2 else 40.0
                deadline = time.time() + timeout
                while time.time() < deadline:
                    try:
                        n = Path(serial_path).read_bytes().count(b'PROBE')
                    except (OSError, TypeError):
                        n = 0
                    if n >= want:
                        break
                    time.sleep(0.1)
                time.sleep(0.15)   # let the post-marker screen settle
            else:
                proc.stdin.write((step + '\n').encode())
                proc.stdin.flush()
                time.sleep(0.2)
        proc.stdin.write(b'quit\n')
        proc.stdin.flush()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
    finally:
        log.close()


def test_floppy(default: str = "build/batty-test.img") -> str:
    """The test floppy image path. Honours BATTY_TEST_FLOPPY so the parallel
    runner can give each concurrent gate its own image (scripts/
    run_gates_parallel.py); defaults to the shared serial path."""
    import os
    return os.environ.get("BATTY_TEST_FLOPPY", default)


def read_probe_dict(floppy) -> dict:
    """Read PROBE.TXT off `floppy` into a key->value dict ({} if absent).
    Accepts both `k=v` and `k:v` lines; skips comments."""
    try:
        raw = subprocess.check_output(["mtype", "-i", str(floppy), "::PROBE.TXT"],
                                      stderr=subprocess.STDOUT).decode("ascii", "replace")
    except subprocess.CalledProcessError:
        return {}
    out = {}
    for line in raw.splitlines():
        if line.startswith("#"):
            continue
        if "=" in line:
            k, v = line.split("=", 1)
        elif ":" in line:
            k, v = line.split(":", 1)
        else:
            continue
        out[k.strip()] = v.strip()
    return out


def boot_until_gameplay(floppy, drive_fn, attempts: int = 3, label: str = ""):
    """Run drive_fn() (boot QEMU + send the BATTY_REPLAY_WAIT_KEY wake),
    then read PROBE.TXT; retry if it is the pre-gameplay seed write.

    The port writes PROBE.TXT with the seeded pre-gameplay state at level
    init (probe_phase=init) BEFORE the wait-key pause. If the wake key is
    missed (a rare slow-boot host-timing race) the gameplay loop never runs
    and the file stays at that init write — a spurious result. A real
    checkpoint write tags probe_phase=play, so we re-boot until we see it
    (or exhaust attempts). Returns the probe dict (last attempt if all
    stale, so callers still surface a clear failure)."""
    probe = {}
    for attempt in range(attempts):
        drive_fn()
        probe = read_probe_dict(floppy)
        if probe.get("probe_phase") == "play":
            return probe
        if attempt + 1 < attempts:
            import sys as _sys
            print(f"    (retry {attempt + 1}/{attempts - 1}: wake missed / "
                  f"probe_phase={probe.get('probe_phase', 'absent')}{(' ' + label) if label else ''})",
                  file=_sys.stderr)
    return probe


def make_diff_png(actual_idx: bytes, expected_idx: bytes, out_path: Path):
    """Write a side-by-side diff (red where pixels disagree)."""
    try:
        from PIL import Image
    except ImportError:
        return
    img = Image.new('RGB', (PLAYFIELD_W, PLAYFIELD_H))
    px = img.load()
    for y in range(PLAYFIELD_H):
        for x in range(PLAYFIELD_W):
            i = y * PLAYFIELD_W + x
            if actual_idx[i] == expected_idx[i]:
                # Render expected in grey for context.
                v = 64 if expected_idx[i] != 0 else 0
                px[x, y] = (v, v, v)
            else:
                px[x, y] = (255, 0, 0)
    img.save(out_path)


def lint_moving_object_attrs(src_path: Path) -> int:
    """Source-code lint: no per-cell attr override in moving-object
    renderers. The original game's print_obj_to_buff writes pixels
    only — it never touches attr_buff for moving objects. User's spec:
    "the game field is monochrome except blocks". So moving sprites
    (bonus drops, bullets, rockets, blasts) MUST inherit each cell's
    existing attr; calling blit_sprite_attrs_to_buff from any of them
    causes a wrong-colour trail visible during gameplay (which
    state4_level1 can't catch — it's a level-entry checkpoint and
    none of those objects are active yet).

    Approved caller: render_bat only (the _clipped variant forces
    bg_attr to keep the bat bg-coloured when it slides into a side-strip
    cell). The enemy used to be approved too, but known-bugs #7 removed
    its recolour — the original blits enemy PIXELS only, so the bird/UFO
    keeps each cell's underlying brick/bg attr (ZX colour-clash). The
    non-clipped blit_sprite_attrs_to_buff helper is gone entirely; any
    new call from a moving-object renderer is now a regression.
    """
    APPROVED_CALLERS = {'render_bat'}
    text = src_path.read_text()
    lines = text.split('\n')
    fails = []
    current_fn = None
    import re
    for ln, line in enumerate(lines, start=1):
        # Track the enclosing function (static <ret> <name>(...) at col 0).
        m = re.match(r'^static\s+\S+\s+(\w+)\s*\(', line)
        if m:
            current_fn = m.group(1)
        # Match call sites only — skip the function definition / forward
        # declaration (lines starting with `static`) and comment lines
        # (which legitimately mention the removed symbol in fix notes).
        stripped = line.lstrip()
        if stripped.startswith(('static ', '*', '/*', '//')):
            continue
        if 'blit_sprite_attrs_to_buff' in line and '(' in line:
            if current_fn and current_fn not in APPROVED_CALLERS:
                fails.append((ln, current_fn, line.strip()))
    if fails:
        print('  FAIL lint: blit_sprite_attrs_to_buff in non-approved renderer:')
        for ln, fn, code in fails:
            print(f'    {src_path}:{ln} in {fn}(): {code}')
        print(f'    Approved callers: {sorted(APPROVED_CALLERS)}')
        return len(fails)
    print('  PASS lint: no stray blit_sprite_attrs_to_buff in moving-object renderers')
    return 0


def lint_bat_redraw_window(src_path: Path) -> int:
    """Source-code lint: bat-only redraw must stay object-window scoped.

    The original print_obj_from_buf_to_scr restores the byte-aligned
    union of previous/current object bounds. A full BAT_Y_PX strip flush
    makes both side borders visibly repaint while the bat moves.
    """
    text = src_path.read_text()
    start = text.find("static void redraw_bat(")
    end = text.find("static void render_hud_to_buff", start)
    if start < 0 or end < 0:
        print("  FAIL lint: could not locate redraw_bat()")
        return 1
    body = text[start:end]
    failed = 0
    if "buff_to_vga_rect_bytes(BAT_Y, BAT_H_PX" not in body:
        print("  FAIL lint: redraw_bat() does not flush a byte-window rectangle")
        failed += 1
    if "byte_lo--" not in body or "byte_hi++" not in body:
        print("  FAIL lint: redraw_bat() must pad the byte window for unaligned sprite edges")
        failed += 1
    if "buff_to_vga_strip(BAT_Y" in body or "buff_to_vga_strip(BAT_Y_PX, BAT_H_PX" in body:
        print("  FAIL lint: redraw_bat() flushes the full bat strip")
        failed += 1
    if failed:
        return failed
    print("  PASS lint: redraw_bat is byte-window scoped")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--floppy', default='build/batty.img')
    ap.add_argument('--out', default='build/test_visual')
    ap.add_argument('--boot-wait', type=float, default=10.0)
    ap.add_argument('--state-wait', type=float, default=1.5)
    args = ap.parse_args()
    floppy = Path(args.floppy)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    SNAP_HISCORE = Path('build/snapshots/20260513T202038Z/screen.scr')
    SNAP_MENU    = Path('build/snapshots/20260513T202041Z/screen.scr')
    TITLE_SCR    = Path('original/Batty.scr')
    # BATTY_LEVEL env (1..15) switches the level-entry GT to that level so
    # `BATTY_LEVEL=N make test` actually compares L_N render vs L_N GT.
    # Defaults to L1 when unset / invalid. The matching env passthrough on
    # the DOS side is in the test floppy's AUTOEXEC.BAT (see Makefile).
    import os
    level_env = (os.environ.get('BATTY_LEVEL') or '').strip()
    try:
        level_n = int(level_env) if level_env else 1
        if not (1 <= level_n <= 15): level_n = 1
    except ValueError:
        level_n = 1
    GT_LEVEL1    = Path(f'build/level_gt/level_{level_n:02d}.scr')
    if level_n != 1:
        print(f'BATTY_LEVEL={level_n} -> diffing against {GT_LEVEL1}')
    # Per checkpoint: (label, expected_scr, assert_match, roi, source_label)
    # roi=None  -> diff the full 256x192 playfield
    # roi=(x0, y0, x1, y1) -> diff only that sub-rectangle in playfield coords
    # source_label=None -> capture and diff this checkpoint's own PPM
    # source_label='other_state' -> diff against another checkpoint's PPM
    #   (used by ROI checkpoints that re-examine an already-captured frame).
    # `assert_match=False` => captured, diff-reported, but not failing.
    # state4_level1 diffs the full playfield against a modded-batty GT
    # captured AFTER one gameplay-loop iter has painted bat/ball/lives
    # to VRAM (see notes/modded-batty.md). The residual is real rendering
    # drift, not the old bat-overlay artefact.
    # state5_bat_band is the same captured frame, ROI'd to the bat band
    # (y=160..192). Separate metric so bat-render regressions are not
    # buried inside the whole-frame number — fix this before state4.
    checkpoints = [
        ('state1_title',    TITLE_SCR,    True,  None,                 None),
        ('state2_menu',     SNAP_MENU,    False, None,                 None),
        ('state3_hiscore',  SNAP_HISCORE, True,  None,                 None),
        ('state4_level1',   GT_LEVEL1,    False, None,                 None),
        ('state5_bat_band', GT_LEVEL1,    True,  (0, 160, 256, 192),  'state4_level1'),
    ]

    # Only checkpoints that own their own PPM contribute a screendump +
    # ENTER step. ROI/derived checkpoints (source_label set) reuse an
    # earlier capture and are skipped here.
    own_captures = [cp for cp in checkpoints if cp[4] is None]
    script = [f'SLEEP {args.boot_wait}']
    for i, cp in enumerate(own_captures):
        label = cp[0]
        script.append(f'screendump {out/label}.ppm')
        script.append('SLEEP 0.3')
        if i < len(own_captures) - 1:
            script.append('sendkey ret')
            script.append(f'SLEEP {args.state_wait}')
    script.append('sendkey esc')

    print(f'booting {floppy} headless (boot wait {args.boot_wait}s)...')
    run_qemu(floppy, script, out / 'qemu.log')

    failed = 0
    for label, expected_scr, assert_match, roi, source_label in checkpoints:
        ppm_path = out / f'{source_label or label}.ppm'
        if not ppm_path.exists():
            print(f'  FAIL {label}: no PPM produced'); failed += 1; continue
        actual   = ppm_inner_to_indices(ppm_path)
        expected = expected_from_scr(expected_scr)
        if roi is None:
            diff = sum(1 for a, e in zip(actual, expected)
                       if PALETTE_RGB[a] != PALETTE_RGB[e])
            total = PLAYFIELD_W * PLAYFIELD_H
        else:
            x0, y0, x1, y1 = roi
            diff = 0
            total = (x1 - x0) * (y1 - y0)
            for y in range(y0, y1):
                row = y * PLAYFIELD_W
                for x in range(x0, x1):
                    if PALETTE_RGB[actual[row + x]] != PALETTE_RGB[expected[row + x]]:
                        diff += 1
        pct = 100.0 * diff / total
        if diff == 0:
            print(f'  PASS {label}: pixel-identical ({total} px)'
                  + (f' [roi {roi}]' if roi else ''))
        else:
            make_diff_png(actual, expected, out / f'{label}_diff.png')
            tag = 'FAIL' if assert_match else 'INFO'
            roi_tag = f' [roi {roi}]' if roi else ''
            print(f'  {tag} {label}: {diff}/{total} px differ ({pct:.2f}%){roi_tag}')
            print(f'        diff -> {out}/{label}_diff.png')
            if assert_match:
                failed += 1

    # Source-code lint: catch this class of regression even when state4
    # can't (= the buggy code path is only exercised mid-gameplay).
    failed += lint_moving_object_attrs(Path('src/main.cpp'))
    failed += lint_bat_redraw_window(Path('src/main.cpp'))

    sys.exit(failed)


if __name__ == '__main__':
    main()
