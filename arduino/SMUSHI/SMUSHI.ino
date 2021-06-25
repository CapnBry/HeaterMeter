#include <HeaterMeterClient.h>
#include <TFT_eSPI.h>
#include <esp_adc_cal.h>
#include "background.h"
#include "Orbitron_Medium_12.h"

#define WIFI_SSID       "network" // wifi credentials only used if right button (S2) is held on power up!
#define WIFI_PASSWORD   "password"
#define HEATERMEATER_IP "" // IP or leave blank to use discovery
#define TZ              "EST+5EDT,M3.2.0/2,M11.1.0/2" // time zone string

#define DPIN_BTN_ROTATE     0
#define DPIN_ADC_EN         14
#define APIN_BATTERY        34
#define DPIN_BTN_WIFI       35

static HeaterMeterClient hm(HEATERMEATER_IP);
static TFT_eSPI tft;
static TFT_eSprite spr(&tft); // used for Food Probes

#define RGB565(r,g,b) ((r&0xf8)<<8 | (g&0xfc)<<3 | (b&0xf8)>>3)
#define WEB565(hex) (RGB565(hex>>16, (hex>>8) & 0xff, hex & 0xff))

#define COLOR_HIGHLIGHT   WEB565(0xbdf3ff)
#define COLOR_BACKGROUND  WEB565(0x07182a)
#define COLOR_PROBE_BACK  0 // WEB565(0x02070d)
#define COLOR_DRAW_DARK   WEB565(0x264152)
#define COLOR_DRAW_MID    WEB565(0x387385)
#define COLOR_DRAW_LIGHT  WEB565(0x79a4b4)
#define COLOR_RED         WEB565(0xff3a72)
#define COLOR_GREEN       WEB565(0x11fe92)

#define TEMP_DELTA_COUNT 12
static struct tagSmushiState {
  uint8_t rotation;
  bool hmUpdated;
  float tempTotal;
  float tempHist[TEMP_DELTA_COUNT];
  
  struct tagLastStatusState {
    // Output
    uint8_t Current;
    uint8_t Fan;
    uint8_t Servo;
    // Set
    bool Enabled;
    bool HasPit;
    float Pit;
    uint16_t Setpoint;
  } lastStatus;

  struct tagBatteryState {
    uint32_t lastRead;
    uint16_t vref;
    uint32_t mv;
  } battery;

  uint8_t btnHoldCount;
} g_State;

static void proxy_onHmStatus()
{
  g_State.hmUpdated = true;

  // Temperature History
  g_State.tempTotal -= g_State.tempHist[0];
  for (uint8_t i = 0; i < TEMP_DELTA_COUNT - 1; ++i)
    g_State.tempHist[i] = g_State.tempHist[i + 1];
  float newVal = hm.state.Probes[0].HasTemperature ? hm.state.Probes[0].Temperature : 0.0f;
  g_State.tempHist[TEMP_DELTA_COUNT - 1] = newVal;
  g_State.tempTotal += newVal;
}

static void proxy_onWifiConnect()
{
  //Serial.println(F("onWifiConnect"));
  Serial.print(F("Connected to "));
  Serial.println(WiFi.SSID());
}

static void proxy_onWifiDisconnect()
{
  Serial.println(F("onWifiDisconnect"));
}

static void proxy_onConnect()
{
  Serial.println(F("onConnect"));
}

static void proxy_onDisconnect()
{
  Serial.println(F("onDisconnect"));
}

static void proxy_onError(err_t err)
{
  Serial.println(F("onError"));
}

static void drawBattery()
{
  // No battery display when connected to USB power
  if (g_State.battery.mv > 4500)
  {
    tft.fillRect(93, 5, 16, 6, COLOR_BACKGROUND);
    return;
  }

  const uint8_t ICON_BATTERY[] = { 0xff, 0xfe,  0x80, 0x02,  0x80, 0x03,  0x80, 0x03,  0x80, 0x02,  0xff, 0xfe };
  uint8_t w = constrain(map(g_State.battery.mv, 3300, 4200, 0, 13), 0, 13);
  uint32_t color = g_State.battery.mv > 3500 ? COLOR_GREEN : COLOR_RED;
  tft.drawBitmap(93, 5, ICON_BATTERY, 16, 6, color);
  tft.fillRect(94, 6, w, 4, color);
  tft.fillRect(94+w, 6, 13-w, 4, COLOR_BACKGROUND);
}

