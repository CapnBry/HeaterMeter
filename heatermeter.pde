#define HEATERMETER_NETWORKING  // comment this out to remove networking

#include <WProgram.h>
#include <avr/eeprom.h>
#include <ShiftRegLCD.h>

#include "strings.h"
#include "menus.h"
#include "grillpid.h"
#include "flashfiles.h"

// Analog Pins
#define PIN_PIT     5
#define PIN_FOOD1   4
#define PIN_FOOD2   3
#define PIN_AMB     2
#define PIN_BUTTONS 0
// Digital Output Pins
#define PIN_BLOWER       3
#define PIN_LCD_CLK      4
#define PIN_DATAFLASH_SS 7
#define PIN_LCD_DATA     8
#define PIN_WIFI_SS     10

const struct steinhart_param STEINHART[] = {
  {2.3067434e-4f, 2.3696596e-4f, 1.2636414e-7f},  // Maverick Probe
  {8.98053228e-4f, 2.49263324e-4f, 2.04047542e-7f}, // Radio Shack 10k
};

static TempProbe probe0(PIN_PIT,   &STEINHART[0]);
static TempProbe probe1(PIN_FOOD1, &STEINHART[0]);
static TempProbe probe2(PIN_FOOD2, &STEINHART[0]);
static TempProbe probe3(PIN_AMB,   &STEINHART[1]);
static GrillPid pid(PIN_BLOWER);

#ifdef HEATERMETER_NETWORKING
#include <WiServer.h>  
#include <dataflash.h>
// Wireless configuration parameters ----------------------------------------
unsigned char local_ip[] = {192,168,1,252};	// IP address of WiShield
unsigned char gateway_ip[] = {192,168,1,1};	// router or gateway IP address
unsigned char subnet_mask[] = {255,255,255,0};	// subnet mask for the local network
const prog_char ssid[] PROGMEM = {"M75FE"};		// max 32 bytes

unsigned char security_type = 1;	// 0 - open; 1 - WEP; 2 - WPA; 3 - WPA2
// WPA/WPA2 passphrase
const prog_char security_passphrase[] PROGMEM = {""};	// max 64 characters
// WEP 128-bit key, only key 1 because that's all we use
prog_uchar wep_keys[] PROGMEM = { 0xEC, 0xA8, 0x1A, 0xB4, 0x65, 0xf0, 0x0d, 0xbe, 0xef, 0xde, 0xad, 0x00, 0x00};

// setup the wireless mode
// infrastructure - connect to AP
// adhoc - connect to another WiFi device
unsigned char wireless_mode = WIRELESS_MODE_INFRA;

unsigned char ssid_len;
unsigned char security_passphrase_len;
// End of wireless configuration parameters ----------------------------------------
static boolean g_NetworkInitialized;
#endif /* HEATERMETER_NETWORKING */

// scratch space for edits
static int editInt;  
static char editString[17];

static ShiftRegLCD lcd(PIN_LCD_DATA, PIN_LCD_CLK, TWO_WIRE, 2); 

#define MIN(x,y) ( x > y ? y : x )
#define eeprom_read_to(dst_p, eeprom_field, dst_size) eeprom_read_block(dst_p, (void *)offsetof(__eeprom_data, eeprom_field), MIN(dst_size, sizeof((__eeprom_data*)0)->eeprom_field))
#define eeprom_read(dst, eeprom_field) eeprom_read_to(&dst, eeprom_field, sizeof(dst))
#define eeprom_write_from(src_p, eeprom_field, src_size) eeprom_write_block(src_p, (void *)offsetof(__eeprom_data, eeprom_field), MIN(src_size, sizeof((__eeprom_data*)0)->eeprom_field))
#define eeprom_write(src, eeprom_field) { typeof(src) x = src; eeprom_write_from(&x, eeprom_field, sizeof(x)); }

#define EEPROM_MAGIC 0xf00d800
#define PROBE_NAME_SIZE 13

