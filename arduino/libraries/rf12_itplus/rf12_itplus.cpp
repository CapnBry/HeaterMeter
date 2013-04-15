// RFM12B driver implementation
// 2009-02-09 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#include "rf12_itplus.h"
#include <avr/io.h>
#include <util/crc16.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <util/atomic.h>
#include <Arduino.h> // Arduino 1.0

// #define OPTIMIZE_SPI 1  // uncomment this to write to the RFM12B @ 8 Mhz

// pin change interrupts are currently only supported on ATmega328's
// #define PINCHG_IRQ 1    // uncomment this to use pin-change interrupts

#define RF_MAX   (RF12_MAXDATA)

// pins used for the RFM12B interface - yes, there *is* logic in this madness:
//
//  - leave RFM_IRQ set to the pin which corresponds with INT0, because the
//    current driver code will use attachInterrupt() to hook into that
//  - (new) you can now change RFM_IRQ, if you also enable PINCHG_IRQ - this
//    will switch to pin change interrupts instead of attach/detachInterrupt()
//  - use SS_DDR, SS_PORT, and SS_BIT to define the pin you will be using as
//    select pin for the RFM12B (you're free to set them to anything you like)
//  - please leave SPI_SS, SPI_MOSI, SPI_MISO, and SPI_SCK as is, i.e. pointing
//    to the hardware-supported SPI pins on the ATmega, *including* SPI_SS !

#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)

#define RFM_IRQ     2
#define SS_DDR      DDRB
#define SS_PORT     PORTB
#define SS_BIT      0

#define SPI_SS      53    // PB0, pin 19
#define SPI_MOSI    51    // PB2, pin 21
#define SPI_MISO    50    // PB3, pin 22
#define SPI_SCK     52    // PB1, pin 20

#elif defined(__AVR_ATmega644P__)

#define RFM_IRQ     10
#define SS_DDR      DDRB
#define SS_PORT     PORTB
#define SS_BIT      4

#define SPI_SS      4
#define SPI_MOSI    5
#define SPI_MISO    6
#define SPI_SCK     7

#elif defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny44__)

#define RFM_IRQ     2
#define SS_DDR      DDRB
#define SS_PORT     PORTB
#define SS_BIT      1

#define SPI_SS      1     // PB1, pin 3
#define SPI_MISO    4     // PA6, pin 7
#define SPI_MOSI    5     // PA5, pin 8
#define SPI_SCK     6     // PA4, pin 9

#elif defined(__AVR_ATmega32U4__) //Arduino Leonardo 

#define RFM_IRQ     0	    // PD0, INT0, Digital3 
#define SS_DDR      DDRB
#define SS_PORT     PORTB
#define SS_BIT      6	    // Dig10, PB6

#define SPI_SS      17    // PB0, pin 8, Digital17
#define SPI_MISO    14    // PB3, pin 11, Digital14
#define SPI_MOSI    16    // PB2, pin 10, Digital16
#define SPI_SCK     15    // PB1, pin 9, Digital15

#else

// ATmega168, ATmega328, etc.
#define RFM_IRQ     2
#define SS_DDR      DDRB
#define SS_PORT     PORTB
#define SS_BIT      2     // for PORTB: 2 = d.10, 1 = d.9, 0 = d.8

#define SPI_SS      10    // PB2, pin 16
#define SPI_MOSI    11    // PB3, pin 17
#define SPI_MISO    12    // PB4, pin 18
#define SPI_SCK     13    // PB5, pin 19

#endif 

// RF12 command codes
#define RF_RECEIVER_ON  0x82DD
#define RF_XMITTER_ON   0x823D
#define RF_IDLE_MODE    0x820D
#define RF_SLEEP_MODE   0x8205
#define RF_WAKEUP_MODE  0x8207
#define RF_TXREG_WRITE  0xB800
#define RF_RX_FIFO_READ 0xB000
#define RF_WAKEUP_TIMER 0xE000

// RF12 status bits
#define RF_LBD_BIT      0x0400
#define RF_RSSI_BIT     0x0100

// bits in the node id configuration byte
#define NODE_BAND       0xC0        // frequency band
#define NODE_ACKANY     0x20        // ack on broadcast packets if set
#define NODE_ID         0x1F        // id of this node, as A..Z or 1..31

#define TX29_GROUP      0xD4

// transceiver states, these determine what to do with each interrupt
enum {
    TXCRC, TXTAIL, TXDONE, TXIDLE,
    TXRECV,
    TXSYN1, TXSYN2,
};

