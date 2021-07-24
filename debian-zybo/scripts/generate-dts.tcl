# -------------------------------------------------------------------------------
#  PROJECT: Zybo Base
# -------------------------------------------------------------------------------
#  AUTHORS: Pavel Benacek <pavel.benacek@gmail.com>
#  LICENSE: The MIT License (MIT), please read LICENSE file
#  WEBSITE: https://github.com/benycze/zybo-base
# -------------------------------------------------------------------------------

set xsa [lindex $argv 0]
set output [lindex $argv 1]
set repo [lindex $argv 2]

puts "* XSA File:       $xsa"
puts "* OUTPUT folder:  $output"
puts "* REPO folder:    $repo"

hsi open_hw_design $xsa
hsi set_repo_path $repo
hsi create_sw_design device-tree -os device_tree -proc ps7_cortexa9_0
hsi generate_target -dir $output
