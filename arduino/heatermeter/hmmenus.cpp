// HeaterMeter Copyright 2016 Bryan Mayland <bmayland@capnbry.net>
#include "hmcore.h"
#include "grillpid.h"
#include "strings.h"
#include "floatprint.h"
#include "bigchars.h"

static state_t menuHome(button_t button);
static state_t menuSetpoint(button_t button);
static state_t menuProbename(button_t button);
static state_t menuProbeOffset(button_t button);
static state_t menuLidOpenOff(button_t button);
static state_t menuLidOpenDur(button_t button);
static state_t menuManualMode(button_t button);
static state_t menuResetConfig(button_t button);
static state_t menuMaxFanSpeed(button_t button);
static state_t menuAlarmTriggered(button_t button);
static state_t menuLcdBacklight(button_t button);
static state_t menuToast(button_t button);
static state_t menuProbeDiag(button_t button);

static const menu_definition_t MENU_DEFINITIONS[] PROGMEM = {
  { ST_HOME_FOOD1, menuHome, 5, BUTTON_LEFT },
  { ST_HOME_FOOD2, menuHome, 5, BUTTON_LEFT },
  { ST_HOME_FOOD3, menuHome, 5, BUTTON_LEFT },
  { ST_HOME_NOPROBES, menuHome, 1, BUTTON_LEFT }, // Both No Pit Probe AND Pit with no food probes
  { ST_HOME_ALARM, menuAlarmTriggered, 0 },
  { ST_SETPOINT, menuSetpoint, 10 },
  { ST_MANUALMODE, menuManualMode, 10, BUTTON_LEFT },
  { ST_PROBEOFF0, menuProbeOffset, 10 },
  { ST_PROBEOFF1, menuProbeOffset, 10 },
  { ST_PROBEOFF2, menuProbeOffset, 10 },
  { ST_PROBEOFF3, menuProbeOffset, 10 },
  { ST_LIDOPEN_OFF, menuLidOpenOff, 10 },
  { ST_LIDOPEN_DUR, menuLidOpenDur, 10 },
  { ST_RESETCONFIG, menuResetConfig, 10 },
  { ST_MAXFANSPEED, menuMaxFanSpeed, 10 },
  { ST_LCDBACKLIGHT, menuLcdBacklight, 10},
  { ST_TOAST, menuToast, 20 },
  { ST_ENG_PROBEDIAG, menuProbeDiag, 0, BUTTON_LEFT },
  { 0, 0 },
};