const struct PROGMEM __eeprom_data {
  long magic;
  int setPoint;
  char probeNames[TEMP_COUNT][PROBE_NAME_SIZE];
  char probeTempOffsets[TEMP_COUNT];
  unsigned char lidOpenOffset;
  unsigned int lidOpenDuration;
  float pidConstants[4]; // constants are stored Kb, Kp, Ki, Kd
  boolean manualMode;
  unsigned char maxFanSpeed;  // in percent
} DEFAULT_CONFIG PROGMEM = { 
  EEPROM_MAGIC,  // magic
  225,  // setpoint
  { "Pit", "Food Probe1", "Food Probe2", "Ambient" },  // probe names
  { 0, 0, 0 },  // probe offsets
  15,  // lid open offset
  240, // lid open duration
  { 5.0f, 4.0f, 0.002f, 4.0f },
  false, // manual mode
  100  // max fan speed
};

// Menu configuration parameters ------------------------
#define BUTTON_LEFT  (1<<0)
#define BUTTON_RIGHT (1<<1)
#define BUTTON_UP    (1<<2)
#define BUTTON_DOWN  (1<<3)
#define BUTTON_4     (1<<4)

#define ST_HOME_FOOD1 (ST_VMAX+1) // ST_HOME_X must stay sequential and in order
#define ST_HOME_FOOD2 (ST_VMAX+2)
#define ST_HOME_AMB   (ST_VMAX+3)
#define ST_HOME_NOPROBES (ST_VMAX+4)
#define ST_CONNECTING (ST_VMAX+5)
#define ST_SETPOINT   (ST_VMAX+6)
#define ST_PROBENAME1 (ST_VMAX+7)  // ST_PROBENAMEX must stay sequential and in order
#define ST_PROBENAME2 (ST_VMAX+8)
#define ST_PROBENAME3 (ST_VMAX+9)
#define ST_PROBEOFF0  (ST_VMAX+10) // ST_PROBEOFFX must stay sequential and in order
#define ST_PROBEOFF1  (ST_VMAX+11)
#define ST_PROBEOFF2  (ST_VMAX+12)
#define ST_PROBEOFF3  (ST_VMAX+13)
#define ST_LIDOPEN_OFF (ST_VMAX+14)
#define ST_LIDOPEN_DUR (ST_VMAX+15)
#define ST_MANUALMODE  (ST_VMAX+16)
#define ST_RESETCONFIG (ST_VMAX+17)
#define ST_MAXFANSPEED (ST_VMAX+18)

