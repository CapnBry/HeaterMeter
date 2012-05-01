// Ports library definitions
// 2009-02-13 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#include "Ports.h"
#include <avr/sleep.h>
#include <util/atomic.h>

// #define DEBUG_DHT 1 // add code to send info over the serial port of non-zero

// ATtiny84 has BODS and BODSE for ATtiny84, revision B, and newer, even though
// the iotnx4.h header doesn't list it, so we *can* disable brown-out detection!
// See the ATtiny24/44/84 datasheet reference, section 7.2.1, page 34.
#if (defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny44__)) && !defined(BODSE) && !defined(BODS)
#define BODSE 2
#define BODS  7
#endif

// flag bits sent to the receiver
#define MODE_CHANGE 0x80    // a pin mode was changed
#define DIG_CHANGE  0x40    // a digital output was changed
#define PWM_CHANGE  0x30    // an analog (pwm) value was changed on port 2..3
#define ANA_MASK    0x0F    // an analog read was requested on port 1..4

uint16_t Port::shiftRead(uint8_t bitOrder, uint8_t count) const {
    uint16_t value = 0, mask = bit(LSBFIRST ? 0 : count - 1);
    for (uint8_t i = 0; i < count; ++i) {
        digiWrite2(1);
        delayMicroseconds(5);
        if (digiRead())
            value |= mask;
        if (bitOrder == LSBFIRST)
            mask <<= 1;
        else
            mask >>= 1;
        digiWrite2(0);
        delayMicroseconds(5);
    }
    return value;
}

void Port::shiftWrite(uint8_t bitOrder, uint16_t value, uint8_t count) const {
    uint16_t mask = bit(LSBFIRST ? 0 : count - 1);
    for (uint8_t i = 0; i < count; ++i) {
        digiWrite((value & mask) != 0);
        if (bitOrder == LSBFIRST)
            mask <<= 1;
        else
            mask >>= 1;
        digiWrite2(1);
        digiWrite2(0);
    }
}

RemoteNode::RemoteNode (char id, uint8_t band, uint8_t group) 
    : nid (id & 0x1F)
{
    memset(&data, 0, sizeof data);
    RemoteHandler::setup(nid, band, group);
}

void RemoteNode::poll(uint16_t msecs) {
    uint8_t pending = millis() >= lastPoll + msecs;
    if (RemoteHandler::poll(*this, pending))
        lastPoll = millis();
}

void RemotePort::mode(uint8_t value) const {
    node.data.flags |= MODE_CHANGE;
    bitWrite(node.data.modes, pinBit(), value);
}

uint8_t RemotePort::digiRead() const {
    return bitRead(node.data.digiIO, pinBit());
}

void RemotePort::digiWrite(uint8_t value) const {
    node.data.flags |= DIG_CHANGE;
    bitWrite(node.data.digiIO, pinBit(), value);
}

void RemotePort::anaWrite(uint8_t val) const {
    if (portNum == 2 || portNum == 3) {
        bitSet(node.data.flags, portNum + 2);
        node.data.anaOut[portNum - 2] = val;
    } else
        digiWrite2(val >= 128);
}
   
void RemotePort::mode2(uint8_t value) const {
    node.data.flags |= MODE_CHANGE;
    bitWrite(node.data.modes, pinBit2(), value);
}
   
uint16_t RemotePort::anaRead() const {
    bitSet(node.data.flags, pinBit());
    return node.data.anaIn[pinBit()];
}
   
uint8_t RemotePort::digiRead2() const {
    return bitRead(node.data.digiIO, pinBit2());
}

void RemotePort::digiWrite2(uint8_t value) const {
    node.data.flags |= DIG_CHANGE;
    bitWrite(node.data.digiIO, pinBit2(), value);
}

PortI2C::PortI2C (uint8_t num, uint8_t rate)
    : Port (num), uswait (rate)
{
    sdaOut(1);
    mode2(OUTPUT);
    sclHi();
}

uint8_t PortI2C::start(uint8_t addr) const {
    sclLo();
    sclHi();
    sdaOut(0);
    return write(addr);
}

void PortI2C::stop() const {
    sdaOut(0);
    sclHi();
    sdaOut(1);
}

uint8_t PortI2C::write(uint8_t data) const {
    sclLo();
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
        sdaOut(data & mask);
        sclHi();
        sclLo();
    }
    sdaOut(1);
    sclHi();
    uint8_t ack = ! sdaIn();
    sclLo();
    return ack;
}

