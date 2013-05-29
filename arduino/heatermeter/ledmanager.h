// HeaterMeter Copyright 2013 Bryan Mayland <bmayland@capnbry.net> 
#ifndef __LEDMANAGER_H__
#define __LEDMANAGER_H__

#include <inttypes.h>

#define LEDSTIMULUS_INVERT 0x80
#define LEDSTIMULUS_MASK   0x7f
#define LEDSTIMULUS_Off       0
#define LEDSTIMULUS_Alarm0L   1
#define LEDSTIMULUS_Alarm0H   2
#define LEDSTIMULUS_Alarm1L   3
#define LEDSTIMULUS_Alarm1H   4
#define LEDSTIMULUS_Alarm2L   5
#define LEDSTIMULUS_Alarm2H   6
#define LEDSTIMULUS_Alarm3L   7
#define LEDSTIMULUS_Alarm3H   8
#define LEDSTIMULUS_RfReceive 9
#define LEDSTIMULUS_LidOpen   10
#define LEDSTIMULUS_FanOn     11
#define LEDSTIMULUS_PitTempReached 12
#define LEDSTIMULUS_FanMax    13

#define LEDACTION_Off         0 // Must be == false
#define LEDACTION_OnSteady    1 // Must be == true
#define LEDACTION_OneShot     2

#define LED_COUNT 4

typedef void (*led_executor_t)(uint8_t led, uint8_t on);

typedef struct tagLedStatus
{
  // config
  uint8_t stimulus;    // LEDSTIMULUS_* and possible LEDSTIMULUS_INVERT
  // state
  uint8_t triggered;   // LEDACTION_*
  uint8_t on;          // LEDACTION_*
} led_status_t;

class LedManager
{
public:
  LedManager(const led_executor_t executor) :
    _executor(executor) { }

  void publish(uint8_t stimulus, uint8_t action);
  void doWork(void);
  void setAssignment(uint8_t led, uint8_t ledconf);
  uint8_t getAssignment(uint8_t led) const { return _leds[led].stimulus; }

private:
  led_status_t _leds[LED_COUNT];
  uint32_t _blinkMillis;
  uint8_t _blinkCount;
  led_executor_t _executor;
};

#endif /* __LEDMANAGER_H__ */
