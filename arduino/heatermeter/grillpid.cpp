#include <math.h>
#include <string.h>
#include "grillpid.h"

// The temperatures are averaged over 1, 2, 4 or 8 samples
#define TEMP_AVG_COUNT 8

// The minimum fan speed (%) that activates the "long pulse" mode
#define MINIMUM_FAN_SPEED 10

// 1/(Number of samples used in the exponential moving average)
#define TEMPPROBE_AVG_SMOOTH (1.0f/15.0f)
#define FANSPEED_AVG_SMOOTH (1.0f/60.0f)

void calcExpMovingAverage(const float smooth, float *currAverage, float newValue)
{
  if (*currAverage == -1.0f)
    *currAverage = newValue;
  else
  {
    newValue = newValue - *currAverage;
    *currAverage = *currAverage + (smooth * newValue);
  }
}

inline void ProbeAlarm::updateStatus(int value)
{
  if (Status & HIGH_ENABLED != 0)
    if (value >= _high) 
      Status |= HIGH_RINGING;
    else
      Status &= ~(HIGH_RINGING | HIGH_SILENCED);

  if (Status & LOW_ENABLED != 0)
    if (value <= _low) 
      Status |= LOW_RINGING;
    else
      Status &= ~(LOW_RINGING | LOW_SILENCED);
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

void TempProbe::loadConfig(struct __eeprom_probe *config)
{
  ProbeType = config->probeType;
  Offset = config->tempOffset;
  memcpy(Steinhart, config->steinhart, sizeof(Steinhart));
  Alarms.setHigh(config->alarmHigh);
  Alarms.setLow(config->alarmLow);
  Alarms.Status =
    config->alHighEnabled & ProbeAlarm::HIGH_ENABLED |
    config->alLowEnabled & ProbeAlarm::LOW_ENABLED;
}

inline void TempProbe::readTemp(unsigned char num)
{
  unsigned int analog_temp = analogRead(_pin);
  // If we get *any* analogReads that are 0 or 1023, the measurement for 
  // the entire period is invalidated, so set the _accumulator to 0
  if (analog_temp <= 0 || analog_temp >= 1023)
    _accumulator = 0;
  else if (num == 0)
    _accumulator = analog_temp;
  else if (_accumulator != 0)
    _accumulator += analog_temp;
}

inline void TempProbe::calcTemp(void)
{
  const float Vin = 1023.0f;  

  unsigned int Vout = _accumulator / TEMP_AVG_COUNT;
  
  if ((Vout == 0) || (Vout >= (unsigned int)Vin))
  {
    Temperature = 0.0f;
    return;
  }
  else 
  {
    float R, T;
    // If you put the fixed resistor on the Vcc side of the thermistor, use the following
    R = log(Steinhart[3] / ((Vin / (float)Vout) - 1.0f));
    // If you put the thermistor on the Vcc side of the fixed resistor use the following
    // R = log(Rknown * Vin / (float)Vout - Rknown);
  
    // Compute degrees K  
    T = 1.0f / ((Steinhart[2] * R * R + Steinhart[1]) * R + Steinhart[0]);
  
    // return degrees F
    Temperature = ((T - 273.15f) * (9.0f / 5.0f)) + 32.0f;
    // Sanity - anything less than 0F or greater than 999F is rejected
    if (Temperature < 0.0f || Temperature > 999.0f)
      Temperature = 0.0f;
    
    if (Temperature != 0.0f)
    {
      Temperature += Offset;
      calcExpMovingAverage(TEMPPROBE_AVG_SMOOTH, &TemperatureAvg, Temperature);
      Alarms.updateStatus(Temperature);
    }
  } 
}

/* Calucluate the desired fan speed using the proportional–integral-derivative (PID) controller algorithm */
inline void GrillPid::calcFanSpeed(TempProbe *controlProbe)
{
  unsigned char lastFanSpeed = _fanSpeed;
  _fanSpeed = 0;
   
  float currentTemp = controlProbe->Temperature;
  // If the pit probe is registering 0 degrees, don't jack the fan up to MAX
  if (currentTemp == 0.0f)
    return;
  // If we're in lid open mode, fan should be off
  if (LidOpenResumeCountdown != 0)
    return;

  float error;
  error = _setPoint - currentTemp;

  // anti-windup: Make sure we only adjust the I term while
  // inside the proportional control range
  if ((error > 0 && lastFanSpeed < MaxFanSpeed) ||
      (error < 0 && lastFanSpeed > 0))
    _pidErrorSum += (error * Pid[PIDI]);

  // the B and P terms are in 0-100 scale, but the I and D terms are dependent on degrees    
  float averageTemp = controlProbe->TemperatureAvg;
  int control 
    = Pid[PIDB] + Pid[PIDP] * (error + _pidErrorSum - (Pid[PIDD] * (currentTemp - averageTemp)));
  
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
    pwmVal = (_fanSpeed > _longPwmTmr) ? 255/MINIMUM_FAN_SPEED : 0;
    
    analogWrite(_blowerPin, pwmVal);
    // Long PWM period is 10 sec
    if (++_longPwmTmr > 4)
      _longPwmTmr = 0;
  }  /* long PWM */
}

boolean GrillPid::isAnyFoodProbeActive(void) const
{
  unsigned char i;
  for (i=TEMP_FOOD1; i<TEMP_COUNT; i++)
    if (Probes[i]->Temperature != 0.0f)
      return true;
      
  return false;
}
  
void GrillPid::resetLidOpenResumeCountdown(void)
{
  LidOpenResumeCountdown = LidOpenDuration;
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

boolean GrillPid::doWork(void)
{
  unsigned long m = millis();
  if ((m - _lastTempRead) < (2000 / TEMP_AVG_COUNT))
    return false;
  _lastTempRead = m;

  unsigned char i;
  for (i=0; i<TEMP_COUNT; i++)
    if (Probes[1]->ProbeType == PROBETYPE_INTERNAL)
      Probes[i]->readTemp(_accumulatedCount);

  if (++_accumulatedCount < TEMP_AVG_COUNT)
    return false;
    
  for (i=0; i<TEMP_COUNT; i++)
    Probes[i]->calcTemp();

  if (!_manualFanMode)
  {
    // Always calculate the fan speed
    // calFanSpeed() will bail if it isn't supposed to be in control
    TempProbe *probePit = Probes[TEMP_PIT];
    calcFanSpeed(probePit);
    
    int pitTemp = probePit->Temperature;
    if (pitTemp >= _setPoint)
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
      LidOpenResumeCountdown = LidOpenResumeCountdown - 2;
    }
    // If the pit temperature has been reached
    // and if the pit temperature is [lidOpenOffset]% less that the setpoint
    // and if the fan has been running less than 90% (more than 90% would indicate probable out of fuel)
    // Note that the code assumes g_LidOpenResumeCountdown <= 0 and pitTemp < _setPoint
    else if (_pitTemperatureReached && 
      (((_setPoint-pitTemp)*100/_setPoint) >= (int)LidOpenOffset) &&
      ((int)FanSpeedAvg < 90))
    {
      resetLidOpenResumeCountdown();
    }
  }   /* if !manualFanMode */
  commitFanSpeed();
  
  _accumulatedCount = 0;
  return true;
}