uint8_t PortI2C::read(uint8_t last) const {
    uint8_t data = 0;
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
        sclHi();
        if (sdaIn())
            data |= mask;
        sclLo();
    }
    sdaOut(last);
    sclHi();
    sclLo();
    if (last)
        stop();
    sdaOut(1);
    return data;
}

bool DeviceI2C::isPresent () const {
    byte ok = send();
    stop();
    return ok;
}

byte MilliTimer::poll(word ms) {
    byte ready = 0;
    if (armed) {
        word remain = next - millis();
        // since remain is unsigned, it will overflow to large values when
        // the timeout is reached, so this test works as long as poll() is
        // called no later than 5535 millisecs after the timer has expired
        if (remain <= 60000)
            return 0;
        // return a value between 1 and 255, being msecs+1 past expiration
        // note: the actual return value is only reliable if poll() is
        // called no later than 255 millisecs after the timer has expired
        ready = -remain;
    }
    set(ms);
    return ready;
}

word MilliTimer::remaining() const {
    word remain = armed ? next - millis() : 0;
    return remain <= 60000 ? remain : 0;
}

void MilliTimer::set(word ms) {
    armed = ms != 0;
    if (armed)
        next = millis() + ms - 1;
}

void BlinkPlug::ledOn (byte mask) {
    if (mask & 1) {
        digiWrite(0);
        mode(OUTPUT);
    }
    if (mask & 2) {
        digiWrite2(0);
        mode2(OUTPUT);
    }
    leds |= mask; //TODO could be read back from pins, i.s.o. saving here
}

void BlinkPlug::ledOff (byte mask) {
    if (mask & 1) {
        mode(INPUT);
        digiWrite(1);
    }
    if (mask & 2) {
        mode2(INPUT);
        digiWrite2(1);
    }
    leds &= ~ mask; //TODO could be read back from pins, i.s.o. saving here
}

byte BlinkPlug::state () {
    byte saved = leds;
    ledOff(1+2);
    byte result = !digiRead() | (!digiRead2() << 1);
    ledOn(saved);
    return result;
}

//TODO deprecated, use buttonCheck() !
byte BlinkPlug::pushed () {
    if (debounce.idle() || debounce.poll()) {
        byte newState = state();
        if (newState != lastState) {
            debounce.set(100); // don't check again for at least 100 ms
            byte nowOn = (lastState ^ newState) & newState;
            lastState = newState;
            return nowOn;
        }
    }
    return 0;
}

byte BlinkPlug::buttonCheck () {
    // collect button changes in the checkFlags bits, with proper debouncing
    if (debounce.idle() || debounce.poll()) {
        byte newState = state();
        if (newState != lastState) {
            debounce.set(100); // don't check again for at least 100 ms
            if ((lastState ^ newState) & 1)
                bitSet(checkFlags, newState & 1 ? ON1 : OFF1);
            if ((lastState ^ newState) & 2)
                bitSet(checkFlags, newState & 2 ? ON2 : OFF2);
            lastState = newState;
        }
    }
    // note that simultaneous button events will be returned in successive calls
    if (checkFlags)
        for (byte i = ON1; i <= OFF2; ++i) {
            if (bitRead(checkFlags, i)) {
                bitClear(checkFlags, i);
                return i;
            }
        }
    // if there are no button events, return the overall current button state
    return lastState == 3 ? ALL_ON : lastState ? SOME_ON : ALL_OFF;
}

void MemoryPlug::load (word page, void* buf, byte offset, int count) {
    // also don't load right after a save, see http://forum.jeelabs.net/node/469
    while (millis() < nextSave)
        ;

    setAddress(0x50 + (page >> 8));
    send();
    write((byte) page);
    write(offset);
    receive();
    byte* p = (byte*) buf;
    while (--count >= 0)
        *p++ = read(count == 0);
    stop();
}

void MemoryPlug::save (word page, const void* buf, byte offset, int count) {
    // don't do back-to-back saves, last one must have had time to finish!
    while (millis() < nextSave)
        ;

    setAddress(0x50 + (page >> 8));
    send();
    write((byte) page);
    write(offset);
    const byte* p = (const byte*) buf;
    while (--count >= 0)
        write(*p++);
    stop();

    nextSave = millis() + 6;
    // delay(5);
}

long MemoryStream::position (byte writing) const {
    long v = (curr - start) * step;
    if (pos > 0 && !writing)
        --v; // get() advances differently than put()
    return (v << 8) | pos;
}

