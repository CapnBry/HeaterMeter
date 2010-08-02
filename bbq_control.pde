#include <avr/pgmspace.h>
#include <ShiftRegLCD.h>

#define NUM_OF(x) (sizeof (x) / sizeof *(x))

// Configuration
#define SETPOINT 230  // in degrees F
// Analog Pins
#define PIN_PIT   5
#define PIN_FOOD1 4
#define PIN_FOOD2 3
#define PIN_AMB   2
// Digital Output Pins
#define PIN_BLOWER    3
#define PIN_LCD_CLK   4
#define PIN_LCD_DATA  8
// The temperatures are averaged over 1, 2, 4 or 8 samples
// Set this define to log2(samples) to adjust the number of samples
#define TEMP_AVG_COUNT_LOG2 2

const static struct probe_mapping {
  unsigned char pin; 
  unsigned char steinhart_index; 
} PROBES[] = {
  {PIN_PIT, 0}, {PIN_FOOD1, 0}, 
  {PIN_FOOD2, 0}, {PIN_AMB, 1}
};

#define TEMP_PIT    0
#define TEMP_FOOD1  1
#define TEMP_FOOD2  2
#define TEMP_AMB    3
#define TEMP_COUNT  NUM_OF(PROBES)
#define LID_OPEN_RESUME_COUNT 240;

enum eDisplayModes {dmFOOD1, dmFOOD2, dmAMBIENT, dmPID_DEBUG};

// Runtime Data
static unsigned int g_TempAccum[TEMP_COUNT];  // Both g_TempAccum and g_TempAvgs MUST be the same size, see resetTemps
static unsigned int g_TempAvgs[TEMP_COUNT];
static unsigned char fanSpeedPCT = 0;
static eDisplayModes displayMode = dmFOOD1;
static char g_PROBENAMES[][13] = {"Food Probe1 ", "Food Probe2 ", "Ambient     "};
static boolean g_TemperatureReached = false;
static unsigned int g_LidOpenResumeCountdown = 0;

static float integralSum = 0.0f;

ShiftRegLCD lcd(PIN_LCD_DATA, PIN_LCD_CLK, TWO_WIRE, 2); 

#define DEGREE "\xdf" // \xdf is the degree symbol on the Hitachi HD44780
const char LCD_LINE1[] PROGMEM = "Pit:%3u"DEGREE"F [%3u%%]";
const char LCD_LINE1_DELAYING[] PROGMEM = "Pit:%3u"DEGREE"F Lid%3u";
const char LCD_LINE1_UNPLUGGED[] PROGMEM = "- No Pit Probe -";
const char LCD_LINE2[] PROGMEM = "%12s%3u"DEGREE;

const char DEFAULT_PROBE_NAMES[] PROGMEM = "Food Probe1 \0Food Probe2 \0Ambient     ";

void resetTemps(unsigned int *p)
{
  memset(p, 0, sizeof(g_TempAccum));  // all temp arrays are the same size
}

void outputSerial(void)
{
#ifdef SERIAL_OUT
  Serial.print(g_TempAvgs[TEMP_PIT]);
  Serial.print(",");
  Serial.print(g_TempAvgs[TEMP_FOOD1]);
  Serial.print(",");
  Serial.print(g_TempAvgs[TEMP_FOOD2]);
  Serial.print(",");
  Serial.print(g_TempAvgs[TEMP_AMB]);
  Serial.print(",");
  Serial.print(fanSpeedPCT,DEC);
  Serial.println();
#endif
}

void updateDisplay(void)
{
  char buffer[17];
  
  lcd.home();
  if (g_TempAvgs[TEMP_PIT] == 0)
    memcpy_P(buffer, LCD_LINE1_UNPLUGGED, sizeof(LCD_LINE1_UNPLUGGED));
  else if (g_LidOpenResumeCountdown > 0)
    snprintf_P(buffer, sizeof(buffer), LCD_LINE1_DELAYING, g_TempAvgs[TEMP_PIT], g_LidOpenResumeCountdown);
  else
    snprintf_P(buffer, sizeof(buffer), LCD_LINE1, g_TempAvgs[TEMP_PIT], fanSpeedPCT);
  lcd.print(buffer); 

  lcd.setCursor(0, 1);
  switch (displayMode)
  {
    case dmFOOD1:
      if (g_TempAvgs[TEMP_FOOD1] != 0)
      {
        snprintf_P(buffer, sizeof(buffer), LCD_LINE2, g_PROBENAMES[0], g_TempAvgs[TEMP_FOOD1]);
        displayMode = dmFOOD2;
        break;
      } // intentional fallthrough
    case dmFOOD2:
      if (g_TempAvgs[TEMP_FOOD2] != 0)
      {
        snprintf_P(buffer, sizeof(buffer), LCD_LINE2, g_PROBENAMES[1], g_TempAvgs[TEMP_FOOD2]);
        displayMode = dmAMBIENT;
        break;
      } // intentional fallthrough
    case dmAMBIENT:
      snprintf_P(buffer, sizeof(buffer), LCD_LINE2, g_PROBENAMES[2], g_TempAvgs[TEMP_AMB]);
      displayMode = dmPID_DEBUG;
      break;
    case dmPID_DEBUG:
      unsigned char frac;
      if (integralSum > 0)
        frac = (integralSum - (int)integralSum) * 100;
      else
        frac = ((int)integralSum - integralSum) * 100;
      snprintf(buffer, sizeof(buffer), "I %5d.%02d      ", (int)integralSum, frac);
      displayMode = dmFOOD1;
      break;
  }
  lcd.print(buffer);
}

