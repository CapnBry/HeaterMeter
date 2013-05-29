// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#ifndef __RFMANAGER_H__
#define __RFMANAGER_H__

#include <rf12_itplus.h>

#define RFSOURCEID_ANY    0x7f
#define RFSOURCEID_NONE   0xff

#define RF_SOURCE_COUNT   4
// Actually transmit the fan speed every RF_SEND_INTERVAL sendUpdate() calls
// Undefine or set to 0 to disable transmission
#define RF_SEND_INTERVAL  5

// The count of milliseconds with no receive that the source is considered stale
// This should be large enough to allow the remote node to sleep
#define RF_STALE_TIME (3 * 60 * 1000UL)

#define RFSOURCEFLAG_LowBattery   bit(0)
#define RFSOURCEFLAG_RecentReset  bit(1)
#define RFSOURCEFLAG_NativeItPlus bit(2)

typedef struct tagRf12Packet
{
  // 4 bits = "9"
  // 6 bits of ID
  // 1 bit Reset flag (set for first few hours after poweron)
  // 1 bit native flag (0 for IT+ transmitterm, 1 for lmremote)
  // 12 bits of data, either BCD (IT+) or raw analog read (lmremote)
  // 1 bit low battery inticator
  // 7 bits hygrometer
  // 8 bit CRC
  unsigned char byte0;
  unsigned char byte1;
  unsigned char byte2;
  unsigned char hygro;
  unsigned char crc;
} rf12_packet_t;

class RFSource
{
public:
  RFSource(void) : _id(RFSOURCEID_NONE) {};
  
  // The 6 bitID of the remote node (0-63)
  unsigned char getId(void) const { return _id; }
  void setId(unsigned char id);
  unsigned char getFlags(void) const { return _flags; }
  // millis() of the last receive
  unsigned long getLastReceive(void) const { return _lastReceive; }
  // Returns 0 (weakest) to 3 (strongest) signal
  unsigned char getRssi(void) const { return _rssi; }

  // true if last packet had the low battery flag
  boolean isBatteryLow(void) const { return _flags & RFSOURCEFLAG_LowBattery; }
  // true if last packet indicated IT+, that value = degrees C * 10
  boolean isNative(void) const { return _flags & RFSOURCEFLAG_NativeItPlus; }
  boolean isFree(void) const { return _id == RFSOURCEID_NONE; }
  boolean isStale(void) const { return !isFree() && ((millis() - _lastReceive) > RF_STALE_TIME); }

  boolean update(rf12_packet_t *pkt);

  int Value;

private:
  unsigned char _id;
  unsigned long _lastReceive;
  unsigned char _flags;
  unsigned char _rssi;
};

#define RFEVENT_Add    bit(0)
#define RFEVENT_Update bit(1)
#define RFEVENT_Remove bit(2)

class RFManager
{  
public:
  typedef void (*event_callback)(RFSource& source, unsigned char event);

  RFManager(const event_callback fn) :
    _callback(fn), _crcOk(0x80) {};
  
  void init(unsigned char band);
  void freeStaleSources(void);
  unsigned char findFreeSourceIdx(void);
  unsigned char findSourceIdx(unsigned char srcId);
  void status(void);
  boolean doWork(void);
  unsigned long getLastReceive(void) const { return _lastReceive; }
  static unsigned char getAdcBits(void) { return 12; }
  void sendUpdate(unsigned char val);

  RFSource *getSourceById(unsigned char srcId);
  
private:
  boolean _initialized;
  unsigned long _lastReceive;
  const event_callback _callback;
  unsigned char _crcOk;
  RFSource _sources[RF_SOURCE_COUNT];
  unsigned char _txCounter;
};

#endif /* __RFMANAGER_H__ */

