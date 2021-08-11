#!/usr/bin/env bash

set -e

vivado -mode batch -source translate_project.tcl
echo "Generation bin file ..."
scripts/convert-bit `find -name *.bit` board_design_wrapper.bit.bin
echo "Done!"