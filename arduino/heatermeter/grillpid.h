// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net>
#ifndef __GRILLPID_H__
#define __GRILLPID_H__

#include "Arduino.h"
#include "grillpid_conf.h"

// Probe types used in probeType config
#define PROBETYPE_DISABLED 0  // do not read
#define PROBETYPE_INTERNAL 1  // read via analogRead()
#define PROBETYPE_RF12     2  // RFM12B wireless
#define PROBETYPE_TC_ANALOG  3  // Analog thermocouple, Stein[3] is mV/C

#define STEINHART_COUNT 4

//#define NOISEDUMP_PIN 5

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

#define TSTATUS_NONE  'U'
#define TSTATUS_HIGH  'H'
#define TSTATUS_LOW   'L'
#define TSTATUS_OK    'O'

class TempProbe
{
private:
  const unsigned char _pin; 
  unsigned char _probeType;
  char _tempStatus;
  boolean _hasTempAvg;
  
public:
  TempProbe(const unsigned char pin);

  const unsigned char getPin(void) const { return _pin; }

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

  /* Runtime Data/Methods */
  // Last averaged temperature reading
  float Temperature;
  boolean hasTemperature(void) const { return _tempStatus == TSTATUS_OK; }
  char getTempStatus(void) const { return _tempStatus; }
  void setTemperatureC(float T);
  // Temperature moving average 
  float TemperatureAvg;
  boolean hasTemperatureAvg(void) const { return _hasTempAvg; }
  // Convert ADC to Temperature
  void calcTemp(unsigned int _accumulator);
  // Perform once-per-period processing
  void processPeriod(void);
  
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
// Try to output a constant voltage instead of PWM
#define PIDFLAG_FAN_FEEDVOLT  4
// Line noise cancel is 2 bits, 00=Normal, 01=50Hz, 10=60Hz, 11=Unused
#define PIDFLAG_LINECANCEL_50 5
#define PIDFLAG_LINECANCEL_60 6

// pitStartRecover constants
// STARTUP - Attempting to reach temperature for the first time
// after a setpoint change
#define PIDSTARTRECOVER_STARTUP  0
// RECOVERY - Is attempting to return to temperature after a lid event
#define PIDSTARTRECOVER_RECOVERY 1
// NORMAL - Setpoint has been attained, normal operation
#define PIDSTARTRECOVER_NORMAL   2

// oversampled analogRead from current freerunning ADC
unsigned int analogReadOver(unsigned char pin, unsigned char bits);
// Range of the last ADC period for this pin, always 10bit
unsigned int analogReadRange(unsigned char pin);
#if defined(GRILLPID_DYNAMIC_RANGE)
// Is the pin using the 1.1V reference
bool analogIsBandgapReference(unsigned char pin);
void analogSetBandgapReference(unsigned char pin, bool enable);
#endif /* GRILLPID_DYNAMIC_RANGE */

class GrillPid
{
private:
  unsigned char _pidOutput;
  unsigned long _lastWorkMillis;
  unsigned char _pitStartRecover;
  int _setPoint;
  boolean _manualOutputMode;
  unsigned char _periodCounter;
  // Counter used for "long PWM" mode
  unsigned char _longPwmTmr;
  unsigned int _lidOpenDuration;
  // Last values used in PID calculation = B + P + I + D (B is always 0)
  float _pidCurrent[4];
  char _units;
  unsigned char _fanMaxSpeed;
  unsigned char _fanMaxStartupSpeed;
  unsigned char _fanMinSpeed;
  unsigned char _fanActiveFloor;
  unsigned char _servoMaxPos;
  unsigned char _servoMinPos;
  unsigned char _outputFlags;

  // Current fan speed (percent)
  unsigned char _fanSpeed;
  // Feedback switching mode voltage controller
  unsigned char _feedvoltLastOutput;
  // Desired fan target (0-255)
  unsigned char _lastBlowerOutput;
  // Target servo position (ticks)
  int _servoTarget;
  int _servoStep;
  // count of periods a servo write has been delayed
  unsigned char _servoHoldoff;
  
  void calcPidOutput(void);
  void commitFanOutput(void);
  void commitServoOutput(void);
  void commitPidOutput(void);
  void adjustFeedbackVoltage(void);
public:
  GrillPid(void) : _periodCounter(0x80), _units('F') {};
  void init(void);

  TempProbe *Probes[TEMP_COUNT+1];

  /* Configuration */
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
  void setProbeType(unsigned char idx, unsigned char probeType);
  void updateControlProbe(void);

  // Fan Speed
  // The maximum fan speed percent that will be used in automatic mode
  unsigned char getFanMaxSpeed(void) const { return _fanMaxSpeed; }
  void setFanMaxSpeed(unsigned char value) { _fanMaxSpeed = constrain(value, 0, 100); }
  unsigned char getFanMaxStartupSpeed(void) const { return _fanMaxStartupSpeed; }
  void setFanMaxStartupSpeed(unsigned char value) { _fanMaxStartupSpeed = constrain(value, 0, 100); }
  // Active floor means "fan on above this PID output". Must be < 100!
  unsigned char getFanActiveFloor(void) const { return _fanActiveFloor; }
  void setFanActiveFloor(unsigned char value) { _fanActiveFloor = (value < 100) ? value : 0; }
  // The minimum fan speed percent before converting to "long PID" (SRTP) mode
  unsigned char getFanMinSpeed(void) const { return _fanMinSpeed; }
  void setFanMinSpeed(unsigned char value) { _fanMinSpeed = constrain(value, 0, 100); }

  // Servo timing
  // The duration (in 10x usec) for the maxium servo position
  unsigned char getServoMaxPos(void) const { return _servoMaxPos; }
  void setServoMaxPos(unsigned char value) { _servoMaxPos = value; }
  // The duration (in 10x usec) for the minimum servo position
  unsigned char getServoMinPos(void) const { return _servoMinPos; }
  void setServoMinPos(unsigned char value) { _servoMinPos = value; }
  // The number of timer ticks the servo is moving to
  int getServoTarget(void) const { return _servoTarget; }
  // Step size moving toward the target
  int getServoStep(void) const { return _servoStep; }

  // Collection of PIDFLAG_*
  void setOutputFlags(unsigned char value);
  unsigned char getOutputFlags(void) const { return _outputFlags; }
  
  /* Runtime Data */
  // Current PID output in percent, setting this will turn on manual output mode
  unsigned char getPidOutput() const { return _pidOutput; }
  void setPidOutput(int value);
  // Current fan speed output in percent
  unsigned char getFanSpeed(void) const { return _fanSpeed; };
  unsigned long getLastWorkMillis(void) const { return _lastWorkMillis; }

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
  boolean isPitTempReached(void) const { return _pitStartRecover == PIDSTARTRECOVER_NORMAL; }
  unsigned char getPitStartRecover(void) const { return _pitStartRecover; }
  
  // Call this in loop()
  boolean doWork(void);
  void resetLidOpenResumeCountdown(void);
  void status(void) const;
  void pidStatus(void) const;
};

#endif /* __GRILLPID_H__ */
