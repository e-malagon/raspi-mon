/* MIT License                                                                        */
/*                                                                                    */
/* Copyright (c) 2024 Edgar Malagón                                                   */
/*                                                                                    */
/* Permission is hereby granted, free of charge, to any person obtaining a copy       */
/* of this software and associated documentation files (the "Software"), to deal      */
/* in the Software without restriction, including without limitation the rights       */
/* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell          */
/* copies of the Software, and to permit persons to whom the Software is              */
/* furnished to do so, subject to the following conditions:                           */
/*                                                                                    */
/* The above copyright notice and this permission notice shall be included in all     */
/* copies or substantial portions of the Software.                                    */
/*                                                                                    */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR         */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,           */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE        */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER             */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,      */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE      */
/* SOFTWARE.                                                                          */

/**************************************************************************************/
/* Raspberry PI 4 server monitoring                                                   */
/*                                                                                    */
/* This application was created to monitor a Linux server mounted on a Raspberry PI 4,*/
/* it was developed as a single file source code, only one dependency is required the */
/* libgpiod library to handle the gpio pin status. This application was tested on     */
/* Raspberry Pi OS version 12 (Bookworm).                                             */
/*                                                                                    */
/* There is no makefile, but you can compile this program with the following command  */
/* line:                                                                              */
/* gcc -O3 monitor.c -o monitor -lgpiod -lpthread                                     */
/*                                                                                    */
/**************************************************************************************/

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <gpiod.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <net/if.h>
#include <netinet/in.h>
#include <signal.h>
#include <sysexits.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

