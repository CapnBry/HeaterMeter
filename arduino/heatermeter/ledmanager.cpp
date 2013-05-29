#include "ledmanager.h"
#include <Arduino.h>

#define LED_BLINK_MILLIS 750

void LedManager::publish(uint8_t stimulus, uint8_t action)
{
  for (uint8_t i=0; i<LED_COUNT; ++i)
  {
    led_status_t &a = _leds[i];
    if ((a.stimulus & LEDSTIMULUS_MASK) == stimulus)
    {
      a.triggered = action;
      if (action != LEDACTION_OneShot)
      {
        uint8_t invert = a.stimulus >> 7;
        uint8_t invertedState = invert ^ action;

        if (invertedState != a.on)
        {
          a.on = invertedState;
          _executor(i, invertedState);
        }
      }
    } /* stimulus match */
  } /* LED_COUNT */
}

void LedManager::doWork(void)
{
  if ((_blinkMillis > 0) && (millis() - _blinkMillis < LED_BLINK_MILLIS))
    return;
  _blinkMillis = millis();

  ++_blinkCount;
  for (uint8_t i=0; i<LED_COUNT; ++i)
  {
    led_status_t &a = _leds[i];
    boolean stateChanged = false;

    if (_blinkCount & 1)
    {
      if (a.triggered == LEDACTION_OneShot)
      {
        a.on = LEDACTION_OneShot;
        a.triggered = LEDACTION_Off;
        stateChanged = true;
      }
    }
    else
    {
      if (a.on == LEDACTION_OneShot)
      {
        a.on = LEDACTION_Off;
        stateChanged = true;
      }
    } /* !_blinkState */

    if (stateChanged)
      _executor(i, a.on);
  } /* LED_COUNT */
}

void LedManager::setAssignment(uint8_t led, uint8_t stimulus)
{
  _leds[led].stimulus = stimulus;
  if (_leds[led].on = LEDACTION_OnSteady)
    _executor(led, LEDACTION_Off);
}
