#include "WProgram.h"
#include "menus.h"

button_t readButton(void)
{
  unsigned char button = analogRead(0) >> 2;
  if (button == 0)
    return BUTTON_NONE;

  Serial.print("BtnRaw ");
  Serial.println(button, DEC); 

  if (button > 0 && button < 41)
    return BUTTON_LEFT;  
  if (button > 41 && button < 82)
    return BUTTON_RIGHT;  
  if (button > 82 && button < 131)
    return BUTTON_UP;  
  if (button > 131 && button < 172)
    return BUTTON_DOWN;  
    
  return BUTTON_NONE;
}

MenuSystem::MenuSystem(void)
{
  State = ST_NONE;
  m_lastButton = BUTTON_NONE;
}

void MenuSystem::init(const menu_definition_t *defs, const menu_transition_t *trans)
{
  m_definitions = defs;
  m_transitions = trans;
}

unsigned long MenuSystem::getTimeoutDuration(void) const
{
  return (m_currMenu) ? (unsigned long)pgm_read_byte(&m_currMenu->timeout) * 1000 : 0;
}

/*
unsigned char MenuSystem::getTimeoutState(void) const
{
  return (m_currMenu) ? (unsigned long)pgm_read_byte(&m_currMenu->timeoutstate) : 0;
}
*/

handler_t MenuSystem::getHandler(void) const
{
  return (m_currMenu) ? (handler_t)pgm_read_word(&m_currMenu->handler) : 0;
}

unsigned long MenuSystem::getElapsedDuration(void) const
{
  return millis() - m_lastActivity;
}

state_t MenuSystem::findTransition(button_t button) const
{
  unsigned char i;
  for (i=0; pgm_read_byte(&m_transitions[i].state); i++)
  {
    if (pgm_read_byte(&m_transitions[i].state) == State) 
    {
      button_t transButton = pgm_read_byte(&m_transitions[i].button);
      if (button == transButton || 
        (button == BUTTON_TIMEOUT && (transButton & BUTTON_TIMEOUT)))
        return pgm_read_byte(&m_transitions[i].newstate);
    }
  }
  return State;
}

void MenuSystem::setState(state_t state)
{
  while (state != ST_AUTO && state != State)
  {
    handler_t handler = getHandler();
    if (handler)
      handler(BUTTON_LEAVE);

    State = state;
    m_currMenu = m_definitions;

    state_t lookup;
    while (lookup = pgm_read_byte(&m_currMenu->state))
    {
      if (lookup == State)
        break;
      m_currMenu++;
    }
    
    if (m_currMenu)
    {
      handler = getHandler();
      if (handler)
        state = handler(BUTTON_ENTER);
    } else {
      state = ST_NONE;
    }
  }  // while state changing
}

void MenuSystem::doWork(void)
{
  button_t button = readButton();
  if (button != BUTTON_NONE)
  {
    Serial.print("Button ");
    Serial.println(button, DEC);
  } else {
    long dur = getTimeoutDuration();
    if (dur != 0 && getElapsedDuration() >= dur)
      button = BUTTON_TIMEOUT;
  }
  // Debounce: wait for last button to be released
  if (button == m_lastButton)
    return;
  m_lastActivity = millis();
  m_lastButton = button;
  
  state_t newState = ST_AUTO;
  handler_t handler = getHandler();
  if (handler != NULL)
    newState = handler(button);
  if (newState == ST_AUTO)
    newState = findTransition(button);

  setState(newState);
}

