APP = pingpong

# for supv, it is ok to use normal linker file
# lnk files are: sark, app.lnk and pmagent.lnk
SCRIPT = ../../include/pmapp.lnk

# List of objects making up the application. If there are other files
# in the application, add their object file names to this variable.
OBJECTS = $(APP).o

include ../Makefile.common

# If there are other files that need to be built first, put them here:
# $(BUILD_DIR)whatever.o: whatever.c
#	$(MKDIR) $(BUILD_DIR)
#	$(CC) $(CFLAGS) whatever.c -o $(BUILD_DIR)whatever.o

OUTPUT_DIR := ../../binaries/

all: $(APP).aplx
