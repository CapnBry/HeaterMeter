// HeaterMeter Copyright 2019 Bryan Mayland <bmayland@capnbry.net>
#include "Arduino.h"
#include "econfig.h"
#include <avr/wdt.h>
#include <avr/power.h>
#include <digitalWriteFast.h>

#include "hmcore.h"

#ifdef HEATERMETER_RFM12
#include "rfmanager.h"
#endif

#include "ledmanager.h"
#include "tone_4khz.h"

static TempProbe probe0(PIN_PIT);
static TempProbe probe1(PIN_FOOD1);
static TempProbe probe2(PIN_FOOD2);
static TempProbe probe3(PIN_AMB);
GrillPid pid;

#ifdef SHIFTREGLCD_NATIVE
ShiftRegLCD lcd(PIN_LCD_BACKLGHT, PIN_SERVO, PIN_LCD_CLK, TWO_WIRE, 2);
#else
ShiftRegLCD lcd(PIN_LCD_BACKLGHT, PIN_LCD_CLK, 2);
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

#define config_store_byte(eeprom_field, src) { econfig_write_byte((void *)offsetof(__eeprom_data, eeprom_field), src); }
#define config_store_word(eeprom_field, src) { econfig_write_word((void *)offsetof(__eeprom_data, eeprom_field), src); }

#define EEPROM_MAGIC 0xf00e

static const struct __eeprom_data {
  unsigned int magic;
  int setPoint;
  unsigned char lidOpenOffset;
  unsigned int lidOpenDuration;
  float pidConstants[4]; // constants are stored Kb, Kp, Ki, Kd
  unsigned char pidMode;
  unsigned char lcdBacklight; // in PWM (max 100)
#ifdef HEATERMETER_RFM12
  unsigned char rfMap[TEMP_COUNT];
#endif
  char pidUnits;
  unsigned char fanMinSpeed;  // in percent
  unsigned char fanMaxSpeed;  // in percent
  unsigned char pidOutputFlags;
  unsigned char homeDisplayMode;
  unsigned char fanMaxStartupSpeed; // in percent
  unsigned char ledConf[LED_COUNT];
  unsigned char servoMinPos;  // in 10us
  unsigned char servoMaxPos;  // in 10us
  unsigned char fanActiveFloor; // in percent
  unsigned char servoActiveCeil; // in percent
} DEFAULT_CONFIG[] PROGMEM = {
 {
  EEPROM_MAGIC,  // magic
  225,  // setpoint
  6,    // lid open offset %
  240,  // lid open duration
  { 0.0f, 4.0f, 0.02f, 5.0f },  // PID constants
  PIDMODE_STARTUP,  // PID mode
  50,   // lcd backlight (%)
#ifdef HEATERMETER_RFM12
  { RFSOURCEID_ANY, RFSOURCEID_ANY, RFSOURCEID_ANY, RFSOURCEID_ANY },  // rfMap
#endif
  'F',  // Units
  0,    // min fan speed
  100,  // max fan speed
  bit(PIDFLAG_FAN_FEEDVOLT), // PID output flags bitmask
  0xff, // 2-line home
  100, // max startup fan speed
  { LEDSTIMULUS_FanMax, LEDSTIMULUS_LidOpen, LEDSTIMULUS_FanOn, LEDSTIMULUS_Off },
  150-50, // min servo pos = 1000us
  150+50,  // max servo pos = 2000us
  0, // fan active floor
  100 // servo active ceil
}
};

// EEPROM address of the start of the probe structs, the 2 bytes before are magic
#define EEPROM_PROBE_START  64

static const struct  __eeprom_probe DEFAULT_PROBE_CONFIG PROGMEM = {
  "Probe  ", // Name if you change this change the hardcoded number-appender in eepromLoadProbeConfig()
  PROBETYPE_INTERNAL,  // probeType
  0,  // unused1, was offset
  -40,  // alarm low
  -200, // alarm high
  0,  // offset
  {
    //2.4723753e-4,2.3402251e-4,1.3879768e-7  // Maverick ET-72/73
    //5.2668241e-4,2.0037400e-4,2.5703090e-8 // Maverick ET-732
    //8.98053228e-4,2.49263324e-4,2.04047542e-7 // Radio Shack 10k
    //1.14061e-3,2.32134e-4,9.63666e-8 // Vishay 10k NTCLE203E3103FB0
    //7.2237825e-4,2.1630182e-4,9.2641029e-8 // EPCOS100k
    //8.1129016e-4,2.1135575e-4,7.1761474e-8 // Semitec 104GT-2
    7.3431401e-4,2.1574370e-4,9.5156860e-8 // ThermoWorks Pro-Series
    ,1.0e+4
  }
};

