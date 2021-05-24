#include <HeaterMeterClient.h>
#include <TM1637Display.h>
#include "segment_chars.h"

#define WIFI_SSID       "network"
#define WIFI_PASSWORD   "password"
#define HEATERMEATER_IP "" // IP or leave blank to use discovery
#define LED_BRIGHTNESS  7  // 1 (low) - 7 (high)

static HeaterMeterClient hm(HEATERMEATER_IP);

#define DPIN_LED_CLK D1
static TM1637Display led0(DPIN_LED_CLK, D2, 90);
static TM1637Display led1(DPIN_LED_CLK, D7, 90);
static TM1637Display led2(DPIN_LED_CLK, D6, 90);
static TM1637Display led3(DPIN_LED_CLK, D5, 90);
static TM1637Display* leds[TEMP_COUNT];

static float g_LastTemps[TEMP_COUNT];
static uint8_t g_HmTempsChanged;
static err_t g_LastClientError;

static void displayTemps(void)
{
  --g_HmTempsChanged;

  bool isLid = hm.state.LidCountdown > 0;
  for (uint8_t i = 0; i < TEMP_COUNT; ++i)
  {
    //if (i == 1) { leds[1]->showNumberDec(ESP.getFreeHeap() / 10); continue; }
    if (g_HmTempsChanged == 0)
      g_LastTemps[i] = hm.state.Probes[i].Temperature;

    // If Lid mode, show "Lid Mode" in LED[1,2] and the countdown in LED[3]
    if (isLid && i == 1)
      leds[1]->setSegments((const uint8_t[4]){ TM1637_L, TM1637_I, TM1637_D, 0 });
    else if (isLid && i == 2)
      leds[2]->setSegments((const uint8_t[4]){ TM1637_O, TM1637_P, TM1637_E, TM1637_N });
    else if (isLid && i == 3)
      leds[3]->showNumberDecEx(hm.state.LidCountdown, 0);

    else if (hm.state.Probes[i].HasTemperature)
    {
      // LERP from the last temperature just to make it seem like there's more going on here
      float t = g_LastTemps[i] + ((4 - g_HmTempsChanged) / 4.0f) * (hm.state.Probes[i].Temperature - g_LastTemps[i]);
      leds[i]->showNumberDecEx(int32_t(t * 10.0f), 0b00100000);
    }
    else
      leds[i]->clear();

    yield();
  }
}

static void proxy_onHmStatus(void)
{
  // Update over the next 4x loops
  g_HmTempsChanged = 4;
}

static void ledsShowIp(void)
{
  uint32_t ip = (uint32_t)hm.getRemoteIP();
  leds[0]->showNumberDec(ip & 0xff, false, 4, 0);
  leds[1]->showNumberDec((ip >> 8) & 0xff, false, 4, 0);
  leds[2]->showNumberDec((ip >> 16) & 0xff, false, 4, 0);
  leds[3]->showNumberDec(ip >> 24, false, 4, 0);
}

static void ledsShowDisconnected(void)
{
  // Display DISCONNECTED
  if (g_LastClientError != 0)
    leds[0]->showNumberDec(g_LastClientError);
  else
    leds[0]->clear();
  leds[1]->setSegments((const uint8_t[4]){ TM1637_D, TM1637_I, TM1637_S, TM1637_C });
  leds[2]->setSegments((const uint8_t[4]){ TM1637_O, TM1637_N, TM1637_N, TM1637_E });
  leds[3]->setSegments((const uint8_t[4]){ TM1637_C, TM1637_T, TM1637_E, TM1637_D });
}

static void ledsShowNoWifi(void)
{
  // Display No Wifi
  //if (g_LastClientError != 0)
  //  leds[0]->showNumberDec(g_LastClientError);
  //else
  leds[0]->clear();
  leds[1]->setSegments((const uint8_t[4]){ 0, TM1637_N, TM1637_O, 0 });
  leds[2]->setSegments((const uint8_t[4]){ TM1637_W, TM1637_I, TM1637_F, TM1637_I });
  leds[3]->clear();
}

static void proxy_onConnect(void)
{
  Serial.println(F("onConnect"));
  ledsShowIp();
  g_LastClientError = 0;
}

static void proxy_onDisconnect(void)
{
  Serial.println(F("onDisconnect"));
  ledsShowDisconnected();
}

static void proxy_onError(err_t err)
{
  Serial.print(F("Socket Error ")); Serial.println(err);
  g_LastClientError = err;

  // There's no "disconnect" event if never connected, so trigger display err
  if (hm.getProtocolState() == HeaterMeterClient::hpsConnecting)
    ledsShowDisconnected();
}

static void proxy_onWifiConnect(void)
{
  Serial.println(F("onWifiConnect"));
  ledsShowDisconnected();
}

static void proxy_onWifiDisconnect(void)
{
  Serial.println(F("onWifiDisconnect"));
  ledsShowNoWifi();
}

static void setupLeds(void)
{
  leds[0] = &led0;
  leds[1] = &led1;
  leds[2] = &led2;
  leds[3] = &led3;
  for (uint8_t i = 0; i < TEMP_COUNT; ++i)
  {
    leds[i]->setBrightness(LED_BRIGHTNESS);
    leds[i]->clear();
    yield();
  }
  ledsShowNoWifi();
}

void setup()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  delay(100);
  Serial.begin(115200);
  Serial.print(F("$UCID,MeterMonitor," __DATE__ " " __TIME__ "\n"));

  hm.onHmStatus = &proxy_onHmStatus;
  hm.onWifiConnect = &proxy_onWifiConnect;
  hm.onWifiDisconnect = &proxy_onWifiDisconnect;
  hm.onConnect = &proxy_onConnect;
  hm.onDisconnect = &proxy_onDisconnect;
  hm.onError = &proxy_onError;

  setupLeds();
}

void loop()
{
  hm.update();
  // Updating each LED takes ~22ms with 100uS delay
  if (g_HmTempsChanged > 0)
    displayTemps();
  delay(100);
}