const menu_transition_t MENU_TRANSITIONS[] PROGMEM = {
  { ST_HOME_FOOD1, BUTTON_DOWN | BUTTON_TIMEOUT, ST_HOME_FOOD2 },
  { ST_HOME_FOOD1, BUTTON_RIGHT,   ST_SETPOINT },
  { ST_HOME_FOOD1, BUTTON_UP,      ST_HOME_FOOD3 },

  { ST_HOME_FOOD2, BUTTON_DOWN | BUTTON_TIMEOUT, ST_HOME_FOOD3 },
  { ST_HOME_FOOD2, BUTTON_RIGHT,   ST_SETPOINT },
  { ST_HOME_FOOD2, BUTTON_UP,      ST_HOME_FOOD1 },

  { ST_HOME_FOOD3, BUTTON_DOWN | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_HOME_FOOD3, BUTTON_RIGHT,     ST_SETPOINT },
  { ST_HOME_FOOD3, BUTTON_UP,        ST_HOME_FOOD2 },

  { ST_HOME_NOPROBES, BUTTON_DOWN | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_HOME_NOPROBES, BUTTON_RIGHT,ST_SETPOINT },
  { ST_HOME_NOPROBES, BUTTON_UP,   ST_HOME_FOOD3 },

  { ST_SETPOINT, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_SETPOINT, BUTTON_RIGHT, ST_MANUALMODE },

  { ST_MANUALMODE, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_MANUALMODE, BUTTON_LEFT | BUTTON_LONG, ST_ENG_PROBEDIAG },
  { ST_MANUALMODE, BUTTON_RIGHT, ST_LCDBACKLIGHT },
  
  { ST_LCDBACKLIGHT, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_LCDBACKLIGHT, BUTTON_RIGHT, ST_MAXFANSPEED },

  { ST_MAXFANSPEED, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_MAXFANSPEED, BUTTON_RIGHT, ST_PROBEOFF0 },

  { ST_PROBEOFF0, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBEOFF0, BUTTON_RIGHT, ST_PROBEOFF1 },
  { ST_PROBEOFF1, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBEOFF1, BUTTON_RIGHT, ST_PROBEOFF2 },
  { ST_PROBEOFF2, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBEOFF2, BUTTON_RIGHT, ST_PROBEOFF3 },
  { ST_PROBEOFF3, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBEOFF3, BUTTON_RIGHT, ST_LIDOPEN_OFF },
  
  /* Probe 1 Submenu */
#if 0
  { ST_PROBESUB1, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBESUB1, BUTTON_RIGHT, ST_PROBESUB2 },
  { ST_PROBESUB1, BUTTON_DOWN | BUTTON_UP, ST_PROBEOFF1 },
//  { ST_PROBENAME1, BUTTON_LEFT | BUTTON_TIMEOUT, ST_PROBESUB1 },
//  { ST_PROBENAME1, BUTTON_RIGHT, ST_PROBEOFF1 },

  { ST_PROBEOFF1, BUTTON_LEFT, ST_PROBESUB1 },
  { ST_PROBEOFF1, BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBEOFF1, BUTTON_RIGHT, ST_PALARM1_H_ON },
  { ST_PALARM1_H_ON, BUTTON_LEFT, ST_PROBESUB1 },
  { ST_PALARM1_H_ON, BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PALARM1_H_ON, BUTTON_RIGHT, ST_PALARM1_H_VAL },
  { ST_PALARM1_H_VAL, BUTTON_LEFT, ST_PROBESUB1 },
  { ST_PALARM1_H_VAL, BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PALARM1_H_VAL, BUTTON_RIGHT, ST_PROBEOFF1 },
#endif

  { ST_LIDOPEN_OFF, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_LIDOPEN_OFF, BUTTON_RIGHT, ST_LIDOPEN_DUR },

  { ST_LIDOPEN_DUR, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_LIDOPEN_DUR, BUTTON_RIGHT, ST_RESETCONFIG },

  { ST_RESETCONFIG, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_RESETCONFIG, BUTTON_RIGHT, ST_SETPOINT },

  { ST_ENG_PROBEDIAG, BUTTON_LEFT | BUTTON_LONG, ST_HOME_FOOD1 },

  { 0, 0, 0 },
};

// scratch space for edits
int editInt;
char editString[17];
// Generic buffer for formatting floats
static FloatPrint<8> fp;

static button_t readButton(void)
{
  unsigned char button = analogReadOver(PIN_BUTTONS, 8);
  if (button == 0)
    return BUTTON_NONE;

  //SerialX.print("HMLG,Btn="); SerialX.print(button, DEC); Serial_nl();

  if (button > 20 && button < 60)
    return BUTTON_LEFT;  
  if (button > 60 && button < 100)
    return BUTTON_DOWN;  
  if (button > 120 && button < 160)
    return BUTTON_UP;  
  if (button > 160 && button < 200)
    return BUTTON_RIGHT;  
    
  return BUTTON_NONE;
}

static void lcdPrintBigNum(float val)
{
  // good up to 3276.8
  int16_t ival = val * 10;
  uint16_t uval;
  boolean isNeg;
  if (ival < 0)
  {
    isNeg = true;
    uval = -ival;
  }
  else
  {
    isNeg = false;
    uval = ival;
  }

  int8_t x = 16;
  do
  {
    if (uval != 0 || x >= 9)
    {
      const char *numData = NUMS + ((uval % 10) * 6);

      x -= C_WIDTH;
      lcd.setCursor(x, 0);
      lcd.write_P(numData, C_WIDTH);
      numData += C_WIDTH;

      lcd.setCursor(x, 1);
      lcd.write_P(numData, C_WIDTH);

      uval /= 10;
    }  /* if val */
    --x;
    lcd.setCursor(x, 0);
    lcd.write(C_BLK);
    lcd.setCursor(x, 1);
    if (x == 12)
      lcd.write('.');
    else if (uval == 0 && x < 9 && isNeg)
    {
      lcd.write(C_CT);
      isNeg = false;
    }
    else
      lcd.write(C_BLK);
  } while (x != 0);
}

