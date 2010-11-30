#include <WProgram.h>
#include <ShiftRegLCD.h>
#include "hmcore.h"

#ifdef HEATERMETER_NETWORKING
#include <WiServer.h>  
#include <dataflash.h>
// Wireless configuration parameters ----------------------------------------
unsigned char local_ip[] = {192,168,1,252};	// IP address of WiShield
unsigned char gateway_ip[] = {192,168,1,1};	// router or gateway IP address
unsigned char subnet_mask[] = {255,255,255,0};	// subnet mask for the local network
const prog_char ssid[] PROGMEM = {"M75FE"};		// max 32 bytes

unsigned char security_type = 1;	// 0 - open; 1 - WEP; 2 - WPA; 3 - WPA2; 4 - WPA_PRECALC; 5 - WPA2_PRECALC
// WPA/WPA2 passphrase
const prog_char security_passphrase[] PROGMEM = {""};	// 8-63 characters
// http://www.xs4all.nl/~rjoris/wpapsk.html
//prog_uchar wpa_psk[] PROGMEM = { }; // capnbry24:ldsi11744a 62d9d385ce5413dcafd6395a832c8d3bd0a9f713970fe17c3e4f01176cbad9be
// WEP 128-bit key, only key 1 because that's all we use
prog_uchar wep_keys[] PROGMEM = { 0xEC, 0xA8, 0x1A, 0xB4, 0x65, 0xf0, 0x0d, 0xbe, 0xef, 0xde, 0xad, 0x00, 0x00};

// setup the wireless mode
// infrastructure - connect to AP
// adhoc - connect to another WiFi device
unsigned char wireless_mode = WIRELESS_MODE_INFRA;

unsigned char ssid_len;
unsigned char security_passphrase_len;
// End of wireless configuration parameters ----------------------------------------
#endif /* HEATERMETER_NETWORKING */

void setup(void)
{
  Serial.begin(115200);
  hmcoreSetup();
}

void loop(void)
{
  hmcoreLoop();
}

