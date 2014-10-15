// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net>
// GrillPid uses TIMER1 COMPB vector, as well as modifies the waveform
// generation mode of TIMER1. Blower output pin needs to be a hardware PWM pin.
// Fan output is 489Hz phase-correct PWM
// Servo output is 50Hz pulse duration
#include <math.h>
#include <string.h>
#include <util/atomic.h>
#include <digitalWriteFast.h>

#include "strings.h"
#include "grillpid.h"

extern const GrillPid pid;

// For this calculation to work, ccpm()/8 must return a round number
#define uSecToTicks(x) ((unsigned int)(clockCyclesPerMicrosecond() / 8) * x)

// LERP percentage o into the unsigned range [A,B]. B - A must be < 655
#define mappct(o, a, b)  (((b - a) * (unsigned int)o / 100) + a)

#define DIFFMAX(x,y,d) ((x - y + d) <= (d*2U))

#define satlimit(orig,filt) ((orig)<(filt)?(-1):((orig)>(filt)?(1):(0)))

//Changing the PWM to Inverted PWM
#define INVERT(x) (255-x)

#if defined(GRILLPID_SERVO_ENABLED)
ISR(TIMER1_CAPT_vect)
{
  unsigned int cnt = OCR1B;
  if (cnt != 0)
  {
    OCR1B = cnt + pid.getServoStep();
    digitalWriteFast(PIN_SERVO, HIGH);
  }
}

ISR(TIMER1_COMPB_vect)
{
  digitalWriteFast(PIN_SERVO, LOW);
}
#endif

#if defined(FAN_PWM_FRACTION)
volatile unsigned char fracPwmComp;  //Value to emulate the shutoff point for duty cycle
ISR(TIMER2_COMPB_vect) 
{
	static unsigned char frac;  //Accumulator for compare to duty cycle value

	++frac;
	if (frac == 0) {
		OCR2B = INVERT(0); // Output is off (pulses are being dropped)
	}
	if (frac == fracPwmComp) {
		OCR2B = INVERT(1); // Output compare match of 1/256 duty cycle
	}
}
#endif /* FAN_PWM_FRACTION */

static struct tagAdcState
{
  unsigned char cnt;      // count in accumulator
  unsigned long accumulator;  // total
  unsigned char discard;  // Discard this many ADC readings
  unsigned int thisHigh;  // High this period
  unsigned int thisLow;   // Low this period
  unsigned int analogReads[NUM_ANALOG_INPUTS]; // Current values
  unsigned int analogRange[NUM_ANALOG_INPUTS]; // high-low on last period
#if defined(GRILLPID_DYNAMIC_RANGE)
  bool useBandgapReference[NUM_ANALOG_INPUTS]; // Use 1.1V reference instead of AVCC
  unsigned int bandgapAdc;                     // 10-bit adc reading of BG with AVCC ref
#endif
#if defined(NOISEDUMP_PIN)
  unsigned int data[256];
#endif
} adcState;

#if defined(NOISEDUMP_PIN)
unsigned char g_NoisePin = NOISEDUMP_PIN;
#endif