static void lcdDefineChars(void)
{
  for (unsigned char i = 0; i<8; ++i)
    lcd.createChar_P(i, BIG_CHAR_PARTS + (i * 8));
}

void lcdprint_P(const char *p, const boolean doClear)
{
  if (doClear)
    lcd.clear();
  while (unsigned char c = pgm_read_byte(p++)) lcd.write(c);
}

static void updateHome(void)
{
  // Updates to the temperature can come at any time, only update
  // if we're in a state that displays them
  state_t state = Menus.getState();

  char buffer[17];
  unsigned char probeIdxLow, probeIdxHigh;

  // Fixed pit area
  lcd.setCursor(0, 0);
  if (state == ST_HOME_ALARM)
  {
    uint8_t id = pid.getAlarmId();
    if (ALARM_ID_TO_IDX(id) == ALARM_IDX_LOW)
      lcdprint_P(PSTR("** ALARM LOW  **"), false);
    else
      lcdprint_P(PSTR("** ALARM HIGH **"), false);

    probeIdxLow = probeIdxHigh = ALARM_ID_TO_PROBE(id);
  }  /* if ST_HOME_ALARM */
  else
  {
    /* Big Number probes overwrite the whole display if it has a temperature */
    uint8_t mode = Menus.getHomeDisplayMode();
    if (mode >= TEMP_PIT && mode <= TEMP_FOOD3)
    {
      TempProbe *probe = pid.Probes[mode];
      if (probe->hasTemperature())
      {
        lcdPrintBigNum(probe->Temperature);
        return;
      }
    }

    /* Default Pit / Fan Speed first line */
    int pitTemp;
    if (pid.Probes[TEMP_CTRL]->hasTemperature())
      pitTemp = pid.Probes[TEMP_CTRL]->Temperature;
    else
      pitTemp = 0;
    if (!pid.isManualOutputMode() && !pid.Probes[TEMP_CTRL]->hasTemperature())
      memcpy_P(buffer, LCD_LINE1_UNPLUGGED, sizeof(LCD_LINE1_UNPLUGGED));
    else if (pid.isDisabled())
      snprintf_P(buffer, sizeof(buffer), PSTR("Pit:%3d" DEGREE "%c  [Off]"),
        pitTemp, pid.getUnits());
    else if (pid.LidOpenResumeCountdown > 0)
      snprintf_P(buffer, sizeof(buffer), PSTR("Pit:%3d" DEGREE "%c Lid%3u"),
        pitTemp, pid.getUnits(), pid.LidOpenResumeCountdown);
    else
    {
      char c1, c2;
      if (pid.isManualOutputMode())
      {
        c1 = '^';  // LCD_ARROWUP
        c2 = '^';  // LCD_ARROWDN
      }
      else
      {
        c1 = '[';
        c2 = ']';
      }
      snprintf_P(buffer, sizeof(buffer), PSTR("Pit:%3d" DEGREE "%c %c%3u%%%c"),
        pitTemp, pid.getUnits(), c1, pid.getPidOutput(), c2);
    }

    lcd.print(buffer);
    // Display mode 0xff is 2-line, which only has space for 1 non-pit value
    if (Menus.getHomeDisplayMode() == 0xff)
      probeIdxLow = probeIdxHigh = state - ST_HOME_FOOD1 + TEMP_FOOD1;
    else
    {
      // Display mode 0xfe is 4 line home, display 3 other temps there
      probeIdxLow = TEMP_FOOD1;
      probeIdxHigh = TEMP_FOOD3;
    }
  } /* if !ST_HOME_ALARM */

    // Rotating probe display
  for (unsigned char probeIndex = probeIdxLow; probeIndex <= probeIdxHigh; ++probeIndex)
  {
    if (probeIndex < TEMP_COUNT && pid.Probes[probeIndex]->hasTemperature())
    {
      loadProbeName(probeIndex);
      snprintf_P(buffer, sizeof(buffer), PSTR("%-12s%3d" DEGREE), editString,
        (int)pid.Probes[probeIndex]->Temperature);
    }
    else
    {
      // If probeIndex is outside the range (in the case of ST_HOME_NOPROBES)
      // just fill the bottom line with spaces
      memset(buffer, ' ', sizeof(buffer));
      buffer[sizeof(buffer) - 1] = '\0';
    }

    lcd.setCursor(0, probeIndex - probeIdxLow + 1);
    lcd.print(buffer);
  }
}

