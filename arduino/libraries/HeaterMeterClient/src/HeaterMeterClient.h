#pragma once

#include <Arduino.h>
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#else
  #include <WiFi.h>
  #include <AsyncTCP.h>
#endif
#include <ArduinoJson.h>

#define TEMP_COUNT  4
#define PROBE_NAME_SIZE 13

struct HeaterMeterClientProbe
{
  char Name[PROBE_NAME_SIZE+1];
  float Temperature;
  bool HasTemperature;
  int16_t AlarmLow;
  int16_t AlarmHigh;
  char AlarmRinging;
  float TemperatureDph;
  bool HasTemperatureDph;

  void clear(void);
};

struct HeaterMeterClientPidInternals
{
  uint32_t LastUpdate;
  float P;
  float I;
  float D;
  float dT;

  void clear();
  bool valid() { return ((LastUpdate != 0) && (millis() - LastUpdate < 3000)); }
};

struct HeaterMeterClientPidOutput
{
  bool Enabled; // false if "Off"
  uint8_t Current;
  uint8_t Fan;
  uint8_t Servo;
  struct HeaterMeterClientPidInternals Internals;

  void clear(void);
};

struct HeaterMeterClientPid
{
  time_t UpdateUtc;
  HeaterMeterClientProbe Probes[TEMP_COUNT];
  HeaterMeterClientPidOutput Output;
  int16_t Setpoint;
  uint16_t LidCountdown;

  void clear(void);
};

class HeaterMeterClient
{
public:
  enum HmclientProtocolState { hpsNone, hpsNoNetwork, hpsDiscover, hpsReconnectDelay,
    hpsDisconnected, hpsConnecting, hpsConnected, hpsRequestSent, hpsHeaders, hpsChunk, hpsChunkData };

  HeaterMeterClient(const char* host);

  // The current HeaterMeter state updated when ProtocolState is >hpsHeaders
  HeaterMeterClientPid state;
  HmclientProtocolState getProtocolState(void) {  return _protocolState; }
  IPAddress getRemoteIP(void) { return _client.remoteIP();  }
  const char* getHost(void) const { return _host; }
  void update();
  bool isCommunicating(void) const { return _protocolState >= hpsChunk; }

  // Events
  std::function<void(void)> onWifiConnect;
  std::function<void(void)> onWifiDisconnect;
  std::function<void(void)> onConnect;
  std::function<void(void)> onDisconnect;
  std::function<void(void)> onHmStatus;
  std::function<void(void)> onPidInt;
  std::function<void(err_t)> onError;
  std::function<void(HmclientProtocolState)> onProtocolStateChange;

private:
  AsyncClient _client;
  char _host[16];
  char _eventType[32];
  char _lineBuffer[1024];
  uint16_t _lineBufferPos;
  uint16_t _expectedChunkSize;
  HmclientProtocolState _protocolState;
  uint32_t _lastClientActivity;
  uint32_t _reconnectDelay;

  enum NotifyPendingType { nptHmStatus, nptPidInt };
  uint32_t _notifyPending;
  void setNotifyPending(NotifyPendingType npt);
  bool getNotifyPending(NotifyPendingType npt) const;
  void clearNotifyPending(void);

  bool readLine(char** pos, size_t* len);
  void updateProxyFromJson(JsonDocument& doc);
  void handleHmStatus(char* data);
  void handlePidInt(char* data);
  void handlePeaks(char* data);
  void handleServerSentEvent(char* data);
  void handleServerSentLine(void);
  void clientConnect(void);
  void clientSendRequest(void);
  void clientCheckTimeout(void);
  bool setProtocolState(HmclientProtocolState hps);
  void discover(void);

  // AsyncClient callbacks
  void onData_cb(void* data, size_t len);
  void onConnect_cb(void);
  void onDisconnect_cb(void);
  void onError_cb(err_t err);
  static void asynctcp_onData(void* arg, AsyncClient* c, void* data, size_t len);
  static void asynctcp_onConnect(void* arg, AsyncClient* c);
  static void asynctcp_onDisconnect(void* arg, AsyncClient* c);
  static void asynctcp_onError(void* arg, AsyncClient* c, err_t err);
};


