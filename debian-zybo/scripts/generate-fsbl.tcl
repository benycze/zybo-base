# -------------------------------------------------------------------------------
#  PROJECT: Zybo Base
# -------------------------------------------------------------------------------
#  AUTHORS: Pavel Benacek <pavel.benacek@gmail.com>
#  LICENSE: The MIT License (MIT), please read LICENSE file
#  WEBSITE: https://github.com/benycze/zybo-base
# -------------------------------------------------------------------------------

set xsa [lindex $argv 0]
set output [lindex $argv 1]

puts "* XSA File:       $xsa"
puts "* OUTPUT folder:  $output"

set hwdsgn [hsi open_hw_design $xsa]
hsi generate_app -hw $hwdsgn -os standalone -proc ps7_cortexa9_0 -app zynq_fsbl -sw fsbl -dir $output