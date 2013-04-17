// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#include "ShiftRegLCD.h"
#include "hmcore.h"

// See hmcore.h for most options and tweaks

#ifdef HEATERMETER_RFM12
#include "rf12_itplus.h"
#endif /* HEATERMETER_RFM12 */

/* Disable the watchdog timer immediately after zero_reg is set */
__attribute__((naked)) __attribute__((section(".init3")))
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

