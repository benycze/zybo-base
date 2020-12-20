# -------------------------------------------------------------------------------
#  PROJECT: Zybo Base
# -------------------------------------------------------------------------------
#  AUTHORS: Pavel Benacek <pavel.benacek@gmail.com>
#  LICENSE: The MIT License (MIT), please read LICENSE file
#  WEBSITE: https://github.com/benycze/zybo-base
# -------------------------------------------------------------------------------

if {[info exists ::xsa_file]} {
	set xsa_file $::xsa_file
} else {
	set xsa_file [concat [file normalize [file dirname [info script]]] "/board_design_wrapper.xsa"]
}

# Run the translation flow
#
# The project is preconfigured from the create_project.tcl script
open_project zybo-base.xpr

# Run the synthesis, implementation, write a bitstream and export the architecture
puts "INFO: Translating the desing ..."
launch_runs synth_1 -jobs 2 -verbose
launch_runs impl_1 -jobs 2 -verbose
launch_runs impl_1 -to_step write_bitstream -jobs 2

puts "INFO: Exporting the design to XSA: $xsa_file"
write_hw_platform -fixed -force  -include_bit -file $xsa_file

puts "***************************"
puts "DONE!"
puts "***************************"