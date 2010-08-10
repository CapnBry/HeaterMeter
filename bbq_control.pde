#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <ShiftRegLCD.h>
#include <WiServer.h>

#include "menus.h"
//#include "grillpid.h"

#ifdef APP_WISERVER
// Wireless configuration parameters ----------------------------------------
unsigned char local_ip[] = {192,168,1,252};	// IP address of WiShield
unsigned char gateway_ip[] = {192,168,1,1};	// router or gateway IP address
unsigned char subnet_mask[] = {255,255,255,0};	// subnet mask for the local network
const prog_char ssid[] PROGMEM = {"M75FE"};		// max 32 bytes

unsigned char security_type = 1;	// 0 - open; 1 - WEP; 2 - WPA; 3 - WPA2
// WPA/WPA2 passphrase
const prog_char security_passphrase[] PROGMEM = {""};	// max 64 characters
// WEP 128-bit keys
// sample HEX keys
prog_uchar wep_keys[] PROGMEM = { 0xEC, 0xA8, 0x1A, 0xB4, 0x65, 0xf0, 0x0d, 0xbe, 0xef, 0xde, 0xad, 0x00, 0x00,	// Key 0
				  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// Key 1
				  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	// Key 2
				  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00	// Key 3
				};

// setup the wireless mode
// infrastructure - connect to AP
// adhoc - connect to another WiFi device
unsigned char wireless_mode = WIRELESS_MODE_INFRA;

unsigned char ssid_len;
unsigned char security_passphrase_len;
// End of wireless configuration parameters ----------------------------------------
#endif /* APP_WISERVER */

#define NUM_OF(x) (sizeof (x) / sizeof *(x))
#define MIN(x,y) ( x > y ? y : x )

// Analog Pins
#define PIN_PIT     5
#define PIN_FOOD1   4
#define PIN_FOOD2   3
#define PIN_AMB     2
#define PIN_BUTTONS 0
// Digital Output Pins
#define PIN_BLOWER    3
#define PIN_LCD_CLK   4
#define PIN_LCD_DATA  8
// The temperatures are averaged over 1, 2, 4 or 8 samples
// Set this define to log2(samples) to adjust the number of samples
#define TEMP_AVG_COUNT_LOG2 3

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

// Runtime Data
static unsigned int g_TempAccum[TEMP_COUNT];
static int g_TempAvgs[TEMP_COUNT];
static unsigned char fanSpeedPCT;
static float fanSpeedAVG;
static boolean g_TemperatureReached;
static unsigned int g_LidOpenResumeCountdown;
static boolean g_NetworkInitialized;
// scratch space for edits
static int editInt;  
static char editString[15];

// cached config
static unsigned char lidOpenOffset;
static char probeTempOffsets[TEMP_COUNT];
static float pidConstants[3]; 

static float pidErrorSum;

static ShiftRegLCD lcd(PIN_LCD_DATA, PIN_LCD_CLK, TWO_WIRE, 2); 

#define DEGREE "\xdf" // \xdf is the degree symbol on the Hitachi HD44780
const prog_char LCD_LINE1[] PROGMEM = "Pit:%3d"DEGREE"F [%3u%%]";
const prog_char LCD_LINE1_DELAYING[] PROGMEM = "Pit:%3d"DEGREE"F Lid%3u";
const prog_char LCD_LINE1_UNPLUGGED[] PROGMEM = "- No Pit Probe -";
const prog_char LCD_LINE2[] PROGMEM = "%-12s%3d"DEGREE;

const prog_char LCD_SETPOINT1[] PROGMEM = "Set temperature:";
const prog_char LCD_SETPOINT2[] PROGMEM = "%3d"DEGREE"F";
const prog_char LCD_PROBENAME1[] PROGMEM = "Set probe %1d name";
const prog_char LCD_PROBEOFFSET2[] PROGMEM = "Offset %4d"DEGREE"F";
const prog_char LCD_LIDOPENOFFS1[] PROGMEM = "Lid open offset";
const prog_char LCD_LIDOPENOFFS2[] PROGMEM = "%3d"DEGREE"F";
const prog_char LCD_LIDOPENDUR1[] PROGMEM = "Lid open timer";
const prog_char LCD_LIDOPENDUR2[] PROGMEM = "%3d seconds";

