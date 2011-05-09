#ifndef __HMCORE_H__
#define __HMCORE_H__

#ifdef __cplusplus
extern "C" {
#endif
 
//#define HEATERMETER_NETWORKING  // enable wifi interface
#define HEATERMETER_SERIAL      // enable serial interface
//#define USE_EXTERNAL_VREF       // Using external 5V as reference to analog inputs

#ifdef HEATERMETER_NETWORKING
#define DFLASH_SERVING          // Serve web pages out of dflash
#endif

#include <ShiftRegLCD.h>
#include "grillpid.h"
#include "hmmenus.h"

// Analog Pins
#define PIN_PIT     5
#define PIN_FOOD1   4
#define PIN_FOOD2   3
#define PIN_AMB     2
#define PIN_BUTTONS 0
// Digital Output Pins
#define PIN_SERIALRX     0  // Can not be changed
#define PIN_SERIALTX     1  // Can not be changed
#define PIN_WIFI_INT     2
#define PIN_BLOWER       3
#define PIN_LCD_CLK      4
#define PIN_LCD_BACKLGHT 5
#define PIN_ALARM        6
#define PIN_DATAFLASH_SS 7
#define PIN_LCD_DATA     8
#define PIN_WIFI_LED     9
#define PIN_WIFI_SS     10
#define PIN_SPI_MOSI    11  // Can not be changed
#define PIN_SPI_MISO    12  // Can not be changed
#define PIN_SPI_SCK     13  // Can not be changed

const char CSV_DELIMITER = ',';

void hmcoreSetup(void);
void hmcoreLoop(void);

void updateDisplay(void);
void lcdprint_P(const prog_char *p, const boolean doClear);

void eepromLoadConfig(boolean forceDefault);
void storeSetPoint(int sp);
void storeProbeName(unsigned char probeIndex, const char *name);
void loadProbeName(unsigned char probeIndex);
void storeProbeOffset(unsigned char probeIndex, char offset);
void storeProbeAlarmOn(unsigned char probeIndex, boolean isHigh, boolean value);
void storeProbeAlarmVal(unsigned char probeIndex, boolean isHigh, int value);
void storePidParam(char which, float value);
void storeMaxFanSpeed(unsigned char maxFanSpeed);
void storeLidOpenOffset(unsigned char value);
void storeLidOpenDuration(unsigned int value);

extern GrillPid pid;
extern ShiftRegLCD lcd;

#ifdef __cplusplus
}
#endif

#endif /* __HMCORE_H__ */
