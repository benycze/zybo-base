#!/usr/bin/env bash
# -------------------------------------------------------------------------------
#  PROJECT: Zybo Base
# -------------------------------------------------------------------------------
#  AUTHORS: Pavel Benacek <pavel.benacek@gmail.com>
#  LICENSE: The MIT License (MIT), please read LICENSE file
#  WEBSITE: https://github.com/benycze/zybo-base
# -------------------------------------------------------------------------------

set -e

# Arguments filtering (no big checks there)...
distro=${1:-trixie}

export LANG=en_US.UTF-8

# Run second stage if not yet done
if [ -f /debootstrap/debootstrap ]; then
    /debootstrap/debootstrap --second-stage
fi

# Cleanup trap to ensure virtual filesystems are cleanly unmounted on exit
cleanup() {
    echo "Unmounting virtual filesystems in chroot ..."
    umount /dev/pts 2>/dev/null || true
    umount /sys 2>/dev/null || true
    umount /proc 2>/dev/null || true
}
trap cleanup EXIT

# Mount virtual filesystems needed for package installation and Python py3compile
echo "Mounting /proc, /sys, and /dev/pts ..."
mount -t proc proc /proc 2>/dev/null || true
mount -t sysfs sysfs /sys 2>/dev/null || true
mount -t devpts devpts /dev/pts 2>/dev/null || true

# #######################################################################################
# Package installation
# #######################################################################################

DEB_PACKAGES="openssh-server vim build-essential cmake git device-tree-compiler mc htop gdb \
    initramfs-tools ifupdown net-tools resolvconf sudo less hwinfo tcsh zsh file pkg-config pkgconf u-boot-tools libssl-dev \
    socat python3 python3-dev python3-setuptools python3-wheel python3-pip python3-numpy \
    screen bash-completion haveged gdbserver iproute2 iputils-ping" 

echo "Setting APT and installing packages ..."

# Remove legacy sources.list and configure modern deb822 debian.sources
rm -f /etc/apt/sources.list
mkdir -p /etc/apt/sources.list.d

cat <<EOT > /etc/apt/sources.list.d/debian.sources
Types: deb deb-src
URIs: http://deb.debian.org/debian
Suites: ${distro} ${distro}-updates
Components: main contrib non-free non-free-firmware
Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg

Types: deb deb-src
URIs: http://deb.debian.org/debian-security
Suites: ${distro}-security
Components: main contrib non-free non-free-firmware
Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg
EOT

cat <<EOT > /etc/apt/apt.conf.d/71-no-recommends
APT::Install-Recommends "0";
APT::Install-Suggests   "0";
EOT

echo "Installing packages ..."
apt update
dpkg --configure -a || true
apt --fix-broken install -y || true
apt upgrade -y
apt install -y locales dialog
sed -i -e "s/# $LANG.*/$LANG UTF-8/" /etc/locale.gen && \
    dpkg-reconfigure --frontend=noninteractive locales && \
    update-locale LANG=$LANG
apt install -y ${DEB_PACKAGES}

# Configure kernel modules and initramfs if installed
echo "Configuring kernel modules and initramfs ..."
kver="$(ls -1 /lib/modules 2>/dev/null | tail -n 1)"
if [ -n "${kver}" ]; then
    echo "Running depmod for kernel ${kver} ..."
    depmod -a "${kver}" || true
    echo "Generating initramfs for kernel ${kver} ..."
    update-initramfs -c -k "${kver}" || mkinitramfs -o "/boot/initrd.img-${kver}" "${kver}" || true
    if [ -f "/boot/initrd.img-${kver}" ]; then
        echo "Generating U-Boot initramdisk image (/boot/initramdisk.image.gz) ..."
        mkimage -A arm -T ramdisk -C gzip -n "Linux initramdisk" -d "/boot/initrd.img-${kver}" /boot/initramdisk.image.gz || true
    fi
fi

# Install non-distro tools
echo "Installing Xilinx tools ..."
for tool in /usr/src/xilinx-tools/*; do
    [ -d "${tool}" ] || continue
    make -C ${tool} install clean
done

echo "Install Zybo Base tools ..."
for tool in /usr/src/pb-zybo/apps/*; do
    [ -d "${tool}" ] || continue
    make -C ${tool} install clean
done

echo "Setting system ..."
# Setup hostname  & initial stuff
echo debian-zybo > /etc/hostname
echo "root:root" | chpasswd

# Configure fstab
echo "/dev/mmcblk0p1 /boot vfat defaults 0 0" >> /etc/fstab
echo "/dev/mmcblk0p2 / ext4 defaults 0 0" >> /etc/fstab

# Configure timezone
echo "Europe/Prague" > /etc/timezone
dpkg-reconfigure -f noninteractive tzdata

# Setup user
adduser --disabled-password --gecos "" fpga
echo "fpga:fpga" | chpasswd
echo "fpga ALL=(ALL:ALL) ALL" > /etc/sudoers.d/fpga

# Configure SSH
mkdir -p /etc/ssh/sshd_config.d
cat <<EOT > /etc/ssh/sshd_config.d/zybo.conf
PasswordAuthentication yes
PermitRootLogin yes
EOT

# Configure network
cat <<EOT > /etc/network/interfaces.d/eth0
allow-hotplug eth0
iface eth0 inet dhcp
EOT

# Create & configure FPGA service
cat <<EOT > /etc/systemd/system/fpga.service
[Unit]
Description=Load the FPGA bitstream

[Service]
Type=oneshot
ExecStart=/usr/bin/fpgautil -b /lib/firmware/fpga/board_design_wrapper.bit.bin -o /lib/firmware/fpga/board_design_wrapper.bit.bin.dtbo
RemainAfterExit=true
StandardOutput=journal

[Install]
WantedBy=multi-user.target
EOT

ln -sf /etc/systemd/system/fpga.service /etc/systemd/system/multi-user.target.wants/fpga.service

# Cleanup & finish
apt-get clean
dpkg -l > dpkg-list.txt
rm -f /usr/bin/qemu-arm-static

echo "System ready!"
