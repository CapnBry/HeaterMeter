#include <avr/wdt.h>
#include <avr/sleep.h>
#include <rf12_itplus.h>
#include <digitalWriteFast.h>
//#include "grillpid.h"

// Power down CPU as much as possible, and attempt to sync receiver at exact
// transmit time. Can't be used with LMREMOTE_SERIAL
// Or if an output is defined (fan/servo)
#define MINIMAL_POWER_MODE
// Enabling LMREMOTE_SERIAL also disables MINIMAL_POWER_MODE
//#define LMREMOTE_SERIAL 38400

// Base Idenfier for the RFM12B (0-63)
// The transmitted ID is this ID plus the pin number
const char _rfNodeBaseId = 0;
// RFM12B band RF12_433MHZ, RF12_868MHZ, RF12_915MHZ
const unsigned char _rfBand = RF12_915MHZ;
// How many seconds to delay between temperature measurements
const unsigned char _sleepInterval = 2;
// Analog pins to read. This is a bitfield, LSB is analog 5
const unsigned char _enabledProbePins = 0x01;
// Analog pin connected to source power.  Set to 0xff to disable sampling
const unsigned char _pinBattery = 0xff;
// Digital pins for LEDs, 0xff to disable
const unsigned char _pinLedRx = 0xff;
const unsigned char _pinLedTx = 0xff;
const unsigned char _pinLedRxSearch = 4;   // ATmega pin 6
const unsigned char _pinLedRxConverge = 5; // ATmega pin 11
const unsigned char _pinLedRxLocked = 9;   // ATmega pin 15
// Digital pin used for sourcing power to the probe dividers
const unsigned char _pinProbeSupply = 7;   // ATmega pin 13
// Digital pin used for fan output, must support PWM, 0xff to disable
const unsigned char _pinOutputFan = 3;     // ATmega pin 5
// Digital pin used for servo output, 0xff to disable
const unsigned char _pinOutputServo = 8;   // ATmega pin 14
// Percentage (integer) of VCC where the battery is considered low (33% = 1.1V)
#define BATTERY_LOW_PCT 33
// Number of seconds to keep the "recent/new" bit set
#define RECENT_EXPIRE_SECS 1800

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

#define RECV_CYCLE_TIME  5000    // expected receive cycle, millisecond
#define MIN_RECV_WIN     8       // minimum window size (ms), power of 2
#define MAX_RECV_WIN     128     // maximum window size (ms), power of 2

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

#ifndef digitalWriteFast
#define digitalWriteFast digitalWrite
#endif

#define WAKETASK_COUNT       3
#define WAKETASK_TEMPERATURE 0
#define WAKETASK_PIDOUTPUT   1
#define WAKETASK_RFRECEIVE   2

static unsigned int _previousReads[RF_PINS_PER_SOURCE];
static unsigned long _tempReadLast;
static unsigned char _sameCount;
static unsigned char _isRecent = BYTE1_RECENT_BOOT;
static unsigned char _isBattLow;

#define RECVSTATE_SEARCHING  0
#define RECVSTATE_CONVERGING 1
#define RECVSTATE_LOCKED     2

static unsigned int _recvCycleAct;
static unsigned int _recvWindow;
static unsigned long _recvLast;
static unsigned char _recvLost;
static unsigned char _recvState;

// #define LMREMOTE_OUTPUT
#define TEMP_OVERSAMPLE_BITS 2

#if defined(LMREMOTE_OUTPUT)
GrillPid pid;
#endif //LMREMOTE_OUTPUT

static void setupPidOutput(void)
{
#if defined(LMREMOTE_OUTPUT)
  pid.setFanMinSpeed(10);
  pid.setFanMaxSpeed(100);
  pid.setServoMinPos(120);
  pid.setServoMaxPos(160);
  pid.setOutputFlags(PIDFLAG_INVERT_SERVO);
#endif //LMREMOTE_OUTPUT
}

static void setOutputPercent(unsigned char val)
{
#if defined(LMREMOTE_OUTPUT)
  if (_pinOutputFan != 0xff || _pinOutputServo != 0xff)
    pid.setPidOutput(val);
#endif
}

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
  setOutputPercent(val);
  return true;
}

static bool rf12_doWork(void)
{
  if (rf12_recvDone() && rf12_crc == 0)
  {
    if (_pinLedRx != 0xff) digitalWriteFast(_pinLedRx, HIGH);

    unsigned char nodeId = ((rf12_buf[0] & 0x0f) << 2) | (rf12_buf[1] >> 6);
    unsigned int val = (rf12_buf[1] & 0x0f) << 8 | rf12_buf[2];
    return packetReceived(nodeId, val);
  }
  if (_pinLedRx != 0xff) digitalWriteFast(_pinLedRx, LOW);
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
#else
static void sleepPeriod(uint8_t wdt_period) {}
static void sleepPwrDown(unsigned int msec) {}
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
#endif /* MINIMAL_POWER_MODE */
}

