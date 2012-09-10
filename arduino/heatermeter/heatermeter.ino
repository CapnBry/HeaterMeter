// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#include "ShiftRegLCD.h"
#include "hmcore.h"

// See hmcore.h for most options and tweaks

#ifdef HEATERMETER_NETWORKING
// these are redundant but if you don't include them, the Arduino build 
// process won't include them to the temporary build location
#include "WiServer.h"
#include "dataflash.h"
#include "wishieldconf.h"
#endif /* HEATERMETER_NETWORKING */

#ifdef HEATERMETER_RFM12
#include "JeeLib.h"
#endif /* HEATERMETER_RFM12 */

/* Disable the watchdog timer immediately after zero_reg is set */
void clearWdt(void) __attribute__((naked)) __attribute__((section(".init3")));
void clearWdt(void)
{
  MCUSR = 0;
  WDTCSR = _BV(WDCE) | _BV(WDE);
  WDTCSR = 0;
}

int main(void)
{
  init();
  hmcoreSetup();
  for (;;)
    hmcoreLoop();
  return 0;
}

