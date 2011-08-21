// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#include "hmcore.h"
#include "grillpid.h"
#include "strings.h"

#ifdef HEATERMETER_NETWORKING
state_t menuConnecting(button_t button);
state_t menuNetworkInfo(button_t button);
#endif  /* HEATERMETER_NETWORKING */

const menu_definition_t MENU_DEFINITIONS[] PROGMEM = {
#ifdef HEATERMETER_NETWORKING
  { ST_CONNECTING, menuConnecting, 2 },
  { ST_NETWORK_INFO, menuNetworkInfo, 10 },
#endif  /* HEATERMETER_NETWORKING */
  { ST_HOME_FOOD1, menuHome, 5 },
  { ST_HOME_FOOD2, menuHome, 5 },
  { ST_HOME_AMB, menuHome, 5 },
  { ST_HOME_NOPROBES, menuHome, 1 },
  { ST_SETPOINT, menuSetpoint, 10 },
  { ST_MANUALMODE, menuManualMode, 10 },
  { ST_PROBESUB0, menuProbeSubmenu, 10 },
  { ST_PROBESUB1, menuProbeSubmenu, 10 },
  { ST_PROBESUB2, menuProbeSubmenu, 10 },
  { ST_PROBESUB3, menuProbeSubmenu, 10 },
//  { ST_PROBENAME1, menuProbename, 10 },
//  { ST_PROBENAME2, menuProbename, 10 },
//  { ST_PROBENAME3, menuProbename, 10 },
  { ST_PROBEOFF0, menuProbeOffset, 10 },
  { ST_PROBEOFF1, menuProbeOffset, 10 },
  { ST_PROBEOFF2, menuProbeOffset, 10 },
  { ST_PROBEOFF3, menuProbeOffset, 10 },
  { ST_LIDOPEN_OFF, menuLidOpenOff, 10 },
  { ST_LIDOPEN_DUR, menuLidOpenDur, 10 },
  { ST_RESETCONFIG, menuResetConfig, 10 },
  { ST_MAXFANSPEED, menuMaxFanSpeed, 10 },
  { ST_PALARM1_H_ON, menuProbeAlarmOn, 10 },
  { ST_PALARM1_H_VAL, menuProbeAlarmVal, 10 },
  { 0, 0 },
};

const menu_transition_t MENU_TRANSITIONS[] PROGMEM = {
#ifdef HEATERMETER_NETWORKING
  { ST_CONNECTING, BUTTON_TIMEOUT, ST_HOME_FOOD1 },
#endif  /* HEATERMETER_NETWORKING */
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
  { ST_MANUALMODE, BUTTON_RIGHT, ST_MAXFANSPEED },
  
  { ST_MAXFANSPEED, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_MAXFANSPEED, BUTTON_RIGHT, ST_PROBEOFF1 },

  { ST_PROBEOFF1, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBEOFF1, BUTTON_RIGHT, ST_PROBEOFF2 },
  { ST_PROBEOFF2, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBEOFF2, BUTTON_RIGHT, ST_PROBEOFF3 },
  { ST_PROBEOFF3, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBEOFF3, BUTTON_RIGHT, ST_PROBEOFF0 },
  { ST_PROBEOFF0, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_PROBEOFF0, BUTTON_RIGHT, ST_LIDOPEN_OFF },
  
  /* Probe 1 Submenu */
#if defined(NEVER)  // disabled temporarily
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
#ifdef HEATERMETER_NETWORKING
  { ST_LIDOPEN_DUR, BUTTON_RIGHT, ST_NETWORK_INFO },

  { ST_NETWORK_INFO, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_NETWORK_INFO, BUTTON_RIGHT, ST_RESETCONFIG },
#else
   { ST_LIDOPEN_DUR, BUTTON_RIGHT, ST_RESETCONFIG },
#endif  /* HEATERMETER_NETWORKING */

  { ST_RESETCONFIG, BUTTON_LEFT | BUTTON_TIMEOUT, ST_HOME_FOOD1 },
  { ST_RESETCONFIG, BUTTON_RIGHT, ST_SETPOINT },

  { 0, 0, 0 },
};

