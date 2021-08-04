SRC_URI += "file://kernel_config.cfg"

FILESEXTRAPATHS_prepend := "${THISDIR}/linux:${EXT_SRC_ROOT}/kernel-config:"
