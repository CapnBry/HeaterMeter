// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#ifndef __GRILLPID_H__
#define __GRILLPID_H__

#include "Arduino.h"

#define TEMP_PIT    0
#define TEMP_FOOD1  1
#define TEMP_FOOD2  2
#define TEMP_AMB    3
#define TEMP_COUNT  4

#define PROBE_NAME_SIZE 13

// Probe types used in probeType config
#define PROBETYPE_DISABLED 0  // do not read
#define PROBETYPE_INTERNAL 1  // read via analogRead()
#define PROBETYPE_RF12     2  // RFM12B wireless

#define STEINHART_COUNT 4

// Use oversample/decimation to increase ADC resolution to 2^(10+n) bits n=[0..3]
#define TEMP_OVERSAMPLE_BITS 3

struct __eeprom_probe
{
  char name[PROBE_NAME_SIZE];
  unsigned char probeType;
  char tempOffset;
  int alarmLow;
  int alarmHigh;
  char unused1;
  char unused2;
  float steinhart[STEINHART_COUNT];  // The last one is actually Rknown
};

#define ALARM_IDX_LOW  0
#define ALARM_IDX_HIGH 1
#define ALARM_ID_TO_PROBE(id) (id / 2)
#define ALARM_ID_TO_IDX(id) (id % 2)
#define MAKE_ALARM_ID(probe, idx) (probe * 2 + idx)

class ProbeAlarm
{
public:
  ProbeAlarm(void) {};

  // Check value against Low/High
  void updateStatus(int value);
  void setLow(int value);
  void setHigh(int value);
  int getLow(void) const { return Thresholds[ALARM_IDX_LOW]; }
  int getHigh(void) const { return Thresholds[ALARM_IDX_HIGH]; }
  boolean getLowEnabled(void) const { return Thresholds[ALARM_IDX_LOW] > 0; }
  boolean getHighEnabled(void) const { return Thresholds[ALARM_IDX_HIGH] > 0; }
  boolean getLowRinging(void) const { return Ringing[ALARM_IDX_LOW]; }
  boolean getHighRinging(void) const { return Ringing[ALARM_IDX_HIGH]; }
  void setThreshold(unsigned char idx, int value);
  int getThreshold(unsigned char idx) const { return Thresholds[idx]; }
  void silenceAll(void) { setThreshold(ALARM_IDX_LOW, 0); setThreshold(ALARM_IDX_HIGH, 0); }
  int Thresholds[2];
  boolean Ringing[2];
  boolean Armed[2];
};

class TempProbe
{
private:
  const unsigned char _pin; 
  unsigned char _accumulatedCount;
  unsigned int _accumulator;
  unsigned char _probeType;  
  
public:
  TempProbe(const unsigned char pin);

  /* Configuration */  
  // Probe Type
  unsigned char getProbeType(void) const { return _probeType; }
  void setProbeType(unsigned char probeType);
  // Offset (in degrees) applied when calculating temperature
  char Offset;
  // Steinhart coefficients
  float Steinhart[STEINHART_COUNT];
  // Copy struct to members
  void loadConfig(struct __eeprom_probe *config);
  // Takes a Temperarure ADC value and adds it to the accumulator
  void addAdcValue(unsigned int analog_temp);

  /* Runtime Data/Methods */
  // Last averaged temperature reading
  float Temperature;
  boolean hasTemperature(void) const { return !isnan(Temperature); }
  void setTemperatureC(float T);
  // Temperature moving average 
  float TemperatureAvg;
  boolean hasTemperatureAvg(void) const { return !isnan(TemperatureAvg); }
  // Do the duty of reading ADC
  void readTemp(void);
  // Convert ADC to Temperature
  void calcTemp(void);
  
  ProbeAlarm Alarms;
};

// Indexes into the pid array
#define PIDB 0
#define PIDP 1
#define PIDI 2
#define PIDD 3

struct GrillPidOutput
{
  typedef enum {
    Default = 0xff,
    Fan = 0,
    Servo = 1
  } Type;
};

