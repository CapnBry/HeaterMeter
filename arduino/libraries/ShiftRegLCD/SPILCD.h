#ifndef SPILCD_H
#define SPILCD_H

#include "ShiftRegLCD.h"

// Register is wired as XXRE4567
#define SPI_LCD_RS 0x04
#define SPI_LCD_E  0x08

class SPILCD : public ShiftRegLCD
{
public:
  SPILCD(uint8_t srlatch, uint8_t lines);

protected:
  virtual void send(uint8_t value, uint8_t mode);
  virtual void init4bits(uint8_t value);

private:
  void spi_byte(uint8_t out);
  void spi_lcd(uint8_t value);
};
#endif /* SPILCD_H */