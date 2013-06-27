// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
// GrillPid uses TIMER1 COMPB and OVF vectors, as well as modifies the waveform
// generation mode of TIMER1. Blower output pin does not need to be a hardware
// PWM pin.
// Fan output is 489Hz fast PWM
// Servo output is 50Hz pulse duration
#include <math.h>
#include <string.h>

#include "grillpid.h"
#include "strings.h"

extern const GrillPid pid;

// The time (ms) of the measurement period
#define TEMP_MEASURE_PERIOD 1000
// The temperatures are averaged over 1, 2, 4 or 8 samples per period
#define TEMP_AVG_COUNT 8
// 2/(1+Number of samples used in the exponential moving average)
#define TEMPPROBE_AVG_SMOOTH (2.0f/(1.0f+60.0f))
#define FANSPEED_AVG_SMOOTH (2.0f/(1.0f+240.0f))
// Once entering LID OPEN mode, the minimum number of seconds to stay in
// LID OPEN mode before autoresuming due to temperature returning to setpoint 
#define LIDOPEN_MIN_AUTORESUME 30

// Servo refresh period in usec, 20000 usec = 20ms = 50Hz
#define SERVO_REFRESH          20000

// For this calculation to work, ccpm()/8 must return a round number
#define uSecToTicks(x) ((unsigned int)(clockCyclesPerMicrosecond() / 8) * x)

//#define TIMER1_DEBUG
#if defined(TIMER1_DEBUG)
static bool ovf;
static unsigned int rst;
#endif

ISR(TIMER1_COMPB_vect)
{
#if defined(TIMER1_DEBUG)
  rst = TCNT1;
#endif
  // In fan mode at 100% the COMPB interrupt never fires, so it is always safe
  // to set the pin LOW
  digitalWrite(pid.getBlowerPin(), LOW);
}

ISR(TIMER1_OVF_vect)
{
#if defined(TIMER1_DEBUG)
  ovf = true;
#endif
  if (OCR1B != 0)
    digitalWrite(pid.getBlowerPin(), HIGH);
}

static void calcExpMovingAverage(const float smooth, float *currAverage, float newValue)
{
  if (isnan(*currAverage))
    *currAverage = newValue;
  else
  {
    newValue = newValue - *currAverage;
    *currAverage = *currAverage + (smooth * newValue);
  }
}

void ProbeAlarm::updateStatus(int value)
{
  // Low: Arming point >= Thresh + 1.0f, Trigger point < Thresh
  // A low alarm set for 100 enables at 101.0 and goes off at 99.9999...
  if (getLowEnabled())
  {
    if (value >= (getLow() + 1))
      Armed[ALARM_IDX_LOW] = true;
    else if (value < getLow() && Armed[ALARM_IDX_LOW])
      Ringing[ALARM_IDX_LOW] = true;
  }

  // High: Arming point < Thresh - 1.0f, Trigger point >= Thresh
  // A high alarm set for 100 enables at 98.9999... and goes off at 100.0
  if (getHighEnabled())
  {
    if (value < (getHigh() - 1))
      Armed[ALARM_IDX_HIGH] = true;
    else if (value >= getHigh() && Armed[ALARM_IDX_HIGH])
      Ringing[ALARM_IDX_HIGH] = true;
  }

  if (pid.isLidOpen())
    Ringing[ALARM_IDX_LOW] = Ringing[ALARM_IDX_HIGH] = false;
}

void ProbeAlarm::setHigh(int value)
{
  setThreshold(ALARM_IDX_HIGH, value);
}

void ProbeAlarm::setLow(int value)
{
  setThreshold(ALARM_IDX_LOW, value);
}

void ProbeAlarm::setThreshold(unsigned char idx, int value)
{
  Armed[idx] = false;
  Ringing[idx] = false;
  /* 0 just means silence */
  if (value == 0)
    return;
  Thresholds[idx] = value;
}

