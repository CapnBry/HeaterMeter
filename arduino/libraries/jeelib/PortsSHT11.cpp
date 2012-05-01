// Port library interface to SHT11 sensors connected via "something like I2C"
// 2009-02-16 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

// rewritten in C++ using the SENSIRION SHTxx Sample Code Application Note
// the CRC calculation is from the SENSIRION SHTxx CRC Application Note

#include <Ports.h>
#include "PortsSHT11.h"
#include <avr/pgmspace.h>
#if ARDUINO>=100
#include <Arduino.h> // Arduino 1.0
#else
#include <WProgram.h> // Arduino 0022
#endif

enum {
    MEASURE_TEMP = 0x03,
    MEASURE_HUMI = 0x05,
    STATUS_REG_W = 0x06,
    STATUS_REG_R = 0x07,
    RESET        = 0x1e,
};

static uint8_t crcTab [] PROGMEM = {
    0, 49, 98, 83, 196, 245, 166, 151, 185, 136, 219, 234, 125, 76, 31, 46, 67,
    114, 33, 16, 135, 182, 229, 212, 250, 203, 152, 169, 62, 15, 92, 109, 134,
    183, 228, 213, 66, 115, 32, 17, 63, 14, 93, 108, 251, 202, 153, 168, 197,
    244, 167, 150, 1, 48, 99, 82, 124, 77, 30, 47, 184, 137, 218, 235, 61, 12,
    95, 110, 249, 200, 155, 170, 132, 181, 230, 215, 64, 113, 34, 19, 126, 79,
    28, 45, 186, 139, 216, 233, 199, 246, 165, 148, 3, 50, 97, 80, 187, 138,
    217, 232, 127, 78, 29, 44, 2, 51, 96, 81, 198, 247, 164, 149, 248, 201, 154,
    171, 60, 13, 94, 111, 65, 112, 35, 18, 133, 180, 231, 214, 122, 75, 24, 41,
    190, 143, 220, 237, 195, 242, 161, 144, 7, 54, 101, 84, 57, 8, 91, 106, 253,
    204, 159, 174, 128, 177, 226, 211, 68, 117, 38, 23, 252, 205, 158, 175, 56,
    9, 90, 107, 69, 116, 39, 22, 129, 176, 227, 210, 191, 142, 221, 236, 123,
    74, 25, 40, 6, 55, 100, 85, 194, 243, 160, 145, 71, 118, 37, 20, 131, 178,
    225, 208, 254, 207, 156, 173, 58, 11, 88, 105, 4, 53, 102, 87, 192, 241,
    162, 147, 189, 140, 223, 238, 121, 72, 27, 42, 193, 240, 163, 146, 5, 52,
    103, 86, 120, 73, 26, 43, 188, 141, 222, 239, 130, 179, 224, 209, 70, 119,
    36, 21, 59, 10, 89, 104, 255, 206, 157, 172
};

void SHT11::crcCalc(uint8_t x) {
    crc8 = pgm_read_byte(crcTab + (x ^ crc8));
}

static void dummyCRC(uint8_t) {}

// static variables, i.e. CRC can only be enabled for all SHT11 devices at once
// the code below avoids linking in any CRC code if enableCRC() is never called

uint8_t SHT11::crc8 = 0;
void (*SHT11::crcFun)(uint8_t) = &dummyCRC;

void SHT11::enableCRC() {
    SHT11::crcFun = crcCalc;
}

// idle line state is with data as input pulled high, and clock as output low

void SHT11::clock(uint8_t x) const {
    delayMicroseconds(2);
    digiWrite2(x);
    delayMicroseconds(5);
}

void SHT11::release() const {
    mode(INPUT);
    digiWrite(1);
}

uint8_t SHT11::writeByte(uint8_t value) const {
    mode(OUTPUT);
    for (uint8_t i = 0x80; i != 0; i >>= 1) {
        digiWrite(value & i);
        clock(1);
        clock(0);
    }
    release();
    clock(1);
    uint8_t error = digiRead();
    clock(0);
    
    crcFun(value);
    return error;
}

uint8_t SHT11::readByte(uint8_t ack) const {
    uint8_t value = 0;
    for (uint8_t i = 0x80; i != 0; i >>= 1) {
        clock(1);
        if (digiRead())
            value |= i;
        clock(0);
    }
    mode(OUTPUT);
    digiWrite(!ack);
    clock(1);
    clock(0);
    release();
    
    crcFun(value);
    return value;
}

void SHT11::start() const {
    clock(0);
    mode(OUTPUT);
    digiWrite(1);
    
    clock(1); 
    digiWrite(0); 
    clock(0);   
    clock(1); 
    digiWrite(1);      
    clock(0);
    release();
    
    crc8 = 0;
}

void SHT11::connReset() const {
    mode2(OUTPUT);
    clock(0);
    mode(OUTPUT);
    digiWrite(1);
    for (uint8_t i = 0; i < 9; ++i) {
        clock(1);
        clock(0);
    }
    start();
}

void SHT11::softReset() const {
    connReset();
    writeByte(RESET);
    delay(11);
}

uint8_t SHT11::readStatus() const {
    start();
    writeByte(STATUS_REG_R);
    uint8_t value = readByte(1);
    readByte(0);
    return value;
}

void SHT11::writeStatus(uint8_t value) const {
    start();
    writeByte(STATUS_REG_W);
    writeByte(value);
}

uint8_t SHT11::measure(uint8_t type, void (*delayFun)()) {
    start();
    writeByte(type == TEMP? MEASURE_TEMP : MEASURE_HUMI);
    for (uint8_t i = 0; i < 250; ++i) {
        if (!digiRead()) {
            meas[type] = readByte(1) << 8;
            meas[type] |= readByte(1);
            uint8_t flipped = 0;
            for (uint8_t j = 0x80; j != 0; j >>= 1) {
                flipped >>= 1;
                if (crc8 & j)
                    flipped |= 0x80;
            }
            if (readByte(0) != flipped)
                break;
            return 0;
        }
        if (delayFun)
            delayFun();
        else
            delay(1);
    }
    connReset();
    return 1;
}

#ifndef __AVR_ATtiny84__ || __AVR_ATtiny44__
void SHT11::calculate(float& rh_true, float& t_C) const {
    const float C1=-2.0468;
    const float C2= 0.0367;
    const float C3=-1.5955e-6;
    const float T1=0.01;
    const float T2=0.00008;

    t_C = meas[TEMP] * 0.01 - 39.66;  // for 3.3 V

    float rh = meas[HUMI];
    rh_true = (t_C-25)*(T1+T2*rh) + C3*rh*rh + C2*rh + C1;
    if (rh_true > 99) rh_true = 100;
    if (rh_true < 0.1) rh_true = 0.1;
} 

float SHT11::dewpoint(float h, float t) {
    float k = (log10(h)-2)/0.4343 + (17.62*t)/(243.12+t); 
    return 243.12*k/(17.62-k);  
} 
#else
//XXX TINY!
#endif
