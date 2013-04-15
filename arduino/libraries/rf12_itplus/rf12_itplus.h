// RFM12B driver definitions
// 2009-02-09 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#ifndef rf12_itplus_h
#define rf12_itplus_h

#include <stdint.h>

#define RF12_MAXDATA    10

#define RF12_433MHZ     1
#define RF12_868MHZ     2
#define RF12_915MHZ     3

// options for RF12_sleep()
#define RF12_SLEEP 0
#define RF12_WAKEUP -1

extern volatile uint8_t rf12_crc;  // running crc value, should be zero at end
extern volatile uint8_t rf12_buf[]; // recv/xmit buf including hdr & crc bytes
extern volatile uint8_t rf12_len;
extern volatile uint16_t rf_bad_status;

// only needed if you want to init the SPI bus before rf12_initialize does it
void rf12_spiInit(void);

// call this once with the frequency band
void rf12_initialize(uint8_t band);

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
void rf12_sendStart(void);
void rf12_sendStart(const void* ptr, uint8_t len);

// wait for send to finish, sleep mode: 0=none, 1=idle, 2=standby, 3=powerdown
void rf12_sendWait(uint8_t mode);

// power off the RF12, ms > 0 sets watchdog to wake up again after N * 32 ms
// note: once off, calling this with -1 can be used to bring the RF12 back up
void rf12_sleep(char n);

// returns nonzero if the supply voltage is below 3.1V
char rf12_lowbat(void);

// returns signal level 0 (weakest) to 3 (strongest)
char rf12_rssi(void);

// low-level control of the RFM12B via direct register access
// http://tools.jeelabs.org/rfm12b is useful for calculating these
uint16_t rf12_control(uint16_t cmd);

uint8_t itplus_crc_update(uint8_t crc, uint8_t data);

// Callback type used from the interrupt handler to indicate the first byte
// of a packet has been received. Called with interrupts disabled so be
// quick about it
typedef void (*itplus_initial_t)(void);
extern itplus_initial_t itplus_initial_cb;

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