TempProbe::TempProbe(const unsigned char pin) :
  _pin(pin), Temperature(NAN), TemperatureAvg(NAN)
{
}

void TempProbe::loadConfig(struct __eeprom_probe *config)
{
  _probeType = config->probeType;
  Offset = config->tempOffset;
  memcpy(Steinhart, config->steinhart, sizeof(Steinhart));
  Alarms.setLow(config->alarmLow);
  Alarms.setHigh(config->alarmHigh);
}

void TempProbe::setProbeType(unsigned char probeType)
{
  _probeType = probeType;
  _accumulator = 0;
  _accumulatedCount = 0;
  Temperature = NAN;
  TemperatureAvg = NAN;
}

void TempProbe::addAdcValue(unsigned int analog_temp)
{
  if (analog_temp == 0) // >= MAX is reduced in readTemp()
    _accumulator = 0;
  else if (_accumulatedCount == 0)
    _accumulator = analog_temp;
  else if (_accumulator != 0)
    _accumulator += analog_temp;
  ++_accumulatedCount;
}

void TempProbe::readTemp(void)
{
  const unsigned char OVERSAMPLE_COUNT[] = {1, 4, 16, 64};  // 4^n
  unsigned int oversampled_adc = 0;
  for (unsigned char i=OVERSAMPLE_COUNT[TEMP_OVERSAMPLE_BITS]; i; --i)
  {
    unsigned int adc = analogRead(_pin);
    // If we get *any* analogReads that are 0 or 1023, the measurement for 
    // the entire period is invalidated, so set the _accumulator to 0
    if (adc == 0 || adc >= 1023)
    {
      addAdcValue(0);
      return;
    }
    oversampled_adc += adc;
  }
  oversampled_adc = oversampled_adc >> TEMP_OVERSAMPLE_BITS;
  addAdcValue(oversampled_adc);
}

void TempProbe::calcTemp(void)
{
  const float ADCmax = (1 << (10+TEMP_OVERSAMPLE_BITS)) - 1;
  if (_accumulatedCount != 0)
  {
    unsigned int ADCval = _accumulator / _accumulatedCount;
    _accumulatedCount = 0;

    // Units 'A' = ADC value
    if (pid.getUnits() == 'A')
    {
      Temperature = ADCval;
      return;
    }

    if (ADCval != 0)  // Vout >= MAX is reduced in readTemp()
    {
      float R, T;
      // If you put the fixed resistor on the Vcc side of the thermistor, use the following
      R = Steinhart[3] / ((ADCmax / (float)ADCval) - 1.0f);
      // If you put the thermistor on the Vcc side of the fixed resistor use the following
      //R = Steinhart[3] * ADCmax / (float)Vout - Steinhart[3];

      // Units 'R' = resistance, unless this is the pit probe (which should spit out Celsius)
      if (pid.getUnits() == 'R' && this != pid.Probes[TEMP_PIT])
      {
        Temperature = R;
        return;
      };

      // Compute degrees K
      R = log(R);
      T = 1.0f / ((Steinhart[2] * R * R + Steinhart[1]) * R + Steinhart[0]);

      setTemperatureC(T - 273.15f);
    } /* if ADCval */
    else
      Temperature = NAN;
  } /* if accumulatedcount */

  if (hasTemperature())
  {
    calcExpMovingAverage(TEMPPROBE_AVG_SMOOTH, &TemperatureAvg, Temperature);
    Alarms.updateStatus(Temperature);
  }
  else
    Alarms.silenceAll();
}

void TempProbe::setTemperatureC(float T)
{
  // Sanity - anything less than -20C (-4F) or greater than 500C (932F) is rejected
  if (T <= -20.0f || T > 500.0f)
    Temperature = NAN;
  else
  {
    if (pid.getUnits() == 'F')
      Temperature = (T * (9.0f / 5.0f)) + 32.0f;
    else
      Temperature = T;
    Temperature += Offset;
  }
}

