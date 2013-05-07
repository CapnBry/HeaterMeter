#include "ledmanager.h"
#include <Arduino.h>

#define LED_BLINK_MILLIS 1000

void LedManager::publish(LedStimulus t, bool state)
{
  for (uint8_t i=0; i<LED_COUNT; ++i)
  {
    led_status_t &a = Assignment[i];
    if (a.stimulus == t)
    {
      a.triggered = state;
      if (a.action == Steady && state != a.on)
      {
        a.on = state;
        _executor(i, state);
      }
    } /* stimulus match */
  } /* LED_COUNT */
}

void LedManager::doWork(void)
{
  if (millis() - _blinkMillis < LED_BLINK_MILLIS)
    return;
  _blinkMillis = millis();

  _blinkState = !_blinkState;
  for (uint8_t i=0; i<LED_COUNT; ++i)
  {
    led_status_t &a = Assignment[i];
    if (a.action == Steady)
      continue;

    if (_blinkState)
    {
      if (a.triggered)
      {
        a.on = true;
        _executor(i, true);
      }
      if (a.action == OneShot)
        a.triggered = false;
    }
    else
    {
      if (a.on)
      {
        a.on = false;
        _executor(i, false);
      }
    } /* !_blinkState */
  } /* LED_COUNT */
}