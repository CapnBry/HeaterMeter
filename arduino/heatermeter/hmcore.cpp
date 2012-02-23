// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#include "Arduino.h"
#include <avr/eeprom.h>

#include "hmcore.h"

#ifdef HEATERMETER_NETWORKING
#include <WiServer.h>  
#endif

#ifdef HEATERMETER_RFM12
#include "rfmanager.h"
#endif

#ifdef DFLASH_SERVING
#include <dataflash.h>
#include "flashfiles.h"
#endif

#include "strings.h"

static TempProbe probe0(PIN_PIT);
static TempProbe probe1(PIN_FOOD1);
static TempProbe probe2(PIN_FOOD2);
static TempProbe probe3(PIN_AMB);
GrillPid pid(PIN_BLOWER);

#ifdef SHIFTREGLCD_SPI
ShiftRegLCD lcd(PIN_LCD_CLK, 2);
#else
ShiftRegLCD lcd(PIN_LCD_DATA, PIN_LCD_CLK, TWO_WIRE, 2); 
#endif /* SHIFTREGLCD_SPI */

#ifdef HEATERMETER_NETWORKING
static boolean g_NetworkInitialized;
#endif /* HEATERMETER_NETWORKING */
#ifdef HEATERMETER_SERIAL
static char g_SerialBuff[64]; 
#endif /* HEATERMETER_SERIAL */
#ifdef HEATERMETER_RFM12
static void rfSourceNotify(RFSource &r, RFManager::event e); // prototype
static RFManager rfmanager(rfSourceNotify);
static rf12_map_item_t rfMap[TEMP_COUNT];
#endif /* HEATERMETER_RFM12 */

#define config_store_byte(eeprom_field, src) { eeprom_write_byte((uint8_t *)offsetof(__eeprom_data, eeprom_field), src); }
#define config_store_word(eeprom_field, src) { eeprom_write_word((uint16_t *)offsetof(__eeprom_data, eeprom_field), src); }

#define EEPROM_MAGIC 0xf00d

static const struct __eeprom_data {
  unsigned int magic;
  int setPoint;
  unsigned char lidOpenOffset;
  unsigned int lidOpenDuration;
  float pidConstants[4]; // constants are stored Kb, Kp, Ki, Kd
  boolean manualMode;
  unsigned char maxFanSpeed;  // in percent
  unsigned char lcdBacklight; // in PWM (max 255)
#ifdef HEATERMETER_RFM12
  rf12_map_item_t rfMap[TEMP_COUNT];
#endif
  char pidUnits;
} DEFAULT_CONFIG PROGMEM = { 
  EEPROM_MAGIC,  // magic
  225,  // setpoint
  6,  // lid open offset %
  240, // lid open duration
  { 4.0f, 3.0f, 0.01f, 5.0f },  // PID constants
  false, // manual mode
  100,  // max fan speed
  128, // lcd backlight (50%)
#ifdef HEATERMETER_RFM12
  {{ RFSOURCEID_NONE, 0 }, { RFSOURCEID_NONE, 0 }, { RFSOURCEID_NONE, 0 }, { RFSOURCEID_NONE, 0 }},  // rfMap
#endif
  'F',
};

// EEPROM address of the start of the probe structs, the 2 bytes before are magic
#define EEPROM_PROBE_START  64

static const struct  __eeprom_probe DEFAULT_PROBE_CONFIG PROGMEM = {
  "Probe", // Name
  PROBETYPE_INTERNAL,  // probeType
  0,  // offset
  200, // alarm high
  40,  // alarm low
  false,  // high enabled
  false,  // low enabled
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

static void setLcdBacklight(unsigned char lcdBacklight)
{
  analogWrite(PIN_LCD_BACKLGHT, lcdBacklight);
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

void storeProbeName(unsigned char probeIndex, const char *name)
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
    pid.setFanSpeed(-sp);
    isManualMode = true;
  }

  config_store_byte(manualMode, isManualMode);
}

static void storePidUnits(char units)
{
  if (units)
  {
    pid.setUnits(units);
    if (units == 'C' || units == 'F')
      config_store_byte(pidUnits, units);
  }
}

void storeProbeOffset(unsigned char probeIndex, int offset)
{
  unsigned char ofs = getProbeConfigOffset(probeIndex, offsetof( __eeprom_probe, tempOffset));
  if (ofs != 0)
  {
    pid.Probes[probeIndex]->Offset = offset;
    eeprom_write_byte((uint8_t *)ofs, offset);
  }  
}