GrillPid::GrillPid(const unsigned char blowerPin) :
    _blowerPin(blowerPin), _periodCounter(0x80), _units('F'), FanSpeedAvg(NAN)
{
  pinMode(_blowerPin, OUTPUT);
  // To prevent the wrong thing from happening by default, all output is disabled
  // until setOutputDevice is called for the first time
  //setOutputDevice(GrillPidOutput::Fan);
}

unsigned int GrillPid::countOfType(unsigned char probeType) const
{
  unsigned char retVal = 0;
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
    if (Probes[i]->getProbeType() == probeType)
      ++retVal;
  return retVal;  
}

unsigned char GrillPid::getOutputMax(void) const
{
  switch (_outputDevice)
  {
    case PIDOUTPUT_Fan:
      return _maxFanSpeed;
      break;

    // In servo mode the "fan speed" always is 0-100 as in percent of open
    case PIDOUTPUT_Servo:
    default:
      return 100;
      break;
  }
}

/* Calucluate the desired fan speed using the proportional–integral-derivative (PID) controller algorithm */
inline void GrillPid::calcFanSpeed(void)
{
  unsigned char lastFanSpeed = _fanSpeed;
  _fanSpeed = 0;

  // If the pit probe is registering 0 degrees, don't jack the fan up to MAX
  if (!Probes[TEMP_PIT]->hasTemperature())
    return;

  // If we're in lid open mode, fan should be off
  if (isLidOpen())
    return;

  float currentTemp = Probes[TEMP_PIT]->Temperature;
  float error;
  error = _setPoint - currentTemp;

  // PPPPP = fan speed percent per degree of error
  _pidCurrent[PIDP] = Pid[PIDP] * error;

  // IIIII = fan speed percent per degree of accumulated error
  unsigned char max = getOutputMax();
  // anti-windup: Make sure we only adjust the I term while inside the proportional control range
  if ((error > 0 && lastFanSpeed < max) || (error < 0 && lastFanSpeed > 0))
    _pidCurrent[PIDI] += Pid[PIDI] * error;

  // DDDDD = fan speed percent per degree of change over TEMPPROBE_AVG_SMOOTH period
  _pidCurrent[PIDD] = Pid[PIDD] * (Probes[TEMP_PIT]->TemperatureAvg - currentTemp);
  // BBBBB = fan speed percent
  _pidCurrent[PIDB] = Pid[PIDB];

  int control = _pidCurrent[PIDB] + _pidCurrent[PIDP] + _pidCurrent[PIDI] + _pidCurrent[PIDD];
  if (control >= max)
    _fanSpeed = max;
  else if (control > 0)
    _fanSpeed = control;
}