#ifdef PIEZO_HZ
// A simple beep-beep-beep-(pause) alarm
static const unsigned char tone_durs[] PROGMEM = { 10, 5, 10, 5, 10, 50 };  // in 10ms units
#define tone_cnt (sizeof(tone_durs)/sizeof(tone_durs[0]))
static unsigned char tone_idx = 0xff;
static unsigned long tone_last;
#endif /* PIZEO_HZ */

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
    econfig_write_block(name, (void *)(uintptr_t)ofs, PROBE_NAME_SIZE);
}

void loadProbeName(unsigned char probeIndex)
{
  unsigned char ofs = getProbeConfigOffset(probeIndex, offsetof( __eeprom_probe, name));
  if (ofs != 0)
    econfig_read_block(editString, (void *)(uintptr_t)ofs, PROBE_NAME_SIZE);
}

void storePidMode()
{
  unsigned char mode = pid.getPidMode();
  if (mode <= PIDMODE_AUTO_LAST)
    mode = PIDMODE_STARTUP;
  config_store_byte(pidMode, mode);
}

void storeSetPoint(int sp)
{
  // If the setpoint is >0 that's an actual setpoint.  
  // 0 or less is a manual fan speed
  if (sp > 0)
  {
    config_store_word(setPoint, sp);
    pid.setSetPoint(sp);
  }
  else
  {
    pid.setPidOutput(-sp);
  }

  storePidMode();
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
    econfig_write_word((void *)(uintptr_t)ofs, offset);
  }  
}

static void storeProbeOffsetLegacy(unsigned char probeIndex)
{
  // This forces the old probe offset storage location to 0
  unsigned char ofs = getProbeConfigOffset(probeIndex, offsetof(__eeprom_probe, unused1));
  if (ofs != 0)
    econfig_write_byte((void*)(uintptr_t)ofs, 0);
}

