#ifndef __RFMANAGER_H__
#define __RFMANAGER_H__

#include <RF12.h>

#define RF_SOURCE_COUNT 4
// The count of milliseconds with no receive that the source is considered stale
// This should be large enough to allow the remote node to sleep, but short enough
// that the sequence number can't roll without being detected
// i.e. this value should be under MIN_TRANSMIT_PERIOD * 255 * 1000
#define RF_STALE_TIME (5 * 60 * 1000)

class RFSource
{
  unsigned char _id;
  unsigned long _lastReceive;
  unsigned char _nextSeq;
  boolean _lowBattery;
  unsigned char _signalLevel;
  
public:
  RFSource(void) {};
  
  // The ID of the remote node (2-31)
  unsigned char getId(void) const { return _id; };
  void setId(unsigned char id);
  // Signal level (0-255) representing how many packets we've missed
  unsigned char getSignalLevel(void) const { return _signalLevel; };
  // Is the node indicating low battery?
  boolean getLowBattery(void) const { return _lowBattery; };
  // millis() of the last receive
  unsigned long getLastReceive(void) const { return _lastReceive; };

  boolean isFree(void) const { return _id == 0; };
  void doFree(void) { _id = 0; };
  boolean isStale(void) const { return millis() - _lastReceive > RF_STALE_TIME; };
  
  void update(struct __rfm12_probe_update_hdr *hdr, unsigned char len);
};

class RFManager
{
private:
  const char _rxLed;
  unsigned char _crcOk;
  RFSource _sources[RF_SOURCE_COUNT];
  
public:
  RFManager(char rxLed);
  
  void init(unsigned char band);
  void freeStaleSources(void);
  char findFreeSourceIdx(void);
  char findSourceIdx(unsigned char srcId);
  void status(void);
  void doWork(void);
};

#endif /* __RFMANAGER_H__ */

