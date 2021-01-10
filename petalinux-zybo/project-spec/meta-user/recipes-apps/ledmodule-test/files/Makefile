# -------------------------------------------------------------------------------
#  PROJECT: Zybo Base
# -------------------------------------------------------------------------------
#  AUTHORS: Pavel Benacek <pavel.benacek@gmail.com>
#  LICENSE: The MIT License (MIT), please read LICENSE file
#  WEBSITE: https://github.com/benycze/zybo-base
# -------------------------------------------------------------------------------

###############################################################################
# Export helping variables for the compilation via Makefile
## Root of the project
PROJ_ROOT=$(shell pwd)/../../../../..
# Cross compilation settings
export ARCH=arm
export CROSS_COMPILE:=arm-linux-gnueabihf-

CC=$(CROSS_COMPILE)gcc

###############################################################################

APP = ledmodule-test

# Add any other object files to this list below
APP_OBJS = ledmodule-test.o

all: print_config build

build: print_config $(APP)

$(APP): print_config $(APP_OBJS)
	$(CC) -o $@ $(APP_OBJS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(APP) *.o

print_config:
	@echo "#######################################################"
	@echo "Using the following configuration"
	@echo " * CC = ${CC}"
	@echo " * LDFLAGS = ${LDFLAGS}"
	@echo " * LDLIBS = ${LDLIBS}"
	@echo "#######################################################"
