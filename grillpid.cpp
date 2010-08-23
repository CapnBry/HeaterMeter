#include <math.h>
//#include "WProgram.h"
#include "WConstants.h"
#include "grillpid.h"

// The temperatures are averaged over 1, 2, 4 or 8 samples
// Set this define to log2(samples) to adjust the number of samples
#define TEMP_AVG_COUNT_LOG2 3

void calcMovingAverage(const float period, float *currAverage, float newValue)
{
  if (*currAverage == -1.0f)
    *currAverage = newValue;
  else
    *currAverage = (1.0f - (1.0f / (float)period)) * *currAverage +
      (1.0f / (float)period) * newValue;
}
      
void TempProbe::readTemp(void)
{
  unsigned int analog_temp = analogRead(_pin);
  _accumulator += analog_temp;
}

void TempProbe::calcTemp(void)
{
  const float Rknown = 22000.0f;
  const float Vin = 1023.0f;  

  unsigned int Vout = _accumulator >> TEMP_AVG_COUNT_LOG2;
  _accumulator = 0; 
  
  if ((Vout == 0) || (Vout >= (unsigned int)Vin))
  {
    Temperature = 0;
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
    T = (1.0f / ((_steinhart->C * R * R + _steinhart->B) * R + _steinhart->A));
  
    // return degrees F
    Temperature = (int)((T - 273.15f) * (9.0f / 5.0f)) + 32;
    // Sanity - anything less than 0F or greater than 999F is rejected
    if (Temperature < 0 || Temperature > 999)
      Temperature = 0;
    
    if (Temperature != 0)
    {
      Temperature += Offset;
      calcMovingAverage(TEMPPROBE_AVG_PERIOD, &TemperatureAvg, Temperature);
    }
  } 
}

/* Calucluate the desired fan speed using the proportional–integral-derivative (PID) controller algorithm */
void GrillPid::calcFanSpeed(TempProbe *controlProbe)
{
  int currentTemp = controlProbe->Temperature;
  // If the pit probe is registering 0 degrees, don't jack the fan up to MAX
  if (currentTemp == 0)
    return;

  float error;
  int control;
  error = SetPoint - currentTemp;

  // anti-windup: Make sure we only adjust the I term while
  // inside the proportional control range
  // Note that I still allow the errorSum to degrade within 1 degrees even if 
  // the fan is off because it is much easier for the sum to increase than
  // decrease due to the fan generally being at 0 once it passes the SetPoint
  if (!(FanSpeed >= 100 && error > 0) && 
      !(FanSpeed <= 0   && error < -1.0f))
    _pidErrorSum += (error * PidI);
    
  float averageTemp = controlProbe->TemperatureAvg;
  control = PidP * (error + _pidErrorSum - (PidD * (currentTemp - averageTemp)));
  
  if (control > 100)
    FanSpeed = 100;
  else if (control < 0)
    FanSpeed = 0;
  else
    FanSpeed = control;
}

void GrillPid::resetLidOpenResumeCountdown(void)
{
  LidOpenResumeCountdown = LidOpenDuration;
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
    
  ++_accumulatedCount;
  if (_accumulatedCount < (1 << TEMP_AVG_COUNT_LOG2))
    return false;
    
  for (i=0; i<TEMP_COUNT; i++)
    Probes[i]->calcTemp();

  calcFanSpeed(Probes[TEMP_PIT]);
  int pitTemp = Probes[TEMP_PIT]->Temperature;
  if (pitTemp >= SetPoint)
  {
    _pitTemperatureReached = true;
    LidOpenResumeCountdown = 0;
  }
  else if (LidOpenResumeCountdown != 0)
  {
    --LidOpenResumeCountdown;
    FanSpeed = 0;
  }
  // If the pit temperature dropped has more than [lidOpenOffset] degrees 
  // after reaching temp, and the fan has not been running more than 90% of 
  // the average period. note that the code assumes g_LidOpenResumeCountdown <= 0
  else if (_pitTemperatureReached && ((SetPoint - pitTemp) > (int)LidOpenOffset) && FanSpeedAvg < 90.0f)
  {
    resetLidOpenResumeCountdown();
    _pitTemperatureReached = false;
  }
  calcMovingAverage(FANSPEED_AVG_PERIOD, &FanSpeedAvg, FanSpeed);
  analogWrite(_blowerPin, (FanSpeed * 255 / 100));
  
  _accumulatedCount = 0;
  return true;
}


