// HeaterMeter Copyright 2013 Bryan Mayland <bmayland@capnbry.net> 
#ifndef __LEDMANAGER_H__
#define __LEDMANAGER_H__

#include <inttypes.h>

struct LedStimulus
{
  typedef enum {
    Off,
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
    FanMax,
  } Type;
};

struct LedAction
{
  typedef enum {
    Off = 0,       // Must be == false
    OnSteady = 1,  // Must be == true
    OneShot
  } Type;
};

#define LED_COUNT 4

#define LEDSTIMULUS_INVERT 0x80
#define LEDSTIMULUS_MASK   0x7f

typedef void (*led_executor_t)(uint8_t led, uint8_t on);

typedef struct tagLedStatus
{
  // config
  LedStimulus::Type stimulus;
  uint8_t invert;
  // state
  LedAction::Type triggered;
  LedAction::Type on;
} led_status_t;

class LedManager
{
public:
  LedManager(const led_executor_t executor) :
    _executor(executor) { }

  led_status_t Assignment[LED_COUNT];
  void publish(LedStimulus::Type t, LedAction::Type state);
  void doWork(void);
  void setLedConf(uint8_t led, uint8_t ledconf);

private:
  uint32_t _blinkMillis;
  uint8_t _blinkCount;
  led_executor_t _executor;
};

#endif /* __LEDMANAGER_H__ */
