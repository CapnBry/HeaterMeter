#include <avr/wdt.h>
#include <avr/sleep.h>
#include <rf12_itplus.h>

// Power down CPU as much as possible, and attempt to sync receiver at exact
// transmit time. Can't be used with LMREMOTE_SERIAL
// Or if an output is defined (fan/servo)
#define MINIMAL_POWER_MODE
// Enabling LMREMOTE_SERIAL also disables MINIMAL_POWER_MODE
#define LMREMOTE_SERIAL 38400

// Base Idenfier for the RFM12B (0-63)
// The transmitted ID is this ID plus the pin number
const char _rfNodeBaseId = 0;
// RFM12B band RF12_433MHZ, RF12_868MHZ, RF12_915MHZ
const unsigned char _rfBand = RF12_915MHZ;
// How many seconds to delay between measurements
const unsigned char _sleepInterval = 2;
// Analog pins to read. This is a bitfield, LSB is analog 0
const unsigned char _enabledProbePins = 0x01;
// Analog pin connected to source power.  Set to 0xff to disable sampling
const unsigned char _pinBattery = 1;
// Digital pins for LEDs, 0xff to disable
const unsigned char _pinLedRx = 0xff;
const unsigned char _pinLedTx = 0xff; //9
// Digital pin used for sourcing power to the probe dividers
const unsigned char _pinProbeSupply = 4;
// Percentage (integer) of VCC where the battery is considered low (33% = 1.1V)
#define BATTERY_LOW_PCT 33
// Number of seconds to keep the "recent/new" bit set
#define RECENT_EXPIRE_SECS 1800
// Number of oversampling bits when measuring temperature [0-2]
#define TEMP_OVERSAMPLE_BITS 1

#define RF_PINS_PER_SOURCE 6 
#define PIN_DISABLED(pin) ((_enabledProbePins & (1 << pin)) == 0)

// Bits used in output packet
// byte0/1 reserved Node IDs
#define NODEID_MASTER      0x3F
// byte1
#define BYTE1_DUAL_PROBE   0x10
#define BYTE1_RECENT_BOOT  0x20
// hygro byte
#define HYGRO_BATTERY_OK   0x00
#define HYGRO_BATTERY_LOW  0x80
#define HYGRO_NO_HYGRO     0x6A
#define HYGRO_SECOND_PROBE 0x7D
#define HYGRO_LMREMOTE_KEY 0x7F

#ifdef LMREMOTE_SERIAL
#undef MINIMAL_POWER_MODE
#endif

#if defined(MINIMAL_POWER_MODE)
  #define SLEEPMODE_TX    2
  #define SLEEPMODE_ADC   SLEEP_MODE_ADC
#else
  #define SLEEPMODE_TX    1
  #define SLEEPMODE_ADC   SLEEP_MODE_IDLE
#endif

#define RECV_CYCLE_TIME  5000    // expected receive cycle, millisecond
#define MIN_RECV_WIN     8       // minimum window size, power of 2
#define MAX_RECV_WIN     512     // maximum window size, power of 2

static unsigned int _previousReads[RF_PINS_PER_SOURCE];
static unsigned long _tempReadLast;
static unsigned int _loopCnt;
static unsigned char _sameCount;
static unsigned char _isRecent = BYTE1_RECENT_BOOT;
static unsigned char _isBattLow;

static unsigned int _recvCycleAct;
static unsigned int _recvWindow;
static unsigned long _recvLast;
static bool _recvSynced;

static bool packetReceived(unsigned char nodeId, unsigned int val)
{
#if LMREMOTE_SERIAL
  Serial.print(F("IN("));
  Serial.print(nodeId);
  Serial.print(',');
  Serial.print(rf12_rssi(), DEC);
  Serial.print(F(")="));
  Serial.print(val);
  Serial.print('\n');
#endif

  if (nodeId != NODEID_MASTER)
    return false;
  // val contains requested fan speed percent
  return true;
}

static bool rf12_doWork(void)
{
  if (rf12_recvDone() && rf12_crc == 0)
  {
    if (_pinLedRx != 0xff) digitalWrite(_pinLedRx, HIGH);

    unsigned char nodeId = ((rf12_buf[0] & 0x0f) << 2) | (rf12_buf[1] >> 6);
    unsigned int val = (rf12_buf[1] & 0x0f) << 8 | rf12_buf[2];
    return packetReceived(nodeId, val);
  }
  if (_pinLedRx != 0xff) digitalWrite(_pinLedRx, LOW);
  return false;
}

static volatile bool _adcBusy;
ISR(ADC_vect) { _adcBusy = false; }

