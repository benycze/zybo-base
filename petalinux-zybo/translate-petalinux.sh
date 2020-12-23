#!/usr/bin/env bash
set -e

# Functions & code --------------------------------------------------------------

function print_usage {
    echo "The script has following commands:"
    echo "-k         => run the kernel configuration"
    echo "-r         => run the rootfs configuration"
    echo "-h         => prints the HELP"
    echo ""
    echo "The interactive mode is started if you don't pass any argument."
}

# Arguments parsing -----------------------------------------------
# Parse arguments ----------------------------------------------------------------
kernel_config=0
rootfs_config=0

while getopts "krh" opt; do
    case "$opt" in
    h)
        print_usage
        exit 0
        ;;
    k)  kernel_config=1
        ;;
    r)  rootfs_config=1
        ;;
    esac
done
shift $((OPTIND -1))

# Main code -------------------------------------------------------

echo "#####################################################"
echo "Importing XSA file ..."
echo "#####################################################"

XSA_FILE=${PWD}/../proj/board_design_wrapper.xsa

if [ ! -e ${XSA_FILE} ]; then
    echo "XSA file wasn't detected, please translate the project and export the HW."
    exit 1
fi

petalinux-config --get-hw-description ${XSA_FILE}

echo "#####################################################"
echo "Running the rootfs configuration..."
echo "#####################################################"
if [ ${rootfs_config} -eq 1 ]; then
    petalinux-config -c rootfs
fi

echo "#####################################################"
echo "Running the kernel configuration ..."
echo "#####################################################"
if [ ${kernel_config} -eq 1 ]; then
    petalinux-config -c kernel
fi

echo "#####################################################"
echo "Running the translation ..."
echo "#####################################################"
petalinux-build

echo "#####################################################"
echo "Running the packaging ..."
echo "#####################################################"
petalinux-package --boot --force --fsbl images/linux/zynq_fsbl.elf --fpga images/linux/system.bit --u-boot

echo "#####################################################"
echo "DONE! We are ready to boot :-)"
echo "#####################################################"
exit 0