static void storeProbeType(unsigned char probeIndex, unsigned char probeType)
{
  unsigned char ofs = getProbeConfigOffset(probeIndex, offsetof( __eeprom_probe, probeType));
  if (ofs != 0)
  {
    pid.setProbeType(probeIndex, probeType);
    econfig_write_byte((void *)(uintptr_t)ofs, probeType);
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
  econfig_write_byte(ofs, source);

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

static void storeFanMinSpeed(unsigned char fanMinSpeed)
{
  pid.setFanMinSpeed(fanMinSpeed);
  config_store_byte(fanMinSpeed, pid.getFanMinSpeed());
}

static void storeFanMaxSpeed(unsigned char fanMaxSpeed)
{
  pid.setFanMaxSpeed(fanMaxSpeed);
  config_store_byte(fanMaxSpeed, pid.getFanMaxSpeed());
}

static void storeFanMaxStartupSpeed(unsigned char fanMaxStartupSpeed)
{
  pid.setFanMaxStartupSpeed(fanMaxStartupSpeed);
  config_store_byte(fanMaxStartupSpeed, pid.getFanMaxStartupSpeed());
}

static void storeFanActiveFloor(unsigned char fanActiveFloor)
{
  pid.setFanActiveFloor(fanActiveFloor);
  config_store_byte(fanActiveFloor, pid.getFanActiveFloor());
}

static void storeServoActiveCeil(unsigned char servoActiveCeil)
{
  pid.setServoActiveCeil(servoActiveCeil);
  config_store_byte(servoActiveCeil, pid.getServoActiveCeil());
}

static void storeServoMinPos(unsigned char servoMinPos)
{
  pid.setServoMinPos(servoMinPos);
  config_store_byte(servoMinPos, servoMinPos);
}

static void storeServoMaxPos(unsigned char servoMaxPos)
{
  pid.setServoMaxPos(servoMaxPos);
  config_store_byte(servoMaxPos, servoMaxPos);
}

static void storePidOutputFlags(unsigned char pidOutputFlags)
{
  pid.setOutputFlags(pidOutputFlags);
  config_store_byte(pidOutputFlags, pidOutputFlags);
}

void storeLcdBacklight(unsigned char lcdBacklight)
{
  lcd.setBacklight(lcdBacklight, true);
  config_store_byte(lcdBacklight, lcdBacklight);
}

static void storeLedConf(unsigned char led, unsigned char ledConf)
{
  ledmanager.setAssignment(led, ledConf);

  unsigned char *ofs = (unsigned char *)offsetof(__eeprom_data, ledConf);
  ofs += led;
  econfig_write_byte(ofs, ledConf);
}

static void toneEnable(boolean enable)
{
#ifdef PIEZO_HZ
  // If (enable and tone_idx != 0xff) || (!enable and tone_idx == 0xff)
  if (enable != (tone_idx == 0xff))
    return;
  if (enable)
  {
    tone_last = 0;
    // Start the tone with the last element (the delay)
    // rather than the first beep (cnt-2 vs cnt-1)
    tone_idx = tone_cnt - 2;
  }
  else
  {
    tone_idx = 0xff;
    tone4khz_end();
    lcd.setBacklight(lcd.getBacklight(), false);
  }
#endif /* PIEZO_HZ */
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
  econfig_write_block(&pid.Pid[k], (void *)(ofs + k * sizeof(float)), sizeof(value));
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
  while (f < 1.0f && f != 0.0f)
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
      econfig_write_block(fDest, (void *)(uintptr_t)ofs, sizeof(float));
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
  SerialX.print(lcd.getBacklight(), DEC);
  Serial_csv();
  SerialX.print(Menus.getHomeDisplayMode(), DEC);
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
      Menus.setHomeDisplayMode(val);
      config_store_byte(homeDisplayMode, val);
      break;
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
  SerialX.print(pid.getFanMinSpeed(), DEC);
  Serial_csv();
  SerialX.print(pid.getFanMaxSpeed(), DEC);
  Serial_csv();
  SerialX.print(pid.getServoMinPos(), DEC);
  Serial_csv();
  SerialX.print(pid.getServoMaxPos(), DEC);
  Serial_csv();
  SerialX.print(pid.getOutputFlags(), DEC);
  Serial_csv();
  SerialX.print(pid.getFanMaxStartupSpeed(), DEC);
  Serial_csv();
  SerialX.print(pid.getFanActiveFloor(), DEC);
  Serial_csv();
  SerialX.print(pid.getServoActiveCeil(), DEC);
  Serial_nl();
}

void storeAndReportMaxFanSpeed(unsigned char maxFanSpeed)
{
  storeFanMaxSpeed(maxFanSpeed);
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

  unsigned char ofs = getProbeConfigOffset(probeIndex, offsetof(__eeprom_probe, alarmLow));
  if (ofs != 0 && val != 0)
  {
    ofs += alarmIndex * sizeof(val);
    econfig_write_block(&val, (void *)(uintptr_t)ofs, sizeof(val));
  }
}

void silenceRingingAlarm(void)
{
  storeAlarmLimits(pid.getAlarmId(), 0);
  reportAlarmLimits();
}

static void storeFanParams(unsigned char idx, int val)
{
  switch (idx)
  {
    case 0:
      storeFanMinSpeed(val);
      break;
    case 1:
      storeFanMaxSpeed(val);
      break;
    case 2:
      storeServoMinPos(val);
      break;
    case 3:
      storeServoMaxPos(val);
      break;
    case 4:
      storePidOutputFlags(val);
      break;
    case 5:
      storeFanMaxStartupSpeed(val);
      break;
    case 6:
      storeFanActiveFloor(val);
      break;
    case 7:
      storeServoActiveCeil(val);
      break;
  }
}

static void setTempParam(unsigned char idx, int val)
{
  switch (idx)
  {
    case 0:
      pid.setAutoreportInternals(val);
      break;
    case 1:
      pid.setNoisePin(val);
      break;
  }
}

static void handleCommandUrl(char *URL)
{
  unsigned char urlLen = strlen(URL);

  // Any complete line from the host turns us on(line)
  // Set this first so the code below knows the host is online
  // BRY: Disabled until I rewrite the host side
  //Menus.setHostStateOnline();

  if (strncmp_P(URL, PSTR("set?sp="), 7) == 0)
  {
    // store the units first, in case of 'O' disabling the PID output
    storePidUnits(URL[urlLen - 1]);
    // prevent sending "C" or "F" which would setpoint(0)
    if (*(URL+7) <= '9')
      storeSetPoint(atoi(URL + 7));
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
  else if (strncmp_P(URL, PSTR("set?pid"), 7) == 0)
  {
    if (urlLen > 9)
    {
      float f = atof(URL + 9);
      storePidParam(URL[7], f);
    }
    reportPidParams();
  }
  else if (strncmp_P(URL, PSTR("set?pn"), 6) == 0)
  {
    if (urlLen > 8)
      storeProbeName(URL[6] - '0', URL + 8);
    reportProbeNames();
  }
  else if (strncmp_P(URL, PSTR("set?pc"), 6) == 0 && urlLen > 7)
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
  else if (strncmp_P(URL, PSTR("set?hi="), 7) == 0)
  {
    Menus.hostMsgReceived(URL + 7);
  }
  else if (strncmp_P(URL, PSTR("config"), 6) == 0)
  {
    reportConfig();
  }
  else if (strncmp_P(URL, PSTR("ucid"), 4) == 0)
  {
    reportVersion();
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
        pid.Probes[i]->calcTemp(0);
      else if (r.isNative())
        pid.Probes[i]->setTemperatureC(r.Value / 10.0f);
      else
      {
        unsigned int val = r.Value;
        unsigned char adcBits = rfmanager.getAdcBits();
        // If the remote is lower resolution then shift it up to our resolution
        if (adcBits < pid.getAdcBits())
          val <<= (pid.getAdcBits() - adcBits);
        pid.Probes[i]->calcTemp(val);
      }
    } /* if probe is this source */

  if (event & (RFEVENT_Add | RFEVENT_Remove))
    outputRfStatus();
}
#endif /* HEATERMETER_RFM12 */

static void outputAdcStatus(void)
{
#if defined(HEATERMETER_SERIAL)
  print_P(PSTR("HMAR"));
  for (unsigned char i=0; i<NUM_ANALOG_INPUTS; ++i)
  {
    Serial_csv();
    SerialX.print(analogReadRange(i), DEC);
  }
  Serial_nl();
#endif
}

static void tone_doWork(void)
{
#ifdef PIEZO_HZ
  if (tone_idx == 0xff)
    return;
  unsigned int elapsed = millis() - tone_last;
  unsigned int dur = pgm_read_byte(&tone_durs[tone_idx]) * 10;
  if (elapsed > dur)
  {
    tone_last = millis();
    tone_idx = (tone_idx + 1) % tone_cnt;
    if (tone_idx % 2 == 0)
    {
      dur = pgm_read_byte(&tone_durs[tone_idx]) * 10;
      tone4khz_begin(dur);
      lcd.setBacklight(0, false);
    }
    else
      lcd.setBacklight(lcd.getBacklight(), false);
  }
#endif /* PIEZO_HZ */
}

static void checkAlarms(void)
{
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
  {
    for (unsigned char j=ALARM_IDX_LOW; j<=ALARM_IDX_HIGH; ++j)
    {
      boolean ringing = pid.Probes[i]->Alarms.Ringing[j];
      ledmanager.publish(LEDSTIMULUS_Alarm0L + MAKE_ALARM_ID(i, j), ringing);
    }
  }

  bool anyRinging = pid.getAlarmId() != ALARM_ID_NONE;
  ledmanager.publish(LEDSTIMULUS_AlarmAny, anyRinging);
  if (anyRinging)
  {
    toneEnable(true);
    reportAlarmLimits();
    Menus.setState(ST_HOME_ALARM);
  }
  else
  {
    toneEnable(false);
    if (Menus.getState() == ST_HOME_ALARM)
      Menus.setState(ST_LAST);
  }
}

static void eepromLoadBaseConfig(unsigned char forceDefault)
{
  // The compiler likes to join eepromLoadBaseConfig and eepromLoadProbeConfig s
  // this union saves stack space by reusing the same memory area for both structs
  union {
    struct __eeprom_data base;
    struct __eeprom_probe probe;
  } config;

  econfig_read_block(&config.base, 0, sizeof(__eeprom_data));
  forceDefault = forceDefault || config.base.magic != EEPROM_MAGIC;
  if (forceDefault != 0)
  {
    memcpy_P(&config.base, &DEFAULT_CONFIG[forceDefault - 1], sizeof(__eeprom_data));
    econfig_write_block(&config.base, 0, sizeof(__eeprom_data));
  }
  
  pid.setSetPoint(config.base.setPoint);
  pid.LidOpenOffset = config.base.lidOpenOffset;
  pid.setLidOpenDuration(config.base.lidOpenDuration);
  memcpy(pid.Pid, config.base.pidConstants, sizeof(config.base.pidConstants));
  pid.setPidMode(config.base.pidMode);
  lcd.setBacklight(config.base.lcdBacklight, true);
#ifdef HEATERMETER_RFM12
  memcpy(rfMap, config.base.rfMap, sizeof(rfMap));
#endif
  pid.setUnits(config.base.pidUnits == 'C' ? 'C' : 'F');
  pid.setFanMinSpeed(config.base.fanMinSpeed);
  pid.setFanMaxSpeed(config.base.fanMaxSpeed);
  pid.setFanActiveFloor(config.base.fanActiveFloor);
  pid.setFanMaxStartupSpeed(config.base.fanMaxStartupSpeed);
  pid.setOutputFlags(config.base.pidOutputFlags);
  Menus.setHomeDisplayMode(config.base.homeDisplayMode);
  pid.setServoMinPos(config.base.servoMinPos);
  pid.setServoMaxPos(config.base.servoMaxPos);
  pid.setServoActiveCeil(config.base.servoActiveCeil);

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
  econfig_read_block(&config.base, (void *)(EEPROM_PROBE_START-sizeof(config.base.magic)), sizeof(config.base.magic));
  if (config.base.magic != EEPROM_MAGIC)
  {
    forceDefault = 1;
    econfig_write_word((void *)(EEPROM_PROBE_START-sizeof(config.base.magic)), EEPROM_MAGIC);
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
      econfig_write_block(&config.probe, p, sizeof(__eeprom_probe));
    }
    else
      econfig_read_block(&config.probe, p, sizeof(__eeprom_probe));

    pid.Probes[i]->loadConfig(&config.probe);
    // MIGRATION: Offset used to be signed char, now signed int. If old value is non-zero, 
    // move it to new location and clear old value (30 bytes of code space for this migration)
    if (config.probe.unused1 != 0)
    {
      storeProbeOffset(i, config.probe.unused1);
      storeProbeOffsetLegacy(i);
    }
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
    print_P(PSTR("HMLG" CSV_DELIMITER));
}

void publishLeds(void)
{
  ledmanager.publish(LEDSTIMULUS_Off, LEDACTION_Off);
  ledmanager.publish(LEDSTIMULUS_LidOpen, pid.isLidOpen());
  ledmanager.publish(LEDSTIMULUS_FanOn, pid.isOutputActive());
  ledmanager.publish(LEDSTIMULUS_FanMax, pid.isOutputMaxed());
  ledmanager.publish(LEDSTIMULUS_PitTempReached, pid.isPitTempReached());
  ledmanager.publish(LEDSTIMULUS_Startup, pid.getPidMode() == PIDMODE_STARTUP);
  ledmanager.publish(LEDSTIMULUS_Recovery, pid.getPidMode() == PIDMODE_RECOVERY);
}

static void newTempsAvail(void)
{
  static unsigned char pidCycleCount;

  Menus.updateDisplay();
  ++pidCycleCount;
    
  if ((pidCycleCount % 0x20) == 0)
    outputRfStatus();

  pid.reportStatus();
  // We want to report the status before the alarm readout so
  // receivers can tell what the value was that caused the alarm
  checkAlarms();

  if ((pidCycleCount % 0x04) == 1)
    outputAdcStatus();

  publishLeds();

#ifdef HEATERMETER_RFM12
  rfmanager.sendUpdate(pid.getPidOutput());
#endif
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
  pinModeFast(PIN_WIRELESS_LED, OUTPUT);
  blinkLed();
  
#ifdef HEATERMETER_SERIAL
  Serial.begin(HEATERMETER_SERIAL);
  // don't use SerialX because we don't want any preamble
  Serial.write('\n');
  reportVersion();
#endif  /* HEATERMETER_SERIAL */
  // Disable Analog Comparator
  ACSR = bit(ACD);
  // Disable Digital Input on ADC pins
  DIDR0 = bit(ADC5D) | bit(ADC4D) | bit(ADC3D) | bit(ADC2D) | bit(ADC1D) | bit(ADC0D);
  // And other unused units
  power_twi_disable();

  tone4khz_init();

  pid.Probes[TEMP_PIT] = &probe0;
  pid.Probes[TEMP_FOOD1] = &probe1;
  pid.Probes[TEMP_FOOD2] = &probe2;
  pid.Probes[TEMP_FOOD3] = &probe3;

  eepromLoadConfig(0);
  pid.init();
#ifdef HEATERMETER_RFM12
  checkInitRfManager();
#endif

  Menus.init();
  Menus.setState(ST_BOOT);
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

  if (pid.doWork())
    newTempsAvail();
  Menus.doWork();
  tone_doWork();
  ledmanager.doWork();
}
