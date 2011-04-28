// 3-wire connection to a HD44780-compatible LCD via shiftregister with:
//   Data on pin 12
//   Clock on pin 13
//   Enable on pin 8
//
// Shiftregister bits #0 - #1 - not used (bit #0 must be set to zero)
// Shiftregister bit  #2 - connects to LCD RS (Register Select)
// Shiftregister bits #3 - #6 on LCD data inputs D4-D7.
// Shiftregister bit  #7 - used for enabling the Enable puls.
// LCD R/!W (or rw) hardwired to GND (write to LCD only, no reading from)
//
// USAGE: ShiftRegLCD LCDobjectvariablename(Datapin, Clockpin, Enablepin or TWO_WIRE [, Lines [, Font]]])
//   where Lines and Font are optional.
//     Enablepin: can be replaced by constant TWO_WIRE, if using only 2 wires.
//     Lines: 1 or 2 lines (or more if possible)
//     Font : 0 or 1, small or big font (8 or 10 pixel tall font, if available).

#include <ShiftRegLCD.h>

ShiftRegLCD srlcd(12, 13, 8);

void setup()
{
  // Print a message to the LCD.
  srlcd.print("HELLO, WORLD!");
}

void loop()
{
}
