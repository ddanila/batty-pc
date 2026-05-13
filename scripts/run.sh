#!/bin/bash
# run.sh — boot the batty floppy in QEMU.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FLOPPY="${1:-$SCRIPT_DIR/../build/batty.img}"

if [[ ! -f "$FLOPPY" ]]; then
    echo "ERROR: floppy image not found: $FLOPPY"
    echo "Run 'make floppy' first."
    exit 1
fi

case "$(uname -s)" in
    Darwin) AUDIODRV=coreaudio; DISPLAY_BACKEND=cocoa ;;
    Linux)  AUDIODRV=pa;        DISPLAY_BACKEND=gtk   ;;
    *)      AUDIODRV=sdl;       DISPLAY_BACKEND=sdl   ;;
esac

for d in "$DISPLAY_BACKEND" sdl gtk cocoa curses; do
    if qemu-system-i386 -display help 2>&1 | grep -q "^$d\$"; then
        DISPLAY_BACKEND=$d
        break
    fi
done

echo "Booting: $FLOPPY  (audio=$AUDIODRV, display=$DISPLAY_BACKEND)"
exec qemu-system-i386 \
    -drive if=floppy,format=raw,file="$FLOPPY" \
    -boot a \
    -m 4 \
    -audiodev "$AUDIODRV,id=snd0" \
    -machine pcspk-audiodev=snd0 \
    -device adlib,audiodev=snd0 \
    -display "$DISPLAY_BACKEND" \
    -name "batty"
