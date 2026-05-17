#!/usr/bin/env bash
# Download the MS-DOS minimal boot floppy from the ddanila/msdos release
# into vendor/msdos/. Run from the repo root.
#
# Requires: gh, shasum (or sha256sum).

set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

dst="vendor/msdos"
mkdir -p "$dst"

echo "==> fetching floppy-minimal.img from ddanila/msdos@0.1"
gh release download 0.1 --repo ddanila/msdos \
    --pattern floppy-minimal.img --dir "$dst" --clobber

echo "==> done: $(du -sh "$dst/floppy-minimal.img" | cut -f1)"
echo
if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$dst/floppy-minimal.img"
else
    sha256sum "$dst/floppy-minimal.img"
fi
echo
echo "Update the checksum in $dst/README.md if it changed."
