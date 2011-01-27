#ifndef __STRINGS_H__
#define __STRINGS_H__

#include <avr\pgmspace.h>

//const unsigned char LCD_ARROWUP[] PROGMEM = { 0x4,0xe,0x1f,0x00,0x00,0x4,0xe,0x1f };
//const unsigned char LCD_ARROWDN[] PROGMEM = { 0x1f,0xe,0x4,0x00,0x00,0x1f,0xe,0x4 };

#define DEGREE "\xdf" // \xdf is the degree symbol on the Hitachi HD44780
const prog_char LCD_LINE1[] PROGMEM = "Pit:%3d"DEGREE"F %c%3u%%%c";
const prog_char LCD_LINE1_DELAYING[] PROGMEM = "Pit:%3d"DEGREE"F Lid%3u";
const prog_char LCD_LINE1_UNPLUGGED[] PROGMEM = "- No Pit Probe -";
const prog_char LCD_LINE2[] PROGMEM = "%-12s%3d"DEGREE;

const prog_char LCD_CONNECTING[] PROGMEM = "Connecting to   ";
const prog_char LCD_SETPOINT1[] PROGMEM = "Set temperature:";
const prog_char LCD_SETPOINT2[] PROGMEM = "%3d"DEGREE"F";
const prog_char LCD_PROBENAME1[] PROGMEM = "Set probe %1d name";
const prog_char LCD_PROBEOFFSET2[] PROGMEM = "Offset %4d"DEGREE"F";
const prog_char LCD_LIDOPENOFFS1[] PROGMEM = "Lid open offset";
const prog_char LCD_LIDOPENOFFS2[] PROGMEM = "%3d"DEGREE"F";
const prog_char LCD_LIDOPENDUR1[] PROGMEM = "Lid open timer";
const prog_char LCD_LIDOPENDUR2[] PROGMEM = "%3d seconds";
const prog_char LCD_MANUALMODE[] PROGMEM = "Manual fan mode";
const prog_char LCD_RESETCONFIG[] PROGMEM = "Reset config?";
const prog_char LCD_MAXFANSPEED[] PROGMEM = "Maximum auto fan";
const prog_char LCD_MAXFANSPEED2[] PROGMEM = "speed %d%%";
const prog_char LCD_YES[] PROGMEM = "Yes";
const prog_char LCD_NO[] PROGMEM = "No ";
const prog_char LCD_CONFIGURE[] PROGMEM = "v probe config v";
const prog_char LCD_PALARM_H_ON[] PROGMEM = "High alarm? ";
const prog_char LCD_PALARM_L_ON[] PROGMEM = "Low alarm? ";
const prog_char LCD_PALARM_H_VAL[] PROGMEM = "High Alrm %4d"DEGREE"F";
const prog_char LCD_PALARM_L_VAL[] PROGMEM = "High Alrm %4d"DEGREE"F";

#ifdef HEATERMETER_NETWORKING
const prog_char WEB_OK[] PROGMEM = "OK\n";
const prog_char WEB_FAILED[] PROGMEM = "FAILED\n";

const prog_char URL_SETPOINT[] PROGMEM = "set?sp=";
const prog_char URL_SETPID[] PROGMEM = "set?pid";
const prog_char URL_SETPNAME[] PROGMEM = "set?pn";
const prog_char URL_SETPOFF[] PROGMEM = "set?po";
const prog_char URL_CSV[] PROGMEM = "csv";
const prog_char URL_JSON[] PROGMEM = "json";
#ifdef DFLASH_LOGGING
const prog_char URL_LOG[] PROGMEM = "log";
#endif  /* DFLASH_LOGGING */

const prog_char JSON1[] PROGMEM = "{\"temps\":[";
const prog_char JSON_T1[] PROGMEM = "{\"n\":\"";
const prog_char JSON_T2[] PROGMEM = "\",\"c\":";
const prog_char JSON_T3[] PROGMEM = ",\"a\":";
const prog_char JSON_T4[] PROGMEM = "},";
const prog_char JSON2[] PROGMEM = "{}],\"set\":";
const prog_char JSON3[] PROGMEM = ",\"lid\":";
const prog_char JSON4[] PROGMEM = ",\"fan\":{\"c\":";
const prog_char JSON5[] PROGMEM = ",\"a\":";
const prog_char JSON6[] PROGMEM = "}}";
#endif /* HEATERMETER_NETWORKING */

const prog_char COMMA[] PROGMEM = ",";
const prog_char PID_ORDER[] PROGMEM = "bpid";

#endif /* __STRINGS_H__ */
