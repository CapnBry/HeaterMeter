#include <HeaterMeterClient.h>

#define WIFI_SSID       "network"
#define WIFI_PASSWORD   "password"
#define HEATERMEATER_IP "192.168.2.54"

static HeaterMeterClient hm(HEATERMEATER_IP);

static void onHmStatus()
{
  // Called every 1-5s as new data comes pushed
  Serial.print(hm.state.Probes[0].Name);
  Serial.print('=');
  Serial.println(hm.state.Probes[0].Temperature);
}

void setup()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.begin(115200);

  hm.onHmStatus = &onHmStatus;
}

void loop()
{
  hm.update();
}
