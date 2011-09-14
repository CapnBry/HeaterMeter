/*==============================================================================
 * bcm_sdhc.c - Linksys WRT54G/WRT54GS/WRT54GL hardware mod - SDHC/MMHC card driver
 *
 * Version: 3.0.1
 *
 * Authors:
 *
 *   Severn Tsui (kernel 2.6 port)
 *   Madsuk/Rohde/Cyril CATTIAUX/Marc DENTY/rcichielo KRUSCH/Chris
 *
 * Description:
 *
 *   Kernel 2.6 port of the broadcom-sdhc package which was a
 *   rework of the 1.3.5 optimized driver posted on the opewnwrt forum.
 *   See the Release notes for a description of changes and new features.
 * 
 *   This is intended to be used for releases after Backfire 10.03.
 *   NOTE: If you use this with the b43 wireless driver, you need to ensure you pass it the
 *   gpiomask parameter to ensure it doesn't use the pins this driver uses.
 *
 * Module Parameters:
 *
 *   major  - Major number to be assigned to the sdhc device (default 0 - assign dynamically).
 *
 *   cs     - GPIO line connect to the card CS (chip select) pin (default 7).
 *
 *   cl     - GPIO line connected to the card CLK (clock) pin (default 2).
 *
 *   di     - GPIO line connected to the card DI (data in) pin (default 3).
 *
 *   do     - GPIO line connected to the card DO (data out) pin (default 4).
 *
 *   maxsec - Maximum number of sectors that can be clustered into one request (default 32).
 *            Increasing this number can improve read and/or write throughput for large files.
 *            Keep it smaller if you expect frequent concurrent IO to the card (reading/writing
 *            of multiple files at the same time). Experiment with the setting to see what
 *            works best for your system.
 *
 *   dbg    - Only valid if you load the debug version of the kernel module (default 0).
 *            Bit flags that specify what debugging to product to the system log:
 *
 *            1  - Card initialization trace
 *            6  - Calls to module "request" function
 *            8  - Print "busy wait" information
 *
 *  gpio_input    - Set the gpio register memory locations.
 *  gpio_output     Allows defaults to be overridden when testing driver
 *  gpio_enable     on other Broadcom based devices.
 *  gpio_control
 *
 *
 * Release Notes:
 *
 *   Version 2.0.0 - Mar 9, 2008
 *
 *     - Rework of code base:
 *
 *         1) Rework of functions that must honour max clock frequency. These functions
 *            were generalized and condensed. Max clock frequency now managed through 2
 *            global vars - no need to pass timing arguments.
 *
 *         2) Logging functions replaced/simplified by variadic macros.
 *
 *         3) Document and comment. Standardize layout, variables, style, etc. Split
 *            card initialization function into separate source file.
 *
 *     - Switch so module uses a dynamically assigned major number by default. Implement "major="
 *       module parameter to allow a specific major number to be assigned.
 *
 *     - Implement module parameters "cs=", "clk=", "din=", "dout=" for specifying GPIO to card mapping.
 *       Alter read/write algorithms to be more efficient with mappings in variables.
 *
 *     - Implement module parameters "gpio_input=", "gpio_output=", "gpio_enable=", "gpio_control=" for
 *       specifying GPIO register addresses. May be useful if you want to try using this module on other
 *       broadcom based platforms where the gpio registers are located at different locations.
 *
 *     - Debugging improvements. Implement "dbg=" module parameter to allow selective enabling of
 *       debugging output by function. Only available when module compiled with debugging (-DDEBUG)
 *
 *     - Initialize max_segments array so requests are clustered. "maxsec=" module parameter
 *       sets the maximum number of sectors that can be clustered per request (default is 32).
 *
 *     - Implement clustering support in the module request method. Improves speed by allowing more
 *       clusters to be read/written per single invocation of a multi block read/write command.
 *
 *     - Implement Support for high capacity (> 2GB) SDHC and MMC cards.
 *
 *     - Implement /proc/sdhc/status for obtaining information about the detected card.
 *
 *     - Maximum number of supported partitions reduced from 64 to 8 (memory use reduction).
 *
 *     - Build using buildroot-ng environment. Generate ipkg file for installation.
 *       With so little difference in speed, and only a 4k memory savings, compile debug enabled version
 *
 *   Version 2.0.1 - Feb 8, 2009
 *
 *     - Changed module name to sdhc (more people seem to use sdcards with this module).
 *
 *     - Changed device directory to /dev/sdcard.
 *
 *     - Build module without debugging (sdhc.o) and one with debugging enabled (sdhcd.o)
 *       Module without debugging built in is 4Kb smaller and slightly faster.
 *
 *     - Rename init script to sdcard. Modify script to load sdhc.o when no degugging is requested
 *       in the config file, and sdhcd.o when debugging is requested.
 *
 *     - Overhauled the module makefile to support building standalone or via buildrootng.
 *
 *     - Bug Fix: card size calculated incorrectly for cards > 4GB (integer overflow)
 *
 *     - Bug Fix: /dev/sdcard entry remained after module unloaded - it is now properly removed
 *
 *     - Bug Fix: Device leak under /dev/discs/ - number of discs would grow by 1 each time an sd card
 *                was mounted. Entries are now properly removed on module unload
 *
 *     - Bug Fix: A cat of /proc/partitions after sdhc module was unloaded would cause a kernel fault.
 *
 *   Version 2.0.2 - Dec 15, 2009
 *
 *     - Bug Fix: Fix sdcard init script - would not properly pass kernel module arguments when debug
 *                set to 0.
 *
 *   Version 3.0.0 - May 18, 2011
 *
 *     - Kernel 2.6 port
 *     - Change all init scripts to OpenWRT Backfire (10.03) style
 * 
 *   Version 3.0.1 - Sep 6, 2011
 *
 *     - Disable spinlocking/yielding, next port will use kthreads
 *
 * Supported Linksys devices:
 *
 *   Developed and tested on WRT54GL V1.1. Should work on the majority of WRT54G/GS/GL devices.
 *   Reported working on:
 *
 *     - WRT54GL V1.1
 *
 *============================================================================*/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/errno.h>