byte MemoryStream::get () {
    if (pos == 0) {
        dev.load(curr, buffer);
        curr += step;
    }
    return buffer[pos++];
}

void MemoryStream::put (byte data) {
    buffer[pos++] = data;
    if (pos == 0) {
        dev.save(curr, buffer);
        curr += step;
    }
}

word MemoryStream::flush () {
    if (pos != 0) {
        memset(buffer + pos, 0xFF, 256 - pos);
        dev.save(curr, buffer);
    }
    return curr;
}

void MemoryStream::reset () {
    curr = start;
    pos = 0;
}

// uart register definitions
#define RHR     (0 << 3)
#define THR     (0 << 3)
#define DLL     (0 << 3)
#define DLH     (1 << 3)
#define FCR     (2 << 3)
#define LCR     (3 << 3)
#define RXLVL   (9 << 3)

void UartPlug::regSet (byte reg, byte value) {
  dev.send();
  dev.write(reg);
  dev.write(value);
}

void UartPlug::regRead (byte reg) {
  dev.send();
  dev.write(reg);
  dev.receive();
}

void UartPlug::begin (long baud) {
    word divisor = 230400 / baud;
    regSet(LCR, 0x80);          // divisor latch enable
    regSet(DLL, divisor);       // low byte
    regSet(DLH, divisor >> 8);  // high byte
    regSet(LCR, 0x03);          // 8 bits, no parity
    regSet(FCR, 0x07);          // fifo enable (and flush)
    dev.stop();
}

byte UartPlug::available () {
    if (in != out)
        return 1;
    out = 0;
    regRead(RXLVL);
    in = dev.read(1);
    if (in == 0)
        return 0;
    if (in > sizeof rxbuf)
        in = sizeof rxbuf;
    regRead(RHR);
    for (byte i = 0; i < in; ++i)
        rxbuf[i] = dev.read(i == in - 1);
    return 1;
}

int UartPlug::read () {
    return available() ? rxbuf[out++] : -1;
}

void UartPlug::flush () {
    regSet(FCR, 0x07); // flush both RX and TX queues
    dev.stop();
    in = out;
}

WRITE_RESULT UartPlug::write (byte data) {
    regSet(THR, data);
    dev.stop();
#if ARDUINO >= 100 && !defined(__AVR_ATtiny84__) && !defined(__AVR_ATtiny85__) && !defined(__AVR_ATtiny44__)
    return 1;
#endif
}

void DimmerPlug::begin () {
    setReg(MODE1, 0x00);     // normal
    setReg(MODE2, 0x14);     // inverted, totem-pole
    setReg(GRPPWM, 0xFF);    // set group dim to max brightness
    setMulti(LEDOUT0, 0xFF, 0xFF, 0xFF, 0xFF, -1); // all LEDs group-dimmable
}

byte DimmerPlug::getReg(byte reg) const {
    send();
    write(reg);
    receive();
    byte result = read(1);
    stop();
    return result;
}

void DimmerPlug::setReg(byte reg, byte value) const {
    send();
    write(reg);
    write(value);
    stop();
}

void DimmerPlug::setMulti(byte reg, ...) const {
    va_list ap;
    va_start(ap, reg);
    send();
    write(0xE0 | reg); // auto-increment
    for (;;) {
        int v = va_arg(ap, int);
        if (v < 0) break;
        write(v);
    }
    stop();
}

void LuxPlug::setGain(byte high) {
    send();
    write(0x81); // write to Timing regiser
    write(high ? 0x12 : 0x02);
    stop();
}

const word* LuxPlug::getData() {
    send();
    write(0xA0 | DATA0LOW);
    receive();
    data.b[0] = read(0);
    data.b[1] = read(0);
    data.b[2] = read(0);
    data.b[3] = read(1);
    stop();
    return data.w;
}

#define LUX_SCALE	14	// scale by 2^14 
#define RATIO_SCALE 9	// scale ratio by 2^9
#define CH_SCALE    10	// scale channel values by 2^10 

