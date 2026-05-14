#!/usr/bin/env python3
"""Launch the ORIGINAL Batty in ZEsarUX with cheats for clean GT capture.

Always-on cheat: at PC 0xBD32..0xBD3B the game runs

    BD32: ld a, (B7E8)   ; load lives counter
    BD35: dec a          ; <-- the decrement (patched to `or a`)
    BD36: ld (B7E8), a   ; store back
    BD39: jr z, lbd45h   ; if result == 0, jump to game-over
    BD3B: ...            ; else continue / re-init level

We patch BD35 from `dec a` (0x3D) to `or a` (0xB7). Lives stays at
3 forever and the game-over branch never fires.

OPTIONAL experimental cheats (set via env vars before running) to
help isolate enemy / fade-in code paths. The main per-frame entity
loop at 0xBB97..0xBBDD calls sub_b66ah (an 11-slot dispatcher over
the entity list at 0x9AD0) several times with different per-entity
handler routines. NOP-ing each call individually lets us probe
which slot the bird / power-up etc lives in:

    BATTY_NOPATCH=1     skip all patches (vanilla play)
    BATTY_NOP_BB9E=1    NOP the PRNG call at 0xBB9E (3 B)
    BATTY_NOP_BBA4=1    NOP `call sub_b66ah` after PRNG (HL=0x9F54)
    BATTY_NOP_BBA7=1    NOP `call sub_b694h` (5-slot table)
    BATTY_NOP_BBAD=1    NOP `call sub_b66ah` w/ HL=sub_b684h
    BATTY_NOP_BBB6=1    NOP `call sub_b66ah` w/ HL=sub_9910h
    BATTY_NOP_BBCD=1    NOP `call nz, sub_b717h` (deferred-action)

Workflow:
    make run-original-cheat                  # vanilla: infinite lives only
    BATTY_NOP_BBA4=1 make run-original-cheat # try first probe
    # play to L3, see if bird still appears, snapshot if clean
"""
import os
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from zrcp import ZrcpClient


ZESARUX = Path(__file__).resolve().parent.parent / "../generaly/tools/zesarux/src/zesarux"
TAPE    = Path(__file__).resolve().parent.parent / "original/batty.tap"
ZRCP_PORT = 10000

# Address of the `dec a` byte in the lives-decrement sequence.
# Patch 0x3D (dec a) -> 0xB7 (or a). See module docstring.
LIVES_DEC_PC = 0xBD35
OR_A_BYTE    = 0xB7

# Optional NOP-the-call probes (3-byte CD nn nn -> 00 00 00 each).
PROBES = [
    ('BATTY_NOP_BB9E', 0xBB9E, 'PRNG call (sub_8eb4h)'),
    ('BATTY_NOP_BBA4', 0xBBA4, 'sub_b66ah w/ HL=l9f54h (post-PRNG entity update)'),
    ('BATTY_NOP_BBA7', 0xBBA7, 'sub_b694h (5-slot table processor)'),
    ('BATTY_NOP_BBAD', 0xBBAD, 'sub_b66ah w/ HL=sub_b684h'),
    ('BATTY_NOP_BBB6', 0xBBB6, 'sub_b66ah w/ HL=sub_9910h'),
    ('BATTY_NOP_BBCD', 0xBBCD, 'conditional sub_b717h (deferred-action)'),
]

LOAD_WAIT_SECONDS = 18.0    # batty.tap is ~40 KB; real-time tape load


def main():
    cmd = [
        str(ZESARUX),
        '--noconfigfile',
        '--machine', '48k',
        '--enable-remoteprotocol',
        '--remoteprotocol-port', str(ZRCP_PORT),
        '--quickexit',
        str(TAPE),
    ]
    print(f'launching ZEsarUX (windowed): {" ".join(cmd)}')
    print(f'tape will load over ~{LOAD_WAIT_SECONDS:.0f}s; cheat patches after.')
    proc = subprocess.Popen(cmd)

    print(f'waiting {LOAD_WAIT_SECONDS:.0f}s for tape to load...')
    time.sleep(LOAD_WAIT_SECONDS)

    no_patch = os.environ.get('BATTY_NOPATCH')
    if no_patch:
        print('BATTY_NOPATCH set - skipping all patches (vanilla play).')
    else:
        print(f'connecting ZRCP and applying patches...')
        try:
            with ZrcpClient(port=ZRCP_PORT) as zc:
                zc.connect()
                print(f'  ZEsarUX version: {zc.get_version()}')
                # Always: infinite lives
                zc.write_memory(LIVES_DEC_PC, bytes([OR_A_BYTE]))
                print(f'  + infinite lives (0x{LIVES_DEC_PC:04X} = 0x{OR_A_BYTE:02X})')
                # Optional probes
                for envvar, pc, desc in PROBES:
                    if os.environ.get(envvar):
                        zc.write_memory(pc, bytes([0x00, 0x00, 0x00]))
                        print(f'  + NOP 0x{pc:04X}..0x{pc+2:04X}  ({desc})')
        except (OSError, ConnectionRefusedError) as e:
            print(f'  ZRCP connect failed: {e}')
            print('  (ZEsarUX may not have finished starting; try LOAD_WAIT_SECONDS=25)')

    print()
    print('=' * 60)
    print('Play the game in the ZEsarUX window.')
    print('To snapshot RAM + screen at any moment, run in another terminal:')
    print('    make snapshot')
    print('Snapshots land in build/snapshots/<UTC timestamp>/.')
    print()
    print('To probe for the enemy-spawn / fade-in routines, re-launch with:')
    for envvar, pc, desc in PROBES:
        print(f'    {envvar}=1 make run-original-cheat  # NOP 0x{pc:04X} ({desc})')
    print('=' * 60)

    # Wait for the user to close ZEsarUX.
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.terminate()
        proc.wait()


if __name__ == '__main__':
    main()
