// Ports library definitions
// 2009-02-13 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#ifndef Ports_h
#define Ports_h

#if ARDUINO>=100
#include <Arduino.h> // Arduino 1.0
#else
#include <Wprogram.h> // Arduino 0022
#endif
#include <stdint.h>
#include <avr/pgmspace.h>
//#include <util/delay.h>

// keep the ATtiny85 on the "old" conventions until arduino-tiny gets fixed
#if ARDUINO >= 100 && !defined(__AVR_ATtiny84__) && !defined(__AVR_ATtiny85__)
#define WRITE_RESULT size_t
#else
#define WRITE_RESULT void
#endif

class Port {
protected:
    uint8_t portNum;

#if defined(__AVR_ATtiny85__)
    inline uint8_t digiPin() const
        { return 0; }
    inline uint8_t digiPin2() const
        { return 2; }
    static uint8_t digiPin3()
        { return 1; }
    inline uint8_t anaPin() const
        { return 0; }
#else
    inline uint8_t digiPin() const
        { return portNum ? portNum + 3 : 18; }
    inline uint8_t digiPin2() const
        { return portNum ? portNum + 13 : 19; }
    static uint8_t digiPin3()
        { return 3; }
    inline uint8_t anaPin() const
        { return portNum - 1; }
#endif

public:
    inline Port (uint8_t num) : portNum (num) {}

    // DIO pin
    inline void mode(uint8_t value) const
        { pinMode(digiPin(), value); }
    inline uint8_t digiRead() const
        { return digitalRead(digiPin()); }
    inline void digiWrite(uint8_t value) const
        { return digitalWrite(digiPin(), value); }
    inline void anaWrite(uint8_t val) const
        { analogWrite(digiPin(), val); }
    inline uint32_t pulse(uint8_t state, uint32_t timeout =1000000L) const
        { return pulseIn(digiPin(), state, timeout); }
    
    // AIO pin
    inline void mode2(uint8_t value) const
        { pinMode(digiPin2(), value); }
    inline uint16_t anaRead() const
        { return analogRead(anaPin()); }        
    inline uint8_t digiRead2() const
        { return digitalRead(digiPin2()); }
    inline void digiWrite2(uint8_t value) const
        { return digitalWrite(digiPin2(), value); }
    inline uint32_t pulse2(uint8_t state, uint32_t timeout =1000000L) const
        { return pulseIn(digiPin2(), state, timeout); }
        
    // IRQ pin (INT1, shared across all ports)
    static void mode3(uint8_t value)
        { pinMode(digiPin3(), value); }
    static uint8_t digiRead3()
        { return digitalRead(digiPin3()); }
    static void digiWrite3(uint8_t value)
        { return digitalWrite(digiPin3(), value); }
    static void anaWrite3(uint8_t val)
        { analogWrite(digiPin3(), val); }
        
    // both pins: data on DIO, clock on AIO
    inline void shift(uint8_t bitOrder, uint8_t value) const
        { shiftOut(digiPin(), digiPin2(), bitOrder, value); }
    uint16_t shiftRead(uint8_t bitOrder, uint8_t count =8) const;
    void shiftWrite(uint8_t bitOrder, uint16_t value, uint8_t count =8) const;
};

class RemoteNode {
public: 
    typedef struct {
        uint8_t flags, modes, digiIO, anaOut[2];
        uint16_t anaIn[4]; // only bits 0..11 used
    } Data;

    RemoteNode (char id, uint8_t band, uint8_t group =0);
    
    void poll(uint16_t msecs);

    friend class RemoteHandler;
    friend class RemotePort;
private:
    uint8_t nid;
    uint32_t lastPoll;
    Data data;
};

class RemoteHandler {
public:
    static void setup(uint8_t id, uint8_t band, uint8_t group =0);
    static uint8_t poll(RemoteNode& node, uint8_t send);
};

class RemotePort : protected Port {
    RemoteNode& node;

    inline uint8_t pinBit() const
        { return portNum - 1; }
    inline uint8_t pinBit2() const
        { return portNum + 3; }
public:
    RemotePort (RemoteNode& remote, uint8_t num) : Port (num), node (remote) {}