#include <linux/types.h>

#include <linux/genhd.h>
#include <linux/blkdev.h>

#include <linux/hdreg.h> /* for struct hd_geometry */

#include <linux/fs.h>
#include <linux/proc_fs.h>

#include <linux/slab.h>

#include "bcm_sdhc.h"

// Methods for driving the spi bus
#include "spi.c"

// Methods for initializing the cards
#include "init.c"

MODULE_LICENSE ( "GPL" );
MODULE_AUTHOR ( "Severn Tsui/madsuk/Rohde/Cyril CATTIAUX/Marc DENTY/rcichielo KRUSCH/Chris" );
MODULE_DESCRIPTION ( "SD/SDHC/MMC Card Block Driver - " MOD_VERSION );
MODULE_SUPPORTED_DEVICE ( "WRT54GL" );
MODULE_VERSION ( MOD_VERSION );

// Set up module parameters
module_param ( major, int, 0444 );
MODULE_PARM_DESC ( major, "Major device number to use" );
module_param ( din, int, 0444 );
MODULE_PARM_DESC ( din, "Gpio to assign to DI pin" );
module_param ( dout, int, 0444 );
MODULE_PARM_DESC ( dout, "Gpio to assign to DO pin" );
module_param ( clk, int, 0444 );
MODULE_PARM_DESC ( clk, "Gpio to assign to CLK pin" );
module_param ( cs, int, 0444 );
MODULE_PARM_DESC ( cs, "Gpio to assign to CS pin" );
module_param ( maxsec, int, 0444 );
MODULE_PARM_DESC ( maxsec, "Max sectors per request" );
module_param ( gpio_input, int, 0444 );
MODULE_PARM_DESC ( gpio_input, "Address of gpio input register" );
module_param ( gpio_output, int, 0444 );
MODULE_PARM_DESC ( gpio_output, "Address of gpio output register" );
module_param ( gpio_enable, int, 0444 );
MODULE_PARM_DESC ( gpio_enable, "Address of gpio enable register" );
module_param ( gpio_control, int, 0444 );
MODULE_PARM_DESC ( gpio_control, "Address of gpio control register" );

#ifdef DEBUG
module_param ( dbg, int, 0444 );
MODULE_PARM_DESC ( dbg, "Control debugging output" );
#endif

static struct request_queue * Queue;

