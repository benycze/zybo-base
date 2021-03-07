# RGB LED Module Test

This tool is used for the testing of rgb-led-module driver which mainly tests the IOCTL commands. Finally, it also runs some demo where you will see flashing led.

So, compile and enjoy the device driver demo :-)

The binary accepts the path to the char device in `/dev` folder

To compile it locally, run the following command:

```bash
make CC=arm-linux-gnueabihf-gcc
```

or for debug

```bash
make CC=arm-linux-gnueabihf-gcc CFLAGS="-g -O0"
```
