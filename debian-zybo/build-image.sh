#!/usr/bin/env bash

# -------------------------------------------------------------------------------
#  PROJECT: Zybo Base
# -------------------------------------------------------------------------------
#  AUTHORS: Pavel Benacek <pavel.benacek@gmail.com>
#  LICENSE: The MIT License (MIT), please read LICENSE file
#  WEBSITE: https://github.com/benycze/zybo-base
# -------------------------------------------------------------------------------

set -e

# #######################################################################################
# Configuration
# #######################################################################################

# Folder with downloaded repos
REPO_FOLDER="`pwd`/reps"
# Folder with output data
OUT_FOLDER="`pwd`/output"
# Configuration folder
CONF_FOLDER="`pwd`/conf"
# Folder with scripts
SCRIPT_FOLDER="`pwd`/scripts"
# Xilinx tools
XILINX_TOOLS="`pwd`/xilinx-tools"
# Output folder for the FSBL
FSBL_OUTPUT="${OUT_FOLDER}/fsbl"
# DTC folder
DTC_FOLDER="${REPO_FOLDER}/dtc.git"
# Device Tree for Xilinx Device  
DT_XLNX="${REPO_FOLDER}/device-tree-xlnx.git"
DT_XLNX_OUTPUT="${OUT_FOLDER}/dts-zynq"
DTB_FILE="zynq-debian.dtb"
DTB_PL_FILE="board_design_wrapper.bit.dtb"
DTS_TOP="${DT_XLNX_OUTPUT}/zynq-debian.dts"
DTS_PL_TOP="${DT_XLNX_OUTPUT}/fpga-pl.dts"
DTB_TOP="${DT_XLNX_OUTPUT}/${DTB_FILE}"
DTB_PL_TOP="${DT_XLNX_OUTPUT}/${DTB_PL_FILE}"
# FPGA bitstream name
FPGA_BIN="board_design_wrapper.bit.bin"
FPGA_PROJ_FOLDER="`pwd`/../proj"
# U-Boot folder
UBOOT_FOLDER="${REPO_FOLDER}/u-boot-xlnx.git"
UBOOT_OUTPUT="${OUT_FOLDER}/uboot"
# Kernel folder
KERNEL_FOLDER="${REPO_FOLDER}/linux-xlnx.git"
KERNEL_OUTPUT="${OUT_FOLDER}/linux-xlnx/build"
KERNEL_MODULE_FOLDER="${KERNEL_FOLDER}/drivers/pb-zybo"
# Bootloader folder
BOOTLOADER_OUTPUT="${OUT_FOLDER}/bootloader"
# Tarball folder
TARBALL_OUTPUT="${OUT_FOLDER}/tarball"
# Root folder with project sources
SW_SOURCES="`pwd`/../sw-sources"

# Rootfs configuration
DEBIAN_OUTPUT="${OUT_FOLDER}/debian10-rootfs/root"
DEBIAN_DISTRO=buster
DEBIAN_ARCHIVE_FILE="debian-${DEBIAN_DISTRO}-rootfs-vanilla.tgz"

# Cross-compiler configuration
export CROSS_COMPILE=arm-linux-gnueabihf-
export ARCH=arm

# Build jobs
JOBS=4

# Output file name for export device tree
DEVICE_TREE="zynq-zybo-z7"

# #######################################################################################
# Helping functions
# #######################################################################################

function print_boxed () {
    echo "*********************************************************************"
    echo "* $1"
    echo "*********************************************************************"

}

function print_usage {
    echo "The script has following commands:"
    echo "-s         => download sources"
    echo "-h         => prints the help"
    echo "-r         => rebuild all"
    echo "-f         => build FSBL (First-Stage-Boot-Loader)"
    echo "-u         => build U-BOOT"
    echo "-k         => build Linux Kernel"
    echo "-d         => build DTB & DTS"
    echo "-b         => buld bootloader"
    echo "-o         => build the Debian Rootfs"
    echo "-t         => build the tarball"
    echo ""
    echo "The interactive mode is started if you don't pass any argument."
}

