// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#include <math.h>
#include <string.h>

#include "hmcore.h"
#include "strings.h"

// The time (ms) of the measurement period
#define TEMP_MEASURE_PERIOD 1000
// The temperatures are averaged over 1, 2, 4 or 8 samples per period
#define TEMP_AVG_COUNT 8
// 1/(Number of samples used in the exponential moving average)
#define TEMPPROBE_AVG_SMOOTH (1.0f/20.0f)
#define FANSPEED_AVG_SMOOTH (1.0f/120.0f)
// Once entering LID OPEN mode, the minimum number of seconds to stay in
// LID OPEN mode before autoresuming due to temperature returning to setpoint 
#define LIDOPEN_MIN_AUTORESUME 30

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
  if (getLowEnabled())
  {
    if (value > (getLow() + 1))
      Armed[ALARM_IDX_LOW] = true;
    else if (Armed[ALARM_IDX_LOW] && value <= getLow())
      Ringing[ALARM_IDX_LOW] = true;
  }

  if (getHighEnabled())
  {
    if (value < (getHigh() - 1))
      Armed[ALARM_IDX_HIGH] = true;
    else if (Armed[ALARM_IDX_HIGH] && value >= getHigh())
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
}

unsigned int GrillPid::countOfType(unsigned char probeType) const
{
  unsigned char retVal = 0;
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
    if (Probes[i]->getProbeType() == probeType)
      ++retVal;
  return retVal;  
}

/* Calucluate the desired fan speed using the proportional–integral-derivative (PID) controller algorithm */
inline void GrillPid::calcFanSpeed(void)
{
  unsigned char lastFanSpeed = _fanSpeed;
  _fanSpeed = 0;

  // If the pit probe is registering 0 degrees, don't jack the fan up to MAX
  if (!Probes[TEMP_PIT]->hasTemperature())
    return;

  float currentTemp = Probes[TEMP_PIT]->Temperature;
  // If we're in lid open mode, fan should be off
  if (isLidOpen())
    return;

  float error;
  error = _setPoint - currentTemp;

  // anti-windup: Make sure we only adjust the I term while
  // inside the proportional control range
  if ((error > 0 && lastFanSpeed < _maxFanSpeed) ||
      (error < 0 && lastFanSpeed > 0))
    _pidErrorSum += Pid[PIDI] * error;

  // B = fan speed percent
  // P = fan speed percent per degree of error
  // I = fan speed percent per degree of accumulated error
  // D = fan speed percent per degree of change over TEMPPROBE_AVG_SMOOTH period
  float averageTemp = Probes[TEMP_PIT]->TemperatureAvg;
  int control 
    = Pid[PIDB] + Pid[PIDP] * error + _pidErrorSum + (Pid[PIDD] * (averageTemp - currentTemp));
  
  if (control >= _maxFanSpeed)
    _fanSpeed = _maxFanSpeed;
  else if (control > 0)
    _fanSpeed = control;
}

inline void GrillPid::commitFanSpeed(void)
{
  /* Long PWM period is 10 sec */
  const unsigned int LONG_PWM_PERIOD = 10000;
  const unsigned int PERIOD_SCALE = (LONG_PWM_PERIOD / TEMP_MEASURE_PERIOD);

  calcExpMovingAverage(FANSPEED_AVG_SMOOTH, &FanSpeedAvg, _fanSpeed);

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
    if ((PERIOD_SCALE * _fanSpeed / _minFanSpeed) > _longPwmTmr)
      output = _minFanSpeed;
    else
      output = 0;
    
    if (++_longPwmTmr > (PERIOD_SCALE - 1))
      _longPwmTmr = 0;
  }  /* long PWM */

  if (_invertPwm)
    output = _maxFanSpeed - output;
  analogWrite(_blowerPin, (unsigned int)output * 255 / 100);
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
  _pidErrorSum = 0;
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
    _pidErrorSum = 0.0f;
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
        _pidErrorSum = 0.0f;
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
  print_P(PSTR("HMPS"CSV_DELIMITER));
  SerialX.print(_pidErrorSum, 2);
  Serial_csv();
  SerialX.print(Probes[TEMP_PIT]->Temperature - Probes[TEMP_PIT]->TemperatureAvg, 2);
  Serial_nl();
}

void GrillPid::setUnits(char units)
{
  switch (units)
  {
    case 'C':
    case 'F':
    case 'R':
      _units = units;
      break;
  }
}

