#ifndef ShiftRegLCD_h
#define ShiftRegLCD_h

#include <inttypes.h>
#include "Print.h"

// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

// two-wire indicator constant
#define TWO_WIRE 204
#define SR_RS_BIT 0x04
#define SR_EN_BIT 0x80

class ShiftRegLCDBase : public Print {
public:
  void clear();
  void home();

  void noDisplay();
  void display();
  void noBlink();
  void blink();
  void noCursor();
  void cursor();
  void scrollDisplayLeft();
  void scrollDisplayRight();
  void printLeft();
  void printRight();
  void shiftLeft();
  void shiftRight();
  void shiftIncrement();
  void shiftDecrement();

  void createChar(uint8_t, uint8_t[]);
  void createChar_P(uint8_t location, const char *p);
  void setCursor(uint8_t, uint8_t);
  using Print::write;
  virtual size_t write(uint8_t);
  void write_P(const char *p, uint8_t len);
  void command(uint8_t);
  
  // Two pins not used for the LCD but are sent to the shiftreg
  void digitalWrite(uint8_t pin, uint8_t val);
protected:
  void init(uint8_t lines, uint8_t font);
  virtual void send(uint8_t, uint8_t) const = 0;
  virtual void send4bits(uint8_t) const = 0;
  virtual void updateAuxPins(void) const = 0;
  ShiftRegLCDBase(void) {};

  uint8_t _auxPins;
private:
  uint8_t _displayfunction;
  uint8_t _displaycontrol;
  uint8_t _displaymode;
  uint8_t _numlines;
};

class ShiftRegLCDNative : public ShiftRegLCDBase
{
public:
  // Assuming 1 line 8 pixel high font
  ShiftRegLCDNative(uint8_t srdata, uint8_t srclockd, uint8_t enable)
    { ctor(srdata, srclockd, enable, 1, 0); };
  // Set nr. of lines, assume 8 pixel high font
  ShiftRegLCDNative(uint8_t srdata, uint8_t srclockd, uint8_t enable, uint8_t lines)
    { ctor(srdata, srclockd, enable, lines, 0); };
  // Set nr. of lines and font
  ShiftRegLCDNative(uint8_t srdata, uint8_t srclockd, uint8_t enable, uint8_t lines, uint8_t font)
    { ctor(srdata, srclockd, enable, lines, font); };
protected:
  virtual void send(uint8_t, uint8_t) const;
  virtual void send4bits(uint8_t) const;
  virtual void updateAuxPins(void) const;
private:
  void ctor(uint8_t srdata, uint8_t srclockd, uint8_t enable, uint8_t lines, uint8_t font);
  uint8_t _srdata_pin;
  uint8_t _srclock_pin;
  uint8_t _enable_pin;
  uint8_t _two_wire;
};

// SPI shift register is wired as XXRE4567
#define SPI_LCD_RS 0x04
#define SPI_LCD_E  0x08
// This uses the SPI bus as the shift register's data and clock, and using a
// 75HC595 latched shift register instead
// QA - N/C
// QB - N/C
// QC - RS
// QD - E
// QE - D4
// QF - D5
// QG - D6
// QH - D7
class ShiftRegLCDSPI : public ShiftRegLCDBase
{
public:
  ShiftRegLCDSPI(uint8_t srlatch, uint8_t lines);
protected:
  virtual void send(uint8_t, uint8_t) const;
  virtual void send4bits(uint8_t) const;
  virtual void updateAuxPins(void) const;
private:
  void spi_byte(uint8_t out) const;
  void spi_lcd(uint8_t value) const;

  uint8_t _srlatch_pinmask;
  volatile uint8_t *_srlatch_portreg;
};

#ifdef SHIFTREGLCD_NATIVE
#define ShiftRegLCD ShiftRegLCDNative
#else
#define ShiftRegLCD ShiftRegLCDSPI
#endif /* SHIFTREGLCD_NATIVE */

#endif
