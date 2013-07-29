// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#include "Arduino.h"
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <avr/power.h>

#include "hmcore.h"

#ifdef HEATERMETER_RFM12
#include "rfmanager.h"
#endif

#include "bigchars.h"
#include "ledmanager.h"

static TempProbe probe0(PIN_PIT);
static TempProbe probe1(PIN_FOOD1);
static TempProbe probe2(PIN_FOOD2);
static TempProbe probe3(PIN_AMB);
GrillPid pid(PIN_BLOWER, PIN_SERVO);

#ifdef SHIFTREGLCD_NATIVE
ShiftRegLCD lcd(PIN_SERVO, PIN_LCD_CLK, TWO_WIRE, 2);
#else
ShiftRegLCD lcd(PIN_LCD_CLK, 2);
#endif /* SHIFTREGLCD_NATIVE */

#ifdef HEATERMETER_SERIAL
static char g_SerialBuff[64]; 
#endif /* HEATERMETER_SERIAL */

#ifdef HEATERMETER_RFM12
static void rfSourceNotify(RFSource &r, unsigned char event); // prototype
static RFManager rfmanager(&rfSourceNotify);
static unsigned char rfMap[TEMP_COUNT];
#endif /* HEATERMETER_RFM12 */

static void ledExecutor(unsigned char led, unsigned char on); // prototype
static LedManager ledmanager(&ledExecutor);

static unsigned char g_AlarmId; // ID of alarm going off
static unsigned char g_HomeDisplayMode;
static unsigned char g_LogPidInternals; // If non-zero then log PID interals
unsigned char g_LcdBacklight; // 0-100

#define config_store_byte(eeprom_field, src) { eeprom_write_byte((uint8_t *)offsetof(__eeprom_data, eeprom_field), src); }
#define config_store_word(eeprom_field, src) { eeprom_write_word((uint16_t *)offsetof(__eeprom_data, eeprom_field), src); }

#define EEPROM_MAGIC 0xf00e

static const struct __eeprom_data {
  unsigned int magic;
  int setPoint;
  unsigned char lidOpenOffset;
  unsigned int lidOpenDuration;
  float pidConstants[4]; // constants are stored Kb, Kp, Ki, Kd
  boolean manualMode;
  unsigned char lcdBacklight; // in PWM (max 100)
#ifdef HEATERMETER_RFM12
  unsigned char rfMap[TEMP_COUNT];
#endif
  char pidUnits;
  unsigned char minFanSpeed;  // in percent
  unsigned char maxFanSpeed;  // in percent
  unsigned char pidOutputFlags;
  unsigned char homeDisplayMode;
  unsigned char unused;
  unsigned char ledConf[LED_COUNT];
  unsigned char minServoPos;  // in percent
  unsigned char maxServoPos;  // in percent
} DEFAULT_CONFIG[] PROGMEM = {
 {
  EEPROM_MAGIC,  // magic
  225,  // setpoint
  6,    // lid open offset %
  240,  // lid open duration
  { 4.0f, 3.0f, 0.005f, 5.0f },  // PID constants
  false, // manual mode
  50,   // lcd backlight (%)
#ifdef HEATERMETER_RFM12
  { RFSOURCEID_ANY, RFSOURCEID_ANY, RFSOURCEID_ANY, RFSOURCEID_ANY },  // rfMap
#endif
  'F',  // Units
  10,   // min fan speed  
  100,  // max fan speed
  0x00, // PID output flags bitmask
  0xff, // 2-line home
  0xff, // unused
  { LEDSTIMULUS_RfReceive, LEDSTIMULUS_LidOpen, LEDSTIMULUS_FanOn, LEDSTIMULUS_Off },
  60, // min servo pos = 600us
  250  // max servo pos = 2500us
}
};

// EEPROM address of the start of the probe structs, the 2 bytes before are magic
#define EEPROM_PROBE_START  64

static const struct  __eeprom_probe DEFAULT_PROBE_CONFIG PROGMEM = {
  "Probe  ", // Name if you change this change the hardcoded number-appender in eepromLoadProbeConfig()
  PROBETYPE_INTERNAL,  // probeType
  0,  // offset
  -40,  // alarm low
  -200, // alarm high
  0,  // unused1
  0,  // unused2
  {
    2.3067434e-4,2.3696596e-4,1.2636414e-7  // Maverick ET-72
    //5.36924e-4,1.91396e-4,6.60399e-8 // Maverick ET-732 (Honeywell R-T Curve 4)
    //8.98053228e-4,2.49263324e-4,2.04047542e-7 // Radio Shack 10k
    //1.14061e-3,2.32134e-4,9.63666e-8 // Vishay 10k NTCLE203E3103FB0
    ,1.0e+4
  }
};

#ifdef PIEZO_HZ
// A simple beep-beep-beep-(pause) alarm
static unsigned char tone_durs[] PROGMEM = { 10, 5, 10, 5, 10, 50 };  // in 10ms units
#define tone_cnt (sizeof(tone_durs)/sizeof(tone_durs[0]))
static unsigned char tone_idx;
static unsigned long tone_last;
#endif /* PIZEO_HZ */