word LuxPlug::calcLux(byte iGain, byte tInt) const
{
    unsigned long chScale; 
    switch (tInt) {
        case 0:  chScale = 0x7517; break; 
        case 1:  chScale = 0x0fe7; break; 
        default: chScale = (1 << CH_SCALE); break;
    }
    if (!iGain)
        chScale <<= 4;
    unsigned long channel0 = (data.w[0] * chScale) >> CH_SCALE; 
    unsigned long channel1 = (data.w[1] * chScale) >> CH_SCALE; 

    unsigned long ratio1 = 0; 
    if (channel0 != 0)
        ratio1 = (channel1 << (RATIO_SCALE+1)) / channel0;
    unsigned long ratio = (ratio1 + 1) >> 1;

    word b, m;
         if (ratio <= 0x0040) { b = 0x01F2; m = 0x01BE; } 
    else if (ratio <= 0x0080) { b = 0x0214; m = 0x02D1; } 
    else if (ratio <= 0x00C0) { b = 0x023F; m = 0x037B; } 
    else if (ratio <= 0x0100) { b = 0x0270; m = 0x03FE; } 
    else if (ratio <= 0x0138) { b = 0x016F; m = 0x01FC; }
    else if (ratio <= 0x019A) { b = 0x00D2; m = 0x00FB; }
    else if (ratio <= 0x029A) { b = 0x0018; m = 0x0012; }
    else                      { b = 0x0000; m = 0x0000; }

    unsigned long temp = channel0 * b - channel1 * m;
    temp += 1 << (LUX_SCALE-1);
    return temp >> LUX_SCALE;
}

void GravityPlug::sensitivity(byte range, word bandwidth) {
    send();
    write(0x14);
    byte bwcode = bandwidth <= 25 ? 0 :
                    bandwidth <= 50 ? 1 :
                      bandwidth <= 100 ? 2 :
                        bandwidth <= 190 ? 3 :
                          bandwidth <= 375 ? 4 :
                            bandwidth <= 750 ? 5 : 6;
    // this only works correctly if range is 2, 4, or 8
    write(((range & 0x0C) << 1) | bwcode);
    stop();
}

const int* GravityPlug::getAxes() {
    send();
    write(0x02);
    receive();
    for (byte i = 0; i < 5; ++i)
        data.b[i] = read(0);
    data.b[5] = read(1);
    stop();
    data.w[0] = (data.b[0] >> 6) | (data.b[1] << 2);
    data.w[1] = (data.b[2] >> 6) | (data.b[3] << 2);
    data.w[2] = (data.b[4] >> 6) | (data.b[5] << 2);
    for (byte i = 0; i < 3; ++i)
        data.w[i] = (data.w[i] ^ 0x200) - 0x200; // sign extends bit 9
    return data.w;
}

void InputPlug::select(uint8_t channel) {
    digiWrite(0);
    mode(OUTPUT);

    delayMicroseconds(slow ? 400 : 50);
    byte data = 0x10 | (channel & 0x0F);
    byte mask = 1 << (portNum + 3); // digitalWrite is too slow
    
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        for (byte i = 0; i < 5; ++i) {
            byte us = bitRead(data, 4 - i) ? 9 : 3;
            if (slow)
                us <<= 3;
#ifdef PORTD
            PORTD |= mask;
            delayMicroseconds(us);
            PORTD &= ~ mask;
#else
            //XXX TINY!
#endif
            delayMicroseconds(slow ? 32 : 4);
        }
    }
}

byte HeadingBoard::eepromByte(byte reg) const {
    eeprom.send();
    eeprom.write(reg);
    eeprom.receive();
    byte result = eeprom.read(1);
    eeprom.stop();
    return result;
}

void HeadingBoard::getConstants() {
    for (byte i = 0; i < 18; ++i)
        ((byte*) &C1)[i < 14 ? i^1 : i] = eepromByte(16 + i);
    // Serial.println(C1);
    // Serial.println(C2);
    // Serial.println(C3);
    // Serial.println(C4);
    // Serial.println(C5);
    // Serial.println(C6);
    // Serial.println(C7);
    // Serial.println(A, DEC);
    // Serial.println(B, DEC);
    // Serial.println(C, DEC);
    // Serial.println(D, DEC);
}

word HeadingBoard::adcValue(byte press) const {
    aux.digiWrite(1);
    adc.send();
    adc.write(0xFF);
    adc.write(0xE0 | (press << 4));
    adc.stop();
    delay(40);
    adc.send();
    adc.write(0xFD);
    adc.receive();
    byte msb = adc.read(0);
    int result = (msb << 8) | adc.read(1);
    adc.stop();
    aux.digiWrite(0);
    return result;
}

void HeadingBoard::begin() {
    // prepare ADC
    aux.mode(OUTPUT);
    aux.digiWrite(0);
    
    // generate 32768 Hz on IRQ pin (OC2B)
#ifdef TCCR2A
    TCCR2A = bit(COM2B0) | bit(WGM21);
    TCCR2B = bit(CS20);
    OCR2A = 243;
#else
    //XXX TINY!
#endif
    aux.mode3(OUTPUT);
    
    getConstants();
}

