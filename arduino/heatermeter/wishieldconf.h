// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net>
#ifndef __WISHIELDCONF_H__
#define __WISHIELDCONF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <avr/pgmspace.h>

// Wireless configuration parameters ----------------------------------------
unsigned char local_ip[] = {192,168,2,252};	// IP address of WiShield
unsigned char gateway_ip[] = {192,168,2,1};	// router or gateway IP address
unsigned char subnet_mask[] = {255,255,255,0};	// subnet mask for the local network
char ssid[] = {"capnbry24"};		// max 32 bytes

unsigned char security_type = 5;	// 0 - open; 1 - WEP; 2 - WPA; 3 - WPA2; 4 - WPA Precalc; 5 - WPA2 Precalc

// Depending on your security_type, uncomment the appropriate type of security_data
// 0 - None (open)
//const prog_char security_data[] PROGMEM = {};

// 1 - WEP 
// UIP_WEP_KEY_LEN. 5 bytes for 64-bit key, 13 bytes for 128-bit key
// Only supply the appropriate key, do not specify 4 keys and then try to specify which to use
//const prog_char security_data[] PROGMEM = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, };

// 2, 3 - WPA/WPA2 Passphrase
// 8 to 63 characters which will be used to generate the 32 byte calculated key
// Expect the g2100 to take 30 seconds to calculate the key from a passphrase
//const prog_char security_data[] PROGMEM = {"password"};

// 4, 5 - WPA/WPA2 Precalc
// The 32 byte precalculate WPA/WPA2 key. This can be calculated in advance to save boot time
// http://jorisvr.nl/wpapsk.html
const prog_char security_data[] PROGMEM = {   // capnbry24:mypassword
    0x62, 0xd9, 0xd3, 0x85, 0xce, 0x54, 0x13, 0xdc, 0xaf, 0xd6, 0x39, 0x5a, 0x83, 0x2c, 0x8d, 0x3b, 
    0xd0, 0xa9, 0xf7, 0x13, 0x97, 0x0f, 0xe1, 0x7c, 0x3e, 0x4f, 0x01, 0x17, 0x6c, 0xba, 0xd9, 0xbe,
};

// setup the wireless mode
// WIRELESS_MODE_INFRA - connect to AP
// WIRELESS_MODE_ADHOC - connect to another WiFi device
unsigned char wireless_mode = WIRELESS_MODE_INFRA;
// End of wireless configuration parameters ----------------------------------------

#ifdef __cplusplus
}
#endif

#endif  /* __WISHIELDCONF_H__ */