    void mode(uint8_t value) const;
    uint8_t digiRead() const;
    void digiWrite(uint8_t value) const;
    void anaWrite(uint8_t val) const;
    
    void mode2(uint8_t value) const;    
    uint16_t anaRead() const;
    uint8_t digiRead2() const;
    void digiWrite2(uint8_t value) const;    
};

class PortI2C : public Port {
    uint8_t uswait;
    
#if 0
// speed test with fast hard-coded version for Port 1:
    inline void hold() const
        { _delay_us(1); }
    inline void sdaOut(uint8_t value) const
        { bitWrite(DDRD, 4, !value); bitWrite(PORTD, 4, value); }
    inline uint8_t sdaIn() const
        { return bitRead(PORTD, 4); }
    inline void sclHi() const
        { hold(); bitWrite(PORTC, 0, 1); }
    inline void sclLo() const
        { hold(); bitWrite(PORTC, 0, 0); }
#else
    inline void hold() const
        { delayMicroseconds(uswait); }
    inline void sdaOut(uint8_t value) const
        { mode(!value); digiWrite(value); }
    inline uint8_t sdaIn() const
        { return digiRead(); }
    inline void sclHi() const
        { hold(); digiWrite2(1); }
    inline void sclLo() const
        { hold(); digiWrite2(0); }
#endif
public:
    enum { KHZMAX, KHZ400, KHZ100, KHZ_SLOW };
    
    PortI2C (uint8_t num, uint8_t rate =KHZMAX);
    
    uint8_t start(uint8_t addr) const;
    void stop() const;
    uint8_t write(uint8_t data) const;
    uint8_t read(uint8_t last) const;
};

class DeviceI2C {
    const PortI2C& port;
    uint8_t addr;
    
public:
    DeviceI2C(const PortI2C& p, uint8_t me) : port (p), addr (me << 1) {}
    
    bool isPresent() const;
    
    uint8_t send() const
        { return port.start(addr); }
    uint8_t receive() const
        { return port.start(addr | 1); }
    void stop() const
        { port.stop(); }
    uint8_t write(uint8_t data) const
        { return port.write(data); }
    uint8_t read(uint8_t last) const
        { return port.read(last); }
        
    uint8_t setAddress(uint8_t me)
        { addr = me << 1; }
};

// The millisecond timer can be used for timeouts up to 60000 milliseconds.
// Setting the timeout to zero disables the timer.
//
// for periodic timeouts, poll the timer object with "if (timer.poll(123)) ..."
// for one-shot timeouts, call "timer.set(123)" and poll as "if (timer.poll())"

class MilliTimer {
    word next;
    byte armed;
public:
    MilliTimer () : armed (0) {}
    
    byte poll(word ms =0);
    word remaining() const;
    byte idle() const { return !armed; }
    void set(word ms);
};

// Low-power utility code using the Watchdog Timer (WDT). Requires a WDT interrupt handler, e.g.
// EMPTY_INTERRUPT(WDT_vect);
class Sleepy {
public:
    // start the watchdog timer (or disable it if mode < 0)
    static void watchdogInterrupts (char mode);
    
    // enter low-power mode, wake up with watchdog, INT0/1, or pin-change
    static void powerDown (byte prrOff =0xFF);
    
    // spend some time in low-power mode, the timing is only approximate
    // returns 1 if all went normally, or 0 if some other interrupt occurred
    static byte loseSomeTime (word msecs);

    // this must be called from your watchdog interrupt code
    static void watchdogEvent();
};

// simple task scheduler for times up to 6000 seconds
class Scheduler {
    word* tasks;
    word remaining;
    byte maxTasks;
    MilliTimer ms100;
public:
    // initialize for a specified maximum number of tasks
    Scheduler (byte max);
    Scheduler (word* buf, byte max);

    // return next task to run, -1 if there are none ready to run, but there are tasks waiting, or -2 if there are no tasks waiting (i.e. all are idle)
    char poll();
    // same as poll, but wait for event in power-down mode.
    // Uses Sleepy::loseSomeTime() - see comments there re requiring the watchdog timer. 
    char pollWaiting();
    