static void storeProbeType(unsigned char probeIndex, unsigned char probeType)
{
  unsigned char ofs = getProbeConfigOffset(probeIndex, offsetof( __eeprom_probe, probeType));
  if (ofs != 0)
  {
    pid.Probes[probeIndex]->setProbeType(probeType);
    eeprom_write_byte((uint8_t *)ofs, probeType);
  }
}

#ifdef HEATERMETER_RFM12
static void reportRfMap(void)
{
  print_P(PSTR("$HMRM"));
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
  {
    Serial_csv();
    if (rfMap[i].source != RFSOURCEID_NONE)
    {
      Serial_char(rfMap[i].source + 'A' - 1);
      Serial.print(rfMap[i].pin, DEC);
    }
  }
  Serial_nl();
}

static void checkInitRfManager(void)
{
  if (pid.countOfType(PROBETYPE_RF12) != 0)
    rfmanager.init(HEATERMETER_RFM12);
}

static void storeRfMap(unsigned char probeIndex, unsigned char source, unsigned char sourcePin)
{
  rfMap[probeIndex].source = source;
  rfMap[probeIndex].pin = sourcePin;
      
  rf12_map_item_t *ofs = (rf12_map_item_t *)offsetof(__eeprom_data, rfMap);
  ofs += probeIndex;
  eeprom_write_block(&rfMap[probeIndex], ofs, sizeof(rf12_map_item_t));
    
  reportRfMap();
  checkInitRfManager();
}
#endif /* HEATERMETER_RFM12 */

static void storeProbeTypeOrMap(unsigned char probeIndex, char *vals)
{
  // The last value can either be an integer, which indicates that it is a probetype
  // Or it is an RF map description indicating it is of type PROBETYPE_RF12 and
  // the map should be updated
  char probeType = *vals;
  if (probeType >= '0' && probeType <= '9')
  {
    // Convert char to PROBETYPE_x
    probeType -= '0';
    unsigned char oldProbeType = pid.Probes[probeIndex]->getProbeType();
    if (oldProbeType != probeType)
    {
      storeProbeType(probeIndex, probeType);
#ifdef HEATERMETER_RFM12
      if (oldProbeType == PROBETYPE_RF12)
        storeRfMap(probeIndex, RFSOURCEID_NONE, 0);
#endif /* HEATERMETER_RFM12 */
    }
  }  /* if probeType */
  
#ifdef HEATERMETER_RFM12
  else if (probeType >= 'A' && probeType <= 'Z')
  {
    // If probeType is an RF source identifier, it is an RF map item consisting of
    // a one-character source identifier A-Z and a one-character pin identifier 0-9
    unsigned char source = probeType - 'A' + 1;
    vals++;
    unsigned char sourcePin = *vals - '0';
    if (sourcePin >= 0 && sourcePin < RF_PINS_PER_SOURCE)
    {
      if (pid.Probes[probeIndex]->getProbeType() != PROBETYPE_RF12)
        storeProbeType(probeIndex, PROBETYPE_RF12);
      storeRfMap(probeIndex, source, sourcePin);
    }
  }  /* if RF map */
#endif /* HEATERMETER_RFM12 */
}

void storeMaxFanSpeed(unsigned char maxFanSpeed)
{
  pid.MaxFanSpeed = maxFanSpeed;
  config_store_byte(maxFanSpeed, maxFanSpeed);
}

void storeLcdBacklight(unsigned char lcdBacklight)
{
  setLcdBacklight(lcdBacklight);
  config_store_byte(lcdBacklight, lcdBacklight);
}

