#!/usr/bin/env bash
git clean -d -f -x .
vivado -mode batch -source create_project.tcl