static unsigned int analogReadSleep(unsigned char pin)
{
  _adcBusy = true;
  ADMUX = (DEFAULT << 6) | pin;
  bitSet(ADCSRA, ADIE);
  set_sleep_mode(SLEEPMODE_ADC);
  while (_adcBusy)
    sleep_mode();
  return ADC;
}

// The WDT is used solely to wake us from sleep
#if defined(MINIMAL_POWER_MODE)
static volatile bool _watchdogWaiting;
ISR(WDT_vect) { _watchdogWaiting = false; }

static void sleepPeriod(uint8_t wdt_period)
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

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
  wdt_disable();
  sleep_disable();
}

static void sleepPwrDown(unsigned int msec)
// Code copied from Sleepy::loseSomeSleep(), jeelib
{
  unsigned int msleft = msec;
  while (msleft >= 16)
  {
    char wdp = 0; // wdp 0..9 corresponds to roughly 16..8192 ms
    // calc wdp as log2(msleft/16), i.e. loop & inc while next value is ok
    for (unsigned int m = msleft; m >= 32; m >>= 1)
      if (++wdp >= 9)
        break;
    _watchdogWaiting = true;
    sleepPeriod(wdp);
    unsigned int halfms = 8 << wdp;
    msleft -= halfms;
    if (_watchdogWaiting)
      continue;
    msleft -= halfms;
  }

  // adjust the milli ticks, since we will have missed several
#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__) || defined (__AVR_ATtiny44__)
    extern volatile unsigned long millis_timer_millis;
    millis_timer_millis += msecs - msleft;
#else
    extern volatile unsigned long timer0_millis;
    timer0_millis += msec - msleft;
#endif
}
#endif

static void sleepIdle(unsigned int msec)
{
  unsigned long start = millis();
  set_sleep_mode(SLEEP_MODE_IDLE);
  while (millis() - start < msec)
    sleep_mode();
}

static void sleep(unsigned int msec)
{
#if defined(MINIMAL_POWER_MODE)
  sleepPwrDown(msec);
#else
  sleepIdle(msec);
#endif /* SLEEP_MODE_PWR_DOWN */
}

static void resetEstimate(void)
{
#if defined(LMREMOTE_SERIAL) && defined(_DEBUG)
  Serial.print(F("Resetting Estimate\n"));
#endif
  _recvCycleAct = RECV_CYCLE_TIME;
  _recvWindow = MAX_RECV_WIN;
  _recvSynced = false;

  unsigned long start = millis();
  while (!rf12_doWork())
    if (millis() - start > RECV_CYCLE_TIME)
      return;
  _recvLast = millis();
}

static bool optimalSleep(void)
// Code adapted from jeelib/examples/syncRecv optimalSleep()
{
  unsigned char lost = (millis() - _recvLast + _recvCycleAct / 2) / _recvCycleAct;
  if (lost > 10)
  {
    resetEstimate();
    return false;
  }

  unsigned long predict = _recvLast + (lost + 1) * _recvCycleAct;
  unsigned int sleepDur =  predict - _recvWindow - millis();
  sleep(sleepDur);
  rf12_sleep(RF12_WAKEUP);

  unsigned long recvTime;
  unsigned long timeout = predict + _recvWindow;
  do {
    recvTime = millis();
    if ((long)(recvTime - timeout) >= 0)
    {
#if defined(LMREMOTE_SERIAL) && defined(_DEBUG)
      Serial.print(" f "); Serial.print(lost);
      Serial.print(" s "); Serial.print(sleepDur);
      Serial.print(" e "); Serial.print(_recvCycleAct);
      Serial.print(" w "); Serial.print(_recvWindow);
      Serial.print(" p "); Serial.print(predict);
      Serial.print(" r "); Serial.println(recvTime);
      Serial.flush();
#endif
      // double the window every N/2 packets lost
      ++lost;
      if (lost % 2 == 1 && _recvWindow < MAX_RECV_WIN)
        _recvWindow *= 2;

      return false;
    }
  } while (!rf12_doWork());

  unsigned int newEst = (recvTime - _recvLast) / (lost + 1);
  if (_recvSynced)
    _recvCycleAct = (4 * _recvCycleAct + newEst + 3) / 5; // 5-fold smoothing
  else
    _recvCycleAct = newEst;
  _recvLast = recvTime;

#if defined(LMREMOTE_SERIAL) && defined(_DEBUG)
  Serial.print(" n "); Serial.print(lost);
  Serial.print(" e "); Serial.print(_recvCycleAct);
  Serial.print(" E "); Serial.print(newEst);
  Serial.print(" w "); Serial.print(_recvWindow);
  Serial.print(" p "); Serial.print(predict);
  Serial.print(" r "); Serial.println(recvTime);
#endif

  if (_recvWindow > MIN_RECV_WIN)
    _recvWindow /= 2;
  else
    _recvSynced = true;

  return true;
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
      adcSum += analogReadSleep(_pinBattery);
    // Percent of VCC
    battPct = (adcSum * 100UL) / (1024 * BATREAD_COUNT);

#ifdef LMREMOTE_SERIAL
    Serial.print("Battery %: "); Serial.print(battPct, DEC); Serial.print('\n');
#endif

    if (battPct < BATTERY_LOW_PCT)
      _isBattLow = HYGRO_BATTERY_LOW;
  }
}