ISR(ADC_vect)
{
  if (adcState.discard != 0)
  {
    --adcState.discard;
    return;
  }

  unsigned int adc = ADC;
#if defined(NOISEDUMP_PIN)
  if ((ADMUX & 0x07) == g_NoisePin)
    adcState.data[adcState.cnt] = adc;
#endif
  adcState.accumulator += adc;
  ++adcState.cnt;

  if (adcState.cnt != 0)
  {
    // Not checking the range on the 256th sample saves some clock cycles
    // and helps offset the penalty of the end of period calculations
    if (adc > adcState.thisHigh)
      adcState.thisHigh = adc;
    if (adc < adcState.thisLow)
      adcState.thisLow = adc;
  }
  else
  {
    unsigned char pin = ADMUX & 0x0f;
#if defined(GRILLPID_DYNAMIC_RANGE)
    if (pin > NUM_ANALOG_INPUTS)
    {
      // Store only the last ADC value, giving the bandgap ~25ms to stabilize
      adcState.bandgapAdc = adc;
    }
    else
#endif // GRILLPID_DYNAMIC_RANGE
    {
#if defined(ADC_ROUND)
      // round off to bring error to 1/2 LSB instead of 1 LSB
      adcState.accumulator += (1 << (8 - TEMP_OVERSAMPLE_BITS-1) );
#endif
      adcState.analogReads[pin] = adcState.accumulator >> (8 - TEMP_OVERSAMPLE_BITS);
      adcState.analogRange[pin] = adcState.thisHigh - adcState.thisLow;
    }
    adcState.thisHigh = 0;
    adcState.thisLow = 0xffff;
    adcState.accumulator = 0;

    ++pin;
    if (pin >= NUM_ANALOG_INPUTS)
      pin = 0;
#if defined(GRILLPID_DYNAMIC_RANGE)
    unsigned char newref =
      adcState.useBandgapReference[pin] ? (INTERNAL << 6) : (DEFAULT << 6);
    unsigned char curref = ADMUX & 0xc0;
    // If switching references, allow time for AREF cap to charge
    if (curref != newref)
      adcState.discard = 48;  // 48 / 9615 samples/s = 5ms
    else
      adcState.discard = 2;

    ADMUX = newref | pin;
#else
    ADMUX = (DEFAULT << 6) | pin;
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
  // Calc 1/2 LSB of the bits we are dropping and add back to round
#if defined(ADC_ROUND)
  unsigned int round = (10 + TEMP_OVERSAMPLE_BITS - bits);
  if (round > 1 )
  {
    round = 1 << (10 + TEMP_OVERSAMPLE_BITS - bits - 1);
    retVal += round;
  }
#endif /* ADC_ROUND */
  return (retVal >> (10 + TEMP_OVERSAMPLE_BITS - bits));
}

unsigned int analogReadRange(unsigned char pin)
{
  unsigned int retVal;
  ATOMIC_BLOCK(ATOMIC_FORCEON)
  {
    retVal = adcState.analogRange[pin];
  }
  return retVal;
}

#if defined(GRILLPID_DYNAMIC_RANGE)
bool analogIsBandgapReference(unsigned char pin)
{
  return adcState.useBandgapReference[pin];
}

void analogSetBandgapReference(unsigned char pin, bool enable)
{
  adcState.useBandgapReference[pin] = enable;
}
#endif /* GRILLPID_DYNAMIC_RANGE */

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
    ADCSRA = bit(ADEN) | bit(ADATE) | bit(ADIE) | bit(ADPS2) | bit(ADPS1) | bit (ADPS0) | bit(ADSC);
  }
#endif
}

static void calcExpMovingAverage(const float smooth, float *currAverage, float newValue)
{
  newValue = newValue - *currAverage;
  *currAverage = *currAverage + (smooth * newValue);
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
  _pin(pin), _tempStatus(TSTATUS_NONE)
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
  _tempStatus = TSTATUS_NONE;
  _hasTempAvg = false;
}

void TempProbe::calcTemp(unsigned int adcval)
{
  // Units 'A' = ADC value
  if (pid.getUnits() == 'A')
  {
    Temperature = adcval;
    _tempStatus = TSTATUS_OK;
    return;
  }

  // Ignore probes within 1 LSB of max
  if (adcval > 1022 * pow(2, TEMP_OVERSAMPLE_BITS) || adcval == 0)
    _tempStatus = TSTATUS_NONE;
  else
  {
    const float ADCmax = 1023 * pow(2, TEMP_OVERSAMPLE_BITS);

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
#if defined(GRILLPID_DYNAMIC_RANGE)
      if (analogIsBandgapReference(_pin))
      {
        analogSetBandgapReference(_pin, adcval < (1000U * (unsigned char)pow(2, TEMP_OVERSAMPLE_BITS)));
        mvScale /= 1023.0f / adcState.bandgapAdc;
      }
      else
        analogSetBandgapReference(_pin, adcval < (300U * (unsigned char)pow(2, TEMP_OVERSAMPLE_BITS)));
#endif
      setTemperatureC(adcval / ADCmax * mvScale);
    }
    else {
      float R, T;
      // If you put the fixed resistor on the Vcc side of the thermistor, use the following
      R = Steinhart[3] / ((ADCmax / (float)adcval) - 1.0f);
      // If you put the thermistor on the Vcc side of the fixed resistor use the following
      //R = Steinhart[3] * ADCmax / (float)Vout - Steinhart[3];

      // Units 'R' = resistance, unless this is the pit probe (which should spit out Celsius)
      if (pid.getUnits() == 'R' && this != pid.Probes[TEMP_PIT])
      {
        Temperature = R;
        _tempStatus = TSTATUS_OK;
        return;
      };

      // Compute degrees K
      R = log(R);
      T = 1.0f / ((Steinhart[2] * R * R + Steinhart[1]) * R + Steinhart[0]);

      setTemperatureC(T - 273.15f);
    } /* if PROBETYPE_INTERNAL */
  } /* if ADCval */
}

