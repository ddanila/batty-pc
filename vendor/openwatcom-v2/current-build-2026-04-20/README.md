# Open Watcom v2 vendor bundle

A trimmed snapshot of the upstream Open Watcom v2 `Current-build` release,
pinned so the repository builds reproducibly without depending on any path
outside `vendor/`.

## Upstream source

- Repository: `open-watcom/open-watcom-v2`
- Release tag: `Current-build`
- Release page:
  `https://github.com/open-watcom/open-watcom-v2/releases/tag/Current-build`
- Published at: `2026-04-20T05:14:54Z`
- Snapshot asset:
  `https://github.com/open-watcom/open-watcom-v2/releases/download/Current-build/ow-snapshot.tar.xz`
- Snapshot asset SHA-256:
  `76a568c1b803f92eb850287a3774bc18595d3ef08a973d6ff2ae9b693e5d45a7`
- Snapshot asset size: 149,243,588 bytes

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
8e212f64aac2daac4fdfe40a4743913035483cbca799aae95d3e87be3e1fec4f  linux-amd64/wcc
6c6adb16e7a72f0d654e1a87af0c0d6541009ce150bca3e00d4e0404ef3295d3  linux-amd64/wlink
6431aa4dd6c4e89d83e49eea327e63c2c5ce44073bb6b25344b9dfba263d9ad8  macos-arm64/wcc
530340cebbf007bb8d4ae576e382efd20317045de098a115ae4ea09e503b0919  macos-arm64/wlink
97bb7ce8eb93bbee9d3ea1b5a71a2078ff8a22a01b6888318cb102e9467ce543  macos-x64/wcc
4001ce9052a4fc78b59fce44fe0a4ebab4632879a31b95d8c8cdc19acd6063a8  macos-x64/wlink
c7740037d867beb1d7450b072b923e6b6e7b2554c74369949b42f7b4ebfee0eb  lib286/dos/clibs.lib
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