static void rfSetRecvState(const unsigned char state)
{
#if defined(LMREMOTE_SERIAL) && defined(_DEBUG)
  Serial.print(millis(), DEC);
  Serial.print(F(" rfSetRecvState ")); Serial.println(state, DEC);
#endif
  _recvState = state;

  if (_pinLedRxSearch != 0xff) digitalWriteFast(_pinLedRxSearch, LOW);
  if (_pinLedRxConverge != 0xff) digitalWriteFast(_pinLedRxConverge, LOW);
  if (_pinLedRxLocked != 0xff) digitalWriteFast(_pinLedRxLocked, LOW);

  switch (state)
  {
    case RECVSTATE_SEARCHING:
      if (_pinLedRxSearch != 0xff) digitalWriteFast(_pinLedRxSearch, HIGH);
      // If we're offline disable the blower/servo output
      _recvCycleAct = RECV_CYCLE_TIME;
      _recvWindow = MAX_RECV_WIN;
      _recvLast = 0;
      _recvLost = 0;
      setOutputPercent(0);
      break; /* SEARCHING */

    case RECVSTATE_CONVERGING:
      if (_pinLedRxConverge != 0xff) digitalWriteFast(_pinLedRxConverge, HIGH);
      break;

    case RECVSTATE_LOCKED:
      if (_pinLedRxLocked != 0xff) digitalWriteFast(_pinLedRxLocked, HIGH);
      break;
  }
}

static void rfContinueSearch(void)
{
  if (!rf12_doWork())
    return;

  _recvLast = millis();
  rfSetRecvState(RECVSTATE_CONVERGING);
}

static void rfReceive(void)
// Code adapted from jeelib/examples/syncRecv optimalSleep()
{
  if (_recvState == RECVSTATE_SEARCHING)
  {
    rfContinueSearch();
    return;
  }

  unsigned long wakeTime = millis();
  rf12_sleep(RF12_WAKEUP);

  unsigned long recvTime;
  do {
    recvTime = millis();
    if (recvTime - wakeTime > (_recvWindow * 2))
    {
      rf12_sleep(RF12_SLEEP);

#if defined(LMREMOTE_SERIAL) && defined(_DEBUG)
      Serial.print(" f "); Serial.print(_recvLost);
      //Serial.print(" s "); Serial.print(sleepDur);
      Serial.print(" e "); Serial.print(_recvCycleAct);
      Serial.print(" w "); Serial.print(_recvWindow);
      //Serial.print(" p "); Serial.print(predict);
      Serial.print(" r "); Serial.println(recvTime);
      Serial.flush();
#endif
      ++_recvLost;
      if (_recvLost > 8)
        rfSetRecvState(RECVSTATE_SEARCHING);
      else
      {
        // double the window every lost packet
        if (_recvWindow < MAX_RECV_WIN)
          _recvWindow *= 2;
        if (_recvState == RECVSTATE_LOCKED)
          rfSetRecvState(RECVSTATE_CONVERGING);
      }

      return;
    }
  } while (!rf12_doWork());
  rf12_sleep(RF12_SLEEP);

  unsigned int newEst = (recvTime - _recvLast) / (_recvLost + 1);
  if (_recvState == RECVSTATE_LOCKED)
    _recvCycleAct = (4 * _recvCycleAct + newEst + 3) / 5; // 5-fold smoothing
  else
    _recvCycleAct = newEst;
  _recvLast = recvTime;

#if defined(LMREMOTE_SERIAL) && defined(_DEBUG)
  Serial.print(" n "); Serial.print(_recvLost);
  Serial.print(" e "); Serial.print(_recvCycleAct);
  Serial.print(" E "); Serial.print(newEst);
  Serial.print(" w "); Serial.print(_recvWindow);
  //Serial.print(" p "); Serial.print(predict);
  Serial.print(" r "); Serial.println(recvTime);
#endif

  if (_recvWindow > MIN_RECV_WIN)
    _recvWindow /= 2;
  else if (_recvState != RECVSTATE_LOCKED)
    rfSetRecvState(RECVSTATE_LOCKED);
  _recvLost = 0;
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
    Serial.print(F("Battery %: ")); Serial.print(battPct, DEC); Serial.print('\n');
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
  //val <<= (12 - (10 + TEMP_OVERSAMPLE_BITS));
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
    //Serial.println(F("No longer recent"));
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
    digitalWriteFast(_pinLedTx, HIGH);
    sleep(16);
    digitalWriteFast(_pinLedTx, LOW);
  }
}

static void stabilizeAdc(void)
{
  const unsigned char INTERNAL_REF = 0b1110;
  int last;
  unsigned char totalCnt = 0;
  unsigned char sameCnt = 0;
  int curr = analogReadSleep(INTERNAL_REF);
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
    if (abs(last - curr) < 2)
      ++sameCnt;
    else
      sameCnt = 0;
  } while ((totalCnt < 64) && (sameCnt < 4));
}

