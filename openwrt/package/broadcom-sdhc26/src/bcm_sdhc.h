
#ifndef BCM_SDHC26_BCM_SDHC_H_
#define BCM_SDHC26_BCM_SDHC_H_

#define MOD_NAME "bcm_sdhc"     // Module name
#define MOD_VERSION "1.0.0"     // Module version number
#define DEVICE_NAME "bcmsdhc"   // Device name

#define DEBUG

// Default values for modules parameters - can override by passing a module parameter.
#define BLK_MAJOR 0 // Dynamic device major
#define DI  2       // Card DI to GPIO pin mapping
#define DO  4       // Card DO to GPIO pin mapping
#define CLK 3       // Card CLK to GPIP pin mapping
#define CS  7       // Card CS to GPIO pin mapping
#define MSEC 32     // Maximum sectors per request

// Structure for tracking card information
struct card_info {
    unsigned char      state;   // State of card
#define CARD_STATE_DET   (1<<0) // b'00000001' - Card detected
#define CARD_STATE_INI   (1<<1) // b'00000010' - Card initialized
    unsigned char      type;    // high nybble - card type - low nybble - capacity
#define CARD_TYPE_MM     (1<<5) // b'0010xxxx' - MM Card
#define CARD_TYPE_SD     (1<<4) // b'0001xxxx' - SD Card
#define CARD_TYPE_HC     (1<<0) // b'xxxx0001' - High Capacity card if bit set
    unsigned char      version; // card version - based on card type
#define CARD_VERSD_10    0x00;  // SD card - 1.00-1.01
#define CARD_VERSD_11    0x01;  //         - 1.10
#define CARD_VERSD_20    0x02   //         - 2.00
#define CARD_VERMM_10    0x00   // MM card - 1.0-1.2
#define CARD_VERMM_14    0x01   //         - 1.4
#define CARD_VERMM_20    0x02   //         - 2.0-2.2
#define CARD_VERMM_30    0x03   //         - 3.1-3.3
#define CARD_VERMM_40    0x04   //         - 4.0-4.1
    unsigned int       blocks;  // Number of 512 byte blocks on card - use to calculate size
    unsigned int       volt;    // Voltage range bits from OCR
    unsigned char      manid;   // Manufacturer ID
    unsigned char      appid[2];    // OEM/Application ID
    unsigned char      name[6]; // Product Name
    unsigned char      rev;     // Product Revision
    unsigned int       year;    // Product Date - year
    unsigned char      month;   // Product Date - month
    unsigned int       serial;  // Product serial number - can use to detect card change
};

#ifdef DEBUG    // Defined via compiler argument
// Debug flag - bit symbols
#define DBG_INIT (1 << 0)   // 1
#define DBG_OPEN (1 << 1)   // 2 
#define DBG_RLSE (1 << 2)   // 4
#define DBG_CHG  (1 << 3)   // 8
#define DBG_REVAL (1 << 4)  // 16
#define DBG_REQ   (1 << 5)  // 32
#define DBG_IOCTL (1 << 6)  // 64
#define DBG_BUSY (1 << 7)   // 128
#endif

#include <linux/kernel.h>

// Macros for logging via printk.
#define LOG_INFO(...)   printk(KERN_INFO "[INF] bcm_sdhc: " __VA_ARGS__)
#define LOG_NOTICE(...) printk(KERN_NOTICE "[NOT] bcm_sdhc: " __VA_ARGS__)
#define LOG_WARN(...)   printk(KERN_WARNING "[WRN] bcm_sdhc: " __VA_ARGS__)
#define LOG_ERR(...)    printk(KERN_ERR "[ERR] bcm_sdhc: " __VA_ARGS__)

#ifdef DEBUG
#define LOG_DEBUG(flag, ...) if (dbg & flag) printk(KERN_INFO "[DBG] bcm_sdhc: " __VA_ARGS__)
#else
#define LOG_DEBUG(flag, ...)
#endif

// Module global variable declarations and setup.
static unsigned int major = BLK_MAJOR;  // Device major number
static int din = DI;    // GPIO attached to card DI pin
static int dout = DO;   // GPIO attached to card DO pin
static int clk = CLK;   // GPIO attached to card CLK pin
static int cs = CS;     // GPIO attached to card CS pin
static int maxsec = MSEC;   // Maximum number of sectors per request

static volatile unsigned char *gpio_input = ( unsigned char * ) 0xb8000060;
static volatile unsigned char *gpio_output = ( unsigned char * ) 0xb8000064;
static volatile unsigned char *gpio_enable = ( unsigned char * ) 0xb8000068;
static volatile unsigned char *gpio_control = ( unsigned char * ) 0xb800006c;

static struct card_info *card = NULL;

#ifdef DEBUG
static int dbg = 0;     // Flag bits indicating what debugging to produce
#endif

#endif /* BCM_SDHC26_BCM_SDHC_H_ */
