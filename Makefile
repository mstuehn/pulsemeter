APPLICATION = bin/watermeter

-include externals/*.mk

OBJS = main.o
OBJS += mqtt.o evdev.o
OBJS += $(EXT_OBJS)

INCDIRS = 	-I /usr/local/include \
			-I include \
			-I /usr/local/include/jsoncpp \
			-I. \
			$(EXT_INCDIRS)


LIBDIRS = -L /usr/local/lib
LIBDIRS += $(EXT_LIBDIRS)

DBGFLAGS = -g3 -O0
PRFFLAGS =
CVRFLAGS =
OPTFLAGS = -O3 -Wno-uninitialized
WARNFLAGS = -Wall

CFLAGS = $(WARNFLAGS) -std=gnu99
CPPFLAGS = $(WARNFLAGS) -std=c++17

LFLAGS =

#DEBUG_LIBS    = -lHalibD
#PROFILE_LIBS  = -lHalibP
#COVERAGE_LIBS = -lHalibC
#RELEASE_LIBS  = -lHalib

ARCH := $(shell uname -p)

#LIBS_aarch64 := -lgpio
LIBS = -lm -lmosquitto -ljsoncpp $(LIBS_$(ARCH))

.PHONY: $(SUBPROJECTS)
$(SUBPROJECTS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

vpath %.c   src
vpath %.cpp src

all: release

prepare: bin obj

debug: prepare $(SUBPROJECTS)
	$(MAKE) $(APPLICATION)D

profile: prepare $(SUBPROJECTS)
	$(MAKE) $(APPLICATION)P

coverage: prepare $(SUBPROJECTS)
	$(MAKE) $(APPLICATION)C

release: prepare $(SUBPROJECTS)
	$(MAKE) $(APPLICATION)

clean-only:
	rm -rf bin obj install

clean: $(SUBPROJECTS)
	rm -rf bin obj

.PHONY: prepare debug profile coverage release clean $(EXECUTABLE)D


EXECUTABLES = $(APPLICATION)

$(APPLICATION)D: $(addprefix obj/debug/,$(OBJS))
$(APPLICATION)P: $(addprefix obj/profile/,$(OBJS))
$(APPLICATION)C: $(addprefix obj/coverage/,$(OBJS))
$(APPLICATION): $(addprefix obj/release/,$(OBJS))


include mk/make/rules
include mk/make/arch-$(ARCHITECTURE)

-include obj/*.d