void HeadingBoard::pressure(int& temp, int& pres) const {
    word D2 = adcValue(0);
    // Serial.print("D2 = ");
    // Serial.println(D2);
    int corr = (D2 - C5) >> 7;        
    // Serial.print("corr = ");
    // Serial.println(corr);
    int dUT = (D2 - C5) - (corr * (long) corr * (D2 >= C5 ? A : B) >> C);
    // Serial.print("dUT = ");
    // Serial.println(dUT);
    temp = 250 + (dUT * C6 >> 16) - (dUT >> D); 

    word D1 = adcValue(1);
    // Serial.print("D1 = ");
    // Serial.println(D1);
    word OFF = (C2 + ((C4 - 1024) * dUT >> 14)) << 2;
    // Serial.print("OFF = ");
    // Serial.println(OFF);
    word SENS = C1 + (C3 * dUT >> 10);
    // Serial.print("SENS = ");
    // Serial.println(SENS);
    word X = (SENS * (D1 - 7168L) >> 14) - OFF;
    // Serial.print("X = ");
    // Serial.println(X);
    pres = (X * 10L >> 5) + C7;
}

void HeadingBoard::heading(int& xaxis, int& yaxis) {
    // set or reset the magnetometer coil
    compass.send();
    compass.write(0x00);
    compass.write(setReset);
    compass.stop();
    delayMicroseconds(50);
    setReset = 6 - setReset;
    // perform measurement
    compass.send();
    compass.write(0x00);
    compass.write(0x01);
    compass.stop();
    delay(5);
    compass.send();
    compass.write(0x00);
    compass.receive();
    byte tmp, reg = compass.read(0);
    tmp = compass.read(0);
    xaxis = ((tmp << 8) | compass.read(0)) - 2048;
    tmp = compass.read(0);
    yaxis = ((tmp << 8) | compass.read(1)) - 2048;
    compass.stop();
}

int CompassBoard::read2 (byte last) {
    byte b = read(0);
    return (b << 8) | read(last);
}

float CompassBoard::heading () {
    send();     
    write(0x01); // Configuration Register B
    write(0x40); // Reg B: +/- 1.9 Ga
    stop();

    send();
    write(0x02); // Data Output X MSB Register
    write(0x00); // Mode: Continuous-Measurement Mode
    receive();
    int x = read2(0);
    int z = read2(0);
    int y = read2(1);
    stop();
    
    return degrees(atan2(y, x));
}


InfraredPlug::InfraredPlug (uint8_t num)
        : Port (num), slot (140), gap (80), fill (-1), prev (0) {
    digiWrite(0);
    mode(OUTPUT);
    mode2(INPUT);
    digiWrite2(1); // pull-up        
}

void InfraredPlug::configure(uint8_t slot4, uint8_t gap256) {
    slot = slot4;
    gap = gap256;
    fill = -1;
}

void InfraredPlug::poll() {
    byte bit = digiRead2(); // 0 is interpreted as pulse ON
    if (fill < 0) {
        if (fill < -1 || bit == 1)
            return;
        fill = 0;
        prev = micros();
        memset(buf, 0, sizeof buf);
    }
    // act only if the bit changed, using the low bit of the nibble fill count
    if (bit != (fill & 1) && fill < 2 * sizeof buf) {
        uint32_t curr = micros(), diff = (curr - prev + 2) >> 2;
        if (diff > 65000)
            diff = 65000; // * 4 us, i.e. 260 ms
        // convert to a slot number, with rounding halfway between each slot
        word ticks = ((word) diff + slot / 2) / slot;
        if (ticks > 20)
            ticks = 20;
        // condense upper values to fit in the range 0..15
        byte nibble = ticks;
        if (nibble > 10)
            nibble -= (nibble - 10) / 2;
        buf[fill>>1] |= nibble << ((fill & 1) << 2);
        ++fill;
        prev = curr;
    }
}

uint8_t InfraredPlug::done() {
    byte result = 0;
    if (fill > 0)
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            if (((micros() - prev) >> 8) >= gap) {
                result = fill;
                fill = -2; // prevent new pulses from clobbering buf
            }
        }
    else if (fill < -1)
        fill = -1; // second call to done() release buffer again for capture
    return result;
}