static void menuBooleanEdit(button_t button, const char *preamble)
{
  if (button == BUTTON_UP || button == BUTTON_DOWN)
    editInt = !editInt;

  lcd.setCursor(0, 1);
  if (preamble != NULL)
    lcdprint_P(preamble, false);
  lcdprint_P((editInt != 0) ? PSTR("Yes") : PSTR("No "), false);
}

static void menuNumberEdit(button_t button, unsigned char increment,
  int minVal, int maxVal, const char *format)
{
  char buffer[17];

  // If button is being held down, accelerate the increment
  unsigned char repCnt = Menus.getButtonRepeatCnt();
  if (repCnt > 10)
    increment *= 4;
  else if (repCnt > 5)
    increment *= 2;

  if (button == BUTTON_UP)
    editInt += increment;
  else if (button == BUTTON_DOWN)
    editInt -= increment;
  if (editInt < minVal)
    editInt = minVal;
  if (editInt > maxVal)
    editInt = maxVal;

  lcd.setCursor(0, 1);
  snprintf_P(buffer, sizeof(buffer), format, editInt, pid.getUnits());
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
static state_t menuStringEdit(button_t button, const char *line1, unsigned char maxLength)
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
      return Menus.getState();
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

static void menuProbenameLine(unsigned char probeIndex)
{
    loadProbeName(probeIndex);
    lcd.clear();
    lcd.print(editString);
}

static state_t menuHome(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    Menus.setUpdateDisplay(&updateHome);
    if (Menus.getState() != ST_HOME_NOPROBES && !pid.isAnyFoodProbeActive())
      return ST_HOME_NOPROBES;
    else if (Menus.getState() == ST_HOME_FOOD1 && !pid.Probes[TEMP_FOOD1]->hasTemperature())
      return ST_HOME_FOOD2;
    else if (Menus.getState() == ST_HOME_FOOD2 && !pid.Probes[TEMP_FOOD2]->hasTemperature())
      return ST_HOME_FOOD3;
    else if (Menus.getState() == ST_HOME_FOOD3 && !pid.Probes[TEMP_FOOD3]->hasTemperature())
      return ST_HOME_FOOD1;
  }
  else if (button == (BUTTON_LEFT | BUTTON_LONG))
  {
    // Long left press toggles between AUTO/MANUAL -> OFF and OFF -> AUTO
    pid.setPidMode(pid.getPidMode() == PIDMODE_OFF ? PIDMODE_STARTUP : PIDMODE_OFF);
    storePidMode();
  }
  // In manual fan mode Up is +5% Down is -5% and Left is -1%
  else if (pid.isManualOutputMode())
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

    pid.setPidOutput(pid.getPidOutput() + offset);
  }
  else if (button == BUTTON_LEFT && !pid.isDisabled())
  {
    // Left from Home screen enables/disables the lid countdown
    storeLidParam(LIDPARAM_ACTIVE, pid.LidOpenResumeCountdown == 0);
    // Immediately show the Lid open/closed LED indicator and other changed statuses
    publishLeds();
  }
  
  return ST_AUTO;
}