static void drawTime()
{
  char bufTime[12] = { 0 };
  // Precompute the time string before we draw to reduce flicker
  if (hm.state.UpdateUtc != 0)
  {
    struct tm* ltime;
    // time_t is defined as "long" on ESP32 and our time is signed 32-bit UNIX epoch
    const time_t utime = hm.state.UpdateUtc;
    ltime = localtime(&utime);
    bool pm = ltime->tm_hour > 11;
    if (ltime->tm_hour > 12)
      ltime->tm_hour -= 12;
    snprintf(bufTime, sizeof(bufTime), "%02d:%02d:%02d %s",
      ltime->tm_hour, ltime->tm_min, ltime->tm_sec, pm ? "PM" : "AM");

    tft.setTextDatum(TR_DATUM);
    if (hm.isCommunicating())
      tft.setTextColor(COLOR_HIGHLIGHT);
    else
      tft.setTextColor(COLOR_RED);
  }

  tft.fillRect(138, 3, 98, 12, COLOR_DRAW_DARK);
  if (hm.state.UpdateUtc != 0)
    tft.drawString(bufTime, 237, 2);
}

static void drawPitGraph()
{
  // borders
  tft.drawFastHLine(15, 36, 47, COLOR_DRAW_LIGHT);
  tft.drawFastHLine(15, 52, 47, COLOR_DRAW_LIGHT);

  // bars
  float tempAvg = g_State.tempTotal / TEMP_DELTA_COUNT;
  //Serial.print("AVG:"); Serial.println(tempAvg, 2);
  for (uint8_t i = 0; i < TEMP_DELTA_COUNT; ++i)
  {
    uint8_t left = i * 4 + 15;
    int8_t delta = constrain(100.0f * (g_State.tempHist[i] - tempAvg), -70.0f, 70.0f);
    // ceil() except always move away from 0, so even a temp difference of 0.2 -> 1 pixel, or -0.2 => 1 pixel
    if (delta < 0)
      delta -= 8;
    else
      delta += 8;
    delta /= 10;
    if (delta > 0)
      tft.fillRect(left, 44-delta, 3, delta, COLOR_DRAW_DARK);
    else if (delta < 0)
      tft.fillRect(left, 45, 3, -delta, COLOR_DRAW_DARK);
    tft.drawFastHLine(left, 44, 3, COLOR_DRAW_MID);
  }
}

static void drawPit()
{
  // Pit Probe
  tft.fillRect(6, 5, 63, 64, COLOR_PROBE_BACK);

  // Name / Temperature
  tft.setTextColor(COLOR_HIGHLIGHT);
  tft.drawString(hm.state.Probes[0].Name, 37, 5);
  tft.setTextColor(COLOR_RED);
  if (hm.state.Probes[0].HasTemperature)
    tft.drawFloat(hm.state.Probes[0].Temperature, 1, 37, 20);
  else
    tft.drawString("off", 37, 21);

  drawPitGraph();

  // Lid countdown
  // Text is centered but it is two colors, so calculate the position of
  // the entire string then print it in two parts
  char bufLid[9];
  uint32_t color;
  if (hm.state.LidCountdown == 0)
  {
    strcpy(bufLid, "Lid: OK");
    color = COLOR_GREEN;
  }
  else
  {
    snprintf(bufLid, sizeof(bufLid), "Lid:%d", hm.state.LidCountdown);
    color = COLOR_RED;
  }

  uint8_t w = tft.textWidth(bufLid);
  tft.setTextDatum(TL_DATUM);
  tft.setCursor(37-w/2, 66);
  tft.setTextColor(COLOR_HIGHLIGHT, COLOR_PROBE_BACK);
  tft.print("Lid:");
  tft.setTextColor(color, COLOR_PROBE_BACK);
  tft.print(&bufLid[4]);
  tft.setTextDatum(TC_DATUM);
}