void updateDisplay(void)
{
  // Updates to the temperature can come at any time, only update 
  // if we're in a state that displays them
  if (Menus.State < ST_HOME_FOOD1 || Menus.State > ST_HOME_NOPROBES)
    return;
  char buffer[17];

  // Fixed pit area
  lcd.setCursor(0, 0);
  int pitTemp = pid.Probes[TEMP_PIT]->Temperature;
  if (!pid.getManualFanMode() && pitTemp == 0)
    memcpy_P(buffer, LCD_LINE1_UNPLUGGED, sizeof(LCD_LINE1_UNPLUGGED));
  else if (pid.LidOpenResumeCountdown > 0)
    snprintf_P(buffer, sizeof(buffer), PSTR("Pit:%3d"DEGREE"%c Lid%3u"),
      pitTemp, pid.getUnits(), pid.LidOpenResumeCountdown);
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
    snprintf_P(buffer, sizeof(buffer), PSTR("Pit:%3d"DEGREE"%c %c%3u%%%c"),
      pitTemp, pid.getUnits(), c1, pid.getFanSpeed(), c2);
  }
  lcd.print(buffer); 

  // Rotating probe display
  unsigned char probeIndex = Menus.State - ST_HOME_FOOD1 + TEMP_FOOD1;
  if (probeIndex < TEMP_COUNT)
  {
    loadProbeName(probeIndex);
    snprintf_P(buffer, sizeof(buffer), PSTR("%-12s%3d"DEGREE), editString, (int)pid.Probes[probeIndex]->Temperature);
  }
  else
  {
    // If probeIndex is outside the range (in the case of ST_HOME_NOPROBES)
    // just fill the bottom line with spaces
    memset(buffer, ' ', sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
  }

  lcd.setCursor(0, 1);
  lcd.print(buffer);
}

void lcdprint_P(const prog_char *p, const boolean doClear)
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
  print_P(PSTR("$HMSU" CSV_DELIMITER));
  pid.status();
  Serial_nl();
#endif /* HEATERMETER_SERIAL */
}

#if defined(HEATERMETER_NETWORKING) || defined(HEATERMETER_SERIAL)
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
  Serial.print(f, 7);
  Serial_char('e');
  Serial.print(exponent, DEC);
}

static void reportProbeCoeff(unsigned char probeIdx)
{
  print_P(PSTR("$HMPC" CSV_DELIMITER));
  Serial.print(probeIdx, DEC);
  Serial_csv();
  
  TempProbe *p = pid.Probes[probeIdx];
  for (unsigned char i=0; i<STEINHART_COUNT; ++i)
  {
    printSciFloat(p->Steinhart[i]);
    Serial_csv();
  }
  Serial.print(p->getProbeType(), DEC);
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

  storeProbeTypeOrMap(probeIndex, vals);
  reportProbeCoeff(probeIndex);
}

static void reboot(void)
{
  // Once the pin goes low, the avr should reboot
  digitalWrite(PIN_SOFTRESET, LOW);
  while (1) { };
}

static void reportProbeNames(void)
{
  print_P(PSTR("$HMPN"));
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
  {
    loadProbeName(i);
    Serial_csv();
    Serial.print(editString);
  }
  Serial_nl();
}

static void reportPidParams(void)
{
  print_P(PSTR("$HMPD"));
  for (unsigned char i=0; i<4; ++i)
  {
    Serial_csv();
    //printSciFloat(pid.Pid[i]);
    Serial.print(pid.Pid[i], 8);
  }
  Serial_nl();
}

static void reportProbeOffsets(void)
{
  print_P(PSTR("$HMPO"));
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
  {
    Serial_csv();
    Serial.print(pid.Probes[i]->Offset, DEC);
  }
  Serial_nl();
}

static void reportVersion(void)
{
  print_P(PSTR("$UCID" CSV_DELIMITER));
  print_P(PSTR("HeaterMeter" CSV_DELIMITER));
  print_P(HM_VERSION);
  Serial_nl();
}

static void reportLidParameters(void)
{
  print_P(PSTR("$HMLD" CSV_DELIMITER));
  Serial.print(pid.LidOpenOffset, DEC);
  Serial_csv();
  Serial.print(pid.getLidOpenDuration(), DEC);
  Serial_nl();
}

static void reportLcdBacklight(void)
{
  print_P(PSTR("$HMLB" CSV_DELIMITER));
  // The backlight value isn't stored in SRAM so pull it from config
  unsigned char lb = eeprom_read_byte((uint8_t *)offsetof(__eeprom_data, lcdBacklight));
  Serial.print(lb, DEC);
  Serial_nl();
}

static void reportProbeCoeffs(void)
{
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
    reportProbeCoeff(i);
}

