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
# Project root folder
PROJ_ROOT="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"
# Folder with downloaded repos
REPO_FOLDER="${PROJ_ROOT}/reps"
# Folder for download cache (pristine cloned repos)
CACHE_FOLDER="${CACHE_FOLDER:-${PROJ_ROOT}/cache}"
# Folder with output data
OUT_FOLDER="${PROJ_ROOT}/output"
# Configuration folder
CONF_FOLDER="${PROJ_ROOT}/conf"
# Folder with scripts
SCRIPT_FOLDER="${PROJ_ROOT}/scripts"
# Xilinx tools
XILINX_TOOLS="${PROJ_ROOT}/xilinx-tools"
# Output folder for the FSBL
FSBL_OUTPUT="${OUT_FOLDER}/fsbl"
# DTC folder
DTC_FOLDER="${REPO_FOLDER}/dtc.git"
# Device Tree for Xilinx Device  
DT_XLNX="${REPO_FOLDER}/device-tree-xlnx.git"
DT_XLNX_OUTPUT="${OUT_FOLDER}/dts-zynq"
DTB_FILE="zynq-debian.dtb"
DTB_PL_FILE="board_design_wrapper.bit.bin.dtbo"
DTS_TOP="${DT_XLNX_OUTPUT}/zynq-debian.dts"
DTS_PL_TOP="${DT_XLNX_OUTPUT}/fpga-pl.dts"
DTB_TOP="${DT_XLNX_OUTPUT}/${DTB_FILE}"
DTB_PL_TOP="${DT_XLNX_OUTPUT}/${DTB_PL_FILE}"
# FPGA bitstream name
FPGA_BIN="board_design_wrapper.bit.bin"
FPGA_PROJ_FOLDER="${PROJ_ROOT}/../proj"
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
SW_SOURCES="${PROJ_ROOT}/../sw-sources"

# Rootfs configuration
DEBIAN_OUTPUT="${OUT_FOLDER}/debian-rootfs/root"
DEBIAN_DISTRO="${DEBIAN_DISTRO:-trixie}"
DEBIAN_ARCHIVE_FILE="debian-${DEBIAN_DISTRO}-rootfs-vanilla.tgz"

# Cross-compiler configuration
export CROSS_COMPILE=arm-linux-gnueabihf-
export ARCH=arm

# Build jobs
JOBS="$(nproc 2>/dev/null || echo 4)"

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
    echo "-s         => download sources into cache and copy to reps"
    echo "-h         => prints the help"
    echo "-c         => clean build directory (${OUT_FOLDER})"
    echo "-m         => mrproper (clean build directory, reps, and download cache)"
    echo "-r         => rebuild all (cleans build directory before building)"
    echo "-f         => build FSBL (First-Stage-Boot-Loader)"
    echo "-u         => build U-BOOT"
    echo "-k         => build Linux Kernel"
    echo "-d         => build DTB & DTS"
    echo "-b         => build bootloader"
    echo "-o         => build the Debian Rootfs"
    echo "-t         => build the tarball"
    echo "-v <dist>  => set Debian distribution (default: ${DEBIAN_DISTRO})"
    echo "-j <jobs>  => number of build jobs (${JOBS} by default)"
    echo ""
    echo "Without arguments, the script builds all missing components incrementally."
}

function safe_rm_rf () {
    target="$1"
    [ -e "$target" ] || return 0
    if rm -rf "$target" 2>/dev/null; then
        return 0
    fi
    sudo rm -rf "$target"
}

function clean_output () {
    print_boxed "Cleaning build directory (${OUT_FOLDER})"
    if [ -e "${OUT_FOLDER}" ]; then
        echo "Removing build directory (${OUT_FOLDER}) ..."
        safe_rm_rf "${OUT_FOLDER}"
        echo "Build directory removed."
    else
        echo "Build directory does not exist, nothing to clean."
    fi
}

