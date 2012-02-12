#include <avr/wdt.h>
#include <avr/sleep.h>
#include <RF12.h>
#include <Ports.h>

#define LMREMOTE_SERIAL 115200

// Node Idenfier for the RFM12B (2-30)
const unsigned char _rfNodeId = 2;
// RFM12B band RF12_433MHZ, RF12_868MHZ, RF12_915MHZ
const unsigned char _rfBand = RF12_915MHZ;
// How long to sleep between probe measurments, in seconds
const unsigned char _sleepInterval = 2;
// Analog pins to read. This is a bitfield, LSB is analog 0
const unsigned char _enabledProbePins = 0x01;  
// Analog pin connected to source power.  Set to 0xff to disable sampling
const unsigned char _pinBattery = 1;
// Digital pins for LEDs, 0xff to disable
const unsigned char _pinLedRx = 0xff;
const unsigned char _pinLedTx = 9;
// Digital pins used for sourcing power to the probe dividers
const unsigned char _pinProbeSupplyBase = 4;
// Number of oversampling bits when measuring temperature [0-3]
#define TEMP_OVERSAMPLE_BITS 1

#define RF_PINS_PER_SOURCE 6 
#define PIN_DISABLED(pin) ((_enabledProbePins & (1 << pin)) == 0)

typedef struct tagRf12ProbeUpdateHdr 
{
  unsigned char seqNo;
  unsigned int batteryLevel;
  unsigned char adcBits;
} rf12_probe_update_hdr_t;

typedef struct tagRf12ProbeUpdate 
{
  unsigned char probeIdx;
  unsigned int adcValue;
} rf12_probe_update_t;

static unsigned int _previousReads[RF_PINS_PER_SOURCE];
static unsigned char _sameCount;
static unsigned char _seqNo;

// The WDT is used solely to wake us from sleep
ISR(WDT_vect) { wdt_disable(); }
//ISR(WDT_vect) { Sleepy::watchdogEvent(); }

void sleep(uint8_t wdt_period) {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  // Disable ADC
  ADCSRA &= ~bit(ADEN);

  // TODO: Figure out what else I can safely disable
  // Disable analog comparator to
  // Internal Voltage Reference? 
  //   When turned on again, the user must allow the reference to start up before the output is used. 
  //   these should be disabled at all times, not just sleep
  
  // Set the watchdog to wake us up and turn on its interrupt
  wdt_enable(wdt_period);
  WDTCSR |= bit(WDIE);

  // Turn off Brown Out Detector
  // sleep must be entered within 3 cycles of BODS being set
  sleep_enable();
  MCUCR = MCUCR | bit(BODSE) | bit(BODS);
  MCUCR = MCUCR & ~bit(BODSE) | bit(BODS);
  
  // Sleep
  sleep_cpu();
  
  // Back from sleep
  sleep_disable();
  ADCSRA |= bit(ADEN);
}

void sleepSeconds(unsigned char secs)
{
  while (secs >= 8) { sleep(WDTO_8S); secs -=8; }
  if (secs >= 4) { sleep(WDTO_4S); secs -= 4; }
  if (secs >= 2) { sleep(WDTO_2S); secs -= 2; }
  if (secs >= 1) { sleep(WDTO_1S); }
}

void rf12_doWork(void)
{
  if (rf12_recvDone() && rf12_crc == 0)
  {
    if (_pinLedRx != 0xff) digitalWrite(_pinLedRx, HIGH);
    // (currently have nothing to receive)
  }
  if (_pinLedRx != 0xff) digitalWrite(_pinLedRx, LOW);
}

inline unsigned int getBatteryLevel(void)
{
  const unsigned char BATREAD_COUNT = 4;
  
  if (_pinBattery != 0xff)
  {
    unsigned int retVal = 0;
    for (unsigned char i=0; i<BATREAD_COUNT; ++i)
      retVal += analogRead(_pinBattery);
    retVal /= BATREAD_COUNT;
    return (unsigned long)retVal * 3300UL / 1023;
  }
  else
    return 3300;  
}

void transmitTemps(unsigned char txCount)
{
  // Enable the transmitter because it takes 1-5ms to turn on (3ms in my testing)
  rf12_sleep(RF12_WAKEUP);

  char outbuf[
    sizeof(rf12_probe_update_hdr_t) + 
    RF_PINS_PER_SOURCE * sizeof(rf12_probe_update_t)
  ];
  rf12_probe_update_hdr_t *hdr;

  hdr = (rf12_probe_update_hdr_t *)outbuf;
  hdr->seqNo = _seqNo++;
  hdr->batteryLevel = getBatteryLevel();
  hdr->adcBits = 10 + TEMP_OVERSAMPLE_BITS;

  // We're done with the ADC shut it down until the next wake cycle
  ADCSRA &= ~bit(ADEN);

  // Send all values regardless of if they've changed or not
  rf12_probe_update_t *up = (rf12_probe_update_t *)&hdr[1];
  for (unsigned char pin=0; pin < RF_PINS_PER_SOURCE; ++pin)
  {
    if (PIN_DISABLED(pin))
      continue;
    up->probeIdx = pin;
    up->adcValue = _previousReads[pin];
    ++up;
  }

  // Hacky way to determine how much to send is see where our buffer pointer 
  // compared to from the start of the buffer
  unsigned char len = (unsigned int)up - (unsigned int)outbuf;

  if (_pinLedTx != 0xff) digitalWrite(_pinLedTx, HIGH);
  
  while (txCount--)
  {
    // Don't check for air to be clear, we just woke from sleep and it will be milliseconds before
    // the RFM chip is actually up and running

    // HDR=0 means to broadcast, no ACK requested
    rf12_sendStart(0, outbuf, len);
    rf12_sendWait(2);
  }  /* while txCount */

  rf12_sleep(RF12_SLEEP);
  if (_pinLedTx != 0xff) digitalWrite(_pinLedTx, LOW);
}

