#include <WProgram.h>
#include <avr/eeprom.h>

#include "hmcore.h"

#ifdef HEATERMETER_NETWORKING
#include <WiServer.h>  
#endif

#if defined(DFLASH_LOGGING) || defined(DFLASH_SERVING)
#include <dataflash.h>
  #ifdef DFLASH_SERVING
  #include "flashfiles.h"
  #endif
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
static char g_SerialBuff[80]; 
#endif /* HEATERMETER_SERIAL */

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
  // After this is stored all the probe_config recs
} DEFAULT_CONFIG PROGMEM = { 
  EEPROM_MAGIC,  // magic
  225,  // setpoint
  6,  // lid open offset %
  240, // lid open duration
  { 5.0f, 4.0f, 0.004f, 2.5f },  // PID constants
  false, // manual mode
  100,  // max fan speed
};

const struct  __eeprom_probe DEFAULT_PROBE_CONFIG PROGMEM = {
  "Probe", // Name
  0,  // offset
  200, // alarm high
  40,  // alarm low
  false,  // high enabled
  false,  // low enabled
  PROBETYPE_INTERNAL,  // probeType
  {2.3067434e-4,2.3696596e-4,1.2636414e-7,1.0e+4},  // Maverick Probe
  //{8.98053228e-4,2.49263324e-4f,2.04047542e-7,1.0e+4}, // Radio Shack 10k
  //{1.1415e-3,2.31905e-4,9.76423e-8,1.0e+4} // Vishay 10k NTCLE100E3103JB0
};

// Note the storage loaders and savers expect the entire config storage is less than 256 bytes
unsigned char getProbeConfigOffset(unsigned char probeIndex, unsigned char off)
{
  if (probeIndex >= TEMP_COUNT)
    return 0;
  // Point to the name in the first probe_config structure
  unsigned char retVal = sizeof(__eeprom_data) + off;
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

void storeProbeCoeff(unsigned char probeIndex, char *vals)
{
  // vals is SteinA,SteinB,SteinC,RKnown all float
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
      ++vals;
      if (*vals == ',') 
        break;
    }  /* while vals */

    if (fVal != 0.0f)
    {
      *fDest = fVal;
      eeprom_write_block(&fVal, (void *)ofs, sizeof(fVal));
    }

    ofs += sizeof(float);
    ++fDest;
  }  /* for i */
  
  // Might consider resetting the pid accumulator here, but
  // The previous reading was wrong so who cares if one more is
}

void storeMaxFanSpeed(unsigned char maxFanSpeed)
{
  pid.MaxFanSpeed = maxFanSpeed;
  config_store_byte(maxFanSpeed, maxFanSpeed);
}

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

#if defined(HEATERMETER_NETWORKING) || defined(HEATERMETER_SERIAL)
void outputCsv(Print &out)
{
  out.print(pid.getSetPoint());
  out.print(CSV_DELIMITER);

  unsigned char i;
  for (i=0; i<TEMP_COUNT; i++)
  {
    out.print(pid.Probes[i]->Temperature, 1);
    out.print(CSV_DELIMITER);
    out.print(pid.Probes[i]->TemperatureAvg, 1);
    out.print(CSV_DELIMITER);
  }

  out.print(pid.getFanSpeed(), DEC);
  out.print(CSV_DELIMITER);
  out.print((int)pid.FanSpeedAvg, DEC);
  out.print(CSV_DELIMITER);
  out.print(pid.LidOpenResumeCountdown, DEC);
  out.println();
}

void reboot()
{
  // Delay is here to help it sync up with avrdude on the reboot of linkmeter
  delay(250);
  // Once the pin goes low, the avr should reboot
  digitalWrite(PIN_WIFI_LED, LOW);
  while (1) { };
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
  if (strncmp_P(URL, PSTR("set?pid"), 7) == 0 && urlLen > 9) 
  {
    float f = atof(URL + 9);
    storePidParam(URL[7], f);
    return true;
  }
  if (strncmp_P(URL, PSTR("set?pn"), 6) == 0 && urlLen > 8) 
  {
    storeProbeName(URL[6] - '0', URL + 8);
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
  if (strncmp_P(URL, PSTR("reboot"), 5) == 0)
  {
    reboot();
    // reboot doesn't return
  }
  
  return false;
}
#endif /* defined(HEATERMETER_NETWORKING) || defined(HEATERMETER_SERIAL) */

#ifdef HEATERMETER_NETWORKING

#ifdef DFLASH_LOGGING
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
      WiServer.print(CSV_DELIMITER);
      offset = p.temps[i] >> 9;
      WiServer.print(temp + offset,DEC);  // average
      WiServer.print(CSV_DELIMITER);
    }
    
    WiServer.print(p.fan,DEC);
    WiServer.print(CSV_DELIMITER);
    WiServer.println(p.fan_avg,DEC);
  }  
  dflash.DF_CS_inactive();
}
#endif  /* DFLASH_LOGGING */

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

  unsigned char i;
  for (i=0; i<TEMP_COUNT; i++)
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
  if (strcmp_P(URL, PSTR("csv")) == 0) 
  {
    outputCsv(WiServer);
    return true;    
  }
