#!/usr/bin/env bash

set -e

SDK_PATH=`realpath ./images/linux/sdk`

if [ ! -e $SDK_PATH ]; then
    echo "SDK not found, preparing for the deployement ..."
    petalinux-build --sdk
    petalinux-package --sysroot
fi

echo "Sourcing SDK toolchain ..."
source images/linux/sdk/environment-setup-*
bash
