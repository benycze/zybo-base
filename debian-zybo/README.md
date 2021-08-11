# Build the Debian Image for Zybo

TODO: Write the documentation

* Need to source SDK from Vivado
* We will use the arm-linux-gnueabihf- cross compiler (maybe we need to use the Debian one?)
* Write the list of packages which needs to be installed on the system
    * Flex
    * Bison
    * build-essential
    * qemu-user-static debootstrap binfmt-support
* Documentation: 
    + https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/460653138/Xilinx+Open+Source+Linux
    + https://www.xilinx.com/html_docs/xilinx2021_1/vitis_doc/
    + https://ohwr.org/project/soc-course/wikis/Linux-Kernel-Image-and-Modules


Booting:
* https://u-boot.readthedocs.io/en/stable/usage/booti.html
* https://github.com/Kampi/Zybo-Linux/blob/master/docs/wiki/Prepare-a-SD-Card.md
* https://github.com/Kampi/Zybo-Linux
* https://github.com/PyHDI/zynq-linux
* https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18842223/U-boot
* https://github.com/Netgate/meta-ubmc/blob/master/scripts/uEnv.txt-example
* https://www.denx.de/wiki/DULG/LinuxKernelArgs
* https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18841645/Solution+Zynq+PL+Programming+With+FPGA+Manager#SolutionZynqPLProgrammingWithFPGAManager-UsingDeviceTreeOverlay%3A


* We need to create two partitions (one for boot and second for data)
    * Boot one will be vfat
    * Non-boot will ext4


* Install
    * Unpack the tarball
    * Copy dtb files,boot.scr
    * Mount the sd card - partition 1 FAT32 (boot), partition 2 ext4(rootfs), mount boot into rootfs/boot 
    * `tar --preserve-permissions -xzvf output/tarball/debian-buster-rootfs-vanilla.tgz -C output/`
    * Recursive umount and boot
