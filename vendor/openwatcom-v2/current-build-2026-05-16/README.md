# Open Watcom v2 vendor bundle

A trimmed snapshot of the upstream Open Watcom v2 `Current-build` release,
pinned so the repository builds reproducibly without depending on any path
outside `vendor/`.

## Upstream source

- Repository: `open-watcom/open-watcom-v2`
- Release tag: `Current-build`
- Release page:
  `https://github.com/open-watcom/open-watcom-v2/releases/tag/Current-build`
- Published at: `2026-05-16T05:38:20Z`
- Snapshot asset:
  `https://github.com/open-watcom/open-watcom-v2/releases/download/Current-build/ow-snapshot.tar.xz`
- Snapshot asset SHA-256:
  `ec0bebd973ee339dc0425db184ab82a20e0fefee8be839ec75a366f47b12c86a`
- Snapshot asset size: 150,249,056 bytes

## What's vendored

Only the tools needed to cross-compile for 16-bit DOS (`wcc`, `wlink`), plus
the `h/` directory minus the OS-specific trees (`nt/`, `os2/`, `win/`,
`os21x/`) we don't target, and the small-model `clibs.lib`.

Per-host-platform layout:

| Repo directory   | From snapshot  | Executable format                          |
| ---------------- | -------------- | ------------------------------------------ |
| `linux-amd64/`   | `./binl64/`    | ELF 64-bit x86-64, statically linked       |
| `macos-arm64/`   | `./armo64/`    | Mach-O 64-bit arm64 (Apple Silicon)        |
| `macos-x64/`     | `./bino64/`    | Mach-O 64-bit x86-64 (Intel)               |

Shared tree:

- `h/`                      ← `./h/` with `nt/`, `os2/`, `win/`, `os21x/` and
                              extensionless C++ STL headers removed
- `lib286/dos/clibs.lib`    ← `./lib286/dos/clibs.lib` (small model C lib)

## File checksums (SHA-256)

```text
6655b9e09900635ff91e44bf58be30a6db247e17444d8bdd6da0c9dfd58d00cb  linux-amd64/wcc
70e0305e9f3e2fed2ba046f0cf9ce177bc53acf82bb8cb7415ecc6966d87ec0f  linux-amd64/wlink
2855bc3d41da800f8a1493d61401de427095c6b58f546eba723c0dc6fa8fb118  macos-arm64/wcc
497d7204cb7a072d62ae017c92c7aee92af05e6a33f329856ac6ddc78919f211  macos-arm64/wlink
57df5f38273826cfe945568823cd30547f10cfc3b60e0bde0bf3242f6448906a  macos-x64/wcc
df2611ca7f834b7914bf61a922e7d082e51fa09995c485a23e2315231558e947  macos-x64/wlink
bd4e17ef14c62396874264c4e2f0f46457396850640fcb3284a92785a57af073  lib286/dos/clibs.lib
```

## How to refresh

Run `scripts/vendor_openwatcom.sh` from the repo root. It:

1. pulls the current snapshot from the GitHub release,
2. extracts only the subset listed above into a new
   `vendor/openwatcom-v2/current-build-<date>/` directory,
3. prints the new checksums so they can be committed into this README.

After running it, bump the `WATCOM_DIR` default in the `Makefile` to the new
date and remove the previous directory.

## License

Open Watcom is distributed under the Sybase Open Watcom Public License.
Source: https://github.com/open-watcom/open-watcom-v2
