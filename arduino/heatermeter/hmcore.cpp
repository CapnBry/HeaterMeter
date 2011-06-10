#include <WProgram.h>
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

ShiftRegLCD lcd(PIN_LCD_DATA, PIN_LCD_CLK, TWO_WIRE, 2); 

#ifdef HEATERMETER_NETWORKING
static boolean g_NetworkInitialized;
#endif /* HEATERMETER_NETWORKING */
#ifdef HEATERMETER_SERIAL
static char g_SerialBuff[64]; 
#endif /* HEATERMETER_SERIAL */
#ifdef HEATERMETER_RFM12
static RFManager rfmanager(PIN_WIRELESS_LED);
static rf12_map_item_t rfMap[TEMP_COUNT];
#endif /* HEATERMETER_RFM12 */

#define config_store_byte(eeprom_field, src) { eeprom_write_byte((uint8_t *)offsetof(__eeprom_data, eeprom_field), src); }
#define config_store_word(eeprom_field, src) { eeprom_write_word((uint16_t *)offsetof(__eeprom_data, eeprom_field), src); }

#define EEPROM_MAGIC 0xf00d

const struct __eeprom_data {
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
} DEFAULT_CONFIG PROGMEM = { 
  EEPROM_MAGIC,  // magic
  225,  // setpoint
  6,  // lid open offset %
  240, // lid open duration
  { 5.0f, 4.0f, 0.004f, 2.0f },  // PID constants
  false, // manual mode
  100,  // max fan speed
  128, // lcd backlight (50%)
#ifdef HEATERMETER_RFM12
  {{ 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }},  // rfMap
#endif
};

// EEPROM address of the start of the probe structs, the 2 bytes before are magic
#define EEPROM_PROBE_START  64

const struct  __eeprom_probe DEFAULT_PROBE_CONFIG PROGMEM = {
  "Probe", // Name
  PROBETYPE_INTERNAL,  // probeType
  0,  // offset
  200, // alarm high
  40,  // alarm low
  false,  // high enabled
  false,  // low enabled
  {2.3067434e-4,2.3696596e-4,1.2636414e-7,1.0e+4},  // Maverick Probe
  //{8.98053228e-4,2.49263324e-4,2.04047542e-7,1.0e+4}, // Radio Shack 10k
  //{1.1415e-3,2.31905e-4,9.76423e-8,1.0e+4} // Vishay 10k NTCLE100E3103JB0
};

inline void setLcdBacklight(unsigned char lcdBacklight)
{
  analogWrite(PIN_LCD_BACKLGHT, lcdBacklight);
}

// Note the storage loaders and savers expect the entire config storage is less than 256 bytes
unsigned char getProbeConfigOffset(unsigned char probeIndex, unsigned char off)
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

void storeProbeOffset(unsigned char probeIndex, char offset)
{
  unsigned char ofs = getProbeConfigOffset(probeIndex, offsetof( __eeprom_probe, tempOffset));
  if (ofs != 0)
  {
    pid.Probes[probeIndex]->Offset = offset;
    eeprom_write_byte((uint8_t *)ofs, offset);
  }  
}

void storeProbeType(unsigned char probeIndex, unsigned char probeType)
{
  unsigned char ofs = getProbeConfigOffset(probeIndex, offsetof( __eeprom_probe, probeType));
  if (ofs != 0)
  {
    pid.Probes[probeIndex]->setProbeType(probeType);
    eeprom_write_byte((uint8_t *)ofs, probeType);
  }
}