MenuSystem Menus(MENU_DEFINITIONS, MENU_TRANSITIONS, &readButton);
// scratch space for edits
int editInt;  
char editString[17];

#ifdef HEATERMETER_NETWORKING
extern "C" {
#include "witypes.h"
#include "g2100.h"
extern char ssid[];
}
#endif /* HEATERMETER_NETWORKING */

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

void menuProbenameLine(unsigned char probeIndex)
{
    loadProbeName(probeIndex);
    lcd.clear();
    lcd.print(editString);
}

state_t menuHome(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    if (Menus.State != ST_HOME_NOPROBES && !pid.isAnyFoodProbeActive())
      return ST_HOME_NOPROBES;
    else if (Menus.State == ST_HOME_FOOD1 && !pid.Probes[TEMP_FOOD1]->hasTemperature())
      return ST_HOME_FOOD2;
    else if (Menus.State == ST_HOME_FOOD2 && !pid.Probes[TEMP_FOOD2]->hasTemperature())
      return ST_HOME_AMB;
    else if (Menus.State == ST_HOME_AMB && !pid.Probes[TEMP_AMB]->hasTemperature())
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

#ifdef HEATERMETER_NETWORKING
state_t menuConnecting(button_t button)
{
  lcdprint_P(PSTR("Connecting to"), true); 
  lcd.setCursor(0, 1);
  lcd.print(ssid);

  return ST_AUTO;
}

state_t menuNetworkInfo(button_t button)
{
  if (button == BUTTON_ENTER || button == BUTTON_UP || button == BUTTON_DOWN)
  {
    char buffer[17];
    lcdprint_P(PSTR("Wireless Signal"), true);
    lcd.setCursor(0, 1);
    snprintf_P(buffer, sizeof(buffer), PSTR("%3u%% %s"), zg_get_rssi() - 100, ssid);
    lcd.print(buffer);
  }
  return ST_AUTO;
}
#endif /* HEATERMETER_NETWORKING */

void menuBooleanEdit(button_t button, const prog_char *preamble)
{
  if (button == BUTTON_UP || button == BUTTON_DOWN)
    editInt = !editInt;

  lcd.setCursor(0, 1);
  if (preamble != NULL)
    lcdprint_P(preamble, false);
  lcdprint_P((editInt != 0) ? PSTR("Yes") : PSTR("No"), false);
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
/*
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
*/

state_t menuSetpoint(button_t button)
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

  menuNumberEdit(button, 5, PSTR("%3d"DEGREE"F"));
  return ST_AUTO;
}

/*
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
*/

state_t menuProbeOffset(button_t button)
{
  unsigned char probeIndex = Menus.State - ST_PROBEOFF0;
  
  if (button == BUTTON_ENTER)
  {
    menuProbenameLine(probeIndex);
    editInt = pid.Probes[probeIndex]->Offset;
  }
  else if (button == BUTTON_LEAVE)
    storeProbeOffset(probeIndex, editInt);

  menuNumberEdit(button, 1, PSTR("Offset %4d"DEGREE"F"));
  return ST_AUTO;
}

state_t menuProbeSubmenu(button_t button)
{
  unsigned char probeIndex = Menus.State - ST_PROBESUB0;
  if (button == BUTTON_ENTER)
  {
    menuProbenameLine(probeIndex);
    lcd.setCursor(0, 1);  
    lcdprint_P(PSTR("v probe config v"), false);
  }
  
  return ST_AUTO;
}

state_t menuLidOpenOff(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(PSTR("Lid open offset"), true);
    editInt = pid.LidOpenOffset;
  }
  else if (button == BUTTON_LEAVE)
  {
    storeLidOpenOffset(editInt < 0 ? 0 : editInt);
  }

  menuNumberEdit(button, 1, PSTR("%3d%% below set"));
  return ST_AUTO;
}

