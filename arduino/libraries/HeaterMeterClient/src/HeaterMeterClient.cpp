#include "HeaterMeterClient.h"
#include <ESP8266HTTPClient.h>

#define CLIENT_HTTP_TIMEOUT 6000

//static void hexdump(char* data, size_t len)
//{
//  while (len > 0)
//  {
//    uint8_t linepos = 0;
//    char* linestart = data;
//    // Binary part
//    while (len > 0 && linepos < 16)
//    {
//      if (*data < 0x0f)
//        Serial.write('0');
//      Serial.print(*data, HEX);
//      Serial.write(' ');
//      ++data;
//      ++linepos;
//      --len;
//    }
//
//    // Spacer to align last line
//    for (uint8_t i = linepos; i < 16; ++i)
//      Serial.print("   ");
//
//    // ASCII part
//    for (uint8_t i = 0; i < linepos; ++i)
//      Serial.write((linestart[i] < ' ') ? '.' : linestart[i]);
//    Serial.write('\n');
//  }
//}

void HeaterMeterClientProbe::clear(void)
{
  Name[0] = '\0'; 
  //float Temperature;
  HasTemperature = false;
  AlarmLow = 0;;
  AlarmHigh = 0;
  AlarmRinging = ' ';
  //float TemperatureDph;
  HasTemperatureDph = false;
}

void HeaterMeterClientPidInternals::clear(void)
{
  LastUpdate = 0;

  P = 0.0f;
  I = 0.0f;
  D = 0.0f;
  dT = 0.0f;
}

void HeaterMeterClientPidOutput::clear(void)
{
  Enabled = false;
  Current = 0;
  Fan = 0;
  Servo = 0;

  Internals.clear();
}

void HeaterMeterClientPid::clear(void)
{
  for (uint8_t i = 0; i < TEMP_COUNT; ++i)
    Probes[i].clear();
  Output.clear();
}

HeaterMeterClient::HeaterMeterClient(const char* host) :
  _protocolState(hpsNone), _reconnectDelay(60000)
{
  strlcpy(_host, host, sizeof(_host));
}

void HeaterMeterClient::asynctcp_onData(void* arg, AsyncClient* c, void* data, size_t len)
{
  ((HeaterMeterClient*)arg)->onData_cb(data, len);
}

void HeaterMeterClient::asynctcp_onConnect(void* arg, AsyncClient* c)
{
  ((HeaterMeterClient*)arg)->onConnect_cb();
}

void HeaterMeterClient::asynctcp_onDisconnect(void* arg, AsyncClient* c)
{
  ((HeaterMeterClient*)arg)->onDisconnect_cb();
}

void HeaterMeterClient::asynctcp_onError(void* arg, AsyncClient* c, err_t err)
{
  ((HeaterMeterClient*)arg)->onError_cb(err);
}

bool HeaterMeterClient::readLine(char** pos, size_t* len)
{
  // Looking at HTTP lines each ending in [0x0d] 0x0a
  bool wasNl = false;
  while ((*len > 0) && (_lineBufferPos < sizeof(_lineBuffer) - 1) & !wasNl)
  {
    _lineBuffer[_lineBufferPos++] = **pos;
    wasNl = (**pos == 0x0a);
    ++* pos;
    --* len;
  } /* while building line */

  _lineBuffer[_lineBufferPos] = '\0';
  return wasNl;
}

void HeaterMeterClient::updateProxyFromJson(JsonDocument& doc)
{
  for (uint8_t i = 0; i < TEMP_COUNT; ++i)
  {
    HeaterMeterClientProbe& p = state.Probes[i];
    const JsonObject& jp = doc["temps"][i];

    // Name
    strlcpy(p.Name, jp["n"], sizeof(p.Name));
    // Temperature
    const JsonVariant& t = jp["c"];
    p.HasTemperature = !t.isNull();
    if (p.HasTemperature)
      p.Temperature = t;
    else
      p.Temperature = 0.0f;
    // DPH
    p.HasTemperatureDph = jp.containsKey("dph");
    if (p.HasTemperatureDph)
      p.TemperatureDph = jp["dph"];
    else
      p.TemperatureDph = 0.0f;
    // Alarms
    p.AlarmHigh = jp["a"]["h"];
    p.AlarmLow = jp["a"]["l"];
    p.AlarmRinging = jp["a"]["r"].isNull() ? ' ' : jp["a"]["r"];
  }

  // Output
  const JsonObject& output = doc["fan"];
  state.Output.Current = output["c"];
  state.Output.Fan = output["f"];
  state.Output.Servo = output["s"];
  state.Output.Enabled = !doc["set"].isNull();

  // Setpoint
  if (state.Output.Enabled)
    state.Setpoint = doc["set"];
  else
    state.Setpoint = 0;

  // Lid
  state.LidCountdown = doc["lid"];

  if (onHmStatus)
    onHmStatus();
}

