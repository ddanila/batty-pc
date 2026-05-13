# batty — MS-DOS recreation of Batty (Elite, 1987) targeting 8086 + VGA.
# Mirrors the adlib-rng toolchain layout: Open Watcom v2 + mtools + QEMU.

WATCOM_DIR ?= vendor/openwatcom-v2/current-build-2026-04-20
HOST_OS    := $(shell uname -s)
HOST_ARCH  := $(shell uname -m)
ifeq ($(HOST_OS),Darwin)
  ifeq ($(HOST_ARCH),arm64)
    WATCOM_BIN := $(WATCOM_DIR)/macos-arm64
  else
    WATCOM_BIN := $(WATCOM_DIR)/macos-x64
  endif
else
  WATCOM_BIN := $(WATCOM_DIR)/linux-amd64
endif

WCC        = $(WATCOM_BIN)/wcc
WLINK      = $(WATCOM_BIN)/wlink
WATCOM_H   = $(WATCOM_DIR)/h
WATCOM_LIB = $(WATCOM_DIR)/lib286/dos

# -0    = 8086 instruction set
# -ms   = small memory model (64K code + 64K data)
# -os   = optimize for size
# -s    = no stack overflow checks
# -za99 = C99
# -w4 -we = max warnings, treat as errors
# -oi   = inline intrinsics (memset/memcpy)
WCCFLAGS = -0 -ms -os -s -za99 -w4 -we -oi -i=$(WATCOM_H)

SRC     = src/main.c
OBJ     = $(SRC:src/%.c=build/%.obj)
HEADERS = $(wildcard src/*.h)
EXE     = build/batty.exe

ASSETS  = assets/loading.bin

FLOPPY_SRC ?= vendor/msdos/floppy-minimal.img
FLOPPY_OUT  = build/batty.img

ZESARUX ?= ../generaly/tools/zesarux/src/zesarux
ZRCP_PORT ?= 10000

.PHONY: all clean run floppy assets help run-original snapshot candidates regions

all: $(EXE) $(ASSETS)

help:
	@echo "batty targets:"
	@echo "  all           build $(EXE) + assets (default)"
	@echo "  assets        decode original/*.scr into assets/"
	@echo "  floppy        pack $(EXE) + assets onto $(FLOPPY_OUT)"
	@echo "  run           build the floppy and boot it in QEMU (our recreation)"
	@echo "  run-original  boot the ORIGINAL batty.tap in ZEsarUX with ZRCP open"
	@echo "  snapshot      dump RAM + screen from running ZEsarUX -> build/snapshots/"
	@echo "  regions       static scan of main blob -> build/regions.{txt,blockdef}"
	@echo "  candidates    render bytedata regions as PNGs -> assets/candidates/"
	@echo "  clean         remove build/"

build:
	@mkdir -p build

build/%.obj: src/%.c $(HEADERS) | build
	$(WCC) $(WCCFLAGS) -fo=$@ $<

$(EXE): $(OBJ)
	$(WLINK) name $@ format dos $(addprefix file ,$(OBJ)) libpath $(WATCOM_LIB) library clibs.lib

assets: $(ASSETS)

assets/loading.bin: original/Batty.scr scripts/extract_scr.py
	python3 scripts/extract_scr.py $< $@

floppy: $(FLOPPY_OUT)

$(FLOPPY_OUT): $(EXE) $(ASSETS) $(FLOPPY_SRC)
	cp "$(FLOPPY_SRC)" $@
	mcopy -i $@ -o $(EXE) ::BATTY.EXE
	mcopy -i $@ -o assets/loading.bin ::LOADING.BIN
	@printf '@ECHO OFF\r\nBATTY\r\n' > build/AUTOEXEC.BAT
	mcopy -i $@ -o build/AUTOEXEC.BAT ::AUTOEXEC.BAT
	@echo "Floppy ready: $@"

run: $(FLOPPY_OUT)
	bash scripts/run.sh $(FLOPPY_OUT)

# --- Reverse-engineering helpers ---

regions: build/regions.txt

build/regions.txt build/regions.blockdef: \
		original/blocks/03_DATA_headless.dat.bin scripts/scan_regions.py
	@mkdir -p build
	python3 scripts/scan_regions.py $< build/regions.txt build/regions.blockdef

candidates: build/regions.blockdef
	python3 scripts/render_candidates.py \
		original/blocks/03_DATA_headless.dat.bin \
		build/regions.blockdef assets/candidates

run-original:
	$(ZESARUX) --noconfigfile --machine 48k \
		--enable-remoteprotocol --remoteprotocol-port $(ZRCP_PORT) \
		--quickexit $(CURDIR)/original/batty.tap

snapshot:
	@mkdir -p build/snapshots
	python3 scripts/snapshot_ram.py build/snapshots

clean:
	rm -rf build
