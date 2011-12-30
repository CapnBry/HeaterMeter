// Port library interface to BMP085 sensors connected via I2C
// 2009-02-17 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

class BMP085 : public DeviceI2C {
    int16_t ac1, ac2, ac3, b1, b2, mb, mc, md;
    uint16_t ac4, ac5, ac6;
    
    uint16_t readWord(uint8_t last) const
        { uint16_t v = read(0) << 8; return v | read(last); }
    void readFromReg(uint8_t reg) const
        { send(); write(reg); receive(); }
            
public:
    enum { TEMP, PRES };
    int32_t meas[2];
    uint8_t oss;
    
    BMP085 (const PortI2C& p, uint8_t osrs =0)
        : DeviceI2C (p, 0x77), oss (osrs) {}
        
    void setOverSampling(uint8_t osrs) { oss = osrs; }
    
    uint8_t startMeas(uint8_t type) const;
    int32_t getResult(uint8_t type);
    
    int32_t measure(uint8_t type)
        { delay(startMeas(type)); return getResult(type); }

    void getCalibData();
    void calculate(int16_t& tval, int32_t& pval) const;
};
