APP = supv

# for supv, it is ok to use normal linker file
# other lnk are: pmapp.lnk and pmagent.lnk
SCRIPT = ../include/supv.lnk

# List of objects making up the application. If there are other files
# in the application, add their object file names to this variable.
OBJECTS = $(APP).o

# Common makefile for pmapps
# pmagent might use SARK (0), but apps might use API (1)
ifndef API
    API := 1
endif

# First, define SPINN_DIRS and the correct library
SPINN_DIRS := ../../spinnaker_tools_pm/ori_134
SPINN_TOOLS := $(SPINN_DIRS)/tools

ifeq ($(API),1)
    LIBRARY := ../include/libspin1_api-ori.a
else
    LIBRARY := ../include/libsark-ori.a
endif

ifndef GNUBIN
    GNUBIN := /opt/arm-2013.05/bin
endif

# We mainly use GNU toolchain
GNU := 1

# Set to 1 to make Thumb code (0 for ARM)
THUMB := 1

# Prefix for GNU tool binaries
GP  := $(GNUBIN)/arm-none-eabi

INC_DIR := $(SPINN_DIRS)/include
BUILD_DIR := build/

#-------------------------------------------------------------------------------

# Set up the various compile/link options for GNU tools

AS := $(GP)-as --defsym GNU=1 -mthumb-interwork -march=armv5te
CA := $(GP)-gcc -c -Os -mthumb-interwork -march=armv5te -std=gnu99 -I $(INC_DIR)
CT := $(CA) -mthumb -DTHUMB
LD := $(GP)-gcc -T$(SCRIPT) -Wl,-e,cpu_reset -Wl,--gc-sections -Wl,--use-blx
AR := $(GP)-ar -rcs
OC := $(GP)-objcopy
OD := $(GP)-objdump -dxt > $(BUILD_DIR)$(APP).txt

ifeq ($(THUMB),1)
  CC := $(CT)
else
  CC := $(CA)
endif

CAT := \cat
RM  := \rm -f
LS  := \ls -l
RM  := \rm -f
MKDIR := \mkdir -p
CP  := \cp -rf

#-------------------------------------------------------------------------------

# Build the application

# Primary target is an APLX file - built from the ELF
$(APP).aplx: $(APP).elf
ifeq ($(GNU),1)
	$(OC) -O binary -j RO_DATA -j .ARM.exidx $(BUILD_DIR)$(APP).elf $(BUILD_DIR)RO_DATA.bin
	$(OC) -O binary -j RW_DATA $(BUILD_DIR)$(APP).elf $(BUILD_DIR)RW_DATA.bin
	$(SPINN_TOOLS)/mkbin $(BUILD_DIR)RO_DATA.bin $(BUILD_DIR)RW_DATA.bin > $(BUILD_DIR)$(APP).bin
else
	$(OC) --bin --output $(APP).bin $(APP).elf
endif
	$(SPINN_TOOLS)/mkaplx $(BUILD_DIR)$(APP).elf | $(CAT) - $(BUILD_DIR)$(APP).bin > $(BUILD_DIR)$(APP).aplx
	@$(CP) $(BUILD_DIR)$(APP).aplx $(OUTPUT_DIR)
	@$(LS) $(OUTPUT_DIR)$(APP).aplx

# Build the ELF file
$(APP).elf: $(BUILD_DIR)$(OBJECTS) $(SCRIPT) $(LIBRARY)
	@$(MKDIR) $(BUILD_DIR)
	$(SPINN_TOOLS)/mkbuild $(APP) > $(BUILD_DIR)sark_build.c
	$(CC) $(BUILD_DIR)sark_build.c -o $(BUILD_DIR)sark_build.o
	$(LD) $(LFLAGS) $(BUILD_DIR)$(OBJECTS) $(BUILD_DIR)sark_build.o $(LIBRARY) -o $(BUILD_DIR)$(APP).elf
	$(OD) $(BUILD_DIR)$(APP).elf


# Build the main object file.
$(BUILD_DIR)$(APP).o: $(APP).c
	@$(MKDIR) $(BUILD_DIR)
	$(CC) $(CFLAGS) $(APP).c -o $(BUILD_DIR)$(APP).o

clean:
	@$(RM) $(BUILD_DIR)*
	@$(RM) $(OUTPUT_DIR)$(APP).aplx
#-------------------------------------------------------------------------------

# If there are other files that need to be built first, put them here:
# $(BUILD_DIR)whatever.o: whatever.c
#	$(MKDIR) $(BUILD_DIR)
#	$(CC) $(CFLAGS) whatever.c -o $(BUILD_DIR)whatever.o

OUTPUT_DIR := ../binaries/

all: $(APP).aplx
