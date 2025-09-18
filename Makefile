##
## This file is part of the libopenstm32 project.
##
## Copyright (C) ...
##

# List of supported hardware variants
HW_LIST ?= HW_FLYING_ADC_V1 HW_FLYING_ADC_V2 HW_MAX17841B

OUT_DIR      = obj/$(HW)
PREFIX       ?= arm-none-eabi
BINARY       ?= stm32_bms_$(HW)
SIZE         = $(PREFIX)-size
CC           = $(PREFIX)-gcc
CPP          = $(PREFIX)-g++
LD           = $(PREFIX)-gcc
OBJCOPY      = $(PREFIX)-objcopy
OBJDUMP      = $(PREFIX)-objdump
MKDIR_P      = mkdir -p
TERMINAL_DEBUG ?= 0

CFLAGS       = -Os -Wall -Wextra -Iinclude/ -Ilibopeninv/include -Ilibopencm3/include \
               -fno-common -fno-builtin -pedantic -DSTM32F1 \
               -mcpu=cortex-m3 -mthumb -std=gnu99 -ffunction-sections -fdata-sections

CPPFLAGS     = -Og -ggdb -Wall -Wextra -Iinclude/ -Ilibopeninv/include -Ilibopencm3/include \
               -fno-common -std=c++11 -pedantic -DSTM32F1 -DCAN_PERIPH_SPEED=32 -DCAN_SIGNED=1 -DCAN_EXT -D$(HW) \
               -ffunction-sections -fdata-sections -fno-builtin -fno-rtti -fno-exceptions -fno-unwind-tables -mcpu=cortex-m3 -mthumb

EXTRACOMPILERFLAGS  = $(shell \
	if [ -z "$$GITHUB_RUN_NUMBER" ]; then echo "-DGITHUB_RUN_NUMBER=0"; else echo "-DGITHUB_RUN_NUMBER=$$GITHUB_RUN_NUMBER"; fi \
	)

LDSCRIPT     = linker.ld
LDFLAGS      = -Llibopencm3/lib -T$(LDSCRIPT) -march=armv7 -nostartfiles -Wl,--gc-sections,-Map,linker.map

OBJSL        = main.o hwinit.o stm32scheduler.o params.o  \
               my_string.o digio.o my_fp.o printf.o anain.o picontroller.o \
               param_save.o errormessage.o stm32_can.o canhardware.o canmap.o cansdo.o sdocommands.o \
               terminalcommands.o bmsfsm.o bmsalgo.o bmsio.o temp_meas.o selftest.o
				

ifeq ($(HW), HW_FLYING_ADC_V1)
	OBJSL += flyingadcbms.o cellmux.o bitbangi2c.o mcp3421.o pca9536.o
endif
ifeq ($(HW), HW_FLYING_ADC_V2)
	OBJSL += flyingadcbms.o cellmux.o bitbangi2c.o mcp3421.o pca9536.o 
endif
ifeq ($(HW), HW_MAX17841B)
	OBJSL += maxbms.o max17841b.o
endif

OBJS         = $(patsubst %.o,$(OUT_DIR)/%.o, $(OBJSL))
DEPENDS      = $(patsubst %.o,$(OUT_DIR)/%.d, $(OBJSL))

vpath %.c src/ libopeninv/src
vpath %.cpp src/ libopeninv/src

ifneq ($(V),1)
Q := @
NULL := 2>/dev/null
endif

try-run = $(shell set -e;		\
	if ($(1)) >/dev/null 2>&1;	\
	then echo "$(2)";		\
	else echo "$(3)";		\
	fi)

comma := ,
link_command := -Wl$(comma)
ld-option = $(call try-run, $(PREFIX)-ld $(1) -v,$(link_command)$(1))
LDFLAGS += $(call ld-option,--no-warn-rwx-segments)

### Top-level default target (only if HW is not set)
ifndef HW
default: all
else
ifeq ($(filter $(HW),$(HW_LIST)),)
    $(error Unsupported hardware variant)
endif
default: directories images
endif

### Build all hardware variants
all: $(addprefix build-,$(HW_LIST))

build-%:
	@echo "==== Building for HW=$* ===="
	$(MAKE) HW=$* default

### Optional debug/Release entry points
Debug: images
Release: images

### Build images
images: $(BINARY).bin $(BINARY).hex

%.bin: %
	@printf "  OBJCOPY $@\n"
	$(Q)$(OBJCOPY) -Obinary $< $@

%.hex: %
	@printf "  OBJCOPY $@\n"
	$(Q)$(OBJCOPY) -Oihex $< $@
	$(Q)$(SIZE) $<

### Link binary
$(BINARY): $(OBJS) $(LDSCRIPT)
	@printf "  LD      $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(LD) $(LDFLAGS) -o $(BINARY) $(OBJS) -lopencm3_stm32f1 -lm

### Directory creation
directories: ${OUT_DIR}
${OUT_DIR}:
	$(Q)${MKDIR_P} ${OUT_DIR}

### Compile C
$(OUT_DIR)/%.o: %.c Makefile
	@printf "  CC      $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) -MMD -MP -o $@ -c $<

### Compile C++
$(OUT_DIR)/%.o: %.cpp Makefile
	@printf "  CPP     $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(CPP) $(CPPFLAGS) $(EXTRACOMPILERFLAGS) -MMD -MP -o $@ -c $<

-include $(DEPENDS)

### Clean all
clean:
	@printf "  CLEAN   ${OUT_DIR}\n"
	$(Q)rm -rf ${OUT_DIR}
	@for hw in $(HW_LIST); do \
		echo "  CLEAN   $(BINARY)$$hw*"; \
		rm -f $(BINARY)$$hw $(BINARY)$$hw.bin $(BINARY)$$hw.hex || true; \
	done

### Clean tests
cleanTest:
	$(MAKE) -C test clean
	$(MAKE) -C libopeninv/test clean

### Submodules and dependencies
get-deps:
	@printf "  GIT SUBMODULE\n"
	$(Q)git submodule update --init
	@printf "  MAKE libopencm3\n"
	$(Q)${MAKE} -C libopencm3 TARGETS=stm32/f1

### Run tests
Test:
	$(MAKE) -C test
	$(MAKE) -C libopeninv/test

.PHONY: all default build-% clean cleanTest get-deps Test Debug Release images directories