static void reportConfig(void)
{
  reportVersion();
  reportPidParams();
  reportProbeNames();
  reportProbeCoeffs();
  reportProbeOffsets();
  reportLidParameters();
  reportLcdBacklight();
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

/* handleCommandUrl returns true if it consumed the URL */
static boolean handleCommandUrl(char *URL)
{
  unsigned char urlLen = strlen(URL);
  if (strncmp_P(URL, PSTR("set?sp="), 7) == 0) 
  {
    storeSetPoint(atoi(URL + 7));
    storePidUnits(URL[urlLen-1]);
    return true;
  }
  if (strncmp_P(URL, PSTR("set?lb="), 7) == 0) 
  {
    storeLcdBacklight(atoi(URL + 7));
    reportLcdBacklight();
    return true;
  }
  if (strncmp_P(URL, PSTR("set?ld="), 7) == 0) 
  {
    csvParseI(URL + 7, storeLidParam);
    reportLidParameters();
    return true;
  }
  if (strncmp_P(URL, PSTR("set?po="), 7) == 0)
  {
    csvParseI(URL + 7, storeProbeOffset);
    reportProbeOffsets();
    return true;
  }
  if (strncmp_P(URL, PSTR("set?pid"), 7) == 0 && urlLen > 9) 
  {
    float f = atof(URL + 9);
    storePidParam(URL[7], f);
    reportPidParams();
    return true;
  }
  if (strncmp_P(URL, PSTR("set?pn"), 6) == 0 && urlLen > 8) 
  {
    // Store probe name will only store it if a valid probe number is passed
    storeProbeName(URL[6] - '0', URL + 8);
    reportProbeNames();
    return true;
  }
  if (strncmp_P(URL, PSTR("set?pc"), 6) == 0 && urlLen > 8) 
  {
    storeProbeCoeff(URL[6] - '0', URL + 8);
    return true;
  }
  if (strncmp_P(URL, PSTR("config"), 6) == 0) 
  {
    reportConfig();
    return true;
  }
  if (strncmp_P(URL, PSTR("reboot"), 5) == 0)
  {
    reboot();
    // reboot doesn't return
  }
  
  return false;
}
#endif /* defined(HEATERMETER_NETWORKING) || defined(HEATERMETER_SERIAL) */

static void outputRfStatus(void)
{
#if defined(HEATERMETER_SERIAL) && defined(HEATERMETER_RFM12)
  print_P(PSTR("$HMRF" CSV_DELIMITER)); 
  rfmanager.status();
  Serial_nl();
#endif /* defined(HEATERMETER_SERIAL) && defined(HEATERMETER_RFM12) */
}

#ifdef HEATERMETER_NETWORKING

#ifdef DFLASH_SERVING 
#define HTTP_HEADER_LENGTH 19 // "HTTP/1.0 200 OK\r\n\r\n"
static void sendFlashFile(const struct flash_file_t *file)
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

  if (sendSize > uip_mss())
    sendSize = uip_mss();
   
  dflash.Cont_Flash_Read_Enable(page, off);
  while (sendSize-- > 0)
    WiServer.write(dflash.Cont_Flash_Read());
  dflash.DF_CS_inactive();
  
  // Pretend that we've sent the whole file
  app->cursor = (char *)(HTTP_HEADER_LENGTH + size);
}
#endif  /* DFLASH_SERVING */

static void outputJson(void)
{
  WiServer.print_P(PSTR("{\"temps\":["));

  for (unsigned char i=0; i<TEMP_COUNT; ++i)
  {
    WiServer.print_P(PSTR("{\"n\":\""));
    loadProbeName(i);
    WiServer.print(editString);
    WiServer.print_P(PSTR("\",\"c\":"));
    if (pid.Probes[i]->hasTemperature())
      WiServer.print(pid.Probes[i]->Temperature, 1);
    else
      WiServer.print_P(PSTR("null"));
    WiServer.print_P(PSTR(",\"a\":"));
    if (pid.Probes[i]->hasTemperatureAvg())
      WiServer.print(pid.Probes[i]->TemperatureAvg, 2);
    else
      WiServer.print_P(PSTR("null"));
    WiServer.print_P(PSTR("},"));
  }
  
  WiServer.print_P(PSTR("{}],\"set\":"));
  WiServer.print(pid.getSetPoint(),DEC);
  WiServer.print_P(PSTR(",\"lid\":"));
  WiServer.print(pid.LidOpenResumeCountdown,DEC);
  WiServer.print_P(PSTR(",\"fan\":{\"c\":"));
  WiServer.print(pid.getFanSpeed(),DEC);
  WiServer.print_P(PSTR(",\"a\":"));
  WiServer.print((unsigned char)pid.FanSpeedAvg,DEC);
  WiServer.print_P(PSTR("}}"));
}

/*
  This hexdecode function may look dumb as shit but
  the logic does this in 4 instructions (8 bytes)
  instead of 12 instructions
  if (c == 0)
    return 0;
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return c - 'A';
  if (c >= 'a' && c <= 'f')
    return c - 'a';
  return (undefined);
*/
static unsigned char hexdecode(unsigned char c)
{
  // Convert 'a'-'f' to lowercase and '0'-'9' to 0-9
  c &= 0xcf;
  if (c > 9)
    return c - 'A' + 10;
  return c;
}

