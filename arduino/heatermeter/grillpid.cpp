// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#include <math.h>
#include <string.h>

#include "hmcore.h"
#include "strings.h"

// The time (ms) of the measurement period
#define TEMP_MEASURE_PERIOD 2000
// The temperatures are averaged over 1, 2, 4 or 8 samples per period
#define TEMP_AVG_COUNT 8
// The minimum fan speed (%) that activates the "long pulse" mode
#define MINIMUM_FAN_SPEED 10
// 1/(Number of samples used in the exponential moving average)
#define TEMPPROBE_AVG_SMOOTH (1.0f/10.0f)
#define FANSPEED_AVG_SMOOTH (1.0f/60.0f)
// Once entering LID OPEN mode, the minimum number of seconds to stay in
// LID OPEN mode before autoresuming due to temperature returning to setpoint 
#define LIDOPEN_MIN_AUTORESUME 30

void calcExpMovingAverage(const float smooth, float *currAverage, float newValue)
{
  if (isnan(*currAverage))
    *currAverage = newValue;
  else
  {
    newValue = newValue - *currAverage;
    *currAverage = *currAverage + (smooth * newValue);
  }
}

inline void ProbeAlarm::updateStatus(int value)
{
  if (Status & HIGH_ENABLED)
  {
    if (value >= _high) 
      Status |= HIGH_RINGING;
    else
      Status &= ~(HIGH_RINGING | HIGH_SILENCED);
  }

  if (Status & LOW_ENABLED)
  {
    if (value <= _low) 
      Status |= LOW_RINGING;
    else
      Status &= ~(LOW_RINGING | LOW_SILENCED);
  }
}

void ProbeAlarm::setHigh(int value)
{
  _high = value;
  Status &= ~HIGH_MASK;
//  if (value)
//    Status |= HIGH_ENABLED;
}

void ProbeAlarm::setLow(int value)
{
  _low = value;
  Status &= ~LOW_MASK;
//  if (value)
//    Status |= LOW_ENABLED;
}

