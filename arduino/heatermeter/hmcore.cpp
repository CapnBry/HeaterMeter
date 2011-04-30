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

static TempProbe probe0(PIN_PIT,   &STEINHART[0]);
static TempProbe probe1(PIN_FOOD1, &STEINHART[0]);
static TempProbe probe2(PIN_FOOD2, &STEINHART[0]);
static TempProbe probe3(PIN_AMB,   &STEINHART[1]);
GrillPid pid(PIN_BLOWER);

ShiftRegLCD lcd(PIN_LCD_DATA, PIN_LCD_CLK, TWO_WIRE, 2); 

#ifdef HEATERMETER_NETWORKING
static boolean g_NetworkInitialized;
#endif /* HEATERMETER_NETWORKING */
#ifdef HEATERMETER_SERIAL
static char g_SerialBuff[40];  // should be 49 to support /set?pn0=(urlencoded 13 chars)
#endif /* HEATERMETER_SERIAL */

#define config_store_byte(eeprom_field, src) { eeprom_write_byte((uint8_t *)offsetof(__eeprom_data, eeprom_field), src); }
#define config_store_word(eeprom_field, src) { eeprom_write_word((uint16_t *)offsetof(__eeprom_data, eeprom_field), src); }

#define EEPROM_MAGIC 0xf00d8000
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
  struct {
    boolean henabled;
    boolean lenabled;
    int high;
    int low;
  } alarms[TEMP_COUNT];
} DEFAULT_CONFIG PROGMEM = { 
  EEPROM_MAGIC,  // magic
  225,  // setpoint
  { "Pit", "Food Probe1", "Food Probe2", "Ambient" },  // probe names
  { 0, 0, 0, 0 },  // probe offsets
  15,  // lid open offset
  240, // lid open duration
  { 5.0f, 4.0f, 0.004f, 2.5f },
  false, // manual mode
  100,  // max fan speed
  { { false, false, 200, 100 }, { false, false, 200, 100 }, 
    { false, false, 200, 100 }, { false, false, 200, 100 } }
};

boolean storeProbeName(unsigned char probeIndex, const char *name)
{
  if (probeIndex >= TEMP_COUNT)
    return false;
    
  size_t ofs = offsetof(__eeprom_data, probeNames);
  ofs += probeIndex * (PROBE_NAME_SIZE * sizeof(char));
  eeprom_write_block(name, (void *)ofs, PROBE_NAME_SIZE);
  return true;
}

void loadProbeName(unsigned char probeIndex)
{
  size_t ofs = offsetof(__eeprom_data, probeNames);
  ofs += probeIndex * (PROBE_NAME_SIZE * sizeof(char));
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

boolean storeProbeOffset(unsigned char probeIndex, char offset)
{
  if (probeIndex >= TEMP_COUNT)
    return false;
    
  pid.Probes[probeIndex]->Offset = offset;
  uint8_t *ofs = (uint8_t *)&((__eeprom_data*)0)->probeTempOffsets[probeIndex];
  eeprom_write_byte(ofs, offset);
  
  return true;
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

boolean storePidParam(char which, float value)
{
  const prog_char *pos = strchr_P(PID_ORDER, which);
  if (pos == NULL)
    return false;
    
  const unsigned char k = pos - PID_ORDER;
  pid.Pid[k] = value;
  
  void *ofs = (void *)&((__eeprom_data*)0)->pidConstants[k];
  eeprom_write_block(&pid.Pid[k], ofs, sizeof(value));

  return true;
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
  delay(100);
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
  {
    pid.Probes[i]->Offset = config.probeTempOffsets[i];
    pid.Probes[i]->Alarms.setHigh(config.alarms[i].high);
    pid.Probes[i]->Alarms.setLow(config.alarms[i].low);
    pid.Probes[i]->Alarms.Status =
      config.alarms[i].henabled & ProbeAlarm::HIGH_ENABLED |
      config.alarms[i].lenabled & ProbeAlarm::LOW_ENABLED;
  }
    
  pid.setSetPoint(config.setPoint);
  pid.LidOpenOffset = config.lidOpenOffset;
  pid.LidOpenDuration = config.lidOpenDuration;
  pid.MaxFanSpeed = config.maxFanSpeed;
  memcpy(pid.Pid, config.pidConstants, sizeof(config.pidConstants));
  if (config.manualMode)
    pid.setFanSpeed(0);
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
  Serial.begin(9600);
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