uint8_t InfraredPlug::decoder(uint8_t nibbles) {
    switch (nibbles) {
        case 67: // 2 + 64 + 1 nibbles could be a NEC packet
            if (buf[0] == 0x8D && buf[33] == 0x01) {
                // check that all nibbles are either 1 or 3
                for (byte i = 1; i < 33; ++i)
                    if ((buf[i] & ~0x20) != 0x11)
                        return UNKNOWN;
                // valid packet, convert in-place
                for (byte i = 0; i < 4; ++i) {
                    byte v;
                    for (byte j = 0; j < 8; ++j)
                        v = (v << 1) | (buf[1+j+8*i] >> 5);
                    buf[i] = v;
                }
                return NEC;
            }
            break;
        case 3: // 2 + 1 nibbles could be a NEC repeat packet
            if (buf[0] == 0x4D && buf[1] == 0x01)
                return NEC_REP;
            break;
    }
    return UNKNOWN;
}
    
void InfraredPlug::send(const uint8_t* data, uint16_t bits) {
    // TODO: switch to an interrupt-driven design
    for (byte i = 0; i < bits; ++i) {
        digiWrite(bitRead(data[i/8], i%8));
        delayMicroseconds(4 * slot);
    }
    digiWrite(0);
}

void ProximityPlug::begin() {
    delay(100);
    setReg(CONFIG, 0x04);   // reset, STOP1
    delay(100);
    // setReg(TPCONFIG, 0xB5); // TPSE, BKA, ACE, TPTBE, TPE
    setReg(TPCONFIG, 0xB1); // TPSE, BKA, ACE, TPE
    setReg(CONFIG, 0x15);   // RUN1
    delay(100);
}

void ProximityPlug::setReg(byte reg, byte value) const {
    send();
    write(reg);
    write(value);
    stop();
}

byte ProximityPlug::getReg(byte reg) const {
    send();
    write(reg);
    receive();
    byte result = read(1);
    stop();
    return result;
}

void AnalogPlug::begin (byte mode) {
  // default mode is channel 1, continuous, 18-bit, gain x1
  config = mode;
  select(1);
}

void AnalogPlug::select (byte channel) {
  send();
  write(0x80 | ((channel - 1) << 5) | (config & 0x1F));
  stop();    
}

long AnalogPlug::reading () {
  // read out 4 bytes, caller will need to shift out the irrelevant lower bits
  receive();
  long raw = read(0) << 8;
  raw |= read(0);
  raw = (raw << 16) | (read(0) << 8);
  raw |= read(1);
  stop();
  return raw;
}

DHTxx::DHTxx (byte pinNum) : pin (pinNum) {
  digitalWrite(pin, HIGH);
}

bool DHTxx::reading (int& temp, int &humi) {
  pinMode(pin, OUTPUT);
  delay(10); // wait for any previous transmission to end
  digitalWrite(pin, LOW);
  delay(18);
  
  cli();
  
  digitalWrite(pin, HIGH);
  delayMicroseconds(30);
  pinMode(pin, INPUT);
  
  byte data[6]; // holds a few start bits and then the 5 real payload bytes
#if DEBUG_DHT
  static byte times[48];
  memset(times, 0, sizeof times);
#endif

  // each bit is a high edge followed by a var-length low edge
  for (byte i = 7; i < 48; ++i) {
    // wait for the high edge, then measure time until the low edge
    byte timer;
    for (byte j = 0; j < 2; ++j)
      for (timer = 0; timer < 250; ++timer)
        if (digitalRead(pin) != j)
          break;
#if DEBUG_DHT
    times[i] = timer;
#endif
    // if no transition was seen, return 
    if (timer >= 250) {
      sei();
      return false;
    }
    // collect each bit in the data buffer
    byte offset = i / 8;
    data[offset] <<= 1;
    data[offset] |= timer > 7;
  }
  
  sei();

#if DEBUG_DHT
  Serial.print("DHT");
  for (byte i = 7; i < 48; ++i) {
    Serial.print(' ');
    Serial.print(times[i]);
  }
  Serial.println();
  Serial.print("HEX");
  for (byte i = 0; i < sizeof data; ++i) {
    Serial.print(' ');
    Serial.print(data[i], HEX);
  }
  Serial.print(" : ");
  byte s = data[1] + data[2] + data[3] + data[4];
  Serial.print(s, HEX);
  Serial.println();
#endif
  
  byte sum = data[1] + data[2] + data[3] + data[4];
  if (sum != data[5])
    return false;
  
  word h = (data[1] << 8) | data[2];
  humi = ((h >> 3) * 5) >> 4;     // careful with overflow

  int tmul = data[3] & 0x80 ? -5 : 5;
  word t = ((data[3] & 0x7F) << 8) | data[4];
  temp = ((t >> 3) * tmul) >> 4;  // careful with overflow

  return true;
}

