#include "ledmanager.h"
#include <Arduino.h>

#define LED_BLINK_MILLIS 750

void LedManager::publish(LedStimulus::Type t, LedAction::Type state)
{
  for (uint8_t i=0; i<LED_COUNT; ++i)
  {
    led_status_t &a = _leds[i];
    if ((a.stimulus & LEDSTIMULUS_MASK) == t)
    {
      a.triggered = state;
      if (state != LedAction::OneShot)
      {
        uint8_t invert = a.stimulus >> 7;
        LedAction::Type invertedState = (LedAction::Type)(invert ^ (uint8_t)state);

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
      if (a.triggered == LedAction::OneShot)
      {
        a.on = LedAction::OneShot;
        a.triggered = LedAction::Off;
        stateChanged = true;
      }
    }
    else
    {
      if (a.on == LedAction::OneShot)
      {
        a.on = LedAction::Off;
        stateChanged = true;
      }
    } /* !_blinkState */

    if (stateChanged)
      _executor(i, a.on);
  } /* LED_COUNT */
}

void LedManager::setAssignment(uint8_t led, LedStimulus::Type stimulus)
{
  _leds[led].stimulus = stimulus;
  if (_leds[led].on = LedAction::OnSteady)
    _executor(led, LedAction::Off);
}