const menu_definition_t MENU_DEFINITIONS[] PROGMEM = {
  { ST_HOME_FOOD1, menuHome, 5 },
  { ST_HOME_FOOD2, menuHome, 5 },
  { ST_HOME_AMB, menuHome, 5 },
  { ST_HOME_NOPROBES, menuHome, 1 },
  { ST_CONNECTING, menuConnecting, 2 },
  { ST_SETPOINT, menuSetpoint, 10 },
  { ST_MANUALMODE, menuManualMode, 10 },
  { ST_PROBENAME1, menuProbename, 10 },
  { ST_PROBENAME2, menuProbename, 10 },
  { ST_PROBENAME3, menuProbename, 10 },
  { ST_PROBEOFF0, menuProbeOffset, 10 },
  { ST_PROBEOFF1, menuProbeOffset, 10 },
  { ST_PROBEOFF2, menuProbeOffset, 10 },
  { ST_PROBEOFF3, menuProbeOffset, 10 },
  { ST_LIDOPEN_OFF, menuLidOpenOff, 10 },
  { ST_LIDOPEN_DUR, menuLidOpenDur, 10 },
  { ST_RESETCONFIG, menuResetConfig, 10 },
  { ST_MAXFANSPEED, menuMaxFanSpeed, 10 },
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

  { ST_HOME_NOPROBES, BUTTON_DOWN | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_HOME_NOPROBES, BUTTON_RIGHT,ST_SETPOINT },
  { ST_HOME_NOPROBES, BUTTON_UP,   ST_HOME_AMB },

  { ST_SETPOINT, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_SETPOINT, BUTTON_RIGHT, ST_MANUALMODE },
  // UP and DOWN are caught in handler

  { ST_MANUALMODE, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_MANUALMODE, BUTTON_RIGHT, ST_MAXFANSPEED },
  // UP and DOWN are caught in handler
  
  { ST_MAXFANSPEED, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_MAXFANSPEED, BUTTON_RIGHT, ST_PROBENAME1 },
  // UP and DOWN are caught in handler

  { ST_PROBENAME1, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBENAME1, BUTTON_RIGHT, ST_PROBEOFF1 },
  // UP, DOWN caught in handler
  { ST_PROBEOFF1, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBEOFF1, BUTTON_RIGHT, ST_PROBENAME2 },
  // UP, DOWN caught in handler
  
  { ST_PROBENAME2, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBENAME2, BUTTON_RIGHT, ST_PROBEOFF2 },
  // UP, DOWN caught in handler
  { ST_PROBEOFF2, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBEOFF2, BUTTON_RIGHT, ST_PROBENAME3 },
  // UP, DOWN caught in handler

  { ST_PROBENAME3, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBENAME3, BUTTON_RIGHT, ST_PROBEOFF3 },
  // UP, DOWN caught in handler
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
  { ST_LIDOPEN_DUR, BUTTON_RIGHT, ST_RESETCONFIG },
  // UP, DOWN caught in handler
  
  { ST_RESETCONFIG, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_RESETCONFIG, BUTTON_RIGHT, ST_SETPOINT },
  // UP, DOWN caught in handler

  { ST_CONNECTING, BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  
  { 0, 0, 0 },
};

MenuSystem Menus(MENU_DEFINITIONS, MENU_TRANSITIONS, &readButton);
// End Menu configuration parameters ------------------------

boolean storeProbeName(unsigned char probeIndex, const char *name)
{
  if (probeIndex >= TEMP_COUNT)
    return false;
    
  void *ofs = &((__eeprom_data*)0)->probeNames[probeIndex];
  eeprom_write_block(name, ofs, PROBE_NAME_SIZE);
  return true;
}

void loadProbeName(unsigned char probeIndex)
{
  void *ofs = &((__eeprom_data*)0)->probeNames[probeIndex];
  eeprom_read_block(editString, ofs, PROBE_NAME_SIZE);
}

void storeSetPoint(int sp)
{
  // If the setpoint is >0 that's an actual setpoint.  
  // 0 or less is a manual fan speed
  if (sp > 0)
  {
    eeprom_write(sp, setPoint);
    eeprom_write(false, manualMode);
    pid.setSetPoint(sp);
  }
  else
  {
    eeprom_write(true, manualMode);
    pid.setFanSpeed(-sp);
  }
}

boolean storeProbeOffset(unsigned char probeIndex, char offset)
{
  if (probeIndex >= TEMP_COUNT)
    return false;
    
  uint8_t *ofs = (uint8_t *)&((__eeprom_data*)0)->probeTempOffsets[probeIndex];
  pid.Probes[probeIndex]->Offset = offset;
  eeprom_write_byte(ofs, offset);
  
  return true;
}

void storeMaxFanSpeed(unsigned char maxFanSpeed)
{
  pid.MaxFanSpeed = maxFanSpeed;
  eeprom_write(maxFanSpeed, maxFanSpeed);
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
  int pitTemp = pid.Probes[TEMP_PIT]->Temperature;
  if (!pid.getManualFanMode() && pitTemp == 0)
    memcpy_P(buffer, LCD_LINE1_UNPLUGGED, sizeof(LCD_LINE1_UNPLUGGED));
  else if (pid.LidOpenResumeCountdown > 0)
    snprintf_P(buffer, sizeof(buffer), LCD_LINE1_DELAYING, pitTemp, pid.LidOpenResumeCountdown);
  else
  {
    char c1,c2;
    if (pid.getManualFanMode())
    {
      c1 = '^';  // LCD_ARROWUP
      c2 = '^';  // LCD_ARROWDN
    }
    else
    {
      c1 = '[';
      c2 = ']';
    }
    snprintf_P(buffer, sizeof(buffer), LCD_LINE1, pitTemp, c1, pid.getFanSpeed(), c2);
  }
  lcd.print(buffer); 

  // Rotating probe display
  unsigned char probeIndex = Menus.State - ST_HOME_FOOD1 + 1;
  if (probeIndex < TEMP_COUNT)
  {
    loadProbeName(probeIndex);
    snprintf_P(buffer, sizeof(buffer), LCD_LINE2, editString, (int)pid.Probes[probeIndex]->Temperature);
  }
  else
  {
    // If probeIndex is outside the range (in the case of ST_HOME_NOPROBES)
    // just fill the bottom line with spaces
    memset(buffer, ' ', sizeof(buffer));
    buffer[sizeof(buffer-1)] = 0;
  }

  lcd.setCursor(0, 1);
  lcd.print(buffer);
}

state_t menuHome(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    if (!pid.isAnyFoodProbeActive())
      return ST_HOME_NOPROBES;
    else if (Menus.State == ST_HOME_FOOD1 && pid.Probes[TEMP_FOOD1]->Temperature == 0)
      return ST_HOME_FOOD2;
    else if (Menus.State == ST_HOME_FOOD2 && pid.Probes[TEMP_FOOD2]->Temperature == 0)
      return ST_HOME_AMB;
    else if (Menus.State == ST_HOME_AMB && pid.Probes[TEMP_AMB]->Temperature == 0)
      return ST_HOME_FOOD1;
        
    updateDisplay();
  }
  // In manual fan mode Up is +5% Down is -5% and Left is -1%
  else if (pid.getManualFanMode())
  {
    char offset;
    if (button == BUTTON_UP)
      offset = 5;
    else if (button == BUTTON_DOWN)
      offset = -5;
    else if (button == BUTTON_LEFT)
      offset = -1;
    else
      return ST_AUTO;
  
    pid.setFanSpeed(pid.getFanSpeed() + offset);
    updateDisplay();
    return ST_NONE;
  }
  else if (button == BUTTON_LEFT)
  {
    // Left from Home screen enables/disables the lid countdown
    if (pid.LidOpenResumeCountdown == 0)
      pid.resetLidOpenResumeCountdown();
    else
      pid.LidOpenResumeCountdown = 0;
    updateDisplay();
  }
  return ST_AUTO;
}

