#ifndef __STRINGS_H__
#define __STRINGS_H__

#include <avr/pgmspace.h>

//const unsigned char LCD_ARROWUP[] PROGMEM = { 0x4,0xe,0x1f,0x00,0x00,0x4,0xe,0x1f };
//const unsigned char LCD_ARROWDN[] PROGMEM = { 0x1f,0xe,0x4,0x00,0x00,0x1f,0xe,0x4 };

#define DEGREE "\xdf" // \xdf is the degree symbol on the Hitachi HD44780
const prog_char LCD_LINE1_UNPLUGGED[] PROGMEM = "- No Pit Probe -";

#endif /* __STRINGS_H__ */
