# -------------------------------------------------------------------------------
#  PROJECT: Zybo Base
# -------------------------------------------------------------------------------
#  AUTHORS: Pavel Benacek <pavel.benacek@gmail.com>
#  LICENSE: The MIT License (MIT), please read LICENSE file
#  WEBSITE: https://github.com/benycze/zybo-base
# -------------------------------------------------------------------------------

# #############################################################################
# Source project creation
# #############################################################################

if {[info exists ::create_path]} {
	set dest_dir $::create_path
} else {
	set dest_dir [file normalize [file dirname [info script]]]
}
puts "INFO: Creating new project in $dest_dir"

# Set the project name
set _xil_proj_name_ "zybo-base"

# Create project
create_project ${_xil_proj_name_} ${dest_dir}  -part xc7z020clg400-1

# Set the reference directory for source file relative paths (by default the value is script directory path)
set origin_dir "${dest_dir}/.."
puts "INFO: Setting the origin dir to ${origin_dir}"

# Set the directory path for the new project
set proj_dir [get_property directory [current_project]]

# Set project properties
set obj [current_project]
set_property -name "board_part" -value "digilentinc.com:zybo-z7-20:part0:1.0" -objects $obj
set_property -name "default_lib" -value "xil_defaultlib" -objects $obj
set_property -name "enable_vhdl_2008" -value "1" -objects $obj
set_property -name "ip_cache_permissions" -value "read write" -objects $obj
set_property -name "mem.enable_memory_map_generation" -value "1" -objects $obj
set_property -name "platform.board_id" -value "zybo-z7-20" -objects $obj
set_property -name "sim.central_dir" -value "$proj_dir/${_xil_proj_name_}.ip_user_files" -objects $obj
set_property -name "sim.ip.auto_export_scripts" -value "1" -objects $obj
set_property -name "simulator_language" -value "Mixed" -objects $obj
set_property -name "ip_output_repo" -value "$proj_dir/${_xil_proj_name_}.cache/ip" -objects $obj

# Create 'sources_1' fileset (if not found)
if {[string equal [get_filesets -quiet sources_1] ""]} {
  create_fileset -srcset sources_1
}

# Create 'constrs_1' fileset (if not found)
if {[string equal [get_filesets -quiet constrs_1] ""]} {
  create_fileset -constrset constrs_1
}

# Add local files from the original project (-no_copy_sources specified)
# Fill all included paths here
puts "INFO: Adding files into the project ..."
set files [list \
 [file normalize "${origin_dir}/src/bd/board_design/board_design.bd" ] \
]
add_files -scan_for_includes -verbose -fileset sources_1 $files

# Register the BD  
set bd_file "src/bd/board_design/board_design.bd"
set bd_file_obj [get_files -of_objects [get_filesets sources_1] [list "*$bd_file"]]
set_property -name "registered_with_manager" -value "1" -objects $bd_file_obj

# Deal with IP cores
puts "INFO: Updating IP cores ..."
set_property ip_repo_paths [file normalize ${origin_dir}/src/3rd-party/vivado-library/ip] [current_project]
update_ip_catalog -rebuild -verbose

# Constraint files
set constr_files [list \
  [glob -nocomplain ${origin_dir}/src/constr/*.xdc] \
]

add_files -fileset constrs_1 $constr_files

# Make a wrapper of top-level design and set is as a top
add_files -norecurse [ \
  make_wrapper -files [get_files -of_objects [get_filesets sources_1] [list "*src/bd/board_design/board_design.bd"]] -top -force
]

set obj [get_filesets sources_1]
set_property "top" "board_design_wrapper" $obj

# #############################################################################
# Synthesis configuration 
# #############################################################################

# Create 'synth_1' run (if not found)
if {[string equal [get_runs -quiet synth_1] ""]} {
    create_run -name synth_1 -part xc7z020clg400-1 -flow {Vivado Synthesis 2019} -strategy "Vivado Synthesis Defaults" -report_strategy {No Reports} -constrset constrs_1
} else {
  set_property strategy "Vivado Synthesis Defaults" [get_runs synth_1]
  set_property flow "Vivado Synthesis 2019" [get_runs synth_1]
}
set obj [get_runs synth_1]
set_property set_report_strategy_name 1 $obj
set_property report_strategy {Vivado Synthesis Default Reports} $obj
set_property set_report_strategy_name 0 $obj

# set the current synth run
current_run -synthesis [get_runs synth_1]

# #############################################################################
# Implementation configuration 
# #############################################################################

# Create 'impl_1' run (if not found)
if {[string equal [get_runs -quiet impl_1] ""]} {
    create_run -name impl_1 -part xc7z020clg400-1 -flow {Vivado Implementation 2019} -strategy "Vivado Implementation Defaults" -report_strategy {No Reports} -constrset constrs_1 -parent_run synth_1
} else {
  set_property strategy "Vivado Implementation Defaults" [get_runs impl_1]
  set_property flow "Vivado Implementation 2019" [get_runs impl_1]
}

set obj [get_runs impl_1]
set_property -name "strategy" -value "Vivado Implementation Defaults" -objects $obj
set_property -name "steps.phys_opt_design.is_enabled" -value "1" -objects $obj
set_property -name "steps.write_bitstream.args.readback_file" -value "0" -objects $obj
set_property -name "steps.write_bitstream.args.verbose" -value "0" -objects $obj

# set the current impl run
current_run -implementation [get_runs impl_1]

puts "INFO: Project created:${_xil_proj_name_}"