void lcdprint_P(const prog_char *p, const boolean doClear)
{
  char buffer[17];
  strncpy_P(buffer, p, sizeof(buffer));

  if (doClear)
    lcd.clear();
  lcd.print(buffer);
}

state_t menuConnecting(button_t button)
{
#ifdef HEATERMETER_NETWORKING
  lcdprint_P(LCD_CONNECTING, true); 
  lcd.setCursor(0, 1);
  lcdprint_P(ssid, false);
#endif /* HEATERMETER_NETWORKING */

  return ST_AUTO;
}

void menuBooleanEdit(button_t button)
{
  if (button == BUTTON_UP || button == BUTTON_DOWN)
    editInt = !editInt;

  lcd.setCursor(0, 1);
  lcdprint_P((editInt != 0) ? LCD_YES : LCD_NO, false);
}

void menuNumberEdit(button_t button, unsigned char increment, 
  const prog_char *format)
{
  char buffer[17];
  
  if (button == BUTTON_UP)
    editInt += increment;
  else if (button == BUTTON_DOWN)
    editInt -= increment;

  lcd.setCursor(0, 1);
  snprintf_P(buffer, sizeof(buffer), format, editInt);
  lcd.print(buffer);
}

/* 
  menuStringEdit - When entering a string edit, the first line is static text, 
  the second is editString.  Upon entry, the string is in read-only mode.  
  If the user presses the UP or DOWN button, the editing is now active indicated
  by a blinking character at the current edit position.  From here the user can 
  use the UP and DOWN button to change the currently selected letter, Arcade Style.
  LEFT and RIGHT are now repurposed to navigating the edit control.  If the user
  scrolls off the left, this is considered a cancel.  Scrolling right to maxLength
  indicates the caller should commit the data.
  Return value: 
    ST_AUTO - Not in edit mode, continue as normal *or* user cancelled the edit
    ST_NONE - In edit mode, buttons are being eaten by edit navigation
    (State) - If the edit is completed and the caller should commit the new value
              the current Menu State is returned. The menu will return to read-only state
*/            
state_t menuStringEdit(button_t button, const char *line1, unsigned char maxLength)
{
  static unsigned char editPos = 0;

  if (button == BUTTON_TIMEOUT)
    return ST_AUTO;
  if (button == BUTTON_LEAVE)
    lcd.noBlink();
  else if (button == BUTTON_ENTER)
  {
    lcd.clear();
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(editString);
  }
  // Pressing UP or DOWN enters edit mode
  else if (editPos == 0 && (button & (BUTTON_UP | BUTTON_DOWN)))
  {
    editPos = 1;
    lcd.blink();
  }
  // LEFT = cancel edit
  else if (editPos != 0 && button == BUTTON_LEFT)
  {
    --editPos;
    if (editPos == 0)
    {
      lcd.noBlink();
      return ST_AUTO;
    }
  }
  // RIGHT = confirm edit
  else if (editPos != 0 && button == BUTTON_RIGHT)
  {
    ++editPos;
    if (editPos > maxLength)
    {
      editPos = 0;
      lcd.noBlink();
      return Menus.State;
    }
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
    if (c < ' ') c = '}';
    if (c > '}') c = ' ';
    editString[editPos - 1] = c;  
    lcd.setCursor(editPos-1, 1);
    lcd.print(c);
    lcd.setCursor(editPos-1, 1);

    return ST_NONE;
  }  
  
  return ST_AUTO;
}

