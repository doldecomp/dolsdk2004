#################################################################
#              Dolphin SDK 2004 Libraries Makefile              #
#################################################################

include util.mk

# Apr 20 2004 - 0
# May 21 2004 - 1 (Patch 1)
# Nov 10 2004 - 2 (Patch 2)
SDK_REVISION := 2

# If 0, tells the console to chill out. (Quiets the make process.)
VERBOSE ?= 0

# Force correct find on Windows
ifeq ($(OS),Windows_NT)

# Attempt to locate MSYS or Git Bash find.exe
FIND_CANDIDATES := C:/msys64/usr/bin/find.exe \
                   C:/Program\ Files/Git/usr/bin/find.exe

define FIND_MSYS
  $(foreach c,$(FIND_CANDIDATES),\
    $(if $(wildcard $(c)),$(c),))
endef

FOUND_MSYS_FIND := $(firstword $(FIND_MSYS))

ifeq ($(FOUND_MSYS_FIND),)
  $(warning Could not find an MSYS or Git Bash "find.exe". Falling back to Windows find.)
  FIND := find
else
  FIND := "$(FOUND_MSYS_FIND)"
endif

else
  # On non-Windows (Linux, macOS), just assume the normal find.
  FIND := find
endif


# Host OS detection
ifeq ($(OS),Windows_NT)
  HOST_OS := windows
  EXE := .exe
else
  WINE ?=
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Linux)
    HOST_OS := linux
  else ifeq ($(UNAME_S),Darwin)
    HOST_OS := macos
  else
    $(error Unsupported host/building OS <$(UNAME_S)>)
  endif
endif

BUILD_DIR := build
TOOLS_DIR := $(BUILD_DIR)/tools
BASEROM_DIR := baserom
TARGET_LIBS := G2D              \
               ai               \
               am               \
               amcnotstub       \
               amcstubs         \
               ar               \
               ax               \
               axart            \
               axfx             \
               base             \
               card             \
               db               \
               demo             \
               dsp              \
               dtk              \
               dvd              \
               exi              \
               fileCache        \
               gd               \
               gx               \
               hio              \
               mcc              \
               mix              \
               mtx              \
               odemustubs       \
               odenotstub       \
               os               \
               pad              \
               perf             \
               seq              \
               si               \
               sp               \
               support          \
               syn              \
               texPalette       \
               vi

VERIFY_LIBS := $(filter-out amcstubs dsp odemustubs,$(TARGET_LIBS))

ifeq ($(VERBOSE),0)
  QUIET := @
endif

PYTHON ?= python3

# Every file has a debug version. Append D to the list.
TARGET_LIBS_DEBUG := $(addsuffix D,$(TARGET_LIBS))

# TODO, decompile
SRC_DIRS := $(shell $(FIND) src -type d)

###################### Other Tools ######################

