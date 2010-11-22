#include <math.h>
#include <wiring.h>
#include "grillpid.h"

// The temperatures are averaged over 1, 2, 4 or 8 samples
// Set this define to log2(samples) to adjust the number of samples
#define TEMP_AVG_COUNT_LOG2 3

// The minimum fan speed (%) that activates the "long pulse" mode
#define MINIMUM_FAN_SPEED 10

#define TEMPPROBE_AVG_SMOOTH (1.0f/30.0f)
#define FANSPEED_AVG_SMOOTH (1.0f/120.0f)

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
      
inline void TempProbe::readTemp(void)
{
  unsigned int analog_temp = analogRead(_pin);
  _accumulator += analog_temp;
}

inline void TempProbe::calcTemp(void)
{
  const float Rknown = 22000.0f;
  const float Vin = 1023.0f;  

  unsigned int Vout = _accumulator >> TEMP_AVG_COUNT_LOG2;
  _accumulator = 0; 
  
  if ((Vout == 0) || (Vout >= (unsigned int)Vin))
  {
    Temperature = 0.0f;
    return;
  }
  else 
  {
    float R, T;
    // If you put the fixed resistor on the Vcc side of the thermistor, use the following
    R = log(Rknown / ((Vin / (float)Vout) - 1.0f));
    // If you put the thermistor on the Vcc side of the fixed resistor use the following
    // R = log(Rknown * Vin / (float)Vout - Rknown);
  
    // Compute degrees K  
    T = 1.0f / ((_steinhart->C * R * R + _steinhart->B) * R + _steinhart->A);
  
    // return degrees F
    Temperature = ((T - 273.15f) * (9.0f / 5.0f)) + 32.0f;
    // Sanity - anything less than 0F or greater than 999F is rejected
    if (Temperature < 0.0f || Temperature > 999.0f)
      Temperature = 0.0f;
    
    if (Temperature != 0.0f)
    {
      Temperature += Offset;
      calcExpMovingAverage(TEMPPROBE_AVG_SMOOTH, &TemperatureAvg, Temperature);
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

void GrillPid::commitFanSpeed(void)
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
    // Long PWM period is 10 intervals
    if (++_longPwmTmr > 9)
      _longPwmTmr = 0;
  }  /* long PWM */
}

boolean GrillPid::isAnyFoodProbeActive(void)
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
  if ((m - _lastTempRead) < (1000 >> TEMP_AVG_COUNT_LOG2))
    return false;
  _lastTempRead = m;

  unsigned char i;
  for (i=0; i<TEMP_COUNT; i++)
    Probes[i]->readTemp();
    
  if (++_accumulatedCount < (1 << TEMP_AVG_COUNT_LOG2))
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
      --LidOpenResumeCountdown;
    }
    // If the pit temperature dropped has more than [lidOpenOffset] degrees 
    // after reaching temp, and the fan has not been running more than 90% of 
    // the average period. note that the code assumes g_LidOpenResumeCountdown <= 0
    else if (_pitTemperatureReached && ((_setPoint - pitTemp) > (int)LidOpenOffset) && FanSpeedAvg < 90.0f)
    {
      resetLidOpenResumeCountdown();
    }
  }   /* if !manualFanMode */
  commitFanSpeed();
  
  _accumulatedCount = 0;
  return true;
}