void TempProbe::processPeriod(void)
{
  // Called once per measurement period after temperature has been calculated
  if (hasTemperature())
  {
    if (!_hasTempAvg)
    {
      TemperatureAvg = Temperature;
      _hasTempAvg = true;
    }
    else
      calcExpMovingAverage(TEMPPROBE_AVG_SMOOTH, &TemperatureAvg, Temperature);
    Alarms.updateStatus(Temperature);
  }
  else
    Alarms.silenceAll();
}

void TempProbe::setTemperatureC(float T)
{
  // Sanity - anything less than -20C (-4F) or greater than 500C (932F) is rejected
  if (T <= -20.0f)
    _tempStatus = TSTATUS_LOW;
  else if (T >= 500.0f)
    _tempStatus = TSTATUS_HIGH;
  else
  {
    if (pid.getUnits() == 'F')
      Temperature = (T * (9.0f / 5.0f)) + 32.0f;
    else
      Temperature = T;
    Temperature += Offset;
    _tempStatus = TSTATUS_OK;
  }
}

void GrillPid::init(void) const
{
#if defined(GRILLPID_SERVO_ENABLED)
  pinModeFast(PIN_SERVO, OUTPUT);

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
  // Initialize ADC for free running mode at 125kHz
#if defined(GRILLPID_DYNAMIC_RANGE)
  // Start by measuring the bandgap reference for dynamic range scaling
  ADMUX = (DEFAULT << 6) | 0b1110;
#else
  ADMUX = (DEFAULT << 6) | 0;
#endif // GRILLPID_DYNAMIC_RANGE
  ADCSRB = bit(ACME);
  ADCSRA = bit(ADEN) | bit(ADATE) | bit(ADIE) | bit(ADPS2) | bit(ADPS1) | bit (ADPS0) | bit(ADSC);
}

void GrillPid::setOutputFlags(unsigned char value)
{
  _outputFlags = value;
#if defined(FAN_PWM_FRACTION)
  // Timer2 Inverted Fast PWM
  TCCR2A = bit(COM2B1)|bit(COM2B0)|bit(WGM21) | bit(WGM20);
#else
  // Timer2 Fast PWM
  TCCR2A = bit(WGM21) | bit(WGM20);
#endif /* FAN_PWM_FRACTION */
  if (bit_is_set(_outputFlags, PIDFLAG_FAN_FEEDVOLT))
    TCCR2B = bit(CS20); // 62kHz
  else
    TCCR2B = bit(CS22) | bit(CS20); // 488Hz
  // 7khz
  //TCCR2B = bit(CS21);
  // 61Hz
  //TCCR2B = bit(CS22) | bit(CS21) | bit(CS20);
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

  // DDDDD = fan speed percent per degree of change over TEMPPROBE_AVG_SMOOTH period
  if ( _deriv[DRV_FILT] != 0 ) 
  {
    calcExpMovingAverage(DERIV_AVG_SMOOTH, &_deriv[DRV_FILT], currentTemp);
  }
  else
  {
    _deriv[DRV_FILT] = currentTemp;
    _deriv[DRV_PRV1] = _deriv[DRV_PRV2] = _deriv[DRV_PRV3] = 0;
  }
  _pidCurrent[PIDD] = (Probes[TEMP_PIT]->TemperatureAvg - _deriv[DRV_FILT]);
  float secondDrv = _pidCurrent[PIDD] - _deriv[DRV_PRV3];
  secondDrv /= 4; // Average derivative change over last 4 seconds
  //Save values
  _deriv[DRV_PRV3] = _deriv[DRV_PRV2];
  _deriv[DRV_PRV2] = _deriv[DRV_PRV1];
  _deriv[DRV_PRV1] = _pidCurrent[PIDD];
  // Add in the calculated second order derivative. Attempt to reduce the lag created by making derivative filter
  _pidCurrent[PIDD] += secondDrv;
  _pidCurrent[PIDD] = Pid[PIDD] * _pidCurrent[PIDD];

  // BBBBB = fan speed percent
  _pidCurrent[PIDB] = Pid[PIDB];

  int control = _pidCurrent[PIDB] + _pidCurrent[PIDP] + _pidCurrent[PIDI] + _pidCurrent[PIDD];
  _pidOutput = constrain(control, 0, 100);

  // Check if we are trying to drive the controller beyond full saturation
  // sat 0 is ok, -1 is controller is out of saturated low, 1 is saturated high
  signed char sat = satlimit(control,(int)_pidOutput);
  
  // IIIII = fan speed percent per degree of accumulated error
  // Integral latching: Make sure we only adjust the I term while inside the proportional control range
  if ( sat == 0 )
  {
    _pidCurrent[PIDI] += Pid[PIDI] * error;
  } 
  else {
  // Additional check to see if we way over saturated. Keeps integrator from bouncing off the control limits
    if ( (control - (int)_pidOutput) * sat > 2 )
      //Integral Anti-windup will slowly bring the integral back to 0 as long as the control is saturated
    _pidCurrent[PIDI] = (1- (Pid[PIDI]*10)) * _pidCurrent[PIDI];
  }
}

