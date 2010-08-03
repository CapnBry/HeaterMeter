#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <ShiftRegLCD.h>

#define NUM_OF(x) (sizeof (x) / sizeof *(x))
#define MIN(x,y) ( x > y ? y : x )

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

enum eDisplayModes {dmFOOD1, dmFOOD2, dmAMBIENT, dmPID_DEBUG};

// Runtime Data
static unsigned int g_TempAccum[TEMP_COUNT];  // Both g_TempAccum and g_TempAvgs MUST be the same size, see resetTemps
static int g_TempAvgs[TEMP_COUNT];
static unsigned char fanSpeedPCT = 0;
static eDisplayModes displayMode = dmFOOD1;
static boolean g_TemperatureReached = false;
static unsigned int g_LidOpenResumeCountdown = 0;
// cached config
unsigned char lidOpenOffset;
char probeTempOffsets[TEMP_COUNT];

static float pidErrorSum = 0.0f;

ShiftRegLCD lcd(PIN_LCD_DATA, PIN_LCD_CLK, TWO_WIRE, 2); 

#define DEGREE "\xdf" // \xdf is the degree symbol on the Hitachi HD44780
const prog_char LCD_LINE1[] PROGMEM = "Pit:%3d"DEGREE"F [%3u%%]";
const prog_char LCD_LINE1_DELAYING[] PROGMEM = "Pit:%3d"DEGREE"F Lid%3u";
const prog_char LCD_LINE1_UNPLUGGED[] PROGMEM = "- No Pit Probe -";
const prog_char LCD_LINE2[] PROGMEM = "%-12s%3d"DEGREE;

#define eeprom_read_to(dst_p, eeprom_field, dst_size) eeprom_read_block(dst_p, (void *)offsetof(__eeprom_data, eeprom_field), MIN(dst_size, sizeof((__eeprom_data*)0)->eeprom_field))
#define eeprom_read(dst, eeprom_field) eeprom_read_to(&dst, eeprom_field, sizeof(dst))
#define eeprom_write_from(src_p, eeprom_field, src_size) eeprom_write_block(src_p, (void *)offsetof(__eeprom_data, eeprom_field), MIN(src_size, sizeof((__eeprom_data*)0)->eeprom_field))
#define eeprom_write(src, eeprom_field) { typeof(src) x = src; eeprom_write_from(&x, eeprom_field, sizeof(x)); }

const long eeprom_magic = 0xf00d800;

const struct PROGMEM __eeprom_data {
  long magic;
  int setPoint;
  char probeNames[TEMP_COUNT-1][13];
  char probeTempOffsets[TEMP_COUNT];
  unsigned char lidOpenOffset;
  unsigned int lidOpenDuration;
  float pidP;
  float pidI;
} DEFAULT_CONFIG PROGMEM = { 
  eeprom_magic,  // magic
  230,  // setpoint
  { "Food Probe1", "Food Probe2", "Ambient" },  // probe names
  { 0, 0, 0 },  // probe offsets
  20,  // lid open offset
  240, // lid open duration
  7.0f, // pidP
  0.01f // pidI
};

void resetTemps(void *p)
{
  memset(p, 0, sizeof(g_TempAccum));  // all temp arrays are the same size
}

void outputSerial(void)
{
#ifdef SERIAL_OUT
  const prog_char COMMA PROGMEM = ',';
  Serial.print(g_TempAvgs[TEMP_PIT]);
  Serial.print_P(COMMA);
  Serial.print(g_TempAvgs[TEMP_FOOD1]);
  Serial.print_P(COMMA);
  Serial.print(g_TempAvgs[TEMP_FOOD2]);
  Serial.print_P(COMMA);
  Serial.print(g_TempAvgs[TEMP_AMB]);
  Serial.print_P(COMMA);
  Serial.print(fanSpeedPCT,DEC);
  Serial.println();
#endif
}

void formatProbeLine(char *buffer, size_t buffsize, unsigned char probeIndex)
{
  char name[13];
  // This probably looks like it makes no sense, but the eeprom_read macros require constant
  // expressions so I can't use the nameIndex variable directly
  switch (probeIndex)
  {
    case TEMP_FOOD1: eeprom_read(name, probeNames[0]); break;
    case TEMP_FOOD2: eeprom_read(name, probeNames[1]); break;
    case TEMP_AMB:   eeprom_read(name, probeNames[2]); break;
  }
  snprintf_P(buffer, buffsize, LCD_LINE2, name, g_TempAvgs[probeIndex]);
}