/* Calucluate the desired fan speed using the proportional–integral (PI) controller algorithm */
#define PID_P 5.0f
#define PID_I 0.02f
unsigned char calcFanSpeedPct(int setPoint, int currentTemp) 
{
  static unsigned char lastOutput = 0;

  float error;
  float control;
  
  // If the pit probe is registering 0 degrees, don't jack the fan up to MAX
  if (currentTemp == 0)
    return 0;

  error = setPoint - currentTemp;

  if (!((lastOutput >= 100) && (error > 0)) && !((lastOutput <= 0) && (error < 0)))
    integralSum += error;
    
  control = PID_P * (error + (PID_I * integralSum));

  // limit control
  if (control > 100)
    control = 100;
  else if (control < 0)
    control = 0;

  lastOutput = control;
  return control;
}

void tempReadingsAvailable(void)
{
  int pitTemp = (int)g_TempAvgs[TEMP_PIT];
  fanSpeedPCT = calcFanSpeedPct(SETPOINT, pitTemp);

  if (pitTemp >= SETPOINT)
  {
    g_TemperatureReached = true;
    g_LidOpenResumeCountdown = 0;
  }
  else if (g_LidOpenResumeCountdown > 0)
  {
    --g_LidOpenResumeCountdown;
    fanSpeedPCT = 0;
  }
  // If the pit temperature dropped has more than 20 degrees after reaching temp
  // note that the code assumes g_LidOpenResumeCountdown <= 0
  else if (g_TemperatureReached && ((SETPOINT - pitTemp) > 20))
  {
    g_LidOpenResumeCountdown = LID_OPEN_RESUME_COUNT;
    g_TemperatureReached = false;
  }
  
  analogWrite(PIN_BLOWER, (fanSpeedPCT * 255 / 100));

  updateDisplay();
  outputSerial();
}

unsigned int convertAnalogTemp(const unsigned int Vout, const unsigned char steinhart_index) 
{
  const struct steinhart_param
  { 
    float A, B, C; 
  } STEINHART[] = {
    {2.3067434E-4, 2.3696596E-4f, 1.2636414E-7f},  // Maverick Probe
    {8.98053228e-004f, 2.49263324e-004f, 2.04047542e-007f}, // Radio Shack 10k
  };
  // MAX(Voltage in) = 1023
  const float Rknown = 22000.0f;
  const float Vin = 1023.0f;  

  if ((Vout == 0) || (Vout >= (unsigned int)Vin))
    return 0;
    
  const struct steinhart_param *param = &STEINHART[steinhart_index];
  float R, T;
  // If you put the fixed resistor on the Vcc side of the thermistor, use the following
  R = log(Rknown / ((Vin / (float)Vout) - 1.0f));
  // If you put the thermistor on the Vcc side of the fixed resistor use the following
  // R = log(Rknown * Vin / (float)Vout - Rknown);
  
  // Compute degrees C
  T = (1.0f / ((param->C * R * R + param->B) * R + param->A)) - 273.15f;
  // return degrees F
  return (unsigned int)(T * (9.0f / 5.0f)) + 32;
}      

void readTemps(void)
{
  for (unsigned char i=0; i<TEMP_COUNT; i++)
  {
    unsigned int analog_temp = analogRead(PROBES[i].pin);
    // The bottom 3 bits store the loop counter, 3 bits for 8 readings per average
    // and the top 13 bits store the accumulated value.  13 bits = 8192 which is
    // perfect for our 8 readings of 0-1023
    g_TempAccum[i] += (analog_temp << 3);
  }
  
  // Only the first one holds the loop counter
  if ((g_TempAccum[0] & 7) == ((1 << TEMP_AVG_COUNT_LOG2) - 1))
  {
    for (unsigned char i=0; i<TEMP_COUNT; ++i)
    {
      // Shift out 3 bits to remove our loop counter, and TEMP_AVG_COUNT_LOG2 to compute the average
      g_TempAvgs[i] = convertAnalogTemp(g_TempAccum[i] >> (3 + TEMP_AVG_COUNT_LOG2), PROBES[i].steinhart_index);
    }
    
    tempReadingsAvailable();
    resetTemps(g_TempAccum);
  }
  else
    ++g_TempAccum[0];
}

void setup(void)
{
  Serial.begin(9600);
  
  resetTemps(g_TempAccum);
  resetTemps(g_TempAvgs);

  updateDisplay();
}

void loop(void)
{
  readTemps();
  
  delay(1000 >> TEMP_AVG_COUNT_LOG2);
}

