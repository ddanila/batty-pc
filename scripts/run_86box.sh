#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 path/to/floppy.img" >&2
    exit 2
fi

floppy=$1
if [ ! -f "$floppy" ]; then
    echo "86Box runner: floppy image not found: $floppy" >&2
    exit 1
fi

find_86box() {
    if [ -n "${BOX86_BIN:-}" ]; then
        printf '%s\n' "$BOX86_BIN"
        return
    fi
    if [ -x /home/ddanila/.local/86box/bin/86Box ]; then
        printf '%s\n' /home/ddanila/.local/86box/bin/86Box
        return
    fi
    command -v 86Box 2>/dev/null && return
    command -v 86box 2>/dev/null && return
    command -v 86box-qt 2>/dev/null && return
}

box86=$(find_86box || true)
if [ -z "$box86" ] || [ ! -x "$box86" ]; then
    echo "86Box runner: 86Box not found; set BOX86_BIN=/path/to/86Box" >&2
    exit 1
fi

vm_dir=${BOX86_VM_DIR:-build/86box}
machine=${BOX86_MACHINE:-ibmxt}
gfxcard=${BOX86_GFXCARD:-vga}
fdd_type=${BOX86_FDD_TYPE:-35_2hd}
mem_size=${BOX86_MEM_SIZE:-640}
cpu_family=${BOX86_CPU_FAMILY:-8088}
cpu_speed=${BOX86_CPU_SPEED:-4772728}
asset_path=${BOX86_ASSETPATH:-/home/ddanila/fun/86Box/src/unix/assets}
fd_controller=${BOX86_FD_CONTROLLER:-dtk_pii151b}

mkdir -p "$vm_dir"
vm_dir=$(realpath "$vm_dir")
floppy=$(realpath "$floppy")
cfg="$vm_dir/86box.cfg"
global_cfg="$vm_dir/86box_global.cfg"
log="$vm_dir/86box.log"

cat > "$cfg" <<EOF
[General]
scale = 5

[Machine]
machine = $machine
cpu_family = $cpu_family
cpu_speed = $cpu_speed
cpu_multi = 1.000000
mem_size = $mem_size
cpu_use_dynarec = 0

[Video]
gfxcard = $gfxcard

[Sound]
sound_type = float

[Storage controllers]
fdc = $fd_controller

[Floppy and CD-ROM drives]
fdd_01_type = $fdd_type
fdd_02_type = none
fdd_01_turbo = 1
EOF

args=(
    -P "$vm_dir"
    -O "$global_cfg"
    -L "$log"
    -N
    -I "A:$floppy"
)

if [ -n "${BOX86_ROMPATH:-}" ]; then
    args+=(-R "$BOX86_ROMPATH")
fi

if [ -d "$asset_path" ]; then
    args+=(-A "$asset_path")
fi

echo "86Box VM: $vm_dir"
echo "86Box config: $cfg"
echo "86Box log: $log"
exec "$box86" "${args[@]}"
