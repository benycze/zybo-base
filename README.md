# Zybo Base Image

This repository contains the basic template which can be used as a foundation for developing Zynq-based images and hardware designs.
It serves as a comprehensive reference and tutorial for the Digilent Zybo Z7-20 board running Debian Linux.

**Required tools:**

* Vivado / Vitis 2026.1 for bitstream, FSBL, device tree, and software build
* Cross-compilation toolchain `arm-linux-gnueabihf-` (or sourced from Vitis 2026.1)
* Host packages for Debian image bootstrapping (`qemu-user-static`, `debootstrap`, `u-boot-tools`, `libssl-dev`, etc.)

## How to Generate the Vivado Project

The Vivado project is initialized using the `create_project.tcl` script. The project files are generated
inside the `proj` folder by running the following command (or running `regenerate-project.sh`, which also cleans the folder):

```bash
vivado -mode batch -source create_project.tcl
```

or

```bash
./regenerate-project.sh
```

After project creation, you can open `proj/zybo-base.xpr` in Vivado. All source files, block designs, HDL, constraints, and
custom IP cores reside under the `src` directory and are managed by [create_project.tcl](proj/create_project.tcl).
Always update `create_project.tcl` if you add new sources so the project can be reproduced cleanly.

To run synthesis, implementation, bitstream generation, and hardware export to XSA from the command line:

```bash
vivado -mode batch -source translate_project.tcl
```

or

```bash
./translate-project.sh
```

The resulting hardware handoff file (`board_design_wrapper.xsa`) and bitstream (`board_design_wrapper.bit.bin`) will be placed in the `proj` directory.

## Building the Debian Linux Image

The primary Linux distribution for this board is **Debian 13 (Trixie)**, built using the automated scripts located in [`debian-zybo/`](debian-zybo/).

The build system compiles:
* **FSBL (First-Stage BootLoader)**: generated via Vitis 2026.1 Unified Python API (`generate-fsbl.py`)
* **U-Boot**: compiled from Xilinx U-Boot source tree
* **Linux Kernel**: compiled directly from Xilinx Linux tree (`uImage` + loadable kernel modules)
* **Device Tree**: generated using `sdtgen` and patched with custom device nodes
* **Bootloader Image (`BOOT.BIN` and `boot.scr`)**: assembled with `bootgen` and `mkimage`
* **Root Filesystem**: native Debian rootfs generated via `debootstrap` (with QEMU user emulation), pre-loaded with kernel modules, FPGA utilities, and custom drivers

To build all components and assemble the distribution tarball:

```bash
cd debian-zybo
./build-image.sh -t
```

Refer to the [Debian Zybo Documentation](debian-zybo/README.md) for detailed instructions, prerequisites, and build options (`-k`, `-u`, `-f`, `-d`, `-o`, `-c`, `-m`, etc.).

## How to Boot Debian Linux on the Zybo Board

Booting requires a micro SD card partitioned into two parts:

1. **Partition 1 (FAT32, ~100MB, flagged as bootable)**:
   Copy the following boot artifacts from `debian-zybo/output/tarball/`:
   * `BOOT.BIN`
   * `boot.scr`
   * `uImage`
   * `zynq-debian.dtb`
   * `initramdisk.image.gz`

2. **Partition 2 (ext4, remainder of SD card)**:
   Extract the rootfs archive into the mounted partition:
   ```bash
   sudo tar --preserve-permissions -xzvf debian-zybo/output/tarball/debian-trixie-rootfs-vanilla.tgz -C /media/user/rootfs/
   ```

### Board Configuration & Serial Console

1. Set the board jumper (`JP5`) to **SD** boot mode.
2. Insert the micro SD card into the Zybo board slot.
3. Connect a micro-USB cable between the Zybo PROG/UART port and your host PC.
4. Open a serial terminal (e.g. `minicom`, `screen`, or `picocom`) configured with:
   * **Baud rate**: 115200
   * **Data bits**: 8, Parity: None, Stop bits: 1
   * **Hardware Flow Control**: Off
   ```bash
   sudo minicom -b 115200 -D /dev/ttyUSB1
   ```