/* In-place URL decoder */
static void urldecode(char *URL)
{
  char *dest = URL;
  while (true)
  {
    *dest = *URL;
    char ofs = 1;

    switch (*URL)
    {
    case 0:
      return;
      break;

    case '+':
      *dest = ' ';
      break;

    case '%':
      char c1 = *(URL+1);
      char c2 = *(URL+2);
      if (c1 && c2)
      {
        *dest = (hexdecode(c1) << 4 | hexdecode(c2));
        ofs = 3;
      }
      break;
    }  /* switch */
    URL += ofs;
    ++dest;
  }
}

static boolean sendPage(char* URL)
{
  ++URL;  // WARNING: URL no longer has leading '/'
  urldecode(URL);
  if (handleCommandUrl(URL))
  {
    WiServer.print_P(PSTR("OK\n"));
    return true;
  }
  if (strcmp_P(URL, PSTR("json")) == 0) 
  {
    outputJson();
    return true;    
  }
  
#ifdef DFLASH_SERVING
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
#endif  /* DFLASH_SERVING */
  
  return false;
}
#endif /* HEATERMETER_NETWORKING */

#ifdef HEATERMETER_RFM12
static void rfSourceNotify(RFSource &r, RFManager::event e)
{
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
    if ((pid.Probes[i]->getProbeType() == PROBETYPE_RF12) && (rfMap[i].source == r.getId()))
    {
      if (e & (RFManager::Update | RFManager::Remove))
      {
        unsigned char srcPin = rfMap[i].pin;
        unsigned int val = r.Values[srcPin];
        unsigned char adcBits = r.getAdcBits();
        // ADC bits of 0 is direct measurement in 10ths of a degree, i.e. 986 = 98.6
        if (adcBits == 0)
          pid.Probes[i]->Temperature = val / 10.0f;
        else
        {
          // If the remote is lower resolution then shift it up to our resolution
          if (adcBits < pid.getAdcBits())
            val <<= (pid.getAdcBits() - adcBits);
          //else if (adcBits > pid.getAdcBits())
          //  val >>= (adcBits - pid.getAdcBits());
          pid.Probes[i]->addAdcValue(val);
        }
        // Set the pin's value to 0 so when we remove the source later it 
        // adds a 0 to the adcValue, effectively clearing it
        r.Values[srcPin] = 0;
      }
    } /* if probe is this source */
  
  if (e & RFManager::Add)
    outputRfStatus();
}
#endif /* HEATERMETER_RFM12 */

static void toneEnable(boolean enable)
{
#ifdef PIEZO_HZ
  if (enable)
  {
    if (tone_idx == 0xff)
      return;
    tone_last = 0;
    tone_idx = tone_cnt - 1;
  }
  else
  {
    tone_idx = 0xff;
    noTone(PIN_ALARM);
  }
#endif /* PIEZO_HZ */
}

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
    }
  }
#endif /* PIEZO_HZ */
}

static void checkAlarms(void)
{
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
    if (pid.Probes[i]->Alarms.getActionNeeded())
    {
      toneEnable(true);
      return;
    }
    
  toneEnable(false);
}

static void eepromLoadBaseConfig(boolean forceDefault)
{
  struct __eeprom_data config;
  eeprom_read_block(&config, 0, sizeof(config));
  forceDefault = forceDefault || config.magic != EEPROM_MAGIC;
  if (forceDefault)
  {
    memcpy_P(&config, &DEFAULT_CONFIG, sizeof(__eeprom_data));
    eeprom_write_block(&config, 0, sizeof(__eeprom_data));  
  }
  
  pid.setSetPoint(config.setPoint);
  pid.LidOpenOffset = config.lidOpenOffset;
  pid.setLidOpenDuration(config.lidOpenDuration);
  memcpy(pid.Pid, config.pidConstants, sizeof(config.pidConstants));
  if (config.manualMode)
    pid.setFanSpeed(0);
  pid.MaxFanSpeed = config.maxFanSpeed;
  setLcdBacklight(config.lcdBacklight);
  pid.setUnits(config.pidUnits == 'C' ? 'C' : 'F');
  
#ifdef HEATERMETER_RFM12
  memcpy(rfMap, config.rfMap, sizeof(rfMap));
#endif
}

