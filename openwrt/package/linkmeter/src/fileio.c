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

/* $Id: fileio.c 816 2009-03-22 21:28:46Z joerg_wunsch $ */

/* $Id: ser_posix.c 826 2009-07-02 10:31:13Z joerg_wunsch $ */

/*
 * Posix serial interface for avrdude.
 */

/* $Id: stk500.c 804 2009-02-23 22:04:57Z joerg_wunsch $ */

/*
 * avrdude interface for Atmel STK500 programmer
 *
 * Note: most commands use the "universal command" feature of the
 * programmer in a "pass through" mode, exceptions are "program
 * enable", "paged read", and "paged write".
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "hmdude.h"
#include "fileio.h"

#define IHEX_MAXDATA 128
#define MAX_LINE_LEN 128  /* max line length for ASCII format input files */

struct ihexrec {
  unsigned char    reclen;
  unsigned int     loadofs;
  unsigned char    rectyp;
  unsigned char    data[IHEX_MAXDATA];
  unsigned char    cksum;
};

static int ihex_readrec(struct ihexrec * ihex, char * rec)
{
  int i, j;
  char buf[8];
  int offset, len;
  char * e;
  unsigned char cksum;
  int rc;

  len    = strlen(rec);
  offset = 1;
  cksum  = 0;

  /* reclen */
  if (offset + 2 > len)
    return -1;
  for (i=0; i<2; i++)
    buf[i] = rec[offset++];
  buf[i] = 0;
  ihex->reclen = strtoul(buf, &e, 16);
  if (e == buf || *e != 0)
    return -1;

  /* load offset */
  if (offset + 4 > len)
    return -1;
  for (i=0; i<4; i++)
    buf[i] = rec[offset++];
  buf[i] = 0;
  ihex->loadofs = strtoul(buf, &e, 16);
  if (e == buf || *e != 0)
    return -1;

  /* record type */
  if (offset + 2 > len)
    return -1;
  for (i=0; i<2; i++)
    buf[i] = rec[offset++];
  buf[i] = 0;
  ihex->rectyp = strtoul(buf, &e, 16);
  if (e == buf || *e != 0)
    return -1;

  cksum = ihex->reclen + ((ihex->loadofs >> 8) & 0x0ff) +
    (ihex->loadofs & 0x0ff) + ihex->rectyp;

  /* data */
  for (j=0; j<ihex->reclen; j++) {
    if (offset + 2 > len)
      return -1;
    for (i=0; i<2; i++)
      buf[i] = rec[offset++];
    buf[i] = 0;
    ihex->data[j] = strtoul(buf, &e, 16);
    if (e == buf || *e != 0)
      return -1;
    cksum += ihex->data[j];
  }

  /* cksum */
  if (offset + 2 > len)
    return -1;
  for (i=0; i<2; i++)
    buf[i] = rec[offset++];
  buf[i] = 0;
  ihex->cksum = strtoul(buf, &e, 16);
  if (e == buf || *e != 0)
    return -1;

  rc = -cksum & 0x000000ff;

  return rc;
}

int ihex2b(char * infile, FILE * inf,
             unsigned char * outbuf, int bufsize)
{
  char buffer [ MAX_LINE_LEN ];
  unsigned char * buf;
  unsigned int nextaddr, baseaddr, maxaddr;
  int i;
  int lineno;
  int len;
  struct ihexrec ihex;
  int rc;

  lineno   = 0;
  buf      = outbuf;
  baseaddr = 0;
  maxaddr  = 0;

  while (fgets((char *)buffer,MAX_LINE_LEN,inf)!=NULL) {
    lineno++;
    len = strlen(buffer);
    if (buffer[len-1] == '\n')
      buffer[--len] = 0;
    if (buffer[0] != ':')
      continue;
    rc = ihex_readrec(&ihex, buffer);
    if (rc < 0) {
      fprintf(stderr, "%s: invalid record at line %d of \"%s\"\n",
              progname, lineno, infile);
      return -1;
    }
    else if (rc != ihex.cksum) {
      fprintf(stderr, "%s: ERROR: checksum mismatch at line %d of \"%s\"\n",
              progname, lineno, infile);
      fprintf(stderr, "%s: checksum=0x%02x, computed checksum=0x%02x\n",
              progname, ihex.cksum, rc);
      return -1;
    }

    switch (ihex.rectyp) {

      case 0: /* data record */
        nextaddr = ihex.loadofs + baseaddr;
        if (nextaddr + ihex.reclen > bufsize) {
          fprintf(stderr,
                  "%s: ERROR: address 0x%04x out of range at line %d of %s\n",
                  progname, nextaddr+ihex.reclen, lineno, infile);
          return -1;
        }
        for (i=0; i<ihex.reclen; i++) {
          buf[nextaddr+i] = ihex.data[i];
        }
        if (nextaddr+ihex.reclen > maxaddr)
          maxaddr = nextaddr+ihex.reclen;
        break;

      case 1: /* end of file record */
        return maxaddr;
        break;

      case 2: /* extended segment address record */
        baseaddr = (ihex.data[0] << 8 | ihex.data[1]) << 4;
        break;

      case 3: /* start segment address record */
        /* we don't do anything with the start address */
        break;

      case 4: /* extended linear address record */
        baseaddr = (ihex.data[0] << 8 | ihex.data[1]) << 16;
        break;

      case 5: /* start linear address record */
        /* we don't do anything with the start address */
        break;

      default:
        fprintf(stderr,
                "%s: don't know how to deal with rectype=%d "
                "at line %d of %s\n",
                progname, ihex.rectyp, lineno, infile);
        return -1;
        break;
    }

  } /* while */

  fprintf(stderr,
          "%s: WARNING: no end of file record found for Intel Hex "
          "file \"%s\"\n",
          progname, infile);

  return maxaddr;
}

