# Zybo Base Image

This repository contains the basic template which can be used for the template for development of another Zynq based images.
It can be also considered as tutorial for Zybo Z7-20 board (or Zynq based images).

**Required tools:**

* Vivado 2023.2 for bitstream build
* Vivado 2020.2 for Petalinux build (not tested for a longer time so it might require some corrections)
* Petalinux 2020.2

## How to Generate the Vivado Project

The project for Vivado tool is initialized based on the `create_project.tcl` script. The project files are generated
inside the `proj` folder by running of the following command (or run the `regenerate-project.sh` which also cleans the folder using git tool):

```bash
vivado -mode batch -source create_project.tcl
```

or

```bash
./regenerate-project.sh
```

After that, you can use the Vivado and open the `zybo-base.xpr` file. All source files, board designs, HDL and
IP cores are under the `src` directory and they are included in the [create_project.tcl](proj/create_project.tcl)
file. Therefore, edit this file if you want to version it and have a possibility to restore a project!

There is also a helping (and simple) tcl script which can be used for the automatic export of HW design from the
command line.
It just opens the created project and runs syntheis, implementation, bitstream generation and HW export to
XSA file. All you have to do is to run the similar command like before:

```bash
vivado -mode batch -source translate_project.tcl
```

or

```bash
./translate-project.sh
```

Helping scripts are really small and they doesn't check if any file was modified. They are here just for the case that you
don't want to write the command by hand.

Currently supported Linux Distributions:

* _Petalinux (default one)_ - presented on this page because this is being supported by Xilinx
* _Debian Linux_ - you can also use the standard Debian distribution with Linux kernel (patched by Xilinx). 
Documentation is [here](debian-zybo).

## How to translate the Petalinux

Petalinux project for Zybo board is inside the `petalinux-zybo` folder. It contains a helping script which is capable to configure HW
platform, kernel and rootfs (packages inside the image, etc.). You can use the default values. The build process also requires the
exported HW architecture in XSA file (used for Device Tree, bitstream, etc.). The XSA file is automatically exported at the end of
translation TCL script (`translate_project.sh` in `proj` folder). Alternatively, you can translate the design in your Vivado tool and
export it via `File -> Export -> Export Hardware`, click on `Include bitstream` and you can go.

To build the Petalinux with defaults:

```bash
./translate-petalinux.sh
```

To build the Petalinux with conguration changes run the script with `-h` option to get more info. If you want to customize everything
you need to run:

```bash
./translate-petalinux.sh -k -r
```

## How to Boot the Petalinux

The design can be booted on real HW or in QEMU simulator - everything can be found inside the Petalinux documentation on Xilinx website.

### HW

Booting on HW can be done via - (i) JTAG or (ii) SD card image. Both methods requires the translated design and Petalinux. The booting
via SD card requires following steps:

1. Set the jumper on Zybo board for booting from the SD card
2. Format the SD card with FAT32 image and copy the following files from the `image/linux` folder inside the `petalinux-zybo` project - *BOOT.BIN* (bootloader), *image.ub* (kernel image), *boot.scr* (boot script for u-boot)
3. Insert the SD card and
4. Connect to serial port (using Minicom or other tools). The configuration of serial line is:

* Baud: 115200
* Data bits: 8
* Parity: no
* No HW flow control
* Stop bits: 1

You should see the linux booting on the terminal. You can log in using the credentials - user **root**, password **root**

Booting via JTAG is useful for debug cases when you don't want to work a lot with SD card (it consumes time and it is not so
efficient). The process consists of following steps:

1. Set the jumper on Zybo board for booting via the JTAG
2. Start the `hw_server` from the Vivado toolchain (allows remote programming)
3. Run the following command:

```bash
petalinux-boot --jtag --prebuilt 3 --hw_server-url localhost:3121
```

The serial line configuration is as same as in the case of SD card boot. I am recommending you to prepare the serial line
berofe the running of petalinux-boot to see all messages. However, it should be fine to run the serial line after the petalinux command.

You will see something like:

