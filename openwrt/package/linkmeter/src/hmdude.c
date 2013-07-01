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
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/spi/spidev.h>

#include <uci.h>

#include "fileio.h"
#include "bcm2835.h"

const char *progname = "hmdude";
int verbose;

static long baud;
static long lm_baud;
static char *file_ihex;
static char *port;

static int port_fd;
static unsigned char ihex[32 * 1024];
static int ihex_len;
static int spi_intialized;

static bool do_chiperase;
static bool do_disable_autoerase;
static bool do_dumpmemory;
static bool do_verify = true;

#define msleep(x) usleep(x * 1000)

#define PAGE_SIZE 128
#define WADDR_PAGE(x) (x & 0xffffffc0)

#define FUSE_LOW  0
#define FUSE_HIGH 1
#define FUSE_EXT  2
#define FUSE_LOCK 3
static char const * const FUSE_NAME[4] = { "low", "high", "extended", "lock" };
static uint8_t write_fuse[4];
static bool do_fuse[4];

static int read_optiboot_ver(void)
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

static int read_device_signature(void)
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

static void reboot(void)
{
  char *reboot_cmd = "\n/reboot\n";
  ser_setspeed(port_fd, lm_baud, 1);
  ser_send(port_fd, (unsigned char *)reboot_cmd, strlen(reboot_cmd));
  ser_setspeed(port_fd, baud, 1);
}

/* Find the first non-0xff byte in ihex */
static unsigned int ihex_first_nondefault(void)
{
  unsigned int i;
  for (i=0; i<sizeof(ihex); ++i)
    if (ihex[i] != 0xff) 
      return i;
  return sizeof(ihex);
}

