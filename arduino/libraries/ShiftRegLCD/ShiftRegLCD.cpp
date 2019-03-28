// ShiftRegLCD  - Shiftregister-based LCD library for Arduino
//
// Connects an LCD using 2 or 3 pins from the Arduino, via an 8-bit ShiftRegister (SR from now on).
//
// Acknowledgements:
//
// Based very much on the "official" LiquidCrystal library for the Arduino:
//     ttp://arduino.cc/en/Reference/Libraries
// and also the improved version (with examples CustomChar1 and SerialDisplay) from LadyAda:
//     http://web.alfredstate.edu/weimandn/arduino/LiquidCrystal_library/LiquidCrystal_index.html
// and also inspired by this schematics from an unknown author (thanks to mircho on the arduino playground forum!):
//     http://www.scienceprog.com/interfacing-lcd-to-atmega-using-two-wires/
//
// Modified to work serially with the shiftOut() function, an 8-bit shiftregister (SR) and an LCD in 4-bit mode.
//
// Shiftregister connection description (NEW as of 2007.07.27)
//
// Bit  #0 - N/C - not connected, used to hold a zero
// Bits #1 - N/C
// Bit  #2 - connects to RS (Register Select) on the LCD
// Bits #3 - #6 from SR connects to LCD data inputs D4 - D7.
// Bit  #7 - is used to enabling the enable-puls (via a diode-resistor AND "gate")
//
// 2 or 3 Pins required from the Arduino for Data, Clock, and Enable (optional). If not using Enable,
// the Data pin is used for the enable signal by defining the same pin for Enable as for Data.
// Data and Clock outputs/pins goes to the shiftregister.
// LCD RW-pin hardwired to LOW (only writing to LCD). Busy Flag (BF, data bit D7) is not read.
//
// Any shift register should do. I used 74LS164, for the reason that's what I had at hand.
//
//       Project homepage: http://code.google.com/p/arduinoshiftreglcd/
//
// History
// 2009.05.23  raron, but; based mostly (as in almost verbatim) on the "official" LiquidCrystal library.
// 2009.07.23  raron (yes exactly 2 months later). Incorporated some proper initialization routines
//               inspired (lets say copy-paste-tweaked) from LiquidCrystal library improvements from LadyAda
//               Also a little re-read of the datasheet for the HD44780 LCD controller.
// 2009.07.25  raron - Fixed comments. I really messed up the comments before posting this, so I had to fix it.
//               Also renamed a function, but no improvements or functional changes.
// 2009.07.27  Thanks to an excellent suggestion from mircho at the Arduiono playgrond forum,
//               the number of wires now required is only two!
// 2009.07.28  Mircho / raron - a new modification to the schematics, and a more streamlined interface
// 2009.07.30  raron - minor corrections to the comments. Fixed keyword highlights. Fixed timing to datasheet safe.

#include "ShiftRegLCD.h"
#include <stdio.h>
#include <string.h>
#include "Arduino.h"

void ShiftRegLCDBase::init(uint8_t lines, uint8_t font)
{
  if (lines>1)
  	_numlines = LCD_2LINE;
  else
  	_numlines = LCD_1LINE;

  _displayfunction = LCD_4BITMODE | _numlines;

  // For some displays you can select a 10 pixel high font
  // (Just in case I removed the neccesity to have 1-line display for this)
  if (font != 0)
    _displayfunction |= LCD_5x10DOTS;
  else
    _displayfunction |= LCD_5x8DOTS;

  if (_backlight_pin != 0)
    pinMode(_backlight_pin, OUTPUT);

  // At this time this is for 4-bit mode only, as described above.
  // Page 47-ish of this (HD44780 LCD) datasheet:
  //    http://www.datasheetarchive.com/pdf-datasheets/Datasheets-13/DSA-247674.pdf
  // According to datasheet, we need at least 40ms after power rises above 2.7V
  // before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50ms
  delayMicroseconds(0x4000);
  delayMicroseconds(0x4000);
  delayMicroseconds(0x4000);
  send4bits(LCD_FUNCTIONSET | LCD_8BITMODE);
  delayMicroseconds(4500);  // wait more than 4.1ms
  // Second try
  send4bits(LCD_FUNCTIONSET | LCD_8BITMODE);
  delayMicroseconds(150);
  // Third go
  send4bits(LCD_FUNCTIONSET | LCD_8BITMODE);

  // And finally, set to 4-bit interface
  send4bits(LCD_FUNCTIONSET | LCD_4BITMODE);

  // Set # lines, font size, etc.
  command(LCD_FUNCTIONSET | _displayfunction);
  // Turn the display on with no cursor or blinking default
  _displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
  display();
  // Clear it off
  clear();
  // Initialize to default text direction (for romance languages)
  _displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
  // set the entry mode
  command(LCD_ENTRYMODESET | _displaymode);
}

