// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#ifndef __HMCORE_H__
#define __HMCORE_H__

#include "strings.h"

#define HEATERMETER_SERIAL 38400 // enable serial interface
#define HEATERMETER_RFM12  RF12_915MHZ  // enable RFM12B receiving (433MHZ|868MHZ|915MHZ)
//#define USE_EXTERNAL_VREF       // Using external 5V as reference to analog inputs
#define PIEZO_HZ 4000             // enable piezo buzzer at this frequency
#if HM_BOARD_REV == 'A'
#define SHIFTREGLCD_NATIVE        // Use the native shift register instead of SPI (HM PCB <v3.2)
#endif

#include "ShiftRegLCD.h"
#include "serialxor.h"
#include "grillpid.h"
#include "hmmenus.h"

// Analog Pins
// Number in the comment is physical pin on ATMega328
#define PIN_PIT     5       // 28
#define PIN_FOOD1   4       // 27
#define PIN_FOOD2   3       // 26
#define PIN_AMB     2       // 25
#define PIN_BUTTONS 0       // 23
// Digital Output Pins
#define PIN_SERIALRX     0  // 2 Can not be changed
#define PIN_SERIALTX     1  // 3 Can not be changed
#define PIN_INTERRUPT    2  // 4
#define PIN_BLOWER       3  // 5
#define PIN_LCD_CLK      4  // 6
#define PIN_LCD_BACKLGHT 5  // 11
#define PIN_ALARM        6  // 12
#define PIN_SOFTRESET    7  // 13 DataFlash SS on WiShield
#define PIN_SERVO        8  // 14 LCD_DATA on < HM PCB v3.2
#define PIN_WIRELESS_LED 9  // 15
#define PIN_SPI_SS      10  // 16
#define PIN_SPI_MOSI    11  // 17 Can not be changed
#define PIN_SPI_MISO    12  // 18 Can not be changed
#define PIN_SPI_SCK     13  // 19 Can not be changed

void hmcoreSetup(void);
void hmcoreLoop(void);

void updateDisplay(void);
void lcdprint_P(const char PROGMEM *p, const boolean doClear);

void eepromLoadConfig(unsigned char forceDefault);
void storeSetPoint(int sp);
void loadProbeName(unsigned char probeIndex);
void storeAndReportProbeName(unsigned char probeIndex, const char *name);
void storeAndReportProbeOffset(unsigned char probeIndex, int offset);
void storeProbeAlarmOn(unsigned char probeIndex, boolean isHigh, boolean value);
void storeProbeAlarmVal(unsigned char probeIndex, boolean isHigh, int value);
void storeAndReportMaxFanSpeed(unsigned char maxFanSpeed);
void setLcdBacklight(unsigned char lcdBacklight);
void storeLcdBacklight(unsigned char lcdBacklight);
void reportLcdParameters(void);
void Debug_begin(void);
#define Debug_end Serial_nl
void silenceRingingAlarm(void);

#define LIDPARAM_OFFSET 0
#define LIDPARAM_DURATION 1
#define LIDPARAM_ACTIVE 2
void storeLidParam(unsigned char idx, int val);

extern GrillPid pid;
extern ShiftRegLCD lcd;
extern unsigned char g_LcdBacklight;

#endif /* __HMCORE_H__ */