/********** SERIAL I/O **********/

long serial_recv_timeout = 1000; /* ms */

static struct termios original_termios;
static int saved_original_termios;

struct baud_mapping {
  long baud;
  speed_t speed;
};

/* There are a lot more baud rates we could handle, but what's the point? */

static struct baud_mapping baud_lookup_table [] = {
  { 1200,   B1200 },
  { 2400,   B2400 },
  { 4800,   B4800 },
  { 9600,   B9600 },
  { 19200,  B19200 },
  { 38400,  B38400 },
  { 57600,  B57600 },
  { 115200, B115200 },
  { 230400, B230400 },
  { 0,      0 }                 /* Terminator. */
};

static speed_t serial_baud_lookup(long baud)
{
  struct baud_mapping *map = baud_lookup_table;

  while (map->baud) {
    if (map->baud == baud)
      return map->speed;
    map++;
  }

  fprintf(stderr, "%s: serial_baud_lookup(): unknown baud rate: %ld\n",
          progname, baud);
  exit(1);
}

int ser_setspeed(int fd, long baud, int flushfirst)
{
  int rc;
  struct termios termios;
  speed_t speed = serial_baud_lookup (baud);

  if (!isatty(fd))
    return -ENOTTY;

  /*
   * initialize terminal modes
   */
  rc = tcgetattr(fd, &termios);
  if (rc < 0) {
    fprintf(stderr, "%s: ser_setspeed(): tcgetattr() failed",
            progname);
    return -errno;
  }

  /*
   * copy termios for ser_close if we haven't already
   */
  if (! saved_original_termios++) {
    original_termios = termios;
  }

  termios.c_iflag = IGNBRK;
  termios.c_oflag = 0;
  termios.c_lflag = 0;
  termios.c_cflag = (CS8 | CREAD | CLOCAL);
  termios.c_cc[VMIN]  = 1;
  termios.c_cc[VTIME] = 0;

  cfsetospeed(&termios, speed);
  cfsetispeed(&termios, speed);

  rc = tcsetattr(fd, (flushfirst) ? TCSAFLUSH : TCSANOW, &termios);
  if (rc < 0) {
    fprintf(stderr, "%s: ser_setspeed(): tcsetattr() failed\n",
            progname);
    return -errno;
  }

  /*
   * Everything is now set up for a local line without modem control
   * or flow control, so clear O_NONBLOCK again.
   */
  rc = fcntl(fd, F_GETFL, 0);
  if (rc != -1)
    fcntl(fd, F_SETFL, rc & ~O_NONBLOCK);

  return 0;
}

int ser_open(char *port, long baud)
{
  int rc;
  int fd;

  /*
   * open the serial port
   */
  fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) {
    fprintf(stderr, "%s: ser_open(): can't open device \"%s\": %s\n",
            progname, port, strerror(errno));
    exit(1);
  }

  /*
   * set serial line attributes
   */
  rc = ser_setspeed(fd, baud, 0);
  if (rc) {
    fprintf(stderr,
            "%s: ser_open(): can't set attributes for device \"%s\": %s\n",
            progname, port, strerror(-rc));
    exit(1);
  }

  return fd;
}

