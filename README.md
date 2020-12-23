# Zybo Base Image

This repository contains the basic template which can be used for the template for development of another Zynq based images.
It can be also considered as tutorial for Zybo Z7-20 board (or Zynq based images).

**Required tools:**
* Vivado 2019.2
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
