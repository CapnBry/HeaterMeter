// HeaterMeter Copyright 2016 Bryan Mayland <bmayland@capnbry.net>
#ifndef __MENUS_H__
#define __MENUS_H__

// The first BUTTON_s are pseudo-buttons to indicate state changes
// No button pressed
#define BUTTON_NONE  0
// Sent to a menu item before the state is entered
#define BUTTON_ENTER (1<<7)
// Sent to a menu item before the state is left
#define BUTTON_LEAVE (1<<6)
// The timeout specified in the menu definition has expired
#define BUTTON_TIMEOUT (1<<5)
// Button is a long press
#define BUTTON_LONG (1<<4)


#define ST_NONE     0
#define ST_AUTO     1
#define ST_VMAX     1

typedef unsigned char state_t;
typedef unsigned char button_t;

typedef state_t (*handler_t)(button_t button);
typedef button_t (*buttonread_t)(void);

typedef struct tagMenuDefinition
{
  state_t state;
  handler_t handler;
  unsigned char timeout;
  button_t longpress; // bitwise OR of buttons with longpresshandlers in the menu
} menu_definition_t;

typedef struct tagMenuTransition
{
  state_t state;
  button_t button;
  state_t newstate;
} menu_transition_t;

class MenuSystem
{
public:
  MenuSystem(const menu_definition_t *defs, const menu_transition_t *trans,
    const buttonread_t reader);

  // Call in loop() to handle buttons
  void doWork(void);
  
  void setState(state_t state);
  state_t getState(void) const { return m_state; }
  state_t getLastState(void) const { return m_lastState; }
  button_t getButton(void) const { return m_lastButton; }
  unsigned char getButtonRepeatCnt(void) const { return m_buttonRepeatCnt; }
private:
  enum ButtonLongpressState {
    mbsNone,
    mbsLongCheck,
    mbsWaitForKeyup
  };

  const menu_definition_t *m_definitions;
  const menu_transition_t *m_transitions;
  const menu_definition_t *m_currMenu;
  const buttonread_t m_readButton;
  state_t m_state;
  state_t m_lastState;
  button_t m_lastButton;
  ButtonLongpressState m_buttonState;
  unsigned char m_buttonRepeatCnt;
  unsigned long m_lastStateChange;
  unsigned long m_lastButtonActivity;

  unsigned long getTimeoutDuration(void) const;
  unsigned long getElapsedDuration(void) const;
  boolean getHasLongpress(button_t button) const;
  handler_t getHandler(void) const;
  state_t findTransition(button_t button) const;
};

#endif /* __MENUS_H__ */
