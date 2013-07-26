// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#include "hmcore.h"
#include "grillpid.h"
#include "strings.h"

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

static const menu_definition_t MENU_DEFINITIONS[] PROGMEM = {
  { ST_HOME_FOOD1, menuHome, 5 },
  { ST_HOME_FOOD2, menuHome, 5 },
  { ST_HOME_AMB, menuHome, 5 },
  { ST_HOME_NOPROBES, menuHome, 1 },
  { ST_HOME_ALARM, menuAlarmTriggered, 0 },
  { ST_SETPOINT, menuSetpoint, 10 },
  { ST_MANUALMODE, menuManualMode, 10 },
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

  { ST_MANUALMODE, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
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

  { 0, 0, 0 },
};

// scratch space for edits
int editInt;
char editString[17];

static button_t readButton(void)
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

static void menuBooleanEdit(button_t button, const char PROGMEM *preamble)
{
  if (button == BUTTON_UP || button == BUTTON_DOWN)
    editInt = !editInt;

  lcd.setCursor(0, 1);
  if (preamble != NULL)
    lcdprint_P(preamble, false);
  lcdprint_P((editInt != 0) ? PSTR("Yes") : PSTR("No "), false);
}

static void menuNumberEdit(button_t button, unsigned char increment,
  int minVal, int maxVal, const char PROGMEM *format)
{
  char buffer[17];
  
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
    if (Menus.getState() != ST_HOME_NOPROBES && !pid.isAnyFoodProbeActive())
      return ST_HOME_NOPROBES;
    else if (Menus.getState() == ST_HOME_FOOD1 && !pid.Probes[TEMP_FOOD1]->hasTemperature())
      return ST_HOME_FOOD2;
    else if (Menus.getState() == ST_HOME_FOOD2 && !pid.Probes[TEMP_FOOD2]->hasTemperature())
      return ST_HOME_AMB;
    else if (Menus.getState() == ST_HOME_AMB && !pid.Probes[TEMP_AMB]->hasTemperature())
      return ST_HOME_FOOD1;

    updateDisplay();
  }
  // In manual fan mode Up is +5% Down is -5% and Left is -1%
  else if (pid.getManualOutputMode())
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
    updateDisplay();
    return ST_NONE;
  }
  else if (button == BUTTON_LEFT)
  {
    // Left from Home screen enables/disables the lid countdown
    storeLidParam(LIDPARAM_ACTIVE, pid.LidOpenResumeCountdown == 0);
    updateDisplay();
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

  menuNumberEdit(button, 5, 0, 1000, PSTR("%3d"DEGREE"%c"));
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

  menuNumberEdit(button, 1, -100, 100, PSTR("Offset %4d"DEGREE"%c"));
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
    editInt = pid.getManualOutputMode();
  }
  else if (button == BUTTON_LEAVE)
  {
    // Check to see if it is different because the setPoint 
    // field stores either the setPoint or manual mode
    boolean manual = (editInt != 0); 
    if (manual != pid.getManualOutputMode())
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
      eepromLoadConfig(1);
  }
  menuBooleanEdit(button, NULL);
  return ST_AUTO;
}

static state_t menuMaxFanSpeed(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(PSTR("Maximum auto fan"), true);
    editInt = pid.getMaxFanSpeed();
  }
  else if (button == BUTTON_LEAVE)
  {
    if (editInt != pid.getMaxFanSpeed())
      storeAndReportMaxFanSpeed(editInt);
  }
  
  menuNumberEdit(button, 5, 0, 100, PSTR("speed %3d%%"));
  return ST_AUTO;
}

static state_t menuAlarmTriggered(button_t button)
{
  if (button == BUTTON_ENTER)
    updateDisplay();
  // If any physical button is pressed, silence and return home
  else if (button & BUTTON_ANY)
  {
    silenceRingingAlarm();
    return ST_HOME_FOOD1;
  }

  return ST_AUTO;
}

static state_t menuAlarmAction(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(PSTR("Alarm"), true);
    editInt = 0;
  }
  else if (button == BUTTON_TIMEOUT)
    return ST_HOME_ALARM;
  else if (button == BUTTON_LEFT || button == BUTTON_RIGHT)
  {
    /* False is 'silence', true is 'disable' */
    silenceRingingAlarm(); //editInt);
    return ST_HOME_FOOD1;
  }
  else if (button == BUTTON_UP || button == BUTTON_DOWN)
    editInt = !editInt;

  lcd.setCursor(0, 1);
  lcdprint_P((editInt != 0) ? PSTR("^ Disable v") : PSTR("^ Silence v"), false);

  return ST_AUTO;
}

static state_t menuLcdBacklight(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(PSTR("LCD brightness"), true);
    editInt = g_LcdBacklight;
  }
  else if (button == BUTTON_LEAVE)
  {
    if (editInt != g_LcdBacklight)
    {
      storeLcdBacklight(editInt);
      reportLcdParameters();
    }
  }
  
  menuNumberEdit(button, 10, 0, 100, PSTR("%3d%%"));
  setLcdBacklight(0x80 | editInt);
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
  return Menus.getLastState();
}

void HmMenuSystem::displayToast(char *msg)
{
  /* This function attempts to clumsily split msg into two lines for display 
     and pads the extra space characters with space. If there is no comma in
     msg, then just the first line is written */
  char *pos = strchr(msg, ',');
  memset(_toastMsg, ' ', sizeof(_toastMsg));
  if (pos == NULL)
  {
    memcpy(getToastLine0(), msg, min(strlen(msg), sizeof(_toastMsg)));
  }
  else
  {
    memcpy(getToastLine0(), msg, min(pos - msg, sizeof(_toastMsg)/2));
    memcpy(getToastLine1(), pos+1, min(strlen(pos)-1, sizeof(_toastMsg)/2));
  }
  if (getState() != ST_TOAST)
    setState(ST_TOAST);
  else
    menuToast(BUTTON_ENTER); // If already in a toast force a refresh
}

HmMenuSystem Menus(MENU_DEFINITIONS, MENU_TRANSITIONS, &readButton);

