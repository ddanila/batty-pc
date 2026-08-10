#!/usr/bin/env bash
# run_dosbox.sh — boot the batty floppy in DOSBox-X.
#
# A third runner alongside QEMU (scripts/run.sh) and 86Box
# (scripts/run_86box.sh). It exists because DOSBox-X is the one people
# already have installed, and because it starts in about a second where
# QEMU's BIOS + DOS boot takes several.
#
# BOOT, not imgmount + BATTY.EXE. The build-time BATTY_* switches live in
# the floppy's AUTOEXEC.BAT (see the `floppy` target), and DOSBox-X's own
# shell never runs a mounted image's AUTOEXEC — so the convenient
# invocation is the one that silently drops every switch you set. `boot`
# hands the machine to the image's real DOS, which is what QEMU does too,
# so the two runners differ in emulator and in nothing else.
#
# Not a gate and not an oracle: no capture path reads DOSBox-X, and
# nothing here is byte-compared against anything. It is for playing.
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 path/to/floppy.img" >&2
    exit 2
fi

floppy=$1
if [ ! -f "$floppy" ]; then
    echo "DOSBox-X runner: floppy image not found: $floppy" >&2
    echo "Run 'make floppy' first." >&2
    exit 1
fi

find_dosbox() {
    if [ -n "${DOSBOX_BIN:-}" ]; then
        printf '%s\n' "$DOSBOX_BIN"
        return
    fi
    command -v dosbox-x 2>/dev/null && return
    if [ -x "/Applications/dosbox-x.app/Contents/MacOS/dosbox-x" ]; then
        printf '%s\n' "/Applications/dosbox-x.app/Contents/MacOS/dosbox-x"
        return
    fi
}

dosbox=$(find_dosbox || true)
if [ -z "$dosbox" ] || [ ! -x "$dosbox" ]; then
    echo "DOSBox-X runner: dosbox-x not found; set DOSBOX_BIN=/path/to/dosbox-x" >&2
    echo "  macOS: brew install dosbox-x" >&2
    exit 1
fi

# The game reprograms the PIT to 50 Hz (PIT_DIV_50HZ) and paces itself on
# its own IRQ0 count, so cycles do not set the game's speed — they set how
# much CPU is available to finish a frame inside it. `max` gives the frame
# loop far more headroom than a 386 had, which is the right default for
# playing and the wrong one for judging period performance. For that, pin
# it: DOSBOX_CYCLES=12000 is roughly a 386DX-40.
cycles=${DOSBOX_CYCLES:-max}

# Ignore the user's dosbox-x.conf by default: this should behave the same
# on every machine, and a personal conf that sets machine=cga or a 286
# cputype would fail in a way that looks like the port's fault. The build
# has been 386 protected mode under DOS32A since c52f3a2.
conf=${DOSBOX_CONF:-/dev/null}

echo "Booting: $floppy  (dosbox-x, cycles=$cycles)"
exec "$dosbox" -conf "$conf" \
    -c "config -set cpu cycles=$cycles" \
    -c "boot $floppy -l a"
