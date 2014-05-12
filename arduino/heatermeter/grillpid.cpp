// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net>
// GrillPid uses TIMER1 COMPB vector, as well as modifies the waveform
// generation mode of TIMER1. Blower output pin needs to be a hardware PWM pin.
// Fan output is 489Hz phase-correct PWM
// Servo output is 50Hz pulse duration
#include <math.h>
#include <string.h>
#include <util/atomic.h>

#include "strings.h"
#include "grillpid.h"

extern const GrillPid pid;

// For this calculation to work, ccpm()/8 must return a round number
#define uSecToTicks(x) ((unsigned int)(clockCyclesPerMicrosecond() / 8) * x)

// LERP percentage o into the unsigned range [A,B]. B - A must be < 655
#define mappct(o, a, b)  (((b - a) * (unsigned int)o / 100) + a)

#define DIFFMAX(x,y,d) ((x - y + d) <= (d*2U))

#if defined(GRILLPID_SERVO_ENABLED)
ISR(TIMER1_CAPT_vect)
{
  if (OCR1B > 0)
    digitalWrite(pid.getServoPin(), HIGH);
}

ISR(TIMER1_COMPB_vect)
{
  digitalWrite(pid.getServoPin(), LOW);
}
#endif

//#define ADC_DYNAMIC_RANGE

static struct tagAdcState
{
  unsigned char cnt;      // count in accumulator
  unsigned char limitCnt; // count of ADC reads that were min/max
  unsigned long accumulator;  // total
  unsigned char discard;  // Discard this many ADC readings
  unsigned int analogReads[6]; // Current values
#if defined(ADC_DYNAMIC_RANGE)
  unsigned char lowRange[6];   // true if was low range used for read
#endif
#if defined(NOISEDUMP_PIN)
  unsigned int data[256];
#endif
} adcState;

#if defined(NOISEDUMP_PIN)
volatile unsigned char g_NoisePin = NOISEDUMP_PIN;
#endif

ISR(ADC_vect)
{
  if (adcState.discard != 0)
  {
    --adcState.discard;
    return;
  }

  /*
  Don't take any readings that were sampled during the blower "turn on"
  time because it causes a dip in the power. The ADC scaler and the TIMER2
  scaler are both 128 so their clock counts are the same. The ADC takes
  13 clocks to measure. */
  //if (TCNT2 < 17)
  //  return;

  unsigned int adc = ADC;
#if defined(NOISEDUMP_PIN)
  if ((ADMUX & 0x07) == g_NoisePin)
    adcState.data[adcState.cnt] = adc;
#endif
  adcState.accumulator += adc;
  ++adcState.cnt;
  if (adc >= 1023)
  {
    if (adcState.limitCnt > 0x7f)
      adcState.accumulator = 1023 << 8;
    else
      ++adcState.limitCnt;
  }

  if (adcState.cnt == 0)
  {
    //unsigned int val;
    // More than half at the limit? Discard the whole thing
    //if (adcState.limitCnt > 0x7f)
    //  val = 0;
    //else
      // Store the current value with 4 bit oversample
      //val = adcState.accumulator >> 4;

    unsigned char pin = ADMUX & 0x07;
    adcState.analogReads[pin] = adcState.accumulator >> 2;
    adcState.limitCnt = 0;
    adcState.accumulator = 0;

#if defined(ADC_DYNAMIC_RANGE)
    // Select the next pin
    // Because the MUX is change while already reading, discard readings
    // to allow it to change and settle
    pin = (pin + 1) % 6;
    unsigned char reference = DEFAULT << 6;
    if (adcState.lowRange[pin])
      if (adcState.analogReads[pin] > (1000U << 6))
        adcState.lowRange[pin] = false;
      else
        reference = INTERNAL << 6;
    else // lowRange == false
    {
      if (pin ==5 && adcState.analogReads[pin] < (300U << 6))
      {
        adcState.lowRange[pin] = true;
        reference = INTERNAL << 6;
      }
    }
    
    // If switching references, allow time for AREF cap to charge
    if ((ADMUX & 0xc0) != reference)
      adcState.discard = 48;  // 48 / 9615 samples/s = 5ms
    else
      adcState.discard = 2;

    ADMUX = reference | pin;
#else
    ADMUX = (DEFAULT << 6) | ((pin + 1) % 6);
    adcState.discard = 2;
#endif
  }
}