void updateDisplay(void)
{
  // Only rotate LINE2 of the display every 2 calls
  static unsigned char nextDisplay;
  #define ROTATE_DISPLAY(n) { if ((++nextDisplay % 3) == 0) displayMode = n; }
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
        formatProbeLine(buffer, sizeof(buffer), TEMP_FOOD1);
        ROTATE_DISPLAY(dmFOOD2);
        break;
      } // intentional fallthrough
    case dmFOOD2:
      if (g_TempAvgs[TEMP_FOOD2] != 0)
      {
        formatProbeLine(buffer, sizeof(buffer), TEMP_FOOD2);
        ROTATE_DISPLAY(dmAMBIENT);
        break;
      } // intentional fallthrough
    case dmAMBIENT:
      formatProbeLine(buffer, sizeof(buffer), TEMP_AMB);
      ROTATE_DISPLAY(dmPID_DEBUG);
      break;
    case dmPID_DEBUG:
      snprintf(buffer, sizeof(buffer), "I %-7d       ", (long)(pidErrorSum * 100));
      ROTATE_DISPLAY(dmFOOD1);
      break;
  }
  lcd.print(buffer);
}

/* Calucluate the desired fan speed using the proportional–integral (PI) controller algorithm */
unsigned char calcFanSpeedPct(int setPoint, int currentTemp) 
{
  static unsigned char lastOutput = 0;
  float error;
  int control;
  
  // If the pit probe is registering 0 degrees, don't jack the fan up to MAX
  if (currentTemp == 0)
    return 0;

  error = setPoint - currentTemp;

  // anti-windup: Make sure we only adjust the I term while
  // inside the proportional control range
  if (!(lastOutput >= 100 && error > 0) && 
      !(lastOutput <= 0   && error < 0))
    pidErrorSum += error;
    
  float Kp, Ki;
  eeprom_read(Kp, pidP);
  eeprom_read(Ki, pidI);
  control = Kp * (error + (Ki * pidErrorSum));

  // limit control
  if (control > 100)
    lastOutput = 100;
  else if (control < 0)
    lastOutput = 0;
  else
    lastOutput = control;

  return lastOutput;
}

void tempReadingsAvailable(void)
{
  int pitTemp = g_TempAvgs[TEMP_PIT];
  int setPoint;
  eeprom_read(setPoint, setPoint);
  fanSpeedPCT = calcFanSpeedPct(setPoint, pitTemp);

  if (pitTemp >= setPoint)
  {
    g_TemperatureReached = true;
    g_LidOpenResumeCountdown = 0;
  }
  else if (g_LidOpenResumeCountdown > 0)
  {
    --g_LidOpenResumeCountdown;
    fanSpeedPCT = 0;
  }
  // If the pit temperature dropped has more than [lidOpenOffset] degrees 
  // after reaching temp note that the code assumes g_LidOpenResumeCountdown <= 0
  else if (g_TemperatureReached && ((setPoint - pitTemp) > lidOpenOffset))
  {
    unsigned int resume;
    eeprom_read(resume, lidOpenDuration);
    g_LidOpenResumeCountdown = resume;
    g_TemperatureReached = false;
  }
  
  analogWrite(PIN_BLOWER, (fanSpeedPCT * 255 / 100));

  updateDisplay();
  outputSerial();
}

int convertAnalogTemp(unsigned int Vout, unsigned char steinhart_index) 
{
  const struct steinhart_param
  { 
    float A, B, C; 
  } STEINHART[] = {
    {2.3067434E-4, 2.3696596E-4f, 1.2636414E-7f},  // Maverick Probe
    {8.98053228e-004f, 2.49263324e-004f, 2.04047542e-007f}, // Radio Shack 10k
  };
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
  
  // Compute degrees K  
  T = (1.0f / ((param->C * R * R + param->B) * R + param->A));
  
  // Sanity - anything less than 0F or greater than 999F is rejected
  if ((T < 274.0f) || (T > 810.0f))
    return 0;
    
  // return degrees F
  return (int)((T - 273.15f) * (9.0f / 5.0f)) + 32;
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
      g_TempAvgs[i] = probeTempOffsets[i] + 
        convertAnalogTemp(g_TempAccum[i] >> (3 + TEMP_AVG_COUNT_LOG2), PROBES[i].steinhart_index);
    }
    
    tempReadingsAvailable();
    resetTemps(g_TempAccum);
  }
  else
    ++g_TempAccum[0];
}

void eepromInitialize(void)
{
  struct __eeprom_data defConfig;
  memcpy_P(&defConfig, &DEFAULT_CONFIG, sizeof(defConfig));
  eeprom_write_block(&defConfig, 0, sizeof(defConfig));  
}

void eepromCache(void)
{
  eeprom_read(lidOpenOffset, lidOpenOffset);
  eeprom_read(probeTempOffsets, probeTempOffsets);
}

void setup(void)
{
  Serial.begin(9600);

  long magic;
  eeprom_read(magic, magic);
  if(magic != eeprom_magic)
    eepromInitialize();
  eepromCache();

  resetTemps(g_TempAccum);
  resetTemps(g_TempAvgs);

  updateDisplay();
}

void loop(void)
{
  unsigned long time = millis();
  readTemps();
  
  time -= millis(); 
  if (time < (1000 >> TEMP_AVG_COUNT_LOG2))
    delay((1000 >> TEMP_AVG_COUNT_LOG2) - time);
}