function clean_mrproper () {
    print_boxed "Cleaning EVERYTHING (mrproper: output, reps, cache)"
    if [ -e "${OUT_FOLDER}" ]; then
        echo "Removing build directory (${OUT_FOLDER}) ..."
        safe_rm_rf "${OUT_FOLDER}"
    fi
    if [ -e "${REPO_FOLDER}" ]; then
        echo "Removing working tree repositories (${REPO_FOLDER}) ..."
        safe_rm_rf "${REPO_FOLDER}"
    fi
    if [ -e "${CACHE_FOLDER}" ]; then
        echo "Removing download cache (${CACHE_FOLDER}) ..."
        safe_rm_rf "${CACHE_FOLDER}"
    fi
    rm -f "${PROJ_ROOT}/sockets-to-exclude"
    echo "mrproper clean complete."
}

# Receive the source from GIT into download cache and copy to target destination
function get_git_source () {
    URL="$1"
    DEST="$2"
    REPO_NAME="$(basename "${DEST}")"
    CACHE_PATH="${CACHE_FOLDER}/${REPO_NAME}"

    mkdir -p "${CACHE_FOLDER}"
    mkdir -p "$(dirname "${DEST}")"

    # Step 1: Ensure clean, pristine clone exists in download cache
    if [ ! -e "${CACHE_PATH}" ]; then
        echo -e "Downloading ${URL} into download cache (${CACHE_PATH}) ...\n"
        if ! git clone --recursive "${URL}" "${CACHE_PATH}"; then
            echo "Error: Cloning ${URL} failed! Cleaning up partial cache..."
            rm -rf "${CACHE_PATH}"
            exit 1
        fi
    else
        echo -e "Using cached pristine repository for ${REPO_NAME} from ${CACHE_PATH}.\n"
    fi

    # Step 2: Ensure working copy exists in destination folder
    if [ ! -e "${DEST}" ]; then
        echo -e "Copying ${REPO_NAME} from cache to working tree (${DEST}) ...\n"
        cp -a "${CACHE_PATH}" "${DEST}"
    else
        echo -e "Working tree ${DEST} already exists.\n"
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
    get_git_source https://git.kernel.org/pub/scm/utils/dtc/dtc.git "${DTC_FOLDER}"
}

function build_fsbl () {
    print_boxed "Building the FSBL"
    mkdir -p "${FSBL_OUTPUT}"
    output_folder="$(realpath "${FSBL_OUTPUT}")"

    xsa_file="$(find "${FPGA_PROJ_FOLDER}" -name "*.xsa" -exec realpath {} \; | head -n 1)"
    if [ -z "${xsa_file}" ] || [ ! -e "${xsa_file}" ]; then
        echo "XSA file doesn't exist in ${FPGA_PROJ_FOLDER} - cannot generate the FSBL"
        exit 1
    fi

    if ! command -v vitis >/dev/null 2>&1; then
        echo "Error: vitis tool is not found in PATH! Make sure Vivado/Vitis is sourced."
        exit 1
    fi

    vitis -s "${SCRIPT_FOLDER}/generate-fsbl.py" "$xsa_file" "$output_folder"
}