// ISR(WDT_vect) { Sleepy::watchdogEvent(); }

static volatile byte watchdogCounter;

void Sleepy::watchdogInterrupts (char mode) {
    // correct for the fact that WDP3 is *not* in bit position 3!
    if (mode & bit(3))
        mode ^= bit(3) | bit(WDP3);
    // pre-calculate the WDTCSR value, can't do it inside the timed sequence
    // we only generate interrupts, no reset
    byte wdtcsr = mode >= 0 ? bit(WDIE) | mode : 0;
    MCUSR &= ~(1<<WDRF);
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
#ifndef WDTCSR
#define WDTCSR WDTCR
#endif
        WDTCSR |= (1<<WDCE) | (1<<WDE); // timed sequence
        WDTCSR = wdtcsr;
    }
}

void Sleepy::powerDown () {
    byte adcsraSave = ADCSRA;
    ADCSRA &= ~ bit(ADEN); // disable the ADC
    // see http://www.nongnu.org/avr-libc/user-manual/group__avr__sleep.html
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        sleep_enable();
        // sleep_bod_disable(); // can't use this - not in my avr-libc version!
#ifdef BODSE
        MCUCR = MCUCR | bit(BODSE) | bit(BODS); // timed sequence
        MCUCR = MCUCR & ~ bit(BODSE) | bit(BODS);
#endif
    }
    sleep_cpu();
    sleep_disable();
    // re-enable what we disabled
    ADCSRA = adcsraSave;
}

byte Sleepy::loseSomeTime (word msecs) {
    byte ok = 1;
    word msleft = msecs;
    // only slow down for periods longer than the watchdog granularity
    while (msleft >= 16) {
        char wdp = 0; // wdp 0..9 corresponds to roughly 16..8192 ms
        // calc wdp as log2(msleft/16), i.e. loop & inc while next value is ok
        for (word m = msleft; m >= 32; m >>= 1)
            if (++wdp >= 9)
                break;
        watchdogCounter = 0;
        watchdogInterrupts(wdp);
        powerDown();
        watchdogInterrupts(-1); // off
        // when interrupted, our best guess is that half the time has passed
        word halfms = 8 << wdp;
        msleft -= halfms;
        if (watchdogCounter == 0) {
            ok = 0; // lost some time, but got interrupted
            break;
        }
        msleft -= halfms;
    }
    // adjust the milli ticks, since we will have missed several
#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__) || defined (__AVR_ATtiny44__)
    extern volatile unsigned long millis_timer_millis;
    millis_timer_millis += msecs - msleft;
#else
    extern volatile unsigned long timer0_millis;
    timer0_millis += msecs - msleft;
#endif
    return ok; // true if we lost approx the time planned
}

void Sleepy::watchdogEvent() {
    ++watchdogCounter;
}

Scheduler::Scheduler (byte size) : maxTasks (size), remaining (~0) {
    byte bytes = size * sizeof *tasks;
    tasks = (word*) malloc(bytes);
    memset(tasks, 0xFF, bytes);
}

Scheduler::Scheduler (word* buf, byte size) : tasks (buf), maxTasks (size), remaining(~0) {
    byte bytes = size * sizeof *tasks;
    memset(tasks, 0xFF, bytes);
}

char Scheduler::poll() {
    // all times in the tasks array are relative to the "remaining" value
    // i.e. only remaining counts down while waiting for the next timeout
    if (remaining == 0) {
        word lowest = ~0;
        for (byte i = 0; i < maxTasks; ++i) {
            if (tasks[i] == 0) {
                tasks[i] = ~0;
                return i;
            }
            if (tasks[i] < lowest)
                lowest = tasks[i];
        }
        if (lowest != ~0) {
            for (byte i = 0; i < maxTasks; ++i) {
                if(tasks[i] != ~0) {
                    tasks[i] -= lowest;
                }
            }
        }
        remaining = lowest;
    } else if (remaining == ~0) //remaining == ~0 means nothing running
        return -2;
    else if (ms100.poll(100))
        --remaining;
    return -1;
}

