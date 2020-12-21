#!/usr/bin/env bash
set -e

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
petalinux-config -c rootfs

echo "#####################################################"
echo "Running the translation ..."
echo "#####################################################"
petalinux-build

#TODO: Impelement packing
