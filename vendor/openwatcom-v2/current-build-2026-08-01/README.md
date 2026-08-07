# Open Watcom v2 vendor bundle

A trimmed snapshot of the upstream Open Watcom v2 `Current-build` release,
pinned so the repository builds reproducibly without depending on any path
outside `vendor/`.

## Upstream source

- Repository: `open-watcom/open-watcom-v2`
- Release tag: `Current-build`
- Release page:
  `https://github.com/open-watcom/open-watcom-v2/releases/tag/Current-build`
- Published at: `2026-08-01`
- Snapshot asset:
  `https://github.com/open-watcom/open-watcom-v2/releases/download/Current-build/ow-snapshot.tar.xz`

## What's vendored

The tools needed to cross-compile for **32-bit protected-mode DOS** (flat
memory model, LE executable + DOS extender):

| Tool     | Purpose                                                        |
| -------- | -------------------------------------------------------------- |
| `wcc386` | 32-bit C compiler                                              |
| `wpp386` | 32-bit C++ compiler                                            |
| `wlink`  | linker (`format os2 le` + bound stub)                          |
| `wdis`   | disassembler — used to verify a refactor changed no code       |

Per-host-platform layout:

| Repo directory   | From snapshot  | Executable format                          |
| ---------------- | -------------- | ------------------------------------------ |
| `linux-amd64/`   | `./binl64/`    | ELF 64-bit x86-64, statically linked       |
| `macos-arm64/`   | `./armo64/`    | Mach-O 64-bit arm64 (Apple Silicon)        |
| `macos-x64/`     | `./bino64/`    | Mach-O 64-bit x86-64 (Intel)               |

Shared tree:

- `h/`                    ← `./h/` minus the `nt/`, `os2/`, `win/`, `os21x/`
                            trees. The extensionless C++ headers (`<cstdio>`
                            and friends) are kept — `wpp386` needs them.
- `lib386/dos/clib3r.lib` ← 32-bit DOS C library, register calling
- `lib386/dos/plib3r.lib` ← 32-bit DOS C++ library
- `lib386/dos/math3r.lib` ← 32-bit DOS math library
- `dos/wstub.exe`         ← real-mode stub bound into the LE image
- `dos/DOS32A.EXE`        ← the DOS extender shipped on the game floppy
- `dos/DOS4GW.EXE`        ← kept for reference; see below

### Why DOS32A and not DOS/4GW

DOS32A is 27 KB against DOS/4GW's 265 KB, which matters on a 1.44 MB
floppy. More importantly, under DOS/4GW the game hangs on the title
screen: its hardware-interrupt reflection does not deliver the INT 9
keyboard chain that `new_int9` installs, so no keypress ever reaches the
state machine. Under DOS32A the same unmodified handlers work. DOS/4GW is
vendored only so that behaviour can be re-checked without another
150 MB download.

The bound stub searches for the extender under the name `DOS4GW.EXE`, so
the Makefile copies `DOS32A.EXE` to the floppy under that name.

## File checksums (SHA-256)

```text
9592d55a5d9776f9eaaaa24c1760283f1af94676eac4c2d066d0209ddbe68a11  linux-amd64/wcc386
b2877f0ba8bbd289790517d3589fa205457598626aaa442437f72ac156593e55  linux-amd64/wpp386
f44b416bd4506572ff077a2298174de404c8ff5dc1e80edf9d2b577f91ae5259  linux-amd64/wlink
983cb5e9df3fdf924962d710e441c4be3d5bfd9eb12139cff9069ba5baa0d8bd  linux-amd64/wdis
622ce8a668a3d0afe5fbbbd8d5fc58f05619a51a7a5090e938b1a2b7b26b4981  macos-arm64/wcc386
d3cbf74e85d8e766024611fc00c90f0d255f11a9016106cce2f9404dac1f262c  macos-arm64/wpp386
b0c233be72bbb990fca293ad917d5946457baaee00da4b7b20b6af1f53c7eb2e  macos-arm64/wlink
44b08c78b7fec7b60373a914a094b009d0680396ad5b36d7c9dde68c3850bc3a  macos-arm64/wdis
c6512f8a97f55414f1cee3cd311f176a796f12ffefa3866401ced8401b178860  macos-x64/wcc386
8478a39bc329d792e1c4fa988056468c6748deaa9f816471886d33fa104f7364  macos-x64/wpp386
35f72bffacf2eb48163b1ffba8f1787db9ec493372990e5a4a71e053774bdbcc  macos-x64/wlink
62eaad7f5780e1e87c69cefbf9a00afef4fb0852dc8c3119c263fc2e2afe3ea8  macos-x64/wdis
2bc0d2d8aab20fb480d19c0fad9e54b617a9201785f2df577e9f1d672d8c0787  lib386/dos/clib3r.lib
c93316305fbab5eb3789c3e1ed08df4406a442a6f54e71643eba167af568fb5f  lib386/dos/plib3r.lib
5e747b30e9d8ef2a4ba0433f9c87e2b9d3370c49288f53d555ff97cc4be3c083  lib386/dos/math3r.lib
d189be603e72f79d3c2f68114eb34d0cab8bd9744ef0f327497d57bcd8e16817  dos/DOS32A.EXE
b8265123ac8a189637448618409ef3ecd2e9f3e1a47062c685a02240f688dec1  dos/DOS4GW.EXE
81000e41e34b735ccde9f065692b6affd283a91af49d5eea73dfd43695e2b4dc  dos/wstub.exe
```

## How to refresh

Run `scripts/vendor_openwatcom.sh` from the repo root. It pulls the current
snapshot from the GitHub release, extracts the subset listed above into a
new `vendor/openwatcom-v2/current-build-<date>/`, and prints the checksums
to paste back here. Then bump `WATCOM_DIR` in the `Makefile` and delete the
previous directory.

To reuse an already-downloaded snapshot instead of fetching ~150 MB:

```sh
OW_SNAPSHOT_TAR=/path/to/ow-snapshot.tar.xz OW_SNAPSHOT_DATE=2026-08-01 \
    scripts/vendor_openwatcom.sh
```

## License

Open Watcom is distributed under the Sybase Open Watcom Public License.
Source: https://github.com/open-watcom/open-watcom-v2

DOS32A is distributed under the GNU GPL; source and documentation at
https://dos32a.narechk.net/ and in the Open Watcom snapshot.