void HeaterMeterClient::handleHmStatus(char* data)
{
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, data);
  if (err)
  {
    Serial.print(F("Could not deserialize hmstatus event: "));
    Serial.println(err.c_str());
    return;
  }

  updateProxyFromJson(doc);
}

void HeaterMeterClient::handlePidInt(char* data)
{
  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, data);
  if (err)
  {
    Serial.print(F("Could not deserialize pidint event: "));
    Serial.println(err.c_str());
    return;
  }

  state.Output.Internals.LastUpdate = millis();
  state.Output.Internals.P = doc["p"];
  state.Output.Internals.I = doc["i"];
  state.Output.Internals.D = doc["d"];
  state.Output.Internals.dT = doc["t"];

  if (onPidInt)
    onPidInt();
}

void HeaterMeterClient::handleServerSentEvent(char* data)
{
  //Serial.print(_eventType); Serial.write('='); Serial.print(data);

  if (strcmp(_eventType, "hmstatus") == 0)
    handleHmStatus(data);
  else if (strcmp(_eventType, "pidint") == 0)
    handlePidInt(data);
  else
  {
    //Serial.print(F("Unhandled server-sent event type: "));
    //Serial.println(_eventType);
  }
}

void HeaterMeterClient::handleServerSentLine(void)
{
  if (strncmp(_lineBuffer, "event: ", 7) == 0)
    strlcpy(_eventType, _lineBuffer + 7, _lineBufferPos - 6 - 1);
  else if (strncmp(_lineBuffer, "data: ", 6) == 0)
    handleServerSentEvent(_lineBuffer + 6);
  else if (strncmp(_lineBuffer, "retry: ", 7) == 0)
    _reconnectDelay = constrain(atoi(_lineBuffer + 7), 1000, 120*1000);
//  else
//    Serial.print(_lineBuffer);
}

void HeaterMeterClient::onData_cb(void* data, size_t len)
{
  //hexdump((char*)data, len);
  char* pos = (char*)data;
  _lastClientActivity = millis();

  if (_protocolState == hpsRequestSent)
  {
    _lineBufferPos = 0;
    setProtocolState(hpsHeaders);
  }

  while (len > 0 && readLine(&pos, &len))
  {
    switch (_protocolState)
    {
    case hpsHeaders:
      // Line will be blank if we've read the last header
      if (strcmp(_lineBuffer, "\r\n") == 0)
        setProtocolState(hpsChunk);
      //else
      //  Serial.print(g_LineBuffer);
      break;

    case hpsChunk:
      _expectedChunkSize = strtol(_lineBuffer, NULL, 16);
      setProtocolState(hpsChunkData);
      //Serial.print("Expecting chunk size "); Serial.println(_expectedChunkSize, DEC);
      break;

    case hpsChunkData:
      if (_expectedChunkSize == 0) // LineBuffer should be 0x0d 0x0a we can just assume
      {
        setProtocolState(hpsChunk);
      }
      else
      {
        _expectedChunkSize -= _lineBufferPos;
        if (_lineBufferPos > 1) // a server-sent event ends in \n\n so there will be a blank line
          handleServerSentLine();
      }
      break;

    default:
      len = 0; // ERROR
    } /* hmProtocolState */

    _lineBufferPos = 0;
  }  /* while len */
}

void HeaterMeterClient::onConnect_cb(void)
{
  setProtocolState(hpsConnected);
  _lastClientActivity = millis();
}

void HeaterMeterClient::onDisconnect_cb(void)
{
  setProtocolState(hpsReconnectDelay);
  _lastClientActivity = millis();
  state.clear();
}

void HeaterMeterClient::onError_cb(err_t err)
{
  if (onError)
    onError(err);
}

