/*  led-module.c - Example device driver for the LED device

* Copyright (C) 2013 - 2016 Pavel Benacek
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

/* Declare IOCTL handlers */
#define LED_IOCTL_MAGIC				'l'
#define LED_IOCTL_GET_INIT 			_IOR(LED_IOCTL_MAGIC, 0, int)
#define LED_IOCTL_SET_INIT			_IOW(LED_IOCTL_MAGIC, 1, int)
#define LED_IOCTL_GET_MASK			_IOR(LED_IOCTL_MAGIC, 2, int)
#define LED_IOCTL_SET_MASK			_IOW(LED_IOCTL_MAGIC, 3, int)
#define LED_IOCTL_RESET				_IO(LED_IOCTL_MAGIC, 4)

int main(int argc, char **argv)
{
    printf("Hello World!\n");

    return 0;
}