static state_t menuSetpoint(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(PSTR("Set temperature:"), true);
    editInt = pid.getSetPoint();
  }
  else if (button == BUTTON_LEAVE)
  {
    // Check to see if it is different because the setPoint 
    // field stores either the setPoint or manual mode
    if (editInt != pid.getSetPoint())
      storeSetPoint(editInt);
  }

  menuNumberEdit(button, 5, 0, 1000, PSTR("%3d" DEGREE "%c"));
  return ST_AUTO;
}

static state_t menuProbename(button_t button)
{
  char buffer[17];
  unsigned char probeIndex = Menus.getState() - ST_PROBENAME1 + 1;

  if (button == BUTTON_ENTER)
  {
    loadProbeName(probeIndex);
    snprintf_P(buffer, sizeof(buffer), PSTR("Set probe %1d name"), probeIndex);
  }

  // note that we only load the buffer with text on the ENTER call,
  // after that it is OK to have garbage in it  
  state_t retVal = menuStringEdit(button, buffer, PROBE_NAME_SIZE - 1);
  if (retVal == Menus.getState())
    storeAndReportProbeName(probeIndex, editString);
    
  return retVal;
}

static state_t menuProbeOffset(button_t button)
{
  unsigned char probeIndex = Menus.getState() - ST_PROBEOFF0;
  
  if (button == BUTTON_ENTER)
  {
    menuProbenameLine(probeIndex);
    editInt = pid.Probes[probeIndex]->Offset;
  }
  else if (button == BUTTON_LEAVE)
    storeAndReportProbeOffset(probeIndex, editInt);

  menuNumberEdit(button, 1, -100, 100, PSTR("Offset %4d" DEGREE "%c"));
  return ST_AUTO;
}

static state_t menuLidOpenOff(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(PSTR("Lid open offset"), true);
    editInt = pid.LidOpenOffset;
  }
  else if (button == BUTTON_LEAVE)
  {
    storeLidParam(LIDPARAM_OFFSET, editInt);
  }

  menuNumberEdit(button, 1, 0, 100, PSTR("%3d%% below set"));
  return ST_AUTO;
}

static state_t menuLidOpenDur(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(PSTR("Lid open timer"), true);
    editInt = pid.getLidOpenDuration();
  }
  else if (button == BUTTON_LEAVE)
  {
    storeLidParam(LIDPARAM_DURATION, editInt);
  }

  menuNumberEdit(button, 10, 0, 1000, PSTR("%3d seconds"));
  return ST_AUTO;
}

static state_t menuManualMode(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(PSTR("Manual fan mode"), true);
    editInt = pid.isManualOutputMode();
  }
  else if (button == BUTTON_LEAVE)
  {
    // Check to see if it is different because the setPoint 
    // field stores either the setPoint or manual mode
    boolean manual = (editInt != 0); 
    if (manual != pid.isManualOutputMode())
      storeSetPoint(manual ? 0 : pid.getSetPoint());
  }
  menuBooleanEdit(button, NULL);
  return ST_AUTO;
}

static state_t menuResetConfig(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(PSTR("Reset config?"), true);
    editInt = 0;
  }
  else if (button == BUTTON_LEAVE)
  {
    if (editInt != 0)
    {
      eepromLoadConfig(1);
      print_P(PSTR("HMRC,USER")); Serial_nl();
    }
  }
  menuBooleanEdit(button, NULL);
  return ST_AUTO;
}

static state_t menuMaxFanSpeed(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(PSTR("Maximum auto fan"), true);
    editInt = pid.getFanMaxSpeed();
  }
  else if (button == BUTTON_LEAVE)
  {
    if (editInt != pid.getFanMaxSpeed())
      storeAndReportMaxFanSpeed(editInt);
  }
  
  menuNumberEdit(button, 5, 0, 100, PSTR("speed %3d%%"));
  return ST_AUTO;
}

static state_t menuAlarmTriggered(button_t button)
{
  if (button == BUTTON_ENTER)
    Menus.setUpdateDisplay(&updateHome);
  else if (button & BUTTON_ANY)
  {
    silenceRingingAlarm();
    return ST_LAST;
  }

  return ST_AUTO;
}

