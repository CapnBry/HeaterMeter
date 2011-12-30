// RFM12B driver definitions
// 2009-02-09 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#ifndef RF12_h
#define RF12_h

#include <stdint.h>

// version 1 did not include the group code in the crc
// version 2 does include the group code in the crc
#define RF12_VERSION    2

#define rf12_grp        rf12_buf[0]
#define rf12_hdr        rf12_buf[1]
#define rf12_len        rf12_buf[2]
#define rf12_data       (rf12_buf + 3)

#define RF12_HDR_CTL    0x80
#define RF12_HDR_DST    0x40
#define RF12_HDR_ACK    0x20
#define RF12_HDR_MASK   0x1F

#define RF12_MAXDATA    66

#define RF12_433MHZ     1
#define RF12_868MHZ     2
#define RF12_915MHZ     3

// EEPROM address range used by the rf12_config() code
#define RF12_EEPROM_ADDR ((uint8_t*) 0x20)
#define RF12_EEPROM_SIZE 32
#define RF12_EEPROM_EKEY (RF12_EEPROM_ADDR + RF12_EEPROM_SIZE)
#define RF12_EEPROM_ELEN 16

// shorthand to simplify sending out the proper ACK when requested
#define RF12_WANTS_ACK ((rf12_hdr & RF12_HDR_ACK) && !(rf12_hdr & RF12_HDR_CTL))
#define RF12_ACK_REPLY (rf12_hdr & RF12_HDR_DST ? RF12_HDR_CTL : \
            RF12_HDR_CTL | RF12_HDR_DST | (rf12_hdr & RF12_HDR_MASK))
            
// options for RF12_sleep()
#define RF12_SLEEP 0
#define RF12_WAKEUP -1

extern volatile uint16_t rf12_crc;  // running crc value, should be zero at end
extern volatile uint8_t rf12_buf[]; // recv/xmit buf including hdr & crc bytes
extern long rf12_seq;               // seq number of encrypted packet (or -1)

// only needed if you want to init the SPI bus before rf12_initialize does it
void rf12_spiInit(void);

// call this once with the node ID, frequency band, and optional group
uint8_t rf12_initialize(uint8_t id, uint8_t band, uint8_t group=0xD4);

// initialize the RF12 module from settings stored in EEPROM by "RF12demo"
// don't call rf12_initialize() if you init the hardware with rf12_config()
// returns the node ID as 1..31 value (1..26 correspond to nodes 'A'..'Z')
uint8_t rf12_config(uint8_t show =1);

// call this frequently, returns true if a packet has been received
uint8_t rf12_recvDone(void);

// call this to check whether a new transmission can be started
// returns true when a new transmission may be started with rf12_sendStart()
uint8_t rf12_canSend(void);

// call this only when rf12_recvDone() or rf12_canSend() return true
void rf12_sendStart(uint8_t hdr);
void rf12_sendStart(uint8_t hdr, const void* ptr, uint8_t len);
// deprecated: use rf12_sendStart(hdr,ptr,len) followed by rf12_sendWait(sync)
void rf12_sendStart(uint8_t hdr, const void* ptr, uint8_t len, uint8_t sync);

// wait for send to finish, sleep mode: 0=none, 1=idle, 2=standby, 3=powerdown
void rf12_sendWait(uint8_t mode);

// this simulates OOK by turning the transmitter on and off via SPI commands
// use this only when the radio was initialized with a fake zero node ID
void rf12_onOff(uint8_t value);

// power off the RF12, ms > 0 sets watchdog to wake up again after N * 32 ms
// note: once off, calling this with -1 can be used to bring the RF12 back up
void rf12_sleep(char n);

// returns true of the supply voltage is below 3.1V
char rf12_lowbat(void);

// set up the easy tranmission mode, arg is number of seconds between packets
void rf12_easyInit(uint8_t secs);

// call this often to keep the easy transmission mode going
char rf12_easyPoll(void);

// send new data using the easy transmission mode, buffer gets copied to driver
char rf12_easySend(const void* data, uint8_t size);

// enable encryption (null arg disables it again)
void rf12_encrypt(const uint8_t*);

// low-level control of the RFM12B via direct register access
// http://tools.jeelabs.org/rfm12b is useful for calculating these
uint16_t rf12_control(uint16_t cmd);

// See http://blog.strobotics.com.au/2009/07/27/rfm12-tutorial-part-3a/
// Transmissions are packetized, don't assume you can sustain these speeds! 
//
// Note - data rates are approximate. For higher data rates you may need to
// alter receiver radio bandwidth and transmitter modulator bandwidth.
// Note that bit 7 is a prescaler - don't just interpolate rates between
// RF12_DATA_RATE_3 and RF12_DATA_RATE_2.
enum rf12DataRates {
    RF12_DATA_RATE_CMD = 0xC600,
    RF12_DATA_RATE_9 = RF12_DATA_RATE_CMD | 0x02,  // Approx 115200 bps
    RF12_DATA_RATE_8 = RF12_DATA_RATE_CMD | 0x05,  // Approx  57600 bps
    RF12_DATA_RATE_7 = RF12_DATA_RATE_CMD | 0x06,  // Approx  49200 bps
    RF12_DATA_RATE_6 = RF12_DATA_RATE_CMD | 0x08,  // Approx  38400 bps
    RF12_DATA_RATE_5 = RF12_DATA_RATE_CMD | 0x11,  // Approx  19200 bps
    RF12_DATA_RATE_4 = RF12_DATA_RATE_CMD | 0x23,  // Approx   9600 bps
    RF12_DATA_RATE_3 = RF12_DATA_RATE_CMD | 0x47,  // Approx   4800 bps
    RF12_DATA_RATE_2 = RF12_DATA_RATE_CMD | 0x91,  // Approx   2400 bps
    RF12_DATA_RATE_1 = RF12_DATA_RATE_CMD | 0x9E,  // Approx   1200 bps
    RF12_DATA_RATE_DEFAULT = RF12_DATA_RATE_7,
};

#endif