void storeProbeCoeff(unsigned char probeIndex, char *vals)
{
  // vals is SteinA(float),SteinB(float),SteinC(float),RKnown(float),probeType+1(int)
  // If any value is 0, it won't be modified
  unsigned char ofs = getProbeConfigOffset(probeIndex, offsetof( __eeprom_probe, steinhart));
  if (ofs == 0)
    return;
    
  float fVal;
  float *fDest = pid.Probes[probeIndex]->Steinhart;
  for (unsigned char i=0; i<STEINHART_COUNT; ++i)
  {
    fVal = atof(vals);
    while (*vals)
    {
      if (*vals == ',') 
      {
        ++vals;
        break;
      }
      ++vals;
    }  /* while vals */

    if (fVal != 0.0f)
    {
      *fDest = fVal;
      eeprom_write_block(&fVal, (void *)ofs, sizeof(fVal));
    }

    ofs += sizeof(float);
    ++fDest;
  }  /* for i */

  // The probe type is an integer but is passed as actual probeType + 1
  // because passing 0 (PROBETYPE_DISABLED) is reserved for "don't change"
  unsigned char probeType = atoi(vals);
  if (probeType != 0)
  {
    --probeType;
    storeProbeType(probeIndex, probeType);
  }
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

#ifdef HEATERMETER_RFM12
void storeRfMap(char *vals)
{
  // vals should be 3 characters each, back to back
  // <probeIdx><rfSource (letter)><sourcePin>
  // The entire map is replace with this call
  // e.g. 1B02C03C1 sets
  // TEMP_FOOD1 = Source B pin 0
  // TEMP_FOOD2 = Source C pin 0
  // TEMP_AMB = Source C pin 1
  boolean modified = false;
  
  while (strlen(vals) > 2)
  {
    unsigned char probeIdx = (*vals++) - '0';
    unsigned char source = (*vals++) - 'A' + 1;
    unsigned char sourcePin = (*vals++) - '0';
    //Serial.print("RM "); Serial.print(probeIdx, DEC); Serial.print(source, DEC); Serial.print(sourcePin, DEC); Serial.print('\n');
    
    if ((probeIdx >= 0) && (probeIdx < TEMP_COUNT) &&
      (source >= 1) && (source <= 26) &&
      (sourcePin >= 0) && (sourcePin < RF_PINS_PER_SOURCE))
    {
      if (!modified)
      {
        memset(rfMap, 0, sizeof(rfMap));
        modified = true;
      }
      
      if (pid.Probes[probeIdx]->getProbeType() != PROBETYPE_RF12)
        storeProbeType(probeIdx, PROBETYPE_RF12);
      
      rfMap[probeIdx].source = source;
      rfMap[probeIdx].pin = sourcePin;
    }  /* if data valid */
  }  /* while chars left */
  if (modified)
    eeprom_write_block(rfMap, (void *)offsetof(__eeprom_data, rfMap), sizeof(rfMap));
}

void reportRfMap(void)
{
  print_P(PSTR("$HMRM"));
  for (unsigned int i=0; i<TEMP_COUNT; ++i)
  {
    Serial_csv();
    if (rfMap[i].source != 0)
    {
      Serial_char(rfMap[i].source + 'A' - 1);
      Serial.print(rfMap[i].pin, DEC);
    }
  }
  Serial_nl();
}
#endif /* HEATERMETER_RFM12 */

void updateDisplay(void)
{
  // Updates to the temperature can come at any time, only update 
  // if we're in a state that displays them
  if (Menus.State < ST_HOME_FOOD1 || Menus.State > ST_HOME_NOPROBES)
    return;
  char buffer[17];

  // Fixed pit area
  lcd.home();
  int pitTemp = pid.Probes[TEMP_PIT]->Temperature;
  if (!pid.getManualFanMode() && pitTemp == 0)
    memcpy_P(buffer, LCD_LINE1_UNPLUGGED, sizeof(LCD_LINE1_UNPLUGGED));
  else if (pid.LidOpenResumeCountdown > 0)
    snprintf_P(buffer, sizeof(buffer), PSTR("Pit:%3d"DEGREE"F Lid%3u"), pitTemp, pid.LidOpenResumeCountdown);
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
    snprintf_P(buffer, sizeof(buffer), PSTR("Pit:%3d"DEGREE"F %c%3u%%%c"), pitTemp, c1, pid.getFanSpeed(), c2);
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
  char buffer[17];
  strncpy_P(buffer, p, sizeof(buffer));

  if (doClear)
    lcd.clear();
  lcd.print(buffer);
}

void storePidParam(char which, float value)
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
  pid.Pid[k] = value;

  unsigned char ofs = offsetof(__eeprom_data, pidConstants[0]);
  eeprom_write_block(&pid.Pid[k], (void *)(ofs + k * sizeof(float)), sizeof(value));
}