static int load_ihex(void)
{
  FILE *ifile;
  ihex_len = 0;
  if (file_ihex == NULL)
    return 0;

  memset(ihex, 0xff, sizeof(ihex));

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

static void report_progress(unsigned int progress, unsigned int max)
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

static int upload_ihex(void)
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

static int upload_file(void)
{
  if (baud == 0)
    baud = 115200;
  if (lm_baud == 0)
    lm_baud = baud;

  if ((port_fd = ser_open(port, lm_baud)) < 0)
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

  if (verbose > 1 && (rc = read_device_signature()) != 0)
    goto cleanup;
  if (verbose > 1 && (rc = read_optiboot_ver()) != 0)
    goto cleanup;
  if ((rc = upload_ihex()) != 0)
    goto cleanup;
  stk500_disable(port_fd);

cleanup:
  ser_close(port_fd);
  return rc;
}

static uint8_t spi_transaction(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t ret)
{
  uint8_t spi_buf[4] = { a, b, c, d };
  struct spi_ioc_transfer tr = {
    .tx_buf = (unsigned long)spi_buf,
    .rx_buf = (unsigned long)spi_buf,
    .len = 4,
    .delay_usecs = 0,
    .speed_hz = 0,
    .bits_per_word = 0,
    .cs_change = 0
  };

  if (verbose > 3)
    fprintf(stdout, "spi(%02x, %02x, %02x, %02x) = ", a, b, c, d); 
  if (ioctl(port_fd, SPI_IOC_MESSAGE(1), &tr) < 0)
    return 0;
  if (verbose > 3)
    fprintf(stdout, "%02x %02x %02x %02x\n", spi_buf[0], spi_buf[1],
      spi_buf[2], spi_buf[3]); 

  return spi_buf[ret];
}

static int spi_read_device_signature(void)
{
  uint8_t vendor = spi_transaction(0x30, 0x00, 0x00, 0x00, 3); 
  uint8_t family = spi_transaction(0x30, 0x00, 0x01, 0x00, 3); 
  uint8_t part = spi_transaction(0x30, 0x00, 0x02, 0x00, 3); 

  fprintf(stdout, "Device signature: ");
  if (vendor == 0x1e && family == 0x95)
  {
    if (part == 0x14)
      fprintf(stdout, "ATmega328\n");
    else if (part == 0x0f)
      fprintf(stdout, "ATmega328P\n");
    else
      fprintf(stdout, "ATmega32x part 0x%02x\n", part);
    return 0;
  }
  else
  {
    fprintf(stdout, "0x%02x%02x%02x\n", vendor, family, part);
    return -1;
  }
}

static int spi_programming_enable(void)
{
  uint8_t ret;
  ret = spi_transaction(0xac, 0x53, 0x00, 0x00, 2);
  if (ret != 0x53)
  {
    fprintf(stderr, "Can't set AVR programming mode (0x%02x)\n", ret);
    return -1;
  }
  if (verbose > 2) fprintf(stdout, "AVR programming mode set\n");
  return 0;
}

static void spi_wait_not_busy(uint8_t max)
{
  max = max / 2;
  do {
    usleep(200);
  } while (--max && (spi_transaction(0xf0, 0x00, 0x00, 0x00, 3) & 0x01));
}

static int spi_chiperase(void)
{
  spi_transaction(0xac, 0x80, 0x00, 0x00, 0);
  spi_wait_not_busy(90);
  if (verbose > 0)
    fprintf(stdout, "Chip erased\n");
  return 0;
}

static void spi_load_progmem_page(uint8_t waddr_lsb, uint8_t low, uint8_t high)
{
  spi_transaction(0x40, 0x00, waddr_lsb, low, 0);
  spi_transaction(0x48, 0x00, waddr_lsb, high, 0);
}

static void spi_write_progmem_page(uint16_t page)
{
  spi_transaction(0x4c, (page >> 8) & 0xff, page & 0xff, 0, 0);
  spi_wait_not_busy(45);
  if (verbose > 2) fprintf(stdout, "Committed page 0x%04x\n", page);
}

static int spi_upload_ihex(void)
{
  if (ihex_len == 0)
    return 0;

  unsigned int addr = ihex_first_nondefault() & 0xfffe;
  unsigned int waddr = addr / 2; 
  unsigned int page = WADDR_PAGE(waddr);

  if (verbose > 2) fprintf(stdout, "Starting address: 0x%04x\n", addr);

  report_progress(0, ihex_len);
  while (addr < ihex_len) 
  {
    report_progress(addr, ihex_len);
    if (page != WADDR_PAGE(waddr))
    {
      spi_write_progmem_page(page);
      page = WADDR_PAGE(waddr);
    }
    spi_load_progmem_page(waddr & 0x3f, ihex[addr], ihex[addr+1]);
    addr += 2;
    ++waddr;

  }
  spi_write_progmem_page(page);
  report_progress(ihex_len, ihex_len);
  return 0;
}

static int spi_verify_ihex(void)
{
  if (ihex_len == 0)
    return 0;
  
  unsigned int addr = ihex_first_nondefault() & 0xfffe;
  unsigned int waddr = addr / 2;

  fprintf(stdout, "Verifying...\n");
  report_progress(0, ihex_len);
  while (addr < ihex_len)
  {
    uint8_t msb = (waddr >> 8) & 0xff;
    uint8_t lsb = waddr & 0xff;

    report_progress(addr, ihex_len);
    if (spi_transaction(0x20, msb, lsb, 0x00, 3) != ihex[addr])
      break;
    ++addr;
    if (spi_transaction(0x28, msb, lsb, 0x00, 3) != ihex[addr])
      break;
    ++addr;

    ++waddr;
  }
  
  if (addr < ihex_len)
  {
    fprintf(stdout, "\nVerify mismatch at 0x%04x\n", addr);
    return -1;
  }
  
  report_progress(ihex_len, ihex_len);
  return 0;

}

static void spi_write_fuse(uint8_t fuse, uint8_t val)
{
   const uint8_t FUSE_TO_ADDR[4] = { 0xa0, 0xa8, 0xa4, 0xe0 };
   spi_transaction(0xac, FUSE_TO_ADDR[fuse], 0x00, val, 0);
   spi_wait_not_busy(45);
   fprintf(stdout, "Writing %s fuse: 0x%02x\n", FUSE_NAME[fuse], val);
}

/* 
   This is pretty hacky, it requires root permission and we're direcrtly
   manipulating GPIO memory and loading modules
*/
static int spi_setup(bool val)
{
  if (val)
  {
    // init
    spi_intialized = bcm2835_init();
    if (spi_intialized == 0)
      return -1;
    // Set the SPI lines back the way the spi driver expects
    bcm2835_gpio_fsel(9, BCM2835_GPIO_FSEL_ALT0); // MISO
    bcm2835_gpio_fsel(10, BCM2835_GPIO_FSEL_ALT0); // MOSI
    bcm2835_gpio_fsel(11, BCM2835_GPIO_FSEL_ALT0); // CLK

    // Set the GPIO25 to output/low, which is tied to /RESET
    bcm2835_gpio_fsel(25, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_clr(25);
    // Delay a minimum of 20ms
    msleep(20);
  }
  else if (spi_intialized != 0)
  {
    // Make sure the driver cleaned up its GPIOs
    bcm2835_gpio_fsel(9, BCM2835_GPIO_FSEL_INPT); // MISO
    bcm2835_gpio_fsel(10, BCM2835_GPIO_FSEL_INPT); // MOSI
    bcm2835_gpio_fsel(11, BCM2835_GPIO_FSEL_INPT); // CLK
    // Set GPIO25 back to high/input
    bcm2835_gpio_set(25);
    bcm2835_gpio_fsel(25, BCM2835_GPIO_FSEL_INPT);
    // done
    bcm2835_close();
  }

#if 0
  #define gpio_base "/sys/class/gpio/"
  FILE *f;
  if (val)
  {
    if ((f = fopen(gpio_base "gpio25/value", "w")) == 0)
      return -3;
    fputs("1", f);
    fclose(f);
  }
  else
  {
    if ((f = fopen(gpio_base "export", "w")) != 0)
    {
      fputs("25", f);
      fclose(f);
    }

    if ((f = fopen(gpio_base "gpio25/direction", "w")) != 0)
    {
      fputs("out", f);
      fclose(f);
    }

    chmod(gpio_base "gpio25/value", 0666);
    
    if ((f = fopen(gpio_base "gpio25/value", "w")) == 0)
      return -1;
    fputs("0", f);
    fclose(f);
  }
#endif

  return 0;
}

static uint8_t spi_read_fuse(uint8_t fuse)
{
  uint8_t a, b, mask;
  switch (fuse)
  {
    case FUSE_LOW:
      a = 0x50; b = 0x50; mask = 0xff; break;
    case FUSE_HIGH:
      a = 0x58; b = 0x08; mask = 0xff; break;
    case FUSE_EXT:
      a = 0x50; b = 0x08; mask = 0x07; break;
    case FUSE_LOCK:
      a = 0x58; b = 0x00; mask = 0x3f; break;
    default:
      a = b = mask = 0; break;
  }
  return spi_transaction(a, b, 0, 0, 3) & mask;
}

static int spi_read_fuses(void)
{
  fprintf(stdout, "Low: 0x%02x ", spi_read_fuse(FUSE_LOW));
  fprintf(stdout, "High: 0x%02x ", spi_read_fuse(FUSE_HIGH));
  fprintf(stdout, "Ext: 0x%02x ", spi_read_fuse(FUSE_EXT));
  fprintf(stdout, "Lock: 0x%02x\n", spi_read_fuse(FUSE_LOCK));
  return 0;
}

static void spi_dump_progmem(void)
{
  uint16_t waddr;
  for (waddr=0; waddr<0x4000; ++waddr)
  {
    uint8_t msb = (waddr >> 8) & 0xff;
    uint8_t lsb = waddr & 0xff;

    if (waddr % 8 == 0)
      fprintf(stdout, "%04x:", waddr * 2);
    fprintf(stdout, "%02x ", spi_transaction(0x20, msb, lsb, 0x00, 3));
    fprintf(stdout, "%02x ", spi_transaction(0x28, msb, lsb, 0x00, 3));
    if (waddr % 8 == 7)
      fprintf(stdout, "\n");
  }
}

static void spi_dump_eeprom(void)
{
  uint16_t addr;
  for (addr=0; addr<1024; ++addr)
  {
    uint8_t msb = (addr >> 8) & 0xff;
    uint8_t lsb = addr & 0xff;

    if (addr % 16 == 0)
      fprintf(stdout, "%04x:", addr);
    fprintf(stdout, "%02x ", spi_transaction(0xa0, msb, lsb, 0, 3));
    if (addr % 16 == 15)
      fprintf(stdout, "\n");
  }
}

static int spi_upload_file(void)
{
  int rc = 0;
  uint8_t mode = SPI_MODE_0;
  uint8_t bits = 8;

  if (spi_setup(true) != 0)
    return -1;

  port_fd = open(port, O_RDWR);
  if (port_fd < 0)
  {
    rc = port_fd;
    fprintf(stderr, "can't open SPI device\n");
    goto cleanup;
  }

  if (baud == 0)
    baud = 2000;
  baud *= 1000;

  if ((rc = ioctl(port_fd, SPI_IOC_WR_MODE, &mode)) == -1)
  {
    fprintf(stderr, "can't set SPI mode\n");
    goto cleanup;
  }
  if ((rc = ioctl(port_fd, SPI_IOC_WR_BITS_PER_WORD, &bits)) == -1)
  {
    fprintf(stderr, "can't set SPI bits\n");
    goto cleanup;
  }
  if ((rc = ioctl(port_fd, SPI_IOC_WR_MAX_SPEED_HZ, &baud)) == -1)
  {
    fprintf(stderr, "can't set SPI speed\n");
    goto cleanup;
  }
  if ((rc = ioctl(port_fd, SPI_IOC_RD_MAX_SPEED_HZ, &baud)) == -1)
  {
    fprintf(stderr, "can't get SPI speed\n");
    goto cleanup;
  }
  if (verbose > 1) fprintf(stdout, "SPI speed: %ld KHz\n", baud/1024);

  if ((rc = spi_programming_enable()) != 0)
    goto cleanup;
  if (verbose > 0 && (rc = spi_read_device_signature()) != 0)
    goto cleanup;
  if (verbose > 0 && (rc = spi_read_fuses()) != 0)
    goto cleanup;

  // chiperase if user requested or we are flashing and the user didn't
  // explicitly ask not to. ATmegas appear to only be able to un-set bits
  // during a flash operation, so you can flash without erasing if your
  // chip is already full of 0xff
  if (do_chiperase || 
    (file_ihex != NULL && do_disable_autoerase == false))
  { 
    if ((rc = spi_chiperase()) != 0)
      goto cleanup;
  }

  if ((rc = spi_upload_ihex()) != 0)
    goto cleanup;

  if (do_verify)
    rc = spi_verify_ihex();

  uint8_t fuse;
  for (fuse=0; fuse<4; ++fuse)
    if (do_fuse[fuse])
      spi_write_fuse(fuse, write_fuse[fuse]);
  
  if (do_dumpmemory)
  {
    fprintf(stdout, "PROGMEM\n");
    spi_dump_progmem();
    fprintf(stdout, "\nEEPROM\n");
    spi_dump_eeprom();
  }

cleanup:  
  if (port_fd >= 0)
    close(port_fd);
  spi_setup(false);

  return rc;
}

static void set_port(char *val)
{
  if (port)
    free(port);
  if (val)
    port = strdup(val);
  else
    port = NULL;
}

static void set_file_ihex(char *val)
{
  if (file_ihex)
    free(file_ihex);
  if (val)
    file_ihex = strdup(val);
  else
    file_ihex = NULL;
}

static void load_lm_config(void)
{
  char name[64];
  struct uci_context *ctx = uci_alloc_context();

  struct uci_ptr ptr;
  strcpy(name, "lucid.linkmeter.serial_device");
  if (uci_lookup_ptr(ctx, &ptr, name, false) == UCI_OK &&
    (ptr.flags & UCI_LOOKUP_COMPLETE))
  {
    set_port(ptr.o->v.string);
    //fprintf(stdout, "LinkMeter device: %s\n", ptr.o->v.string);
  }
  else printf("%x\n", ptr.flags);
  strcpy(name, "lucid.linkmeter.serial_baud");
  if (uci_lookup_ptr(ctx, &ptr, name, false) == UCI_OK &&
    (ptr.flags & UCI_LOOKUP_COMPLETE))
  {
    lm_baud = atol(ptr.o->v.string);
    //fprintf(stdout, "LinkMeter baud: %ld\n", lm_baud);
  }

  uci_free_context(ctx);
}

static void parse_uflag(char *arg)
{
  char const * const FUSE_PARAMS[] =
    { "lfuse:w:0x", "hfuse:w:0x", "efuse:w:0x", "lock:w:0x" };
  uint8_t fuse;
  for (fuse=0; fuse<4; ++fuse)
  {
    uint8_t l = strlen(FUSE_PARAMS[fuse]);
    if (strncmp(arg, FUSE_PARAMS[fuse], l) == 0)
    {
      write_fuse[fuse] = strtol(arg+l, NULL, 16) & 0xff;
      do_fuse[fuse] = true;
      return;
    }
  }

  set_file_ihex(arg);
}

static int config_fuses_invalid(void)
{
  /* Just some sanity checks to make sure I don't set anything that
     will require a high voltage programmer to fix */
  int rc = 0;
  if (do_fuse[FUSE_HIGH])
  {
    if ((write_fuse[FUSE_HIGH] & 0x80) == 0)
    {
      fprintf(stderr, "FUSE: Will not RSTDISBL\n");
      rc = -1;
    }
    if ((write_fuse[FUSE_HIGH] & 0x20) != 0)
    {
      fprintf(stderr, "FUSE: Will not disable SPIEN\n");
      rc = -1;
    }
  }
  return rc;
}

int main(int argc, char *argv[])
{
  int rc;
  int c;
  fprintf(stdout, "%s: compiled on %s at %s\n",
    progname, __DATE__, __TIME__);

  load_lm_config();

  opterr = 0;
  while ((c = getopt(argc, argv, "b:devDP:U:V")) != -1)
  {
    switch (c)
    {
      case 'b':
        baud = atol(optarg);
        break;
      case 'd':
        do_dumpmemory = true;
        break;
      case 'e':
        do_chiperase = true;
        break;
      case 'v':
        ++verbose;
        break;
      case 'D':
        do_disable_autoerase = true;
        break;
      case 'P':
        set_port(optarg);
        break;
      case 'U':
        parse_uflag(optarg);
        break;
      case 'V':
        do_verify = false;
        break;
      default:
        fprintf(stderr, "Unknown option '-%c'\n", optopt);
        break;
    }
  } /* while getopt */
 
  if (config_fuses_invalid())
    return -1; 

  fprintf(stdout, "Using port: %s\n", port);
  load_ihex();

  // if the device is SPI this is a RaspberryPi, use SPI interface
  if (strncmp(port, "/dev/spidev", 11) == 0)
    rc = spi_upload_file();
  else
    rc = upload_file();

  // Cleanup   
  set_port(NULL);
  set_file_ihex(NULL);
 
  return rc;
}

