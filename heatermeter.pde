#include <WProgram.h>
#include <ShiftRegLCD.h>
#include "hmcore.h"

// See hmcore.h for most options and tweaks

#ifdef HEATERMETER_NETWORKING
// these are redundant but if you don't include them, the Arduino build 
// process won't include them to the temporary build location
#include <WiServer.h>  
#include <dataflash.h>
// Wireless configuration parameters ----------------------------------------
unsigned char local_ip[] = {192,168,1,252};	// IP address of WiShield
unsigned char gateway_ip[] = {192,168,1,1};	// router or gateway IP address
unsigned char subnet_mask[] = {255,255,255,0};	// subnet mask for the local network
char ssid[] = {"M75FE"};		// max 32 bytes

unsigned char security_type = 1;	// 0 - open; 1 - WEP; 2 - WPA; 3 - WPA2; 4 - WPA_PRECALC; 5 - WPA2_PRECALC
// What goes in security_data depends on your security_type above.  Uncomment the correct
// format for your type of security and fill in the proper value
// security_type = 0 (Open)
//const prog_char security_data[] PROGMEM = {};
// security_type = 1 (WEP)
// WEP 128-bit key, only key 1 because that's all we use
const prog_char security_data[] PROGMEM = { 0xec, 0xa8, 0x1a, 0xb4, 0x65, 0xf0, 0x0d, 0xbe, 0xef, 0xde, 0xad, 0x00, 0x00};
// security_type = 2/3 (WPA/WPA2)
// WPA/WPA2 passphrase, 8-63 characters
//const prog_char security_data[] PROGMEM = {"mypassword"};
// security_type = 4/5 (WPA/WPA2 precalc)
// WPA/WPA2 precalculated key data, always 32 bytes
//const prog_char security_data[] PROGMEM = {   // capnbry24:mypassword
//    0x62, 0xd9, 0xd3, 0x85, 0xce, 0x54, 0x13, 0xdc, 
//    0xaf, 0xd6, 0x39, 0x5a, 0x83, 0x2c, 0x8d, 0x3b, 
//    0xd0, 0xa9, 0xf7, 0x13, 0x97, 0x0f, 0xe1, 0x7c, 
//    0x3e, 0x4f, 0x01, 0x17, 0x6c, 0xba, 0xd9, 0xbe,
// };

// setup the wireless mode
// infrastructure - connect to AP
// adhoc - connect to another WiFi device
unsigned char wireless_mode = WIRELESS_MODE_INFRA;

unsigned char ssid_len;
// End of wireless configuration parameters ----------------------------------------
#endif /* HEATERMETER_NETWORKING */

void setup(void)
{
  hmcoreSetup();
}

void loop(void)
{
  hmcoreLoop();
}

