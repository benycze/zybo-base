# PetaLinux LED module

TODO: Driver description here

## Compilation

The "all:" target in the Makefile template will compile compile the module.

To compile and install your module to the target file system copy on the host,
simply run the command.
    "petalinux-build -c kernel" to build kernel first, and then run
    "petalinux-build -c led-module" to build the module

You can also compile the module out of the petalinux tool. All you have to do is to run the `make` command which
which runs the cross compilation with arm-linux-gnueabihf- toolchain. You can also select the new toolachain via `ARCH` and `CROSS_COMPILE`
variables. Additional compilation variables are taken via `MY_CFLAGS` variable. So, to run a different cross compilation with debug symbols run:

```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- MY_CFLAGS="-g -O0 -DDEBUG"
```

## Device Tree

If OF(OpenFirmware) is configured, you need to add the device node to the
DTS(Device Tree Source) file so that the device can be probed when the module is
loaded. Here is an example of the device node in the device tree:

```
	led-module_instance: led-module@XXXXXXXX {
		compatible = "vendor,led-module";
		reg = <PHYSICAL_START_ADDRESS ADDRESS_RANGE>;
		interrupt-parent = <&INTR_CONTROLLER_INSTANCE>;
		interrupts = < INTR_NUM INTR_SENSITIVITY >;
	};
```

Notes:
 * "led-module@XXXXXXXX" is the label of the device node, it is usually the "DEVICE_TYPE@PHYSICAL_START_ADDRESS". E.g. "led-module@89000000".
 * "compatible" needs to match one of the the compatibles in the module's compatible list.
 * "reg" needs to be pair(s) of the physical start address of the device and the address range.
 * If the device has interrupt, the "interrupt-parent" needs to be the interrupt controller which the interrupt connects to. and the "interrupts" need to be pair(s) of the interrupt ID and the interrupt sensitivity.