// Port library interface to SHT11 sensors connected via "something like I2C"
// 2009-02-16 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

class SHT11 : public Port {
    void clock(uint8_t x) const;
    void release() const;

    uint8_t writeByte(uint8_t value) const;
    uint8_t waitAck() const;
    uint8_t readByte(uint8_t ack) const;
    void start() const;

    static void crcCalc(uint8_t x);    
    static void (*crcFun)(uint8_t);
    static uint8_t crc8;
public:
    static void enableCRC();
    
    enum { TEMP, HUMI }; 
    uint16_t meas[2];

    SHT11 (uint8_t num) : Port (num) { connReset(); }
    
    void connReset() const;
    void softReset() const;
    
    uint8_t readStatus() const;
    void writeStatus(uint8_t value) const;

    uint8_t measure(uint8_t type, void (*delayFun)() =0);

#ifndef __AVR_ATtiny84__ || __AVR_ATtiny44__    
    void calculate(float& rh_true, float& t_C) const;

    static float dewpoint(float h, float t);
#else
    //XXX TINY!
#endif
};
