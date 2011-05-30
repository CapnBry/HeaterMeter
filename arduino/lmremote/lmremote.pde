#include <avr/wdt.h>
#include <avr/sleep.h>
#include <RF12.h>

unsigned int lastRead = 0xffff;
unsigned char seqNo;

void setup(void)
{
  //Serial.begin(9600);Serial.println("UP");
  rf12_initialize(2, RF12_915MHZ);
}

struct __rfm12_probe_update_hdr {
  boolean lowBattery;
  unsigned char sourceId;
  unsigned char seqNo;
};
struct __rfm12_probe_update {
  unsigned char probeIdx;
  unsigned int adcValue;
};

void newReadAvailable(void)
{
  struct __rfm12_probe_update_hdr hdr;
  struct __rfm12_probe_update up;
  char buffer[sizeof(struct __rfm12_probe_update_hdr) + sizeof(__rfm12_probe_update)];
  
  hdr.lowBattery = rf12_lowbat();
  hdr.sourceId = 2;
  hdr.seqNo = seqNo++;
  
  up.probeIdx = 0;
  up.adcValue = lastRead;
  
  memcpy(buffer, &hdr, sizeof(__rfm12_probe_update_hdr));
  memcpy(&buffer[sizeof(__rfm12_probe_update_hdr)], &up, sizeof(__rfm12_probe_update));
  
  rf12_sleep(RF12_WAKEUP);
  while (!rf12_canSend())
    rf12_recvDone();
  
  rf12_sendStart(1, buffer, sizeof(buffer));
  rf12_sendWait(1);
  rf12_sleep(RF12_SLEEP);
}

ISR(WDT_vect) {
  wdt_disable();
  //wdt_reset();
#ifdef WDTCSR
  WDTCSR &= ~_BV(WDIE);
#else
  WDTCR &= ~_BV(WDIE);
#endif
}

void sleep(uint8_t wdt_period) {
  wdt_enable(wdt_period);
#ifdef WDTCSR
  WDTCSR |= _BV(WDIE);
#else
  WDTCR |= _BV(WDIE);
#endif
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_mode();
  wdt_disable();
#ifdef WDTCSR
  WDTCSR &= ~_BV(WDIE);
#else
  WDTCR &= ~_BV(WDIE);
#endif
}

unsigned char sameCount;

void loop(void)
{
  unsigned int newRead = analogRead(0);
  if ((newRead != lastRead) || (sameCount > 7))
  {
    lastRead = newRead;
    sameCount = 0;
    //Serial.println(lastRead, DEC);delay(100);
    newReadAvailable();
  }
  else
    ++sameCount;
  sleep(WDTO_8S);
  //Serial.print("Woke up");
}