class GrillPid
{
private:
  const unsigned char _blowerPin;
  unsigned char _fanSpeed;
  unsigned long _lastTempRead;
  boolean _pitTemperatureReached;
  int _setPoint;
  boolean _manualFanMode;
  unsigned char _periodCounter;
  // Counter used for "long PWM" mode
  unsigned char _longPwmTmr;
  unsigned int _lidOpenDuration;
  // Last values used in PID calculation = B + P + I + D;
  float _pidCurrent[4];
  char _units;
  unsigned char _maxFanSpeed;
  unsigned char _minFanSpeed;
  boolean _invertPwm;
  GrillPidOutput::Type _outputDevice;
  
  void calcFanSpeed(void);
  void commitFanSpeed(void);
public:
  GrillPid(const unsigned char blowerPin);
  
  TempProbe *Probes[TEMP_COUNT];
  
  /* Configuration */
  unsigned char getBlowerPin(void) const { return _blowerPin; }
  int getSetPoint(void) const { return _setPoint; }
  void setSetPoint(int value); 
  char getUnits(void) const { return _units; }
  void setUnits(char units);
  // The number of degrees the temperature drops before automatic lidopen mode
  unsigned char LidOpenOffset;
  // The ammount of time to turn off the blower when the lid is open 
  unsigned int getLidOpenDuration(void) const { return _lidOpenDuration; }
  void setLidOpenDuration(unsigned int value);
  // Number of effective bits of the ADC
  unsigned char getAdcBits(void) const { return 10 + TEMP_OVERSAMPLE_BITS; }
  // The PID constants
  float Pid[4];
  void setPidConstant(unsigned char idx, float value);
  // The maximum fan speed percent that will be used in automatic mode
  // In Servo output maxfanspeed is the duration of the maximum pulse in 10x uSec (240 = 2400us)
  unsigned char getMaxFanSpeed(void) const { return _maxFanSpeed; }
  void setMaxFanSpeed(unsigned char value) { _maxFanSpeed = value; }
  // The minimum fan speed percent before converting to "long PID" (SRTP) mode
  // In Servo output minfanspeed is the duration of the minumum pulse in 10x uSec (54 = 540us)
  unsigned char getMinFanSpeed(void) const { return _minFanSpeed; }
  void setMinFanSpeed(unsigned char value) { _minFanSpeed = value; }
  // Reverse the PWM output, i.e. 100% writes 0 to the output
  boolean getInvertPwm(void) const { return _invertPwm; }
  void setInvertPwm(boolean value) { _invertPwm = value; }
  // Sets the type of device connected to the output pin
  GrillPidOutput::Type getOutputDevice(void) const { return _outputDevice; }
  void setOutputDevice(GrillPidOutput::Type outputDevice);
  
  /* Runtime Data */
  // Current fan speed in percent, setting this will put the fan into manual mode
  unsigned char getFanSpeed() const { return _fanSpeed; }
  void setFanSpeed(int value);
  boolean getManualFanMode(void) const { return _manualFanMode; }
  // Fan speed moving average
  float FanSpeedAvg;
  // Seconds remaining in the lid open countdown
  unsigned int LidOpenResumeCountdown;
  boolean isLidOpen(void) const { return LidOpenResumeCountdown != 0; }
  // true if any probe has a non-zero temperature
  boolean isAnyFoodProbeActive(void) const;
  unsigned int countOfType(unsigned char probeType) const;
  // true if FanSpeed > 0
  boolean isFanRunning(void) const { return _fanSpeed != 0; }
  // true if fan is running at maximum speed
  boolean isFanMaxed(void) const { return _fanSpeed >= _maxFanSpeed; }
  // true if temperature was >= setpoint since last set / lid event
  boolean isPitTempReached(void) const { return _pitTemperatureReached; };
  
  // Call this in loop()
  boolean doWork(void);
  void resetLidOpenResumeCountdown(void);
  void status(void) const;
  void pidStatus(void) const;
};

#endif /* __GRILLPID_H__ */