#if defined(FAN_PWM_FRACTION)
void GrillPid::fanVoltWrite(void)
{
  if (_lastBlowerOutput == 0 ) {
    // disable the Fractional ISR
    TIMSK2 = TIMSK2 & ~bit(OCIE2B);
    analogWrite(PIN_BLOWER, 0);  //Use the analogwrite special case for 0 to shutoff the pin
    return;
  }

  unsigned char output = _feedvoltLastOutput;

  // If we have a whole amount then use original method and return
  //Duty cycle control at 62.5 Khz
  if ( output > 0 ) {
    if ( output == 255) //sending 255 to analogWrite when we are inverted is going to be wrong
      output = 254; 
    // disable the Fractional ISR
    TIMSK2 = TIMSK2 & ~bit(OCIE2B);
    analogWrite(PIN_BLOWER, INVERT(output));
    return;
  }

  output = _feedvoltOutputFrac;
  // Make sure we aren't shutting off the voltage output completely
  if (output == 0 )
    output++;
  // Set compare match for PWM ISR
  // We can't lower the duty cycle anymore so now will drop pulses to lower output
  fracPwmComp = INVERT(output);
  if (!(bit_is_set(TIMSK2, OCIE2B))) {
    TIMSK2 |= bit(OCIE2B);  // Output Compare Interupt Enable Time2 Channel B
  }
}
#endif /* FAN_PWM_FRACTION */

