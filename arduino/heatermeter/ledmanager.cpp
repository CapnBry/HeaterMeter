#include "ledmanager.h"
#include <Arduino.h>

#define LED_BLINK_MILLIS 750

void LedManager::publish(LedStimulus::Type t, LedAction::Type state)
{
  for (uint8_t i=0; i<LED_COUNT; ++i)
  {
    led_status_t &a = Assignment[i];
    if (a.stimulus == t)
    {
      a.triggered = state;
      if (state != LedAction::OneShot)
      {
        LedAction::Type invertedState = (LedAction::Type)(a.invert ^ (uint8_t)state);

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
    led_status_t &a = Assignment[i];
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

void LedManager::setLedConf(uint8_t led, uint8_t ledconf)
{
  Assignment[led].stimulus = (LedStimulus::Type)(ledconf & 0x7f);
  Assignment[led].invert = ledconf >> 7;
  if (Assignment[led].on = LedAction::OnSteady)
    _executor(led, LedAction::Off);
}
