/*  led-module.c - Example device driver for the LED device

* Copyright (C) 2020 Pavel Benacek
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
#define LED_IOCTL_MAGIC				'l'
#define LED_IOCTL_GET_INIT 			_IOR(LED_IOCTL_MAGIC, 0, int)
#define LED_IOCTL_SET_INIT			_IOW(LED_IOCTL_MAGIC, 1, int)
#define LED_IOCTL_GET_MASK			_IOR(LED_IOCTL_MAGIC, 2, int)
#define LED_IOCTL_SET_MASK			_IOW(LED_IOCTL_MAGIC, 3, int)
#define LED_IOCTL_SET_VALUE			_IOW(LED_IOCTL_MAGIC, 4, int)
#define LED_IOCTL_RESET				_IO(LED_IOCTL_MAGIC, 5)

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
    printf("Press any key to continue ...\n");
    getchar();
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

static int test_ioctl_init(int fd) {
    int rc;
    int ioctl_ret;
    const int ref_val = 0x2;

    print_box("Starting the IOCTL INIT test");
    printf("Trying to set the INIT value...\n");
    rc = ioctl(fd, LED_IOCTL_SET_INIT, ref_val);
    if (!rc) {
        printf("Unable to set the INIT value!\n");
        return RET_ERR;
    }
    printf("Init value write successfull ...\n");


    rc = ioctl(fd, LED_IOCTL_GET_INIT, &ioctl_ret);
    if (!rc) {
        printf("Unable to get the INIT value!\n");
        return RET_ERR;
    }
    printf("Init value received from the device ...");
    
    if (ioctl_ret != ref_val) {
        printf("Expected value (%x) and received (%x) are not same!", ioctl_ret, ref_val);
        return RET_ERR;
    }

    printf("Wow, IOCLT INIT is working!!!\n\n");
    return RET_OK;
}

static int test_ioctl_mask(int fd) {
    int rc;
    int ioctl_ret;
    const int ref_val = 0x3;

    print_box("Starting the IOCTL MASK test");
    printf("Trying to set the MASK value...\n");
    rc = ioctl(fd, LED_IOCTL_SET_MASK, ref_val);
    if (!rc) {
        printf("Unable to set the MASK value!\n");
        return RET_ERR;
    }
    printf("Mask value write successfull ...\n");


    rc = ioctl(fd, LED_IOCTL_GET_MASK, &ioctl_ret);
    if (!rc) {
        printf("Unable to get the MASK value!\n");
        return RET_ERR;
    }
    printf("Mask value received from the device ...");
    
    if (ioctl_ret != ref_val) {
        printf("Expected value (%x) and received (%x) are not same!", ioctl_ret, ref_val);
        return RET_ERR;
    }

    printf("Wow, IOCLT MASK is working!!!\n\n");
    return RET_OK;
}

static int test_ioctl_blink(int fd) {
    int rc;
    int idx;
    int ioctl_ret;

    int test_data [] = {2,1,3,4,5,0};
    const int test_data_count = sizeof(test_data) / sizeof(int);

    print_box("Starting the IOCTL set/reset test (watch the device :-))");
    for (idx = 0; idx < test_data_count; idx++) {
        printf("\t* Writing %x\n", test_data[idx]);
        rc = ioctl(fd, LED_IOCTL_SET_VALUE, test_data[idx]);
        if (!rc) {
            printf("Error during the IOCTL set operation!\n");
            return RET_ERR;
        }
        wait_for_key_press();
    }

    printf("Trying to reset the device ...\n");
    rc = ioctl(fd, LED_IOCTL_RESET);
    if (!rc) {
        printf("Unable to reset the device!\n");
        return RET_ERR;
    }

    return RET_OK;
}

/* Standard test of the classic fwrite test */

static int device_write_test(FILE* f) {
    const char* test_string  = "abc1";
    int test_size = strnlen(test_string, 5);

    int rc;
    // Setup helping variables to stream write
    const char* ptr = test_string;
    int to_write = test_size;
        
    print_box("Starting the standard write operation to cdev\n");
    while (to_write > 0) {
        rc = fwrite(ptr, sizeof(char), to_write, f);
        if (rc == 0) {
            printf("Unable to write data to device!\n");
            return RET_ERR;
        }

        // Move pointers to the right position in the array
        ptr         += rc;
        to_write    -= rc;
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

    printf("Welcome the to the test utility for the LED module device driver.\n");
    printf("\t* Using the device: %s\n", dev);  
    
    int fd = open(dev, O_RDWR);
    if (!fd) {
        printf("Unable to open the device %s\n", dev);
        return RET_ERR;
    }
    CHECK_FUNC(test_ioctl_init(fd), close(fd));  
    CHECK_FUNC(test_ioctl_mask(fd), close(fd));
    CHECK_FUNC(test_ioctl_blink(fd), close(fd));
    close(fd);
    fd = 0;

    wait_for_key_press();
    printf("So far so good, time to write something via the char driver.\n\n");
    FILE* f = fopen(dev, "w");
    if (!f) {
        printf("Unable to open device %s for writing\n.", dev);
        return RET_ERR;
    } 

    CHECK_FUNC(device_write_test, fclose(f));
    fclose(f);
    f = NULL;

    return RET_OK;
}