static void drawFood()
{
  // Food probes
  for (uint8_t i = 1; i < TEMP_COUNT; ++i)
  {
    spr.fillSprite(COLOR_PROBE_BACK);

    // Name
    spr.setTextColor(COLOR_HIGHLIGHT);
    spr.drawString(hm.state.Probes[i].Name, 24, -1);

    // Temperature
    spr.setTextColor(COLOR_RED);
    if (hm.state.Probes[i].HasTemperature)
      spr.drawFloat(hm.state.Probes[i].Temperature, 1, 24, 13);
    else
      spr.drawString("off", 24, 13);

    // DPH
    if (hm.state.Probes[i].HasTemperatureDph && hm.state.Probes[i].TemperatureDph != 0.0f)
    {
      bool positive = (hm.state.Probes[i].TemperatureDph >= 0.0f);
      uint32_t color = positive ? COLOR_GREEN : COLOR_RED;

      // Icon
      const uint8_t ICON_UP[5] = { 0x10, 0x38, 0x7c, 0xfe, 0x7c };
      const uint8_t ICON_DN[5] = { 0x7c, 0xfe, 0x7c, 0x38, 0x10 };
      spr.drawBitmap(21, 28, positive ? ICON_UP : ICON_DN, 8, 5, color);

      // Value
      spr.setTextColor(color);
      char bufTemp[7];
      snprintf(bufTemp, sizeof(bufTemp), "%+.1f", hm.state.Probes[i].TemperatureDph);
      spr.drawString(bufTemp, 24, 33);
    }
    spr.pushSprite(55 * (i - 1) + 75, 21);
  } // for Food probes
}

static void drawSetpoint()
{
  if (g_State.lastStatus.Enabled == hm.state.Output.Enabled
    && g_State.lastStatus.Setpoint == hm.state.Setpoint
    && g_State.lastStatus.HasPit == hm.state.Probes[0].HasTemperature
    && g_State.lastStatus.Pit == hm.state.Probes[0].Temperature)
    return;

  // Horizontal gauge
  tft.fillRect(8, 101, 68, 22, COLOR_DRAW_DARK);
  bool locked = false;
  if (hm.state.Output.Enabled && hm.state.Probes[0].HasTemperature)
  {
    float tempDiff = hm.state.Probes[0].Temperature - hm.state.Setpoint;
    locked = (tempDiff >= -5.0f) && (tempDiff <= 5.0f);

    // Pip
    int32_t left = constrain((int32_t)(6.0f * (tempDiff + 5.0f)), -2, 63);
    tft.fillRect(10 + left, 102, 3, 20, COLOR_RED);
  }

  // Setpoint LOCKed
  tft.fillRect(10, 90, 8, 8, locked ? COLOR_GREEN : COLOR_BACKGROUND);

  // Gauge hash marks
  for (uint8_t i = 0; i <= 10; ++i)
    tft.drawFastVLine(11 + 6 * i, 108, 5, COLOR_DRAW_LIGHT);

  // Setpoint
  tft.setTextColor(COLOR_HIGHLIGHT);
  if (hm.state.Output.Enabled)
    tft.drawNumber(hm.state.Setpoint, 43, 111);
  else
    tft.drawString("Off", 43, 111);

  g_State.lastStatus.Enabled = hm.state.Output.Enabled;
  g_State.lastStatus.Setpoint = hm.state.Setpoint;
  g_State.lastStatus.HasPit = hm.state.Probes[0].HasTemperature;
  g_State.lastStatus.Pit = hm.state.Probes[0].Temperature;
}

static void drawOutputBar(int32_t x, int32_t y, uint8_t val)
{
  // Draw a horizontal bar of blocks, one for each 10% in COLOR_DRAW_LIGHT,
  // or COLOR_RED if val = 100%
  // Final block is always colored COLOR_HIGHLIGHT
  tft.fillRect(x, y, 10 * 11, 6, COLOR_BACKGROUND);

  uint32_t color = (val == 100) ? COLOR_RED : COLOR_DRAW_LIGHT;
  for (uint8_t i = 0; i < ((val - 10) / 10); ++i)
    tft.fillRect(x + i * 11, y, 10, 6, color);
  if (val > 0)
    tft.fillRect(x + ((val - 10) / 10) * 11, y, 10, 6, COLOR_HIGHLIGHT);
}

#define OUTPUTBAR_IF_CHANGED(x, y, member) { \
  if (g_State.lastStatus.member != hm.state.Output.member) \
  { \
    g_State.lastStatus.member = hm.state.Output.member; \
    drawOutputBar(x, y, g_State.lastStatus.member); \
  } \
}

static void drawOutput()
{
  // Output %
  if (g_State.lastStatus.Current != hm.state.Output.Current)
  {
    tft.setTextColor(COLOR_HIGHLIGHT);
    tft.fillRect(86, 89, 33, 12, COLOR_BACKGROUND);
    tft.drawNumber(hm.state.Output.Current, 103, 88); // drawing the % is too wide :(
  }

  // Output Bars
  OUTPUTBAR_IF_CHANGED(121, 92, Current);
  OUTPUTBAR_IF_CHANGED(121, 105, Fan);
  OUTPUTBAR_IF_CHANGED(121, 118, Servo);
}