5. Power on the board. You will observe U-Boot loading the kernel, initramfs, and device tree, followed by the Debian Linux login prompt.

**Default credentials:**
* User: `root` / Password: `root`
* User: `fpga` / Password: `fpga`

## Checking the FPGA Design & Hardware Modules

The Debian image includes a systemd service (`fpga.service`) that automatically programs the bitstream and loads the device tree overlay at boot using `fpgautil`.

You can manually reload or re-program the PL using:

```bash
fpgautil -b /lib/firmware/fpga/board_design_wrapper.bit.bin -o /lib/firmware/fpga/board_design_wrapper.bit.bin.dtbo
```

### Inspecting Hardware Registers
The image includes the `peek` and `poke` utilities in `/usr/bin/` to access the ARM physical memory space and AXI peripherals (which start at `0x4000_0000`):

```bash
# Read register
peek 0x41200000

# Write register
poke 0x41200000 0x0000000F
```

### Testing Custom Peripheral Drivers
Custom driver test utilities are pre-compiled and installed in `/usr/local/bin/`:
* `ledmodule-test -d /dev/pb-led`
* `rgb-ledmodule-test -d /dev/pb-rgb-led`
* `switchmodule-test -d /dev/pb-switch`

## How to Debug the Linux Kernel

The simplest possibility is to use the [KGDB](https://01.org/linuxgraphics/gfx-docs/drm/dev-tools/kgdb.html) approach.

### 1. Kernel Configuration
The required debugging options are enabled in `sw-sources/kernel-config/kernel_config.cfg`:
* `CONFIG_GDB_SCRIPTS=y`
* `CONFIG_CONSOLE_POLL=y`
* `CONFIG_KGDB=y`
* `CONFIG_KGDB_SERIAL_CONSOLE=y`

### 2. Serial Console Multiplexer (`agent-proxy`)
Because the board is controlled and debugged via the same serial line, a serial console multiplexer is needed.
The tool is located in `util/agent-proxy`:

```bash
cd util/agent-proxy
make
```

Run `agent-proxy` to multiplex `/dev/ttyUSB1` (at 115200 baud) into two network ports: port `5550` for console terminal access and port `5551` for GDB:

```bash
./util/agent-proxy/agent-proxy 5550^5551 0 /dev/ttyUSB1,115200
```

You can connect to the serial communication terminal via `telnet`:

```bash
telnet localhost 5550
```

### 3. Starting the Debug Session
1. **Enable KGDB on target**:
   On the running target board (via the serial console or SSH), activate kgdboc:
   ```bash
   echo ttyPS0 > /sys/module/kgdboc/parameters/kgdboc
   ```
   You will see:
   ```text
   KGDB: Registered I/O driver kgdboc
   KGDB: Waiting for connection from remote gdb...
   ```

2. **Connect with GDB**:
   From your host machine, navigate to the kernel build directory and start GDB (`gdb-multiarch` or `arm-linux-gnueabihf-gdb`):
   ```bash
   cd debian-zybo/output/linux-xlnx/build
   gdb-multiarch ./vmlinux
   ```

   Inside GDB, connect to the agent-proxy GDB port:
   ```gdb
   (gdb) target remote localhost:5551
   Remote debugging using localhost:5551
   arch_kgdb_breakpoint () at arch/arm/include/asm/kgdb.h:46
   46        asm(__inst_arm(0xe7ffdeff));
   (gdb)
   ```

### 4. Debugging Kernel Modules
To debug custom kernel modules (such as `led-module`):
1. On the target board, find the loaded base address of the module:
   ```bash
   cat /proc/modules
   ```
2. In GDB, load the module symbols with the target address:
   ```gdb
   add-symbol-file drivers/pb-zybo/led-module/led-module.ko 0xbf005000
   ```

### Helpful Hints:
* To return to the debug console from the target board: `echo g > /proc/sysrq-trigger`
* You can create a `.gdbinit` file for automated symbol loading and connection setup.