static volatile uint8_t rxfill;     // number of data bytes in rf12_buf
static volatile int8_t rxstate;     // current transceiver state

volatile uint8_t rf12_crc;         // running crc value
volatile uint8_t rf12_buf[RF_MAX];  // recv/xmit buf, including hdr & crc bytes
volatile uint8_t rf12_len;
static uint8_t rf12_status;
static uint8_t drssi;

itplus_initial_t itplus_initial_cb;

const uint8_t drssi_dec_tree[][2] = {
/* Final states are 0, 2, 4, 6 */
 /* down, up */
    { 0, 0 },  // 0
    { 0, 2 },  // 1
    { 2, 2 },  // 2
    { 1, 5 },  // start value, 3
    { 4, 4 },  // 4
    { 4, 6 },  // 5
};

void rf12_spiInit () {
    bitSet(SS_PORT, SS_BIT);
    bitSet(SS_DDR, SS_BIT);
    digitalWrite(SPI_SS, 1);
    pinMode(SPI_SS, OUTPUT);
    pinMode(SPI_MOSI, OUTPUT);
    pinMode(SPI_MISO, INPUT);
    pinMode(SPI_SCK, OUTPUT);
#ifdef SPCR    
    SPCR = _BV(SPE) | _BV(MSTR);
#if F_CPU > 10000000
    // use clk/2 (2x 1/4th) for sending (and clk/8 for recv, see rf12_xferSlow)
    SPSR |= _BV(SPI2X);
#endif
#else
    // ATtiny
    USICR = bit(USIWM0);
#endif    
    pinMode(RFM_IRQ, INPUT);
    digitalWrite(RFM_IRQ, 1); // pull-up
}

static uint8_t rf12_byte (uint8_t out) {
#ifdef SPDR
    SPDR = out;
    // this loop spins 4 usec with a 2 MHz SPI clock
    while (!(SPSR & _BV(SPIF)))
        ;
    return SPDR;
#else
    // ATtiny
    USIDR = out;
    byte v1 = bit(USIWM0) | bit(USITC);
    byte v2 = bit(USIWM0) | bit(USITC) | bit(USICLK);
#if F_CPU <= 5000000
    // only unroll if resulting clock stays under 2.5 MHz
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
    USICR = v1; USICR = v2;
#else
    for (uint8_t i = 0; i < 8; ++i) {
        USICR = v1;
        USICR = v2;
    }
#endif
    return USIDR;
#endif
}

static uint16_t rf12_xferSlow (uint16_t cmd) {
    // slow down to under 2.5 MHz
#if F_CPU > 10000000
    bitSet(SPCR, SPR0);
#endif
    bitClear(SS_PORT, SS_BIT);
    uint16_t reply = rf12_byte(cmd >> 8) << 8;
    reply |= rf12_byte(cmd);
    bitSet(SS_PORT, SS_BIT);
#if F_CPU > 10000000
    bitClear(SPCR, SPR0);
#endif
    return reply;
}

#if OPTIMIZE_SPI
static uint16_t rf12_xfer (uint16_t cmd) {
    // writing can take place at full speed, even 8 MHz works
    bitClear(SS_PORT, SS_BIT);
    uint16_t reply = rf12_byte(cmd >> 8) << 8;
    reply |= rf12_byte(cmd);
    bitSet(SS_PORT, SS_BIT);
    return reply;
}
#else
#define rf12_xfer rf12_xferSlow
#endif

// access to the RFM12B internal registers with interrupts disabled
uint16_t rf12_control(uint16_t cmd) {
#ifdef EIMSK
    bitClear(EIMSK, INT0);
    uint16_t r = rf12_xferSlow(cmd);
    bitSet(EIMSK, INT0);
#else
    // ATtiny
    bitClear(GIMSK, INT0);
    uint16_t r = rf12_xferSlow(cmd);
    bitSet(GIMSK, INT0);
#endif
    return r;
}

#define CRC_POLY 0x31
uint8_t itplus_crc_update(uint8_t crc, uint8_t data)
{
  crc ^= data;
  for (uint8_t i=0; i<8; ++i)
  {
    if (crc & 0x80)
      crc = (crc << 1) ^ CRC_POLY;
    else
      crc <<= 1;
  }
  return crc;
}

static void rf12_setDrssi(uint8_t d)
{
    drssi = d;
    rf12_xfer(0x94A0 | drssi);
}

