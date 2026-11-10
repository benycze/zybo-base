#!/usr/bin/env python3
# -------------------------------------------------------------------------------
#  PROJECT: Zybo Base
# -------------------------------------------------------------------------------
#  AUTHORS: Pavel Benacek <pavel.benacek@gmail.com>
#  LICENSE: The MIT License (MIT), please read LICENSE file
#  WEBSITE: https://github.com/benycze/zybo-base
# -------------------------------------------------------------------------------

import sys
import os
import shutil
import vitis

if len(sys.argv) < 3:
    print("Usage: vitis -s generate-fsbl.py <xsa_file> <output_folder>")
    sys.exit(1)

xsa_file = os.path.abspath(sys.argv[1])
output_folder = os.path.abspath(sys.argv[2])
os.makedirs(output_folder, exist_ok=True)

print(f"* XSA File:      {xsa_file}")
print(f"* OUTPUT folder: {output_folder}")

# Create a clean temporary workspace for Vitis inside the output directory
ws_path = os.path.join(output_folder, "_vitis_ws")
if os.path.exists(ws_path):
    shutil.rmtree(ws_path)
os.makedirs(ws_path, exist_ok=True)

client = vitis.create_client()
client.set_workspace(path=ws_path)

platform = client.create_platform_component(
    name="fsbl_platform",
    hw_design=xsa_file,
    os="standalone",
    cpu="ps7_cortexa9_0"
)

platform.build()
vitis.dispose()

# Search for the built fsbl.elf
found_fsbl = False
for root, dirs, files in os.walk(ws_path):
    for f in files:
        if f in ("fsbl.elf", "executable.elf"):
            src = os.path.join(root, f)
            dst = os.path.join(output_folder, "fsbl.elf")
            shutil.copy2(src, dst)
            found_fsbl = True
            print(f"Generated FSBL copied to: {dst}")
            break
    if found_fsbl:
        break

# Clean up the large intermediate workspace to save disk space
if os.path.exists(ws_path):
    shutil.rmtree(ws_path, ignore_errors=True)

if not found_fsbl:
    print("Error: fsbl.elf was not found in the build output!")
    sys.exit(1)

print("FSBL generation completed successfully.")