void GrillPid::adjustFeedbackVoltage(void)
{
#if defined(FAN_PWM_FRACTION)
  if (_lastBlowerOutput != 0 && bit_is_set(_outputFlags, PIDFLAG_FAN_FEEDVOLT)) {
    union fpm_type
    {
      unsigned char ch[2];
      unsigned int full;
    } currentFPM, newOutputFPM;
    
    currentFPM.ch[1] = _feedvoltLastOutput; //whole number
    // Radix point should be here
    currentFPM.ch[0] = _feedvoltOutputFrac; //fractional number

    // _lastBlowerOutput is the voltage we want on the feedback pin
    // adjust _feedvoltLastOutput until the ffeedback == _lastBlowerOutput
    unsigned int ffeedback = analogReadOver(APIN_FFEEDBACK, 10);
    signed int error = (_lastBlowerOutput<<2) - ffeedback;
    error /= 2;

    // percentage of error in 8:8 format
    // multiply by 8 to get into 8:8 format, another 8 for percentage form
    signed long fracErrorFPM = (error * pow(2,16)) / ffeedback;

    // temp should now contain the amount of offset needed
    // shift 8 to correct for FPM, another 8 to get back to whole number form
    signed long tempFPM = (fracErrorFPM * (unsigned long)currentFPM.full) / pow(2,16);

    newOutputFPM.full = currentFPM.full + tempFPM;

    _feedvoltLastOutput = newOutputFPM.ch[1];
    _feedvoltOutputFrac = newOutputFPM.ch[0];
  }
  else {
    _feedvoltLastOutput = _lastBlowerOutput;
  }
  fanVoltWrite();
#else
  if (_lastBlowerOutput != 0 && bit_is_set(_outputFlags, PIDFLAG_FAN_FEEDVOLT))
  {
    // _lastBlowerOutput is the voltage we want on the feedback pin
    // adjust _feedvoltLastOutput until the ffeedback == _lastBlowerOutput
    unsigned char ffeedback = analogReadOver(APIN_FFEEDBACK, 8);
    int error = ((int)_lastBlowerOutput - (int)ffeedback);
    int newOutput = (int)_feedvoltLastOutput + (error / 2);
    _feedvoltLastOutput = constrain(newOutput, 0, 255);

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

  analogWrite(PIN_BLOWER, _feedvoltLastOutput);
#endif /* FAN_PWM_FRACTION */
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

  if (bit_is_set(_outputFlags, PIDFLAG_FAN_ONLY_MAX) && _pidOutput < 100)
    _fanSpeed = 0;
  else
  {
    unsigned char max;
    if (_pitStartRecover == PIDSTARTRECOVER_STARTUP)
      max = _maxStartupFanSpeed;
    else
      max = _maxFanSpeed;

#if defined(GRILLPID_GANG_ENABLED)
    if (bit_is_set(_outputFlags, PIDFLAG_FAN_GANGED) && (!_manualOutputMode)) {
      boolean madeSpeedShift = false;
      unsigned long elapsed = millis() - _lastFanMillis;
      if (elapsed > FAN_GANG_PERIOD ) {
        if ( (_pidOutput > FAN_GANG_UPSHIFT) ) {
          if  (_lastFanSpeed < (100 - FAN_GANG_SHIFT) ) {
            _lastFanSpeed += FAN_GANG_SHIFT;
            madeSpeedShift = true;
            // Knock the integrator back down as we are going to push more air
            _pidCurrent[PIDI] = _pidCurrent[PIDI] - 5.0;
          }
        }
        if ( _pidOutput < FAN_GANG_DNSHIFT ) {
          //Jump to 0 if servo has went to 0
          if ( _pidOutput == 0 ) {
            _lastFanSpeed = 0;
          }
          if ( _lastFanSpeed > FAN_GANG_SHIFT ) {
            _lastFanSpeed -= FAN_GANG_SHIFT;
            madeSpeedShift = true;
            // Give the integrator a bit more as we reduced airflow
            _pidCurrent[PIDI] = _pidCurrent[PIDI] + 5.0;
          }
        }
        constrain(_lastFanSpeed,0,100);
        /* Check if we actually did anything and if so then update to lock out
           for FAN_GANG_PERIOD
        */
        if ( madeSpeedShift ) {
          _lastFanMillis = millis();
        }
      }
      _fanSpeed = (unsigned int)_lastFanSpeed * max / 100;
#if defined(ROB_OUTPUT_HACK)
      // Simply to show on web page
			Probes[TEMP_AMB]->Temperature = _fanSpeed;
#endif /* ROB_OUTPUT_HACK)
    }
    else {
#endif /* GRILLPID_GANG_ENABLED */
    _fanSpeed = (unsigned int)_pidOutput * max / 100;
#if defined(GRILLPID_GANG_ENABLED)
    }
#endif /* GRILLPID_GANG_ENABLED */
  }

  /* For anything above _minFanSpeed, do a nomal PWM write.
     For below _minFanSpeed we use a "long pulse PWM", where
     the pulse is 10 seconds in length.  For each percent we are
     emulating, run the fan for one interval. */
  if (_fanSpeed >= _minFanSpeed)
    _longPwmTmr = 0;
  else
  {
    // Simple PWM, ON for first [FanSpeed] intervals then OFF
    // for the remainder of the period
    if (((PERIOD_SCALE * _fanSpeed / _minFanSpeed) > _longPwmTmr))
      _fanSpeed = _minFanSpeed;
    else
      _fanSpeed = 0;

    if (++_longPwmTmr > (PERIOD_SCALE - 1))
      _longPwmTmr = 0;
  }  /* long PWM */

  if (bit_is_set(_outputFlags, PIDFLAG_INVERT_FAN))
    _fanSpeed = _maxFanSpeed - _fanSpeed;

  // 0 is always 0
  if (_fanSpeed == 0)
    _lastBlowerOutput = 0;
  else
  {
    bool needBoost = _lastBlowerOutput == 0;
    if (bit_is_set(_outputFlags, PIDFLAG_FAN_FEEDVOLT))
      _lastBlowerOutput = mappct(_fanSpeed, FeedvoltToAdc(5.0f), FeedvoltToAdc(12.1f));
    else
      _lastBlowerOutput = mappct(_fanSpeed, 0, 255);
    // If going from 0% to non-0%, turn the blower fully on for one period
    // to get it moving (boost mode)
    if (needBoost)
    {
      analogWrite(PIN_BLOWER, 255);
      // give the FFEEDBACK control a high starting point so when it reads
      // for the first time and sees full voltage it doesn't turn off
#if defined(FAN_PWM_FRACTION)
      _feedvoltLastOutput = 254;
#else
      _feedvoltLastOutput = 128;
#endif /* FAN_PWM_FRACTION */
      return;
    }
  }
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
  // _servoTarget could be 0 if this is the first set, set to min and slope from there
  if (_servoTarget == 0)
    _servoTarget = uSecToTicks(10U * _minServoPos);
  int targetTicks = uSecToTicks(10U * output);
  int targetDiff = targetTicks - _servoTarget;
  ATOMIC_BLOCK(ATOMIC_FORCEON)
  {
    _servoStep = targetDiff / (TEMP_MEASURE_PERIOD / (SERVO_REFRESH / 1000));
    OCR1B = _servoTarget + _servoStep;
  }
  _servoTarget = targetTicks;
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
  _pitStartRecover = PIDSTARTRECOVER_RECOVERY;
}

void GrillPid::setSetPoint(int value)
{
  _setPoint = value;
  _pitStartRecover = PIDSTARTRECOVER_STARTUP;
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
  // No need to reset the PIDI as it's already been scaled before integration
  // Zeroing just causes the controller to slam shut and makes tuning harder
  //if (idx == PIDI)
    // Proably should scale the error sum by newval / oldval instead of resetting
    //_pidCurrent[PIDI] = 0.0f;
}

void GrillPid::status(void) const
{
#if defined(GRILLPID_SERIAL_ENABLED)
  SerialX.print(getSetPoint(), DEC);
  Serial_csv();

  for (unsigned char i=0; i<TEMP_COUNT; ++i)
  {
#if defined(ROB_OUTPUT_HACK)
		if ((Probes[i]->hasTemperature()) || (i == TEMP_AMB))
#else
    if (Probes[i]->hasTemperature())
#endif /* ROB_OUTPUT_HACK */
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
  Serial_csv();
  SerialX.print(getFanSpeed(), DEC);
#endif
}

boolean GrillPid::doWork(void)
{
  unsigned int elapsed = millis() - _lastWorkMillis;
  if (elapsed < (TEMP_MEASURE_PERIOD / TEMP_OUTADJUST_CNT))
    return false;
  _lastWorkMillis = millis();

  if (_periodCounter < (TEMP_OUTADJUST_CNT-1))
  {
    ++_periodCounter;
    adjustFeedbackVoltage();
    return false;
  }
  _periodCounter = 0;

#if defined(GRILLPID_CALC_TEMP) 
  for (unsigned char i=0; i<TEMP_COUNT; i++)
  {
    if (Probes[i]->getProbeType() == PROBETYPE_INTERNAL ||
        Probes[i]->getProbeType() == PROBETYPE_TC_ANALOG)
      Probes[i]->calcTemp(analogReadOver(Probes[i]->getPin(), 10+TEMP_OVERSAMPLE_BITS));
    Probes[i]->processPeriod();
  }

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
      if (_pitStartRecover == PIDSTARTRECOVER_STARTUP)
      {
        _pidCurrent[PIDI] *= 0.50f;
      }
      _pitStartRecover = PIDSTARTRECOVER_NORMAL;
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
    else if (isPitTempReached() && 
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
