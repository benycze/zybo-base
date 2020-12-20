# Zybo Base Image

This repository contains the basic template which can be used for the template for development of another Zynq based images.
It can be also considered as tutorial for Zybo Z7-20 board (or Zynq based images).

**Required tools:**
* Vivado 2019.2
* Petalinux 2020.2

## How to Generate the Vivado Project

The project for Vivado tool is initialized based on the `create_project.tcl` script. The project files are generated
inside the `proj` folder by running of the following command (or run the `regenerate-project.sh` which also cleans the folder using git tool):

```bash
vivado -mode batch -source create_project.tcl
```

or

```bash
./regenerate-project.sh
```

After that, you can use the Vivado and open the `zybo-base.xpr` file. All source files, board designs, HDL and
IP cores are under the `src` directory and they are included in the [create_project.tcl](proj/create_project.tcl)
file. Therefore, edit this file if you want to version it and have a possibility to restore a project!

There is also a helping (and simple) tcl script which can be used for the automatic export of HW design from the 
command line.
It just opens the created project and runs syntheis, implementation, bitstream generation and HW export to 
XSA file. All you have to do is to run the similar command like before:

```bash
vivado -mode batch -source translate_project.tcl
```