unsigned int analogReadOver(unsigned char pin, unsigned char bits)
{
  unsigned int retVal;
  ATOMIC_BLOCK(ATOMIC_FORCEON)
  {
    retVal = adcState.analogReads[pin];
  }
  return retVal >> (16 - bits);
}

static void adcDump(void)
{
#if defined(NOISEDUMP_PIN)
  static uint8_t x;
  ++x;
  if (x == 5)
  {
    x = 0;
    ADCSRA = bit(ADEN) | bit(ADATE) | bit(ADPS2) | bit(ADPS1) | bit (ADPS0);
    SerialX.print("HMLG,NOISE ");
    for (unsigned int i=0; i<256; ++i)
    {
      SerialX.print(adcState.data[i], DEC);
      SerialX.print(' ');
    }
    Serial_nl();
    ADCSRA = bit(ADEN) | bit(ADATE) | bit(ADIE) | bit(ADPS2) | bit(ADPS1) | bit (ADPS0);
    ADCSRA |= bit(ADSC);
  }
#endif
}

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
  // Low: Arming point >= Thresh + 1.0f, Trigger point < Thresh
  // A low alarm set for 100 enables at 101.0 and goes off at 99.9999...
  if (getLowEnabled())
  {
    if (value >= (getLow() + 1))
      Armed[ALARM_IDX_LOW] = true;
    else if (value < getLow() && Armed[ALARM_IDX_LOW])
      Ringing[ALARM_IDX_LOW] = true;
  }

  // High: Arming point < Thresh - 1.0f, Trigger point >= Thresh
  // A high alarm set for 100 enables at 98.9999... and goes off at 100.0
  if (getHighEnabled())
  {
    if (value < (getHigh() - 1))
      Armed[ALARM_IDX_HIGH] = true;
    else if (value >= getHigh() && Armed[ALARM_IDX_HIGH])
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
  Temperature = NAN;
  TemperatureAvg = NAN;
}

void TempProbe::addAdcValue(unsigned int analog_temp)
{
  _accumulator = analog_temp;
}

