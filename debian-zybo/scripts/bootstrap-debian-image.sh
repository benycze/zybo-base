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
distro=${1}

export LANG=en_US.UTF-8
/debootstrap/debootstrap --second-stage

# #######################################################################################
# Package installation
# #######################################################################################

DEB_PACKAGES="openssh-server vim build-essential cmake git device-tree-compiler mc htop gdb gdb-doc \
    initramfs-tools net-tools resolvconf sudo less hwinfo tcsh zsh file pkg-config u-boot-tools libssl-dev \
    socat python  python-dev  python-setuptools  python-wheel  python-pip \
    python3 python3-dev python3-setuptools python3-wheel python3-pip python-numpy python3-numpy \
    screen bash-completion haveged gdbserver" 

echo "Setting APT and installing packages ..."

cat <<EOT > /etc/apt/sources.list
deb     http://ftp.cz.debian.org/debian            ${distro}         main contrib non-free
deb-src http://ftp.cz.debian.org/debian            ${distro}         main contrib non-free
deb     http://ftp.cz.debian.org/debian            ${distro}-updates main contrib non-free
deb-src http://ftp.cz.debian.org/debian            ${distro}-updates main contrib non-free
deb     http://security.debian.org/debian-security ${distro}/updates main contrib non-free
deb-src http://security.debian.org/debian-security ${distro}/updates main contrib non-free
EOT

cat <<EOT > /etc/apt/apt.conf.d/71-no-recommends
APT::Install-Recommends "0";
APT::Install-Suggests   "0";
EOT

echo "Installing packages ..."
apt update
apt upgrade -y
apt install -y locales dialog
sed -i -e "s/# $LANG.*/$LANG UTF-8/" /etc/locale.gen && \
    dpkg-reconfigure --frontend=noninteractive locales && \
    update-locale LANG=$LANG
apt install -y ${DEB_PACKAGES}

# Install kernel package
echo "Installing kernel ..."
dpkg -i /*.deb

# Install non-distro tools
echo "Installing Xilinx tools ..."
for tool in /usr/src/xilinx-tools/*; do
    make -C ${tool} install clean
done

echo "Install Zybo Base tools ..."
for tool in /usr/src/pb-zybo/apps/*; do
    make -C ${tool} install clean
done

echo "Setting system ..."
# Setup hostname  & initial stuff
echo debian-zybo > /etc/hostname
echo "root:root" | chpasswd
cat <<EOT >> /etc/securetty
# Seral Port for Xilinx Zynq
ttyPS0
EOT

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
sed -i -e 's/#PasswordAuthentication/PasswordAuthentication/g' /etc/ssh/sshd_config

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

ln -s /etc/systemd/system/fpga.service /etc/systemd/system/multi-user.target.wants/fpga.service

# Cleanup & finish
apt-get clean
dpkg -l > dpkg-list.txt
rm -f /usr/bin/qemu-arm-static
rm -f /*.deb

echo "System ready!"