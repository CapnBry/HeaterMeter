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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "fileio.h"

const char *progname = "hmdude";
int verbose;

static long baud;
static char *file_ihex;
static char *port = "/dev/ttyS0";
static char *reboot_cmd = "\n/reboot\n";

static int port_fd;
static unsigned char ihex[32 * 1024];
static int ihex_len;

int read_optiboot_ver(void)
{
  unsigned char ver_maj, ver_min;
  fprintf(stdout, "Optiboot version: ");
  if (stk500_getparm(port_fd, Parm_STK_SW_MAJOR, &ver_maj) == 0 &&
    stk500_getparm(port_fd, Parm_STK_SW_MINOR, &ver_min) == 0)
  {
    fprintf(stdout, "%u.%u\n", ver_maj, ver_min);
    return 0;
  }
  else
  {
    fprintf(stdout, "(failed)\n");
    return -1;
  }
}

int read_device_signature(void)
{
  unsigned int signature;
  fprintf(stdout, "Device signature: ");
  if (arduino_read_sig_bytes(port_fd, &signature) == 0)
  {
    fprintf(stdout, "0x%06x\n", signature);
    return 0;
  }
  else
  {
    fprintf(stdout, "(failed)\n");
    return -1;
  }
}

inline void reboot(void)
{
  ser_send(port_fd, (unsigned char *)reboot_cmd, strlen(reboot_cmd));
}

int load_ihex(void)
{
  FILE *ifile;
  ihex_len = 0;
  if (file_ihex == NULL)
    return 0;

  if ((ifile = fopen(file_ihex, "r")) == 0)
  {
    fprintf(stderr, "Can't open file %s\n", file_ihex);
    return -1;
  }

  int rc = ihex2b(file_ihex, ifile, ihex, sizeof(ihex));
  fclose(ifile);  
  if (rc < 0)
    return -1;

  ihex_len = rc;
  fprintf(stdout, "Loading ihex file: \"%s\" (%d bytes)\n", 
    file_ihex, ihex_len);

  return 0;
}

void report_progress(unsigned int progress, unsigned int max)
{
  static int last;
  static double start_time;
  char hashes[51];
  int percent;
  struct timeval tv;
  double t;
  
  percent = progress * 100 / max;
  if (progress != 0 && percent < last)
    return;
  // only report progress every 1 or 5 percent
  last = percent + (isatty(STDOUT_FILENO) ? 1 : 5);

  memset(hashes, ' ', 50);
  memset(hashes, '#', percent/2);
  hashes[50] = '\0';

  gettimeofday(&tv, NULL);
  t = tv.tv_sec + ((double)tv.tv_usec)/1000000;
  if (progress == 0)
    start_time = t;

  fprintf(stdout, "\r  %3d%% |%s| %5d (%.1fs)", percent, hashes, progress, 
    t - start_time);
  if (percent == 100)
    fprintf(stdout, "\n");
  fflush(stdout);
}

#define PAGE_SIZE 128
int upload_ihex(void)
{
  if (ihex_len == 0)
    return 0;

  unsigned char buf[PAGE_SIZE + 8];
  unsigned int addr;
  int rc = 0;

  for (addr = 0; addr < ihex_len; addr += PAGE_SIZE) 
  {
    // address is in words
    if ((rc = stk500_loadaddr(port_fd, addr/2)) != 0)
      break;
    report_progress(addr, ihex_len);
   
    buf[0] = Cmnd_STK_PROG_PAGE;
    buf[1] = (PAGE_SIZE >> 8) & 0xff;
    buf[2] = PAGE_SIZE & 0xff;
    buf[3] = 'F'; // Flash
    memcpy(&buf[4], &ihex[addr], PAGE_SIZE);
    buf[4 + PAGE_SIZE] = Sync_CRC_EOP;

    stk500_send(port_fd, buf, 5 + PAGE_SIZE);
 
    if (stk500_recv(port_fd, buf, 2) < 0)
      exit(1);
    if (buf[0] != Resp_STK_INSYNC || buf[1] != Resp_STK_OK) {
      fprintf(stderr,
              "\n%s: upload_ihex(): (a) protocol error, "
              "expect=0x%02x%02x, resp=0x%02x%02x\n",
              progname, Resp_STK_INSYNC, Resp_STK_OK,
              buf[0], buf[1]);
      return -4;
    }
  } /* for addr */
  report_progress(ihex_len, ihex_len);

  return rc;
}

int upload_file(void)
{
  if ((port_fd = ser_open(port, (baud) ? : 115200L)) < 0)
    return -1;

  int rc;
  int rebootcnt = 0;
  fprintf(stdout, "Starting sync (release RESET now)...\n");
  reboot();
  do {
    fprintf(stdout, "Sync: ");
    rc = stk500_getsync(port_fd);
    if (rc == '$')
    {
      fprintf(stdout, "HeaterMeter\n");
      reboot();
    }
    else if (rc != 0)
      fprintf(stdout, "ERROR\n");
  } while (rc != 0 && ++rebootcnt < 5);

  if (rc != 0)
    goto cleanup;
  fprintf(stdout, "OK\n");

  if ((rc = read_device_signature()) != 0)
    goto cleanup;
  if ((rc = read_optiboot_ver()) != 0)
    goto cleanup;
  if ((rc = upload_ihex()) != 0)
    goto cleanup;
  stk500_disable(port_fd);

cleanup:
  ser_close(port_fd);
  return rc;
}

int main(int argc, char *argv[])
{
  int c;
  fprintf(stdout, "%s: compiled on %s at %s\n",
    progname, __DATE__, __TIME__);

  opterr = 0;
  while ((c = getopt(argc, argv, "b:vP:U:")) != -1)
  {
    switch (c)
    {
      case 'b':
        baud = atol(optarg);
        break;
      case 'v':
        ++verbose;
        break;
      case 'P':
        port = optarg;
        break;
      case 'U':
        file_ihex = optarg;
        break;
      default:
        fprintf(stderr, "Unknown option '-%c'\n", optopt);
        break;
    }
  } /* while getopt */

  fprintf(stdout, "Using port: %s\n", port);
  load_ihex();
  return upload_file();
}