void storeLidOpenOffset(unsigned char value)
{
  pid.LidOpenOffset = value;    
  config_store_byte(lidOpenOffset, value);
}

void storeLidOpenDuration(unsigned int value)
{
  pid.LidOpenDuration = value;    
  config_store_word(lidOpenDuration, value);
}

inline void outputCsv(void)
{
#ifdef HEATERMETER_SERIAL
  print_P(PSTR("$HMSU"));
  Serial_csv();
  pid.status();
  Serial_nl();
#endif /* HEATERMETER_SERIAL */
}

#if defined(HEATERMETER_NETWORKING) || defined(HEATERMETER_SERIAL)
inline void reboot(void)
{
  // Once the pin goes low, the avr should reboot
  digitalWrite(PIN_SOFTRESET, LOW);
  while (1) { };
}

inline void reportProbeNames(void)
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

/* handleCommandUrl returns true if it consumed the URL */
boolean handleCommandUrl(char *URL)
{
  unsigned char urlLen = strlen(URL);
  if (strncmp_P(URL, PSTR("set?sp="), 7) == 0) 
  {
    storeSetPoint(atoi(URL + 7));
    return true;
  }
  if (strncmp_P(URL, PSTR("set?lb="), 7) == 0) 
  {
    storeLcdBacklight(atoi(URL + 7));
    return true;
  }
  if (strncmp_P(URL, PSTR("set?pid"), 7) == 0 && urlLen > 9) 
  {
    float f = atof(URL + 9);
    storePidParam(URL[7], f);
    return true;
  }
  if (strncmp_P(URL, PSTR("set?pn"), 6) == 0 && urlLen > 8) 
  {
    // Store probe name will only store it if a valid probe number is passed
    storeProbeName(URL[6] - '0', URL + 8);
    reportProbeNames();
    return true;
  }
  if (strncmp_P(URL, PSTR("set?po"), 6) == 0 && urlLen > 8) 
  {
    storeProbeOffset(URL[6] - '0', atoi(URL + 8));
    return true;
  }
  if (strncmp_P(URL, PSTR("set?pc"), 6) == 0 && urlLen > 8) 
  {
    storeProbeCoeff(URL[6] - '0', URL + 8);
    return true;
  }
#ifdef HEATERMETER_RFM12
  if (strncmp_P(URL, PSTR("set?rm"), 6) == 0 and urlLen > 6) 
  {
    storeRfMap(URL + 7);
    reportRfMap();
    return true;
  }
#endif /* HEATERMETER_RFM12 */
  if (strncmp_P(URL, PSTR("reboot"), 5) == 0)
  {
    reboot();
    // reboot doesn't return
  }
  
  return false;
}
#endif /* defined(HEATERMETER_NETWORKING) || defined(HEATERMETER_SERIAL) */

#ifdef HEATERMETER_NETWORKING

#ifdef DFLASH_SERVING 
#define HTTP_HEADER_LENGTH 19 // "HTTP/1.0 200 OK\r\n\r\n"
inline void sendFlashFile(const struct flash_file_t *file)
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
#endif  /* DFLASH_SERVING */

