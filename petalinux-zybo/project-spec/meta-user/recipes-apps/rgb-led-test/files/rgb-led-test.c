/*  led-module.c - Example device driver for the LED device

* Copyright (C) 2021 Pavel Benacek
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.

*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License along
*   with this program. If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include <linux/ioctl.h>
#include <linux/fcntl.h>
#include <getopt.h>
#include <string.h>

/* Declare IOCTL handlers */
#define LED_IOCTL_MAGIC			'l'
#define LED_IOCTL_GET_VAL		_IOR(LED_IOCTL_MAGIC, 0, unsigned long)
#define LED_IOCTL_SET_VAL		_IOW(LED_IOCTL_MAGIC, 1, unsigned long)
#define LED_IOCTL_SET_PERIOD	_IOW(LED_IOCTL_MAGIC, 2, unsigned long)
#define LED_IOCTL_GET_PERIOD	_IOR(LED_IOCTL_MAGIC, 3, unsigned long)
#define LED_IOCTL_INIT			_IO (LED_IOCTL_MAGIC, 4)

/* Some helping macros */
#define RET_OK 0
#define RET_ERR 1

/* Helping macro which runs the test, checks the resutl and
 * runs the cleanup function in the case of any problem */
#define CHECK_FUNC(x_fn,cln_fn) { \
    int rc = x_fn; \
    if(rc != RET_OK) { \
        cln_fn; \
        return rc; \
        } \
    }

static void print_help() {
    printf("Tool for testing of the LED module driver.\n");
    printf("\n\n");
    printf("\t-h = prints this help\n");
    printf("\t-d = device to open\n");
    return;
}

static void wait_for_key_press() {
    int ch;
    printf("Press enter key to continue ...\n");
    while ((ch = getchar()) != '\n') {};
    return;
}

static void print_box(const char* msg) {
    printf("=====================================================\n");
    printf("%s\n", msg);
    printf("=====================================================\n");
    return;
}

/* IOCTL test functions - each function tests the individual part of the IOCTL call
 * and all of them accepts the file descriptor (fd). Everything is fine iff the 
 * RET_OK is returned
 */

static int test_ioctl_period(int fd) {
    const __u32 ref_period = 8192;
    const __u32 def_period = 4096;
    __u32 period;
    long rc;

    print_box("Get/Set period test");
    printf("Trying to set the period = %u\n", ref_period);
    rc = ioctl(fd, LED_IOCTL_SET_PERIOD, &ref_period);
    if (rc) {
        printf("Error during the setting of new period value!\n");
        return RET_ERR;
    }

    printf("Trying to read the already set value back");
    rc = ioctl(fd, LED_IOCTL_GET_PERIOD, &period);
    if (rc) {
        printf("Error during the reading of current value ");
        return RET_ERR;
    }

    printf("Read period value: %u\n", period);
    if (period != ref_period) {
        printf("Invalid value has been received:\n");
        printf("\t* Expected: %u\n", ref_period);
        printf("\t* Received: %u\n", period);
        return RET_ERR;
    } else {
        printf("GET/SET is working!!\n");
    }

    printf("Setting the default period value...\n");
    rc = ioctl(fd, LED_IOCTL_SET_PERIOD, &def_period);
    if (rc) {
        printf("Unable to se the period value!\n");
        return RET_ERR;
    }

    printf("RGB test has been finished\n");

    return RET_OK;   
}

static void print_rgb(__u32 val) {
    __u8 r = val & 0xff;
    __u8 g = (val >> 8) & 0xff;
    __u8 b = (val >> 16) & 0xff;
    printf("(%d, %d, %d)", r, g, b);
}


static int test_ioctl_blink(int fd) {
    // Hex value is the encoded RGB 
    const __u32 rgb_test[] = {
        0xFF00FF, // PURPLE
        0xFF0000, // RED
        0x0000FF, // BLUE
        0x00FF00, // GREEN
    };

    print_box("RGB LED test");
    
    __u32 ret_val;
    int rc;

    const int tests = sizeof(rgb_test)/sizeof(__u32);
    for (int i = 0; i < tests; i++) {
        printf("Setting RGB = "); print_rgb(rgb_test[i]); printf("\n");

        rc = ioctl(fd, LED_IOCTL_SET_VAL , rgb_test + i);
        if (rc) {
            printf("Unable to set the color value!\n");
            return RET_ERR;
        }
        rc = ioctl(fd, LED_IOCTL_GET_VAL, &ret_val);
        if (rc) {
            printf("Unable to read RGB value!");
            return RET_ERR;
        }

        printf("Read RGB = "); print_rgb(ret_val); printf("\n");
        if (ret_val != rgb_test[i]) {
            printf("Set and read values are not same!");
            printf("Read: "); print_rgb(ret_val); printf("\n");
            printf("Expected: "); print_rgb(rgb_test[i]); printf("\n");
            return RET_ERR;
        }

        printf("Check the LED if color matches ...\n");
        wait_for_key_press();
    }

    printf("RGB test has been finished.\n");

    return RET_OK;
}

/* Standard test of the classic fwrite test */

static int  test_device_write(int fd) {
    const char* test_string  = "0xff 0x00 0x00\n";
    int test_size = strnlen(test_string, 32);

    int rc;
    // Setup helping variables to stream write
    const char* ptr = test_string;
    int to_write = test_size;
    print_box("Starting the standard write operation to cdev (writing RED color)\n");
    while (to_write > 0) {
        rc = write(fd, ptr, to_write);
        if (rc == 0) {
            printf("Unable to write data to device!\n");
            return RET_ERR;
        }

        // Move pointers to the right position in the array
        ptr         += rc;
        to_write    -= rc;
    }

    printf("Written string: %s\n", test_string);
    printf("Check if the color is RED ...\n");
    wait_for_key_press();

    return RET_OK;
}

int reset_device(int fd) {
    print_box("Device reset");
    int rc = ioctl(fd, LED_IOCTL_INIT, 0);
    if (rc) {
        printf("Error during the device reset!\n");
        return RET_ERR;
    }
    return RET_OK;
}

int main(int argc, char **argv) {
    /* Parse input arguments */
    int opt;
    const char* dev = NULL;

    while ((opt = getopt(argc, argv, "hd:" )) != -1) {
        switch (opt) {
            case 'h' : print_help(); break;
            case 'd' : dev = optarg; break;
            default:
                printf("Unknown option %s\n", optopt);
                return RET_ERR;
        }
    }

    if (dev == NULL) {
        printf("Device wasn't selected\n");
        return RET_ERR;
    }

    printf("Welcome the to the test utility for the LED module device driver.\n");
    printf("\t* Using the device: %s\n", dev);  
    
    int fd = open(dev, O_RDWR);
    if (!fd) {
        printf("Unable to open the device %s\n", dev);
        return RET_ERR;
    }
    CHECK_FUNC(test_ioctl_blink(fd), close(fd));
    CHECK_FUNC(test_ioctl_period(fd), close(fd));

    printf("So far so good, time to write something via the char driver.\n\n");
    CHECK_FUNC(test_device_write(fd), close(fd));

    printf("Everything done. Running the device reset.\n");
    CHECK_FUNC(reset_device(fd), close(fd));
    close(fd);
    fd = 0;

    return RET_OK;
}