static void transmitTemp(unsigned char pin)
{
  unsigned char outbuf[4];
  unsigned char nodeId = _rfNodeBaseId + pin;
  unsigned int val = _previousReads[pin];
  val <<= (12 - (10 + TEMP_OVERSAMPLE_BITS));
  outbuf[0] = 0x90 | ((nodeId & 0x3f) >> 2);
  outbuf[1] = ((nodeId & 0x3f) << 6) | _isRecent | (val >> 8);
  outbuf[2] = (val & 0xff);
  outbuf[3] = HYGRO_LMREMOTE_KEY | _isBattLow;
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
  if (_isRecent && (millis() > RECENT_EXPIRE_SECS))
  {
    //Serial.print("No longer recent\n");
    _isRecent = 0;
  }

  boolean hasTransmitted = false;
  for (unsigned char pin=0; pin < RF_PINS_PER_SOURCE; ++pin)
  {
    if (PIN_DISABLED(pin))
      continue;
    if (hasTransmitted) sleep(16);
    transmitTemp(pin);
    hasTransmitted = true;
  }
  
  rf12_sleep(RF12_SLEEP);
  if (_pinLedTx != 0xff)
  {
    digitalWrite(_pinLedTx, HIGH);
    sleep(16);
    digitalWrite(_pinLedTx, LOW);
  }
}

static void stabilizeAdc(void)
{
  const unsigned char INTERNAL_REF = 0b1110;
  unsigned int last;
  unsigned char totalCnt = 0;
  unsigned char sameCnt = 0;
  unsigned int curr = analogReadSleep(INTERNAL_REF);
  // Reads the adc a bunch of times until the value settles
  // Usually you hear "discard the first few ADC readings after sleep"
  // but this seems a bit more scientific as we wait for the AREF cap to charge
  do {
    ++totalCnt;
#ifdef LMREMOTE_SERIAL
    Serial.print(curr, DEC);
    Serial.print(' ');
#endif
    last = curr;
    curr = analogReadSleep(INTERNAL_REF);
    if (last == curr)
      ++sameCnt;
    else
      sameCnt = 0;
  } while ((totalCnt < 64) && (sameCnt < 4));
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

  // Enable the ADC
  ADCSRA |= bit(ADEN);
  // The probe themistor voltage dividers are normally powered down
  // to save power, using digital lines to supply Vcc to them.
  digitalWrite(_pinProbeSupply, HIGH);
  // Wait for AREF capacitor to charge
  stabilizeAdc();

  for (unsigned char pin=0; pin < RF_PINS_PER_SOURCE; ++pin)
  {
    if (PIN_DISABLED(pin))
      continue;

    for (unsigned char o=0; o<OVERSAMPLE_COUNT[TEMP_OVERSAMPLE_BITS]; ++o)
    {
      unsigned int adc = analogReadSleep(pin);
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
  }
  digitalWrite(_pinProbeSupply, LOW);

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
  // Disable the ADC
  ADCSRA &= ~bit(ADEN);

  _tempReadLast = millis();
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
  Serial.begin(LMREMOTE_SERIAL); Serial.println("$UCID,lmremote"); Serial.flush();
#endif

  rf12_initialize(_rfBand);
  // Crystal 1.66MHz Low Battery Detect 2.2V
  rf12_control(0xC040);

  if (_pinLedRx != 0xff) pinMode(_pinLedRx, OUTPUT);
  if (_pinLedTx != 0xff) pinMode(_pinLedTx, OUTPUT);

  pinMode(_pinProbeSupply, OUTPUT);

  // Force a transmit on next read
  memset(_previousReads, 0xff, sizeof(_previousReads));
}

void loop(void)
{
  if (_loopCnt % 2 == 0)
    checkTemps();

#ifdef LMREMOTE_SERIAL
  Serial.flush();
#endif

  if (_recvLast == 0)
    resetEstimate();
  else
    optimalSleep();
  rf12_sleep(RF12_SLEEP);

  ++_loopCnt;
}


