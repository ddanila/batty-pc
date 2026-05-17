# MS-DOS boot floppy vendor bundle

Bootable 1.44 MB MS-DOS 4.0 floppy image the build wraps around
`ADLIB.EXE` + the transcoded VGMs + `AUTOEXEC.BAT`.

## Upstream source

- Repository: `ddanila/msdos`
- Release tag: `0.1`
- Release page: `https://github.com/ddanila/msdos/releases/tag/0.1`
- Published at: `2026-04-03T14:13:35Z`
- Asset: `floppy-minimal.img`  (1,474,560 bytes)

## Checksum

```text
65b69de19b2b9a8917e8c0e9a26d7b1728989dcfd5970e12cca7b4bc9b0c6b25  floppy-minimal.img
```

## Refreshing

```sh
gh release download 0.1 --repo ddanila/msdos \
    --pattern floppy-minimal.img --dir vendor/msdos --clobber
shasum -a 256 vendor/msdos/floppy-minimal.img   # then update above
```
