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
KERNEL_SRC=${PROJ_ROOT}/build/tmp/work/zynq_generic-xilinx-linux-gnueabi/linux-xlnx/5.4*/linux-xlnx*/
# Makefile body ###############################################################
# Put all default flags here
MY_CFLAGS += -g -O0

obj-m := led-module.o
ccflags-y += ${MY_CFLAGS}

SRC := $(shell pwd)

all: print_config
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

modules_install: print_config
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c *.a *.mod
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers

print_config:
	@echo "#######################################################"
	@echo "Using the following configuration"
	@echo "	* CC = ${CC}"
	@echo "	* KERNEL_SRC = ${KERNEL_SRC}"
	@echo "	* cflags-y = ${ccflags-y}"
	@echo "#######################################################"