function translate_dtc () {
    if [ ! -d "${DTC_FOLDER}" ]; then
        return 0
    fi
    print_boxed "Building DTC"
    pushd . > /dev/null
    cd "${DTC_FOLDER}"
    if [ ! -f "dtc" ]; then
        make -j"${JOBS}"
    fi
    # Export the path to compiled tool
    export PATH="$(pwd):${PATH}"
    dtc_path="$(which dtc)"
    echo -e "Path of used dtc: $dtc_path \n"
    popd > /dev/null
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

function install_kernel_to_rootfs () {
    print_boxed "Installing Linux Kernel & Modules into Rootfs"
    if [ ! -d "${DEBIAN_OUTPUT}" ]; then
        echo "Rootfs directory (${DEBIAN_OUTPUT}) does not exist, skipping kernel installation."
        return 0
    fi

    echo "Installing kernel image to ${DEBIAN_OUTPUT}/boot ..."
    sudo mkdir -p "${DEBIAN_OUTPUT}/boot"
    if [ -f "${KERNEL_OUTPUT}/arch/arm/boot/uImage" ]; then
        sudo cp -f "${KERNEL_OUTPUT}/arch/arm/boot/uImage" "${DEBIAN_OUTPUT}/boot/uImage"
    fi
    if [ -f "${KERNEL_OUTPUT}/arch/arm/boot/zImage" ]; then
        sudo cp -f "${KERNEL_OUTPUT}/arch/arm/boot/zImage" "${DEBIAN_OUTPUT}/boot/zImage"
    fi
    if [ -f "${KERNEL_OUTPUT}/System.map" ]; then
        sudo cp -f "${KERNEL_OUTPUT}/System.map" "${DEBIAN_OUTPUT}/boot/"
    fi
    if [ -f "${KERNEL_OUTPUT}/.config" ]; then
        sudo cp -f "${KERNEL_OUTPUT}/.config" "${DEBIAN_OUTPUT}/boot/config"
    fi

    echo "Installing kernel modules to ${DEBIAN_OUTPUT}/lib/modules ..."
    sudo make -C "${KERNEL_FOLDER}" O="${KERNEL_OUTPUT}" ARCH=arm CROSS_COMPILE="${CROSS_COMPILE}" INSTALL_MOD_PATH="${DEBIAN_OUTPUT}" INSTALL_MOD_STRIP=1 modules_install
}

function build_kernel () {
    print_boxed "Building Linux Kernel"

    # Add the module into the source tree
    if [ ! -e "${KERNEL_MODULE_FOLDER}" ]; then
        echo "Linking module folder into kernel source ..."
        ln -s "${SW_SOURCES}/modules" "${KERNEL_MODULE_FOLDER}"
        pushd "${KERNEL_MODULE_FOLDER}/.." > /dev/null
        # Add the folder with our sources
        sed -E -i 's/endmenu/source \"drivers\/pb-zybo\/Kconfig\"\nendmenu/' Kconfig
        echo "obj-y += pb-zybo/" >> Makefile
        popd > /dev/null
    fi

    pushd "${KERNEL_FOLDER}" > /dev/null

    # Create output folder and make the default configuration
    mkdir -p "${KERNEL_OUTPUT}"
    make O="${KERNEL_OUTPUT}" ARCH=arm xilinx_zynq_defconfig

    echo "Patching the default configuration ..."
    scripts/kconfig/merge_config.sh -m -O "${KERNEL_OUTPUT}" "${KERNEL_OUTPUT}/.config" "${SW_SOURCES}/kernel-config/*.cfg"
    make O="${KERNEL_OUTPUT}" ARCH=arm olddefconfig

    echo "Compiling Linux kernel (uImage) ..."
    make O="${KERNEL_OUTPUT}" ARCH=arm -j${JOBS} uImage LOADADDR=0x00008000
    echo "Compiling Linux kernel modules ..."
    make O="${KERNEL_OUTPUT}" ARCH=arm -j${JOBS} modules
    popd > /dev/null

    # If the rootfs output directory exists, install kernel and modules into it
    if [ -d "${DEBIAN_OUTPUT}" ]; then
        install_kernel_to_rootfs
    fi
}

function build_zynq_dts () {
    print_boxed "Build Zynq DTS"
    mkdir -p "${DT_XLNX_OUTPUT}"

    xsa_file="$(find "${FPGA_PROJ_FOLDER}" -name "*.xsa" -exec realpath {} \; | head -n 1)"
    if [ -z "${xsa_file}" ] || [ ! -e "${xsa_file}" ]; then
        echo "XSA file doesn't exist in ${FPGA_PROJ_FOLDER} - cannot generate DTS"
        exit 1
    fi

    if ! command -v sdtgen >/dev/null 2>&1; then
        echo "Error: sdtgen tool is not found in PATH! Make sure Vivado/Vitis is sourced."
        exit 1
    fi

    sdtgen "${SCRIPT_FOLDER}/generate-dts.tcl" "$xsa_file" "$DT_XLNX_OUTPUT" "$DT_XLNX"

    echo "Translating to DTB ..."
    echo "  * DTS File = ${DTS_TOP}"
    echo "  * DTB File = ${DTB_TOP}"

    echo "Preparing the Device Tree for the system ..."
    pushd "${DT_XLNX_OUTPUT}" > /dev/null

    echo "Preparing system top-level file ..."
    cat system-top.dts "${SW_SOURCES}/device-tree-mods/system-user.dtsi" > user-patched-system-top.dts
    cat pl.dtsi "${SW_SOURCES}/device-tree-mods/pl-custom.dtsi" > user-patched-pl.dts

    echo "DTS ---> DTB blob generation ..."
    gcc -I "${DT_XLNX_OUTPUT}" -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp "${DT_XLNX_OUTPUT}/user-patched-system-top.dts" -o "${DTS_TOP}"
    dtc -i "${SW_SOURCES}/device-tree-mods" -b 0 -@ -I dts -O dtb -o "${DTB_TOP}" "${DTS_TOP}"

    gcc -I "${DT_XLNX_OUTPUT}" -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp "${DT_XLNX_OUTPUT}/user-patched-pl.dts" -o "${DTS_PL_TOP}"
    dtc -i "${SW_SOURCES}/device-tree-mods" -b 0 -@ -I dts -O dtb -o "${DTB_PL_TOP}" "${DTS_PL_TOP}"
    popd > /dev/null
}

function build_debian_rootfs () {
    print_boxed "Build Debian rootfs"

    if ! command -v qemu-arm-static >/dev/null 2>&1; then
        echo "Error: qemu-arm-static is not installed! Please install qemu-user-static."
        exit 1
    fi

    pushd . > /dev/null
    echo "Bootstraping initial system - Debian ${DEBIAN_DISTRO} ..."
    sudo umount -l "${DEBIAN_OUTPUT}/proc" 2>/dev/null || true
    sudo umount -l "${DEBIAN_OUTPUT}/sys" 2>/dev/null || true
    sudo umount -l "${DEBIAN_OUTPUT}/dev/pts" 2>/dev/null || true
    sudo rm -rf "${DEBIAN_OUTPUT}"
    mkdir -p "${DEBIAN_OUTPUT}"
    sudo chown 0:0 "${DEBIAN_OUTPUT}"
    sudo debootstrap --arch=armhf --foreign "${DEBIAN_DISTRO}" "${DEBIAN_OUTPUT}"
    sudo cp "$(which qemu-arm-static)" "${DEBIAN_OUTPUT}/usr/bin/"

    # Prevent daemons from starting inside the chroot
    printf '#!/bin/sh\nexit 101\n' | sudo tee "${DEBIAN_OUTPUT}/usr/sbin/policy-rc.d" > /dev/null
    sudo chmod 755 "${DEBIAN_OUTPUT}/usr/sbin/policy-rc.d"

    # Install kernel and modules directly into rootfs
    install_kernel_to_rootfs

    # Copy the script which generates the u-boot images after kernel install
    sudo cp "${SCRIPT_FOLDER}/uimage-gen" "${DEBIAN_OUTPUT}/etc/kernel/postinst.d/"
    sudo chmod 755 "${DEBIAN_OUTPUT}/etc/kernel/postinst.d/uimage-gen"
    # Copy Xilinx helping tools
    sudo cp -r "${XILINX_TOOLS}" "${DEBIAN_OUTPUT}/usr/src/"
    sudo cp -r "${SW_SOURCES}" "${DEBIAN_OUTPUT}/usr/src/pb-zybo"

    # Copy FPGA bitstream and device tree overlay if available
    sudo mkdir -p "${DEBIAN_OUTPUT}/lib/firmware/fpga"
    fpga_bit_path="$(find "${FPGA_PROJ_FOLDER}" -name "board_design_wrapper.bit.bin" | head -n 1)"
    if [ -n "${fpga_bit_path}" ] && [ -e "${fpga_bit_path}" ]; then
        sudo cp "${fpga_bit_path}" "${DEBIAN_OUTPUT}/lib/firmware/fpga/"
    fi
    if [ -e "${DTB_PL_TOP}" ]; then
        sudo cp "${DTB_PL_TOP}" "${DEBIAN_OUTPUT}/lib/firmware/fpga/"
    fi

    # Run chroot second stage and bootstrap
    echo "Copying bootstrap script inside the chroot ..."
    sudo cp "${SCRIPT_FOLDER}/bootstrap-debian-image.sh" "${DEBIAN_OUTPUT}/"

    echo "Running chroot ..."
    sudo chroot "${DEBIAN_OUTPUT}" /bin/bash -c "bash /bootstrap-debian-image.sh ${DEBIAN_DISTRO}"
    sudo umount -l "${DEBIAN_OUTPUT}/dev/pts" 2>/dev/null || true
    sudo umount -l "${DEBIAN_OUTPUT}/sys" 2>/dev/null || true
    sudo umount -l "${DEBIAN_OUTPUT}/proc" 2>/dev/null || true
    sudo rm -f "${DEBIAN_OUTPUT}/bootstrap-debian-image.sh"
    sudo rm -f "${DEBIAN_OUTPUT}/usr/sbin/policy-rc.d"

    echo "Packing rootfs into archive (${DEBIAN_ARCHIVE_FILE}) ..."
    mkdir -p "$(dirname "${DEBIAN_OUTPUT}/../${DEBIAN_ARCHIVE_FILE}")"
    cd "${DEBIAN_OUTPUT}"
    sudo find . -type s | sed 's/^\.\///' > ../sockets-to-exclude
    sudo tar --preserve-permissions -czf ../"${DEBIAN_ARCHIVE_FILE}" -X ../sockets-to-exclude -C "${DEBIAN_OUTPUT}" .
    popd > /dev/null
}

function build_bootloader () {
    print_boxed "Building the Bootloader"
    mkdir -p "${BOOTLOADER_OUTPUT}"

    # Collect artefacts and prepare the boot image
    pushd .
    cd "${OUT_FOLDER}"
    find "${FSBL_OUTPUT}" \( -name "fsbl.elf" -o -name "executable.elf" \) -exec cp {} "${BOOTLOADER_OUTPUT}/fsbl.elf"  \;
    find "${UBOOT_OUTPUT}" -name u-boot.elf -exec cp {} "${BOOTLOADER_OUTPUT}" \;

    cd "${BOOTLOADER_OUTPUT}"
    echo "Generating the BOOT.BIN file (FSBL + U-Boot) ..."
    bootgen -arch zynq -image "${CONF_FOLDER}/image.bif" -w -o i BOOT.BIN
    echo "Generating the boot.scr file ..."
    mkimage -A arm -O linux -T script -C none -a 0 -e 0 -n "Boot image script" -d "${CONF_FOLDER}/u-boot.txt" boot.scr
    popd
}

function build_tarball () {
    print_boxed "Collecting all required artefacts into tarball (${TARBALL_OUTPUT}) ..."
    mkdir -p "${TARBALL_OUTPUT}"

    FILES=("${DEBIAN_OUTPUT}/../${DEBIAN_ARCHIVE_FILE}" \
    "${DTB_TOP}" \
    "${BOOTLOADER_OUTPUT}/BOOT.BIN" \
    "${BOOTLOADER_OUTPUT}/boot.scr" \
    "${KERNEL_OUTPUT}/arch/arm/boot/uImage" \
    "${DEBIAN_OUTPUT}/boot/initramdisk.image.gz" )

    for file in "${FILES[@]}"; do
        echo "Processing: ${file}"
        if [ -e "${file}" ]; then
            cp -f "${file}" "${TARBALL_OUTPUT}/"
        else 
            echo "File ${file} wasn't found!"
        fi
    done
}

# #######################################################################################
# Main body
# #######################################################################################

# Parse arguments ----------------------------------------------------------------
p_clean=0
p_mrproper=0
p_download_sources=0
p_build_fsbl=0
p_rebuild_all=0
p_build_uboot=0
p_build_kernel=0
p_build_dts=0
p_build_rootfs=0
p_build_boot=0
p_tarball=0
while getopts "hsfrukdobtcmj:v:" opt; do
    case "$opt" in
    h)  print_usage
        exit 0
        ;;
    c)  p_clean=1
        ;;
    m)  p_mrproper=1
        ;;
    s)  p_download_sources=1
        ;;
    f)  p_build_fsbl=1
        ;;
    r)  p_rebuild_all=1
        ;; 
    u)  p_build_uboot=1
        ;;
    k)  p_build_kernel=1
        ;;
    d)  p_build_dts=1
        ;;
    o)  p_build_rootfs=1
        ;;
    b)  p_build_boot=1
        ;;
    t)  p_tarball=1
        ;;
    v)  DEBIAN_DISTRO="${OPTARG}"
        DEBIAN_ARCHIVE_FILE="debian-${DEBIAN_DISTRO}-rootfs-vanilla.tgz"
        ;;
    j)  if [ -z "${OPTARG##*[!0-9]*}" ]; then
            echo "Number of jobs can be integer only!"
            exit 1
        fi
        JOBS=${OPTARG}
        ;;
    esac