// ********** high level commands, for the user! **********
void ShiftRegLCDBase::clear()
{
  command(LCD_CLEARDISPLAY);  // clear display, set cursor position to zero
  delayMicroseconds(2000);    // this command takes a long time!
}

void ShiftRegLCDBase::home()
{
  command(LCD_RETURNHOME);  // set cursor position to zero
  delayMicroseconds(2000);  // this command takes a long time!
}

void ShiftRegLCDBase::setCursor(uint8_t col, uint8_t row)
{
  const uint8_t row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
  if ( row > _numlines )
    row = _numlines-1;    // we count rows starting w/0
  command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

// Turn the display on/off (quickly)
void ShiftRegLCDBase::noDisplay() {
  _displaycontrol &= ~LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void ShiftRegLCDBase::display() {
  _displaycontrol |= LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turns the underline cursor on/off
void ShiftRegLCDBase::noCursor() {
  _displaycontrol &= ~LCD_CURSORON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void ShiftRegLCDBase::cursor() {
  _displaycontrol |= LCD_CURSORON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turn on and off the blinking cursor
void ShiftRegLCDBase::noBlink() {
  _displaycontrol &= ~LCD_BLINKON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void ShiftRegLCDBase::blink() {
  _displaycontrol |= LCD_BLINKON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// These commands scroll the display without changing the RAM
void ShiftRegLCDBase::scrollDisplayLeft(void) {
  command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void ShiftRegLCDBase::scrollDisplayRight(void) {
  command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

// This is for text that flows Left to Right
void ShiftRegLCDBase::shiftLeft(void) {
  _displaymode |= LCD_ENTRYLEFT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This is for text that flows Right to Left
void ShiftRegLCDBase::shiftRight(void) {
  _displaymode &= ~LCD_ENTRYLEFT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'right justify' text from the cursor
void ShiftRegLCDBase::shiftIncrement(void) {
  _displaymode |= LCD_ENTRYSHIFTINCREMENT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'left justify' text from the cursor
void ShiftRegLCDBase::shiftDecrement(void) {
  _displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// Allows us to fill the first 8 CGRAM locations with custom characters
void ShiftRegLCDBase::createChar(uint8_t location, uint8_t charmap[]) {
  location &= 0x7; // we only have 8 locations 0-7
  command(LCD_SETCGRAMADDR | location << 3);
  write(charmap, 8);
  command(LCD_SETDDRAMADDR); // Reset display to display text (from pos. 0)
}

void ShiftRegLCDBase::createChar_P(uint8_t location, const char *p) {
  location &= 0x7; // we only have 8 locations 0-7
  command(LCD_SETCGRAMADDR | location << 3);
  write_P(p, 8);
  command(LCD_SETDDRAMADDR); // Reset display to display text (from pos. 0)
}

void ShiftRegLCDBase::digitalWrite(uint8_t pin, uint8_t val)
{
  // Only 2 pins (0 and 1) available
  if (pin > 1)
    return;

  pin = 1 << pin;
  if (val)
    _auxPins |= pin;
  else
    _auxPins &= ~pin;
  updateAuxPins();
}

void ShiftRegLCDBase::write_P(const char *p, uint8_t len)
{
  while (len--)
    write(pgm_read_byte(p++));
}

void ShiftRegLCDBase::setBacklight(uint8_t v, bool store)
{
  if (_backlight_pin != 0)
  {
    v = constrain(v, 0, 100);
    if (store)
      _backlight = v;
    analogWrite(_backlight_pin, (uint16_t)v * 255 / 100);
  }
}

void ShiftRegLCDBase::command(uint8_t value) {
  send(value, LOW);
}

size_t ShiftRegLCDBase::write(uint8_t value) {
  send(value, HIGH);
  return sizeof(value);
}

void ShiftRegLCDNative::ctor(uint8_t srdata, uint8_t srclock, uint8_t enable, uint8_t lines, uint8_t font)
{
  _two_wire = 0;
  _srdata_pin = srdata; _srclock_pin = srclock; _enable_pin = enable;
  if (enable == TWO_WIRE)
  {
	_enable_pin = _srdata_pin;
	_two_wire = 1;
  }
  pinMode(_srclock_pin, OUTPUT);
  pinMode(_srdata_pin, OUTPUT);
  pinMode(_enable_pin, OUTPUT);

  init(lines, font);
};

void ShiftRegLCDNative::send(uint8_t value, uint8_t mode) const
{
  uint8_t val1, val2;
  if ( _two_wire ) shiftOut ( _srdata_pin, _srclock_pin, MSBFIRST, 0x00 ); // clear shiftregister
  ::digitalWrite( _enable_pin, LOW );
  mode = mode ? SR_RS_BIT : 0; // RS bit; LOW: command.  HIGH: character.
  mode |= _auxPins;
  val1 = mode | SR_EN_BIT | ((value >> 1) & 0x78); // upper nibble
  val2 = mode | SR_EN_BIT | ((value << 3) & 0x78); // lower nibble
  shiftOut ( _srdata_pin, _srclock_pin, MSBFIRST, val1 );
  ::digitalWrite( _enable_pin, HIGH );
  delayMicroseconds(1);                 // enable pulse must be >450ns
  ::digitalWrite( _enable_pin, LOW );
  if ( _two_wire ) shiftOut ( _srdata_pin, _srclock_pin, MSBFIRST, 0x00 ); // clear shiftregister
  shiftOut ( _srdata_pin, _srclock_pin, MSBFIRST, val2 );
  ::digitalWrite( _enable_pin, HIGH );
  delayMicroseconds(1);                 // enable pulse must be >450ns
  ::digitalWrite( _enable_pin, LOW );
  delayMicroseconds(40);               // commands need > 37us to settle
}

void ShiftRegLCDNative::send4bits(uint8_t value) const
{
  uint8_t val1;
  ::digitalWrite( _enable_pin, LOW );
  if ( _two_wire ) shiftOut ( _srdata_pin, _srclock_pin, MSBFIRST, 0x00 ); // clear shiftregister
  val1 = SR_EN_BIT | ((value >> 1) & 0x78);
  shiftOut ( _srdata_pin, _srclock_pin, MSBFIRST, val1 );
  ::digitalWrite( _enable_pin, HIGH );
  delayMicroseconds(1);                 // enable pulse must be >450ns
  ::digitalWrite( _enable_pin, LOW );
  delayMicroseconds(40);               // commands need > 37us to settle
}

void ShiftRegLCDNative::updateAuxPins(void) const
{
  if ( _two_wire ) shiftOut ( _srdata_pin, _srclock_pin, MSBFIRST, 0x00 ); // clear shiftregister
  shiftOut( _srdata_pin, _srclock_pin, MSBFIRST, _auxPins);
}

ShiftRegLCDSPI::ShiftRegLCDSPI(uint8_t backlight, uint8_t srlatch, uint8_t lines) : ShiftRegLCDBase(backlight)
{
  pinMode(MOSI, OUTPUT); // Shift Register (Serial Input) Data
  pinMode(SCK, OUTPUT);  // Shift Register Clock
  //MISO pin automatically overrides to INPUT.
  //pinMode(MISO, INPUT);
  // SS must be set OUTPUT/HIGH. If it goes to INPUT/LOW,
  // the ATmega automatically switches to SPI slave mode
  pinMode(SS, OUTPUT);
  ::digitalWrite(SS, HIGH);
  // SPI = clk/2
  SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPI2X);
  // SPI = clk/4
  //SPCR = _BV(SPE) | _BV(MSTR);
  // SPI = clk/8
  //SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0) | _BV(SPI2X);
  // SPI = clk/16
  //SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0);
  // SPI = clk/128
  //SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0) | _BV(SPR1);

  _srlatch_pinmask = digitalPinToBitMask(srlatch);
  _srlatch_portreg = portOutputRegister(digitalPinToPort(srlatch));
  pinMode(srlatch, OUTPUT);

  // The enable line may have been left high by something so we need to clear it
  spi_byte(0);

  init(lines, 0);
}

// Write a byte out SPI and toggle the Storage Register Clock
void ShiftRegLCDSPI::spi_byte(uint8_t out) const
{
  // disable interrupts during the transfer
  unsigned char sreg = SREG;
  cli();

  SPDR = out;
  while (!(SPSR & _BV(SPIF)))
      ;
  // Data needs to 40ns setup time and storage clock needs 16ns pulse
  // the 1us delay here covers all that
  *_srlatch_portreg |= _srlatch_pinmask;
  delayMicroseconds(1);
  *_srlatch_portreg &= ~_srlatch_pinmask;

  // re-enable interupts
  SREG = sreg;
}

void ShiftRegLCDSPI::spi_lcd(uint8_t value) const
{
  // The datasheet says that RS needs to be set-up 60ns before E goes
  // high but in my testing this caused the display to get wrecked
  //spi_byte(value);

  value |= SPI_LCD_E;
  spi_byte(value);
  // Enable needs to be HIGH for >450ns but sending a byte through SPI
  // at 8MHz takes 1uS so there's no need to wait

  value &= ~SPI_LCD_E;
  spi_byte(value);
}

void ShiftRegLCDSPI::send(uint8_t value, uint8_t mode) const
{
  uint8_t val;
  mode = mode ? SPI_LCD_RS : 0; // RS bit; LOW: command.  HIGH: character.
  mode |= _auxPins;

  val = mode | (value & 0xf0); // upper nibble
  spi_lcd(val);
  val = mode | (value << 4);   // lower nibble
  spi_lcd(val);

  delayMicroseconds(40);               // commands need > 37us to settle
}

void ShiftRegLCDSPI::send4bits(uint8_t value) const
{
  spi_lcd(value & 0xf0);
  delayMicroseconds(40);               // commands need > 37us to settle
}

void ShiftRegLCDSPI::updateAuxPins(void) const
{
  spi_byte(_auxPins);
}