void ser_close(int fd)
{
  /*
   * restore original termios settings from ser_open
   */
  if (saved_original_termios) {
    int rc = tcsetattr(fd, TCSANOW | TCSADRAIN, &original_termios);
    if (rc) {
      fprintf(stderr,
              "%s: ser_close(): can't reset attributes for device: %s\n",
              progname, strerror(errno));
    }
    saved_original_termios = 0;
  }

  close(fd);
}

int ser_send(int fd, unsigned char * buf, size_t buflen)
{
  //struct timeval timeout, to2;
  int rc;
  unsigned char * p = buf;
  size_t len = buflen;

  if (!len)
    return 0;

  if (verbose > 3)
  {
      fprintf(stderr, "%s: Send: ", progname);

      while (buflen) {
        unsigned char c = *buf;
        if (isprint(c)) {
          fprintf(stderr, "%c ", c);
        }
        else {
          fprintf(stderr, ". ");
        }
        fprintf(stderr, "[%02x] ", c);

        buf++;
        buflen--;
      }

      fprintf(stderr, "\n");
  }

  //timeout.tv_sec = 0;
  //timeout.tv_usec = 500000;
  //to2 = timeout;

  while (len) {
    rc = write(fd, p, (len > 1024) ? 1024 : len);
    if (rc < 0) {
      fprintf(stderr, "%s: ser_send(): write error: %s\n",
              progname, strerror(errno));
      exit(1);
    }
    p += rc;
    len -= rc;
  }

  return 0;
}

int ser_recv(int fd, unsigned char * buf, size_t buflen)
{
  struct timeval timeout, to2;
  fd_set rfds;
  int nfds;
  int rc;
  unsigned char * p = buf;
  size_t len = 0;

  timeout.tv_sec  = serial_recv_timeout / 1000L;
  timeout.tv_usec = (serial_recv_timeout % 1000L) * 1000;
  to2 = timeout;

  while (len < buflen) {
  reselect:
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    nfds = select(fd + 1, &rfds, NULL, NULL, &to2);
    if (nfds == 0) {
      if (verbose > 1)
        fprintf(stderr,
                "%s: ser_recv(): programmer is not responding\n",
                progname);
      return -1;
    }
    else if (nfds == -1) {
      if (errno == EINTR || errno == EAGAIN) {
        fprintf(stderr,
                "%s: ser_recv(): programmer is not responding,reselecting\n",
                progname);
        goto reselect;
      }
      else {
        fprintf(stderr, "%s: ser_recv(): select(): %s\n",
                progname, strerror(errno));
        exit(1);
      }
    }

    rc = read(fd, p, (buflen - len > 1024) ? 1024 : buflen - len);
    if (rc < 0) {
      fprintf(stderr, "%s: ser_recv(): read error: %s\n",
              progname, strerror(errno));
      exit(1);
    }
    p += rc;
    len += rc;
  }

  p = buf;

  if (verbose > 3)
  {
      fprintf(stderr, "%s: Recv: ", progname);

      while (len) {
        unsigned char c = *p;
        if (isprint(c)) {
          fprintf(stderr, "%c ", c);
        }
        else {
          fprintf(stderr, ". ");
        }
        fprintf(stderr, "[%02x] ", c);

        p++;
        len--;
      }
      fprintf(stderr, "\n");
  }

  return 0;
}

int ser_drain(int fd, int display)
{
  struct timeval timeout;
  fd_set rfds;
  int nfds;
  int rc;
  unsigned char buf;

  timeout.tv_sec = 0;
  timeout.tv_usec = 250000;

  if (display) {
    fprintf(stderr, "drain>");
  }

  while (1) {
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

  reselect:
    nfds = select(fd + 1, &rfds, NULL, NULL, &timeout);
    if (nfds == 0) {
      if (display) {
        fprintf(stderr, "<drain\n");
      }

      break;
    }
    else if (nfds == -1) {
      if (errno == EINTR) {
        goto reselect;
      }
      else {
        fprintf(stderr, "%s: ser_drain(): select(): %s\n",
                progname, strerror(errno));
        exit(1);
      }
    }

    rc = read(fd, &buf, 1);
    if (rc < 0) {
      fprintf(stderr, "%s: ser_drain(): read error: %s\n",
              progname, strerror(errno));
      exit(1);
    }
    if (display) {
      fprintf(stderr, "%02x ", buf);
    }
  }

  return 0;
}

/********** STK500 **********/

