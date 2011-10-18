// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#include <WProgram.h>
#include <ShiftRegLCD.h>
#include "hmcore.h"

// See hmcore.h for most options and tweaks

#ifdef HEATERMETER_NETWORKING
// these are redundant but if you don't include them, the Arduino build 
// process won't include them to the temporary build location
#include <WiServer.h>  
#include <dataflash.h>
#include "wishieldconf.h"
#endif /* HEATERMETER_NETWORKING */

#ifdef HEATERMETER_RFM12
#include <RF12.h>
#include <Ports.h>
#endif /* HEATERMETER_RFM12 */

int main(void)
{
  init();
  hmcoreSetup();
  for (;;)
    hmcoreLoop();
  return 0;
}

