#include <avr/wdt.h>
#include <avr/sleep.h>
#include <rf12_itplus.h>

// Enabling LMREMOTE_SERIAL also disables several power saving features
//#define LMREMOTE_SERIAL 38400

// Base Idenfier for the RFM12B (0-63)
// The transmitted ID is this ID plus the pin number
const char _rfNodeBaseId = 0;
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
const unsigned char _pinLedTx = 0xff; //9
// Digital pins used for sourcing power to the probe dividers
const unsigned char _pinProbeSupplyBase = 4;
// Percentage (integer) of VCC where the battery is considered low (33% = 1.1V)
#define BATTERY_LOW_PCT 33
// Number of seconds to keep the "recent/new" bit set
#define RECENT_EXPIRE_SECS 1800
// Number of oversampling bits when measuring temperature [0-2]
#define TEMP_OVERSAMPLE_BITS 1

#define RF_PINS_PER_SOURCE 6 
#define PIN_DISABLED(pin) ((_enabledProbePins & (1 << pin)) == 0)

// Bits used in output packet
// byte1
#define BYTE1_LMREMOTE_KEY 0x10
#define BYTE1_RECENT_BOOT  0x20
// hygro byte
#define HYGRO_BATTERY_LOW  0x80
#define HYGRO_BATTERY_OK   0x00
#define HYGRO_NO_HYGRO     0x6A

#ifdef LMREMOTE_SERIAL
  #define SLEEPMODE_TX    1
#else
  #define SLEEPMODE_TX    2
#endif

static unsigned int _previousReads[RF_PINS_PER_SOURCE];
static unsigned int _loopCnt;
static unsigned char _sameCount;
static unsigned char _isRecent = BYTE1_RECENT_BOOT;
static unsigned char _isBattLow;
static unsigned char _hygroVal;

// The WDT is used solely to wake us from sleep
ISR(WDT_vect) { wdt_disable(); }

static void sleep(uint8_t wdt_period, boolean include_adc) {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  // Disable ADC
  if (include_adc)
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
  MCUCR = (MCUCR & ~bit(BODSE)) | bit(BODS);
  
  // Sleep
  sleep_cpu();
  
  // Back from sleep
  sleep_disable();
  if (include_adc)
    ADCSRA |= bit(ADEN);
}

static void sleepSeconds(unsigned char secs)
{
  while (secs >= 8) { sleep(WDTO_8S, true); secs -=8; }
  if (secs >= 4) { sleep(WDTO_4S, true); secs -= 4; }
  if (secs >= 2) { sleep(WDTO_2S, true); secs -= 2; }
  if (secs >= 1) { sleep(WDTO_1S, true); }
}

static void rf12_doWork(void)
{
  if (rf12_recvDone() && rf12_crc == 0)
  {
    if (_pinLedRx != 0xff) digitalWrite(_pinLedRx, HIGH);
    // (currently have nothing to receive)
  }
  if (_pinLedRx != 0xff) digitalWrite(_pinLedRx, LOW);
}

static void updateBatteryLow(void)
{
  const unsigned char BATREAD_COUNT = 4;
  
  _isBattLow = HYGRO_BATTERY_OK;
  if (_pinBattery != 0xff)
  {
    unsigned int adcSum = 0;
    unsigned char battPct;
    for (unsigned char i=0; i<BATREAD_COUNT; ++i)
      adcSum += analogRead(_pinBattery);
    // Send level as percent of VCC
    battPct = (adcSum * 100UL) / (1024 * BATREAD_COUNT);

#ifdef LMREMOTE_SERIAL
    Serial.print("Battery level: "); Serial.print(retVal, DEC); Serial.print('\n');
#endif

    if (battPct < BATTERY_LOW_PCT)
      _isBattLow = HYGRO_BATTERY_LOW;
    _hygroVal = battPct;
  }
}

static void transmitTemp(unsigned char pin)
{
  unsigned char outbuf[4];
  unsigned char nodeId = _rfNodeBaseId + pin;
  unsigned int val = _previousReads[pin];
  val <<= (12 - (10 + TEMP_OVERSAMPLE_BITS));
  outbuf[0] = 0x90 | ((nodeId & 0x3f) >> 2);
  outbuf[1] = ((nodeId & 0x3f) << 6) | _isRecent | BYTE1_LMREMOTE_KEY | (val >> 8);
  outbuf[2] = (val & 0xff);
  outbuf[3] = _isBattLow | _hygroVal;
  //Serial.println(outbuf[3], HEX);

  // Don't check for air to be clear, we just woke from sleep and it will be milliseconds before
  // the RFM chip is actually up and running
  rf12_sendStart(outbuf, sizeof(outbuf));
  rf12_sendWait(SLEEPMODE_TX);
}