static const uint16_t font[][16] = {
    {0x0000, 0x0E00, 0x1B00, 0x3180, 0x3180, 0x1B00, 0x0E00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* ° */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /*   */
    {0x0000, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0000, 0x0C00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* ! */
    {0x0000, 0x0000, 0x0CC0, 0x0CC0, 0x0880, 0x0880, 0x0880, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* " */
    {0x0000, 0x0D80, 0x0D80, 0x0D80, 0x0D80, 0x3FC0, 0x1B00, 0x3FC0, 0x1B00, 0x1B00, 0x1B00, 0x1B00, 0x0000, 0x0000, 0x0000, 0x0000,}, /* # */
    {0x0400, 0x1F00, 0x3180, 0x3180, 0x3800, 0x1E00, 0x0F00, 0x0380, 0x3180, 0x3180, 0x1F00, 0x0400, 0x0400, 0x0000, 0x0000, 0x0000,}, /* $ */
    {0x0000, 0x1800, 0x2400, 0x2400, 0x18C0, 0x0780, 0x1E00, 0x3180, 0x0240, 0x0240, 0x0180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* % */
    {0x0000, 0x0000, 0x0F00, 0x1800, 0x1800, 0x1800, 0x0C00, 0x1D80, 0x3700, 0x3300, 0x1D80, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* & */
    {0x0000, 0x0000, 0x0300, 0x0300, 0x0200, 0x0200, 0x0200, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* ' */
    {0x0000, 0x0300, 0x0300, 0x0600, 0x0E00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0E00, 0x0600, 0x0300, 0x0300, 0x0000, 0x0000, 0x0000,}, /* ( */
    {0x0000, 0x1800, 0x1800, 0x0C00, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0C00, 0x1C00, 0x1800, 0x0000, 0x0000, 0x0000,}, /* ) */
    {0x0000, 0x0600, 0x0600, 0x3FC0, 0x3FC0, 0x0F00, 0x1F80, 0x1980, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* * */
    {0x0000, 0x0000, 0x0000, 0x0400, 0x0400, 0x0400, 0x3F80, 0x0400, 0x0400, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* + */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0600, 0x0400, 0x0C00, 0x0800, 0x0800, 0x0000, 0x0000,}, /* , */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x3F80, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* - */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* . */
    {0x00C0, 0x00C0, 0x0180, 0x0180, 0x0300, 0x0300, 0x0600, 0x0C00, 0x0C00, 0x1800, 0x1800, 0x3000, 0x3000, 0x0000, 0x0000, 0x0000,}, /* / */
    {0x0000, 0x0E00, 0x1B00, 0x3180, 0x3180, 0x3180, 0x3180, 0x3180, 0x3180, 0x1B00, 0x0E00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* 0 */
    {0x0000, 0x0600, 0x0E00, 0x1E00, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* 1 */
    {0x0000, 0x0F00, 0x1980, 0x3180, 0x3180, 0x0300, 0x0600, 0x0C00, 0x1800, 0x3000, 0x3F80, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* 2 */
    {0x0000, 0x1F00, 0x2180, 0x0180, 0x0300, 0x1F00, 0x0380, 0x0180, 0x0180, 0x2180, 0x1F00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* 3 */
    {0x0000, 0x0700, 0x0700, 0x0F00, 0x0B00, 0x1B00, 0x1300, 0x3300, 0x3F80, 0x0300, 0x0300, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* 4 */
    {0x0000, 0x1F80, 0x1800, 0x1800, 0x1800, 0x1F00, 0x0180, 0x0180, 0x0180, 0x2180, 0x1F00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* 5 */
    {0x0000, 0x0780, 0x1C00, 0x1800, 0x3000, 0x3700, 0x3980, 0x3180, 0x3180, 0x1980, 0x0F00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* 6 */
    {0x0000, 0x7F00, 0x0300, 0x0300, 0x0600, 0x0600, 0x0600, 0x0600, 0x0C00, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* 7 */
    {0x0000, 0x1F00, 0x3180, 0x3180, 0x3180, 0x1F00, 0x3180, 0x3180, 0x3180, 0x3180, 0x1F00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* 8 */
    {0x0000, 0x1E00, 0x3300, 0x3180, 0x3180, 0x3380, 0x1D80, 0x0180, 0x0300, 0x0700, 0x3C00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* 9 */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* : */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0300, 0x0300, 0x0000, 0x0000, 0x0000, 0x0600, 0x0400, 0x0800, 0x0800, 0x0000, 0x0000, 0x0000,}, /* ; */
    {0x0000, 0x0000, 0x0300, 0x0600, 0x0C00, 0x1800, 0x3000, 0x1800, 0x0C00, 0x0600, 0x0300, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* < */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x3F80, 0x0000, 0x3F80, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* = */
    {0x0000, 0x0000, 0x1800, 0x0C00, 0x0600, 0x0300, 0x0180, 0x0300, 0x0600, 0x0C00, 0x1800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* > */
    {0x0000, 0x0000, 0x1F00, 0x3180, 0x3180, 0x0180, 0x0700, 0x0C00, 0x0C00, 0x0000, 0x0C00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* ? */
    {0x0000, 0x0E00, 0x1100, 0x2100, 0x2100, 0x2700, 0x2900, 0x2900, 0x2700, 0x2000, 0x1100, 0x0E00, 0x0000, 0x0000, 0x0000, 0x0000,}, /* @ */
    {0x0000, 0x0000, 0x0F00, 0x0F00, 0x0900, 0x1980, 0x1980, 0x1F80, 0x30C0, 0x30C0, 0x70E0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* A */
    {0x0000, 0x0000, 0x3F00, 0x3180, 0x3180, 0x3180, 0x3F00, 0x3180, 0x3180, 0x3180, 0x3F00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* B */
    {0x0000, 0x0000, 0x1F00, 0x3080, 0x6040, 0x6000, 0x6000, 0x6000, 0x6040, 0x3080, 0x1F00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* C */
    {0x0000, 0x0000, 0x3F00, 0x3180, 0x30C0, 0x30C0, 0x30C0, 0x30C0, 0x30C0, 0x3180, 0x3F00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* D */
    {0x0000, 0x0000, 0x3F80, 0x3000, 0x3000, 0x3000, 0x3E00, 0x3000, 0x3000, 0x3000, 0x3F80, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* E */
    {0x0000, 0x0000, 0x3FC0, 0x3000, 0x3000, 0x3000, 0x3E00, 0x3000, 0x3000, 0x3000, 0x3000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* F */
    {0x0000, 0x0000, 0x1F00, 0x3080, 0x6000, 0x6000, 0x6000, 0x6780, 0x6180, 0x3180, 0x1F00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* G */
    {0x0000, 0x0000, 0x3180, 0x3180, 0x3180, 0x3180, 0x3F80, 0x3180, 0x3180, 0x3180, 0x3180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* H */
    {0x0000, 0x0000, 0x0F00, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0F00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* I */
    {0x0000, 0x0000, 0x0300, 0x0300, 0x0300, 0x0300, 0x0300, 0x6300, 0x6300, 0x6300, 0x3E00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* J */
    {0x0000, 0x0000, 0x30C0, 0x3180, 0x3300, 0x3600, 0x3C00, 0x3E00, 0x3300, 0x3180, 0x30C0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* K */
    {0x0000, 0x0000, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1F80, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* L */
    {0x0000, 0x0000, 0x60C0, 0x60C0, 0x71C0, 0x7BC0, 0x6AC0, 0x6EC0, 0x64C0, 0x60C0, 0x60C0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* M */
    {0x0000, 0x0000, 0x3180, 0x3180, 0x3980, 0x3D80, 0x3580, 0x3780, 0x3380, 0x3180, 0x3180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* N */
    {0x0000, 0x0000, 0x1F00, 0x3180, 0x60C0, 0x60C0, 0x60C0, 0x60C0, 0x60C0, 0x3180, 0x1F00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* O */
    {0x0000, 0x0000, 0x3F00, 0x3180, 0x3180, 0x3180, 0x3180, 0x3F00, 0x3000, 0x3000, 0x3000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* P */
    {0x0000, 0x0000, 0x1F00, 0x3180, 0x60C0, 0x60C0, 0x60C0, 0x64C0, 0x66C0, 0x3380, 0x1EC0, 0x0040, 0x0000, 0x0000, 0x0000, 0x0000,}, /* Q */
    {0x0000, 0x0000, 0x3F00, 0x3180, 0x3180, 0x3180, 0x3E00, 0x3300, 0x3180, 0x3180, 0x3180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* R */
    {0x0000, 0x0000, 0x1F00, 0x3180, 0x3180, 0x3800, 0x1F00, 0x0380, 0x3180, 0x3180, 0x1F00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* S */
    {0x0000, 0x0000, 0x7F80, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* T */
    {0x0000, 0x0000, 0x3180, 0x3180, 0x3180, 0x3180, 0x3180, 0x3180, 0x3180, 0x3180, 0x1F00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* U */
    {0x0000, 0x0000, 0x3180, 0x3180, 0x3180, 0x1B00, 0x1B00, 0x1B00, 0x0A00, 0x0E00, 0x0E00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* V */
    {0x0000, 0x0000, 0x60C0, 0x60C0, 0x64C0, 0x6EC0, 0x6EC0, 0x2A80, 0x3B80, 0x3B80, 0x3180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* W */
    {0x0000, 0x0000, 0x3180, 0x3180, 0x1B00, 0x0E00, 0x0E00, 0x0E00, 0x1B00, 0x3180, 0x3180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* X */
    {0x0000, 0x0000, 0x30C0, 0x30C0, 0x1980, 0x0F00, 0x0F00, 0x0600, 0x0600, 0x0600, 0x0600, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* Y */
    {0x0000, 0x0000, 0x3F80, 0x0180, 0x0300, 0x0600, 0x0400, 0x0C00, 0x1800, 0x3000, 0x3F80, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* Z */
    {0x0000, 0x0780, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0780, 0x0000, 0x0000, 0x0000,}, /* [ */
    {0x3000, 0x3000, 0x1800, 0x1800, 0x0C00, 0x0C00, 0x0600, 0x0300, 0x0300, 0x0180, 0x0180, 0x00C0, 0x00C0, 0x0000, 0x0000, 0x0000,}, /* \ */
    {0x0000, 0x1E00, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x1E00, 0x0000, 0x0000, 0x0000,}, /* ] */
    {0x0400, 0x0A00, 0x0A00, 0x1100, 0x2080, 0x2080, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* ^ */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xFFE0,}, /* _ */
    {0x0800, 0x0400, 0x0200, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* ` */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x1F00, 0x0180, 0x0180, 0x1F80, 0x3180, 0x3380, 0x1D80, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* a */
    {0x0000, 0x3000, 0x3000, 0x3000, 0x3700, 0x3980, 0x30C0, 0x30C0, 0x30C0, 0x3980, 0x3700, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* b */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x1F00, 0x3180, 0x6080, 0x6000, 0x6080, 0x3180, 0x1F00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* c */
    {0x0000, 0x0180, 0x0180, 0x0180, 0x1D80, 0x3380, 0x6180, 0x6180, 0x6180, 0x3380, 0x1D80, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* d */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x1F00, 0x3180, 0x60C0, 0x7FC0, 0x6000, 0x30C0, 0x1F80, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* e */
    {0x0000, 0x0780, 0x0C00, 0x0C00, 0x0C00, 0x1F80, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* f */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x1D80, 0x3380, 0x6180, 0x6180, 0x6180, 0x3380, 0x1D80, 0x0180, 0x0180, 0x1F00, 0x0000, 0x0000,}, /* g */
    {0x0000, 0x3000, 0x3000, 0x3000, 0x3700, 0x3980, 0x3180, 0x3180, 0x3180, 0x3180, 0x3180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* h */
    {0x0000, 0x0600, 0x0600, 0x0000, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* i */
    {0x0000, 0x0300, 0x0300, 0x0000, 0x0300, 0x0300, 0x0300, 0x0300, 0x0300, 0x0300, 0x0300, 0x0300, 0x0300, 0x1E00, 0x0000, 0x0000,}, /* j */
    {0x0000, 0x3000, 0x3000, 0x3000, 0x3700, 0x3600, 0x3C00, 0x3C00, 0x3600, 0x3300, 0x3180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* k */
    {0x0000, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* l */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x3F80, 0x36C0, 0x36C0, 0x36C0, 0x36C0, 0x36C0, 0x36C0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* m */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x3700, 0x3980, 0x3180, 0x3180, 0x3180, 0x3180, 0x3180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* n */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x1F00, 0x3180, 0x60C0, 0x60C0, 0x60C0, 0x3180, 0x1F00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* o */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x3700, 0x3980, 0x30C0, 0x30C0, 0x30C0, 0x3980, 0x3700, 0x3000, 0x3000, 0x3000, 0x0000, 0x0000,}, /* p */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x1D80, 0x3380, 0x6180, 0x6180, 0x6180, 0x3380, 0x1D80, 0x0180, 0x0180, 0x0180, 0x0000, 0x0000,}, /* q */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x1B80, 0x1CC0, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* r */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x1F00, 0x3180, 0x3C00, 0x1F00, 0x0380, 0x3180, 0x1F00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* s */
    {0x0000, 0x0600, 0x0600, 0x0600, 0x1F80, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0380, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* t */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x3180, 0x3180, 0x3180, 0x3180, 0x3180, 0x3380, 0x1D80, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* u */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x3180, 0x3180, 0x3180, 0x1B00, 0x1B00, 0x0E00, 0x0E00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* v */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x60C0, 0x60C0, 0x64C0, 0x6EC0, 0x3B80, 0x3B80, 0x3180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* w */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x3180, 0x1B00, 0x0E00, 0x0E00, 0x0E00, 0x1B00, 0x3180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* x */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x30C0, 0x30C0, 0x1980, 0x1980, 0x0B00, 0x0F00, 0x0600, 0x0600, 0x0C00, 0x1800, 0x0000, 0x0000,}, /* y */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x3F80, 0x0180, 0x0300, 0x0E00, 0x1800, 0x3000, 0x3F80, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* z */
    {0x0000, 0x0600, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x1800, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0600, 0x0000, 0x0000, 0x0000,}, /* { */
    {0x0000, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0000, 0x0000, 0x0000,}, /* | */
    {0x0000, 0x0C00, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0300, 0x0600, 0x0600, 0x0600, 0x0600, 0x0C00, 0x0000, 0x0000, 0x0000,}, /* } */
    {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1800, 0x2480, 0x0300, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,}, /* ~ */
};

#define ST7789_NOP              0x00    /* No Operation: NOP                                                                    */
#define ST7789_SWRESET          0x01    /* Software Reset: SWRESET                                                              */
#define ST7789_RDDID            0x04    /* Read Display ID: RDDID                                                               */
#define ST7789_RDDST            0x09    /* Read Display Status: RDDST                                                           */
#define ST7789_RDDPM            0x0A    /* Read Display Power Mode: RDDPM                                                       */
#define ST7789_RDDMADCTL        0x0B    /* Read Display MADCTL: RDDMADCTL                                                       */
#define ST7789_RDDCOLMOD        0x0C    /* Read Display Pixel Format: RDDCOLMOD                                                 */
#define ST7789_RDDIM            0x0D    /* Read Display Image Mode: RDDIM                                                       */
#define ST7789_RDDSM            0x0E    /* Read Display Signal Mod: RDDSM                                                       */
#define ST7789_RDDSDR           0x0F    /* Read Display Self-Diagnostic Result: RDDSDR                                          */
#define ST7789_SLPIN            0x10    /* Sleep in: SLPIN                                                                      */
#define ST7789_SLPOUT           0x11    /* Sleep Out: SLPOUT                                                                    */
#define ST7789_PTLON            0x12    /* Partial Display Mode On: PTLON                                                       */
#define ST7789_NORON            0x13    /* Normal Display Mode On: NORON                                                        */
#define ST7789_INVOFF           0x20    /* Display Inversion Off: INVOFF                                                        */
#define ST7789_INVON            0x21    /* Display Inversion On: INVON                                                          */
#define ST7789_GAMSET           0x26    /* Gamma Set: GAMSET                                                                    */
#define ST7789_DISPOFF          0x28    /* Display Off: DISPOFF                                                                 */
#define ST7789_DISPON           0x29    /* Display On: DISPON                                                                   */
#define ST7789_CASET            0x2A    /* Column Address Set: CASET                                                            */
#define ST7789_RASET            0x2B    /* Row Address Set: RASET                                                               */
#define ST7789_RAMWR            0x2C    /* Memory Write: RAMWR                                                                  */
#define ST7789_RAMRD            0x2E    /* Memory Read: RAMRD                                                                   */
#define ST7789_PTLAR            0x30    /* Partial Area: PTLAR                                                                  */
#define ST7789_VSCRDEF          0x33    /* Vertical Scrolling Definition: VSCRDEF                                               */
#define ST7789_TEOFF            0x34    /* Tearing Effect Line Off: TEOFF                                                       */
#define ST7789_TEON             0x35    /* Tearing Effect Line On: TEON                                                         */
#define ST7789_MADCTL           0x36    /* Memory Data Access Control: MADCTL                                                   */
#define ST7789_VSCSAD           0x37    /* Vertical Scroll Start Address of RAM: VSCSAD                                         */
#define ST7789_IDMOFF           0x38    /* Idle Mode Off: IDMOFF                                                                */
#define ST7789_IDMON            0x39    /* Idle Mode On: IDMON                                                                  */
#define ST7789_COLMOD           0x3A    /* Interface Pixel Format: COLMOD                                                       */
#define ST7789_WRMEMC           0x3C    /* Write Memory Continue: WRMEMC                                                        */
#define ST7789_RDMEMC           0x3E    /* Read Memory Continue: RDMEMC                                                         */
#define ST7789_STE              0x44    /* Set Tear Scanline: STE                                                               */
#define ST7789_GSCAN            0x45    /* Get Scanline: GSCAN                                                                  */
#define ST7789_WRDISBV          0x51    /* Write Display Brightness: WRDISBV                                                    */
#define ST7789_RDDISBV          0x52    /* Read Display Brightness Value: RDDISBV                                               */
#define ST7789_WRCTRLD          0x53    /* Write CTRL Display: WRCTRLD                                                          */
#define ST7789_RDCTRLD          0x54    /* Read CTRL Value Display: RDCTRLD                                                     */
#define ST7789_WRCACE           0x55    /* Write Content Adaptive Brightness Control and Color Enhancement: WRCACE              */
#define ST7789_RDCABC           0x56    /* Read Content Adaptive Brightness Control: RDCABC                                     */
#define ST7789_WRCABCMB         0x5E    /* Write CABC Minimum Brightnes: WRCABCMB                                               */
#define ST7789_RDCABCMB         0x5F    /* Read CABC Minimum Brightnes: RDCABCMB                                                */
#define ST7789_RDABCSDR         0x68    /* Read Automatic Brightness Control Self-Diagnostic Result: RDABCSDR                   */
#define ST7789_RDID1            0xDA    /* Read ID1: RDID1                                                                      */
#define ST7789_RDID2            0xDB    /* Read ID2: RDID2                                                                      */
#define ST7789_RDID3            0xDC    /* Read ID3: RDID3                                                                      */
#define ST7789_RAMCTRL          0xB0    /* RAM Control: RAMCTRL                                                                 */
#define ST7789_RGBCTRL          0xB1    /* RGB Interface Control: RGBCTRL                                                       */
#define ST7789_PORCTRL          0xB2    /* Porch Setting: PORCTRL                                                               */
#define ST7789_FRCTRL1          0xB3    /* Frame Rate Control 1 (In partial mode/ idle colors): FRCTRL1                         */
#define ST7789_PARCTRL          0xB5    /* Partial mode Control: PARCTRL                                                        */
#define ST7789_GCTRL            0xB7    /* Gate Control: GCTRL                                                                  */
#define ST7789_GTADJ            0xB8    /* Gate On Timing Adjustment: GTADJ                                                     */
#define ST7789_DGMEN            0xBA    /* Digital Gamma Enable: DGMEN                                                          */
#define ST7789_VCOMS            0xBB    /* VCOMS Setting: VCOMS                                                                 */
#define ST7789_POWSAVE          0xBC    /* Power Saving Mode: POWSAVE                                                           */
#define ST7789_DLPOFFSAVE       0xBD    /* Display off power save: DLPOFFSAVE                                                   */
#define ST7789_LCMCTRL          0xC0    /* LCM Control: LCMCTRL                                                                 */
#define ST7789_IDSET            0xC1    /* ID Code Setting: IDSET                                                               */
#define ST7789_VDVVRHEN         0xC2    /* VDV and VRH Command Enable: VDVVRHEN                                                 */
#define ST7789_VRHS             0xC3    /* VRH Set: VRHS                                                                        */
#define ST7789_VDVS             0xC4    /* VDV Set: VDVS                                                                        */
#define ST7789_VCMOFSET         0xC5    /* VCOMS Offset Set: VCMOFSET                                                           */
#define ST7789_FRCTRL2          0xC6    /* Frame Rate Control in Normal Mode: FRCTRL2                                           */
#define ST7789_CABCCTRL         0xC7    /* CABC Control:  CABCCTRL                                                              */
#define ST7789_REGSEL1          0xC8    /* Register Value Selection 1: REGSEL1                                                  */
#define ST7789_REGSEL2          0xCA    /* Register Value Selection 2: REGSEL2                                                  */
#define ST7789_PWMFRSEL         0xCC    /* PWM Frequency Selection: PWMFRSEL                                                    */
#define ST7789_PWCTRL1          0xD0    /* Power Control 1: PWCTRL1                                                             */
#define ST7789_VAPVANEN         0xD2    /* Enable VAP/VAN signal output: VAPVANEN                                               */
#define ST7789_CMD2EN           0xDF    /* Command 2 Enable: CMD2EN                                                             */
#define ST7789_PVGAMCTRL        0xE0    /* Positive Voltage Gamma Control: PVGAMCTRL                                            */
#define ST7789_NVGAMCTRL        0xE1    /* Negative Voltage Gamma Control: NVGAMCTRL                                            */
#define ST7789_DGMLUTR          0xE2    /* Digital Gamma Look-up Table for Red: DGMLUTR                                         */
#define ST7789_DGMLUTB          0xE3    /* Digital Gamma Look-up Table for Blue: DGMLUTB                                        */
#define ST7789_GATECTRL         0xE4    /* Gate Control: GATECTRL                                                               */
#define ST7789_SPI2EN           0xE7    /* SPI2 Enable: SPI2EN                                                                  */
#define ST7789_PWCTRL2          0xE8    /* Power Control 2: PWCTRL2                                                             */
#define ST7789_EQCTRL           0xE9    /* Equalize time control: EQCTRL                                                        */
#define ST7789_PROMCTRL         0xEC    /* Program Mode Control: PROMCTRL                                                       */
#define ST7789_PROMEN           0xFA    /* Program Mode Enable: PROMEN                                                          */
#define ST7789_NVMSET           0xFC    /* NVM Setting: NVMSET                                                                  */
#define ST7789_PROMACT          0xFE    /* Program action: PROMACT                                                              */
#define ST7789_MADCTL_MY        0x80    /* Page Address Order D7=(0: Top to Bottom, 1: Bottom to Top)                           */
#define ST7789_MADCTL_MX        0x40    /* Column Address Order D6=(0: Left to Right, 1: Right to Left)                         */
#define ST7789_MADCTL_MV        0x20    /* Page/Column Order D5=(0: Normal Mode, D5=1: Reverse Mode)                            */
#define ST7789_MADCTL_ML        0x10    /* Line Address Order D4=(0: LCD Refresh Top to Bottom, 1: Bottom to Top)               */
#define ST7789_MADCTL_RGB       0x08    /* RGB/BGR Order D3=(0: RGB, 1: BGR)                                                    */
#define ST7789_MADCTL_MH        0x04    /* Display Data Latch Data Order D2=(0:  LCD Refresh Left to Right, 1: Right to Left)   */
#define ST7789_PORTRAIT         ST7789_MADCTL_MY | ST7789_MADCTL_MX
#define ST7789_PORTRAIT_ROT180  0x00
#define ST7789_LANDSCAPE        ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_ML
#define ST7789_LANDSCAPE_ROT180 ST7789_MADCTL_MY | ST7789_MADCTL_MV

#define FONT_WIDTH              11
#define FONT_HEIGHT             16
#define SCREEN_WIDTH            320
#define SCREEN_HEIGHT           240

#define MAX_CHARS_IN_LINE       SCREEN_WIDTH / FONT_WIDTH //29

#define NAME_DATA_Y1            16

#define TIME_DATA_LENGHT        19
#define TIME_DATA_WIDTH         FONT_WIDTH * TIME_DATA_LENGHT
#define TIME_DATA_X1            58
#define TIME_DATA_X2            TIME_DATA_X1 + TIME_DATA_WIDTH
#define TIME_DATA_Y1            38
#define TIME_DATA_Y2            TIME_DATA_Y1 + FONT_HEIGHT

#define NET1_LABEL_X            22
#define NET1_LABEL_Y            69
#define NET1_DATA_X             82
#define NET1_DATA_Y             91
#define NET2_LABEL_X            22
#define NET2_LABEL_Y            113
#define NET2_DATA_X             82
#define NET2_DATA_Y             135
#define NET_LABEL_RX_X          170
#define NET_LABEL_TX_X          276
#define NET_DATA_LENGHT         4
#define NET_DATA_WIDTH          FONT_WIDTH * NET_DATA_LENGHT

#define IFDEV1_RX_DATA_X1       126
#define IFDEV1_RX_DATA_X2       IFDEV1_RX_DATA_X1 + NET_DATA_WIDTH
#define IFDEV1_RX_DATA_Y1       NET1_LABEL_Y
#define IFDEV1_RX_DATA_Y2       IFDEV1_RX_DATA_Y1 + FONT_HEIGHT

#define IFDEV1_TX_DATA_X1       232
#define IFDEV1_TX_DATA_X2       IFDEV1_TX_DATA_X1 + NET_DATA_WIDTH
#define IFDEV1_TX_DATA_Y1       NET1_LABEL_Y
#define IFDEV1_TX_DATA_Y2       IFDEV1_TX_DATA_Y1 + FONT_HEIGHT

#define IFDEV2_RX_DATA_X1       126
#define IFDEV2_RX_DATA_X2       IFDEV2_RX_DATA_X1 + NET_DATA_WIDTH
#define IFDEV2_RX_DATA_Y1       NET2_LABEL_Y
#define IFDEV2_RX_DATA_Y2       IFDEV2_RX_DATA_Y1 + FONT_HEIGHT

#define IFDEV2_TX_DATA_X1       232
#define IFDEV2_TX_DATA_X2       IFDEV2_TX_DATA_X1 + NET_DATA_WIDTH
#define IFDEV2_TX_DATA_Y1       NET2_LABEL_Y
#define IFDEV2_TX_DATA_Y2       IFDEV2_TX_DATA_Y1 + FONT_HEIGHT

#define CPU_LABEL_X1            22
#define CPU_DATA_LENGHT         4
#define CPU_DATA_WIDTH          FONT_WIDTH * CPU_DATA_LENGHT
#define CPU_DATA_X1             65
#define CPU_DATA_X2             CPU_DATA_X1 + CPU_DATA_WIDTH
#define CPU_DATA_Y1             166
#define CPU_DATA_Y2             CPU_DATA_Y1 + FONT_HEIGHT

#define RAM_LABEL_X1            22
#define RAM_DATA_LENGHT         4
#define RAM_DATA_WIDTH          FONT_WIDTH * RAM_DATA_LENGHT
#define RAM_DATA_X1             65
#define RAM_DATA_X2             RAM_DATA_X1 + RAM_DATA_WIDTH
#define RAM_DATA_Y1             188
#define RAM_DATA_Y2             RAM_DATA_Y1 + FONT_HEIGHT

#define TEMP_LABEL_X1           22
#define TEMP_FIXED_X1           99
#define TEMP_DATA_LENGHT        2
#define TEMP_DATA_WIDTH         FONT_WIDTH * TEMP_DATA_LENGHT
#define TEMP_DATA_X1            76
#define TEMP_DATA_X2            TEMP_DATA_X1 + TEMP_DATA_WIDTH
#define TEMP_DATA_Y1            210
#define TEMP_DATA_Y2            TEMP_DATA_Y1 + FONT_HEIGHT

#define UPT_LABEL_X1            140
#define UPT_DATA_LENGHT         10
#define UPT_DATA_WIDTH          FONT_WIDTH * UPT_DATA_LENGHT
#define UPT_DATA_X1             188
#define UPT_DATA_X2             UPT_DATA_X1 + UPT_DATA_WIDTH
#define UPT_DATA_Y1             166
#define UPT_DATA_Y2             UPT_DATA_Y1 + FONT_HEIGHT

#define FS1_LABEL_X1            140
#define FS1_FIXED_X1            187
#define FS1_DATA_LENGHT         4
#define FS1_DATA_WIDTH          FONT_WIDTH * FS1_DATA_LENGHT
#define FS1_DATA_X1             254
#define FS1_DATA_X2             FS1_DATA_X1 + FS1_DATA_WIDTH
#define FS1_DATA_Y1             188
#define FS1_DATA_Y2             FS1_DATA_Y1 + FONT_HEIGHT

#define FS2_LABEL_X1            140
#define FS2_FIXED_X1            187
#define FS2_DATA_LENGHT         4
#define FS2_DATA_WIDTH          FONT_WIDTH * FS2_DATA_LENGHT
#define FS2_DATA_X1             254
#define FS2_DATA_X2             FS2_DATA_X1 + FS2_DATA_WIDTH
#define FS2_DATA_Y1             210
#define FS2_DATA_Y2             FS2_DATA_Y1 + FONT_HEIGHT

#define SQUARE1_X               10
#define SQUARE1_Y               11
#define SQUARE1_W               300
#define SQUARE1_H               46

#define SQUARE2_X               10
#define SQUARE2_Y               64
#define SQUARE2_W               300
#define SQUARE2_H               90

#define SQUARE3_X               10
#define SQUARE3_Y               161
#define SQUARE3_W               111
#define SQUARE3_H               68

#define SQUARE4_X               128
#define SQUARE4_Y               161
#define SQUARE4_W               182
#define SQUARE4_H               68

const char* chipname = "gpiochip0";
struct gpiod_chip* gpio_chip;
struct gpiod_line* user_button_pin;
struct gpiod_line* st7789_backlight_pin;
struct gpiod_line* st7789_reset_pin;
struct gpiod_line* st7789_data_pin;
long rx_ifdev1_bytes = 0;
long tx_ifdev1_bytes = 0;
long rx_ifdev2_bytes = 0;
long tx_ifdev2_bytes = 0;
bool service_running = true;
bool update_screen = true;
bool check_sda = false;
bool check_sdb = false;
bool check_ifdev1 = true;
bool check_ifdev2 = false;
time_t last_time;
int spidev_fd;

unsigned int update_fs_time = 300;
unsigned int sleep_after = 3600;
unsigned int user_button_pin_id = 20;
unsigned int st7789_backlight_pin_id = 18;
unsigned int st7789_reset_pin_id = 27;
unsigned int st7789_data_pin_id = 25;
char log_file[255] = "raspi-mon.log";
char spi_device[255] = "/dev/spidev0.0";
char ifdev1_id[255] = "eth0";
char ifdev2_id[255] = "wlan0";
char fs1_id[255] = "/";
char fs2_id[255] = "";
uint16_t data_text_color_code = 0xffff;
uint16_t fixed_text_color_code = 0x1ca5;
uint16_t label_text_color_code = 0x5fce;
uint16_t window_color_code = 0x8e11;
uint16_t background_color_code = 0x0d00;

void signals_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM)
        service_running = false;
}

void write_error(char* msg) {
    char time_string[20];
    time_t current_time;
    struct tm* local_time;
    FILE* filePointer;

    if (0 < (current_time = time(NULL))) {
        local_time = localtime(&current_time);
        strftime(time_string, sizeof(time_string), "%F %T", local_time);
        if ((filePointer = fopen(log_file, "a")) != NULL) {
            fprintf(filePointer, "%s ERROR %s - %s\n", time_string, msg, errno != 0 ? strerror(errno) : "");
            fclose(filePointer);
        }
    }
}

void* user_button_read_thread(void* vargp) {
    int readed_status;
    struct timespec ts;

    while (service_running) {
        readed_status = gpiod_line_get_value(user_button_pin);
        if (readed_status == 0) {
            timespec_get(&ts, TIME_UTC);
            last_time = ts.tv_sec;
            update_screen = true;
            gpiod_line_set_value(st7789_backlight_pin, 1);
        }
        usleep(100000);
    }

    return NULL;
}

int gpio_open(void) {
    if ((gpio_chip = gpiod_chip_open_by_name(chipname)) == NULL) {
        write_error("Failed to open gpiochip0");
        return -1;
    }
    if ((user_button_pin = gpiod_chip_get_line(gpio_chip, user_button_pin_id)) == NULL || (gpiod_line_request_input(user_button_pin, "monitor")) < 0) {
        write_error("Failed to request user button pin");
        return -1;
    }
    if ((st7789_backlight_pin = gpiod_chip_get_line(gpio_chip, st7789_backlight_pin_id)) == NULL || (gpiod_line_request_output(st7789_backlight_pin, "monitor", 1)) < 0) {
        write_error("Failed to request backlight pin");
        return -1;
    }
    if ((st7789_reset_pin = gpiod_chip_get_line(gpio_chip, st7789_reset_pin_id)) == NULL || (gpiod_line_request_output(st7789_reset_pin, "monitor", 1)) < 0) {
        write_error("Failed to request reset pin");
        return -1;
    }
    if ((st7789_data_pin = gpiod_chip_get_line(gpio_chip, st7789_data_pin_id)) == NULL || (gpiod_line_request_output(st7789_data_pin, "monitor", 1)) < 0) {
        write_error("Failed to request data pin");
        return -1;
    }
    return 0;
}

void gpio_close(void) {
    gpiod_line_release(st7789_data_pin);
    gpiod_line_release(st7789_reset_pin);
    gpiod_line_release(st7789_backlight_pin);
    gpiod_line_release(user_button_pin);
    gpiod_chip_close(gpio_chip);
}

int spi_transfer(const uint8_t* data, const uint32_t data_size) {
    struct spi_ioc_transfer transfer = {
        .tx_buf = (unsigned long)data,
        .rx_buf = (unsigned long)NULL,
        .len = data_size,
        .delay_usecs = 0,
        .speed_hz = 32000000,
        .bits_per_word = 8,
    };

    if (ioctl(spidev_fd, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        write_error("Failed to perform SPI transfer");
        return -1;
    }
    return 0;
}

int spi_write_register(const uint8_t instruction, const uint8_t* data, const uint32_t data_size) {
    if (gpiod_line_set_value(st7789_data_pin, 0) < 0) {
        write_error("Failed to reset data pin");
        return -1;
    }
    if (spi_transfer(&instruction, 1) < 0)
        return -1;
    if (gpiod_line_set_value(st7789_data_pin, 1) < 0) {
        write_error("Failed to set data pin");
        return -1;
    }
    if (data != NULL && data_size != 0 && spi_transfer(data, data_size) < 0)
        return -1;
    return 0;
}

int lcd_screen_open(void) {
    uint8_t st7789_colmod[] = { 0x05, };
    uint8_t st7789_porctrl[] = { 0x0C, 0x0C, 0x00, 0x33, 0x33, };
    uint8_t st7789_gctrl[] = { 0x35, };
    uint8_t st7789_vcoms[] = { 0x19, };
    uint8_t st7789_lcmctrl[] = { 0x2C, };
    uint8_t st7789_vdvvrhen[] = { 0x01, 0xFF, };
    uint8_t st7789_vrhs[] = { 0x12, };
    uint8_t st7789_vdvs[] = { 0x20, };
    uint8_t st7789_frctrl2[] = { 0x0F, };
    uint8_t st7789_pwctrl1[] = { 0xA4, 0xA1, };
    uint8_t st7789_pvgamctrl[] = { 0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23, };
    uint8_t st7789_nvgamctrl[] = { 0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23, };
    uint8_t st7789_invon[] = { 0x0E, };
    uint8_t st7789_dispon[] = { 0x00, };
    uint8_t st7789_madctl[] = { ST7789_LANDSCAPE_ROT180, };
    unsigned wr_max_speed = 32000000;
    char wr_mode = SPI_MODE_0;
    char bits_per_word = 8;
    int result = 0;

    if ((spidev_fd = open(spi_device, O_RDWR)) < 0) {
        write_error("Failed to open spi device");
        return -1;
    }

    if (ioctl(spidev_fd, SPI_IOC_WR_MODE, &wr_mode) < 0) {
        write_error("Failed to set mode for spi device");
        return -1;
    }

    if (ioctl(spidev_fd, SPI_IOC_WR_BITS_PER_WORD, &bits_per_word) < 0) {
        write_error("Failed to set bits per word for spi device");
        return -1;
    }

    if (ioctl(spidev_fd, SPI_IOC_WR_MAX_SPEED_HZ, &wr_max_speed) < 0) {
        write_error("Failed to set masx speed for spi device");
        return -1;
    }

    if (ioctl(spidev_fd, SPI_IOC_WR_MAX_SPEED_HZ, &wr_max_speed) < 0) {
        write_error("Failed to set masx speed for spi device");
        return -1;
    }

    if (gpiod_line_set_value(st7789_reset_pin, 0) < 0) {
        write_error("Failed to reset the screen");
        return -1;
    }
    usleep(120000);
    if (gpiod_line_set_value(st7789_reset_pin, 1) < 0) {
        write_error("Failed to reset the screen");
        return -1;
    }
    usleep(120000);

    result += spi_write_register(ST7789_SLPOUT, NULL, 0);
    result += spi_write_register(ST7789_COLMOD, st7789_colmod, sizeof(st7789_colmod));
    result += spi_write_register(ST7789_PORCTRL, st7789_porctrl, sizeof(st7789_porctrl));
    result += spi_write_register(ST7789_GCTRL, st7789_gctrl, sizeof(st7789_gctrl));
    result += spi_write_register(ST7789_VCOMS, st7789_vcoms, sizeof(st7789_vcoms));
    result += spi_write_register(ST7789_LCMCTRL, st7789_lcmctrl, sizeof(st7789_lcmctrl));
    result += spi_write_register(ST7789_VDVVRHEN, st7789_vdvvrhen, sizeof(st7789_vdvvrhen));
    result += spi_write_register(ST7789_VRHS, st7789_vrhs, sizeof(st7789_vrhs));
    result += spi_write_register(ST7789_VDVS, st7789_vdvs, sizeof(st7789_vdvs));
    result += spi_write_register(ST7789_FRCTRL2, st7789_frctrl2, sizeof(st7789_frctrl2));
    result += spi_write_register(ST7789_PWCTRL1, st7789_pwctrl1, sizeof(st7789_pwctrl1));
    result += spi_write_register(ST7789_PVGAMCTRL, st7789_pvgamctrl, sizeof(st7789_pvgamctrl));
    result += spi_write_register(ST7789_NVGAMCTRL, st7789_nvgamctrl, sizeof(st7789_nvgamctrl));
    result += spi_write_register(ST7789_INVON, st7789_invon, sizeof(st7789_invon));
    result += spi_write_register(ST7789_DISPON, st7789_dispon, sizeof(st7789_dispon));
    result += spi_write_register(ST7789_MADCTL, st7789_madctl, sizeof(st7789_madctl));

    return result;
}

void lcd_screen_close(void) {
    close(spidev_fd);
}

int write_text_to_display(uint16_t buffer[], uint16_t data_size, char* text, uint8_t text_lenght, uint16_t text_color, uint16_t window_color, uint8_t* caset, uint8_t* raset) {
    uint16_t char_row;
    int result = 0;

    for (uint8_t i = 0; i < text_lenght; i++) {
        for (uint16_t j = 0; j < FONT_HEIGHT; j++) {
            char_row = font[text[i] - 31][j];
            for (uint16_t k = 0; k < FONT_WIDTH && ((i * FONT_WIDTH) + k) < data_size; k++)
                buffer[(j * text_lenght * FONT_WIDTH) + ((i * FONT_WIDTH) + k)] = (char_row << k) & 0x8000 ? text_color : window_color;
        }
    }

    result += spi_write_register(ST7789_CASET, caset, 4);
    result += spi_write_register(ST7789_RASET, raset, 4);
    result += spi_write_register(ST7789_RAMWR, NULL, 0);
    for (uint16_t index = 0; index < (data_size * 2); index += 4096)
        result += spi_transfer(((uint8_t*)(buffer)) + index, (data_size * 2) < (index + 4096) ? (data_size * 2) - index : 4096);
    return result;
}

int display_time_info(uint16_t text_color, uint16_t window_color) {
    static uint16_t buffer[FONT_HEIGHT * TIME_DATA_WIDTH];
    static uint8_t caset[] = { TIME_DATA_X1 >> 8, TIME_DATA_X1 & 0xff, TIME_DATA_X2 >> 8, (TIME_DATA_X2 - 1) & 0xff, };
    static uint8_t raset[] = { TIME_DATA_Y1 >> 8, TIME_DATA_Y1 & 0xff, TIME_DATA_Y2 >> 8, (TIME_DATA_Y2 - 1) & 0xff, };
    char time_string[20];
    time_t current_time;
    struct tm* local_time;
    int result = 0;

    if (0 < (current_time = time(NULL))) {
        local_time = localtime(&current_time);
        strftime(time_string, sizeof(time_string), "%F %T", local_time);
        result + write_text_to_display(buffer, FONT_HEIGHT * TIME_DATA_WIDTH, time_string, TIME_DATA_LENGHT, text_color, window_color, caset, raset);
    }

    return result;
}

int display_ifdev1_rx_info(long rx_bytes_diff, uint16_t text_color, uint16_t window_color) {
    static uint16_t buffer[FONT_HEIGHT * NET_DATA_WIDTH];
    static uint8_t caset[] = { IFDEV1_RX_DATA_X1 >> 8, IFDEV1_RX_DATA_X1 & 0xff, IFDEV1_RX_DATA_X2 >> 8, (IFDEV1_RX_DATA_X2 - 1) & 0xff, };
    static uint8_t raset[] = { IFDEV1_RX_DATA_Y1 >> 8, IFDEV1_RX_DATA_Y1 & 0xff, IFDEV1_RX_DATA_Y2 >> 8, (IFDEV1_RX_DATA_Y2 - 1) & 0xff, };
    char net_data_string[10];
    char units = 'B';
    int result = 0;

    if (999999999 < rx_bytes_diff) {
        rx_bytes_diff /= 1024 * 1024 * 1024;
        units = 'M';
    }
    else if (999999 < rx_bytes_diff) {
        rx_bytes_diff /= 1024 * 1024;
        units = 'M';
    }
    else if (999 < rx_bytes_diff) {
        rx_bytes_diff /= 1024;
        units = 'K';
    }

    sprintf(net_data_string, "%3ld%c", rx_bytes_diff, units);
    result + write_text_to_display(buffer, FONT_HEIGHT * NET_DATA_WIDTH, net_data_string, NET_DATA_LENGHT, text_color, window_color, caset, raset);

    return result;
}

int display_ifdev1_tx_info(long tx_bytes_diff, uint16_t text_color, uint16_t window_color) {
    static uint16_t buffer[FONT_HEIGHT * NET_DATA_WIDTH];
    static uint8_t caset[] = { IFDEV1_TX_DATA_X1 >> 8, IFDEV1_TX_DATA_X1 & 0xff, IFDEV1_TX_DATA_X2 >> 8, (IFDEV1_TX_DATA_X2 - 1) & 0xff, };
    static uint8_t raset[] = { IFDEV1_TX_DATA_Y1 >> 8, IFDEV1_TX_DATA_Y1 & 0xff, IFDEV1_TX_DATA_Y2 >> 8, (IFDEV1_TX_DATA_Y2 - 1) & 0xff, };
    char net_data_string[10];
    char units = 'B';
    int result = 0;

    if (999999999 < tx_bytes_diff) {
        tx_bytes_diff /= 1024 * 1024 * 1024;
        units = 'M';
    }
    else if (999999 < tx_bytes_diff) {
        tx_bytes_diff /= 1024 * 1024;
        units = 'M';
    }
    else if (999 < tx_bytes_diff) {
        tx_bytes_diff /= 1024;
        units = 'K';
    }

    sprintf(net_data_string, "%3ld%c", tx_bytes_diff, units);
    result + write_text_to_display(buffer, FONT_HEIGHT * NET_DATA_WIDTH, net_data_string, NET_DATA_LENGHT, text_color, window_color, caset, raset);

    return result;
}

int display_ifdev2_rx_info(long rx_bytes_diff, uint16_t text_color, uint16_t window_color) {
    static uint16_t buffer[FONT_HEIGHT * NET_DATA_WIDTH];
    static uint8_t caset[] = { IFDEV2_RX_DATA_X1 >> 8, IFDEV2_RX_DATA_X1 & 0xff, IFDEV2_RX_DATA_X2 >> 8, (IFDEV2_RX_DATA_X2 - 1) & 0xff, };
    static uint8_t raset[] = { IFDEV2_RX_DATA_Y1 >> 8, IFDEV2_RX_DATA_Y1 & 0xff, IFDEV2_RX_DATA_Y2 >> 8, (IFDEV2_RX_DATA_Y2 - 1) & 0xff, };
    char net_data_string[10];
    char units = 'B';
    int result = 0;

    if (999999999 < rx_bytes_diff) {
        rx_bytes_diff /= 1024 * 1024 * 1024;
        units = 'M';
    }
    else if (999999 < rx_bytes_diff) {
        rx_bytes_diff /= 1024 * 1024;
        units = 'M';
    }
    else if (999 < rx_bytes_diff) {
        rx_bytes_diff /= 1024;
        units = 'K';
    }

    sprintf(net_data_string, "%3ld%c", rx_bytes_diff, units);
    result + write_text_to_display(buffer, FONT_HEIGHT * NET_DATA_WIDTH, net_data_string, NET_DATA_LENGHT, text_color, window_color, caset, raset);

    return result;
}

int display_ifdev2_tx_info(long tx_bytes_diff, uint16_t text_color, uint16_t window_color) {
    static uint16_t buffer[FONT_HEIGHT * NET_DATA_WIDTH];
    static uint8_t caset[] = { IFDEV2_TX_DATA_X1 >> 8, IFDEV2_TX_DATA_X1 & 0xff, IFDEV2_TX_DATA_X2 >> 8, (IFDEV2_TX_DATA_X2 - 1) & 0xff, };
    static uint8_t raset[] = { IFDEV2_TX_DATA_Y1 >> 8, IFDEV2_TX_DATA_Y1 & 0xff, IFDEV2_TX_DATA_Y2 >> 8, (IFDEV2_TX_DATA_Y2 - 1) & 0xff, };
    char net_data_string[10];
    char units = 'B';
    int result = 0;

    if (999999999 < tx_bytes_diff) {
        tx_bytes_diff /= 1024 * 1024 * 1024;
        units = 'M';
    }
    else if (999999 < tx_bytes_diff) {
        tx_bytes_diff /= 1024 * 1024;
        units = 'M';
    }
    else if (999 < tx_bytes_diff) {
        tx_bytes_diff /= 1024;
        units = 'K';
    }

    sprintf(net_data_string, "%3ld%c", tx_bytes_diff, units);
    result + write_text_to_display(buffer, FONT_HEIGHT * NET_DATA_WIDTH, net_data_string, NET_DATA_LENGHT, text_color, window_color, caset, raset);

    return result;
}

char* skip_text(char* string_ptr, uint8_t n) {
    for (uint8_t i = 0; i < n; i++) {
        while (*string_ptr && *string_ptr != ' ')
            string_ptr++;
        while (*string_ptr && *string_ptr == ' ')
            string_ptr++;
    }
    return string_ptr;
}

int display_net_info(char* ifdev1, char* ifdev2, uint16_t text_color, uint16_t window_color) {
    char net_data_string[300];
    long rx_bytes;
    long tx_bytes;
    long rx_bytes_diff;
    long tx_bytes_diff;
    char* string_pointer;
    FILE* filePointer;
    int result = 0;

    if ((filePointer = fopen("/proc/net/dev", "r")) != NULL) {
        while (fgets(net_data_string, 299, filePointer) != NULL) {
            string_pointer = skip_text(net_data_string, 1);
            if (strncmp(ifdev1, string_pointer, strlen(ifdev1)) == 0) {
                string_pointer = skip_text(string_pointer, 1);
                rx_bytes = atol(string_pointer);
                rx_bytes_diff = rx_bytes - rx_ifdev1_bytes;
                rx_ifdev1_bytes = rx_bytes;
                result += display_ifdev1_rx_info(rx_bytes_diff, text_color, window_color);
                string_pointer = skip_text(string_pointer, 8);
                tx_bytes = atol(string_pointer);
                tx_bytes_diff = tx_bytes - tx_ifdev1_bytes;
                tx_ifdev1_bytes = tx_bytes;
                result += display_ifdev1_tx_info(tx_bytes_diff, text_color, window_color);
            }
            else if (strncmp(ifdev2, string_pointer, strlen(ifdev2)) == 0) {
                string_pointer = skip_text(string_pointer, 1);
                rx_bytes = atol(string_pointer);
                rx_bytes_diff = rx_bytes - rx_ifdev2_bytes;
                rx_ifdev2_bytes = rx_bytes;
                result += display_ifdev2_rx_info(rx_bytes_diff, text_color, window_color);
                string_pointer = skip_text(string_pointer, 8);
                tx_bytes = atol(string_pointer);
                tx_bytes_diff = tx_bytes - tx_ifdev2_bytes;
                tx_ifdev2_bytes = tx_bytes;
                result += display_ifdev2_tx_info(tx_bytes_diff, text_color, window_color);
            }
        }
        fclose(filePointer);
    }

    return result;
}

int display_cpu_info(uint16_t text_color, uint16_t window_color) {
    static uint16_t buffer[FONT_HEIGHT * CPU_DATA_WIDTH];
    static uint8_t caset[] = { CPU_DATA_X1 >> 8, CPU_DATA_X1 & 0xff, CPU_DATA_X2 >> 8, (CPU_DATA_X2 - 1) & 0xff, };
    static uint8_t raset[] = { CPU_DATA_Y1 >> 8, CPU_DATA_Y1 & 0xff, CPU_DATA_Y2 >> 8, (CPU_DATA_Y2 - 1) & 0xff, };
    char cpu_avg_string[10];
    char cpu_string[10];
    FILE* filePointer;
    int result = 0;

    if ((filePointer = fopen("/proc/loadavg", "r")) != NULL) {
        if (fgets(cpu_avg_string, 9, filePointer) != NULL) {
            sprintf(cpu_string, "%3d%%", (int)(atof(cpu_avg_string) * 100 / 4));
            result + write_text_to_display(buffer, FONT_HEIGHT * CPU_DATA_WIDTH, cpu_string, CPU_DATA_LENGHT, text_color, window_color, caset, raset);
        }
        fclose(filePointer);
    }

    return result;
}

int display_ram_info(uint16_t text_color, uint16_t window_color) {
    static uint16_t buffer[FONT_HEIGHT * RAM_DATA_WIDTH];
    static uint8_t caset[] = { RAM_DATA_X1 >> 8, RAM_DATA_X1 & 0xff, RAM_DATA_X2 >> 8, (RAM_DATA_X2 - 1) & 0xff, };
    static uint8_t raset[] = { RAM_DATA_Y1 >> 8, RAM_DATA_Y1 & 0xff, RAM_DATA_Y2 >> 8, (RAM_DATA_Y2 - 1) & 0xff, };
    char total_ram_string[30];
    char free_ram_string[30];
    char ram_string[10];
    FILE* filePointer;
    int result = 0;

    if ((filePointer = fopen("/proc/meminfo", "r")) != NULL) {
        if (fgets(total_ram_string, 30, filePointer) != NULL && fgets(free_ram_string, 30, filePointer) != NULL && fgets(free_ram_string, 30, filePointer) != NULL) {
            char* total_ram = total_ram_string;
            char* free_ram = free_ram_string;
            while (*total_ram) {
                if (isdigit(*total_ram))
                    break;
                total_ram++;
            }
            while (*free_ram) {
                if (isdigit(*free_ram))
                    break;
                free_ram++;
            }
            sprintf(ram_string, "%3d%%", (int)(100 - (atol(free_ram) * 100 / atol(total_ram))));
            result + write_text_to_display(buffer, FONT_HEIGHT * RAM_DATA_WIDTH, ram_string, RAM_DATA_LENGHT, text_color, window_color, caset, raset);
        }
        fclose(filePointer);
    }

    return result;
}

int display_temp_info(uint16_t text_color, uint16_t window_color) {
    static uint16_t buffer[FONT_HEIGHT * TEMP_DATA_WIDTH];
    static uint8_t caset[] = { TEMP_DATA_X1 >> 8, TEMP_DATA_X1 & 0xff, TEMP_DATA_X2 >> 8, (TEMP_DATA_X2 - 1) & 0xff, };
    static uint8_t raset[] = { TEMP_DATA_Y1 >> 8, TEMP_DATA_Y1 & 0xff, TEMP_DATA_Y2 >> 8, (TEMP_DATA_Y2 - 1) & 0xff, };
    char temp_string[10];
    FILE* filePointer;
    int result = 0;

    if ((filePointer = fopen("/sys/class/thermal/thermal_zone0/temp", "r")) != NULL) {
        if (fgets(temp_string, 9, filePointer) != NULL)
            result + write_text_to_display(buffer, FONT_HEIGHT * TEMP_DATA_WIDTH, temp_string, TEMP_DATA_LENGHT, text_color, window_color, caset, raset);
        fclose(filePointer);
    }

    return result;
}

int display_uptime_info(uint16_t text_color, uint16_t window_color) {
    static uint16_t buffer[FONT_HEIGHT * UPT_DATA_WIDTH];
    static uint8_t caset[] = { UPT_DATA_X1 >> 8, UPT_DATA_X1 & 0xff, UPT_DATA_X2 >> 8, (UPT_DATA_X2 - 1) & 0xff, };
    static uint8_t raset[] = { UPT_DATA_Y1 >> 8, UPT_DATA_Y1 & 0xff, UPT_DATA_Y2 >> 8, (UPT_DATA_Y2 - 1) & 0xff, };
    char uptime_data_string[30];
    char uptime_string[11];
    FILE* filePointer;
    time_t uptime;
    int result = 0;

    if ((filePointer = fopen("/proc/uptime", "r")) != NULL) {
        if (fgets(uptime_data_string, 30, filePointer) != NULL) {
            uptime = atol(uptime_data_string);
            int d = (uptime / (24 * 3600));
            uptime %= (24 * 3600);
            int h = (uptime / 3600);
            uptime %= 3600;
            if (0 < d)
                sprintf(uptime_string, "%3d:%02d:%02dD", d, h, (uptime / 60));
            else
                sprintf(uptime_string, " %02d:%02d:%02dH", h, (uptime / 60), (uptime % 60));
            result + write_text_to_display(buffer, FONT_HEIGHT * UPT_DATA_WIDTH, uptime_string, UPT_DATA_LENGHT, text_color, window_color, caset, raset);
        }
        fclose(filePointer);
    }

    return result;
}

int display_fs1_info(char* fs1, uint16_t text_color, uint16_t window_color) {
    static uint16_t buffer[FONT_HEIGHT * FS1_DATA_WIDTH];
    static uint8_t caset[] = { FS1_DATA_X1 >> 8, FS1_DATA_X1 & 0xff, FS1_DATA_X2 >> 8, (FS1_DATA_X2 - 1) & 0xff, };
    static uint8_t raset[] = { FS1_DATA_Y1 >> 8, FS1_DATA_Y1 & 0xff, FS1_DATA_Y2 >> 8, (FS1_DATA_Y2 - 1) & 0xff, };
    struct statvfs stat;
    char fs1_string[10];
    int result = 0;

    if (statvfs(fs1, &stat) == 0) {
        sprintf(fs1_string, "%3d%%", ((stat.f_blocks - stat.f_bfree) * 100 / stat.f_blocks));
        result + write_text_to_display(buffer, FONT_HEIGHT * FS1_DATA_WIDTH, fs1_string, FS1_DATA_LENGHT, text_color, window_color, caset, raset);
    }

    return result;
}

int display_fs2_info(char* fs2, uint16_t text_color, uint16_t window_color) {
    static uint16_t buffer[FONT_HEIGHT * FS2_DATA_WIDTH];
    static uint8_t caset[] = { FS2_DATA_X1 >> 8, FS2_DATA_X1 & 0xff, FS2_DATA_X2 >> 8, (FS2_DATA_X2 - 1) & 0xff, };
    static uint8_t raset[] = { FS2_DATA_Y1 >> 8, FS2_DATA_Y1 & 0xff, FS2_DATA_Y2 >> 8, (FS2_DATA_Y2 - 1) & 0xff, };
    struct statvfs stat;
    char fs2_string[10];
    int result = 0;

    if (statvfs(fs2, &stat) == 0) {
        sprintf(fs2_string, "%3d%%", ((stat.f_blocks - stat.f_bfree) * 100 / stat.f_blocks));
        result + write_text_to_display(buffer, FONT_HEIGHT * FS2_DATA_WIDTH, fs2_string, FS2_DATA_LENGHT, text_color, window_color, caset, raset);
    }

    return result;
}

int warm_net_info(char* ifdev1, char* ifdev2) {
    char net_data_string[300];
    char* string_pointer;
    FILE* filePointer;
    int result = 0;

    if ((filePointer = fopen("/proc/net/dev", "r")) != NULL) {
        while (fgets(net_data_string, 299, filePointer) != NULL) {
            string_pointer = skip_text(net_data_string, 1);
            if (strncmp(ifdev1, string_pointer, strlen(ifdev1)) == 0) {
                string_pointer = skip_text(string_pointer, 1);
                rx_ifdev1_bytes = atol(string_pointer);
                string_pointer = skip_text(string_pointer, 8);
                tx_ifdev1_bytes = atol(string_pointer);
            }
            else if (strncmp(ifdev2, string_pointer, strlen(ifdev2)) == 0) {
                string_pointer = skip_text(string_pointer, 1);
                rx_ifdev2_bytes = atol(string_pointer);
                string_pointer = skip_text(string_pointer, 8);
                tx_ifdev2_bytes = atol(string_pointer);
            }
        }
        fclose(filePointer);
    }

    return result;
}

void buffer_write_string(uint16_t buffer[][SCREEN_WIDTH], uint16_t x, uint16_t y, char* string_ptr, uint16_t text_color, uint16_t window_color) {
    while (*string_ptr) {
        for (uint16_t i = 0; i < FONT_HEIGHT && (y + i) < SCREEN_HEIGHT; i++) {
            uint16_t char_row = font[(*string_ptr) - 31][i];
            for (uint16_t j = 0; j < FONT_WIDTH && (x + j) < SCREEN_WIDTH; j++)
                buffer[y + i][x + j] = (char_row << j) & 0x8000 ? text_color : window_color;
        }
        x += FONT_WIDTH;
        string_ptr++;
    }
}

void buffer_write_h_line(uint16_t buffer[][SCREEN_WIDTH], uint16_t x1, uint16_t x2, uint16_t y, uint16_t color) {
    for (int i = x1; i < x2;i++)
        buffer[y][i] = color;
}

void buffer_write_rectangle(uint16_t buffer[][SCREEN_WIDTH], uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    buffer_write_h_line(buffer, x + 4, x + w - 4, y, color);
    buffer_write_h_line(buffer, x + 3, x + w - 3, y + 1, color);
    buffer_write_h_line(buffer, x + 2, x + w - 2, y + 2, color);
    buffer_write_h_line(buffer, x + 1, x + w - 1, y + 3, color);
    for (int j = y + 4; j < y + h - 4; j++)
        buffer_write_h_line(buffer, x, x + w, j, color);
    buffer_write_h_line(buffer, x + 1, x + w - 1, y + h - 4, color);
    buffer_write_h_line(buffer, x + 2, x + w - 2, y + h - 3, color);
    buffer_write_h_line(buffer, x + 3, x + w - 3, y + h - 2, color);
    buffer_write_h_line(buffer, x + 4, x + w - 4, y + h - 1, color);
}

int flush_buffer(uint16_t buffer[][SCREEN_WIDTH]) {
    static uint8_t caset[] = { 0x00, 0x00, SCREEN_WIDTH >> 8, (SCREEN_WIDTH - 1) & 0xff, };
    static uint8_t raset[] = { 0x00, 0x00, SCREEN_HEIGHT >> 8, (SCREEN_HEIGHT - 1) & 0xff, };
    int result = 0;

    result += spi_write_register(ST7789_CASET, caset, 4);
    result += spi_write_register(ST7789_RASET, raset, 4);
    result += spi_write_register(ST7789_RAMWR, NULL, 0);
    for (uint16_t index = 0; index < 40; index++)
        result += spi_transfer((uint8_t*)(buffer)+(index * 1920 * 2), 1920 * 2);

    return result;
}

int display_fixed_info(char* ifdev1, char* ifdev2, char* fs1, char* fs2, uint16_t data_text_color, uint16_t fixed_text_color, uint16_t label_text_color, uint16_t window_color, uint16_t background_color) {
    uint16_t buffer[SCREEN_HEIGHT][SCREEN_WIDTH];
    char* not_ready = "Device not ready";
    char* waiting = "Waiting...";
    bool ifdev1_ready = false;
    bool ifdev2_ready = false;
    struct ifaddrs* ptr_ifaddrs;
    struct ifaddrs* ptr_entry;
    struct statvfs stat;
    char data_string[30];
    uint8_t retry = 0;
    int result = 0;

    for (uint16_t i = 0; i < SCREEN_HEIGHT; i++)
        for (uint16_t j = 0; j < SCREEN_WIDTH; j++)
            buffer[i][j] = background_color;

    buffer_write_rectangle(buffer, SQUARE1_X, SQUARE1_Y, SQUARE1_W, SQUARE1_H, window_color);
    buffer_write_rectangle(buffer, SQUARE2_X, SQUARE2_Y, SQUARE2_W, SQUARE2_H, window_color);
    buffer_write_rectangle(buffer, SQUARE3_X, SQUARE3_Y, SQUARE3_W, SQUARE3_H, window_color);
    buffer_write_rectangle(buffer, SQUARE4_X, SQUARE4_Y, SQUARE4_W, SQUARE4_H, window_color);

    if (gethostname(data_string, 30) == 0)
        buffer_write_string(buffer, (FONT_WIDTH * MAX_CHARS_IN_LINE / 2) - (FONT_WIDTH * strlen(data_string) / 2), NAME_DATA_Y1, data_string, fixed_text_color, window_color);

    if (check_ifdev1) {
        buffer_write_string(buffer, NET1_LABEL_X, NET1_LABEL_Y, ifdev1, label_text_color, window_color);
        buffer_write_string(buffer, (FONT_WIDTH * MAX_CHARS_IN_LINE / 2) - (FONT_WIDTH * strlen(waiting) / 2), NET1_DATA_Y, waiting, fixed_text_color, window_color);
        buffer_write_string(buffer, NET_LABEL_RX_X, NET1_LABEL_Y, "RX", label_text_color, window_color);
        buffer_write_string(buffer, NET_LABEL_TX_X, NET1_LABEL_Y, "TX", label_text_color, window_color);
    }

    if (check_ifdev2) {
        buffer_write_string(buffer, NET2_LABEL_X, NET2_LABEL_Y, ifdev2, label_text_color, window_color);
        buffer_write_string(buffer, (FONT_WIDTH * MAX_CHARS_IN_LINE / 2) - (FONT_WIDTH * strlen(waiting) / 2), NET2_DATA_Y, waiting, fixed_text_color, window_color);
        buffer_write_string(buffer, NET_LABEL_RX_X, NET2_LABEL_Y, "RX", label_text_color, window_color);
        buffer_write_string(buffer, NET_LABEL_TX_X, NET2_LABEL_Y, "TX", label_text_color, window_color);
    }

    buffer_write_string(buffer, CPU_LABEL_X1, CPU_DATA_Y1, "CPU", label_text_color, window_color);
    buffer_write_string(buffer, RAM_LABEL_X1, RAM_DATA_Y1, "RAM", label_text_color, window_color);
    buffer_write_string(buffer, TEMP_LABEL_X1, TEMP_DATA_Y1, "Temp", label_text_color, window_color);
    buffer_write_string(buffer, TEMP_FIXED_X1, TEMP_DATA_Y1, "\x1f", data_text_color, window_color);
    buffer_write_string(buffer, UPT_LABEL_X1, UPT_DATA_Y1, "UpT", label_text_color, window_color);
    buffer_write_string(buffer, FS1_LABEL_X1, FS1_DATA_Y1, "FS1", label_text_color, window_color);
    buffer_write_string(buffer, FS2_LABEL_X1, FS2_DATA_Y1, "FS2", label_text_color, window_color);

    if (statvfs(fs1, &stat) == 0) {
        double fs_size = (double)(stat.f_blocks * stat.f_frsize) / (1024.0 * 1024.0);
        char size_label = 'M';
        if (100 < fs_size) {
            fs_size /= 1024.0;
            size_label = 'G';
        }
        if (100 < fs_size) {
            fs_size /= 1024.0;
            size_label = 'T';
        }
        sprintf(data_string, "%3.1f%c", fs_size, size_label);
        buffer_write_string(buffer, FS1_FIXED_X1, FS1_DATA_Y1, data_string, fixed_text_color, window_color);
        check_sda = true;
    }
    else
        buffer_write_string(buffer, FS1_FIXED_X1, FS1_DATA_Y1, "N/A", fixed_text_color, window_color);

    if (statvfs(fs2, &stat) == 0) {
        double fs_size = (double)(stat.f_blocks * stat.f_frsize) / (1024.0 * 1024.0);
        char size_label = 'M';
        if (100 < fs_size) {
            fs_size /= 1024.0;
            size_label = 'G';
        }
        if (100 < fs_size) {
            fs_size /= 1024.0;
            size_label = 'T';
        }
        sprintf(data_string, "%3.1f%c", fs_size, size_label);
        buffer_write_string(buffer, FS2_FIXED_X1, FS2_DATA_Y1, data_string, fixed_text_color, window_color);
        check_sdb = true;
    }
    else
        buffer_write_string(buffer, FS2_FIXED_X1, FS2_DATA_Y1, "N/A", fixed_text_color, window_color);

    result += flush_buffer(buffer);

    while (retry < 120) {
        sleep(1);
        if (getifaddrs(&ptr_ifaddrs) == 0) {
            ptr_entry = ptr_ifaddrs;
            while (ptr_entry != NULL) {
                if (ptr_entry->ifa_addr->sa_family == AF_INET) {
                    if (strcmp(ptr_entry->ifa_name, ifdev1) == 0) {
                        if (ptr_entry->ifa_addr != NULL) {
                            char ip_address[INET_ADDRSTRLEN] = { 0, };
                            inet_ntop(ptr_entry->ifa_addr->sa_family, &((struct sockaddr_in*)(ptr_entry->ifa_addr))->sin_addr, ip_address, INET_ADDRSTRLEN);
                            buffer_write_string(buffer, (FONT_WIDTH * MAX_CHARS_IN_LINE / 2) - (FONT_WIDTH * strlen(ip_address) / 2), NET1_DATA_Y, ip_address, fixed_text_color, window_color);
                            ifdev1_ready = true;
                        }
                        else
                            buffer_write_string(buffer, NET1_DATA_X, NET1_DATA_Y, "IP Address N/A", fixed_text_color, window_color);
                    }

                    if (strcmp(ptr_entry->ifa_name, ifdev2) == 0) {
                        if (ptr_entry->ifa_addr != NULL) {
                            char ip_address[INET_ADDRSTRLEN] = { 0, };
                            inet_ntop(ptr_entry->ifa_addr->sa_family, &((struct sockaddr_in*)(ptr_entry->ifa_addr))->sin_addr, ip_address, INET_ADDRSTRLEN);
                            buffer_write_string(buffer, (FONT_WIDTH * MAX_CHARS_IN_LINE / 2) - (FONT_WIDTH * strlen(ip_address) / 2), NET2_DATA_Y, ip_address, fixed_text_color, window_color);
                            ifdev2_ready = true;
                        }
                        else
                            buffer_write_string(buffer, NET2_DATA_X, NET2_DATA_Y, "IP Address N/A", fixed_text_color, window_color);
                    }
                }
                ptr_entry = ptr_entry->ifa_next;
            }
            if ((check_ifdev1 && ifdev1_ready && check_ifdev2 && ifdev2_ready) || (!check_ifdev1 && check_ifdev2 && ifdev2_ready) || (check_ifdev1 && ifdev1_ready && !check_ifdev2))
                break;
            else {
                display_time_info(data_text_color, window_color);
                display_net_info(ifdev1, ifdev2, data_text_color, window_color);
                display_cpu_info(data_text_color, window_color);
                display_ram_info(data_text_color, window_color);
                display_temp_info(data_text_color, window_color);
                display_uptime_info(data_text_color, window_color);
                display_fs1_info(fs1, data_text_color, window_color);
                display_fs2_info(fs2, data_text_color, window_color);
            }
        }
        else
            write_error("Unable to get the network interfaces");
        retry++;
    }

    if (check_ifdev1 && !ifdev1_ready)
        buffer_write_string(buffer, (FONT_WIDTH * MAX_CHARS_IN_LINE / 2) - (FONT_WIDTH * strlen(not_ready) / 2), NET1_DATA_Y, not_ready, fixed_text_color, window_color);

    if (check_ifdev2 && !ifdev2_ready)
        buffer_write_string(buffer, (FONT_WIDTH * MAX_CHARS_IN_LINE / 2) - (FONT_WIDTH * strlen(not_ready) / 2), NET2_DATA_Y, not_ready, fixed_text_color, window_color);

    result += flush_buffer(buffer);
    return result;
}

void update_status(char* ifdev1, char* ifdev2, char* fs1, char* fs2, uint16_t data_text_color, uint16_t fixed_text_color, uint16_t label_text_color, uint16_t window_color, uint16_t background_color) {
    struct timespec ts;
    time_t current_time, previouse_time = 0;
    long nanoseconds, diff = 500;

    timespec_get(&ts, TIME_UTC);
    last_time = current_time = ts.tv_sec;
    nanoseconds = ts.tv_nsec;
    warm_net_info(ifdev1, ifdev2);
    display_fixed_info(ifdev1, ifdev2, fs1, fs2, data_text_color, fixed_text_color, label_text_color, window_color, background_color);
    while (service_running) {
        if (update_screen) {
            display_time_info(data_text_color, window_color);
            display_net_info(ifdev1, ifdev2, data_text_color, window_color);
            display_cpu_info(data_text_color, window_color);
            display_ram_info(data_text_color, window_color);
            display_temp_info(data_text_color, window_color);
            display_uptime_info(data_text_color, window_color);
            if (update_fs_time < (current_time - previouse_time)) {
                if (check_sda)
                    display_fs1_info(fs1, data_text_color, window_color);
                if (check_sdb)
                    display_fs2_info(fs2, data_text_color, window_color);
                previouse_time = current_time;
            }
            if (sleep_after < (current_time - last_time)) {
                update_screen = false;
                gpiod_line_set_value(st7789_backlight_pin, 0);
            }
        }
        timespec_get(&ts, TIME_UTC);
        diff = (abs((ts.tv_nsec - nanoseconds) / 1000) + diff) / 2;
        current_time = ts.tv_sec;
        nanoseconds = ts.tv_nsec;
        usleep(1000000 - diff);
    }
}

void load_config(char* config_file_path) {
    char config_string[300];
    FILE* filePointer;
    if ((filePointer = fopen(config_file_path, "r")) != NULL) {
        check_ifdev1 = false;
        check_ifdev2 = false;
        while (fgets(config_string, 299, filePointer) != NULL) {
            if (sscanf(config_string, "spi_device = %s", spi_device) == 1) {
                continue;
            }
            if (sscanf(config_string, "user_button_pin = %u", &user_button_pin_id) == 1) {
                continue;
            }
            if (sscanf(config_string, "backlight_pin_id = %u", &st7789_backlight_pin_id) == 1) {
                continue;
            }
            if (sscanf(config_string, "reset_pin_id = %u", &st7789_reset_pin_id) == 1) {
                continue;
            }
            if (sscanf(config_string, "data_pin_id = %u", &st7789_data_pin_id) == 1) {
                continue;
            }
            if (sscanf(config_string, "net_device1 = %s", ifdev1_id) == 1) {
                check_ifdev1 = true;
                continue;
            }
            if (sscanf(config_string, "net_device2 = %s", ifdev2_id) == 1) {
                check_ifdev2 = true;
                continue;
            }
            if (sscanf(config_string, "filesystem1 = %s", fs1_id) == 1) {
                continue;
            }
            if (sscanf(config_string, "filesystem2 = %s", fs2_id) == 1) {
                continue;
            }
            if (sscanf(config_string, "colors = %4hx %4hx %4hx %4hx %4hx", &data_text_color_code, &fixed_text_color_code, &label_text_color_code, &window_color_code, &background_color_code) == 5) {
                continue;
            }
            if (sscanf(config_string, "update_fs_time = %u", &update_fs_time) == 1) {
                continue;
            }
            if (sscanf(config_string, "sleep_after = %u", &sleep_after) == 1) {
                continue;
            }
            if (sscanf(config_string, "log_file = %s", log_file) == 1) {
                continue;
            }
        }
        fclose(filePointer);
    }
}

int main(int argc, char* argv[]) {
    pthread_t user_button_read_thread_id;
    signal(SIGINT, signals_handler);
    signal(SIGTERM, signals_handler);

    if (1 < argc)
        load_config(argv[1]);

    if (gpio_open() == 0) {
        if (lcd_screen_open() == 0) {
            if (pthread_create(&user_button_read_thread_id, NULL, user_button_read_thread, NULL) == 0)
                pthread_detach(user_button_read_thread_id);
            update_status(ifdev1_id, ifdev2_id, fs1_id, fs2_id, data_text_color_code, fixed_text_color_code, label_text_color_code, window_color_code, background_color_code);
            lcd_screen_close();
        }
        gpio_close();
    }

    return 0;
}