#define eeprom_read_to(dst_p, eeprom_field, dst_size) eeprom_read_block(dst_p, (void *)offsetof(__eeprom_data, eeprom_field), MIN(dst_size, sizeof((__eeprom_data*)0)->eeprom_field))
#define eeprom_read(dst, eeprom_field) eeprom_read_to(&dst, eeprom_field, sizeof(dst))
#define eeprom_write_from(src_p, eeprom_field, src_size) eeprom_write_block(src_p, (void *)offsetof(__eeprom_data, eeprom_field), MIN(src_size, sizeof((__eeprom_data*)0)->eeprom_field))
#define eeprom_write(src, eeprom_field) { typeof(src) x = src; eeprom_write_from(&x, eeprom_field, sizeof(x)); }

#define EEPROM_MAGIC 0xf00d800

const struct PROGMEM __eeprom_data {
  long magic;
  int setPoint;
  char probeNames[TEMP_COUNT][13];
  char probeTempOffsets[TEMP_COUNT];
  unsigned char lidOpenOffset;
  unsigned int lidOpenDuration;
  float pidConstants[3]; // constants are stored Kp, Ki, Kd
} DEFAULT_CONFIG PROGMEM = { 
  EEPROM_MAGIC,  // magic
  230,  // setpoint
  { "Pit", "Food Probe1", "Food Probe2", "Ambient" },  // probe names
  { 0, 0, 0 },  // probe offsets
  20,  // lid open offset
  240, // lid open duration
  { 7.0f, 0.01f, 1.0f }
};

// Menu configuration parameters ------------------------
#define BUTTON_LEFT  (1<<0)
#define BUTTON_RIGHT (1<<1)
#define BUTTON_UP    (1<<2)
#define BUTTON_DOWN  (1<<3)
#define BUTTON_4     (1<<4)

//button_t readButton(void); // forward

#define ST_HOME_FOOD1 (ST_VMAX+1) // ST_HOME_X must stay sequential and in order
#define ST_HOME_FOOD2 (ST_VMAX+2)
#define ST_HOME_AMB   (ST_VMAX+3)
#define ST_CONNECTING (ST_VMAX+4)
#define ST_SETPOINT   (ST_VMAX+5)
#define ST_PROBENAME1 (ST_VMAX+6)  // ST_PROBENAMEX must stay sequential and in order
#define ST_PROBENAME2 (ST_VMAX+7)
#define ST_PROBENAME3 (ST_VMAX+8)
#define ST_PROBEOFF0  (ST_VMAX+9)  // ST_PROBEOFFX must stay sequential and in order
#define ST_PROBEOFF1  (ST_VMAX+10)
#define ST_PROBEOFF2  (ST_VMAX+11)
#define ST_PROBEOFF3  (ST_VMAX+12)
#define ST_LIDOPEN_OFF (ST_VMAX+13)
#define ST_LIDOPEN_DUR (ST_VMAX+14)
// #define ST_SAVECHANGES (ST_VMAX+14)

const menu_definition_t MENU_DEFINITIONS[] PROGMEM = {
  { ST_HOME_FOOD1, menuHome, 5 },
  { ST_HOME_FOOD2, menuHome, 5 },
  { ST_HOME_AMB, menuHome, 5 },
  { ST_CONNECTING, menuConnecting, 2 },
  { ST_SETPOINT, menuSetpoint, 10 },
  { ST_PROBENAME1, menuProbename, 10 },
  { ST_PROBENAME2, menuProbename, 10 },
  { ST_PROBENAME3, menuProbename, 10 },
  { ST_PROBEOFF0, menuProbeOffset, 10 },
  { ST_PROBEOFF1, menuProbeOffset, 10 },
  { ST_PROBEOFF2, menuProbeOffset, 10 },
  { ST_PROBEOFF3, menuProbeOffset, 10 },
  { ST_LIDOPEN_OFF, menuLidOpenOff, 10 },
  { ST_LIDOPEN_DUR, menuLidOpenDur, 10 },
  { 0, 0 },
};

