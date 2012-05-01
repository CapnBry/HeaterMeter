// Port library interface to BMP085 sensors connected via I2C
// 2009-02-17 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

// computation and algorithm taken from the Bosch Sensortec BMP085 data sheet
// (IIRC, I had to cast to an int32_t for the b3 calculation to work above 25C)

#include <Ports.h>
#include "PortsBMP085.h"
#if ARDUINO>=100
#include <Arduino.h> // Arduino 1.0
#else
#include <WProgram.h> // Arduino 0022
#endif

uint8_t BMP085::startMeas(uint8_t type) const {
    send();
    write(0xF4);
    write(type == TEMP ? 0x2E : 0x34 | (oss << 6));
    stop();
    return oss == 0 ? 5 : oss == 1 ? 8 : oss == 2 ? 14 : 26;
}

int32_t BMP085::getResult(uint8_t type) {
    readFromReg(0xF6);
    if (type == TEMP) 
        meas[TEMP] = readWord(1);
    else {
        meas[PRES] = readWord(0);
        meas[PRES] <<= oss;
        meas[PRES] |= read(1) >> (8-oss);
    }
    return meas[type];
}

void BMP085::getCalibData() {
    readFromReg(0xAA);
    ac1 = readWord(0);
    ac2 = readWord(0);
    ac3 = readWord(0);
    ac4 = readWord(0);
    ac5 = readWord(0);
    ac6 = readWord(0);
    b1 = readWord(0);
    b2 = readWord(0);
    mb = readWord(0);
    mc = readWord(0);
    md = readWord(1);
}

void BMP085::calculate(int16_t& tval, int32_t& pval) const {
    int32_t ut = meas[TEMP], up = meas[PRES];
    int32_t x1, x2, x3, b3, b5, b6, p;
    uint32_t b4, b7;

    x1 = (ut - ac6) * ac5 >> 15;
    x2 = ((int32_t) mc << 11) / (x1 + md);
    b5 = x1 + x2;
    tval = (b5 + 8) >> 4;
    
    b6 = b5 - 4000;
    x1 = (b2 * (b6 * b6 >> 12)) >> 11; 
    x2 = ac2 * b6 >> 11;
    x3 = x1 + x2;
    b3 = ((((int32_t) ac1 * 4 + x3) << oss) + 2) >> 2;
    x1 = ac3 * b6 >> 13;
    x2 = (b1 * (b6 * b6 >> 12)) >> 16;
    x3 = ((x1 + x2) + 2) >> 2;
    b4 = (ac4 * (uint32_t) (x3 + 32768)) >> 15;
    b7 = ((uint32_t) up - b3) * (50000 >> oss);
    p = b7 < 0x80000000 ? (b7 * 2) / b4 : (b7 / b4) * 2;
    
    x1 = (p >> 8) * (p >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * p) >> 16;
    pval = p + ((x1 + x2 + 3791) >> 4);
}