```bash
user@device $-> petalinux-boot --jtag --prebuilt 3 --hw_server-url 127.0.0.1:3121
INFO: Sourcing build tools
INFO: FPGA manager enabled, skipping bitstream to load in jtag...
INFO: Append dtb - /home/pavel/Sources/hdl/zybo/zybo-base.git/petalinux-zybo/pre-built/linux/images/system.dtb and other options to boot zImage
INFO: Launching XSDB for file download and boot.
INFO: This may take a few minutes, depending on the size of your image.
INFO: Downloading ELF file: /home/pavel/Sources/hdl/zybo/zybo-base.git/petalinux-zybo/pre-built/linux/images/zynq_fsbl.elf to the target.
INFO: Downloading ELF file: /home/pavel/Sources/hdl/zybo/zybo-base.git/petalinux-zybo/pre-built/linux/images/u-boot.elf to the target.
INFO: Loading image: /home/pavel/Sources/hdl/zybo/zybo-base.git/petalinux-zybo/pre-built/linux/images/system.dtb at 0x00100000
INFO: Loading image: /home/pavel/Sources/hdl/zybo/zybo-base.git/petalinux-zybo/pre-built/linux/images/uImage at 0x00200000
INFO: Loading image: /home/pavel/Sources/hdl/zybo/zybo-base.git/petalinux-zybo/pre-built/linux/images/rootfs.cpio.gz.u-boot at 0x04000000

```

### QEMU

QEMU emulator is useful in the time of image preparation - you can also do some work without HW.
This step requires a finished built of Petalinux project. After that, you can run the simulation using the following command:

```bash
petalinux-boot --qemu --prebuilt 3
```

The meaning of prebuilt stages is following:

* prebuilt 1 - boot includes FPGA bitstream programming
* prebuilt 2 - bot includes U-BOOT stage
* prebuilt 3 - boot includes a  Linux image (full system boot)

## How to check the FPGA design

Petalinux project is using the FPGA manager which allows you to load the FPGA bitstream after the boot. This can be useful if you
want to debug HW via SSH (translate, prepare device tree and configuration). The generated petalinux is embedded with the
`fpga-manager` from Xilinx which allows to program a bitstream and overlay device tree. Initial bitstream is inside the `/lib/firmware/`
folder. To load the design you need to to run the `fpgautil` command (the example output is also attached):

```bash
user@device $-> fpgautil -b /lib/firmware/base/board_design_wrapper.bit.bin
fpga_manager fpga0: writing board_design_wrapper.bit.bin to Xilinx Zynq FPGA Manager
OF: overlay: WARNING: memory leak will occur if overlay removed, property: /fpga-full/firmware-name
OF: overlay: WARNING: memory leak will occur if overlay removed, property: /__symbols__/overlay0
OF: overlay: WARNING: memory leak will occur if overlay removed, property: /__symbols__/overlay1
OF: overlay: WARNING: memory leak will occur if overlay removed, property: /__symbols__/clocking0
OF: overlay: WARNING: memory leak will occur if overlay removed, property: /__symbols__/overlay2
OF: overlay: WARNING: memory leak will occur if overlay removed, property: /__symbols__/axi_gpio_led
OF: overlay: WARNING: memory leak will occur if overlay removed, property: /__symbols__/axi_gpio_sw
OF: overlay: WARNING: memory leak will occur if overlay removed, property: /__symbols__/axi_led_pwm
GPIO IRQ not connected
XGpio: gpio@41210000: registered, base is 902
GPIO IRQ not connected
XGpio: gpio@41200000: registered, base is 898

```

Notice that if you want to load the design during the boot, disable the FPGA Manager functionality during the device import.

After the successfull loading, you can use the `peek` and `poke` commands to read/write from the ARM address space. The address
space also contains the PL (programmable logic) which starts on the 0x4000_0000 offset.

## How to debug the Linux Kernel

