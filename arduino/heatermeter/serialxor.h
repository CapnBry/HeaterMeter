#ifndef __SERIALXOR_H__
#define __SERIALXOR_H__

#include "Arduino.h"

class SerialXorChecksum : public Print
{
public:
  using Print::write;
  virtual size_t write(uint8_t ch)
  {
    if (!_preambleSent)
    {
      _preambleSent = true;
      _xsum = 0;
      Serial.write('$');
    }
    _xsum ^= ch;
    return Serial.write(ch);
  }
  
  void nl(void)
  {
    Serial.write('*');
    hexwrite(_xsum / 16);
    hexwrite(_xsum % 16);
    Serial.write('\n');
    _preambleSent = false;
  }
  
private:
  boolean _preambleSent;
  uint8_t _xsum;

  inline void hexwrite(uint8_t val) const 
  {
    val = val < 10 ? val + '0' : val + 'A' - 10; 
    Serial.write(val);
  };
};

extern SerialXorChecksum SerialX;
#endif /* __SERIALXOR_H__ */
