// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
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
  state_t State;

  // Call in loop() to handle buttons
  void doWork(void);
  
  unsigned long getTimeoutDuration(void) const;
  unsigned long getElapsedDuration(void) const;
  handler_t getHandler(void) const;
  void setState(state_t state);
private:
  const menu_definition_t *m_definitions;
  const menu_transition_t *m_transitions;
  const menu_definition_t *m_currMenu;
  const buttonread_t m_readButton;
  button_t m_lastButton;
  unsigned long m_lastActivity;

  state_t findTransition(button_t button) const;
};

#endif /* __MENUS_H__ */