static void checkTemps(void)
{
  _tempReadLast = millis();
  if (_enabledProbePins == 0)
    return;

  const unsigned char OVERSAMPLE_COUNT[] = {1, 4, 16, 64};  // 4^n
  boolean modified = false;
  unsigned int oversampled_adc = 0;

#ifdef LMREMOTE_SERIAL
  Serial.print(millis(), DEC);
  Serial.print(F(" Checking temps: "));
#endif

  // Enable the ADC
  ADCSRA |= bit(ADEN);
  // The probe themistor voltage dividers are normally powered down
  // to save power, using digital lines to supply Vcc to them.
  if (_pinProbeSupply != 0xff) digitalWriteFast(_pinProbeSupply, HIGH);
  // Wait for AREF capacitor to charge
  stabilizeAdc();

  for (unsigned char pin=0; pin < RF_PINS_PER_SOURCE; ++pin)
  {
    if (PIN_DISABLED(pin))
      continue;

    for (unsigned char o=0; o<OVERSAMPLE_COUNT[TEMP_OVERSAMPLE_BITS]; ++o)
    {
      // 'Pins' 0-5 are actually wired starting at A5 down to A0
      unsigned int adc = analogReadSleep(5 - pin);
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
  if (_pinProbeSupply != 0xff) digitalWriteFast(_pinProbeSupply, LOW);

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
}

static unsigned char scheduleSleep(void)
{
  // Need millis to remain constant through this function to prevent overflow
  unsigned long m = millis();
  unsigned long dur[WAKETASK_COUNT];
  unsigned int interval[WAKETASK_COUNT];

  dur[WAKETASK_TEMPERATURE] = _tempReadLast;
  interval[WAKETASK_TEMPERATURE] = _sleepInterval * 1000U;

  dur[WAKETASK_RFRECEIVE] = _recvLast;
  if (_recvState == RECVSTATE_SEARCHING)
    interval[WAKETASK_RFRECEIVE] = 0;
  else
    interval[WAKETASK_RFRECEIVE] = (_recvLost + 1) * _recvCycleAct - _recvWindow;

#if defined(LMREMOTE_OUTPUT)
  dur[WAKETASK_PIDOUTPUT] = pid.getLastWorkMillis();
  interval[WAKETASK_PIDOUTPUT] = TEMP_MEASURE_PERIOD;
#else
  dur[WAKETASK_PIDOUTPUT] = 0;
  interval[WAKETASK_PIDOUTPUT] = 0;
#endif // !LMREMOTE_OUTPUT

  /* check to see if any task is past due */
  for (unsigned char task=0; task<WAKETASK_COUNT; ++task)
  {
    if (interval[task] == 0)
      continue;
    dur[task] = m - dur[task];
    if (dur[task] > interval[task])
      return task;
  }

  // We now know that all of our events are in the future so it is ok to subtract
  unsigned char retVal = WAKETASK_TEMPERATURE;
  for (unsigned char task=0; task<WAKETASK_COUNT; ++task)
  {
    if (interval[task] == 0)
      continue;
    dur[task] = interval[task] - dur[task];
    if (dur[task] < dur[retVal])
      retVal = task;
  }

  sleep(dur[retVal]);
  return retVal;
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
  Serial.begin(LMREMOTE_SERIAL); Serial.println("$UCID,lmremote," __DATE__ " " __TIME__); Serial.flush();
#endif

  rf12_initialize(_rfBand, 1);
  if (_pinLedRx != 0xff) pinMode(_pinLedRx, OUTPUT);
  if (_pinLedTx != 0xff) pinMode(_pinLedTx, OUTPUT);
  if (_pinLedRxSearch != 0xff) pinMode(_pinLedRxSearch, OUTPUT);
  if (_pinLedRxConverge != 0xff) pinMode(_pinLedRxConverge, OUTPUT);
  if (_pinLedRxLocked != 0xff) pinMode(_pinLedRxLocked, OUTPUT);

  if (_pinProbeSupply != 0xff) pinMode(_pinProbeSupply, OUTPUT);

  // Force a transmit on next read
  memset(_previousReads, 0xff, sizeof(_previousReads));
  rfSetRecvState(RECVSTATE_SEARCHING);
  setupPidOutput();
}

void loop(void)
{
#ifdef LMREMOTE_SERIAL
  Serial.flush();
#endif

  unsigned char wakeTask = scheduleSleep();
  switch (wakeTask)
  {
    case WAKETASK_RFRECEIVE:
      rfReceive();
      break;
    case WAKETASK_TEMPERATURE:
      checkTemps();
      break;
    case WAKETASK_PIDOUTPUT:
#if defined(LMREMOTE_OUTPUT)
      pid.doWork();
#endif
      break;
  }
}
