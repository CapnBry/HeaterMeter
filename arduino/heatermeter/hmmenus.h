// HeaterMeter Copyright 2019 Bryan Mayland <bmayland@capnbry.net>
#ifndef __HMMENUS_H__
#define __HMMENUS_H__

#include <avr/pgmspace.h>
#include "menus.h"

#define BUTTON_LEFT  (1<<0)
#define BUTTON_RIGHT (1<<1)
#define BUTTON_UP    (1<<2)
#define BUTTON_DOWN  (1<<3)
#define BUTTON_ANY   (0x0f)

enum HmMenuStates {
  ST_HOME_FOOD1 = (ST_VMAX+1), // ST_HOME_X must stay sequential and in order
  ST_HOME_FOOD2,
  ST_HOME_FOOD3,
  ST_HOME_NOPROBES,
  ST_HOME_ALARM,
  ST_ALARM_ACTION,
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
  ST_TOAST,
  ST_ENG_PROBEDIAG,
  ST_NETINFO,
};

enum class HmMenuInteractiveTopic : unsigned char {
  NETINFO = 0
};

enum class HmMenuSystemHostState : unsigned char {
  OFFLINE, // No host detected
  ONLINE,  // Host detected but not active
  ACTIVE   // Host controlling display
};

class HmMenuSystem : public MenuSystem
{
public:
  HmMenuSystem(const menu_definition_t *defs, const menu_transition_t *trans,
    const buttonread_t reader) : MenuSystem(defs, trans, reader)
    {};

  void init(void);
  void doWork(void) override;

  void displayToast(char *msg);
  void displayHostMsg(void);
  unsigned char *getHostMsgLine0(void) { return &_hostMsgBuf[0]; }
  unsigned char *getHostMsgLine1(void) { return &_hostMsgBuf[sizeof(_hostMsgBuf)/2]; }
  unsigned char ProbeNum;

  unsigned char getHomeDisplayMode(void) const { return _homeDisplayMode; }
  void setHomeDisplayMode(unsigned char v);

  // LCD Interactive Menu Functions - Host-Driven
  unsigned int getHostOpaque(void) const { return _hostOpaque; }
  void setHostOpaque(unsigned int v) { _hostOpaque = v;  }
  void sendHostInteract(HmMenuInteractiveTopic topic, button_t button);
  void hostMsgReceived(char* msg);
  void hostSplitLines(char* msg);
  HmMenuSystemHostState getHostState(void) const { return _hostState; }
  void setHostStateOnline(void) { if (_hostState == HmMenuSystemHostState::OFFLINE) _hostState = HmMenuSystemHostState::ONLINE; }
  //void setHostState(HmMenuSystemHostState state) { _hostState = state; }
private:
  unsigned char _homeDisplayMode;
  unsigned char _hostMsgBuf[33];
  unsigned int _hostOpaque;
  HmMenuSystemHostState _hostState;
  unsigned long _hostInteractiveSentTime;
};

extern HmMenuSystem Menus;
extern char editString[];

#endif /* __HMMENUS_H__ */
