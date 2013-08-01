// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#ifndef __STRINGS_H__
#define __STRINGS_H__

#include "serialxor.h"
#include <avr/pgmspace.h>

//const unsigned char LCD_ARROWUP[] PROGMEM = { 0x4,0xe,0x1f,0x00,0x00,0x4,0xe,0x1f };
//const unsigned char LCD_ARROWDN[] PROGMEM = { 0x1f,0xe,0x4,0x00,0x00,0x1f,0xe,0x4 };

#define CSV_DELIMITER ","

#ifndef HM_BOARD_REV
#define HM_BOARD_REV 'B'
#endif

#define DEGREE "\xdf" // \xdf is the degree symbol on the Hitachi HD44780
#define HM_VERSION "20130731"

const char LCD_LINE1_UNPLUGGED[] PROGMEM = "- No Pit Probe -";

inline void Serial_char(char c) { SerialX.write(c); }
inline void Serial_nl(void) { SerialX.nl(); }
inline void Serial_csv(void) { Serial_char(CSV_DELIMITER[0]); }
inline void print_P(const char PROGMEM *s) {  while (unsigned char c = pgm_read_byte(s++)) Serial_char(c); }

#endif /* __STRINGS_H__ */