static void rf12_interrupt() {
    // a transfer of 2x 16 bits @ 2 MHz over SPI takes 2x 8 us inside this ISR
    // correction: now takes 2 + 8 µs, since sending can be done at 8 MHz
    rf12_status = rf12_xfer(0x0000) >> 8;
    
    if (rxstate == TXRECV) {
        uint8_t in = rf12_xferSlow(RF_RX_FIFO_READ);

        /*** DRSSI has a relatively long response time:
           0: The RSSI has been set for a while so trust this reading and adjust
              DRSSI either up or down toward the result
           3: By the third byte the DRSSI set from T0 should have taken effect
              Use this result as the RSSI. We set the threshold again but never
              get that result
        ***/
        if (rxfill % 3 == 0)
            rf12_setDrssi(drssi_dec_tree[drssi][rf12_status & (RF_RSSI_BIT >> 8)]);

        if (rxfill == 0)
        {
            // The first 4 bits should be the length of 
            // the data that follows in quartets (4 bitses)
            // Round up to the nearest byte
            rf12_len = ((in >> 4) + 2) * 4 / 8;
            if (rf12_len < 2 || rf12_len > RF_MAX)
                rf12_len = 2;
            if (itplus_initial_cb)
                itplus_initial_cb();
        }

        rf12_buf[rxfill++] = in;
        rf12_crc = itplus_crc_update(rf12_crc, in);

        if (rxfill == rf12_len)
            rf12_xfer(RF_IDLE_MODE);
    } else {
        uint8_t out;

        if (rxstate < 0) {
            uint8_t pos = rf12_len + rxstate++;
            out = rf12_buf[pos];
            rf12_crc = itplus_crc_update(rf12_crc, out);
        } else
            switch (rxstate++) {
                case TXSYN1: out = 0x2D; break;
                case TXSYN2: out = TX29_GROUP; rxstate = -rf12_len; break;
                case TXCRC:  out = rf12_crc; break;
                case TXDONE: rf12_xfer(RF_IDLE_MODE); // fall through
                default:     out = 0xAA;
            }
            
        rf12_xfer(RF_TXREG_WRITE + out);
    }
}

#if PINCHG_IRQ
    #if RFM_IRQ < 8
        ISR(PCINT2_vect) {
            while (!bitRead(PIND, RFM_IRQ))
                rf12_interrupt();
        }
    #elif RFM_IRQ < 14
        ISR(PCINT0_vect) { 
            while (!bitRead(PINB, RFM_IRQ - 8))
                rf12_interrupt();
        }
    #else
        ISR(PCINT1_vect) {
            while (!bitRead(PINC, RFM_IRQ - 14))
                rf12_interrupt();
        }
    #endif
#endif

static void rf12_recvStart () {
    rxfill = 0;
    rf12_crc = 0;
    rf12_len = 0xff;
    rxstate = TXRECV;
    ATOMIC_BLOCK(ATOMIC_FORCEON)
    {
      rf12_setDrssi(3);
      rf12_xfer(RF_RECEIVER_ON);
    }
}

uint8_t rf12_recvDone () {
    if (rxstate == TXRECV && rf12_len == rxfill) {
        rxstate = TXIDLE;
        return 1;
    }
    if (rxstate == TXIDLE)
        rf12_recvStart();
    return 0;
}

uint8_t rf12_canSend () {
    // no need to test with interrupts disabled: state TXRECV is only reached
    // outside of ISR and we don't care if rxfill jumps from 0 to 1 here
    if (rxstate == TXRECV && rxfill == 0) {
        ATOMIC_BLOCK(ATOMIC_FORCEON)
        {
          rf12_xfer(RF_IDLE_MODE); // stop receiver
          rxstate = TXIDLE;
        }
        return 1;
    }
    return 0;
}

void rf12_sendStart() {
    ATOMIC_BLOCK(ATOMIC_FORCEON)
    {
      rf12_crc = 0;
      rxstate = TXSYN1;
      rf12_xfer(RF_XMITTER_ON); // bytes will be fed via interrupts
    }
}

void rf12_sendStart (const void* ptr, uint8_t len) {
    rf12_len = len;
    memcpy((void*)rf12_buf, ptr, len);
    rf12_sendStart();
}

void rf12_sendWait (uint8_t mode) {
    // wait for packet to actually finish sending
    // go into low power mode, as interrupts are going to come in very soon
    while (rxstate != TXIDLE)
        if (mode) {
            // power down mode is only possible if the fuses are set to start
            // up in 258 clock cycles, i.e. approx 4 us - else must use standby!
            // modes 2 and higher may lose a few clock timer ticks
            set_sleep_mode(mode == 3 ? SLEEP_MODE_PWR_DOWN :
#ifdef SLEEP_MODE_STANDBY
                           mode == 2 ? SLEEP_MODE_STANDBY :
#endif
                                       SLEEP_MODE_IDLE);
            sleep_mode();
        }
}

