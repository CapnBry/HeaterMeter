// LiquidCrystal library, extended for use over I2C with the LCD Plug
// 2009-09-23 <jcw@equi4.com> http://opensource.org/licenses/mit-license.php
// $Id: PortsLCD.h 4769 2010-01-22 18:59:59Z jcw $

// see http://news.jeelabs.org/2009/09/26/generalized-liquidcrystal-library/

#ifndef LiquidCrystal_h
#define LiquidCrystal_h

#include <inttypes.h>
#include <Print.h>
#include <Ports.h>

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

// this class defines all the basic functionality needed to drive an LCD display
// it is an incomplete (abstract) base class, which needs to be extended for use
// see the LiquidCrystal and LiquidCrystalI2C classes for two usable versions

class LiquidCrystalBase : public Print {
public:
  LiquidCrystalBase () {}
  
  void begin(byte cols, byte rows, byte charsize = LCD_5x8DOTS);

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
  void leftToRight();
  void rightToLeft();
  void autoscroll();
  void noAutoscroll();

  void createChar(byte, byte[]);
  void setCursor(byte, byte); 
  virtual void write(byte);
  void command(byte);
protected:
  virtual void config() =0;
  virtual void send(byte, byte) =0;
  virtual void write4bits(byte) =0;

  byte _displayfunction;
  byte _displaycontrol;
  byte _displaymode;
  byte _initialized;
  byte _numlines,_currline;
};

// This class can be used to create an object with drives an LCD through many
// different I/O pins, connected to the display in parallel - it is equivalent
// to the LiquidCrystal class defined in the Arduino, but has be adjusted to
// work with the above LiquidCrystalBase instead.

class LiquidCrystal : public LiquidCrystalBase {
public:
  LiquidCrystal(byte rs, byte enable,
		byte d0, byte d1, byte d2, byte d3, byte d4, byte d5, byte d6, byte d7);
  LiquidCrystal(byte rs, byte rw, byte enable,
		byte d0, byte d1, byte d2, byte d3, byte d4, byte d5, byte d6, byte d7);
  LiquidCrystal(byte rs, byte rw, byte enable,
		byte d0, byte d1, byte d2, byte d3);
  LiquidCrystal(byte rs, byte enable,
		byte d0, byte d1, byte d2, byte d3);

  void init(byte fourbitmode, byte rs, byte rw, byte enable,
	    byte d0, byte d1, byte d2, byte d3, byte d4, byte d5, byte d6, byte d7);
  
  virtual void config();
  virtual void send(byte, byte);
  virtual void write4bits(byte);

  void write8bits(byte);
  void pulseEnable();

  byte _rs_pin; // LOW: command.  HIGH: character.
  byte _rw_pin; // LOW: write to LCD.  HIGH: read from LCD.
  byte _enable_pin; // activated by a HIGH pulse.
  byte _data_pins[8];
};

// This class allows driving an LCD connected via I2C using an LCD Plug, which
// is in turn based on an MCP23008 I2C I/O expander chip and some other parts.
// The available functions include all those of the LiquidCrystal class.

class LiquidCrystalI2C : public LiquidCrystalBase {
  DeviceI2C device;
public:
  LiquidCrystalI2C (const PortI2C& p, byte addr =0x24);
  
  // this display can also turn the back-light on or off
  void backlight();
  void noBacklight();
protected:
  virtual void config();
  virtual void send(byte, byte);
  virtual void write4bits(byte);
};

#endif