inline void GrillPid::commitFanSpeed(void)
{
  /* Long PWM period is 10 sec */
  const unsigned int LONG_PWM_PERIOD = 10000;
  const unsigned int PERIOD_SCALE = (LONG_PWM_PERIOD / TEMP_MEASURE_PERIOD);

  calcExpMovingAverage(FANSPEED_AVG_SMOOTH, &FanSpeedAvg, _fanSpeed);

  if (_outputDevice == PIDOUTPUT_Fan)
  {
    /* For anything above _minFanSpeed, do a nomal PWM write.
       For below _minFanSpeed we use a "long pulse PWM", where
       the pulse is 10 seconds in length.  For each percent we are
       emulating, run the fan for one interval. */
    unsigned char output;
    if (_fanSpeed >= _minFanSpeed)
    {
      output = _fanSpeed;
      _longPwmTmr = 0;
    }
    else
    {
      // Simple PWM, ON for first [FanSpeed] intervals then OFF
      // for the remainder of the period
      if (((PERIOD_SCALE * _fanSpeed / _minFanSpeed) > _longPwmTmr))
        output = _minFanSpeed;
      else
        output = 0;

      if (++_longPwmTmr > (PERIOD_SCALE - 1))
        _longPwmTmr = 0;
    }  /* long PWM */

    if (_invertPwm)
      output = _maxFanSpeed - output;
    OCR1B = (unsigned int)output * 512 / 100;
  }
  else
  {
    // GrillPidOutput::Servo
    unsigned char output;
    if (_invertPwm)
      output = 100 - _fanSpeed;
    else
      output = _fanSpeed;
    // Get the output speed in 10x usec by LERPing between min and max
    output = ((_maxFanSpeed - _minFanSpeed) * (unsigned int)output / 100) + _minFanSpeed;
    OCR1B = uSecToTicks(10U * output);
  }

#if defined(TIMER1_DEBUG)
  SerialX.print("HMLG,0,");
  SerialX.print(" ICR1="); SerialX.print(ICR1, DEC);
  SerialX.print(" OCR1B="); SerialX.print(OCR1B, DEC);
  SerialX.print(" TCCR1A=x"); SerialX.print(TCCR1A, HEX);
  SerialX.print(" TCCR1B=x"); SerialX.print(TCCR1B, HEX);
  SerialX.print(" TCCR1C=x"); SerialX.print(TCCR1C, HEX);
  SerialX.print(" TIMSK1=x"); SerialX.print(TIMSK1, HEX);
  if (ovf) { ovf = false; SerialX.print(" ovf"); }
  if (rst) { SerialX.print(' '); SerialX.print(rst, DEC); rst = 0; }
  if (bit_is_set(TIFR1, TOV1)) { SerialX.print(" TOV1"); TIFR1 = bit(TOV1); }
  SerialX.nl();
#endif
}

void GrillPid::setOutputDevice(unsigned char outputDevice)
{
  TIMSK1 = 0;
  // Fast PWM Timer with TOP=ICR1 with no OC1x output
  TCCR1A = bit(WGM11);

  switch (outputDevice)
  {
    case PIDOUTPUT_Default:
    case PIDOUTPUT_Fan:
      _outputDevice = PIDOUTPUT_Fan;
      // 64 prescaler
      TCCR1B = bit(WGM13) | bit(WGM12) | bit(CS11) | bit(CS10);
      // 511 TOP to be close to original 488Hz and allows OCR1B to be set to
      // 512 to prevent COMB_vect from firing (which means no LOW when at 100%)
      ICR1 = 511;
      break;
    case PIDOUTPUT_Servo:
      _outputDevice = PIDOUTPUT_Servo;
      // 8 prescaler gives us down to 30Hz with 0.5usec resolution
      TCCR1B = bit(WGM13) | bit(WGM12) | bit(CS11);
      // TOP = servo refresh
      ICR1 = uSecToTicks(SERVO_REFRESH);
      break;
  }

  // Interrupt on overflow, and OCB
  TIMSK1 = bit(OCIE1B) | bit(TOIE1);
}

boolean GrillPid::isAnyFoodProbeActive(void) const
{
  unsigned char i;
  for (i=TEMP_FOOD1; i<TEMP_COUNT; i++)
    if (Probes[i]->hasTemperature())
      return true;
      
  return false;
}
  
void GrillPid::resetLidOpenResumeCountdown(void)
{
  LidOpenResumeCountdown = _lidOpenDuration;
  _pitTemperatureReached = false;
}

void GrillPid::setSetPoint(int value)
{
  _setPoint = value;
  _pitTemperatureReached = false;
  _manualFanMode = false;
  _pidCurrent[PIDI] = 0.0f;
  LidOpenResumeCountdown = 0;
}

void GrillPid::setFanSpeed(int value)
{
  _manualFanMode = true;
  if (value < 0)
    _fanSpeed = 0;
  else if (value > 100)
    _fanSpeed = 100;
  else
    _fanSpeed = value;
}

void GrillPid::setLidOpenDuration(unsigned int value)
{
  _lidOpenDuration = (value > LIDOPEN_MIN_AUTORESUME) ? value : LIDOPEN_MIN_AUTORESUME;
}

