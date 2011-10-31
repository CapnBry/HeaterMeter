// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#ifndef __STRINGS_H__
#define __STRINGS_H__

#include <HardwareSerial.h>
#include <avr/pgmspace.h>

//const unsigned char LCD_ARROWUP[] PROGMEM = { 0x4,0xe,0x1f,0x00,0x00,0x4,0xe,0x1f };
//const unsigned char LCD_ARROWDN[] PROGMEM = { 0x1f,0xe,0x4,0x00,0x00,0x1f,0xe,0x4 };

#define CSV_DELIMITER ','

#define DEGREE "\xdf" // \xdf is the degree symbol on the Hitachi HD44780
const prog_char LCD_LINE1_UNPLUGGED[] PROGMEM = "- No Pit Probe -";
const prog_char HM_VERSION[] PROGMEM = "201110310";

inline void Serial_char(char c) { Serial.write(c); }
inline void Serial_nl(void) { Serial_char('\n'); }
inline void Serial_csv(void) { Serial_char(CSV_DELIMITER); }
inline void print_P(const prog_char *s) {  while (unsigned char c = pgm_read_byte(s++)) Serial_char(c); }

#endif /* __STRINGS_H__ */
