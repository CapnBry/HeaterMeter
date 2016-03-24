// HeaterMeter Copyright 2016 Bryan Mayland <bmayland@capnbry.net>
/*
  Use TIMER1's 50Hz frequency to generate a ~4kHz tone on any pin
  OCR1A is incremented 160x per overflow to generate the 8khz interrupt

  This expect TIMER1 to a 50Hz CTC (TOP=40000) before use!
*/
#include <Arduino.h>
#include <stdint.h>
#include <digitalWriteFast.h>
#include "tone_4khz.h"

static struct tagTimer4KHzState {
  uint16_t cnt;
} timer4k;

ISR(TIMER1_COMPA_vect)
{
  if (timer4k.cnt != 0)
  {
    uint16_t trigger = OCR1A;
    trigger += (40000/160);
    if (trigger >= 40000)
      trigger = 0;
    OCR1A = trigger;
    --timer4k.cnt;
    uint8_t v = *digitalPinToPortReg(PIN_ALARM);
    *digitalPinToPortReg(PIN_ALARM) = v ^ (1 << __digitalPinToBit(PIN_ALARM));
  }
  else
    tone4khz_end();
}

void tone4khz_init(void)
{
  pinModeFast(PIN_ALARM, OUTPUT);
}

void tone4khz_end(void)
{
  TIMSK1 &= ~bit(OCIE1A);
  digitalWriteFast(PIN_ALARM, LOW);
}

void tone4khz_begin(unsigned char pin, unsigned char dur)
{
  // Stop the tone if it is running
  tone4khz_end();
  timer4k.cnt = (uint16_t)dur * 8U;
  TIMSK1 |= bit(OCIE1A);
}