#ifdef DFLASH_LOGGING  
  if (strcmp_P(URL, PSTR("log")) == 0) 
  {
    outputLog();
    return true;    
  }
#endif  /* DFLASH_LOGGING */
  
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

inline void checkAlarms(void)
{
  unsigned char i;
  for (i=0; i<TEMP_COUNT; i++)
    if (pid.Probes[i]->Alarms.getActionNeeded())
    {
      tone(PIN_ALARM, 440);  // 440Hz = A4
      return;
    }
    
  noTone(PIN_ALARM);
}

boolean eepromLoadBaseConfig(boolean forceDefault)
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
  pid.MaxFanSpeed = config.maxFanSpeed;
  memcpy(pid.Pid, config.pidConstants, sizeof(config.pidConstants));
  if (config.manualMode)
    pid.setFanSpeed(0);
  
  return forceDefault;
}

void eepromLoadProbeConfig(boolean forceDefault)
{
  struct  __eeprom_probe config;
  struct  __eeprom_probe *p;
  p = (struct  __eeprom_probe *)(sizeof(__eeprom_data));
    
  for (unsigned char i=0; i<TEMP_COUNT; i++)
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
  forceDefault = eepromLoadBaseConfig(forceDefault);
  eepromLoadProbeConfig(forceDefault);
}

#ifdef HEATERMETER_SERIAL
inline void checkSerial(void)
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

inline void dflashInit(void)
{
#if defined(DFLASH_LOGGING) || defined(DFLASH_SERVING)
  // Set the WiFi Slave Select to HIGH (disable) to
  // prevent it from interferring with the dflash init
  pinMode(PIN_WIFI_SS, OUTPUT);
  digitalWrite(PIN_WIFI_SS, HIGH);
  dflash.init(PIN_DATAFLASH_SS);
#ifdef DFLASH_LOGGING
  flashRingBufferInit();
#endif  /* DFLASH_LOGGING */
#endif  /* DFLASH_LOGGING || SERVING */
}

void hmcoreSetup(void)
{
#ifdef HEATERMETER_SERIAL
  Serial.begin(19200);
#endif  /* HEATERMETER_SERIAL */
#ifdef USE_EXTERNAL_VREF  
  analogReference(EXTERNAL);
#endif  /* USE_EXTERNAL_VREF */

  // Switch the pin mode first to INPUT with internal pullup
  // to take it to 5V before setting the mode to OUTPUT. 
  // If we reverse this, the pin will go OUTPUT,LOW and reboot.
  digitalWrite(PIN_WIFI_LED, HIGH);
  pinMode(PIN_WIFI_LED, OUTPUT);
  
  pid.Probes[TEMP_PIT] = &probe0;
  pid.Probes[TEMP_FOOD1] = &probe1;
  pid.Probes[TEMP_FOOD2] = &probe2;
  pid.Probes[TEMP_AMB] = &probe3;

  eepromLoadConfig(false);

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
  Menus.doWork();
  if (pid.doWork())
  {
    checkAlarms();
    updateDisplay();
#ifdef HEATERMETER_SERIAL
    checkSerial();
    outputCsv(Serial);
#endif  /* HEATERMETER_SERIAL */
    
#ifdef HEATERMETER_NETWORKING
#ifdef DFLASH_LOGGING
    storeTemps();
#endif  /* DFLASH_LOGGING */
  }
  if (g_NetworkInitialized)
    WiServer.server_task(); 
#else
  }
#endif /* HEATERMETER_NETWORKING */
}