/*!
  Call this once with the node ID (0-31), frequency band (0-3)
*/
void rf12_initialize (uint8_t band) {
    rf12_spiInit();

    rf12_xfer(0x0000); // intitial SPI transfer added to avoid power-up problem

    rf12_xfer(RF_SLEEP_MODE); // DC (disable clk pin), enable lbd
    
    // wait until RFM12B is out of power-up reset, this takes several *seconds*
    rf12_xfer(RF_TXREG_WRITE); // in case we're still in OOK mode
    while (digitalRead(RFM_IRQ) == 0)
        rf12_xfer(0x0000);
        
    rf12_xfer(0x80C7 | (band << 4)); // EL (ena TX), EF (ena RX FIFO), 12.0pF
    switch (band)
    {
        case RF12_868MHZ: rf12_xfer(0xA67C); break;// FREQUENCY 868.300MHz
        case RF12_915MHZ: rf12_xfer(0xA7D0); break;// FREQUENCY 915.000MHz
        default: rf12_xfer(0xA640); // ???
    }
    //rf12_xfer(0xC606); // approx 49.2 Kbps, i.e. 10000/29/(1+6) Kbps
    rf12_xfer(0xC613); // DATA RATE 17.241 kbps
    rf12_xfer(0x94A2); // VDI,FAST,134kHz,0dBm,-91dBm
    rf12_xfer(0xC2AC); // AL,!ml,DIG,DQD4 
    rf12_xfer(0xCA83); // FIFO8,2-SYNC,!ff,DR 
    rf12_xfer(0xCE00 | TX29_GROUP); // SYNC=2DXX； 
    rf12_xfer(0xC483); // @PWR,NO RSTRIC,!st,!fi,OE,EN 
    rf12_xfer(0x9850); // !mp,90kHz,MAX OUT 
    rf12_xfer(0xCC77); // OB1，OB0, LPX,！ddy，DDIT，BW0 
    rf12_xfer(0xE000); // NOT USE 
    rf12_xfer(0xC800); // NOT USE 
    rf12_xfer(0xC049); // 1.66MHz,3.1V 

    rxstate = TXIDLE;
#if PINCHG_IRQ
    #if RFM_IRQ < 8
        if ((nodeid & NODE_ID) != 0) {
            bitClear(DDRD, RFM_IRQ);      // input
            bitSet(PORTD, RFM_IRQ);       // pull-up
            bitSet(PCMSK2, RFM_IRQ);      // pin-change
            bitSet(PCICR, PCIE2);         // enable
        } else
            bitClear(PCMSK2, RFM_IRQ);
    #elif RFM_IRQ < 14
        if ((nodeid & NODE_ID) != 0) {
            bitClear(DDRB, RFM_IRQ - 8);  // input
            bitSet(PORTB, RFM_IRQ - 8);   // pull-up
            bitSet(PCMSK0, RFM_IRQ - 8);  // pin-change
            bitSet(PCICR, PCIE0);         // enable
        } else
            bitClear(PCMSK0, RFM_IRQ - 8);
    #else
        if ((nodeid & NODE_ID) != 0) {
            bitClear(DDRC, RFM_IRQ - 14); // input
            bitSet(PORTC, RFM_IRQ - 14);  // pull-up
            bitSet(PCMSK1, RFM_IRQ - 14); // pin-change
            bitSet(PCICR, PCIE1);         // enable
        } else
            bitClear(PCMSK1, RFM_IRQ - 14);
    #endif
#else
    attachInterrupt(0, rf12_interrupt, LOW);
#endif
}

void rf12_sleep (char n) {
    if (n < 0)
        rf12_control(RF_IDLE_MODE);
    else {
        rf12_control(RF_WAKEUP_TIMER | 0x0500 | n);
        rf12_control(RF_SLEEP_MODE);
        if (n > 0)
            rf12_control(RF_WAKEUP_MODE);
    }
    rxstate = TXIDLE;
}

char rf12_lowbat () {
    return rf12_status & (RF_LBD_BIT >> 8);
}

char rf12_rssi () {
    //const int8_t table[] = {-109, -103, -97, -91, -85, -79, -73};
    return drssi / 2;
}