void HeaterMeterClient::clientConnect(void)
{
  _client.onData(&asynctcp_onData, this);
  _client.onConnect(&asynctcp_onConnect, this);
  _client.onDisconnect(&asynctcp_onDisconnect, this);
  _client.onError(&asynctcp_onError, this);
  // data should be received every 2-5s 
  _client.setRxTimeout(CLIENT_HTTP_TIMEOUT);
  _client.connect(_host, 80);

  setProtocolState(hpsConnecting);
}

void HeaterMeterClient::clientSendRequest(void)
{
  if (_client.canSend())
  {
    _client.write("GET /luci/lm/stream HTTP/1.1\r\n\r\n");
    setProtocolState(hpsRequestSent);
    _lastClientActivity = millis();
  }
}

void HeaterMeterClient::clientCheckTimeout(void)
{
  if ((millis() - _lastClientActivity) > CLIENT_HTTP_TIMEOUT)
    _client.close();
}

void HeaterMeterClient::discover(void)
{
  _lastClientActivity = millis();
  // If there is an IP then there's no need to discover
  if (strlen(_host) > 0)
  {
    setProtocolState(hpsDisconnected);
    return;
  }

  // Request the JSON device list from HeaterMeter LIVE and use its best guess at the IP
  HTTPClient http;
  WiFiClient client;
  http.useHTTP10(true);
  http.begin(client, "http://heatermeter.com/devices/?fmt=json");
  int responseCode = http.GET();
  if (responseCode == 200)
  {
    // Use a JSON filter to only parse the "guess" address field
    StaticJsonDocument<32> filter;
    filter["guess"] = true;
    // Deserialize from HTTP stream
    StaticJsonDocument<64> doc;
    DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (err)
    {
      Serial.print(F("Could not deserialize JSON discover: "));
      Serial.println(err.c_str());
      goto discoverError;
    }

    const JsonVariant& guess = doc["guess"];
    if (guess.isNull())
    {
      Serial.print(F("No HeaterMeter discovered"));
      goto discoverError;
    }

    // Found IP
    strlcpy(_host, guess, sizeof(_host));
    setProtocolState(hpsDisconnected);
    return;
  }
  else
  {
    Serial.print(F("Discover failed: "));
    Serial.println(responseCode, DEC);
    http.end();
  }

discoverError:
  setProtocolState(hpsReconnectDelay);
}

/* Return true if state actually changed */
bool HeaterMeterClient::setProtocolState(HmclientProtocolState hps)
{
  if (_protocolState == hps)
    return false;
  //Serial.print(F("State=")); Serial.println(static_cast<uint8_t>(hps), DEC);
  // Notify before change in case the client needs to know the old state
  if (onProtocolStateChange)
    onProtocolStateChange(hps);

  HmclientProtocolState oldState = _protocolState;
  _protocolState = hps;

  switch (_protocolState)
  {
  case hpsNoNetwork: if (oldState > hpsNoNetwork && onWifiDisconnect) onWifiDisconnect(); break;
  case hpsDiscover: if (oldState == hpsNoNetwork && onWifiConnect) onWifiConnect(); break;
  case hpsReconnectDelay:if (oldState > hpsConnecting && onDisconnect) onDisconnect(); break;
  case hpsConnected: if (onConnect) onConnect(); break;
  case hpsNone:
  case hpsDisconnected:
  case hpsConnecting:
  case hpsRequestSent:
  case hpsHeaders:
  case hpsChunk:
  case hpsChunkData:
    break;
  }


  return true;
}

void HeaterMeterClient::update(void)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    setProtocolState(hpsNoNetwork);
    return;
  }

  switch (_protocolState)
  {
  case hpsNone:
  case hpsNoNetwork:
    setProtocolState(hpsDiscover);
    break;
  case hpsDiscover:
    discover();
    break;
  case hpsReconnectDelay:
    if (millis() - _lastClientActivity > _reconnectDelay)
      setProtocolState(hpsDiscover);
    break;
  case hpsDisconnected:
    state.clear();
    clientConnect();
    break;
  case hpsConnecting:
    break;
  case hpsConnected:
    clientSendRequest();
    break;
  case hpsRequestSent:
  case hpsHeaders:
  case hpsChunk:
  case hpsChunkData:
    clientCheckTimeout();
    break;
  } /* switch */
}