done
shift $((OPTIND -1))

# Main body  ---------------------------------------------------------------------

# Calculate if any specific targets were requested
target_count=$((p_download_sources + p_build_fsbl + p_build_uboot + p_build_kernel + p_build_dts + p_build_rootfs + p_build_boot + p_tarball))

# Handle mrproper cleaning
if [ "${p_mrproper}" -eq 1 ]; then
    clean_mrproper
    # If no build targets were requested, exit now
    if [ "${target_count}" -eq 0 ] && [ "${p_rebuild_all}" -eq 0 ]; then
        echo "Done!"
        exit 0
    fi
fi

# Handle clean build directory or rebuild all
if [ "${p_clean}" -eq 1 ] || [ "${p_rebuild_all}" -eq 1 ]; then
    clean_output
    # If clean was called standalone without build targets, exit now
    if [ "${p_clean}" -eq 1 ] && [ "${target_count}" -eq 0 ] && [ "${p_rebuild_all}" -eq 0 ]; then
        echo "Done!"
        exit 0
    fi
fi

if [ "${p_download_sources}" -eq 1 ] || { [ "${target_count}" -eq 0 ] && [ ! -e "${REPO_FOLDER}" ]; }; then
    get_sources
fi

echo "Number of build jobs: ${JOBS}"