# Receive the source from GIT
function get_git_source () {
    URL="$1"
    DEST="$2"
    echo -e "Processing ${URL} (destination ${DEST}) ...\n"
    if [ ! -e "${DEST}" ]; then
        # Clone the project if it doesn't exist
        git clone --recursive "${URL}" "${DEST}"
    fi
}

# #######################################################################################
# Target functions
# #######################################################################################

function get_sources () {
    print_boxed "Getting sources"
    mkdir -p "${REPO_FOLDER}"

    get_git_source https://github.com/Xilinx/linux-xlnx.git "${KERNEL_FOLDER}"
    get_git_source https://github.com/Xilinx/u-boot-xlnx.git "${UBOOT_FOLDER}"
    get_git_source https://github.com/Xilinx/device-tree-xlnx.git "${DT_XLNX}"
    get_git_source https://git.kernel.org/pub/scm/utils/dtc/dtc.git "${REPO_FOLDER}/dtc.git" "${DTC_FOLDER}"
}

function build_fsbl () {
    print_boxed "Building the FSBL"
    mkdir -p "${FSBL_OUTPUT}"
    output_folder="`realpath "${FSBL_OUTPUT}"`"

    xsa_file="`find ../proj/*.xsa -exec realpath {} \;`"
    if [ -z "${xsa_file}" ]; then
        "XSA file doesn't exist - cannot generate the FSBL"
        exit 1
    fi

    xsct scripts/generate-fsbl.tcl "$xsa_file" "$output_folder"
    pushd .
    cd "${output_folder}"
    make CFLAGS=-DFSBL_DEBUG_INFO
    popd
}

function translate_dtc () {
    print_boxed "Building DTC"
    pushd .
    cd "${DTC_FOLDER}"
    make 
    # Export the path to compiled tool
    export PATH="`pwd`":"${PATH}"
    dtc_path="`which dtc`"
    echo -e "Path of used dtc: $dtc_path \n"
    popd
}

function build_uboot () {
    print_boxed "Building U-BOOT"
    pushd .
    cd "${UBOOT_FOLDER}"
    mkdir -p "${UBOOT_OUTPUT}"
    # Build the u-boot together with device tree. The name of output file
    # is stored in DEVICE_TREE variable
    make O="${UBOOT_OUTPUT}" distclean
    make O="${UBOOT_OUTPUT}" xilinx_zynq_virt_defconfig
    make O="${UBOOT_OUTPUT}" -j${JOBS} DEVICE_TREE="${DEVICE_TREE}"
    popd
}

function build_kernel () {
    print_boxed "Building Linux Kernel"

    # Add the module into the source tree
    if [ ! -e "${KERNEL_MODULE_FOLDER}" ]; then
        echo "Linking module folder into kernel source ..."
        ln -s "${SW_SOURCES}/modules" "${KERNEL_MODULE_FOLDER}"
        cd "${KERNEL_MODULE_FOLDER}/.."
        # Add the folder with out sources
        sed -E -i 's/endmenu/source \"drivers\/pb-zybo\/Kconfig\"\nendmenu/' Kconfig
        echo "obj-y += pb-zybo/" >> Makefile
    fi

    cd "${KERNEL_FOLDER}"
    pushd .

    # Create output folder and make the default configuration
    mkdir -p "${KERNEL_OUTPUT}"
    make O="${KERNEL_OUTPUT}" ARCH=arm xilinx_zynq_defconfig

    echo "Patching the defualt configuration ..."
    scripts/kconfig/merge_config.sh -m -O "${KERNEL_OUTPUT}" "${KERNEL_OUTPUT}/.config" "${SW_SOURCES}/kernel-config/*.cfg"

    make O="${KERNEL_OUTPUT}" bindeb-pkg
    popd
}

