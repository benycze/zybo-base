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
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

/* Declare IOCTL handlers */
#define SW_IOCTL_MAGIC			'l'
#define SW_IOCTL_GET_MASK		_IOR(SW_IOCTL_MAGIC, 0, int)
#define SW_IOCTL_SET_MASK		_IOW(SW_IOCTL_MAGIC, 1, int)
#define SW_IOCTL_GET_VALUE		_IOW(SW_IOCTL_MAGIC, 2, int)

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


/**
 * @brief Detection of interupt signal
 * 
 */
int sig_int = 0;

/**
 * @brief Signal handler method for callback handling
 * 
 * @param s Signal value
 */
void sig_handler(int s){
    sig_int = 1;
}

static void print_help() {
    printf("Tool for testing of the SWITCH module driver.\n");
    printf("\n\n");
    printf("\t-h = prints this help\n");
    printf("\t-d = device to open\n");
    return;
}

static void print_box(const char* msg) {
    printf("=====================================================\n");
    printf("%s\n", msg);
    printf("=====================================================\n");
    return;
}


static int ioctl_test_mask(int fd) {
    print_box("Starting the IOCTL mask test");
    int rc;

    const int mask_test_val = 0x2;
    int mask_val;

    printf("Writing the mask value 0x%x\n", mask_test_val);
    rc = ioctl(fd, SW_IOCTL_SET_MASK, mask_test_val);
    if(rc) {
        printf("Unable to set the MASK value!\n");
        return RET_ERR;
    }

    printf("Trying to read mask value ...\n");
    rc = ioctl(fd, SW_IOCTL_GET_MASK, &mask_val);
    if(rc) {
        printf("Unable to read the mask value!\n");
        return RET_ERR;
    }

    if (mask_val != mask_test_val) {
        printf("Reference and read mask values are not same!\n");
        printf("\t* Reference = 0x%x\n", mask_test_val);
        printf("\t* Read value = 0x%x\n", mask_val);
        return RET_ERR;
    }

    rc = ioctl(fd, SW_IOCTL_SET_MASK, 0xf);
    if(rc) {
        printf("Unable to reset the MASK value!\n");
        return RET_ERR;
    }

    printf("Everything seems fine :-)\n");
    return RET_OK;
}

static int ioctl_loop_read(int fd) {
    print_box("Starting the loop read\n.");
    printf("* Press the CTRL + C if you want to end.\n");
    int rc;
    int sw_val;

    // Register signal handler and run the loop
    signal(SIGINT, sig_handler);
    while(sig_int == 0) {
        rc = ioctl(fd, SW_IOCTL_GET_VALUE, &sw_val);
        if(rc) {
            printf("Unable to read the current switch value!\n");
            signal(SIGINT, SIG_DFL);
            return RET_ERR;
        }

        printf("Current value: 0x%x\n", sw_val);
        sleep(1);
    }
    // Unregister & end
    printf("Loop read has been finished.\n");
    signal(SIGINT, SIG_DFL);
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

    printf("Switch test utility is staring.\n");
    printf("\t* Using the device: %s\n", dev);  
 
    int fd = open(dev, O_RDWR);
    if (!fd) {
        printf("Unable to open the device %s\n", dev);
        return RET_ERR;
    }
    CHECK_FUNC(ioctl_test_mask(fd), close(fd));
    CHECK_FUNC(ioctl_loop_read(fd), close(fd));
    close(fd);
    fd = 0;

    return 0;
}