boolean ProbeAlarm::getActionNeeded(void) const
{
  return
    ((Status & HIGH_MASK) == (HIGH_ENABLED | HIGH_RINGING)) ||
    ((Status & LOW_MASK) == (LOW_ENABLED | LOW_RINGING));
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
  Alarms.setHigh(config->alarmHigh);
  Alarms.setLow(config->alarmLow);
  Alarms.Status =
    (config->alHighEnabled & ProbeAlarm::HIGH_ENABLED) |
    (config->alLowEnabled & ProbeAlarm::LOW_ENABLED);
  //Serial.print(" P=");Serial.print(_probeType,DEC);Serial.print(" O=");Serial.print(Offset,DEC);
  //Serial.print(" A=");Serial.print(Steinhart[0],8);Serial.print(" B=");Serial.print(Steinhart[1],8);
  //Serial.print(" C=");Serial.print(Steinhart[2],8);Serial.print(" R=");Serial.println(Steinhart[3],8);
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
  for (unsigned char i=0; i<OVERSAMPLE_COUNT[TEMP_OVERSAMPLE_BITS]; ++i)
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
  if (_accumulatedCount == 0)
    return; 
    
  unsigned int ADCval = _accumulator / _accumulatedCount;
  _accumulatedCount = 0;
  
  if (ADCval == 0)  // Vout >= MAX is reduced in readTemp()
  {
    Temperature = NAN;
    return;
  }
  else 
  {
    float R, T;
    // If you put the fixed resistor on the Vcc side of the thermistor, use the following
    R = log(Steinhart[3] / ((ADCmax / (float)ADCval) - 1.0f));
    // If you put the thermistor on the Vcc side of the fixed resistor use the following
    //R = log(Steinhart[3] * ADCmax / (float)Vout - Steinhart[3]);
  
    // Compute degrees K  
    T = 1.0f / ((Steinhart[2] * R * R + Steinhart[1]) * R + Steinhart[0]);
  
    Temperature = T - 273.15f;
    if (pid.getUnits() == 'F')
    {
      Temperature = (Temperature * (9.0f / 5.0f)) + 32.0f;
      // Sanity - anything less than 0F or greater than 1000F is rejected
      if (Temperature < 0.0f || Temperature >= 1000.0f)
        Temperature = NAN;
    } else if (Temperature <= -20.0f || Temperature > 500.0f)  // C
      Temperature = NAN;
    
    if (hasTemperature())
    {
      Temperature += Offset;
      calcExpMovingAverage(TEMPPROBE_AVG_SMOOTH, &TemperatureAvg, Temperature);
      Alarms.updateStatus(Temperature);
    }
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
  if (LidOpenResumeCountdown != 0)
    return;

  float error;
  error = _setPoint - currentTemp;

  // anti-windup: Make sure we only adjust the I term while
  // inside the proportional control range
  if ((error > 0 && lastFanSpeed < MaxFanSpeed) ||
      (error < 0 && lastFanSpeed > 0))
    _pidErrorSum += Pid[PIDI] * error;

  // B = fan speed percent
  // P = fan speed percent per degree of error
  // I = fan speed percent per degree of accumulated error
  // D = fan speed percent per degree of change over TEMPPROBE_AVG_SMOOTH period
  float averageTemp = Probes[TEMP_PIT]->TemperatureAvg;
  int control 
    = Pid[PIDB] + Pid[PIDP] * error + _pidErrorSum + (Pid[PIDD] * (averageTemp - currentTemp));
  
  if (control >= MaxFanSpeed)
    _fanSpeed = MaxFanSpeed;
  else if (control > 0)
    _fanSpeed = control;
}

inline void GrillPid::commitFanSpeed(void)
{
  calcExpMovingAverage(FANSPEED_AVG_SMOOTH, &FanSpeedAvg, _fanSpeed);

  /* For anything above MINIMUM_FAN_SPEED, do a nomal PWM write.
     For below MINIMUM_FAN_SPEED we use a "long pulse PWM", where 
     the pulse is 10 seconds in length.  For each percent we are 
     emulating, run the fan for one interval. */
  if (_fanSpeed >= MINIMUM_FAN_SPEED)
  {
    analogWrite(_blowerPin, _fanSpeed * 255 / 100);
    _longPwmTmr = 0;
  }
  else
  {
    // Simple PWM, ON for first [FanSpeed] intervals then OFF 
    // for the remainder of the period
    unsigned char pwmVal;
    pwmVal = ((_fanSpeed / (TEMP_MEASURE_PERIOD / 1000)) > _longPwmTmr) ? 255/MINIMUM_FAN_SPEED : 0;
    
    analogWrite(_blowerPin, pwmVal);
    // Long PWM period is 10 sec
    if (++_longPwmTmr > ((10000 / TEMP_MEASURE_PERIOD) - 1))
      _longPwmTmr = 0;
  }  /* long PWM */
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
  Serial.print(getSetPoint(), DEC);
  Serial_csv();

  for (unsigned char i=0; i<TEMP_COUNT; ++i)
  {
    if (Probes[i]->hasTemperature())
      Serial.print(Probes[i]->Temperature, 1);
    else
      Serial_char('U');
    Serial_csv();
  }

  Serial.print(getFanSpeed(), DEC);
  Serial_csv();
  Serial.print((int)FanSpeedAvg, DEC);
  Serial_csv();
  Serial.print(LidOpenResumeCountdown, DEC);
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
  print_P(PSTR("$HMPS"CSV_DELIMITER));
  Serial.print(_pidErrorSum, 2);
  Serial_csv();
  Serial.print(MaxFanSpeed, DEC);
  Serial_csv();
  Serial.print(Probes[TEMP_PIT]->TemperatureAvg - Probes[TEMP_PIT]->Temperature, 2);
  Serial_nl();
}