state_t menuLidOpenDur(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(PSTR("Lid open timer"), true);
    editInt = pid.LidOpenDuration;    
  }
  else if (button == BUTTON_LEAVE)
  {
    storeLidOpenDuration(editInt < 0 ? 0 : editInt);
  }

  menuNumberEdit(button, 10, PSTR("%3d seconds"));
  return ST_AUTO;
}

state_t menuManualMode(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(PSTR("Manual fan mode"), true);
    editInt = pid.getManualFanMode();    
  }
  else if (button == BUTTON_LEAVE)
  {
    // Check to see if it is different because the setPoint 
    // field stores either the setPoint or manual mode
    boolean manual = (editInt != 0); 
    if (manual != pid.getManualFanMode())
      storeSetPoint(manual ? 0 : pid.getSetPoint());
  }
  menuBooleanEdit(button, NULL);
  return ST_AUTO;
}

state_t menuResetConfig(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(PSTR("Reset config?"), true);
    editInt = 0;
  }
  else if (button == BUTTON_LEAVE)
  {
    if (editInt != 0)
      eepromLoadConfig(true);
  }
  menuBooleanEdit(button, NULL);
  return ST_AUTO;
}

state_t menuMaxFanSpeed(button_t button)
{
  if (button == BUTTON_ENTER)
  {
    lcdprint_P(PSTR("Maximum auto fan"), true);
    editInt = pid.MaxFanSpeed;
  }
  else if (button == BUTTON_LEAVE)
  {
    if (editInt != pid.MaxFanSpeed)
      storeMaxFanSpeed(editInt);
  }
  
  menuNumberEdit(button, 5, PSTR("speed %d%%"));
  return ST_AUTO;
}

state_t menuProbeAlarmOn(button_t button)
{
  // This function works for both low and high so determine which we're being called for
  unsigned char highOrLow;
  if (Menus.State >= ST_PALARM0_H_ON && Menus.State <= ST_PALARM3_H_ON)
    highOrLow = ST_PALARM0_H_ON;
  else
    highOrLow = ST_PALARM0_L_ON;
    
  unsigned char probeIndex = Menus.State - highOrLow;
  if (button == BUTTON_ENTER)
  {
    menuProbenameLine(probeIndex);
    unsigned char whichMask = (highOrLow == ST_PALARM0_H_ON) ? ProbeAlarm::HIGH_ENABLED : ProbeAlarm::LOW_ENABLED;
    editInt = pid.Probes[probeIndex]->Alarms.Status & whichMask;
  }
  else if (button == BUTTON_LEAVE)
  {
//    boolean val = (editInt != 0);
//    if 
  }

  menuBooleanEdit(button, (highOrLow == ST_PALARM0_H_ON) ? PSTR("High alarm? ") : PSTR("Low alarm? "));
  return ST_AUTO;
}

state_t menuProbeAlarmVal(button_t button)
{
  // This function works for both low and high so determine which we're being called for
  unsigned char highOrLow;
  if (Menus.State >= ST_PALARM0_H_VAL && Menus.State <= ST_PALARM3_H_VAL)
    highOrLow = ST_PALARM0_H_VAL;
  else
    highOrLow = ST_PALARM0_L_VAL;
    
  unsigned char probeIndex = Menus.State - highOrLow;
  if (button == BUTTON_ENTER)
  {
    menuProbenameLine(probeIndex);
    ProbeAlarm &alarm  = pid.Probes[probeIndex]->Alarms;
    editInt = (highOrLow == ST_PALARM0_H_VAL) ? alarm.getHigh() : alarm.getLow();
  }
  
  menuNumberEdit(button, 5, (highOrLow == ST_PALARM0_H_VAL) ? PSTR("High Alrm %4d"DEGREE"F") : PSTR("Low Alrm %5d"DEGREE"F"));
  return ST_AUTO;
}