static void eepromLoadProbeConfig(boolean forceDefault)
{
  unsigned int magic;
  // instead of this use below because we don't have eeprom_read_word linked yet
  // magic = eeprom_read_word((uint16_t *)(EEPROM_PROBE_START-sizeof(magic))); 
  eeprom_read_block(&magic, (void *)(EEPROM_PROBE_START-sizeof(magic)), sizeof(magic));
  if (magic != EEPROM_MAGIC)
  {
    forceDefault = true;
    eeprom_write_word((uint16_t *)(EEPROM_PROBE_START-sizeof(magic)), EEPROM_MAGIC);
  }
    
  struct  __eeprom_probe config;
  struct  __eeprom_probe *p;
  p = (struct  __eeprom_probe *)(EEPROM_PROBE_START);
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
  {
    if (forceDefault)
    {
      memcpy_P(&config, &DEFAULT_PROBE_CONFIG, sizeof( __eeprom_probe));
      eeprom_write_block(&config, p, sizeof(__eeprom_probe));
    }
    else
      eeprom_read_block(&config, p, sizeof(__eeprom_probe));

    pid.Probes[i]->loadConfig(&config);
    ++p;
  }  /* for i<TEMP_COUNT */
}

void eepromLoadConfig(boolean forceDefault)
{
  // These are separated into two functions to prevent needing stack
  // space for both a __eeprom_data and __eeprom_probe structure
  eepromLoadBaseConfig(forceDefault);
  eepromLoadProbeConfig(forceDefault);
}

static void blinkLed(void)
{
#ifndef HEATERMETER_NETWORKING  
  pinMode(PIN_WIRELESS_LED, OUTPUT);
  digitalWrite(PIN_WIRELESS_LED, HIGH);
  delay(100);
  digitalWrite(PIN_WIRELESS_LED, LOW);
  delay(50);
#endif // !HEATERMETER_NETWORKING
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

static void newTempsAvail(void)
{
  static unsigned char pidCycleCount;

  checkAlarms();
  updateDisplay();
  ++pidCycleCount;
    
  if ((pidCycleCount % 0x10) == 0)
    outputRfStatus();
#ifdef PID_DEBUG
  if ((pidCycleCount % 2) == 0)
    pid.pidStatus();
#endif

  outputCsv();
}

static void dflashInit(void)
{
#ifdef DFLASH_SERVING
  // Set the WiFi Slave Select to HIGH (disable) to
  // prevent it from interferring with the dflash init
  pinMode(PIN_SPI_SS, OUTPUT);
  digitalWrite(PIN_SPI_SS, HIGH);
  dflash.init(PIN_SOFTRESET);  // actually DATAFLASH_SS
#endif  /* DFLASH_SERVING */
}

void hmcoreSetup(void)
{
  // BLINK 1: Booted
  blinkLed();
  
#ifdef HEATERMETER_SERIAL
  Serial.begin(HEATERMETER_SERIAL);
  Serial_nl();
  reportVersion();
#endif  /* HEATERMETER_SERIAL */
#ifdef USE_EXTERNAL_VREF  
  analogReference(EXTERNAL);
#endif  /* USE_EXTERNAL_VREF */

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

  eepromLoadConfig(false);
#ifdef HEATERMETER_RFM12
  checkInitRfManager();
#endif

#ifdef HEATERMETER_NETWORKING
  dflashInit();
  
  g_NetworkInitialized = readButton() == BUTTON_NONE;
  if (g_NetworkInitialized)  
  {
    Menus.setState(ST_CONNECTING);
    WiServer.init(sendPage);
  }
#endif  /* HEATERMETER_NETWORKING */
  Menus.setState(ST_HOME_NOPROBES);

  // BLINK 2: Initialization complete
  blinkLed();
}

void hmcoreLoop(void)
{ 
#ifdef HEATERMETER_SERIAL 
  serial_doWork();
#endif /* HEATERMETER_SERIAL */

#ifdef HEATERMETER_RFM12
  if (rfmanager.doWork()) 
  {
    digitalWrite(PIN_WIRELESS_LED, HIGH);
    delay(10);
  }
  else
    digitalWrite(PIN_WIRELESS_LED, LOW);
#endif /* HEATERMETER_RFM12 */

#ifdef HEATERMETER_NETWORKING 
  if (g_NetworkInitialized)
    WiServer.server_task(); 
#endif /* HEATERMETER_NETWORKING */

  Menus.doWork();
  if (pid.doWork())
    newTempsAvail();
  tone_doWork();
}