state_t menuSetpoint(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(LCD_SETPOINT1, true);
    editInt = pid.getSetPoint();
  }
  else if (button == BUTTON_LEAVE)
  {
    if (editInt != pid.getSetPoint())
      storeSetPoint(editInt);
  }

  menuNumberEdit(button, 5, LCD_SETPOINT2);
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
  state_t retVal = menuStringEdit(button, buffer, PROBE_NAME_SIZE - 1);
  if (retVal == Menus.State)
    storeProbeName(probeIndex, editString);
    
  return retVal;
}

state_t menuProbeOffset(button_t button)
{
  unsigned char probeIndex = Menus.State - ST_PROBEOFF0;
  
  if (button == BUTTON_ENTER)
  {
    loadProbeName(probeIndex);
    lcd.clear();
    lcd.print(editString);
    editInt = pid.Probes[probeIndex]->Offset;
  }
  else if (button == BUTTON_LEAVE)
    storeProbeOffset(probeIndex, editInt);

  menuNumberEdit(button, 1, LCD_PROBEOFFSET2);
  return ST_AUTO;
}

state_t menuLidOpenOff(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(LCD_LIDOPENOFFS1, true);
    editInt = pid.LidOpenOffset;
  }
  else if (button == BUTTON_LEAVE)
  {
    if (editInt < 0)
      pid.LidOpenOffset = 0;
    else
      pid.LidOpenOffset = editInt;    
    eeprom_write(pid.LidOpenOffset, lidOpenOffset);
  }

  menuNumberEdit(button, 5, LCD_LIDOPENOFFS2);
  return ST_AUTO;
}

state_t menuLidOpenDur(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(LCD_LIDOPENDUR1, true);
    editInt = pid.LidOpenDuration;    
  }
  else if (button == BUTTON_LEAVE)
  {
    if (editInt < 0)
      pid.LidOpenDuration = 0;
    else
      pid.LidOpenDuration = editInt;    
    eeprom_write(pid.LidOpenDuration, lidOpenDuration);
  }

  menuNumberEdit(button, 10, LCD_LIDOPENDUR2);
  return ST_AUTO;
}

state_t menuManualMode(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(LCD_MANUALMODE, true);
    editInt = pid.getManualFanMode() ? !0 : 0;    
  }
  else if (button == BUTTON_LEAVE)
  {
    boolean manual = (editInt != 0); 
    if (manual != pid.getManualFanMode())
      storeSetPoint(manual ? 0 : pid.getSetPoint());
  }
  menuBooleanEdit(button);
  return ST_AUTO;
}