int stk500_getsync(int fd)
{
  unsigned char buf[4], resp[4];
  int verbose_drain = verbose > 4;

  /*
   * get in sync */
  buf[0] = Cmnd_STK_GET_SYNC;
  buf[1] = Sync_CRC_EOP;

  /*
   * First send and drain a few times to get rid of line noise
   */

  stk500_send(fd, buf, 2);
  stk500_drain(fd, verbose_drain);
  //stk500_send(fd, buf, 2);
  //stk500_drain(fd, verbose_drain);

  stk500_send(fd, buf, 2);
  if (stk500_recv(fd, resp, 1) < 0)
    return -1;
  if (resp[0] != Resp_STK_INSYNC) {
    fprintf(stderr,
            "%s: stk500_getsync(): not in sync: resp=0x%02x\n",
            progname, resp[0]);
    stk500_drain(fd, verbose_drain);
    // BRY: Return the character we got if it wasn't 0x00
    return (resp[0]) ? : -1;
  }

  if (stk500_recv(fd, resp, 1) < 0)
    return -1;
  if (resp[0] != Resp_STK_OK) {
    fprintf(stderr,
            "%s: stk500_getsync(): can't communicate with device: "
            "resp=0x%02x\n",
            progname, resp[0]);
    return -1;
  }

  return 0;
}

int stk500_getparm(int fd, unsigned char parm, unsigned char * value)
{
  unsigned char buf[8];

  buf[0] = Cmnd_STK_GET_PARAMETER;
  buf[1] = parm;
  buf[2] = Sync_CRC_EOP;

  stk500_send(fd, buf, 3);

  if (stk500_recv(fd, buf, 3) < 0)
    exit(1);
  if (buf[0] != Resp_STK_INSYNC || buf[2] != Resp_STK_OK) {
    fprintf(stderr,
            "\n%s: stk500_getparm(): protocol error, "
            "expect=0x%02x%02x, resp=0x%02x%02x\n",
            progname, Resp_STK_INSYNC, Resp_STK_OK,
            buf[0], buf[2]);
    return -1;
  }

  *value = buf[1];

  return 0;
}

int arduino_read_sig_bytes(int fd, unsigned int *sig)
{
  unsigned char buf[8];

  /* Signature byte reads are always 3 bytes. */

  buf[0] = Cmnd_STK_READ_SIGN;
  buf[1] = Sync_CRC_EOP;

  stk500_send(fd, buf, 2);

  if (stk500_recv(fd, buf, 5) < 0)
    exit(1);
  if (buf[0] != Resp_STK_INSYNC || buf[4] != Resp_STK_OK) {
    fprintf(stderr, 
            "\n%s: arduino_read_sig_bytes(): protocol error, "
            "expect=0x%02x%02x, resp=0x%02x%02x\n",
            progname, Resp_STK_INSYNC, Resp_STK_OK,
            buf[0], buf[4]);
    return -1;
  }

  *sig = (buf[1] << 16) | (buf[2] << 8) | buf[3];
  return 0;
}

int stk500_loadaddr(int fd, unsigned int addr)
{
  unsigned char buf[8];

  buf[0] = Cmnd_STK_LOAD_ADDRESS;
  buf[1] = addr & 0xff;
  buf[2] = (addr >> 8) & 0xff;
  buf[3] = Sync_CRC_EOP;

  stk500_send(fd, buf, 4);

  if (stk500_recv(fd, buf, 2) < 0)
    exit(1);
  if (buf[0] != Resp_STK_INSYNC || buf[1] != Resp_STK_OK) {
    fprintf(stderr,
            "%s: stk500_loadaddr(): protocol error, "
            "expect=0x%02x%02x, resp=0x%02x%02x\n",
            progname, Resp_STK_INSYNC, Resp_STK_OK, 
            buf[0], buf[1]);
    return -1;
  }

  return 0;
}

int stk500_disable(int fd)
{
  unsigned char buf[8];

  buf[0] = Cmnd_STK_LEAVE_PROGMODE;
  buf[1] = Sync_CRC_EOP;

  stk500_send(fd, buf, 2);
  if (stk500_recv(fd, buf, 2) < 0)
    exit(1);
  if (buf[0] != Resp_STK_INSYNC || buf[1] != Resp_STK_OK) {
    fprintf(stderr,
            "%s: stk500_disable(): protocol error, "
            "expect=0x%02x%02x, resp=0x%02x%02x\n",
            progname, Resp_STK_INSYNC, Resp_STK_OK,
            buf[0], buf[1]);
    return -1;
  }

  return 0;
}

