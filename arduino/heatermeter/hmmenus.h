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
#define BUTTON_ANY   (0x1f)

enum HmMenuStates {
  ST_HOME_FOOD1 = (ST_VMAX+1), // ST_HOME_X must stay sequential and in order
  ST_HOME_FOOD2,
  ST_HOME_AMB,
  ST_HOME_NOPROBES,
  ST_HOME_ALARM,
  ST_CONNECTING,
  ST_SETPOINT,
  ST_PROBENAME1,  // ST_PROBENAMEX must stay sequential and in order
  ST_PROBENAME2,
  ST_PROBENAME3,
  ST_PROBEOFF0, // ST_PROBEOFFX must stay sequential and in order
  ST_PROBEOFF1,
  ST_PROBEOFF2,
  ST_PROBEOFF3,
  ST_LIDOPEN_OFF,
  ST_LIDOPEN_DUR,
  ST_MANUALMODE,
  ST_RESETCONFIG,
  ST_LCDBACKLIGHT,
  ST_MAXFANSPEED,
  ST_PROBESUB0,  // ST_PROBESUBX must stay sequential and in order
  ST_PROBESUB1,
  ST_PROBESUB2,
  ST_PROBESUB3,
  ST_PALARM0_H_ON,
  ST_PALARM1_H_ON,
  ST_PALARM2_H_ON,
  ST_PALARM3_H_ON,
  ST_PALARM0_H_VAL,
  ST_PALARM1_H_VAL,
  ST_PALARM2_H_VAL,
  ST_PALARM3_H_VAL,
  ST_PALARM0_L_ON,
  ST_PALARM1_L_ON,
  ST_PALARM2_L_ON,
  ST_PALARM3_L_ON,
  ST_PALARM0_L_VAL,
  ST_PALARM1_L_VAL,
  ST_PALARM2_L_VAL,
  ST_PALARM3_L_VAL,
  ST_NETWORK_INFO,
  ST_TOAST,
};

class HmMenuSystem : public MenuSystem
{
public:
  HmMenuSystem(const menu_definition_t *defs, const menu_transition_t *trans,
    const buttonread_t reader) : MenuSystem(defs, trans, reader)
    {};

  void displayToast(char *msg);
  state_t getSavedState(void) const { return _savedState; }
  unsigned char *getToastLine0(void) { return &_toastMsg[0]; }
  unsigned char *getToastLine1(void) { return &_toastMsg[sizeof(_toastMsg)/2+1]; }
private:
  unsigned char _toastMsg[33];
  state_t _savedState;
};

extern HmMenuSystem Menus;
extern char editString[];

#endif /* __HMMENUS_H__ */
