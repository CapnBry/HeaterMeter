// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#ifndef __HMMENUS_H__
#define __HMMENUS_H__

#include <avr/pgmspace.h>
#include "menus.h"

#define BUTTON_LEFT  (1<<0)
#define BUTTON_RIGHT (1<<1)
#define BUTTON_UP    (1<<2)
#define BUTTON_DOWN  (1<<3)
#define BUTTON_4     (1<<4)

#define ST_HOME_FOOD1 (ST_VMAX+1) // ST_HOME_X must stay sequential and in order
#define ST_HOME_FOOD2 (ST_VMAX+2)
#define ST_HOME_AMB   (ST_VMAX+3)
#define ST_HOME_NOPROBES (ST_VMAX+4)
#define ST_CONNECTING (ST_VMAX+5)
#define ST_SETPOINT   (ST_VMAX+6)
#define ST_PROBENAME1 (ST_VMAX+7)  // ST_PROBENAMEX must stay sequential and in order
#define ST_PROBENAME2 (ST_VMAX+8)
#define ST_PROBENAME3 (ST_VMAX+9)
#define ST_PROBEOFF0  (ST_VMAX+10) // ST_PROBEOFFX must stay sequential and in order
#define ST_PROBEOFF1  (ST_VMAX+11)
#define ST_PROBEOFF2  (ST_VMAX+12)
#define ST_PROBEOFF3  (ST_VMAX+13)
#define ST_LIDOPEN_OFF (ST_VMAX+14)
#define ST_LIDOPEN_DUR (ST_VMAX+15)
#define ST_MANUALMODE  (ST_VMAX+16)
#define ST_RESETCONFIG (ST_VMAX+17)
#define ST_MAXFANSPEED (ST_VMAX+18)
#define ST_PROBESUB0   (ST_VMAX+19)  // ST_PROBESUBX must stay sequential and in order
#define ST_PROBESUB1   (ST_VMAX+20)
#define ST_PROBESUB2   (ST_VMAX+21)
#define ST_PROBESUB3   (ST_VMAX+22)
#define ST_PALARM0_H_ON  (ST_VMAX+23)
#define ST_PALARM1_H_ON  (ST_VMAX+24)
#define ST_PALARM2_H_ON  (ST_VMAX+25)
#define ST_PALARM3_H_ON  (ST_VMAX+26)
#define ST_PALARM0_H_VAL (ST_VMAX+27)
#define ST_PALARM1_H_VAL (ST_VMAX+28)
#define ST_PALARM2_H_VAL (ST_VMAX+29)
#define ST_PALARM3_H_VAL (ST_VMAX+30)
#define ST_PALARM0_L_ON  (ST_VMAX+31)
#define ST_PALARM1_L_ON  (ST_VMAX+32)
#define ST_PALARM2_L_ON  (ST_VMAX+33)
#define ST_PALARM3_L_ON  (ST_VMAX+34)
#define ST_PALARM0_L_VAL (ST_VMAX+35)
#define ST_PALARM1_L_VAL (ST_VMAX+36)
#define ST_PALARM2_L_VAL (ST_VMAX+37)
#define ST_PALARM3_L_VAL (ST_VMAX+38)
#define ST_NETWORK_INFO  (ST_VMAX+39)

void menuBooleanEdit(button_t button);
void menuNumberEdit(button_t button, unsigned char increment, const prog_char *format);
state_t menuStringEdit(button_t button, const char *line1, unsigned char maxLength);

button_t readButton(void);

state_t menuHome(button_t button);
state_t menuSetpoint(button_t button);
state_t menuProbename(button_t button);
state_t menuProbeOffset(button_t button);
state_t menuProbeSubmenu(button_t button);
state_t menuLidOpenOff(button_t button);
state_t menuLidOpenDur(button_t button);
state_t menuManualMode(button_t button);
state_t menuResetConfig(button_t button);
state_t menuMaxFanSpeed(button_t button);
state_t menuProbeAlarmOn(button_t button);
state_t menuProbeAlarmVal(button_t button);

extern MenuSystem Menus;
extern int editInt;  
extern char editString[];

#endif /* __HMMENUS_H__ */