static struct bcm_sdhc_device {
    unsigned long size;
    unsigned long nr_sects;
    unsigned int sectorSize;
    spinlock_t lock;
    
    unsigned char in_request;
    spinlock_t req_lock;

    struct gendisk *gd;
} Device;

// Support for /proc filesystem
static ssize_t bcm_sdhc_proc_read ( struct file * file, char *buf, size_t count, loff_t * ppos );

static struct proc_dir_entry *sdhcd;
static struct file_operations bcm_sdhc_proc_fops = {
    .read = bcm_sdhc_proc_read,
    .write = NULL,
};

static int bcm_sdhc_getgeo ( struct block_device * block_device, struct hd_geometry * geo )
{
    // We have no real geometry, so make something up that makes sense
    geo->cylinders = Device.nr_sects / ( 4*16 );
    geo->heads = 4;
    geo->sectors = 16;
    geo->start = 0;
    return 0;
}

static struct block_device_operations bcm_sdhc_bdops = {
    .owner  = THIS_MODULE,
    .getgeo = bcm_sdhc_getgeo,
};

static void bcm_sdhc_request ( struct request_queue * q )
{
    struct request * req = 0;

    unsigned long last_req_end_address = 0;
    unsigned char last_write = 0;
    unsigned char first = 1;
    /*
    spin_lock_irq ( &Device.req_lock );
    
    if ( Device.in_request )
    {
        // this function was called while a call was already in progress,
        // stop the queue until the it finishes
        blk_stop_queue ( Queue );
        spin_unlock_irq ( &Device.req_lock );
        return;
    }
    
    Device.in_request = 1;
    
    spin_unlock_irq ( &Device.req_lock );
*/
    if ( ! ( req = blk_fetch_request ( q ) ) )
        return;

    while ( req ) {
        unsigned int block_address = ( unsigned int ) blk_rq_pos ( req );
        unsigned int card_address = block_address;
        unsigned long nr_sectors = blk_rq_cur_sectors ( req );
        unsigned char write = rq_data_dir ( req );
        int res = -EIO;
        int r = -1;

        if ( req->cmd_type != REQ_TYPE_FS )
            goto done;

        if ( ! ( card->state & CARD_STATE_DET ) ) {
            LOG_ERR ( "Request: IO request but no card detected!\n" );
            goto done;
        }

        if ( ( block_address + nr_sectors ) > Device.nr_sects ) {
            LOG_ERR ( "Request: Sector out of range!\n" );
            goto done;
        }

        if ( ! ( card->type & CARD_TYPE_HC ) )
            card_address <<= 9; // Multiply by 512 if byte addressing

        if ( first == 0 ) {
            // if this new request doesn't continue from the previous request, finish the previous request (send stop, wait busy)
            // before telling the new request to send command prior to reading/writing data.
            if ( write != last_write || block_address != last_req_end_address ) {
                if ( last_write ) {
                    r = spi_card_write_multi_o ( 0, 0, 0, 0, 1 );
                } else {
                    r = spi_card_read_multi_o ( 0, 0, 0, 0, 1 );
                }
                first = 1;
                LOG_DEBUG ( DBG_REQ, "last %s_multi_o: ba=%lu, err=%d\n", last_write ? "write" : "read", last_req_end_address, r );
            }
        }

        if ( first == 0 ) {
            LOG_DEBUG ( DBG_REQ, "%s_multi_o: nr_sec=%lu, ba=%u\n", write ? "write" : "read", nr_sectors, block_address );
        } else {
            LOG_DEBUG ( DBG_REQ, "first %s_multi_o: nr_sec=%lu, ba=%u\n", write ? "write" : "read", nr_sectors, block_address );
        }

        if ( write ) {
            r = spi_card_write_multi_o ( card_address, req->buffer, nr_sectors, first, 0 );
        } else {
            r = spi_card_read_multi_o ( card_address, req->buffer, nr_sectors, first, 0 );
        }

        if ( r != 0 ) {
            LOG_ERR ( "Request to %s failed. ba=%u, ca=%u, nr_sec=%lu, err=%d\n",
                      write ? "write" : "read",
                      block_address, card_address, nr_sectors, r );

            if ( r == 3 ) {
                // didn't receive data or data token. Try to recover by ending current request.
                if ( write ) {
                    r = spi_card_write_multi_o ( 0, 0, 0, 0, 1 );
                } else {
                    r = spi_card_read_multi_o ( 0, 0, 0, 0, 1 );
                }
                LOG_DEBUG ( DBG_REQ, "finish fail %s_multi_o: ba=%u, err=%d\n", last_write ? "write" : "read", block_address, r );
            }
            first = 1;
        } else {
            last_req_end_address = block_address + nr_sectors;
            last_write = write;
            first = 0;
            res = 0;
            LOG_DEBUG ( DBG_REQ, "endreq %s_multi_o: ba=%lu, err=%d\n", last_write ? "write" : "read", last_req_end_address, r );
        }

    done:
        /* wrap up, 0 = success, -errno = fail */
        if ( !__blk_end_request_cur ( req, res ) )
            req = blk_fetch_request ( q );
    }

    if ( first == 0 ) {
        int r = -1;
        if ( last_write ) {
            r = spi_card_write_multi_o ( 0, 0, 0, 0, 1 );
        } else {
            r = spi_card_read_multi_o ( 0, 0, 0, 0, 1 );
        }
        LOG_DEBUG ( DBG_REQ, "finish %s_multi_o: ba=%lu, err=%d\n", last_write ? "write" : "read", last_req_end_address, r );
    }
/*
    spin_lock_irq ( &Device.req_lock );
    Device.in_request = 0;
    spin_unlock_irq ( &Device.req_lock );
    
    blk_start_queue ( Queue );
    */
}