function build_zynq_dts () {
    print_boxed "Build Zynq DTS"
    mkdir -p "${DT_XLNX_OUTPUT}"

    xsa_file="`find ../proj/*.xsa -exec realpath {} \;`"
    if [ -z "${xsa_file}" ]; then
        echo "XSA file doesn't exist - cannot generate the FSBL"
        exit 1
    fi

    xsct scripts/generate-dts.tcl "$xsa_file" "$DT_XLNX_OUTPUT" "$DT_XLNX"

    echo "Translating to DTB ..."
    echo "  * DTS File = ${DTS_TOP}"
    echo "  * DTB File = ${DTB_TOP}"

    echo "Preparing the Device Tree for the system ..."
    pushd .
    cd "${DT_XLNX_OUTPUT}"

    echo "Preparing system top-level file ..."
    cat system-top.dts "${SW_SOURCES}/device-tree-mods/system-user.dtsi" > user-patched-system-top.dts
    cat pl.dtsi "${SW_SOURCES}/device-tree-mods/pl-custom.dtsi" > user-patched-pl.dts

    echo "DTS ---> DTB blob generation ..."
    gcc -I "${DT_XLNX_OUTPUT}" -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp "${DT_XLNX_OUTPUT}/user-patched-system-top.dts" -o "${DTS_TOP}"
    dtc -i "${SW_SOURCES}/device-tree-mods" -I dts -O dtb -o "${DTB_TOP}" "${DTS_TOP}"

    gcc -I "${DT_XLNX_OUTPUT}" -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp "${DT_XLNX_OUTPUT}/user-patched-pl.dts" -o "${DTS_PL_TOP}"
    dtc -i "${SW_SOURCES}/device-tree-mods" -I dts -O dtb -o "${DTB_PL_TOP}" "${DTS_PL_TOP}"
    popd
}