state_t menuResetConfig(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(LCD_RESETCONFIG, true);
    editInt = 0;
  }
  else if (button == BUTTON_LEAVE)
  {
    if (editInt != 0)
      eepromLoadConfig(true);
  }
  menuBooleanEdit(button);
  return ST_AUTO;
}

state_t menuMaxFanSpeed(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(LCD_MAXFANSPEED, true);
    editInt = pid.MaxFanSpeed;
  }
  else if (button == BUTTON_LEAVE)
  {
    if (editInt != pid.MaxFanSpeed)
      storeMaxFanSpeed(editInt);
  }
  
  menuNumberEdit(button, 5, LCD_MAXFANSPEED2);
  return ST_AUTO;
}

boolean storePidParam(char which, float Value)
{
  const prog_char *pos = strchr_P(PID_ORDER, which);
  if (pos == NULL)
    return false;
    
  const unsigned char k = pos - PID_ORDER;
  pid.Pid[k] = Value;
  
  uint8_t *ofs = (uint8_t *)&((__eeprom_data*)0)->pidConstants[k];
  eeprom_write_block(&pid.Pid[k], ofs, sizeof(Value));

  return true;
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

#ifdef HEATERMETER_NETWORKING

struct temp_log_record {
  unsigned int temps[TEMP_COUNT]; 
  unsigned char fan;
  unsigned char fan_avg;
};

#define RING_POINTER_INC(x) x = (x + 1) % ((DATAFLASH_PAGE_BYTES / sizeof(struct temp_log_record)) - 1)

void flashRingBufferInit(void)
{
  /* A simple ring buffer in the dflash buffer page, the first "record" is reserved 
     to store the head and tail indexes ((index+1) * size = addr)
     as well as a "write lock".  Because the web server may take several seconds
     to serve the entire log, we stop logging during that time to keep the data
     consistent across the entire page dispatch 
     */
  unsigned char dummy[sizeof(struct temp_log_record)];
  memset(dummy, 0, sizeof(dummy));
  
  // The first record is actually (uint8_t head, uint8_t tail, uint32_t writestart) but 
  // this is just to initialize it all to 0
  dflash.Buffer_Write_Str(1, 0, sizeof(dummy), dummy);
  dflash.DF_CS_inactive();
}

void flashRingBufferWrite(struct temp_log_record *p)
{
  unsigned char head = dflash.Buffer_Read_Byte(1, 0);
  unsigned char tail = dflash.Buffer_Read_Byte(1, 1);

  unsigned int addr = (tail + 1) * sizeof(*p);
  dflash.Buffer_Write_Str(1, addr, sizeof(*p), (unsigned char *)p);
  RING_POINTER_INC(tail);
  dflash.Buffer_Write_Byte(1, 1, tail);
  
  if (tail == head)
  {
    RING_POINTER_INC(head);
    dflash.Buffer_Write_Byte(1, 0, head);
  }
  
  dflash.DF_CS_inactive();
}

void storeTemps(void)
{
  struct temp_log_record temp_log;
  unsigned char i;
  for (i=0; i<TEMP_COUNT; i++)
  {
    // Store the difference between the temp and the average in the high 7 bits
    // This allows the temperature to be between 0-511 and the average to be 
    // within 63 degrees of that
    char avgOffset = (char)(pid.Probes[i]->Temperature - pid.Probes[i]->TemperatureAvg);
    temp_log.temps[i] = (avgOffset << 9) | (int)pid.Probes[i]->Temperature;
  }
  temp_log.fan = pid.getFanSpeed();
  temp_log.fan_avg = (unsigned char)pid.FanSpeedAvg;
  
  flashRingBufferWrite(&temp_log);
}

#define HTTP_HEADER_LENGTH 19 // "HTTP/1.0 200 OK\r\n\r\n"
void sendFlashFile(const struct flash_file_t *file)
{
  // Note we mess with the underlying UIP stack to prevent reading the entire
  // file each time from flash just to discard all but 300 bytes of it
  // Speeds up an 11kB send by approximately 3x (up to 1.5KB/sec)
  uip_tcp_appstate_t *app = &(uip_conn->appstate);
  unsigned int sentBytes = app->ackedCount;

  // The first time through, the buffer contains the header but nothing is acked yet
  // so don't mess with any of the state, just send the first segment  
  if (app->ackedCount > 0)
  {
   app->cursor = (char *)sentBytes;
   sentBytes -= HTTP_HEADER_LENGTH;
  }

  unsigned int page = pgm_read_word(&file->page) + (sentBytes / DATAFLASH_PAGE_BYTES);
  unsigned int off = sentBytes % DATAFLASH_PAGE_BYTES;
  unsigned int size = pgm_read_word(&file->size);
  unsigned int sendSize = size - sentBytes;

  if (sendSize > UIP_TCP_MSS)
    sendSize = UIP_TCP_MSS;
   
  dflash.Cont_Flash_Read_Enable(page, off);
  while (sendSize-- > 0)
    WiServer.write(dflash.Cont_Flash_Read());
  dflash.DF_CS_inactive();
  
  // Pretend that we've sent the whole file
  app->cursor = (char *)(HTTP_HEADER_LENGTH + size);
}

void outputLog(void)
{
  unsigned char head = dflash.Buffer_Read_Byte(1, 0);
  unsigned char tail = dflash.Buffer_Read_Byte(1, 1);
  
  while (head != tail)
  {
    struct temp_log_record p;
    unsigned int addr = (head + 1) * sizeof(p);
    dflash.Buffer_Read_Str(1, addr, sizeof(p), (unsigned char *)&p);
    RING_POINTER_INC(head);
    
    char offset;
    int temp;
    unsigned char i;
    for (i=0; i<TEMP_COUNT; i++)
    {
      temp = p.temps[i] & 0x1ff;
      WiServer.print(temp,DEC);  // temperature
      WiServer.print_P(COMMA);
      offset = p.temps[i] >> 9;
      WiServer.print(temp + offset,DEC);  // average
      WiServer.print_P(COMMA);
    }
    
    WiServer.print(p.fan,DEC);
    WiServer.print_P(COMMA);
    WiServer.println(p.fan_avg,DEC);
  }  
  dflash.DF_CS_inactive();
}

void outputCsv(void)
{
  WiServer.print(pid.getSetPoint());
  WiServer.print_P(COMMA);

  unsigned char i;
  for (i=0; i<TEMP_COUNT; i++)
  {
    WiServer.print((double)pid.Probes[i]->Temperature, 1);
    WiServer.print_P(COMMA);
  }

  WiServer.print(pid.getFanSpeed(),DEC);
  WiServer.print_P(COMMA);
  WiServer.print(round(pid.FanSpeedAvg),DEC);
  WiServer.print_P(COMMA);
  WiServer.print(pid.LidOpenResumeCountdown, DEC);
}

void outputJson(void)
{
  WiServer.print_P(JSON1);

  unsigned char i;
  for (i=0; i<TEMP_COUNT; i++)
  {
    WiServer.print_P(JSON_T1);
    loadProbeName(i);
    WiServer.print(editString);
    WiServer.print_P(JSON_T2);
    WiServer.print(pid.Probes[i]->Temperature, 1);
    WiServer.print_P(JSON_T3);
    WiServer.print(pid.Probes[i]->TemperatureAvg, 2);
    WiServer.print_P(JSON_T4);
  }
  
  WiServer.print_P(JSON2);
  WiServer.print(pid.getSetPoint(),DEC);
  WiServer.print_P(JSON3);
  WiServer.print(pid.LidOpenResumeCountdown,DEC);
  WiServer.print_P(JSON4);
  WiServer.print(pid.getFanSpeed(),DEC);
  WiServer.print_P(JSON5);
  WiServer.print((unsigned char)pid.FanSpeedAvg,DEC);
  WiServer.print_P(JSON6);
}

boolean sendPage(char* URL)
{
  ++URL;  // WARNING: URL no longer has leading '/'
  unsigned char urlLen = strlen(URL);
  
  if (strcmp_P(URL, URL_JSON) == 0) 
  {
    outputJson();
    return true;    
  }
  if (strcmp_P(URL, URL_CSV) == 0) 
  {
    outputCsv();
    return true;    
  }
  if (strcmp_P(URL, URL_LOG) == 0) 
  {
    outputLog();
    return true;    
  }
  if (strncmp_P(URL, URL_SETPOINT, 7) == 0) 
  {
    storeSetPoint(atoi(URL + 7));
    WiServer.print_P(WEB_OK);
    return true;
  }
  if (strncmp_P(URL, URL_SETPID, 7) == 0 && urlLen > 9) 
  {
    float f = atof(URL + 9);
    if (storePidParam(URL[7], f))
      WiServer.print_P(WEB_OK);
    else
      WiServer.print_P(WEB_FAILED);
    return true;
  }
  if (strncmp_P(URL, URL_SETPNAME, 6) == 0 && urlLen > 8) 
  {
    if (storeProbeName(URL[6] - '0', URL + 8))
      WiServer.print_P(WEB_OK);
    else
      WiServer.print_P(WEB_FAILED);
    return true;
  }
  if (strncmp_P(URL, URL_SETPOFF, 6) == 0 && urlLen > 8) 
  {
    if (storeProbeOffset(URL[6] - '0', atoi(URL + 8)))
      WiServer.print_P(WEB_OK);
    else
      WiServer.print_P(WEB_FAILED);
    return true;
  }
  if (strcmp(URL, "p") == 0) 
  {
    WiServer.print((double)pid._pidErrorSum, 3);
    return true;    
  }
  
  const struct flash_file_t *file = FLASHFILES;
  while (pgm_read_word(&file->fname))
  {
    if (strcmp_P(URL, (const prog_char *)pgm_read_word(&file->fname)) == 0)
    {
      sendFlashFile(file);
      return true;
    }
    ++file;
  }
  
  return false;
}
#endif /* HEATERMETER_NETWORKING */

void eepromLoadConfig(boolean forceDefault)
{
  struct __eeprom_data config;
  eeprom_read_block(&config, 0, sizeof(config));
  if (forceDefault || config.magic != EEPROM_MAGIC)
  {
    memcpy_P(&config, &DEFAULT_CONFIG, sizeof(config));
    eeprom_write_block(&config, 0, sizeof(config));  
  }

  unsigned char i;
  for (i=0; i<TEMP_COUNT; i++)
    pid.Probes[i]->Offset = config.probeTempOffsets[i];
    
  pid.setSetPoint(config.setPoint);
  pid.LidOpenOffset = config.lidOpenOffset;
  pid.LidOpenDuration = config.lidOpenDuration;
  pid.MaxFanSpeed = config.maxFanSpeed;
  memcpy(pid.Pid, config.pidConstants, sizeof(config.pidConstants));
  if (config.manualMode)
    pid.setFanSpeed(0);
}

void setup(void)
{
  Serial.begin(115200);
  
  pid.Probes[TEMP_PIT] = &probe0;
  pid.Probes[TEMP_FOOD1] = &probe1;
  pid.Probes[TEMP_FOOD2] = &probe2;
  pid.Probes[TEMP_AMB] = &probe3;

  eepromLoadConfig(false);
  
#ifdef HEATERMETER_NETWORKING
  // Set the WiFi Slave Select to HIGH (disable) to
  // prevent it from interferring with the dflash init
  pinMode(PIN_WIFI_SS, OUTPUT);
  digitalWrite(PIN_WIFI_SS, HIGH);
  dflash.init(PIN_DATAFLASH_SS);
  flashRingBufferInit();
  
  g_NetworkInitialized = readButton() == BUTTON_NONE;
  if (g_NetworkInitialized)  
  {
    Menus.setState(ST_CONNECTING);
    WiServer.init(sendPage);
  }
  else
#endif  /* HEATERMETER_NETWORKING */
    Menus.setState(ST_HOME_AMB);
}

void loop(void)
{
  Menus.doWork();
  if (pid.doWork())
  {
    updateDisplay();
#ifdef HEATERMETER_NETWORKING
    //storeTemps();
  }
  if (g_NetworkInitialized)
    WiServer.server_task(); 
#else
  }
#endif /* HEATERMETER_NETWORKING */
}