void GrillPid::setPidConstant(unsigned char idx, float value)
{
  Pid[idx] = value;
  if (idx == PIDI)
    // Proably should scale the error sum by newval / oldval instead of resetting
    _pidCurrent[PIDI] = 0.0f;
}

void GrillPid::status(void) const
{
  SerialX.print(getSetPoint(), DEC);
  Serial_csv();

  for (unsigned char i=0; i<TEMP_COUNT; ++i)
  {
    if (Probes[i]->hasTemperature())
      SerialX.print(Probes[i]->Temperature, 1);
    else
      Serial_char('U');
    Serial_csv();
  }

  SerialX.print(getFanSpeed(), DEC);
  Serial_csv();
  SerialX.print((int)FanSpeedAvg, DEC);
  Serial_csv();
  SerialX.print(LidOpenResumeCountdown, DEC);
}

boolean GrillPid::doWork(void)
{
  unsigned long m = millis();
  
  // If this is the first invocation, force an immediate read and temperature 
  // update to display a value as soon as possible after booting
  unsigned int elapsed = m - _lastTempRead;
  if (elapsed < (TEMP_MEASURE_PERIOD / TEMP_AVG_COUNT))
    return false;
  _lastTempRead = m;

  for (unsigned char i=0; i<TEMP_COUNT; i++)
    if (Probes[i]->getProbeType() == PROBETYPE_INTERNAL)
      Probes[i]->readTemp();
  
  ++_periodCounter;
  if (_periodCounter < TEMP_AVG_COUNT)
    return false;
    
  for (unsigned char i=0; i<TEMP_COUNT; i++)
    Probes[i]->calcTemp();

  if (!_manualFanMode)
  {
    // Always calculate the fan speed
    // calFanSpeed() will bail if it isn't supposed to be in control
    calcFanSpeed();
    
    int pitTemp = (int)Probes[TEMP_PIT]->Temperature;
    if ((pitTemp >= _setPoint) &&
      (_lidOpenDuration - LidOpenResumeCountdown > LIDOPEN_MIN_AUTORESUME))
    {
      // When we first achieve temperature, reset any P sum we accumulated during startup
      // If we actually neded that sum to achieve temperature we'll rebuild it, and it
      // prevents bouncing around above the temperature when you first start up
      if (!_pitTemperatureReached)
      {
        _pitTemperatureReached = true;
        _pidCurrent[PIDI] = 0.0f;
      }
      LidOpenResumeCountdown = 0;
    }
    else if (LidOpenResumeCountdown != 0)
    {
      LidOpenResumeCountdown = LidOpenResumeCountdown - (TEMP_MEASURE_PERIOD / 1000);
    }
    // If the pit temperature has been reached
    // and if the pit temperature is [lidOpenOffset]% less that the setpoint
    // and if the fan has been running less than 90% (more than 90% would indicate probable out of fuel)
    // Note that the code assumes we're not currently counting down
    else if (_pitTemperatureReached && 
      (((_setPoint-pitTemp)*100/_setPoint) >= (int)LidOpenOffset) &&
      ((int)FanSpeedAvg < 90))
    {
      resetLidOpenResumeCountdown();
    }
  }   /* if !manualFanMode */
  commitFanSpeed();

  _periodCounter = 0;  
  return true;
}

void GrillPid::pidStatus(void) const
{
  TempProbe const* const pit = Probes[TEMP_PIT];
  if (pit->hasTemperature())
  {
    print_P(PSTR("HMPS"CSV_DELIMITER));
    for (unsigned char i=PIDB; i<=PIDD; ++i)
    {
      SerialX.print(_pidCurrent[i], 2);
      Serial_csv();
    }

    SerialX.print(pit->Temperature - pit->TemperatureAvg, 2);
    Serial_nl();
  }
}

void GrillPid::setUnits(char units)
{
  switch (units)
  {
    case 'A':
    case 'C':
    case 'F':
    case 'R':
      _units = units;
      break;
  }
}