    // set a task timer, in tenths of seconds
    void timer(byte task, word tenths);
    // cancel a task timer
    void cancel(byte task);
    
    // return true if a task timer is not running
    byte idle(byte task) { return tasks[task] == ~0; }
};

// interface for the Blink Plug - see http://jeelabs.org/bp1
class BlinkPlug : public Port {
    MilliTimer debounce;
    byte leds, lastState, checkFlags;
public:
    enum { ALL_OFF, ON1, OFF1, ON2, OFF2, SOME_ON, ALL_ON }; // for buttonCheck
    
    BlinkPlug (byte port)
        : Port (port), leds (0), lastState (0), checkFlags (0) {}
    
    void ledOn(byte mask);
    void ledOff(byte mask);
    byte state();
    byte pushed(); // deprecated, don't use in combination with buttonCheck
    byte buttonCheck();
};

// interface for the Memory Plug - see http://jeelabs.org/mp1
class MemoryPlug : public DeviceI2C {
    uint32_t nextSave;
public:
    MemoryPlug (PortI2C& port)
        : DeviceI2C (port, 0x50), nextSave (0) {}

    void load(word page, void* buf, byte offset =0, int count =256);
    void save(word page, const void* buf, byte offset =0, int count =256);
};

class MemoryStream {
    MemoryPlug& dev;
    word start, curr;
    char step;
    byte buffer[256], pos;
public:
    MemoryStream (MemoryPlug& plug, word page =0, char dir =1)
            : dev (plug), start (page), curr (page), step (dir), pos (0) {}
    
    long position(byte writing) const;
    byte get();
    void put(byte data);
    word flush();
    void reset();
};

// interface for the UART Plug - see http://jeelabs.org/up1
class UartPlug : public Print {
    DeviceI2C dev;
    // avoid per-byte access, fill entire buffer instead to reduce I2C overhead
    byte rxbuf[20], in, out;

    void regSet (byte reg, byte value);
    void regRead (byte reg);
    
public:
    UartPlug (PortI2C& port, byte addr)
        : dev (port, addr), in (0), out (0) {}
        
    void begin(long);
    byte available();
    int read();
    void flush();
    virtual WRITE_RESULT write(byte);
};

// interface for the Dimmer Plug - see http://jeelabs.org/dp1
class DimmerPlug : public DeviceI2C {
public:
    enum {
        MODE1, MODE2,
        PWM0, PWM1, PWM2, PWM3, PWM4, PWM5, PWM6, PWM7,
        PWM8, PWM9, PWM10, PWM11, PWM12, PWM13, PWM14, PWM15,
        GRPPWM, GRPFREQ,
        LEDOUT0, LEDOUT1, LEDOUT2, LEDOUT3,
        SUBADR1, SUBADR2, SUBADR3, ALLCALLADR,
    };

    DimmerPlug (PortI2C& port, byte addr)
        : DeviceI2C (port, addr) {}
    
    void begin ();
    byte getReg(byte reg) const;
    void setReg(byte reg, byte value) const;
    void setMulti(byte reg, ...) const;
};

// interface for the Lux Plug - see http://jeelabs.org/xp1
class LuxPlug : public DeviceI2C {
    union { byte b[4]; word w[2]; } data;
public:
    enum {
        CONTROL, TIMING,
        THRESHLOWLOW, THRESHLOWHIGH, THRESHHIGHLOW, THRESHHIGHHIGH, INTERRUPT,
        LUXID = 0xA,
        DATA0LOW = 0xC, DATA0HIGH, DATA1LOW, DATA1HIGH,
    };

    LuxPlug (PortI2C& port, byte addr) : DeviceI2C (port, addr) {}
    
    void begin() {
        send();
        write(0xC0 | CONTROL);
        write(3); // power up
        stop();
    }
    
    void setGain(byte high);
    
    const word* getData();

    word calcLux(byte iGain =0, byte tInt =2) const;
};

// interface for the Gravity Plug - see http://jeelabs.org/gp1
class GravityPlug : public DeviceI2C {
    union { byte b[6]; int w[3]; } data;
public:
    GravityPlug (PortI2C& port) : DeviceI2C (port, 0x38) {}
    