const menu_transition_t MENU_TRANSITIONS[] PROGMEM = {
  { ST_HOME_FOOD1, BUTTON_DOWN | BUTTON_TIMEOUT, ST_HOME_FOOD2 },
  { ST_HOME_FOOD1, BUTTON_RIGHT,   ST_SETPOINT },
  { ST_HOME_FOOD1, BUTTON_UP,      ST_HOME_AMB },

  { ST_HOME_FOOD2, BUTTON_DOWN | BUTTON_TIMEOUT, ST_HOME_AMB },
  { ST_HOME_FOOD2, BUTTON_RIGHT,   ST_SETPOINT },
  { ST_HOME_FOOD2, BUTTON_UP,      ST_HOME_FOOD1 },

  { ST_HOME_AMB, BUTTON_DOWN | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_HOME_AMB, BUTTON_RIGHT,     ST_SETPOINT },
  { ST_HOME_AMB, BUTTON_UP,        ST_HOME_FOOD2 },

  { ST_SETPOINT, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_SETPOINT, BUTTON_RIGHT, ST_PROBENAME1 },
  // UP and DOWN are caught in handler

  { ST_PROBENAME1, BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  // UP, DOWN, LEFT, RIGHT are caught in handler
  { ST_PROBEOFF1, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBEOFF1, BUTTON_RIGHT, ST_PROBENAME2 },
  // UP, DOWN caught in handler
  
  { ST_PROBENAME2, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  // UP, DOWN, LEFT, RIGHT are caught in handler
  { ST_PROBEOFF2, BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBEOFF2, BUTTON_RIGHT, ST_PROBENAME3 },
  // UP, DOWN caught in handler

  { ST_PROBENAME3, BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  // UP, DOWN, LEFT, RIGHT are caught in handler
  { ST_PROBEOFF3, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBEOFF3, BUTTON_RIGHT, ST_PROBEOFF0 },
  // UP, DOWN caught in handler

  { ST_PROBEOFF0, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBEOFF0, BUTTON_RIGHT, ST_LIDOPEN_OFF },
  // UP, DOWN caught in handler
  
  { ST_LIDOPEN_OFF, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_LIDOPEN_OFF, BUTTON_RIGHT, ST_LIDOPEN_DUR },
  // UP, DOWN caught in handler

  { ST_LIDOPEN_DUR, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_LIDOPEN_DUR, BUTTON_RIGHT, ST_SETPOINT },
  // UP, DOWN caught in handler

  { ST_CONNECTING, BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  
  { 0, 0, 0 },
};

MenuSystem Menus(MENU_DEFINITIONS, MENU_TRANSITIONS, &readButton);
// End Menu configuration parameters ------------------------

void outputRaw(Print &out)
{
  int setPoint;
  
  out.print(g_TempAvgs[TEMP_PIT]);
  out.print(',');
  out.print(g_TempAvgs[TEMP_FOOD1]);
  out.print(',');
  out.print(g_TempAvgs[TEMP_FOOD2]);
  out.print(',');
  out.print(g_TempAvgs[TEMP_AMB]);
  out.print(',');
  out.print(fanSpeedPCT,DEC);
  out.print(',');
  out.print(round(fanSpeedAVG),DEC);
  eeprom_read(setPoint, setPoint);
  out.print(',');
  out.print(setPoint);
  
  out.println();
}

void storeProbeName(unsigned char probeIndex)
{
  void *ofs = &((__eeprom_data*)0)->probeNames[probeIndex];
  eeprom_write_block(ofs, editString, sizeof((__eeprom_data*)0)->probeNames[0]);
}

void loadProbeName(unsigned char probeIndex)
{
  void *ofs = &((__eeprom_data*)0)->probeNames[probeIndex];
  eeprom_read_block(editString, ofs, sizeof((__eeprom_data*)0)->probeNames[0]);
}

void updateDisplay(void)
{
  // Updates to the temperature can come at any time, only update 
  // if we're in a state that displays them
  if (Menus.State < ST_HOME_FOOD1 || Menus.State > ST_HOME_AMB)
    return;
  char buffer[17];

  // Fixed pit area
  lcd.home();
  if (g_TempAvgs[TEMP_PIT] == 0)
    memcpy_P(buffer, LCD_LINE1_UNPLUGGED, sizeof(LCD_LINE1_UNPLUGGED));
  else if (g_LidOpenResumeCountdown > 0)
    snprintf_P(buffer, sizeof(buffer), LCD_LINE1_DELAYING, g_TempAvgs[TEMP_PIT], g_LidOpenResumeCountdown);
  else
    snprintf_P(buffer, sizeof(buffer), LCD_LINE1, g_TempAvgs[TEMP_PIT], fanSpeedPCT);
  lcd.print(buffer); 

  // Rotating probe display
  unsigned char probeIndex = Menus.State - ST_HOME_FOOD1 + 1;
  loadProbeName(probeIndex);
  snprintf_P(buffer, sizeof(buffer), LCD_LINE2, editString, g_TempAvgs[probeIndex]);

  lcd.setCursor(0, 1);
  lcd.print(buffer);
}

/* Calucluate the desired fan speed using the proportional–integral (PI) controller algorithm */
unsigned char calcFanSpeedPct(int setPoint, int currentTemp) 
{
  static unsigned char lastOutput = 0;
  static int lastTemp = 0;
  
  // If the pit probe is registering 0 degrees, don't jack the fan up to MAX
  if (currentTemp == 0)
    return 0;

  float error;
  int control;
  error = setPoint - currentTemp;

  // anti-windup: Make sure we only adjust the I term while
  // inside the proportional control range
  if (!(lastOutput >= 100 && error > 0) && 
      !(lastOutput <= 0   && error < 0))
    pidErrorSum += (error * pidConstants[1]);
    
  control = pidConstants[0] * (error + pidErrorSum - (pidConstants[2] * (currentTemp - lastTemp)));
  
  if (control > 100)
    lastOutput = 100;
  else if (control < 0)
    lastOutput = 0;
  else
    lastOutput = control;
  lastTemp = currentTemp;

  return lastOutput;
}

void resetLidOpenResumeCountdown(void)
{
  unsigned int resume;
  eeprom_read(resume, lidOpenDuration);
  g_LidOpenResumeCountdown = resume;
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
    resetLidOpenResumeCountdown();
    g_TemperatureReached = false;
  }
  
  fanSpeedAVG = (0.99f * fanSpeedAVG) + (0.01f * fanSpeedPCT);
  analogWrite(PIN_BLOWER, (fanSpeedPCT * 255 / 100));

  updateDisplay();
  //outputRaw(Serial);
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
  
  // return degrees F
  int retVal = (int)((T - 273.15f) * (9.0f / 5.0f)) + 32; // 
  // Sanity - anything less than 0F or greater than 999F is rejected
  if (retVal < 0 || retVal > 999)
    return 0;
    
  return retVal;
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
    // reset the accumulator
    memset(g_TempAccum, 0, sizeof(g_TempAccum));
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
  eeprom_read(pidConstants, pidConstants);
}

state_t menuHome(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    if (Menus.State == ST_HOME_FOOD1 && g_TempAvgs[TEMP_FOOD1] == 0)
      return ST_HOME_FOOD2;
    else if (Menus.State == ST_HOME_FOOD2 && g_TempAvgs[TEMP_FOOD2] == 0)
      return ST_HOME_AMB;
    updateDisplay();
  }
  else if (button == BUTTON_LEFT)
  {
    // Left from Home screen enables/disables the lid countdown
    if (g_LidOpenResumeCountdown == 0)
      resetLidOpenResumeCountdown();
    else
      g_LidOpenResumeCountdown == 0;
    updateDisplay();
  }
  return ST_AUTO;
}

state_t menuConnecting(button_t button)
{
  char buffer[17];
  lcd.clear();
  lcd.print("Connecting to   "); 
  lcd.setCursor(0, 1);
//  strncpy_P(buffer, ssid, sizeof(buffer));
  lcd.print(buffer);
  return ST_AUTO;
}

void menuNumberEdit(button_t button, unsigned char increment, 
  const char *line1, const prog_char *format)
{
  char buffer[17];
  
  if (button == BUTTON_ENTER)
  {
    lcd.clear();
    lcd.print(line1);
  }
  else if (button == BUTTON_UP)
    editInt += increment;
  else if (button == BUTTON_DOWN)
    editInt -= increment;

  lcd.setCursor(0, 1);
  snprintf_P(buffer, sizeof(buffer), format, editInt);
  lcd.print(buffer);
}

boolean menuStringEdit(button_t button, const char *line1, unsigned char maxLength)
{
  static unsigned char editPos = 0;
  
  if (button == BUTTON_ENTER)
  {
    lcd.clear();
    lcd.print(line1);
  }
  else if (button == BUTTON_LEAVE)
  {
    editPos = 0;
    lcd.noBlink();
  }
  // Pressing UP or DOWN enters edit mode
  else if (editPos == 0 && (button == BUTTON_UP || button == BUTTON_DOWN))
    editPos = 1;
  // LEFT = cancel edit
  else if (editPos != 0 && button == BUTTON_LEFT)
  {
    --editPos;
    if (editPos == 0)
      return false;
  }
  // RIGHT = confirm edit
  else if (editPos != 0 && button == BUTTON_RIGHT)
  {
    ++editPos;
    if (editPos > maxLength)
      return true;
  }

  if (editPos > 0)
  {
    char c = editString[editPos - 1];
    if (c == '\0')
    {
      c = ' ';
      editString[editPos] = '\0';
    }
    else if (button == BUTTON_DOWN)
      --c;
    else if (button == BUTTON_UP)
      ++c;
    if (c < ' ') c = '~';
    if (c > '~') c = ' ';
    editString[editPos - 1] = c;  
  }  
  
  lcd.setCursor(0, 1);
  lcd.print(editString);

  if (editPos)
  {
    lcd.setCursor(editPos-1, 1);
    lcd.blink();
  }
  
  return false;
}

state_t menuSetpoint(button_t button)
{
  char buffer[17];
  
  if (button == BUTTON_ENTER)
  {
    strncpy_P(buffer, LCD_SETPOINT1, sizeof(buffer));
    eeprom_read(editInt, setPoint);
  }
  else if (button == BUTTON_LEAVE)
  {
    eeprom_write(editInt, setPoint);
  }

  menuNumberEdit(button, 5, buffer, LCD_SETPOINT2);
  return ST_AUTO;
}

state_t menuProbename(button_t button)
{
  char buffer[17];
  unsigned char probeIndex = Menus.State - ST_PROBENAME1 + 1;

  if (button == BUTTON_ENTER)
  {
    loadProbeName(probeIndex);
    snprintf_P(buffer, sizeof(buffer), LCD_PROBENAME1, probeIndex);
  }

  // note that we only load the buffer with text on the ENTER call,
  // after that it is OK to have garbage in it  
  if (menuStringEdit(button, buffer, sizeof((__eeprom_data*)0)->probeNames[0] - 1))
    storeProbeName(probeIndex);
    
  return ST_AUTO;
}

state_t menuProbeOffset(button_t button)
{
  unsigned char probeIndex = Menus.State - ST_PROBEOFF0;
  
  if (button == BUTTON_ENTER)
  {
    loadProbeName(probeIndex);
    editInt = probeTempOffsets[probeIndex];
  }
  else if (button == BUTTON_LEAVE)
  {
    uint8_t *ofs = (uint8_t *)&((__eeprom_data*)0)->probeTempOffsets[probeIndex];
    probeTempOffsets[probeIndex] = editInt;
    eeprom_write_byte(ofs, probeTempOffsets[probeIndex]);
  }

  menuNumberEdit(button, 1, editString, LCD_PROBEOFFSET2);
  return ST_AUTO;
}

state_t menuLidOpenOff(button_t button)
{
  char buffer[17];
  
  if (button == BUTTON_ENTER)
  {
    strncpy_P(buffer, LCD_LIDOPENOFFS1, sizeof(buffer));
    editInt = lidOpenOffset;
    //eeprom_read(editInt, lidOpenOffset);
  }
  else if (button == BUTTON_LEAVE)
  {
    if (editInt < 0)
      lidOpenOffset = 0;
    else
      lidOpenOffset = editInt;    
    eeprom_write(lidOpenOffset, lidOpenOffset);
  }

  menuNumberEdit(button, 5, buffer, LCD_LIDOPENOFFS2);
  return ST_AUTO;
}

state_t menuLidOpenDur(button_t button)
{
  unsigned int lidOpenDuration;
  char buffer[17];

  if (button == BUTTON_ENTER)
  {
    eeprom_read(lidOpenDuration, lidOpenDuration);
    editInt = lidOpenDuration;    
    strncpy_P(buffer, LCD_LIDOPENDUR1, sizeof(buffer));
  }
  else if (button == BUTTON_LEAVE)
  {
    if (editInt < 0)
      lidOpenDuration = 0;
    else
      lidOpenOffset = editInt;    
    eeprom_write(lidOpenDuration, lidOpenDuration);
  }

  menuNumberEdit(button, 10, buffer, LCD_LIDOPENDUR2);
  return ST_AUTO;
}

button_t readButton(void)
{
  unsigned char button = analogRead(PIN_BUTTONS) >> 2;
  if (button == 0)
    return BUTTON_NONE;

  //Serial.print("BtnRaw ");
  //Serial.println(button, DEC); 

  if (button > 20 && button < 60)
    return BUTTON_LEFT;  
  if (button > 60 && button < 100)
    return BUTTON_DOWN;  
  if (button > 140 && button < 160)
    return BUTTON_UP;  
  if (button > 160 && button < 200)
    return BUTTON_RIGHT;  
    
  return BUTTON_NONE;
}

boolean sendPage(char* URL)
{
  if (strncmp(URL, "/set?sp=", 8) == 0) 
  {
    int setPoint;
    setPoint = atoi(URL + 8);
    eeprom_write(setPoint, setPoint);
    WiServer.print("OK");
    return true;
  }
  if (strcmp(URL, "/data") == 0) 
  {
    outputRaw(WiServer);
    return true;    
  }
  
  return false;
}

void setup(void)
{
  Serial.begin(57600);

  long magic;
  eeprom_read(magic, magic);
  if(magic != EEPROM_MAGIC)
    eepromInitialize();
  eepromCache();

  g_NetworkInitialized = readButton() == BUTTON_NONE;
  if (g_NetworkInitialized)  
  {
    Menus.setState(ST_CONNECTING);
    WiServer.init(sendPage);
  }
  else
    Menus.setState(ST_HOME_AMB);
}

void loop(void)
{
  unsigned long time = millis();
  readTemps();
  Menus.doWork();

  if (g_NetworkInitialized)
    WiServer.server_task(); 
  
  time -= millis(); 
  if (time < (1000 >> TEMP_AVG_COUNT_LOG2))
    delay((1000 >> TEMP_AVG_COUNT_LOG2) - time);
}