void outputJson(void)
{
  WiServer.print_P(PSTR("{\"temps\":["));

  for (unsigned char i=0; i<TEMP_COUNT; ++i)
  {
    WiServer.print_P(PSTR("{\"n\":\""));
    loadProbeName(i);
    WiServer.print(editString);
    WiServer.print_P(PSTR("\",\"c\":"));
    WiServer.print(pid.Probes[i]->Temperature, 1);
    WiServer.print_P(PSTR(",\"a\":"));
    WiServer.print(pid.Probes[i]->TemperatureAvg, 2);
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

boolean sendPage(char* URL)
{
  ++URL;  // WARNING: URL no longer has leading '/'
  unsigned char urlLen = strlen(URL);

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
void rfDataToProbes(void)
{
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
    if ((pid.Probes[i]->getProbeType() == PROBETYPE_RF12) && (rfMap[i].source != 0))
    {
      RFSource *src = rfmanager.getSourceById(rfMap[i].source);
      if (src != NULL)
      {
        unsigned char srcPin = rfMap[i].pin;
        if (src->Values[srcPin] != 0)
        {
          pid.Probes[i]->addAdcValue(src->Values[srcPin]);
          src->Values[srcPin] = 0;
        }
      }  /* if source present */
    }  /* if map has source */
}
#endif /* HEATERMETER_RFM12 */

inline void checkAlarms(void)
{
  for (unsigned char i=0; i<TEMP_COUNT; ++i)
    if (pid.Probes[i]->Alarms.getActionNeeded())
    {
      tone(PIN_ALARM, 440);  // 440Hz = A4
      return;
    }
    
  noTone(PIN_ALARM);
}

void eepromLoadBaseConfig(boolean forceDefault)
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
  pid.LidOpenDuration = config.lidOpenDuration;
  memcpy(pid.Pid, config.pidConstants, sizeof(config.pidConstants));
  if (config.manualMode)
    pid.setFanSpeed(0);
  pid.MaxFanSpeed = config.maxFanSpeed;
  setLcdBacklight(config.lcdBacklight);
  
#ifdef HEATERMETER_RFM12
  memcpy(rfMap, config.rfMap, sizeof(rfMap));
#endif
}

void eepromLoadProbeConfig(boolean forceDefault)
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

#ifdef HEATERMETER_SERIAL
inline void serial_doWork(void)
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

inline void outputRfStatus(void)
{
#ifdef HEATERMETER_RFM12
  print_P(PSTR("$HMRF")); 
  Serial_csv();
  rfmanager.status();
  Serial_nl();
#endif /* HEATERMETER_RFM12 */
}

inline void newTempsAvail(void)
{
  static unsigned char pidCycleCount;

  checkAlarms();
  updateDisplay();
  ++pidCycleCount;
    
  if ((pidCycleCount % 0x10) == 0)
    outputRfStatus();

  outputCsv();
}

inline void dflashInit(void)
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
#ifdef HEATERMETER_SERIAL
  Serial.begin(HEATERMETER_SERIAL);
#endif  /* HEATERMETER_SERIAL */
#ifdef USE_EXTERNAL_VREF  
  analogReference(EXTERNAL);
#endif  /* USE_EXTERNAL_VREF */

  // Switch the pin mode first to INPUT with internal pullup
  // to take it to 5V before setting the mode to OUTPUT. 
  // If we reverse this, the pin will go OUTPUT,LOW and reboot.
  digitalWrite(PIN_SOFTRESET, HIGH);
  pinMode(PIN_SOFTRESET, OUTPUT);
  
  pid.Probes[TEMP_PIT] = &probe0;
  pid.Probes[TEMP_FOOD1] = &probe1;
  pid.Probes[TEMP_FOOD2] = &probe2;
  pid.Probes[TEMP_AMB] = &probe3;

  eepromLoadConfig(false);

#ifdef HEATERMETER_RFM12
  rfmanager.init(HEATERMETER_RFM12);
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
}

void hmcoreLoop(void)
{ 
#ifdef HEATERMETER_SERIAL 
  serial_doWork();
#endif /* HEATERMETER_SERIAL */

#ifdef HEATERMETER_RFM12
  if (rfmanager.doWork())
    rfDataToProbes();
#endif /* HEATERMETER_RFM12 */

#ifdef HEATERMETER_NETWORKING 
  if (g_NetworkInitialized)
    WiServer.server_task(); 
#endif /* HEATERMETER_NETWORKING */

  Menus.doWork();
  if (pid.doWork())
    newTempsAvail();    
}
