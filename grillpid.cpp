#include <math.h>
#include <wiring.h>
#include "grillpid.h"

// The temperatures are averaged over 1, 2, 4 or 8 samples
// Set this define to log2(samples) to adjust the number of samples
#define TEMP_AVG_COUNT_LOG2 3

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
  float currentTemp = controlProbe->Temperature;
  // If the pit probe is registering 0 degrees, don't jack the fan up to MAX
  if (currentTemp == 0.0f)
    return;

  float error;
  float control;
  error = SetPoint - currentTemp;

  // anti-windup: Make sure we only adjust the I term while
  // inside the proportional control range
  // Note that I still allow the errorSum to degrade within N degrees even if 
  // the fan is off because it is much easier for the sum to increase than
  // decrease due to the fan generally being at 0 once it passes the SetPoint
  if (!(_fanSpeedPwm >= 255 && error > 0) && 
      !(_fanSpeedPwm <= 0   && error < -1.0f))
    _pidErrorSum += (error * Pid[PIDI]);

  // the B and P terms are in 0-255 scale, but the I and D terms are dependent on degrees    
  float averageTemp = controlProbe->TemperatureAvg;
  control = Pid[PIDB] + Pid[PIDP] * (error + _pidErrorSum - (Pid[PIDD] * (currentTemp - averageTemp)));
  
  if (control >= 255.0f)
    _fanSpeedPwm = 255;
  else if (control <= 0.0f)
    _fanSpeedPwm = 0;
  else
    _fanSpeedPwm = control;

  FanSpeed = _fanSpeedPwm * 100 / 255;
}

void GrillPid::resetLidOpenResumeCountdown(void)
{
  LidOpenResumeCountdown = LidOpenDuration;
}

void GrillPid::commitFanSpeed(void)
{
  calcExpMovingAverage(FANSPEED_AVG_SMOOTH, &FanSpeedAvg, FanSpeed);

  /* 10% or greater fan speed, we do a normal PWM write.
     For below 10% we use a "long pulse PWM", where the pulse is 
     10 seconds in length.  For each percent we are emulating, run
     the fan for one interval.
  */
  if (FanSpeed >= 10)
  {
    analogWrite(_blowerPin, _fanSpeedPwm);
    _longPwmTmr = 0;
  }
  else
  {
    // Simple PWM, ON for first [FanSpeed] seconds then OFF 
    // for the remainder of the period
    unsigned char pwmVal;
    pwmVal = (FanSpeed > _longPwmTmr) ? 255/10 : 0; // 10% or 0%
    
    analogWrite(_blowerPin, pwmVal);
    if (++_longPwmTmr > 9)
      _longPwmTmr = 0;
  }  /* long PWM */
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

  // Always feed the PID loop even if the lid detect is active.  It may
  // mess with the error sum but we end up tracking better when control resumes
  calcFanSpeed(Probes[TEMP_PIT]);
  int pitTemp = Probes[TEMP_PIT]->Temperature;
  if (pitTemp == 0)
  {
    FanSpeed = 0;
    _fanSpeedPwm = 0;
  }
  else if (pitTemp >= SetPoint)
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
    FanSpeed = 0;
    _fanSpeedPwm = 0;
  }
  // If the pit temperature dropped has more than [lidOpenOffset] degrees 
  // after reaching temp, and the fan has not been running more than 90% of 
  // the average period. note that the code assumes g_LidOpenResumeCountdown <= 0
  else if (_pitTemperatureReached && ((SetPoint - pitTemp) > (int)LidOpenOffset) && FanSpeedAvg < 90.0f)
  {
    resetLidOpenResumeCountdown();
    _pitTemperatureReached = false;
  }
  commitFanSpeed();
  
  _accumulatedCount = 0;
  return true;
}