static ssize_t bcm_sdhc_proc_read ( struct file * file, char *buf, size_t count, loff_t *ppos )
{
    // Following are used for mapping bit patterns to strings for printing
    static char * typ_to_str[] = { "", "SD", "MM" }; // SD/MM
    static char * cap_to_str[] = { "", "HC" };   // std/high capacity
    static char * sdv_to_str[] = { "1.00 - 1.01", "1.10", "2.0" };  // SD version
    static char * mmv_to_str[] = { "1.0 - 1.2", "1.4", "2.0 - 2.2", "3.0 - 3.3", "4.1 - 4.3" }; // MM version
    static char * mon_to_str[] = { "", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    char *page;
    int len = 0;
    ( void ) file;

    if ( ( page = kmalloc ( 1024, GFP_KERNEL ) ) == NULL )
        return -ENOBUFS;

    // calc min/max voltage (times 10) from the OCR voltage window bits.
    unsigned int voltmin = 0;
    unsigned int voltmax = 0;
    unsigned int j;

    for ( j = 4; j < 24 ; j++ ) {
        if ( ( 0x00000001 << j ) & card->volt ) {
            voltmax = j + 13;
            voltmin = ( voltmin == 0 ) ? j + 12 : voltmin;
        }
    }

    len = sprintf ( page,
                    "Card Type      : %s%s\n"
                    "Spec Version   : %s\n"
                    "Card Size      : %d MB\n"
                    "Block Size     : 512 bytes\n"
                    "Num. of Blocks : %d\n"
                    "Voltage Range  : %d.%d-%d.%d\n"
                    "Manufacture ID : %02x\n"
                    "Application ID : %.2s\n"
                    "Product Name   : %.6s\n"
                    "Revision       : %d.%d\n"
                    "Serial Number  : %08x\n"
                    "Manu. Date     : %s %d\n",
                    typ_to_str[card->type >> 4], cap_to_str[card->type & 0x0f],
                    ( card->type & CARD_TYPE_SD ) ? sdv_to_str[card->version] : mmv_to_str[card->version],
                    card->blocks / 2 / 1024,
                    card->blocks,
                    voltmin/10, voltmin%10, voltmax/10, voltmax%10,
                    card->manid,
                    card->appid,
                    card->name,
                    ( card->rev >> 4 ), ( card->rev & 0x0f ),
                    card->serial,
                    mon_to_str[card->month],card->year
                  );

    len += 1;

    if ( *ppos < len ) {
        len = min_t ( int, len - *ppos, count );

        if ( copy_to_user ( buf, ( page + *ppos ), len ) ) {
            kfree ( page );
            return -EFAULT;
        }

        *ppos += len;
    } else {
        len = 0;
    }

    kfree ( page );

    return len;
}

static int __init bcm_sdhc_init ( void )
{
    int ret;

    static struct proc_dir_entry *p;

    // Log the module version and parameters
#ifdef DEBUG
    LOG_INFO ( "Version: " MOD_VERSION "  Params: major=%d din=%d dout=%d clk=%d cs=%d maxsec=%d dbg=%04x\n", major, din, dout, clk, cs, maxsec, dbg );
#else
    LOG_INFO ( "Version: " MOD_VERSION "  Params: major=%d din=%d dout=%d clk=%d cs=%d maxsec=%d \n", major, din, dout, clk, cs, maxsec );
#endif

    // Initialize spi module - set gpio pins used
    spi_init();

    // Try and initialize card - returns a card info structure on success.
    if ( ( ret = init_card ( &card ) ) ) {
        // Give it one more shot!
        if ( card )
            kfree ( card );

        if ( ( ret = init_card ( &card ) ) )
            goto err1;
    }

    if ( card->blocks == 0 )
        goto err1;

    // Initialize the /proc entries for this module
    if ( ! ( sdhcd = proc_mkdir ( MOD_NAME, NULL ) ) ) {
        LOG_ERR ( "Failure creating /proc/%s\n", MOD_NAME );
        ret = -EPROTO;
        goto err1;
    }

    if ( ! ( p = create_proc_entry ( "status", S_IRUSR, sdhcd ) ) ) {
        LOG_ERR ( "Failure creating /proc/%s/status\n", MOD_NAME );
        ret = -EPROTO;
        goto err1;
    }

    p->proc_fops = &bcm_sdhc_proc_fops;

    ret = register_blkdev ( major, DEVICE_NAME );

    if ( ret < 0 ) {
        LOG_ERR ( "Failure requesting major %d - rc=%d\n", major, ret );
        goto err2;
    }

    if ( major == 0 ) {
        major = ret;
        LOG_INFO ( "Assigned dynamic major number %d\n", major );
    }

    spin_lock_init ( &Device.req_lock );
    Device.in_request = 0;
    
    spin_lock_init ( &Device.lock );
    Queue = blk_init_queue ( bcm_sdhc_request, &Device.lock );

    if ( Queue == NULL ) {
        LOG_ERR ( "Failed to initialize device queue" );
        ret = -ENOMEM;
        goto err3;
    }

    Device.sectorSize = 512;
    Device.nr_sects = card->blocks;
    Device.size = Device.nr_sects * Device.sectorSize;

    blk_queue_logical_block_size ( Queue, Device.sectorSize );
    blk_queue_max_hw_sectors ( Queue, maxsec );

    Device.gd = alloc_disk ( 16 );
    Device.gd->major = major;
    Device.gd->first_minor = 0;
    Device.gd->flags = 0;
    Device.gd->fops = &bcm_sdhc_bdops;
    Device.gd->private_data = &Device;
    Device.gd->queue = Queue;
    strcpy ( Device.gd->disk_name, DEVICE_NAME );

    set_capacity ( Device.gd, Device.nr_sects );
    add_disk ( Device.gd );

    LOG_INFO ( "Module loaded\n" );

    return 0;

    /* Error handling */
err3:
    unregister_blkdev ( major, DEVICE_NAME );

err2:
    remove_proc_entry ( "status", sdhcd );
    remove_proc_entry ( MOD_NAME, NULL );

err1:
    if ( card )
        kfree ( card );

    LOG_INFO ( "Module unloaded due to error\n" );

    return ret;
}
module_init ( bcm_sdhc_init );

static void __exit bcm_sdhc_exit ( void )
{
    int i;

    if ( major != 0 ) {
        for ( i = 0; i < 16; i++ )
            fsync_bdev ( bdget ( MKDEV ( major, i ) ) );
    }

    unregister_blkdev ( major, DEVICE_NAME );

    del_gendisk ( Device.gd );
    put_disk ( Device.gd );

    blk_cleanup_queue ( Queue );

    remove_proc_entry ( "status", sdhcd );
    remove_proc_entry ( MOD_NAME, NULL );

    if ( card )
        kfree ( card );

    LOG_INFO ( "Module unloaded\n" );
}
module_exit ( bcm_sdhc_exit );