C_FILES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
S_FILES := $(foreach dir,$(SRC_DIRS) $(ASM_DIRS),$(wildcard $(dir)/*.s))
DATA_FILES := $(foreach dir,$(DATA_DIRS),$(wildcard $(dir)/*.bin))
BASEROM_FILES := $(foreach dir,$(BASEROM_DIRS)\,$(wildcard $(dir)/*.s))

# Object files
O_FILES := $(foreach file,$(C_FILES),$(BUILD_DIR)/$(file:.c=.c.o)) \
           $(foreach file,$(S_FILES),$(BUILD_DIR)/$(file:.s=.s.o)) \
           $(foreach file,$(DATA_FILES),$(BUILD_DIR)/$(file:.bin=.bin.o)) \

DEP_FILES := $(O_FILES:.o=.d) $(DECOMP_C_OBJS:.o=.asmproc.d)

##################### Compiler Options #######################
# Use a helper to detect if a command is found, but handle Windows vs. Unix
ifeq ($(HOST_OS),windows)
  # In Windows (MSYS), "where" returns 0 if found, 1 if not found
  find-command = $(shell where $(1) 1>NUL 2>NUL && echo 0 || echo 1)
else
  # In Unix, "type" returns 0 if found, non-zero if not found
  find-command = $(shell type $(1) >/dev/null 2>/dev/null; echo $$?)
endif

# detect prefix for PowerPC toolchain
ifneq ($(call find-command,powerpc-linux-gnu-ld),0)
  ifneq ($(call find-command,powerpc-eabi-ld),0)
    $(error Unable to detect a suitable PowerPC toolchain installed. Please install/configure one!)
  else
    CROSS := powerpc-eabi-
  endif
else
  CROSS := powerpc-linux-gnu-
endif

COMPILER_VERSION ?= 1.2.5n

COMPILER_DIR := mwcc_compiler/GC/$(COMPILER_VERSION)
AS = $(CROSS)as
AR = $(CROSS)ar
LD = $(CROSS)ld
OBJDUMP = $(CROSS)objdump
OBJCOPY = $(CROSS)objcopy

# On Windows, you can run the compiler natively.
# If you still want to rely on Wine, you can keep it. If not, remove "$(WINE) ".
MWCC    := $(WINE) $(COMPILER_DIR)/mwcceppc$(EXE)

ifeq ($(HOST_OS),macos)
  CPP := clang -E -P -x c
else ifeq ($(HOST_OS),windows)
  # If using MSYS, we often have `cpp` installed. If you prefer clang, adjust:
  CPP := cpp -P -x c
else
  CPP := cpp
endif

DTK     := $(TOOLS_DIR)/dtk$(EXE)
DTK_VERSION := 0.7.4
SJISWRAP := $(TOOLS_DIR)/sjiswrap$(EXE)
SJISWRAP_VERSION := 1.2.0

CC        = $(MWCC)

######################## Flags #############################

CHARFLAGS          := -char unsigned
RELEASE_OPTLEVEL   := -O4,p
SYM_ON             := -sym on

# why. Did some SDK libs (like CARD) prefer char signed over unsigned? TODO: Figure out consistency behind this.
build/debug/src/card/CARDRename.o: CHARFLAGS := -char signed
build/release/src/card/CARDRename.o: CHARFLAGS := -char signed
build/debug/src/card/CARDOpen.o: CHARFLAGS := -char signed
build/release/src/card/CARDOpen.o: CHARFLAGS := -char signed

build/debug/src/dvd/%.o: CHARFLAGS := -char signed
build/release/src/dvd/%.o: CHARFLAGS := -char signed

build/debug/src/mtx/mtx.o: CHARFLAGS := -char signed
build/release/src/mtx/mtx.o: CHARFLAGS := -char signed
build/debug/src/mtx/mtx44.o: CHARFLAGS := -char signed
build/release/src/mtx/mtx44.o: CHARFLAGS := -char signed

build/debug/src/demo/%.o: CFLAGS += -char signed
build/release/src/demo/%.o: CFLAGS += -char signed

build/release/src/exi/EXIBios.o: RELEASE_OPTLEVEL := -O3,p

# no sym on for these?
build/release/src/gd/GDIndirect.o: SYM_ON := 
build/release/src/G2D/G2D.o: SYM_ON := 

%/stub.o: CFLAGS += -warn off

CFLAGS = $(CHARFLAGS) -nodefaults -proc gekko -fp hard -Cpp_exceptions off -enum int -warn pragmas -requireprotos -pragma 'cats off' -D__GEKKO__ -DSDK_REVISION=$(SDK_REVISION)
INCLUDES := -Iinclude -Iinclude/libc -ir src

ASFLAGS = -mgekko -I src -I include

######################## Targets #############################

$(foreach dir,$(SRC_DIRS) $(ASM_DIRS) $(DATA_DIRS),$(shell mkdir -p build/release/$(dir) build/debug/$(dir)))

######################## Build #############################

A_FILES := $(foreach dir,$(BASEROM_DIR),$(wildcard $(dir)/*.a))

TARGET_LIBS := $(addprefix baserom/,$(addsuffix .a,$(TARGET_LIBS)))
TARGET_LIBS_DEBUG := $(addprefix baserom/,$(addsuffix .a,$(TARGET_LIBS_DEBUG)))

default: all

all: $(DTK) amcnotstub.a amcnotstubD.a gx.a gxD.a hio.a hioD.a amcstubs.a amcstubsD.a odemustubs.a odemustubsD.a odenotstub.a odenotstubD.a vi.a viD.a os.a osD.a card.a cardD.a pad.a padD.a exi.a exiD.a mtx.a mtxD.a mcc.a mccD.a gd.a gdD.a si.a siD.a dvd.a dvdD.a base.a baseD.a ai.a aiD.a ar.a arD.a db.a dbD.a dsp.a dspD.a G2D.a G2DD.a dtk.a dtkD.a demo.a demoD.a seq.a seqD.a syn.a synD.a mix.a mixD.a perf.a perfD.a support.a supportD.a fileCache.a fileCacheD.a texPalette.a texPaletteD.a ax.a axD.a axfx.a axfxD.a axart.a axartD.a sp.a spD.a am.a amD.a

verify: build/release/test.bin build/debug/test.bin build/verify.sha1
	@sha1sum -c build/verify.sha1

extract: $(DTK)
	$(info Extracting files...)
	@$(DTK) ar extract $(TARGET_LIBS) --out baserom/release/src
	@$(DTK) ar extract $(TARGET_LIBS_DEBUG) --out baserom/debug/src
    # Thank you GPT, very cool. Temporary hack to remove D off of inner src folders to let objdiff work.
	@for dir in $$($(FIND) baserom/debug/src -type d -name 'src'); do \
		$(FIND) "$$dir" -mindepth 1 -maxdepth 1 -type d | while read subdir; do \
			mv "$$subdir" "$${subdir%?}"; \
		done \
	done
	# Disassemble the objects and extract their dwarf info.
	$(FIND) baserom -name '*.o' | while read i; do \
		$(DTK) elf disasm $$i $${i%.o}.s ; \
		$(DTK) dwarf dump $$i -o $${i%.o}_DWARF.c ; \
	done

# clean extraction so extraction can be done again.
distclean:
	rm -rf $(BASEROM_DIR)/release
	rm -rf $(BASEROM_DIR)/debug
	make clean

clean:
	rm -rf $(BUILD_DIR)
	rm -rf *.a

$(TOOLS_DIR):
	$(QUIET) mkdir -p $(TOOLS_DIR)

.PHONY: check-dtk

check-dtk: $(TOOLS_DIR)
	@version=$$($(DTK) --version | awk '{print $$2}'); \
	if [ "$(DTK_VERSION)" != "$$version" ]; then \
		$(PYTHON) tools/download_dtk.py dtk $(DTK) --tag "v$(DTK_VERSION)"; \
    $(PYTHON) tools/download_dtk.py sjiswrap $(SJISWRAP) --tag "v$(SJISWRAP_VERSION)"; \
	fi

$(DTK): check-dtk

build/debug/src/%.o: src/%.c
	@echo 'Compiling $< (debug)'
	$(QUIET)$(SJISWRAP)$(CC) -c -opt level=0 -inline off -schedule off -sym on $(CFLAGS) -I- $(INCLUDES) -DDEBUG $< -o $@

build/release/src/%.o: src/%.c
	@echo 'Compiling $< (release)'
	$(QUIET)$(SJISWRAP)$(CC) -c $(RELEASE_OPTLEVEL) -inline auto $(SYM_ON) $(CFLAGS) -I- $(INCLUDES) -DRELEASE $< -o $@

################################ Build AR Files ###############################

ai_c_files := $(wildcard src/ai/*.c)
ai.a  : $(addprefix $(BUILD_DIR)/release/,$(ai_c_files:.c=.o))
aiD.a : $(addprefix $(BUILD_DIR)/debug/,$(ai_c_files:.c=.o))

am_c_files := $(wildcard src/am/*.c)
am.a  : $(addprefix $(BUILD_DIR)/release/,$(am_c_files:.c=.o))
amD.a : $(addprefix $(BUILD_DIR)/debug/,$(am_c_files:.c=.o))

amcnotstub_c_files := $(wildcard src/amcnotstub/*.c)
amcnotstub.a  : $(addprefix $(BUILD_DIR)/release/,$(amcnotstub_c_files:.c=.o))
amcnotstubD.a : $(addprefix $(BUILD_DIR)/debug/,$(amcnotstub_c_files:.c=.o))

amcstubs_c_files := $(wildcard src/amcstubs/*.c)
amcstubs.a  : $(addprefix $(BUILD_DIR)/release/,$(amcstubs_c_files:.c=.o))
amcstubsD.a : $(addprefix $(BUILD_DIR)/debug/,$(amcstubs_c_files:.c=.o))

ar_c_files := $(wildcard src/ar/*.c)
ar.a  : $(addprefix $(BUILD_DIR)/release/,$(ar_c_files:.c=.o))
arD.a : $(addprefix $(BUILD_DIR)/debug/,$(ar_c_files:.c=.o))

ax_c_files := $(wildcard src/ax/*.c)
ax.a  : $(addprefix $(BUILD_DIR)/release/,$(ax_c_files:.c=.o))
axD.a : $(addprefix $(BUILD_DIR)/debug/,$(ax_c_files:.c=.o))

axart_c_files := $(wildcard src/axart/*.c)
axart.a  : $(addprefix $(BUILD_DIR)/release/,$(axart_c_files:.c=.o))
axartD.a : $(addprefix $(BUILD_DIR)/debug/,$(axart_c_files:.c=.o))

axfx_c_files := $(wildcard src/axfx/*.c)
axfx.a  : $(addprefix $(BUILD_DIR)/release/,$(axfx_c_files:.c=.o))
axfxD.a : $(addprefix $(BUILD_DIR)/debug/,$(axfx_c_files:.c=.o))

base_c_files := $(wildcard src/base/*.c)
base.a  : $(addprefix $(BUILD_DIR)/release/,$(base_c_files:.c=.o))
baseD.a : $(addprefix $(BUILD_DIR)/debug/,$(base_c_files:.c=.o))

card_c_files := $(wildcard src/card/*.c)
card.a  : $(addprefix $(BUILD_DIR)/release/,$(card_c_files:.c=.o))
cardD.a : $(addprefix $(BUILD_DIR)/debug/,$(card_c_files:.c=.o))

db_c_files := $(wildcard src/db/*.c)
db.a  : $(addprefix $(BUILD_DIR)/release/,$(db_c_files:.c=.o))
dbD.a : $(addprefix $(BUILD_DIR)/debug/,$(db_c_files:.c=.o))

demo_c_files := $(wildcard src/demo/*.c)
demo.a  : $(addprefix $(BUILD_DIR)/release/,$(demo_c_files:.c=.o))
demoD.a : $(addprefix $(BUILD_DIR)/debug/,$(demo_c_files:.c=.o))

dsp_c_files := $(wildcard src/dsp/*.c)
dsp.a  : $(addprefix $(BUILD_DIR)/release/,$(dsp_c_files:.c=.o))
dspD.a : $(addprefix $(BUILD_DIR)/debug/,$(dsp_c_files:.c=.o))

dtk_c_files := $(wildcard src/dtk/*.c)
dtk.a  : $(addprefix $(BUILD_DIR)/release/,$(dtk_c_files:.c=.o))
dtkD.a : $(addprefix $(BUILD_DIR)/debug/,$(dtk_c_files:.c=.o))

dvd_c_files := $(wildcard src/dvd/*.c)
dvd.a  : $(addprefix $(BUILD_DIR)/release/,$(dvd_c_files:.c=.o))
dvdD.a  : $(addprefix $(BUILD_DIR)/debug/,$(dvd_c_files:.c=.o))

exi_c_files := $(wildcard src/exi/*.c)
exi.a  : $(addprefix $(BUILD_DIR)/release/,$(exi_c_files:.c=.o))
exiD.a : $(addprefix $(BUILD_DIR)/debug/,$(exi_c_files:.c=.o))

fileCache_c_files := $(wildcard src/fileCache/*.c)
fileCache.a  : $(addprefix $(BUILD_DIR)/release/,$(fileCache_c_files:.c=.o))
fileCacheD.a : $(addprefix $(BUILD_DIR)/debug/,$(fileCache_c_files:.c=.o))

gx_c_files := $(wildcard src/gx/*.c)
gx.a  : $(addprefix $(BUILD_DIR)/release/,$(gx_c_files:.c=.o))
gxD.a : $(addprefix $(BUILD_DIR)/debug/,$(gx_c_files:.c=.o))

G2D_c_files := $(wildcard src/G2D/*.c)
G2D.a  : $(addprefix $(BUILD_DIR)/release/,$(G2D_c_files:.c=.o))
G2DD.a : $(addprefix $(BUILD_DIR)/debug/,$(G2D_c_files:.c=.o))

gd_c_files := $(wildcard src/gd/*.c)
gd.a  : $(addprefix $(BUILD_DIR)/release/,$(gd_c_files:.c=.o))
gdD.a : $(addprefix $(BUILD_DIR)/debug/,$(gd_c_files:.c=.o))

hio_c_files := $(wildcard src/hio/*.c)
hio.a  : $(addprefix $(BUILD_DIR)/release/,$(hio_c_files:.c=.o))
hioD.a : $(addprefix $(BUILD_DIR)/debug/,$(hio_c_files:.c=.o))

mcc_c_files := $(wildcard src/mcc/*.c)
mcc.a  : $(addprefix $(BUILD_DIR)/release/,$(mcc_c_files:.c=.o))
mccD.a : $(addprefix $(BUILD_DIR)/debug/,$(mcc_c_files:.c=.o))

mix_c_files := $(wildcard src/mix/*.c)
mix.a  : $(addprefix $(BUILD_DIR)/release/,$(mix_c_files:.c=.o))
mixD.a : $(addprefix $(BUILD_DIR)/debug/,$(mix_c_files:.c=.o))

mtx_c_files := $(wildcard src/mtx/*.c)
mtx.a  : $(addprefix $(BUILD_DIR)/release/,$(mtx_c_files:.c=.o))
mtxD.a : $(addprefix $(BUILD_DIR)/debug/,$(mtx_c_files:.c=.o))

odemustubs_c_files := $(wildcard src/odemustubs/*.c)
odemustubs.a  : $(addprefix $(BUILD_DIR)/release/,$(odemustubs_c_files:.c=.o))
odemustubsD.a : $(addprefix $(BUILD_DIR)/debug/,$(odemustubs_c_files:.c=.o))

odenotstub_c_files := $(wildcard src/odenotstub/*.c)
odenotstub.a  : $(addprefix $(BUILD_DIR)/release/,$(odenotstub_c_files:.c=.o))
odenotstubD.a : $(addprefix $(BUILD_DIR)/debug/,$(odenotstub_c_files:.c=.o))

os_c_files := $(wildcard src/os/*.c)
os.a  : $(addprefix $(BUILD_DIR)/release/,$(os_c_files:.c=.o))
osD.a : $(addprefix $(BUILD_DIR)/debug/,$(os_c_files:.c=.o))

pad_c_files := $(wildcard src/pad/*.c)
pad.a  : $(addprefix $(BUILD_DIR)/release/,$(pad_c_files:.c=.o))
padD.a : $(addprefix $(BUILD_DIR)/debug/,$(pad_c_files:.c=.o))

perf_c_files := $(wildcard src/perf/*.c)
perf.a  : $(addprefix $(BUILD_DIR)/release/,$(perf_c_files:.c=.o))
perfD.a : $(addprefix $(BUILD_DIR)/debug/,$(perf_c_files:.c=.o))

seq_c_files := $(wildcard src/seq/*.c)
seq.a  : $(addprefix $(BUILD_DIR)/release/,$(seq_c_files:.c=.o))
seqD.a : $(addprefix $(BUILD_DIR)/debug/,$(seq_c_files:.c=.o))

si_c_files := $(wildcard src/si/*.c)
si.a  : $(addprefix $(BUILD_DIR)/release/,$(si_c_files:.c=.o))
siD.a : $(addprefix $(BUILD_DIR)/debug/,$(si_c_files:.c=.o))

sp_c_files := $(wildcard src/sp/*.c)
sp.a  : $(addprefix $(BUILD_DIR)/release/,$(sp_c_files:.c=.o))
spD.a : $(addprefix $(BUILD_DIR)/debug/,$(sp_c_files:.c=.o))

support_c_files := $(wildcard src/support/*.c)
support.a  : $(addprefix $(BUILD_DIR)/release/,$(support_c_files:.c=.o))
supportD.a : $(addprefix $(BUILD_DIR)/debug/,$(support_c_files:.c=.o))

syn_c_files := $(wildcard src/syn/*.c)
syn.a  : $(addprefix $(BUILD_DIR)/release/,$(syn_c_files:.c=.o))
synD.a : $(addprefix $(BUILD_DIR)/debug/,$(syn_c_files:.c=.o))

texPalette_c_files := $(wildcard src/texPalette/*.c)
texPalette.a  : $(addprefix $(BUILD_DIR)/release/,$(texPalette_c_files:.c=.o))
texPaletteD.a : $(addprefix $(BUILD_DIR)/debug/,$(texPalette_c_files:.c=.o))

vi_c_files := $(wildcard src/vi/*.c)
vi.a  : $(addprefix $(BUILD_DIR)/release/,$(vi_c_files:.c=.o))
viD.a : $(addprefix $(BUILD_DIR)/debug/,$(vi_c_files:.c=.o))

build/release/baserom.elf: build/release/src/stub.o $(foreach l,$(VERIFY_LIBS),baserom/$(l).a)
build/release/test.elf:    build/release/src/stub.o $(foreach l,$(VERIFY_LIBS),$(l).a)
build/debug/baserom.elf:   build/release/src/stub.o $(foreach l,$(VERIFY_LIBS),baserom/$(l)D.a)
build/debug/test.elf:      build/release/src/stub.o $(foreach l,$(VERIFY_LIBS),$(l)D.a)

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

%.elf:
	@echo Linking ELF $@
	$(QUIET)$(LD) -T gcn.ld --whole-archive $(filter %.o,$^) $(filter %.a,$^) -o $@ -Map $(@:.elf=.map)

%.a:
	@ test ! -z '$?' || { echo 'no object files for $@'; return 1; }
	@echo 'Creating static library $@'
	$(QUIET)$(AR) -v -r $@ $(filter %.o,$?)

# generate baserom hashes
build/verify.sha1: build/release/baserom.bin build/debug/baserom.bin
	sha1sum $^ | sed 's/baserom/test/' > $@

# ------------------------------------------------------------------------------

.PHONY: all clean distclean default split setup extract

print-% : ; $(info $* is a $(flavor $*) variable set to [$($*)]) @true

-include $(DEP_FILES)