inline void newTempsAvailable(void)
{
  transmitTemps(1);
}

void enableAdcPullups(void)
{
  for (unsigned char pin=0; pin < RF_PINS_PER_SOURCE; ++pin)
  {
    if (PIN_DISABLED(pin))
      continue;

    // The probe themistor voltage dividers are normally powered down
    // to save power, using digital lines to supply Vcc to them.
    // Lines are used sequentially starting from _pinProbeSupplyBase
    // Note you can use the analog internal pullups as the fixed half of
    // the divider by setting this value to A0 (or higher).
    // The analog pullups are 20k-40kOhms, about 36k on my handful of chips.
    // If using digital lines you will supply your own resistor for the fixed half.
    digitalWrite(_pinProbeSupplyBase + pin, HIGH);
  }
  // takes about 15uS to stabilize and the enabling process takes 6us per pin at 16MHz
  delayMicroseconds(10);
}

void checkTemps(void)
{
  const unsigned char OVERSAMPLE_COUNT[] = {1, 4, 16, 64};  // 4^n
  boolean modified = false;
  unsigned int oversampled_adc = 0;

  enableAdcPullups();
  for (unsigned char pin=0; pin < RF_PINS_PER_SOURCE; ++pin)
  {
    if (PIN_DISABLED(pin))
      continue;

    for (unsigned char o=0; o<OVERSAMPLE_COUNT[TEMP_OVERSAMPLE_BITS]; ++o)
    {
      unsigned int adc = analogRead(pin);
      if (adc == 0 || adc >= 1023)
      {
        oversampled_adc = 0;
        break;
      }
      oversampled_adc += adc;
    }
    oversampled_adc = oversampled_adc >> TEMP_OVERSAMPLE_BITS;

#ifdef LMREMOTE_SERIAL
    Serial.println(oversampled_adc, DEC);
#endif
    if (oversampled_adc != _previousReads[pin])
      modified = true;
    _previousReads[pin] = oversampled_adc;

    digitalWrite(_pinProbeSupplyBase + pin, LOW);
  }

  if (modified || (_sameCount > (30 / _sleepInterval)))
  {
    _sameCount = 0;
    newTempsAvailable();
  }
  else
    ++_sameCount;
}

inline void setupSupplyPins(void)
{
  // Analog pullup supply pins don't need DIR set
  if (_pinProbeSupplyBase >= A0)
    return;
  for (unsigned char pin=0; pin < RF_PINS_PER_SOURCE; ++pin)
  {
    if (PIN_DISABLED(pin))
      continue;
    pinMode(_pinProbeSupplyBase + pin, OUTPUT);
  }
}

void setup(void)
{
  // Turn off the units we never use (this only affects non-sleep power)
  PRR = bit(PRUSART0) | bit(PRTWI) | bit(PRTIM1) | bit(PRTIM2);
  // Disable digital input buffers on the analog in ports
  DIDR0 = bit(ADC5D) | bit(ADC4D) | bit(ADC3D) | bit(ADC2D) | bit(ADC1D) | bit(ADC0D);
  DIDR1 = bit(AIN1D) | bit(AIN0D);

#ifdef LMREMOTE_SERIAL
  PRR &= ~bit(PRUSART0);
  Serial.begin(115200);  Serial.println("$UCID,lmremote");
#endif

  rf12_initialize(_rfNodeId, _rfBand);

  if (_pinLedRx != 0xff) pinMode(_pinLedRx, OUTPUT);
  if (_pinLedTx != 0xff) pinMode(_pinLedTx, OUTPUT);
  
  setupSupplyPins();

  // Force a reading and transmit multiple times so the master can sync its seqno
  memset(_previousReads, 0xff, sizeof(_previousReads));
  checkTemps();
  transmitTemps(2);
}

void stabilizeAdc(void)
{
  if (_pinBattery != 0xff)
  {
    unsigned int last;
    unsigned int curr = analogRead(_pinBattery);
    unsigned int cnt = 0;
    // Reads the adc a bunch of times until the value settles
    // Usually you hear "discard the first few ADC readings after sleep"
    // but this seems a bit more scientific
    do {
      ++cnt;
      last = curr;
#ifdef LMREMOTE_SERIAL
      Serial.print(curr,DEC);
      Serial.print(' ');
#endif
      curr = analogRead(_pinBattery);
    } while ((cnt < 10) && (abs(last - curr) > 2));
  }
#ifdef LMREMOTE_SERIAL
  Serial.println();
#endif
}

void loop(void)
{
  //Sleepy::loseSomeTime(_sleepInterval * 1000);
  sleepSeconds(_sleepInterval);
  //stabilizeAdc();
  checkTemps();
#ifdef LMREMOTE_SERIAL
  delay(1);
#endif
}


