# Zybo Base Image

This repository contains the basic template which can be used for the template for development of another Zynq based images.
It can be also considered as tutorial for Zybo Z7-20 board (or Zynq based images).

**Required tools:**
* Vivado 2019.2
* Petalinux 2020.2

## How to Generate the Vivado Project

The project for Vivado tool is initialized based on the `create_project.tcl` script. The project files are generated
inside the `proj` folder by running of the following command:

```bash
vivado -mode batch -source create_project.tcl
```

After that, you can use the Vivado and open the `zybo-base.xpr` file. All source files, board designs, HDL and
IP cores are under the `src` directory and they are included in the [create_project.tcl](proj/create_project.tcl)
file. Therefore, edit this file if you want to version it and have a possibility to restore a project!
