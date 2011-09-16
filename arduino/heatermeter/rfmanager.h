// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#ifndef __RFMANAGER_H__
#define __RFMANAGER_H__

#include <RF12.h>

#define RFSOURCEID_NONE 0

#define RF_PINS_PER_SOURCE 6
#define RF_SOURCE_COUNT 4

// The count of milliseconds with no receive that the source is considered stale
// This should be large enough to allow the remote node to sleep, but short enough
// that the sequence number can't roll without being detected
// i.e. this value should be under MIN_TRANSMIT_PERIOD * 255 * 1000
#define RF_STALE_TIME (3 * 60 * 1000UL)

typedef struct tagRf12MapItem 
{
  unsigned char pin: 3;
  unsigned char source: 5;
} rf12_map_item_t;

typedef struct tagRf12ProbeUpdateHdr 
{
  unsigned char seqNo;
  unsigned int batteryLevel;
} rf12_probe_update_hdr_t;

typedef struct tagRf12ProbeUpdate 
{
  unsigned char probeIdx: 6;
  unsigned int adcValue: 10;
} rf12_probe_update_t;

class RFSource
{
  unsigned char _id;
  unsigned long _lastReceive;
  unsigned char _nextSeq;
  unsigned int _batteryLevel;
  unsigned char _signalLevel;
  
public:
  RFSource(void) {};
  
  // The ID of the remote node (2-31)
  unsigned char getId(void) const { return _id; };
  void setId(unsigned char id);
  // Signal level (0-255) representing how many packets we've missed
  unsigned char getSignalLevel(void) const { return _signalLevel; };
  // Is the node indicating low battery?
  unsigned int getBatteryLevel(void) const { return _batteryLevel; };
  // millis() of the last receive
  unsigned long getLastReceive(void) const { return _lastReceive; };

  boolean isFree(void) const { return _id == RFSOURCEID_NONE; };
  boolean isStale(void) const { return !isFree() && ((millis() - _lastReceive) > RF_STALE_TIME); };
  
  void update(rf12_probe_update_hdr_t *hdr, unsigned char len);

  unsigned int Values[RF_PINS_PER_SOURCE];
};

class RFManager
{  
public:
  enum event { Add = 0x01, Remove = 0x02, Update = 0x04 };
  typedef void (*event_callback)(RFSource&, event);

  RFManager(const event_callback fn) :
    _crcOk(0xff), _callback(fn) {};
  
  void init(unsigned char band);
  void freeStaleSources(void);
  char findFreeSourceIdx(void);
  char findSourceIdx(unsigned char srcId);
  void status(void);
  boolean doWork(void);
  
  RFSource *getSourceById(unsigned char srcId);
  
private:
  boolean _initialized;
  const event_callback _callback;
  unsigned char _crcOk;
  RFSource _sources[RF_SOURCE_COUNT];
};

#endif /* __RFMANAGER_H__ */