static void updateDisplay()
{
  g_State.hmUpdated = false;
  uint32_t start = millis();

  tft.setFreeFont(&Orbitron_Medium_12);
  tft.setTextDatum(TC_DATUM);

  drawPit(); yield();
  drawFood(); yield();
  drawSetpoint(); yield();
  drawOutput(); yield();
  drawTime(); yield();

  Serial.print(F("updateDisplay "));
  Serial.print(millis() - start, DEC);
  Serial.println(F("ms"));
}

static void drawBackground()
{
  tft.setRotation(g_State.rotation);
  tft.setSwapBytes(true);
  tft.pushImage(0, 0, 240, 135, background);
  tft.setSwapBytes(false);
}

static void displayFlip()
{
  g_State.rotation = (g_State.rotation + 2) % 4;
  drawBackground();

  // Force everything to repaint
  g_State.hmUpdated = true;
  memset(&g_State.lastStatus, 0, sizeof(g_State.lastStatus));
  g_State.lastStatus.Current = 0xff;
}

static void readBattery()
{
  uint32_t now = millis();
  if (g_State.battery.lastRead != 0 && (now - g_State.battery.lastRead) < 1000)
    return;

  g_State.battery.lastRead = now;

  // Determine Vref if we haven't yet by querying the calibration info
  if (g_State.battery.vref == 0)
  {
    esp_adc_cal_characteristics_t adcCal;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adcCal);
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
      g_State.battery.vref = adcCal.vref;
    else
      g_State.battery.vref = 1100;
  } // if need vref

  uint32_t val = analogRead(APIN_BATTERY);
  g_State.battery.mv = val * 2 * 33 * g_State.battery.vref / (4095 * 10);
  //Serial.printf("Battery: %dmV\n", g_State.battery.mv);
  drawBattery();
}

static void powerDown()
{
  digitalWrite(TFT_BL, LOW);
  // Allow enough time for the button to be released
  while (digitalRead(DPIN_BTN_ROTATE) == LOW)
    delay(100);

  tft.writecommand(TFT_DISPOFF);
  tft.writecommand(TFT_SLPIN);

  digitalWrite(DPIN_ADC_EN, INPUT);
  pinMode(DPIN_ADC_EN, INPUT);
  adc_power_off();

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);
  delay(100);
  esp_deep_sleep_start();
  while (1) {}
}

static void checkButton()
{
  // Rotate the display 180 if button is pressed
  // Or power down if held for ~1 second
  if (digitalRead(DPIN_BTN_ROTATE) == LOW)
  {
    ++g_State.btnHoldCount;
    if (g_State.btnHoldCount > 9)
      powerDown(); // does not return
  }
  else if (g_State.btnHoldCount > 0)
  {
    g_State.btnHoldCount = 0;
    displayFlip();
  }
}

static void setupTft()
{
  tft.init();
  g_State.rotation = 3;
  drawBackground();

  spr.setColorDepth(16);
  spr.createSprite(48, 48);
  spr.setFreeFont(&Orbitron_Medium_12);
  spr.setTextDatum(TC_DATUM);
}

static void setupWifi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.mode(WIFI_STA);
    // Only use the compiled-in wifi credentials if the button is pressed
    if (digitalRead(DPIN_BTN_WIFI) == HIGH)
      WiFi.begin();
    else
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.print(F("$UCID,SMUSHI," __DATE__ " " __TIME__ "\n"));

  pinMode(DPIN_BTN_ROTATE, INPUT);
  pinMode(DPIN_BTN_WIFI, INPUT);
  pinMode(DPIN_ADC_EN, OUTPUT);
  digitalWrite(DPIN_ADC_EN, HIGH);

  setupWifi();
  setupTft();

  hm.onHmStatus = &proxy_onHmStatus;
  hm.onWifiConnect = &proxy_onWifiConnect;
  hm.onWifiDisconnect = &proxy_onWifiDisconnect;
  hm.onConnect = &proxy_onConnect;
  hm.onDisconnect = &proxy_onDisconnect;
  hm.onError = &proxy_onError;

  g_State.lastStatus.Current = 0xff; // force drawing the output % the first time

  configTzTime(TZ, nullptr);
}

void loop()
{
  hm.update();
  checkButton();
  readBattery();

  if (g_State.hmUpdated)
    updateDisplay(); // clears flag

  delay(100);
}
