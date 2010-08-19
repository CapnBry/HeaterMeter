#ifndef __GRILLPID_H__
#define __GRILLPID_H__

#define TEMP_PIT    0
#define TEMP_FOOD1  1
#define TEMP_FOOD2  2
#define TEMP_AMB    3
#define TEMP_COUNT  4

struct steinhart_param
{ 
    float A, B, C; 
};

#define TEMPPROBE_AVG_PERIOD 30 // seconds
#define FANSPEED_AVG_PERIOD 120 // seconds

class TempProbe
{
private:
  const struct steinhart_param *_steinhart;
  const unsigned char _pin; 
  
  unsigned int _accumulator;
public:
  TempProbe(const unsigned char pin, const struct steinhart_param *steinhart) : 
    _pin(pin), _steinhart(steinhart), Temperature(0), Offset(0),
    TemperatureAvg(0.0f) {};
  
  // Last averaged temperature reading
  int Temperature;
  // Temperature moving average 
  float TemperatureAvg;
  // Offset (in degrees) applied when calculating temperature
  char Offset;
  // Do the duty of reading ADC
  void readTemp(void);
  // Convert ADC to Temperature
  void calcTemp(void);
};

class GrillPid
{
private:
  const unsigned char _blowerPin;
  unsigned long _lastTempRead;
  unsigned char _accumulatedCount;
  boolean _pitTemperatureReached;
  float _pidErrorSum;
  
  void calcFanSpeed(TempProbe *controlProbe);
public:
  GrillPid(const unsigned char blowerPin) : 
    _blowerPin(blowerPin), _lastTempRead(0), _accumulatedCount(0), 
    _pitTemperatureReached(false), FanSpeed(0), FanSpeedAvg(0.0f)
    {};
  
  TempProbe *Probes[TEMP_COUNT];
  
  /* Configuration */
  int SetPoint;
  // The number of degrees the temperature drops before automatic lidopen mode
  unsigned char LidOpenOffset;
  // The ammount of time to turn off the blower when the lid is open 
  unsigned int LidOpenDuration;
  // The PID constants
  float PidP;
  float PidI;
  float PidD;
  
  /* Runtime Data */
  // Current fan speed in percent
  unsigned char FanSpeed;
  // Fan speed moving average
  float FanSpeedAvg;
  // Seconds remaining in the lid open countdown
  unsigned int LidOpenResumeCountdown;
  
  // Call this in loop()
  boolean doWork(void);
  void resetLidOpenResumeCountdown(void);
};

#endif /* __GRILLPID_H__ */
