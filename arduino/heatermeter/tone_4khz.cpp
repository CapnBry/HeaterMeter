// HeaterMeter Copyright 2014 Bryan Mayland <bmayland@capnbry.net>
/*
  Use TIMER1's 50Hz frequency to generate a ~4kHz tone on any pin
  OCR1A is incremented 160x per overflow to generate the 8khz interrupt

  This expect TIMER1 to a 50Hz CTC (TOP=40000) before use!
*/
#include <Arduino.h>
#include <stdint.h>
#include "tone_4khz.h"

static struct tagTimer4KHzState {
  volatile uint8_t *pinPort;
  uint8_t pinMask;
  uint16_t cnt;
} timer4k;

ISR(TIMER1_COMPA_vect)
{
  if (timer4k.cnt != 0)
  {
    uint16_t trigger = OCR1A;
    trigger += (40000/160);
    if (trigger >= 40000)
      trigger -= 40000;
    OCR1A = trigger;
    --timer4k.cnt;
    *timer4k.pinPort ^= timer4k.pinMask;
  }
  else
    tone4khz_end();
}

void tone4khz_end(void)
{
  if (timer4k.pinPort)
  {
    TIMSK1 &= ~bit(OCIE1A);
    *timer4k.pinPort &= ~(timer4k.pinMask);
  }
}

void tone4khz_begin(unsigned char pin, unsigned char dur)
{
  // Stop the tone if it is running
  tone4khz_end();

  timer4k.pinMask = digitalPinToBitMask(pin);
  uint8_t port = digitalPinToPort(pin);
  timer4k.pinPort = portOutputRegister(port);
  // set pinMode(pin, OUTPUT)
  volatile uint8_t *ddr = portModeRegister(port);
  *ddr |= timer4k.pinMask;

  timer4k.cnt = (uint16_t)dur * 8U;
  TIMSK1 |= bit(OCIE1A);
}