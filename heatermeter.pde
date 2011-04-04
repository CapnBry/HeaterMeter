#include <WProgram.h>
#include <ShiftRegLCD.h>
#include "hmcore.h"

// See hmcore.h for most options and tweaks

#ifdef HEATERMETER_NETWORKING
// these are redundant but if you don't include them, the Arduino build 
// process won't include them to the temporary build location
#include <WiServer.h>  
#include <dataflash.h>
#include "WiShieldConf.h"
#endif /* HEATERMETER_NETWORKING */

void setup(void)
{
  hmcoreSetup();
}

void loop(void)
{
  hmcoreLoop();
}

