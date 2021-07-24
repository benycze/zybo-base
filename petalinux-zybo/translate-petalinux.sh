#!/usr/bin/env bash

# -------------------------------------------------------------------------------
#  PROJECT: Zybo Base
# -------------------------------------------------------------------------------
#  AUTHORS: Pavel Benacek <pavel.benacek@gmail.com>
#  LICENSE: The MIT License (MIT), please read LICENSE file
#  WEBSITE: https://github.com/benycze/zybo-base
# -------------------------------------------------------------------------------

set -e

# Functions & code --------------------------------------------------------------

function print_usage {
    echo "The script has following commands:"
    echo "-k         => run the kernel configuration"
    echo "-r         => run the rootfs configuration"
    echo "-h         => prints the HELP"
    echo "-x         => import the XSA file. The file is included automatically if"
    echo "              the board folder is not detected"
    echo ""
    echo "The interactive mode is started if you don't pass any argument."
}

# Parse arguments ----------------------------------------------------------------
kernel_config=0
rootfs_config=0
import_xsa_config=0

while getopts "krhx" opt; do
    case "$opt" in
    h)
        print_usage
        exit 0
        ;;
    k)  kernel_config=1
        ;;
    r)  rootfs_config=1
        ;;
    x)  import_xsa_config=1
        ;;
    esac
done
shift $((OPTIND -1))

# Main code -------------------------------------------------------

XSA_FILE=${PWD}/../proj/board_design_wrapper.xsa
XSA_IMPORT_FOLDER=${PWD}/components/plnx_workspace

if [ ! -e $XSA_IMPORT_FOLDER ] || [ $import_xsa_config -eq 1 ]; then
    echo "#####################################################"
    echo "Importing XSA file ..."
    echo "#####################################################"

    if [ ! -e ${XSA_FILE} ]; then
        echo "XSA file wasn't detected, please translate the project and export the HW."
        exit 1
    fi

    petalinux-config --get-hw-description ${XSA_FILE}
fi

if [ ${rootfs_config} -eq 1 ]; then
    echo "#####################################################"
    echo "Running the rootfs configuration..."
    echo "#####################################################"
    petalinux-config -c rootfs
fi

if [ ${kernel_config} -eq 1 ]; then
    echo "#####################################################"
    echo "Running the kernel configuration ..."
    echo "#####################################################"
    petalinux-config -c kernel
fi

echo "#####################################################"
echo "Running the translation ..."
echo "#####################################################"
petalinux-build

echo "#####################################################"
echo "Running the packaging ..."
echo "#####################################################"
petalinux-package --prebuilt --force
petalinux-package --boot --force --fsbl images/linux/zynq_fsbl.elf --u-boot
echo "FPGA bitstream is not included in the image because the FPGA manager is being used."
echo "*  See the documentation how to boot it"

echo "#####################################################"
echo "DONE! We are ready to boot :-)"
echo "#####################################################"
exit 0
