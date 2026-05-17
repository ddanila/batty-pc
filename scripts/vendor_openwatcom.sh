#!/usr/bin/env bash
# Download the current Open Watcom v2 snapshot release from GitHub and
# extract just the subset needed to build this repo into
# vendor/openwatcom-v2/current-build-<published-date>/.
#
# The bundle stays self-contained so the project never references any
# path outside the repository.
#
# Requires: gh, tar, xz, shasum (or sha256sum). Run from the repo root.

set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

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

echo "==> extracting needed subset"
tar -xf "$tmp/ow-snapshot.tar.xz" -C "$tmp" \
    ./binl64/wcc ./binl64/wlink \
    ./armo64/wcc ./armo64/wlink \
    ./bino64/wcc ./bino64/wlink \
    ./h \
    ./lib286/dos/clibs.lib

dst="vendor/openwatcom-v2/current-build-$date"
echo "==> placing files in $dst"
rm -rf "$dst"
mkdir -p "$dst"/{linux-amd64,macos-arm64,macos-x64,lib286/dos}

cp "$tmp"/binl64/wcc   "$dst"/linux-amd64/wcc
cp "$tmp"/binl64/wlink "$dst"/linux-amd64/wlink
cp "$tmp"/armo64/wcc   "$dst"/macos-arm64/wcc
cp "$tmp"/armo64/wlink "$dst"/macos-arm64/wlink
cp "$tmp"/bino64/wcc   "$dst"/macos-x64/wcc
cp "$tmp"/bino64/wlink "$dst"/macos-x64/wlink
chmod +x "$dst"/linux-amd64/* "$dst"/macos-arm64/* "$dst"/macos-x64/*

cp -R "$tmp"/h "$dst"/
# Drop the non-DOS OS-specific trees and the extensionless C++ STL
# headers the project doesn't use. Leaves just the C headers wcc needs.
rm -rf "$dst"/h/nt "$dst"/h/os2 "$dst"/h/win "$dst"/h/os21x
find "$dst"/h -type f ! -name '*.h' -delete

cp "$tmp"/lib286/dos/clibs.lib "$dst"/lib286/dos/clibs.lib

echo "==> done: $(du -sh "$dst" | cut -f1) in $dst"
echo
echo "Next steps:"
echo "  1. Update WATCOM_DIR in the Makefile to point at $dst"
echo "  2. Refresh the checksums in $dst/README.md:"
echo
if command -v shasum >/dev/null 2>&1; then
    (cd "$dst" && shasum -a 256 \
        linux-amd64/wcc linux-amd64/wlink \
        macos-arm64/wcc macos-arm64/wlink \
        macos-x64/wcc macos-x64/wlink \
        lib286/dos/clibs.lib) | sed 's/^/     /'
else
    (cd "$dst" && sha256sum \
        linux-amd64/wcc linux-amd64/wlink \
        macos-arm64/wcc macos-arm64/wlink \
        macos-x64/wcc macos-x64/wlink \
        lib286/dos/clibs.lib) | sed 's/^/     /'
fi
echo
echo "  3. Remove any older vendor/openwatcom-v2/current-build-* directory"
