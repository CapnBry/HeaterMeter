/*
 * avrdude - A Downloader/Uploader for AVR device programmers
 * Copyright (C) 2002-2004 Brian S. Dean <bsd@bsdhome.com>
 * Copyright (C) 2008 Joerg Wunsch
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Code assembled from various avrdude sources by Bryan Mayland */

#ifndef fileio_h
#define fileio_h

// IHEX loader
int ihex2b(char * infile, FILE * inf,
           unsigned char * outbuf, int bufsize);

// Serial
int ser_open(char *port, long baud);
void ser_close(int fd);
int ser_send(int fd, unsigned char * buf, size_t buflen);
int ser_recv(int fd, unsigned char * buf, size_t buflen);
int ser_setspeed(int fd, long baud, int flushfirst);

// STK500 (Arduino/Optiboot)
#define Resp_STK_OK                0x10  // LF
#define Resp_STK_FAILED            0x11
#define Resp_STK_INSYNC            0x14
#define Resp_STK_NOSYNC            0x15

#define Sync_CRC_EOP               0x20  // ' '

#define Cmnd_STK_GET_SYNC          0x30  // '0'
#define Cmnd_STK_GET_PARAMETER     0x41  // 'A'
#define Cmnd_STK_LEAVE_PROGMODE    0x51  // 'Q'
#define Cmnd_STK_LOAD_ADDRESS      0x55  // 'U'
#define Cmnd_STK_PROG_PAGE         0x64  // 'd'
#define Cmnd_STK_READ_SIGN         0x75  // 'u'

#define Parm_STK_SW_MAJOR          0x81 
#define Parm_STK_SW_MINOR          0x82

#define stk500_send ser_send
#define stk500_recv ser_recv
#define stk500_drain ser_drain

int stk500_getsync(int fds);
int stk500_getparm(int fd, unsigned char parm, unsigned char * value);
int stk500_loadaddr(int fd, unsigned int addr);
int stk500_disable(int fd);

// Arduino
int arduino_read_sig_bytes(int fd, unsigned int *sig);

#endif
