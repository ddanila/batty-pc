#!/usr/bin/env python3
"""Headless regression: the level-intro hard-brick shimmer must run at the
original's pace and must NOT be skippable by buffered input.

The original (all_metal_briks_animation_snd $B765) shows each of the 8
anim_brik frames after exactly TWO 50Hz interrupts (`EI/HALT/EI/HALT/DI`)
and reads no input — ~16 interrupt edges (~0.32 s) total. An earlier port
version (a) sampled `pit_ticks()` mid-tick and waited `< 2`, i.e. 1..2
ticks per frame, and (b) ABORTED the whole animation on any buffered
keypress — so a key held / typematic-repeating at level entry (moving the
bat, pressing FIRE) skipped the shimmer almost entirely (known-bugs.md #4
"initial shimmer very fast").

The gate: build the floppy with BATTY_TEST_KEY_BEFORE_ANIM=1, which makes
the port stuff one ENTER into the BIOS keyboard buffer right before
play_brik_anim. The fixed animation must (1) ignore-but-not-consume the
key (it then releases BATTY_REPLAY_WAIT_KEY, proving it survived) and
(2) report >= 16 PIT edges for the pass via the PROBE.TXT
`brik_anim_ticks=` line (8 frames x 2 full edge waits is >= 16 by
construction; drawing spill adds a little). With the abort bug the key
kills the animation at frame 0 and the probe reports ~0 ticks.

    make test-brik-anim-pace
"""
import re
import subprocess
import sys
from pathlib import Path

FLOPPY = 'build/batty-test.img'

ENV = ('BATTY_LEVEL=1 BATTY_START_LEVEL=1 BATTY_REPLAY_WAIT_KEY=1 '
       'BATTY_REPLAY_PROBE=1 BATTY_TEST_KEY_BEFORE_ANIM=1 '
       'BATTY_VISUAL_PROBE_FRAMES=2')

TICKS_MIN = 16     # 8 frames x 2 full PIT edges — the original's floor
TICKS_MAX = 40     # generous QEMU/TCG slack above the ~16-17 expected


def main():
    Path(FLOPPY).unlink(missing_ok=True)
    subprocess.run(f'{ENV} make {FLOPPY}', shell=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, 'scripts/capture_frame_timeline.py',
                    '--floppy', FLOPPY, '--frames', '2', '--wait-key',
                    '--out', 'build/tl_brikanim'],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    probe = Path('build/PROBE_brikanim.txt')
    probe.unlink(missing_ok=True)
    subprocess.run(['mcopy', '-n', '-i', FLOPPY, '::PROBE.TXT', str(probe)],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not probe.exists():
        print('FAIL brik_anim_pace: no PROBE.TXT — the stuffed key never '
              'released BATTY_REPLAY_WAIT_KEY (the animation consumed it?)')
        return 1
    m = re.search(r'brik_anim_ticks=(\d+)', probe.read_text())
    if not m:
        print('FAIL brik_anim_pace: PROBE.TXT has no brik_anim_ticks line')
        return 1
    ticks = int(m.group(1))
    ok = TICKS_MIN <= ticks <= TICKS_MAX
    print(f'  intro shimmer: {ticks} PIT edges with a key buffered '
          f'(expect {TICKS_MIN}..{TICKS_MAX})  [{"PASS" if ok else "FAIL"}]')
    if ok:
        print('PASS brik_anim_pace: 8 frames x 2 full edges, key survives '
              '(not abortable, original pacing)')
        return 0
    print('FAIL brik_anim_pace: intro shimmer pace/abort regression '
          f'(ticks={ticks})')
    return 1


if __name__ == '__main__':
    sys.exit(main())
