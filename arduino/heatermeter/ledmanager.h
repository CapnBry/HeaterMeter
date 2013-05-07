// HeaterMeter Copyright 2013 Bryan Mayland <bmayland@capnbry.net> 
#ifndef __LEDMANAGER_H__
#define __LEDMANAGER_H__

#include <inttypes.h>

enum LedStimulus
{
  None,
  UserOn,
  Alarm0L,
  Alarm0H,
  Alarm1L,
  Alarm1H,
  Alarm2L,
  Alarm2H,
  Alarm3L,
  Alarm3H,
  RfReceive,
  LidOpen,
  FanOn,
  PitTempReached,
};

enum LedAction { Steady, Blink, OneShot };

#define LED_COUNT 3

typedef void (*led_executor_t)(uint8_t led, bool on);

typedef struct tagLedStatus
{
  LedStimulus stimulus;
  LedAction action;
  bool on;
  bool triggered;
} led_status_t;

class LedManager
{
public:
  LedManager(const led_executor_t executor) :
    _executor(executor) { }

  led_status_t Assignment[LED_COUNT];
  void publish(LedStimulus t, bool state);
  void doWork(void);

private:
  uint32_t _blinkMillis;
  bool _blinkState;
  led_executor_t _executor;
};

#endif /* __LEDMANAGER_H__ */
