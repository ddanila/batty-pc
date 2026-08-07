#!/usr/bin/env bash
# Download the current Open Watcom v2 snapshot release from GitHub and
# extract just the subset needed to build this repo into
# vendor/openwatcom-v2/current-build-<published-date>/.
#
# The bundle stays self-contained so the project never references any
# path outside the repository.
#
# What the build needs (see notes/toolchain.md):
#   wcc386 / wpp386   32-bit C and C++ compilers (batty targets 32-bit
#                     protected-mode DOS via a flat memory model)
#   wlink             linker; `format os2 le` + a stub produces the LE
#                     executable the DOS extender loads
#   wdis              disassembler — used to verify that a refactor
#                     changed no generated code
#   h/                headers INCLUDING the extensionless C++ ones
#                     (<cstdio> and friends); an earlier revision of this
#                     script deleted those and C++ could not compile
#   lib386/dos/       clib3r (C), plib3r (C++), math3r — register-calling
#                     32-bit DOS libraries
#   binw/dos32a.exe   the DOS extender shipped on the game floppy. DOS32A,
#                     not DOS/4GW: 27 KB vs 265 KB, and DOS/4GW's hardware
#                     interrupt reflection breaks batty's INT 9 keyboard
#                     chain (the game hangs on the title screen).
#   binw/wstub.exe    real-mode stub bound into the LE executable
#
# Requires: gh, tar, xz, shasum (or sha256sum). Run from the repo root.
#
# To reuse an already-downloaded snapshot instead of fetching 150 MB again:
#   OW_SNAPSHOT_TAR=/path/to/ow-snapshot.tar.xz OW_SNAPSHOT_DATE=2026-08-01 \
#       scripts/vendor_openwatcom.sh

set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

if [[ -n "${OW_SNAPSHOT_TAR:-}" ]]; then
    echo "==> using local snapshot: $OW_SNAPSHOT_TAR"
    [[ -n "${OW_SNAPSHOT_DATE:-}" ]] || {
        echo "OW_SNAPSHOT_DATE must be set alongside OW_SNAPSHOT_TAR" >&2
        exit 1
    }
    date="$OW_SNAPSHOT_DATE"
    snapshot="$OW_SNAPSHOT_TAR"
else
    echo "==> fetching release metadata"
    published=$(gh release view Current-build \
        --repo open-watcom/open-watcom-v2 \
        --json publishedAt --jq .publishedAt)
    date="${published%%T*}"
    asset_digest=$(gh release view Current-build \
        --repo open-watcom/open-watcom-v2 \
        --json assets \
        --jq '.assets[] | select(.name=="ow-snapshot.tar.xz") | .digest')
    asset_size=$(gh release view Current-build \
        --repo open-watcom/open-watcom-v2 \
        --json assets \
        --jq '.assets[] | select(.name=="ow-snapshot.tar.xz") | .size')

    echo "    published : $published"
    echo "    digest    : $asset_digest"
    echo "    size      : $asset_size bytes"

    echo "==> downloading snapshot"
    gh release download Current-build \
        --repo open-watcom/open-watcom-v2 \
        --pattern ow-snapshot.tar.xz \
        --dir "$tmp" \
        --clobber
    snapshot="$tmp/ow-snapshot.tar.xz"
fi

echo "==> extracting needed subset"
tar -xf "$snapshot" -C "$tmp" \
    ./binl64/wcc386 ./binl64/wpp386 ./binl64/wlink ./binl64/wdis \
    ./armo64/wcc386 ./armo64/wpp386 ./armo64/wlink ./armo64/wdis \
    ./bino64/wcc386 ./bino64/wpp386 ./bino64/wlink ./bino64/wdis \
    ./h \
    ./lib386/dos/clib3r.lib \
    ./lib386/dos/plib3r.lib \
    ./lib386/dos/math3r.lib \
    ./binw/dos32a.exe \
    ./binw/dos4gw.exe \
    ./binw/wstub.exe

dst="vendor/openwatcom-v2/current-build-$date"
echo "==> placing files in $dst"
rm -rf "$dst"
mkdir -p "$dst"/{linux-amd64,macos-arm64,macos-x64,lib386/dos,dos}

for tool in wcc386 wpp386 wlink wdis; do
    cp "$tmp/binl64/$tool" "$dst/linux-amd64/$tool"
    cp "$tmp/armo64/$tool" "$dst/macos-arm64/$tool"
    cp "$tmp/bino64/$tool" "$dst/macos-x64/$tool"
done
chmod +x "$dst"/linux-amd64/* "$dst"/macos-arm64/* "$dst"/macos-x64/*

cp -R "$tmp"/h "$dst"/
# Drop the OS-specific trees the project doesn't target. The extensionless
# C++ headers (<cstdio>, <cstring>, ...) MUST be kept — wpp386 needs them.
rm -rf "$dst"/h/nt "$dst"/h/os2 "$dst"/h/win "$dst"/h/os21x

cp "$tmp"/lib386/dos/clib3r.lib "$dst"/lib386/dos/
cp "$tmp"/lib386/dos/plib3r.lib "$dst"/lib386/dos/
cp "$tmp"/lib386/dos/math3r.lib "$dst"/lib386/dos/

# DOS-side binaries: the extender that ships on the floppy, and the
# real-mode stub the linker binds into the LE executable.
cp "$tmp"/binw/dos32a.exe "$dst"/dos/DOS32A.EXE
cp "$tmp"/binw/dos4gw.exe "$dst"/dos/DOS4GW.EXE
cp "$tmp"/binw/wstub.exe  "$dst"/dos/wstub.exe

echo "==> done: $(du -sh "$dst" | cut -f1) in $dst"
echo
echo "Next steps:"
echo "  1. Update WATCOM_DIR in the Makefile to point at $dst"
echo "  2. Refresh the checksums in $dst/README.md:"
echo
sha_cmd="shasum -a 256"
command -v shasum >/dev/null 2>&1 || sha_cmd="sha256sum"
(cd "$dst" && $sha_cmd \
    linux-amd64/wcc386 linux-amd64/wpp386 linux-amd64/wlink linux-amd64/wdis \
    macos-arm64/wcc386 macos-arm64/wpp386 macos-arm64/wlink macos-arm64/wdis \
    macos-x64/wcc386 macos-x64/wpp386 macos-x64/wlink macos-x64/wdis \
    lib386/dos/clib3r.lib lib386/dos/plib3r.lib lib386/dos/math3r.lib \
    dos/DOS32A.EXE dos/DOS4GW.EXE dos/wstub.exe) | sed 's/^/     /'
echo
echo "  3. Remove any older vendor/openwatcom-v2/current-build-* directory"
