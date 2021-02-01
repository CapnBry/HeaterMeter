#include <HeaterMeterClient.h>

#define WIFI_SSID       "network"
#define WIFI_PASSWORD   "password"
#define HEATERMEATER_IP "192.168.2.54"

static HeaterMeterClient hm(HEATERMEATER_IP);

static void onHmStatus()
{
  // Called every 1-5s as new data comes pushed

  // Setpoint and Output
  Serial.print(F("SetPoint="));
  Serial.print(hm.state.Setpoint, DEC);
  Serial.print(F(" Output="));
  if (hm.state.Output.Enabled)
  {
    Serial.print(hm.state.Output.Current, DEC);
    Serial.print(F("% Fan="));
    Serial.print(hm.state.Output.Fan, DEC);
    Serial.print(F("% Servo="));
    Serial.print(hm.state.Output.Servo, DEC);
    Serial.println('%');
  }
  else
    Serial.println(F("Off"));

  // Lid Mode active / time remaining
  if (hm.state.LidCountdown > 0)
  {
    Serial.print(F("Lid open. Remaining="));
    Serial.println(hm.state.LidCountdown);
  }

  // Probe information
  for (uint8_t i=0; i<TEMP_COUNT; ++i)
  {
    Serial.print('\t');
    Serial.print(hm.state.Probes[i].Name);
    Serial.print('=');
    Serial.print(hm.state.Probes[i].Temperature, 1);

    Serial.print(F(" Alarms="));
    Serial.print(hm.state.Probes[i].AlarmLow, DEC);
    Serial.print('/');
    Serial.print(hm.state.Probes[i].AlarmHigh, DEC);
    if (hm.state.Probes[i].AlarmRinging != ' ')
    {
      Serial.print(' ');
      Serial.print(hm.state.Probes[i].AlarmRinging);
      Serial.println(F(" Ringing!"));
    }
    else
      Serial.println();

    if (hm.state.Probes[i].HasTemperatureDph)
    {
      Serial.print(F("\t\tDegrees per hour="));
      Serial.println(hm.state.Probes[i].TemperatureDph, 1);
    }
  }

  Serial.println(F("----------"));
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