function build_debian_rootfs () {
    print_boxed "Build Debian rootfs"

    pushd .
    echo "Bootstraping initial system ..."
    sudo rm -rf "${DEBIAN_OUTPUT}"
    mkdir -p "${DEBIAN_OUTPUT}"
    sudo debootstrap --arch=armhf --foreign "${DEBIAN_DISTRO}" "${DEBIAN_OUTPUT}"
    sudo cp `which qemu-arm-static` "${DEBIAN_OUTPUT}/usr/bin"

    # Used in the case of pernament image
    sudo cp "${KERNEL_OUTPUT}"/../*.deb "${DEBIAN_OUTPUT}/"
    # Copy the script which generates the u-boot images after kernel install
    sudo cp "${SCRIPT_FOLDER}/uimage-gen" "${DEBIAN_OUTPUT}/etc/kernel/postinst.d/"
    sudo chmod 755 "${DEBIAN_OUTPUT}/etc/kernel/postinst.d/uimage-gen"
    # Copy Xilinx helping tools
    sudo cp -r "${XILINX_TOOLS}" "${DEBIAN_OUTPUT}/usr/src/"
    sudo cp -r "${SW_SOURCES}" "${DEBIAN_OUTPUT}/usr/src/pb-zybo"
    # Copy FPGA bistream and device tree
    sudo mkdir -p "${DEBIAN_OUTPUT}/lib/firmware/fpga"
    fpga_bit_path="`find "${FPGA_PROJ_FOLDER}" -name board_design_wrapper.bin`"
    sudo cp "${DTB_PL_TOP}" "${DEBIAN_OUTPUT}/lib/firmware/fpga/"
    sudo cp "${fpga_bit_path}" "${DEBIAN_OUTPUT}/lib/firmware/fpga/"

    # Used in the case of non-persistent image
    echo "Copying data inside the chroot ..."
    cp "${SCRIPT_FOLDER}/bootstrap-debian-image.sh" "${DEBIAN_OUTPUT}"

    echo "Running chroot ..."
    sudo chroot "${DEBIAN_OUTPUT}" /bin/bash -c "bash /bootstrap-debian-image.sh ${DEBIAN_DISTRO}"
    sudo rm -f "${DEBIAN_OUTPUT}/bootstrap-debian-image.sh"

    cd "${DEBIAN_OUTPUT}"

    echo "Packing rootfs into archive (${DEBIAN_ARCHIVE_FILE}) ..."
    sudo find . -type s | sed 's/^\.\///' > ../sockets-to-exclude
    sudo tar --preserve-permissions -czf ../"${DEBIAN_ARCHIVE_FILE}" -X ../sockets-to-exclude *
    popd
}

function build_bootloader () {
    print_boxed "Building the Bootloader"
    mkdir -p "${BOOTLOADER_OUTPUT}"

    # Collect artefacts and prepare the boot image
    pushd .
    cd "${OUT_FOLDER}"
    find "${FSBL_OUTPUT}" -name executable.elf -exec cp {} "${BOOTLOADER_OUTPUT}/fsbl.elf"  \;
    find "${UBOOT_OUTPUT}" -name u-boot.elf -exec cp {} "${BOOTLOADER_OUTPUT}" \;

    cd "${BOOTLOADER_OUTPUT}"
    echo "Generaring the BOOT.BIN file (FSBL + U-Boot) ..."
    bootgen -arch zynq -image "${CONF_FOLDER}/image.bif" -w -o i BOOT.BIN
    echo "Generating the boot.scr file ..."
    mkimage -A arm -O linux -T script -C none -a 0 -e 0 -n "Boot image script" -d "${CONF_FOLDER}/u-boot.txt" boot.scr
    popd
}

function build_tarball () {
    print_boxed "Collecting all required artefacts into tarball (${TARBALL_OUTPUT}) ..."
    mkdir -p "${TARBALL_OUTPUT}"

    FILES=("${DEBIAN_OUTPUT}/../${DEBIAN_ARCHIVE_FILE}" \
    "${BOOTLOADER_OUTPUT}/${DTB_FILE}" \
    "${BOOTLOADER_OUTPUT}/BOOT.BIN" \
    "${BOOTLOADER_OUTPUT}/boot.scr" )

    for file in "${FILES[@]}"; do
        echo "Processing: ${file}"
        if [ -e "${file}" ]; then
            mv "${file}"  "${TARBALL_OUTPUT}"
        else 
            echo "File ${file} wasn't found!"
        fi
    done
}

# #######################################################################################
# Main body
# #######################################################################################

# Parse arguments ----------------------------------------------------------------
p_download_sources=0
p_build_fsbl=0
p_rebuild_all=0
p_build_uboot=0
p_build_kernel=0
p_build_dts=0
p_build_rootfs=0
p_build_boot=0
p_tarball=0
while getopts "hsfrukdobxt" opt; do
    case "$opt" in
    h)  print_usage
        exit 0
        ;;
    s)  p_download_sources=1
        ;;
    f)  p_build_fsbl=1
        ;;
    r)  p_rebuild_all=1
        ;; 
    u) p_build_uboot=1
        ;;
    k) p_build_kernel=1
        ;;
    d) p_build_dts=1
        ;;
    o) p_build_rootfs=1
        ;;
    b) p_build_boot=1
        ;;
    t) p_tarball=1
        ;;
    esac
done
shift $((OPTIND -1))

# Main body  ---------------------------------------------------------------------

if [ "${p_download_sources}" -eq 1 ] || [ ! -e "${REPO_FOLDER}" ]; then
    get_sources
fi

if [ "${p_rebuild_all}" -eq 1 ]; then
    # We need to remove the folder with outputs which will enforce the build process
    # again
    echo "Cleaning output folder (${OUT_FOLDER}) ..."
    sudo rm -rf "${OUT_FOLDER}"
fi

if [ "${p_build_fsbl}" -eq 1 ] || [ ! -e "${FSBL_OUTPUT}" ]; then
    build_fsbl
fi

translate_dtc

if [ "${p_build_uboot}" -eq 1 ] || [ ! -e "${UBOOT_OUTPUT}" ]; then
    build_uboot
fi
# Update the path for the mkimage
export PATH="${UBOOT_OUTPUT}"/tools:"$PATH"
mkimage_path="`which mkimage`"
echo -e "Path of used mkimage: $mkimage_path \n"

if [ "${p_build_kernel}" -eq 1 ] || [ ! -e "${KERNEL_OUTPUT}" ]; then
    build_kernel
fi

if [ "${p_build_dts}" -eq 1 ] || [ ! -e "${DT_XLNX_OUTPUT}" ]; then
    build_zynq_dts
fi

if [ "${p_build_rootfs}" -eq 1 ] || [ ! -e "${DEBIAN_OUTPUT}" ]; then
    build_debian_rootfs
fi

if [ "${p_build_boot}" -eq 1 ] || [ ! -e "${BOOTLOADER_OUTPUT}" ]; then
    build_bootloader
fi

if [ "${p_tarball}" -eq 1 ] || [ ! -e "${TARBALL_OUTPUT}" ]; then
    build_tarball
fi

echo "Done!"
exit 0
