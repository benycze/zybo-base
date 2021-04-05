# LED Module Test

This tool is used for the testing of led-module driver which mainly tests the IOCTL commands. Finally, it also runs some demo where you will see flashing leds.

So, compile and enjoy the device driver demo :-)

The binary accepts the path to the char device in `/dev` folder

To compile it locally, run the following command:

```bash
make
```

or for debug

```bash
make CFLAGS="-g -O0"
```
