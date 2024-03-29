#
# This file is the rgb-ledmodule-test recipe.
#

SUMMARY = "Simple rgb-ledmodule-test application"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"


FILESEXTRAPATHS_prepend := "${EXT_SRC_ROOT}/apps/rgbled-test:"

SRC_URI = "file://rgb-led-test.c \
	   file://Makefile \
		  "

S = "${WORKDIR}"

do_compile() {
	     oe_runmake
}

do_install() {
	     install -d ${D}${bindir}
	     install -m 0755 rgb-ledmodule-test ${D}${bindir}
}