static void newTempsAvailable(void)
{
  // Enable the transmitter because it takes 1-5ms to turn on (3ms in my testing)
  rf12_sleep(RF12_WAKEUP);

  updateBatteryLow();
  // We're done with the ADC shut it down until the next wake cycle
  ADCSRA &= ~bit(ADEN);
  if (_isRecent && (_loopCnt > (RECENT_EXPIRE_SECS/_sleepInterval)))
  {
    //Serial.print("No longer recent\n");
    _isRecent = 0;
  }

  boolean hasTransmitted = false;
  for (unsigned char pin=0; pin < RF_PINS_PER_SOURCE; ++pin)
  {
    if (PIN_DISABLED(pin))
      continue;
    if (hasTransmitted) sleep(WDTO_15MS, false);
    transmitTemp(pin);
    hasTransmitted = true;
  }
  
  rf12_sleep(RF12_SLEEP);
  if (_pinLedTx != 0xff)
  {
    digitalWrite(_pinLedTx, HIGH);
    sleep(WDTO_15MS, false);
    digitalWrite(_pinLedTx, LOW);
  }
}

static void enableAdcPullups(void)
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

static void checkTemps(void)
{
  const unsigned char OVERSAMPLE_COUNT[] = {1, 4, 16, 64};  // 4^n
  boolean modified = false;
  unsigned int oversampled_adc = 0;

#ifdef LMREMOTE_SERIAL
  Serial.print(millis(), DEC);
  Serial.print(" Checking temps: ");
#endif

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
    Serial.print(oversampled_adc, DEC); Serial.print(' ');
#endif
    if (oversampled_adc != _previousReads[pin])
      modified = true;
    _previousReads[pin] = oversampled_adc;

    digitalWrite(_pinProbeSupplyBase + pin, LOW);
  }

#ifdef LMREMOTE_SERIAL
  Serial.print('\n');
#endif

  if (modified || (_sameCount > (30 / _sleepInterval)))
  {
    _sameCount = 0;
    newTempsAvailable();
  }
  else
    ++_sameCount;
}

static void setupSupplyPins(void)
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

static void stabilizeAdc(void)
{
  if (_pinBattery != 0xff)
  {
    unsigned int last;
    unsigned int curr = analogRead(_pinBattery);
    unsigned int cnt = 0;
    // Reads the adc a bunch of times until the value settles
    // Usually you hear "discard the first few ADC readings after sleep"
    // but this seems a bit more scientific as we wait for the AREF cap to charge
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
  Serial.print('\n'); delay(10);
#endif
}

void setup(void)
{
  stabilizeAdc();

  // Turn off the units we never use (this only affects non-sleep power)
  PRR = bit(PRUSART0) | bit(PRTWI) | bit(PRTIM1) | bit(PRTIM2);
  // Disable digital input buffers on the analog in ports
  DIDR0 = bit(ADC5D) | bit(ADC4D) | bit(ADC3D) | bit(ADC2D) | bit(ADC1D) | bit(ADC0D);
  DIDR1 = bit(AIN1D) | bit(AIN0D);

#ifdef LMREMOTE_SERIAL
  PRR &= ~bit(PRUSART0);
  Serial.begin(LMREMOTE_SERIAL); Serial.println("$UCID,lmremote"); delay(10);
#endif

  rf12_initialize(1, _rfBand);
  // Crystal 1.66MHz Low Battery Detect 2.2V
  rf12_control(0xC040);

  if (_pinLedRx != 0xff) pinMode(_pinLedRx, OUTPUT);
  if (_pinLedTx != 0xff) pinMode(_pinLedTx, OUTPUT);

  setupSupplyPins();

  // Force a transmit on next read
  memset(_previousReads, 0xff, sizeof(_previousReads));
}

void loop(void)
{
  //stabilizeAdc();
  checkTemps();
#ifdef LMREMOTE_SERIAL
  delay(10);
#endif
  sleepSeconds(_sleepInterval);
  ++_loopCnt;
}