    void begin() {}
    
    const int* getAxes();
};

// interface for the Input Plug - see http://jeelabs.org/ip1
class InputPlug : public Port {
    uint8_t slow;
public:
    InputPlug (uint8_t num, uint8_t fix =0) : Port (num), slow (fix) {}
    
    void select(uint8_t channel);
};

// interface for the Infrared Plug - see http://jeelabs.org/ir1
class InfraredPlug : public Port {
    uint8_t slot, gap, buf [40];
    char fill;
    uint32_t prev;
public:
    // initialize with default values for NEC protocol
    InfraredPlug (uint8_t num);
    
    // set slot size (us*4) and end-of-data gap (us*256)
    void configure(uint8_t slot4, uint8_t gap256 =80);
    
    // call this continuously or at least right after a pin change
    void poll();
    
    // returns number of nibbles read, or 0 if not yet ready
    uint8_t done();
    
    // try to decode a received packet, return type of packet
    // if recognized, the receive buffer will be overwritten with the results
    enum { UNKNOWN, NEC, NEC_REP };
    uint8_t decoder(uint8_t nibbles);
    
    // access to the receive buffer
    const uint8_t* buffer() { return buf; }
    
    // send out a bit pattern, cycle time is the "slot4" config value
    void send(const uint8_t* data, uint16_t bits);
};

// interface for the Heading Board - see http://jeelabs.org/hb1
class HeadingBoard : public PortI2C {
    DeviceI2C eeprom, adc, compass;
    Port aux;
    // keep following fields in order:
    word C1, C2, C3, C4, C5, C6, C7;
    byte A, B, C, D, setReset;

    byte eepromByte(byte reg) const;
    void getConstants();
    word adcValue(byte press) const;

public:
    HeadingBoard (int num)
        : PortI2C (num), eeprom (*this, 0x50), adc (*this, 0x77),
          compass (*this, 0x30), aux (5-num), setReset (0x02) {}
    
    void begin();
    void pressure(int& temp, int& pres) const;
    void heading(int& xaxis, int& yaxis);
};

// interface for the Proximity Plug - see http://jeelabs.org/yp1
class ProximityPlug : public DeviceI2C {
public:
    enum {
        FIFO, FAULT, TPSTATUS, TPCONFIG,
        STR1, STR2, STR3, STR4, STR5, STR6, STR7, STR8, 
        ECEMR, MNTPR, MTPR, TASPR, SCR, LPCR, SKTR,
        CONFIG, SINFO,
    };

    ProximityPlug (PortI2C& port, byte num =0)
        : DeviceI2C (port, 0x5C + num) {}
    
    void begin();
    
    void setReg(byte reg, byte value) const;
    byte getReg(byte reg) const;
};

#ifdef Stream_h // only available in recent Arduino IDE versions

// simple parser for input data and one-letter commands
class InputParser {
public:
    typedef struct {
        char code;      // one-letter command code
        byte bytes;     // number of bytes required as input
        void (*fun)();  // code to call for this command
    } Commands;
    
    // set up with a buffer of specified size
    InputParser (byte size, Commands PROGMEM*, Stream& =Serial);
    InputParser (byte* buf, byte size, Commands PROGMEM*, Stream& =Serial);
    
    // number of data bytes
    byte count() { return fill; }
    
    // call this frequently to check for incoming data
    void poll();
    
    InputParser& operator >> (char& v)      { return get(&v, 1); }
    InputParser& operator >> (byte& v)      { return get(&v, 1); }
    InputParser& operator >> (int& v)       { return get(&v, 2); }
    InputParser& operator >> (word& v)      { return get(&v, 2); }
    InputParser& operator >> (long& v)      { return get(&v, 4); }
    InputParser& operator >> (uint32_t& v)  { return get(&v, 4); }
    InputParser& operator >> (const char*& v);

private:
    InputParser& get(void*, byte);
    void reset();
    
    byte *buffer, limit, fill, top, next;
    byte instring, hexmode, hasvalue;
    uint32_t value;
    Commands* cmds;
    Stream& io;
};

#endif // Stream_h

#endif