# If specific targets were requested, execute only those; otherwise run incremental auto-build
if [ "${p_build_fsbl}" -eq 1 ] || { [ "${target_count}" -eq 0 ] && [ ! -e "${FSBL_OUTPUT}" ]; }; then
    build_fsbl
fi

translate_dtc

if [ "${p_build_uboot}" -eq 1 ] || { [ "${target_count}" -eq 0 ] && [ ! -e "${UBOOT_OUTPUT}" ]; }; then
    build_uboot
fi
# Update the path for the mkimage if built in u-boot
if [ -d "${UBOOT_OUTPUT}/tools" ]; then
    export PATH="${UBOOT_OUTPUT}/tools:${PATH}"
fi
if command -v mkimage >/dev/null 2>&1; then
    echo -e "Path of used mkimage: $(which mkimage) \n"
fi

if [ "${p_build_kernel}" -eq 1 ] || { [ "${target_count}" -eq 0 ] && [ ! -f "${KERNEL_OUTPUT}/arch/arm/boot/uImage" ]; }; then
    build_kernel
fi

if [ "${p_build_dts}" -eq 1 ] || { [ "${target_count}" -eq 0 ] && [ ! -e "${DT_XLNX_OUTPUT}" ]; }; then
    build_zynq_dts
fi

if [ "${p_build_rootfs}" -eq 1 ] || { [ "${target_count}" -eq 0 ] && [ ! -e "${DEBIAN_OUTPUT}" ]; }; then
    build_debian_rootfs
fi

if [ "${p_build_boot}" -eq 1 ] || { [ "${target_count}" -eq 0 ] && [ ! -e "${BOOTLOADER_OUTPUT}" ]; }; then
    build_bootloader
fi

if [ "${p_tarball}" -eq 1 ]; then
    build_tarball
fi

echo "Done!"
exit 0
