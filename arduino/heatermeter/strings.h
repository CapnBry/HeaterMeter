// HeaterMeter Copyright 2016 Bryan Mayland <bmayland@capnbry.net>
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
#define HM_VERSION "20180706"

const char LCD_LINE1_UNPLUGGED[] PROGMEM = "- No Pit Probe -";

const char LCD_PROBETYPE_DISABLED[] PROGMEM = "Disable";
const char LCD_PROBETYPE_THERMISTOR[] PROGMEM = "Thermis";
const char LCD_PROBETYPE_RFM12[] PROGMEM = "RFWirel";
const char LCD_PROBETYPE_THERMOCOUPLE[] PROGMEM = "Kcouple";
const char * const LCD_PROBETYPES[] PROGMEM = {
  LCD_PROBETYPE_DISABLED, LCD_PROBETYPE_THERMISTOR, LCD_PROBETYPE_RFM12, LCD_PROBETYPE_THERMOCOUPLE
};

inline void Serial_char(char c) { SerialX.write(c); }
inline void Serial_nl(void) { SerialX.nl(); }
inline void Serial_csv(void) { Serial_char(CSV_DELIMITER[0]); }
inline void print_P(const char *s) {  while (unsigned char c = pgm_read_byte(s++)) Serial_char(c); }

#endif /* __STRINGS_H__ */
