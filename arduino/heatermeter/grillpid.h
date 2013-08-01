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

// Indexes into outputFlags bitfield
// Invert the fan PWM - pidOutput=100 would generate no PWM pulses
#define PIDFLAG_INVERT_FAN    0
// Invert servo direction - scale from max to min rather than min to max
#define PIDFLAG_INVERT_SERVO  1
// Fan only runs (at max) when pidOutput=100 (max)
#define PIDFLAG_FAN_ONLY_MAX  2
// Servo opens (to max) when pidOutput>0 (any output)
#define PIDFLAG_SERVO_ANY_MAX 3

class GrillPid
{
private:
  unsigned char const _fanPin;
  unsigned char const _servoPin;

  unsigned char _pidOutput;
  unsigned long _lastTempRead;
  boolean _pitTemperatureReached;
  int _setPoint;
  boolean _manualOutputMode;
  unsigned char _periodCounter;
  // Counter used for "long PWM" mode
  unsigned char _longPwmTmr;
  unsigned int _lidOpenDuration;
  // Last values used in PID calculation = B + P + I + D;
  float _pidCurrent[4];
  unsigned int _servoOutput;
  char _units;
  unsigned char _maxFanSpeed;
  unsigned char _minFanSpeed;
  unsigned char _maxServoPos;
  unsigned char _minServoPos;

  unsigned char _outputFlags;
  
  void calcPidOutput(void);
  void commitFanOutput(void);
  void commitServoOutput(void);
  void commitPidOutput(void);
public:
  GrillPid(unsigned char const fanPin, unsigned char const servoPin);
  void init(void) const;

  TempProbe *Probes[TEMP_COUNT];

  /* Configuration */
  unsigned char const getFanPin(void) const { return _fanPin; }
  unsigned char const getServoPin(void) const { return _servoPin; }
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

  // Fan Speed
  // The maximum fan speed percent that will be used in automatic mode
  unsigned char getMaxFanSpeed(void) const { return _maxFanSpeed; }
  void setMaxFanSpeed(unsigned char value) { _maxFanSpeed = value; }
  // The minimum fan speed percent before converting to "long PID" (SRTP) mode
  unsigned char getMinFanSpeed(void) const { return _minFanSpeed; }
  void setMinFanSpeed(unsigned char value) { _minFanSpeed = value; }

  // Servo timing
  // The duration (in 10x usec) for the maxium servo position
  unsigned char getMaxServoPos(void) const { return _maxServoPos; }
  void setMaxServoPos(unsigned char value) { _maxServoPos = value; }
  // The duration (in 10x usec) for the minimum servo position
  unsigned char getMinServoPos(void) const { return _minServoPos; }
  void setMinServoPos(unsigned char value) { _minServoPos = value; }

  // Collection of PIDFLAG_*
  void setOutputFlags(unsigned char value) { _outputFlags = value; }
  unsigned char getOutputFlags(void) const { return _outputFlags; }
  
  /* Runtime Data */
  // Current PID output in percent, setting this will turn on manual output mode
  unsigned char getPidOutput() const { return _pidOutput; }
  void setPidOutput(int value);
  // Current fan speed output in percent
  unsigned char getFanSpeed(void) const;
  // Current servo output in TIMER1 ticks
  unsigned int getServoOutput(void) const { return _servoOutput; }

  boolean getManualOutputMode(void) const { return _manualOutputMode; }
  // PID output moving average
  float PidOutputAvg;
  // Seconds remaining in the lid open countdown
  unsigned int LidOpenResumeCountdown;
  boolean isLidOpen(void) const { return LidOpenResumeCountdown != 0; }
  // true if any probe has a non-zero temperature
  boolean isAnyFoodProbeActive(void) const;
  unsigned int countOfType(unsigned char probeType) const;
  // true if PidOutput > 0
  boolean isOutputActive(void) const { return _pidOutput != 0; }
  // true if fan is running at maximum speed or servo wide open
  boolean isOutputMaxed(void) const { return _pidOutput >= 100; }
  // true if temperature was >= setpoint since last set / lid event
  boolean isPitTempReached(void) const { return _pitTemperatureReached; }
  
  // Call this in loop()
  boolean doWork(void);
  void resetLidOpenResumeCountdown(void);
  void status(void) const;
  void pidStatus(void) const;
};

#endif /* __GRILLPID_H__ */