void setLcdBacklight(unsigned char lcdBacklight)
{
  /* If the high bit is set, that means just set the output, do not store */
  if ((0x80 & lcdBacklight) == 0)
    g_LcdBacklight = lcdBacklight;
  lcdBacklight &= 0x7f;
  analogWrite(PIN_LCD_BACKLGHT, (unsigned int)(lcdBacklight) * 255 / 100);
}

// Note the storage loaders and savers expect the entire config storage is less than 256 bytes
static unsigned char getProbeConfigOffset(unsigned char probeIndex, unsigned char off)
{
  if (probeIndex >= TEMP_COUNT)
    return 0;
  // Point to the name in the first probe_config structure
  unsigned char retVal = EEPROM_PROBE_START + off;
  // Stride to the proper configuration structure
  retVal += probeIndex * sizeof( __eeprom_probe);
  
  return retVal;
}

static void storeProbeName(unsigned char probeIndex, const char *name)
{
  unsigned char ofs = getProbeConfigOffset(probeIndex, offsetof( __eeprom_probe, name));
  if (ofs != 0)
    eeprom_write_block(name, (void *)ofs, PROBE_NAME_SIZE);
}

void loadProbeName(unsigned char probeIndex)
{
  unsigned char ofs = getProbeConfigOffset(probeIndex, offsetof( __eeprom_probe, name));
  if (ofs != 0)
    eeprom_read_block(editString, (void *)ofs, PROBE_NAME_SIZE);
}

void storeSetPoint(int sp)
{
  // If the setpoint is >0 that's an actual setpoint.  
  // 0 or less is a manual fan speed
  boolean isManualMode;
  if (sp > 0)
  {
    config_store_word(setPoint, sp);
    pid.setSetPoint(sp);
    
    isManualMode = false;
  }
  else
  {
    pid.setPidOutput(-sp);
    isManualMode = true;
  }

  config_store_byte(manualMode, isManualMode);
}

static void storePidUnits(char units)
{
  pid.setUnits(units);
  if (units == 'C' || units == 'F')
    config_store_byte(pidUnits, units);
}

static void storeProbeOffset(unsigned char probeIndex, int offset)
{
  unsigned char ofs = getProbeConfigOffset(probeIndex, offsetof( __eeprom_probe, tempOffset));
  if (ofs != 0)
  {
    pid.Probes[probeIndex]->Offset = offset;
    eeprom_write_byte((unsigned char *)ofs, offset);
  }  
}

static void storeProbeType(unsigned char probeIndex, unsigned char probeType)
{
  unsigned char ofs = getProbeConfigOffset(probeIndex, offsetof( __eeprom_probe, probeType));
  if (ofs != 0)
  {
    pid.Probes[probeIndex]->setProbeType(probeType);
    eeprom_write_byte((unsigned char *)ofs, probeType);
  }
}

#ifdef HEATERMETER_RFM12
static void reportRfMap(void)
{
  print_P(PSTR("HMRM"));
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
  {
    Serial_csv();
    if (pid.Probes[i]->getProbeType() == PROBETYPE_RF12)
      SerialX.print(rfMap[i], DEC);
  }
  Serial_nl();
}

static void checkInitRfManager(void)
{
  if (pid.countOfType(PROBETYPE_RF12) != 0)
    rfmanager.init(HEATERMETER_RFM12);
}

static void storeRfMap(unsigned char probeIndex, unsigned char source)
{
  rfMap[probeIndex] = source;

  unsigned char *ofs = (unsigned char *)offsetof(__eeprom_data, rfMap);
  ofs += probeIndex;
  eeprom_write_byte(ofs, source);

  reportRfMap();
  checkInitRfManager();
}
#endif /* HEATERMETER_RFM12 */

static void storeProbeTypeOrMap(unsigned char probeIndex, unsigned char probeType)
{
  /* If probeType is < 128 it is just a probe type */
  if (probeType < 128)
  {
    unsigned char oldProbeType = pid.Probes[probeIndex]->getProbeType();
    if (oldProbeType != probeType)
    {
      storeProbeType(probeIndex, probeType);
#ifdef HEATERMETER_RFM12
      if (oldProbeType == PROBETYPE_RF12)
        storeRfMap(probeIndex, RFSOURCEID_ANY);
#endif /* HEATERMETER_RFM12 */
    }
  }  /* if probeType */
  
#ifdef HEATERMETER_RFM12
  /* If probeType > 128 then it is an wireless probe and the value is 128+source ID */
  else
  {
    unsigned char newSrc = probeType - 128;
    /* Force the storage of TempProbe::setProbeType() if the src changes
       because we need to clear Temperature and any accumulated ADC readings */
    if (pid.Probes[probeIndex]->getProbeType() != PROBETYPE_RF12 ||
      rfMap[probeIndex] != newSrc)
      storeProbeType(probeIndex, PROBETYPE_RF12);
    storeRfMap(probeIndex, newSrc);
  }  /* if RF map */
#endif /* HEATERMETER_RFM12 */
}

