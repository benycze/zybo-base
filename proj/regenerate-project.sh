#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"
cd "${SCRIPT_DIR}"

git clean -d -f -x -e board_files -e "board_files/**" .
vivado -mode batch -source create_project.tcl