static state_t menuLcdBacklight(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(PSTR("LCD brightness"), true);
    editInt = lcd.getBacklight();
  }
  else if (button == BUTTON_LEAVE)
  {
    if (editInt != lcd.getBacklight())
    {
      storeLcdBacklight(editInt);
      reportLcdParameters();
    }
  }
  
  menuNumberEdit(button, 10, 0, 100, PSTR("%3d%%"));
  lcd.setBacklight(editInt, false);
  return ST_AUTO;
}

static state_t menuToast(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcd.home();
    lcd.write(Menus.getToastLine0(), 16);
    lcd.setCursor(0, 1);
    lcd.write(Menus.getToastLine1(), 16);
    return ST_AUTO;
  }
  // Timeout or button press returns you to the previous menu
  return ST_LAST;
}

static void updateProbeDiag(void)
{
  lcd.home();

  // P1 ADC65535 99Nz - ProbeNum RawADCReading Noise
  const TempProbe *probe = pid.Probes[Menus.ProbeNum];
  snprintf_P(editString, sizeof(editString), PSTR("P%1u ADC%05u %02uNz"),
    Menus.ProbeNum,
    analogReadOver(probe->getPin(), 10 + TEMP_OVERSAMPLE_BITS),
    analogReadRange(probe->getPin())
  );
  lcd.write(editString);

  // Thermis 999.99oC - ProbeType Temp Units
  lcd.setCursor(0, 1);
  const char *ptype = (char *)pgm_read_word(&LCD_PROBETYPES[probe->getProbeType()]);
  lcdprint_P(ptype, false);
  lcd.write(' ');
  if (probe->hasTemperature())
  {
    fp.print(lcd, probe->Temperature, 6, 2);
    lcd.write(DEGREE);
  }
  else
    lcdprint_P(PSTR("       "), false);
  lcd.write(pid.getUnits());
}

static state_t menuProbeDiag(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    // On first entry, set to first probe
    if (Menus.getLastState() != ST_ENG_PROBEDIAG)
      Menus.ProbeNum = TEMP_PIT;
    Menus.setUpdateDisplay(&updateProbeDiag);
  }
  else if (button == BUTTON_RIGHT)
  {
    // Next Probe
    Menus.ProbeNum = (Menus.ProbeNum + 1) % TEMP_COUNT;
  }
  else if (button == BUTTON_LEFT)
  {
    // Previous Probe
    if (Menus.ProbeNum == 0)
      Menus.ProbeNum = TEMP_COUNT - 1;
    else
      --Menus.ProbeNum;
  }

  return ST_AUTO;
}

void HmMenuSystem::displayToast(char *msg)
{
  /* This function attempts to clumsily split msg into two lines for display 
     and pads the extra space characters with space. If there is no comma in
     msg, then just the first line is written */
  unsigned char dst = 0;
  while (*msg && dst < sizeof(_toastMsg))
  {
    if (*msg != ',')
      _toastMsg[dst++] = *msg;
    else
      while (dst < (sizeof(_toastMsg)/2))
        _toastMsg[dst++] = ' ';
    ++msg;
  }
  while (dst < sizeof(_toastMsg))
     _toastMsg[dst++] = ' ';

  if (getState() != ST_TOAST)
    setState(ST_TOAST);
  else
    menuToast(BUTTON_ENTER); // If already in a toast force a refresh
}

void HmMenuSystem::setHomeDisplayMode(unsigned char v)
{
  _homeDisplayMode = v;

  state_t state = getState();
  // If we're in home, clear in case we're switching from 4 to 2
  if (state >= ST_HOME_FOOD1 && state <= ST_HOME_FOOD3)
  {
    lcd.clear();
    updateDisplay();
  }
}

void HmMenuSystem::init(void)
{
  lcdDefineChars();
}

HmMenuSystem Menus(MENU_DEFINITIONS, MENU_TRANSITIONS, &readButton);