static void storeMinFanSpeed(unsigned char minFanSpeed)
{
  pid.setMinFanSpeed(minFanSpeed);
  config_store_byte(minFanSpeed, minFanSpeed);
}

static void storeMaxFanSpeed(unsigned char maxFanSpeed)
{
  pid.setMaxFanSpeed(maxFanSpeed);
  config_store_byte(maxFanSpeed, maxFanSpeed);
}

static void storeMinServoPos(unsigned char minServoPos)
{
  pid.setMinServoPos(minServoPos);
  config_store_byte(minServoPos, minServoPos);
}

static void storeMaxServoPos(unsigned char maxServoPos)
{
  pid.setMaxServoPos(maxServoPos);
  config_store_byte(maxServoPos, maxServoPos);
}

static void storeInvertPidOutput(unsigned char pidOutputFlags)
{
  pid.setOutputFlags(pidOutputFlags);
  config_store_byte(pidOutputFlags, pidOutputFlags);
}

void storeLcdBacklight(unsigned char lcdBacklight)
{
  setLcdBacklight(lcdBacklight);
  config_store_byte(lcdBacklight, lcdBacklight);
}

static void storeLedConf(unsigned char led, unsigned char ledConf)
{
  ledmanager.setAssignment(led, ledConf);

  unsigned char *ofs = (unsigned char *)offsetof(__eeprom_data, ledConf);
  ofs += led;
  eeprom_write_byte(ofs, ledConf);
}