void TempProbe::calcTemp(void)
{
  //const unsigned int OVERSAMPLE_CNT[] = { 1, 4, 16, 64, 256, 1024, 4096 };
  const float ADCmax = (1 << (10+TEMP_OVERSAMPLE_BITS)) - 1; //OVERSAMPLE_CNT[TEMP_OVERSAMPLE_BITS];

  if (_probeType == PROBETYPE_INTERNAL || _probeType == PROBETYPE_TC_ANALOG)
    _accumulator = analogReadOver(_pin, 10+TEMP_OVERSAMPLE_BITS);
  //SerialX.print(_pin); SerialX.print('-'); SerialX.print(_accumulator); SerialX.print(' ');
  unsigned int ADCval = _accumulator;

  // Units 'A' = ADC value
  if (pid.getUnits() == 'A')
  {
    Temperature = ADCval;
    return;
  }

  if (ADCval != 0)  // Vout >= MAX is reduced in readTemp()
  {
    if (_probeType == PROBETYPE_TC_ANALOG)
    {
      float mvScale = Steinhart[3];
      // Commented out because there's no "divide by zero" exception so
      // just allow undefined results to save prog space
      //if (mvScale == 0.0f)
      //  mvScale = 1.0f;
      // If scale is <100 it is assumed to be mV/C with a 3.3V reference
      if (mvScale < 100.0f)
        mvScale = 3300.0f / mvScale;
#if defined(ADC_DYNAMIC_RANGE)
      if (adcState.lowRange[_pin])
        mvScale /= 3;
#endif
      setTemperatureC(ADCval / ADCmax * mvScale);
    }
    else {
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
    } /* if PROBETYPE_INTERNAL */
  } /* if ADCval */
  else
    Temperature = NAN;

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

GrillPid::GrillPid(unsigned char const fanPin, unsigned char const servoPin, unsigned char const feedbackAPin) :
    _fanPin(fanPin), _servoPin(servoPin), _feedbackAPin(feedbackAPin),
    _periodCounter(0x80), _units('F'), PidOutputAvg(NAN)
{
#if defined(GRILLPID_SERVO_ENABLED)
  pinMode(_servoPin, OUTPUT);
#endif
}

void GrillPid::init(void) const
{
#if defined(GRILLPID_SERVO_ENABLED)
  // CTC mode with ICR1 as TOP, 8 prescale, INT on COMPB and TOP (ICR
  // Period set to SERVO_REFRESH
  // If GrillPid is constructed statically this can't be done in the constructor
  // because the Arduino core init is called after the constructor and will set
  // the values back to the default
  ICR1 = uSecToTicks(SERVO_REFRESH);
  TCCR1A = 0;
  TCCR1B = bit(WGM13) | bit(WGM12) | bit(CS11);
  TIMSK1 = bit(ICIE1) | bit(OCIE1B);
#endif

  // TIMER2 488Hz Fast PWM
  TCCR2A = bit(WGM21) | bit(WGM20);
  //TCCR2B = bit(CS22) | bit(CS20);
  // 62khz
  TCCR2B = bit(CS20);
  // 7khz
  //TCCR2B = bit(CS21);
  // 61Hz
  //TCCR2B = bit(CS22) | bit(CS21) | bit(CS20);

  // Initialize ADC for free running mode at 125kHz
  ADMUX = (DEFAULT << 6) | 0;
  ADCSRB = bit(ACME);
  ADCSRA = bit(ADEN) | bit(ADATE) | bit(ADIE) | bit(ADPS2) | bit(ADPS1) | bit (ADPS0);
  ADCSRA |= bit(ADSC);
}

unsigned int GrillPid::countOfType(unsigned char probeType) const
{
  unsigned char retVal = 0;
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
    if (Probes[i]->getProbeType() == probeType)
      ++retVal;
  return retVal;  
}

/* Calucluate the desired output percentage using the proportional–integral-derivative (PID) controller algorithm */
inline void GrillPid::calcPidOutput(void)
{
  unsigned char lastOutput = _pidOutput;
  _pidOutput = 0;

  // If the pit probe is registering 0 degrees, don't jack the fan up to MAX
  if (!Probes[TEMP_PIT]->hasTemperature())
    return;

  // If we're in lid open mode, fan should be off
  if (isLidOpen())
    return;

  float currentTemp = Probes[TEMP_PIT]->Temperature;
  float error;
  error = _setPoint - currentTemp;

  // PPPPP = fan speed percent per degree of error
  _pidCurrent[PIDP] = Pid[PIDP] * error;

  // IIIII = fan speed percent per degree of accumulated error
  // anti-windup: Make sure we only adjust the I term while inside the proportional control range
  if ((error > 0 && lastOutput < 100) || (error < 0 && lastOutput > 0))
    _pidCurrent[PIDI] += Pid[PIDI] * error;

  // DDDDD = fan speed percent per degree of change over TEMPPROBE_AVG_SMOOTH period
  _pidCurrent[PIDD] = Pid[PIDD] * (Probes[TEMP_PIT]->TemperatureAvg - currentTemp);
  // BBBBB = fan speed percent
  _pidCurrent[PIDB] = Pid[PIDB];

  int control = _pidCurrent[PIDB] + _pidCurrent[PIDP] + _pidCurrent[PIDI] + _pidCurrent[PIDD];
  _pidOutput = constrain(control, 0, 100);
}

unsigned char GrillPid::getFanSpeed(void) const
{
  if (bit_is_set(_outputFlags, PIDFLAG_FAN_ONLY_MAX) && _pidOutput < 100)
    return 0;
  return (unsigned int)_pidOutput * _maxFanSpeed / 100;
}

void GrillPid::adjustFeedbackVoltage(void)
{
  if (_lastBlowerOutput > 0 && _lastBlowerOutput < 255)
  {
    // _lastBlowerOutput is the voltage we want on the feedback pin
    // adjust _feedvoltLastOutput until the ffeedback == _lastBlowerOutput
    unsigned char ffeedback = analogReadOver(_feedbackAPin, 8);
    int error = ((int)_lastBlowerOutput - (int)ffeedback);
    int newOutput = _feedvoltLastOutput + (error * 2 / 3);
    if (newOutput >= 255)
      _feedvoltLastOutput = 255;
    else if (newOutput <= 0)
      _feedvoltLastOutput = 0;
    else
      _feedvoltLastOutput = newOutput;
#if defined(GRILLPID_FEEDVOLT_DEBUG)
    SerialX.print("HMLG,");
    SerialX.print("SMPS: ffeed="); SerialX.print(ffeedback, DEC);
    SerialX.print(" out="); SerialX.print(newOutput, DEC);
    SerialX.print(" fdesired="); SerialX.print(_lastBlowerOutput, DEC);
    Serial_nl();
#endif
  }
  else
    _feedvoltLastOutput = _lastBlowerOutput;

  analogWrite(_fanPin, _feedvoltLastOutput);
}

inline unsigned char FeedvoltToAdc(float v)
{
  // Calculates what an 8 bit ADC value would be for the given voltage
  const unsigned long R1 = 22000;
  const unsigned long R2 = 68000;
  // Scale the voltage by the voltage divder
  // v * R1 / (R1 + R2) = pV
  // Scale to ADC assuming 3.3V reference
  // (pV / 3.3) * 256 = ADC
  return ((v * R1 * 256) / ((R1 + R2) * 3.3f));
}

inline void GrillPid::commitFanOutput(void)
{
  /* Long PWM period is 10 sec */
  const unsigned int LONG_PWM_PERIOD = 10000;
  const unsigned int PERIOD_SCALE = (LONG_PWM_PERIOD / TEMP_MEASURE_PERIOD);

  unsigned char fanSpeed = getFanSpeed();
  /* For anything above _minFanSpeed, do a nomal PWM write.
     For below _minFanSpeed we use a "long pulse PWM", where
     the pulse is 10 seconds in length.  For each percent we are
     emulating, run the fan for one interval. */
  if (fanSpeed >= _minFanSpeed)
    _longPwmTmr = 0;
  else
  {
    // Simple PWM, ON for first [FanSpeed] intervals then OFF
    // for the remainder of the period
    if (((PERIOD_SCALE * fanSpeed / _minFanSpeed) > _longPwmTmr))
      fanSpeed = _minFanSpeed;
    else
      fanSpeed = 0;

    if (++_longPwmTmr > (PERIOD_SCALE - 1))
      _longPwmTmr = 0;
  }  /* long PWM */

  if (bit_is_set(_outputFlags, PIDFLAG_INVERT_FAN))
    fanSpeed = _maxFanSpeed - fanSpeed;

  unsigned char newBlowerOutput; //mappct(fanSpeed, 0, 255);
  //if (bit_is_set(_outputFlags, PIDFLAG_FAN_FEEDVOLT))
  newBlowerOutput = mappct(fanSpeed, FeedvoltToAdc(5.0f), FeedvoltToAdc(12.0f));
  //analogWrite(_fanPin, newBlowerOutput);

  // 0 is always 0
  if (fanSpeed == 0)
    _lastBlowerOutput = 0;
  // If going from 0% to non-0%, turn the blower fully on for one period
  // to get it moving (boost mode)
  else if (_lastBlowerOutput == 0 && newBlowerOutput != 0)
    _lastBlowerOutput = 255;
  else
    _lastBlowerOutput = newBlowerOutput;
  adjustFeedbackVoltage();
}

inline void GrillPid::commitServoOutput(void)
{
#if defined(GRILLPID_SERVO_ENABLED)
  unsigned char output;
  if (bit_is_set(_outputFlags, PIDFLAG_SERVO_ANY_MAX) && _pidOutput > 0)
    output = 100;
  else
    output = _pidOutput;

  if (bit_is_set(_outputFlags, PIDFLAG_INVERT_SERVO))
    output = 100 - output;

  // Get the output speed in 10x usec by LERPing between min and max
  output = mappct(output, _minServoPos, _maxServoPos);
  OCR1B = uSecToTicks(10U * output);
#endif
}

inline void GrillPid::commitPidOutput(void)
{
  calcExpMovingAverage(PIDOUTPUT_AVG_SMOOTH, &PidOutputAvg, _pidOutput);
  commitFanOutput();
  commitServoOutput();
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
  _manualOutputMode = false;
  _pidCurrent[PIDI] = 0.0f;
  LidOpenResumeCountdown = 0;
}

void GrillPid::setPidOutput(int value)
{
  _manualOutputMode = true;
  _pidOutput = constrain(value, 0, 100);
  LidOpenResumeCountdown = 0;
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
    _pidCurrent[PIDI] = 0.0f;
}

void GrillPid::status(void) const
{
#if defined(GRILLPID_SERIAL_ENABLED)
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

  SerialX.print(getPidOutput(), DEC);
  Serial_csv();
  SerialX.print((int)PidOutputAvg, DEC);
  Serial_csv();
  SerialX.print(LidOpenResumeCountdown, DEC);
#endif
}

#include "adc_noise.h"

boolean GrillPid::doWork(void)
{
  unsigned int elapsed = millis() - _lastWorkMillis;
  if (elapsed < TEMP_MEASURE_PERIOD)
    return false;
  _lastWorkMillis = millis();

#if defined(GRILLPID_CALC_TEMP) 
  //SerialX.print("HMLG,ADC ");
  for (unsigned char i=0; i<TEMP_COUNT; i++)
    Probes[i]->calcTemp();
  //Serial_nl();

  if (!_manualOutputMode)
  {
    // Always calculate the output
    // calcPidOutput() will bail if it isn't supposed to be in control
    calcPidOutput();
    
    int pitTemp = (int)Probes[TEMP_PIT]->Temperature;
    if ((pitTemp >= _setPoint) &&
      (_lidOpenDuration - LidOpenResumeCountdown > LIDOPEN_MIN_AUTORESUME))
    {
      // When we first achieve temperature, reduce any I sum we accumulated during startup
      // If we actually neded that sum to achieve temperature we'll rebuild it, and it
      // prevents bouncing around above the temperature when you first start up
      if (!_pitTemperatureReached)
      {
        _pitTemperatureReached = true;
        _pidCurrent[PIDI] *= 0.25f;
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
      ((int)PidOutputAvg < 90))
    {
      resetLidOpenResumeCountdown();
    }
  }   /* if !manualFanMode */
#endif

  commitPidOutput();
  adcDump();
  return true;
}

void GrillPid::pidStatus(void) const
{
#if defined(GRILLPID_SERIAL_ENABLED)
  TempProbe const* const pit = Probes[TEMP_PIT];
  if (pit->hasTemperature())
  {
    print_P(PSTR("HMPS"CSV_DELIMITER));
    for (unsigned char i=PIDB; i<=PIDD; ++i)
    {
      SerialX.print(_pidCurrent[i], 2);
      Serial_csv();
    }

    SerialX.print(pit->Temperature - pit->TemperatureAvg, 2);
    Serial_nl();
  }
#endif
}

void GrillPid::setUnits(char units)
{
  switch (units)
  {
    case 'A':
    case 'C':
    case 'F':
    case 'R':
      _units = units;
      break;
  }
}
