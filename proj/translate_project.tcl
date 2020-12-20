# -------------------------------------------------------------------------------
#  PROJECT: Zybo Base
# -------------------------------------------------------------------------------
#  AUTHORS: Pavel Benacek <pavel.benacek@gmail.com>
#  LICENSE: The MIT License (MIT), please read LICENSE file
#  WEBSITE: https://github.com/benycze/zybo-base
# -------------------------------------------------------------------------------

# Main body ###################################################################
if {[info exists ::xsa_file]} {
	set xsa_file $::xsa_file
} else {
	set xsa_file [join [file normalize [file dirname [info script]]] "/board_design_wrapper.xsa"]
}

if {[info exists ::results_dir]} {
	set results_dir $::results_dir
} else {
	set results_dir [join [file normalize [file dirname [info script]]] "/results"]
}

# Run the translation flow
#
# The project is preconfigured from the create_project.tcl script
open_project zybo-base.xpr

# Run the synthesis, implementation, write a bitstream and export the architecture
puts "INFO: Translating the desing ..."
    # Synthesis ---------------------------------
synth_design
write_checkpoint -force ${results_dir}/post_synth.dcp

    # Optimizations & routing -------------------
opt_design
route_design
write_checkpoint -force ${results_dir}/post_route.dcp

    # Placing -----------------------------------
place_design
phys_opt_design
write_checkpoint -force ${results_dir}/post_place.dcp

    # Reporting ---------------------------------
report_route_status -file ${results_dir}/post_route_status.rpt
report_timing_summary -file ${results_dir}/post_route_timing_summary.rpt
report_utilization -file ${results_dir}/post_place_util.rpt
report_power -file ${results_dir}/post_route_power.rpt
report_drc -file ${results_dir}/post_imp_drc.rpt
report_io -file ${results_dir}/post_imp_io.rpt

    # Bitstream generation ----------------------
write_bitstream -verbose -force ${results_dir}/top.bit

    # XSA export --------------------------------
puts "INFO: Exporting the design to XSA: $xsa_file"
write_hw_platform -fixed -force  -include_bit -file $xsa_file

puts "INFO: DONE!"