static void toneEnable(boolean enable)
{
#ifdef PIEZO_HZ
  if (enable)
  {
    if (tone_idx != 0xff)
      return;
    tone_last = 0;
    tone_idx = tone_cnt - 1;
  }
  else
  {
    tone_idx = 0xff;
    noTone(PIN_ALARM);
    setLcdBacklight(g_LcdBacklight);
  }
#endif /* PIEZO_HZ */
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
      const char PROGMEM *numData = NUMS + ((uval % 10) * 6);

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

static boolean isMenuHomeState(void)
{
  state_t state = Menus.getState();
  return (state >= ST_HOME_FOOD1 && state <= ST_HOME_ALARM);
}

void updateDisplay(void)
{
  // Updates to the temperature can come at any time, only update 
  // if we're in a state that displays them
  state_t state = Menus.getState();
  if (!isMenuHomeState())
    return;

  char buffer[17];
  unsigned char probeIdxLow, probeIdxHigh;

  // Fixed pit area
  lcd.setCursor(0, 0);
  if (state == ST_HOME_ALARM)
  {
    toneEnable(true);
    if (ALARM_ID_TO_IDX(g_AlarmId) == ALARM_IDX_LOW)
      lcdprint_P(PSTR("** ALARM LOW  **"), false);
    else
      lcdprint_P(PSTR("** ALARM HIGH **"), false);

    probeIdxLow = probeIdxHigh = ALARM_ID_TO_PROBE(g_AlarmId);
  }  /* if ST_HOME_ALARM */
  else
  {
    toneEnable(false);

    /* Big Number probes overwrite the whole display if it has a temperature */
    if (g_HomeDisplayMode >= TEMP_PIT && g_HomeDisplayMode <= TEMP_AMB)
    {
      TempProbe *probe = pid.Probes[g_HomeDisplayMode];
      if (probe->hasTemperature())
      {
        lcdPrintBigNum(probe->Temperature);
        return;
      }
    }

    /* Default Pit / Fan Speed first line */
    int pitTemp = pid.Probes[TEMP_PIT]->Temperature;
    if (!pid.getManualOutputMode() && pitTemp == 0)
      memcpy_P(buffer, LCD_LINE1_UNPLUGGED, sizeof(LCD_LINE1_UNPLUGGED));
    else if (pid.LidOpenResumeCountdown > 0)
      snprintf_P(buffer, sizeof(buffer), PSTR("Pit:%3d"DEGREE"%c Lid%3u"),
        pitTemp, pid.getUnits(), pid.LidOpenResumeCountdown);
    else
    {
      char c1,c2;
      if (pid.getManualOutputMode())
      {
        c1 = '^';  // LCD_ARROWUP
        c2 = '^';  // LCD_ARROWDN
      }
      else
      {
        c1 = '[';
        c2 = ']';
      }
      snprintf_P(buffer, sizeof(buffer), PSTR("Pit:%3d"DEGREE"%c %c%3u%%%c"),
        pitTemp, pid.getUnits(), c1, pid.getPidOutput(), c2);
    }

    lcd.print(buffer);
    // Display mode 0xff is 2-line, which only has space for 1 non-pit value
    if (g_HomeDisplayMode == 0xff)
      probeIdxLow = probeIdxHigh = state - ST_HOME_FOOD1 + TEMP_FOOD1;
    else
    {
      // Display mode 0xfe is 4 line home, display 3 other temps there
      probeIdxLow = TEMP_FOOD1;
      probeIdxHigh = TEMP_AMB;
    }
  } /* if !ST_HOME_ALARM */

  // Rotating probe display
  for (unsigned char probeIndex=probeIdxLow; probeIndex<=probeIdxHigh; ++probeIndex)
  {
    if (probeIndex < TEMP_COUNT && pid.Probes[probeIndex]->hasTemperature())
    {
      loadProbeName(probeIndex);
      snprintf_P(buffer, sizeof(buffer), PSTR("%-12s%3d"DEGREE), editString,
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

void lcdprint_P(const char PROGMEM *p, const boolean doClear)
{
  if (doClear)
    lcd.clear();
  while (unsigned char c = pgm_read_byte(p++)) lcd.write(c);
}

static void storePidParam(char which, float value)
{
  unsigned char k;
  switch (which)
  {
    case 'b': k = 0; break;
    case 'p': k = 1; break;
    case 'i': k = 2; break;
    case 'd': k = 3; break;
    default:
      return;
  }
  pid.setPidConstant(k, value);

  unsigned char ofs = offsetof(__eeprom_data, pidConstants[0]);
  eeprom_write_block(&pid.Pid[k], (void *)(ofs + k * sizeof(float)), sizeof(value));
}

static void outputCsv(void)
{
#ifdef HEATERMETER_SERIAL
  print_P(PSTR("HMSU" CSV_DELIMITER));
  pid.status();
  Serial_nl();
#endif /* HEATERMETER_SERIAL */
}

#if defined(HEATERMETER_SERIAL)
static void printSciFloat(float f)
{
  // This function could use a rework, it is pretty expensive
  // in terms of space and speed. 
  char exponent = 0;
  bool neg = f < 0.0f;
  if (neg)
    f *= -1.0f;
  while (f < 1.0f)
  {
    --exponent;
    f *= 10.0f;
  }
  while (f >= 10.0f)
  {
    ++exponent;
    f /= 10.0f;
  }
  if (neg)
    f *= -1.0f;
  SerialX.print(f, 7);
  Serial_char('e');
  SerialX.print(exponent, DEC);
}

static void reportProbeCoeff(unsigned char probeIdx)
{
  print_P(PSTR("HMPC" CSV_DELIMITER));
  SerialX.print(probeIdx, DEC);
  Serial_csv();
  
  TempProbe *p = pid.Probes[probeIdx];
  for (unsigned char i=0; i<STEINHART_COUNT; ++i)
  {
    printSciFloat(p->Steinhart[i]);
    Serial_csv();
  }
  SerialX.print(p->getProbeType(), DEC);
  Serial_nl();
}

static void storeProbeCoeff(unsigned char probeIndex, char *vals)
{
  // vals is SteinA(float),SteinB(float),SteinC(float),RKnown(float),probeType+1(int)|probeMap(char+int)
  // If any value is blank, it won't be modified
  unsigned char ofs = getProbeConfigOffset(probeIndex, offsetof( __eeprom_probe, steinhart));
  if (ofs == 0)
    return;
    
  unsigned char idx = 0;
  while (*vals)
  {
    if (idx >= STEINHART_COUNT)
      break;
    if (*vals == ',')
    {
      ++idx;
      ++vals;
      ofs += sizeof(float);
    }
    else
    {
      float *fDest = &pid.Probes[probeIndex]->Steinhart[idx];
      *fDest = atof(vals);
      eeprom_write_block(fDest, (void *)ofs, sizeof(float));
      while (*vals && *vals != ',')
        ++vals;
    }
  }

  if (*vals)
    storeProbeTypeOrMap(probeIndex, atoi(vals));
  reportProbeCoeff(probeIndex);
}

static void reboot(void)
{
  // Once the pin goes low, the avr should reboot
  digitalWrite(PIN_SOFTRESET, LOW);
  // Use the watchdog in case SOFTRESET isn't hooked up (e.g. HM4.0)
  // If hoping to program via Optiboot, this won't work if the WDT trigers the reboot
  cli();
  WDTCSR = bit(WDCE) | bit(WDE);
  WDTCSR = bit(WDE) | WDTO_30MS;
  while (1) { };
}

static void reportProbeNames(void)
{
  print_P(PSTR("HMPN"));
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
  {
    loadProbeName(i);
    Serial_csv();
    SerialX.print(editString);
  }
  Serial_nl();
}

static void reportPidParams(void)
{
  print_P(PSTR("HMPD"));
  for (unsigned char i=0; i<4; ++i)
  {
    Serial_csv();
    //printSciFloat(pid.Pid[i]);
    SerialX.print(pid.Pid[i], 8);
  }
  Serial_nl();
}

static void reportProbeOffsets(void)
{
  print_P(PSTR("HMPO"));
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
  {
    Serial_csv();
    SerialX.print(pid.Probes[i]->Offset, DEC);
  }
  Serial_nl();
}

void storeAndReportProbeOffset(unsigned char probeIndex, int offset)
{
  storeProbeOffset(probeIndex, offset);
  reportProbeOffsets();
}

void storeAndReportProbeName(unsigned char probeIndex, char *name)
{
  storeProbeName(probeIndex, name);
  reportProbeNames();
}

static void reportVersion(void)
{
  print_P(PSTR("UCID" CSV_DELIMITER "HeaterMeter" CSV_DELIMITER HM_VERSION));
  SerialX.print(HM_BOARD_REV);
  Serial_nl();
}

static void reportLidParameters(void)
{
  print_P(PSTR("HMLD" CSV_DELIMITER));
  SerialX.print(pid.LidOpenOffset, DEC);
  Serial_csv();
  SerialX.print(pid.getLidOpenDuration(), DEC);
  Serial_nl();
}

void reportLcdParameters(void)
{
  print_P(PSTR("HMLB" CSV_DELIMITER));
  SerialX.print(g_LcdBacklight, DEC);
  Serial_csv();
  SerialX.print(g_HomeDisplayMode, DEC);
  for (unsigned char i=0; i<LED_COUNT; ++i)
  {
    Serial_csv();
    SerialX.print(ledmanager.getAssignment(i), DEC);
  }
  Serial_nl();
}

void storeLcdParam(unsigned char idx, int val)
{
  switch (idx)
  {
    case 0:
      storeLcdBacklight(val);
      break;
    case 1:
      g_HomeDisplayMode = val;
      config_store_byte(homeDisplayMode, g_HomeDisplayMode);
      // If we're in home, clear in case we're switching from 4 to 2
      if (isMenuHomeState())
        lcd.clear();
    case 2:
    case 3:
    case 4:
    case 5:
      storeLedConf(idx - 2, val);
      break;
  }
}

static void reportProbeCoeffs(void)
{
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
    reportProbeCoeff(i);
}

static void reportAlarmLimits(void)
{
#ifdef HEATERMETER_SERIAL
  print_P(PSTR("HMAL"));
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
  {
    ProbeAlarm &a = pid.Probes[i]->Alarms;
    Serial_csv();
    SerialX.print(a.getLow(), DEC);
    if (a.getLowRinging()) Serial_char('L');
    Serial_csv();
    SerialX.print(a.getHigh(), DEC);
    if (a.getHighRinging()) Serial_char('H');
  }
  Serial_nl();
#endif
}

static void reportFanParams(void)
{
  print_P(PSTR("HMFN" CSV_DELIMITER));
  SerialX.print(pid.getMinFanSpeed(), DEC);
  Serial_csv();
  SerialX.print(pid.getMaxFanSpeed(), DEC);
  Serial_csv();
  SerialX.print(pid.getMinServoPos(), DEC);
  Serial_csv();
  SerialX.print(pid.getMaxServoPos(), DEC);
  Serial_csv();
  SerialX.print(pid.getOutputFlags(), DEC);
  Serial_nl();
}

void storeAndReportMaxFanSpeed(unsigned char maxFanSpeed)
{
  storeMaxFanSpeed(maxFanSpeed);
  reportFanParams();
}

static void reportConfig(void)
{
  reportVersion();
  reportPidParams();
  reportFanParams();
  reportProbeNames();
  reportProbeCoeffs();
  reportProbeOffsets();
  reportLidParameters();
  reportLcdParameters();
  reportAlarmLimits();
#ifdef HEATERMETER_RFM12
  reportRfMap();  
#endif /* HEATERMETER_RFM12 */
}

typedef void (*csv_int_callback_t)(unsigned char idx, int val);

static void csvParseI(char *vals, csv_int_callback_t c)
{
  unsigned char idx = 0;
  while (*vals)
  {
    if (*vals == ',')
    {
      ++idx;
      ++vals;
    }
    else
    {
      int val = atoi(vals);
      c(idx, val);
      while (*vals && *vals != ',')
        ++vals;
    }
  }
}

void storeLidParam(unsigned char idx, int val)
{
  if (val < 0)
    val = 0;

  switch (idx)
  {
    case 0:
      pid.LidOpenOffset = val;
      config_store_byte(lidOpenOffset, val);
      break;
    case 1:
      pid.setLidOpenDuration(val);
      config_store_word(lidOpenDuration, val);
      break;
    case 2:
      if (val)
        pid.resetLidOpenResumeCountdown();
      else
        pid.LidOpenResumeCountdown = 0;
      break;
  }
}

/* storeAlarmLimits: Expects pairs of data L,H,L,H,L,H,L,H one for each probe,
   the passed index is coincidently the ALARM_ID */
static void storeAlarmLimits(unsigned char idx, int val)
{
  unsigned char probeIndex = ALARM_ID_TO_PROBE(idx);
  ProbeAlarm &a = pid.Probes[probeIndex]->Alarms;
  unsigned char alarmIndex = ALARM_ID_TO_IDX(idx);
  a.setThreshold(alarmIndex, val);

  unsigned char ofs = getProbeConfigOffset(probeIndex, offsetof( __eeprom_probe, alarmLow));
  if (ofs != 0 && val != 0)
  {
    ofs += alarmIndex * sizeof(val);
    eeprom_write_block(&val, (void *)ofs, sizeof(val));
  }
}

void silenceRingingAlarm(void)
{
  /*
  unsigned char probeIndex = ALARM_ID_TO_PROBE(g_AlarmId);
  ProbeAlarm &a = pid.Probes[probeIndex]->Alarms;
  unsigned char alarmIndex = ALARM_ID_TO_IDX(g_AlarmId);
  storeAlarmLimits(g_AlarmId, disable ? -a.getThreshold(alarmIndex) : 0);
  */
  storeAlarmLimits(g_AlarmId, 0);
  reportAlarmLimits();
}

static void storeFanParams(unsigned char idx, int val)
{
  switch (idx)
  {
    case 0:
      storeMinFanSpeed(val);
      break;
    case 1:
      storeMaxFanSpeed(val);
      break;
    case 2:
      storeMinServoPos(val);
      break;
    case 3:
      storeMaxServoPos(val);
      break;
    case 4:
      storeInvertPidOutput(val);
      break;
  }
}

static void setTempParam(unsigned char idx, int val)
{
  switch (idx)
  {
    case 0:
      g_LogPidInternals = val;
      break;
  }
}

static void handleCommandUrl(char *URL)
{
  unsigned char urlLen = strlen(URL);
  if (strncmp_P(URL, PSTR("set?sp="), 7) == 0) 
  {
    storeSetPoint(atoi(URL + 7));
    storePidUnits(URL[urlLen-1]);
  }
  else if (strncmp_P(URL, PSTR("set?lb="), 7) == 0)
  {
    csvParseI(URL + 7, storeLcdParam);
    reportLcdParameters();
  }
  else if (strncmp_P(URL, PSTR("set?ld="), 7) == 0)
  {
    csvParseI(URL + 7, storeLidParam);
    reportLidParameters();
  }
  else if (strncmp_P(URL, PSTR("set?po="), 7) == 0)
  {
    csvParseI(URL + 7, storeProbeOffset);
    reportProbeOffsets();
  }
  else if (strncmp_P(URL, PSTR("set?pid"), 7) == 0 && urlLen > 9)
  {
    float f = atof(URL + 9);
    storePidParam(URL[7], f);
    reportPidParams();
  }
  else if (strncmp_P(URL, PSTR("set?pn"), 6) == 0 && urlLen > 8)
  {
    // Store probe name will only store it if a valid probe number is passed
    storeAndReportProbeName(URL[6] - '0', URL + 8);
  }
  else if (strncmp_P(URL, PSTR("set?pc"), 6) == 0 && urlLen > 8)
  {
    storeProbeCoeff(URL[6] - '0', URL + 8);
  }
  else if (strncmp_P(URL, PSTR("set?al="), 7) == 0)
  {
    csvParseI(URL + 7, storeAlarmLimits);
    reportAlarmLimits();
  }
  else if (strncmp_P(URL, PSTR("set?fn="), 7) == 0)
  {
    csvParseI(URL + 7, storeFanParams);
    reportFanParams();
  }
  else if (strncmp_P(URL, PSTR("set?tt="), 7) == 0)
  {
    Menus.displayToast(URL+7);
  }
  else if (strncmp_P(URL, PSTR("set?tp="), 7) == 0)
  {
    csvParseI(URL + 7, setTempParam);
  }
  else if (strncmp_P(URL, PSTR("config"), 6) == 0)
  {
    reportConfig();
  }
  else if (strncmp_P(URL, PSTR("reboot"), 5) == 0)
  {
    reboot();
    // reboot doesn't return
  }
}
#endif /* defined(HEATERMETER_SERIAL) */

static void outputRfStatus(void)
{
#if defined(HEATERMETER_SERIAL) && defined(HEATERMETER_RFM12)
  rfmanager.status();
#endif /* defined(HEATERMETER_SERIAL) && defined(HEATERMETER_RFM12) */
}

#ifdef HEATERMETER_RFM12
static void rfSourceNotify(RFSource &r, unsigned char event)
{
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
    if ((pid.Probes[i]->getProbeType() == PROBETYPE_RF12) &&
    ((rfMap[i] == RFSOURCEID_ANY) || (rfMap[i] == r.getId()))
    )
    {
      if (event == RFEVENT_Remove)
        pid.Probes[i]->addAdcValue(0);
      else if (r.isNative())
        pid.Probes[i]->setTemperatureC(r.Value / 10.0f);
      else
      {
        unsigned int val = r.Value;
        unsigned char adcBits = rfmanager.getAdcBits();
        // If the remote is lower resolution then shift it up to our resolution
        if (adcBits < pid.getAdcBits())
          val <<= (pid.getAdcBits() - adcBits);
        pid.Probes[i]->addAdcValue(val);
      }
    } /* if probe is this source */

  if (event & (RFEVENT_Add | RFEVENT_Remove))
    outputRfStatus();
}
#endif /* HEATERMETER_RFM12 */

static void tone_doWork(void)
{
#ifdef PIEZO_HZ
  if (tone_idx == 0xff)
    return;
  unsigned long t = millis();
  unsigned int dur = pgm_read_byte(&tone_durs[tone_idx]) * 10;
  if (t - tone_last > dur)
  {
    tone_last = t;
    tone_idx = (tone_idx + 1) % tone_cnt;
    if (tone_idx % 2 == 0)
    {
      dur = pgm_read_byte(&tone_durs[tone_idx]) * 10;
      tone(PIN_ALARM, PIEZO_HZ, dur);
      setLcdBacklight(0x80 | 0);
    }
    else
      setLcdBacklight(0x80 | g_LcdBacklight);
  }
#endif /* PIEZO_HZ */
}

static void checkAlarms(void)
{
  boolean anyRinging = false;
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
  {
    for (unsigned char j=ALARM_IDX_LOW; j<=ALARM_IDX_HIGH; ++j)
    {
      boolean ringing = pid.Probes[i]->Alarms.Ringing[j];
      unsigned char alarmId = MAKE_ALARM_ID(i, j);
      if (ringing)
      {
        anyRinging = true;
        g_AlarmId = alarmId;
      }
      ledmanager.publish(LEDSTIMULUS_Alarm0L + alarmId, ringing);
    }
  }

  ledmanager.publish(LEDSTIMULUS_AlarmAny, anyRinging);
  if (anyRinging)
  {
    reportAlarmLimits();
    Menus.setState(ST_HOME_ALARM);
  }
  else if (Menus.getState() == ST_HOME_ALARM)
    // No alarms ringing, return to HOME
    Menus.setState(ST_HOME_FOOD1);
}

static void eepromLoadBaseConfig(unsigned char forceDefault)
{
  // The compiler likes to join eepromLoadBaseConfig and eepromLoadProbeConfig s
  // this union saves stack space by reusing the same memory area for both structs
  union {
    struct __eeprom_data base;
    struct __eeprom_probe probe;
  } config;

  eeprom_read_block(&config.base, 0, sizeof(__eeprom_data));
  forceDefault = forceDefault || config.base.magic != EEPROM_MAGIC;
  if (forceDefault != 0)
  {
    memcpy_P(&config.base, &DEFAULT_CONFIG[forceDefault - 1], sizeof(__eeprom_data));
    eeprom_write_block(&config.base, 0, sizeof(__eeprom_data));
  }
  
  pid.setSetPoint(config.base.setPoint);
  pid.LidOpenOffset = config.base.lidOpenOffset;
  pid.setLidOpenDuration(config.base.lidOpenDuration);
  memcpy(pid.Pid, config.base.pidConstants, sizeof(config.base.pidConstants));
  if (config.base.manualMode)
    pid.setPidOutput(0);
  setLcdBacklight(config.base.lcdBacklight);
#ifdef HEATERMETER_RFM12
  memcpy(rfMap, config.base.rfMap, sizeof(rfMap));
#endif
  pid.setUnits(config.base.pidUnits == 'C' ? 'C' : 'F');
  pid.setMinFanSpeed(config.base.minFanSpeed);
  pid.setMaxFanSpeed(config.base.maxFanSpeed);
  pid.setOutputFlags(config.base.pidOutputFlags);
  g_HomeDisplayMode = config.base.homeDisplayMode;
  pid.setMinServoPos(config.base.minServoPos);
  pid.setMaxServoPos(config.base.maxServoPos);

  for (unsigned char led = 0; led<LED_COUNT; ++led)
    ledmanager.setAssignment(led, config.base.ledConf[led]);
}

static void eepromLoadProbeConfig(unsigned char forceDefault)
{
  // The compiler likes to join eepromLoadBaseConfig and eepromLoadProbeConfig s
  // this union saves stack space by reusing the same memory area for both structs
  union {
    struct __eeprom_data base;
    struct __eeprom_probe probe;
  } config;

  // instead of this use below because we don't have eeprom_read_word linked yet
  //magic = eeprom_read_word((uint16_t *)(EEPROM_PROBE_START-sizeof(magic))); 
  eeprom_read_block(&config.base, (void *)(EEPROM_PROBE_START-sizeof(config.base.magic)), sizeof(config.base.magic));
  if (config.base.magic != EEPROM_MAGIC)
  {
    forceDefault = 1;
    eeprom_write_word((uint16_t *)(EEPROM_PROBE_START-sizeof(config.base.magic)), EEPROM_MAGIC);
  }
    
  struct  __eeprom_probe *p;
  p = (struct  __eeprom_probe *)(EEPROM_PROBE_START);
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
  {
    if (forceDefault != 0)
    {
      memcpy_P(&config.probe, &DEFAULT_PROBE_CONFIG, sizeof( __eeprom_probe));
      // Hardcoded to change the last character of the string instead of [strlen(config.name)-1]
      config.probe.name[6] = '0' + i;
      eeprom_write_block(&config.probe, p, sizeof(__eeprom_probe));
    }
    else
      eeprom_read_block(&config.probe, p, sizeof(__eeprom_probe));

    pid.Probes[i]->loadConfig(&config.probe);
    ++p;
  }  /* for i<TEMP_COUNT */
}

void eepromLoadConfig(unsigned char forceDefault)
{
  eepromLoadBaseConfig(forceDefault);
  eepromLoadProbeConfig(forceDefault);
}

static void blinkLed(void)
{
  // This function only works the first time, when all the LEDs are assigned to
  // LedStimulus::Off, and OneShot turns them on for one blink
  ledmanager.publish(LEDSTIMULUS_Off, LEDACTION_OneShot);
  ledmanager.doWork();
}

#ifdef HEATERMETER_SERIAL
static void serial_doWork(void)
{
  unsigned char len = strlen(g_SerialBuff);
  while (Serial.available())
  {
    char c = Serial.read();
    // support CR, LF, or CRLF line endings
    if (c == '\n' || c == '\r')  
    {
      if (len != 0 && g_SerialBuff[0] == '/')
        handleCommandUrl(&g_SerialBuff[1]);
      len = 0;
    }
    else {
      g_SerialBuff[len++] = c;
      // if the buffer fills without getting a newline, just reset
      if (len >= sizeof(g_SerialBuff))
        len = 0;
    }
    g_SerialBuff[len] = '\0';
  }  /* while Serial */
}
#endif  /* HEATERMETER_SERIAL */

/* Starts a debug log output message line, end with Debug_end() */
void Debug_begin(void)
{
    print_P(PSTR("HMLG" CSV_DELIMITER "0" CSV_DELIMITER));
}

static void newTempsAvail(void)
{
  static unsigned char pidCycleCount;

  updateDisplay();
  ++pidCycleCount;
    
  if ((pidCycleCount % 0x20) == 0)
    outputRfStatus();

  outputCsv();
  // We want to report the status before the alarm readout so
  // receivers can tell what the value was that caused the alarm
  checkAlarms();

  if (g_LogPidInternals)
    pid.pidStatus();

  ledmanager.publish(LEDSTIMULUS_Off, LEDACTION_Off);
  ledmanager.publish(LEDSTIMULUS_LidOpen, pid.isLidOpen());
  ledmanager.publish(LEDSTIMULUS_FanOn, pid.isOutputActive());
  ledmanager.publish(LEDSTIMULUS_FanMax, pid.isOutputMaxed());
  ledmanager.publish(LEDSTIMULUS_PitTempReached, pid.isPitTempReached());

#ifdef HEATERMETER_RFM12
  rfmanager.sendUpdate(pid.getPidOutput());
#endif
}

static void lcdDefineChars(void)
{
  for (unsigned char i=0; i<8; ++i)
    lcd.createChar_P(i, BIG_CHAR_PARTS + (i * 8));
}

static void ledExecutor(unsigned char led, unsigned char on)
{
  switch (led)
  {
    case 0:
      digitalWrite(PIN_WIRELESS_LED, on);
      break;
    default:
      lcd.digitalWrite(led - 1, on);
      break;
  }
}

void hmcoreSetup(void)
{
  pinMode(PIN_WIRELESS_LED, OUTPUT);
  blinkLed();
  
#ifdef HEATERMETER_SERIAL
  Serial.begin(HEATERMETER_SERIAL);
  // don't use SerialX because we don't want any preamble
  Serial.write('\n');
  reportVersion();
#endif  /* HEATERMETER_SERIAL */
#ifdef USE_EXTERNAL_VREF  
  analogReference(EXTERNAL);
#endif  /* USE_EXTERNAL_VREF */
  // Disable Analog Comparator
  ACSR = bit(ACD);
  // Disable Digital Input on ADC pins
  DIDR0 = bit(ADC5D) | bit(ADC4D) | bit(ADC3D) | bit(ADC2D) | bit(ADC1D) | bit(ADC0D);
  // And other unused units
  power_twi_disable();

  // Switch the pin mode first to INPUT with internal pullup
  // to take it to 5V before setting the mode to OUTPUT. 
  // If we reverse this, the pin will go OUTPUT,LOW and reboot.
  // SoftReset and WiShield are mutually exlusive, but it is HIGH/OUTPUT too
  digitalWrite(PIN_SOFTRESET, HIGH);
  pinMode(PIN_SOFTRESET, OUTPUT);
  
  pid.Probes[TEMP_PIT] = &probe0;
  pid.Probes[TEMP_FOOD1] = &probe1;
  pid.Probes[TEMP_FOOD2] = &probe2;
  pid.Probes[TEMP_AMB] = &probe3;
  pid.init();

  eepromLoadConfig(0);
  lcdDefineChars();
#ifdef HEATERMETER_RFM12
  checkInitRfManager();
#endif

  Menus.setState(ST_HOME_NOPROBES);
}

void hmcoreLoop(void)
{ 
#ifdef HEATERMETER_SERIAL 
  serial_doWork();
#endif /* HEATERMETER_SERIAL */

#ifdef HEATERMETER_RFM12
  if (rfmanager.doWork())
    ledmanager.publish(LEDSTIMULUS_RfReceive, LEDACTION_OneShot);
#endif /* HEATERMETER_RFM12 */

  Menus.doWork();
  if (pid.doWork())
    newTempsAvail();
  tone_doWork();
  ledmanager.doWork();
}
