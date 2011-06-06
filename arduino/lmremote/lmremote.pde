#include <avr/wdt.h>
#include <avr/sleep.h>
#include <RF12.h>

// Node Idenfier for the RFM12B (2-30)
const unsigned char _rfNodeId = 2; 
// RFM12B band RF12_433MHZ, RF12_868MHZ, RF12_915MHZ
const unsigned char _rfBand = RF12_915MHZ;
// How long to sleep between probe measurments, in seconds
const unsigned char _sleepInterval = 4; 
// Analog pins to read, this is a bitfild. LSB is analog 0
const unsigned char _enabledProbePins = 0xff;  
// Analog pin connected to source power.  Set to 0xff to disable sampling
const unsigned char _pinBattery = 1;
// Digital pins for LEDs
const unsigned char _pinLedRx = 4;
const unsigned char _pinLedTx = 5;

#define PRR_ALWAYS (_BV(PRUSART0))

#define RF_PINS_PER_SOURCE 4
struct __rfm12_probe_update_hdr {
  unsigned char sourceId;
  unsigned char seqNo;
  unsigned int batteryLevel;
};
struct __rfm12_probe_update {
  unsigned char probeIdx;
  unsigned int adcValue;
};

static unsigned int _previousReads[RF_PINS_PER_SOURCE];
static unsigned char _sameCount;
static unsigned char _seqNo;

ISR(WDT_vect) {
  // The WDT is used solely to wake us from sleep
  wdt_disable();
}

void sleep(uint8_t wdt_period) {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  // Set the watchdog to wake us up and turn on its interrupt
  wdt_enable(wdt_period);
  WDTCSR |= _BV(WDIE);

  // Disable ADC, still needs to be shutdown via PRR
  ADCSRA &= ~_BV(ADEN);
//  PRR = PRR_ALWAYS | _BV(PRADC);

  // disabling SPI means it needs to be re initialized when we come back up
  //PRR |= _BV(PRSPI);
  // TODO: Figure out what else from PRR I can safely disable
  // Disable analog comparator to
  // Internal Voltage Reference? 
  //   When turned on again, the user must allow the reference to start up before the output is used. 
  // DIDR1 DIDR0 (diable digital input registers)  
  //   these should be disabled at all times, not just sleep
  
  // Turn off Brown Out Detector
  // sleep must be entered within 3 cycles of BODS being set
  MCUSR |= _BV(BODS) | _BV(BODSE);
  MCUSR &= ~_BV(BODSE);  
  
  // Sleep
  sleep_mode();
  
  // Back from sleep
  wdt_disable();
  WDTCSR &= ~_BV(WDIE);
  ADCSRA |= _BV(ADEN);
//  PRR = PRR_ALWAYS;
}

void sleepSeconds(unsigned char secs)
{
  while (secs >= 8) { sleep(WDTO_8S);  secs -=8; }
  if (secs >= 4) { sleep(WDTO_4S);  secs -= 4; }
  if (secs >= 2) { sleep(WDTO_2S);  secs -= 2; }
  if (secs >= 1) { sleep(WDTO_1S); }
}

void rf12_doWork(void)
{
  if (rf12_recvDone() && rf12_crc == 0)
  {
    digitalWrite(_pinLedRx, HIGH);
    sleep(WDTO_120MS);  // temp placeholder code
  }
  else
    digitalWrite(_pinLedRx, LOW);
}
void newReadAvailable(void)
{
  char outbuf[
    sizeof(struct __rfm12_probe_update_hdr) + 
    RF_PINS_PER_SOURCE * sizeof(struct __rfm12_probe_update)
  ];
  struct __rfm12_probe_update_hdr *hdr;

  hdr = (struct __rfm12_probe_update_hdr *)outbuf;
  hdr->sourceId = _rfNodeId;
  hdr->seqNo = _seqNo++;
  if (_pinBattery != 0xff)
    hdr->batteryLevel = (unsigned long)analogRead(_pinBattery) * 3300L / 1023;
  else
    hdr->batteryLevel = 3300;

  // Send all values regardless of if they've changed or not
  struct __rfm12_probe_update *up = (struct __rfm12_probe_update *)&hdr[1];
  for (unsigned char pin=0; pin < RF_PINS_PER_SOURCE; ++pin)
  {
    // If the pin is not enabled, skip it
    if ((_enabledProbePins & (1 << pin)) == 0)
      continue;
    up->probeIdx = pin;
    up->adcValue = _previousReads[pin];
    ++up;
  }

  digitalWrite(_pinLedTx, HIGH);
  rf12_sleep(RF12_WAKEUP);
  while (!rf12_canSend())
    rf12_doWork();
  
  // Hacky way to determine how much to send is see where our buffer pointer 
  // compared to from the start of the buffer
  unsigned char len = (unsigned int)up - (unsigned int)outbuf;
  rf12_sendStart(1, outbuf, len);
  rf12_sendWait(1);
  rf12_sleep(RF12_SLEEP);
  digitalWrite(_pinLedTx, LOW);
}

void checkTemps(void)
{
  boolean modified = false;
  for (unsigned char pin=0; pin < RF_PINS_PER_SOURCE; ++pin)
  {
    // If the pin is not enabled, skip it
    if ((_enabledProbePins & (1 << pin)) == 0)
      continue;
      
    unsigned int newRead = analogRead(pin);
    if (newRead != _previousReads[pin])
      modified = true;
    _previousReads[pin] = newRead;
  }

  if (modified || (_sameCount > (60 / _sleepInterval)))
  {
    _sameCount = 0;
    newReadAvailable();
  }
  else
    ++_sameCount;
}

void setup(void)
{
  //Serial.begin(115200);
  memset(_previousReads, 0xff, sizeof(_previousReads));
  
  //PRR = PRR_ALWAYS; 
  rf12_initialize(_rfNodeId, _rfBand);

  pinMode(_pinLedRx, OUTPUT);
  pinMode(_pinLedTx, OUTPUT);
}

void loop(void)
{
  rf12_doWork();
  checkTemps();
  sleepSeconds(_sleepInterval);
}