char Scheduler::pollWaiting() {
    if(remaining == ~0)  // Nothing running!
        return -2;
    // first wait until the remaining time we need to wait is less than 0.1s
    while (remaining > 0) {
        word step = remaining > 600 ? 600 : remaining;
        if (!Sleepy::loseSomeTime(100 * step)) // uses least amount of power
            return -1;
        remaining -= step;
    }
    // now lose some more time until that 0.1s mark
    if (!Sleepy::loseSomeTime(ms100.remaining()))
        return -1;
    // lastly, just ignore the 0..15 ms still left to go until the 0.1s mark
    return poll();
}

void Scheduler::timer(byte task, word tenths) {
    // if new timer will go off sooner than the rest, then adjust all entries
    if (tenths < remaining) {
        word diff = remaining - tenths;
        for (byte i = 0; i < maxTasks; ++i)
            if (tasks[i] != ~0)
                tasks[i] += diff;
        remaining = tenths;
    }
    tasks[task] = tenths - remaining;
}

void Scheduler::cancel(byte task) {
    tasks[task] = ~0;
}

#ifdef Stream_h // only available in recent Arduino IDE versions

InputParser::InputParser (byte* buf, byte size, Commands* ctab, Stream& stream)
        : buffer (buf), limit (size), cmds (ctab), io (stream) {
    reset();
}

InputParser::InputParser (byte size, Commands* ctab, Stream& stream)
        : limit (size), cmds (ctab), io (stream) {
    buffer = (byte*) malloc(size);
    reset();
}

void InputParser::reset() {
    fill = next = 0;
    instring = hexmode = hasvalue = 0;
    top = limit;
}

void InputParser::poll() {
    if (!io.available())
        return;
    char ch = io.read();
    if (ch < ' ' || fill >= top) {
        reset();
        return;
    }
    if (instring) {
        if (ch == '"') {
            buffer[fill++] = 0;
            do
                buffer[--top] = buffer[--fill];
            while (fill > value);
            ch = top;
            instring = 0;
        }
        buffer[fill++] = ch;
        return;
    }
    if (hexmode && ('0' <= ch && ch <= '9' ||
                    'A' <= ch && ch <= 'F' ||
                    'a' <= ch && ch <= 'f')) {
        if (!hasvalue)
            value = 0;
        if (ch > '9')
            ch += 9;
        value <<= 4;
        value |= (byte) (ch & 0x0F);
        hasvalue = 1;
        return;
    }
    if ('0' <= ch && ch <= '9') {
        if (!hasvalue)
            value = 0;
        value = 10 * value + (ch - '0');
        hasvalue = 1;
        return;
    }
    hexmode = 0;
    switch (ch) {
        case '$':   hexmode = 1;
                    return;
        case '"':   instring = 1;
                    value = fill;
                    return;
        case ':':   (word&) buffer[fill] = value;
                    fill += 2;
                    value >>= 16;
                    // fall through
        case '.':   (word&) buffer[fill] = value;
                    fill += 2;
                    hasvalue = 0;
                    return;
        case '-':   value = - value;
                    hasvalue = 0;
                    return;
        case ' ':   if (!hasvalue)
                        return;
                    // fall through
        case ',':   buffer[fill++] = value;
                    hasvalue = 0;
                    return;
    }
    if (hasvalue) {
        io.print("Unrecognized character: ");
        io.print(ch);
        io.println();
        reset();
        return;
    }
    
    for (Commands* p = cmds; ; ++p) {
        char code = pgm_read_byte(&p->code);
        if (code == 0)
            break;
        if (ch == code) {
            byte bytes = pgm_read_byte(&p->bytes);
            if (fill < bytes) {
                io.print("Not enough data, need ");
                io.print((int) bytes);
                io.println(" bytes");
            } else {
                memset(buffer + fill, 0, top - fill);
                ((void (*)()) pgm_read_word(&p->fun))();
            }
            reset();
            return;
        }
    }
        
    io.print("Known commands:");
    for (Commands* p = cmds; ; ++p) {
        char code = pgm_read_byte(&p->code);
        if (code == 0)
            break;
        io.print(' ');
        io.print(code);
    }
    io.println();
}

InputParser& InputParser::get(void* ptr, byte len) {
    memcpy(ptr, buffer + next, len);
    next += len;
    return *this;
}

InputParser& InputParser::operator >> (const char*& v) {
    byte offset = buffer[next++];
    v = top <= offset && offset < limit ? (char*) buffer + offset : "";
    return *this;
}

#endif // Stream_h