The simplest possibility is to use the [KGDB - usage](https://01.org/linuxgraphics/gfx-docs/drm/dev-tools/gdb-kernel-debugging.html#:~:text=The%20kernel%20debugger%20kgdb%2C%20hypervisors,simplify%20typical%20kernel%20debugging%20steps.), [KGDB - overall info](https://01.org/linuxgraphics/gfx-docs/drm/dev-tools/kgdb.html) approach which is known quite well from the
Linux world. First of all, we need to configure the kernel. To achieve this, run the
`petalinux-config -c kernel` and set the following variables (everything is in the
`Kernel hacking` submenu).

* CONFIG_GDB_SCRIPTS
* CONFIG_CONSOLE_POLL
* CONFIG_KGDB
* CONFIG_KGDB_SERIAL_CONSOLE

We need to translate the serial console multiplexer because we are going to debug
and control the board via the same serial line. Linux world has a great tool which
handles this. The code of this tool is located inside the `util/proxy-agent` directory.
Enter the directory and translate it using the `make` command. We will use the translated
tool in next steps :-).

The kernel parameters are configured to wait on the KGDB (if translated) connection.
Another kernel parameters can be configured via the `petalinux-configure` command.

Translate the kernel and boot it (via JTAG or SD card). We need to enter the directory
where the kernel was translated (because of the GDB scripts are located there).

Now, we need to setup the debug and communication session via the `proxy-agent` tool.
The following command creates two slots for communication (5550) and gdb debug (5551)
via the serial line `/dev/ttyUSB1` with baudrate 115200 (you can also check opened
ports via the `ss` tool).

```bash
 ./agent-proxy 5550^5551 0 /dev/ttyUSB1,115200 
```

We can connect to communication terminal using the `telnet` command:

```bash
telnet localhost 5550
```

Now, we need to run the debug session of the kernel. This is achieved in two steps:

* Enabling the debug session via the sysfs on the debugged target - `echo ttyPS0 > /sys/module/kgdboc/parameters/kgdboc`
* Connection to the debug session via the GBD

### Connection to debug session

The first mentioned command will enable the debug session via the ttyPS0 serial line.
You will see something like this:

```bash
[   60.352486] KGDB: Registered I/O driver kgdboc
[   60.372279] KGDB: Waiting for connection from remote gdb...
```

The first half of the process is done. Now, we need to enter the build directory
of the kernel and start the debug session inside the `gdb` tool. The build folder has
path like this `build/tmp/work/zynq_generic-xilinx-linux-gnueabi/linux-xlnx/5.4+git999-r0/linux-xlnx-5.4+git999` (correct this for your translation, you can also look for the
`.config` file. The following command opens the translated `vmlinux` file. After that,
we need to use the target remote localhost:5551 to connect to the debug session:

```bash
arm-xilinx-linux-gnueabi-gdb ./vmlinux
GNU gdb (GDB) 8.3.1
Copyright (C) 2019 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
Type "show copying" and "show warranty" for details.
This GDB was configured as "--host=x86_64-petalinux-linux --target=arm-xilinx-linux-gnueabi".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<http://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
    <http://www.gnu.org/software/gdb/documentation/>.

For help, type "help".
Type "apropos word" to search for commands related to "word"...
Reading symbols from ./vmlinux...
(gdb) target remote localhost:5551
Remote debugging using localhost:5551
arch_kgdb_breakpoint ()
    at /home/jeremy/zybo-base.git/petalinux-zybo/components/yocto/workspace/sources/linux-xlnx/arch/arm/include/asm/kgdb.h:46
46		asm(__inst_arm(0xe7ffdeff));
(gdb) 
```

We are done! You can insert breakpoints and debug the kernel and your modules :-).

First of all, you have to be sure that the kernel module is compiled with -O0 and -g options. After that, you need to determine where the module is loaded.
This can be done via `cat /proc/modules` command on the debugged machine. After that you have to add the symbol file in the `gbd` instance.
The example for the mapping is: `add-symbol-file ../../project-spec/meta-user/recipes-modules/led-module/files/led-module.ko 0xbf005000`.
After that, you can run all the gdb commands as usually :-).

Some helping hints:

* If you want to go back to the debug console - go to the remote target and run `echo g > /proc/sysrq-trigger`
* You can create a `.gdbinit` file with symbol file loading but be aware of the situation that base linux module address can change between runs
* Minimize the size of the image to load it faster during the debug. This image is larger because you may use it for different purposes :-)

## How to obtain the Xilinx build toolchain

The toolchain can be accessed via the Petalinux SDK. You have to do the following:

```bash
cd petalinux-zybo
./source-toolchain.sh
```

The sourcing script checks if the the SDK is available and it also builds it for you if not.
