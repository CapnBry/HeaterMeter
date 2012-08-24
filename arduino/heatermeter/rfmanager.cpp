// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#include "Arduino.h"
#include "strings.h"
#include "rfmanager.h"
#include "hmcore.h"

//#define RFMANAGER_DEBUG

void RFSource::setId(unsigned char id)
{
  if (_id == id) return;

  _id = id;
  _lastReceive = 0;
  _batteryLevel = 0;
  _signalLevel = 0xff;
  //_nextSeq = 0;
  memset(Values, 0, sizeof(Values));
}

void RFSource::update(rf12_probe_update_hdr_t *hdr, unsigned char len)
{
  _batteryLevel = hdr->batteryLevel;
  _adcBits = hdr->adcBits;
#ifdef RFMANAGER_DEBUG
  Debug_begin(); print_P(PSTR("RFM"));
  SerialX.print(hdr->seqNo, DEC); Serial_char(' ');
  SerialX.print(_adcBits, DEC); Serial_char(' ');
  SerialX.print(_batteryLevel, DEC);
#endif
  if (_lastReceive != 0)
  {
    // _signalLevel is just a bitfield that shifts in a 1 for every packet
    // we get in sequence, and shifts out that 1 for every packet we miss
    // e.g. 01111111 indicates 7 of the past 8 packets received
    unsigned char seqDiff = hdr->seqNo - _nextSeq;
    if (seqDiff == 0)
    {
      _signalLevel = (_signalLevel << 1) | 1;
    }
    else 
    {
      _signalLevel = _signalLevel >> seqDiff;
    } 
  }  /* if have received before */
  
  _nextSeq = hdr->seqNo + 1;
  _lastReceive = millis();
  
  rf12_probe_update_t *probe = (rf12_probe_update_t *)&hdr[1];
  len -= sizeof(rf12_probe_update_hdr_t);
  while (len >= sizeof(rf12_probe_update_t))
  {
    Values[probe->probeIdx] = probe->adcValue;
    len -= sizeof(rf12_probe_update_t);
#ifdef RFMANAGER_DEBUG
    Serial_char(' '); SerialX.print(probe->probeIdx, DEC); Serial_char(' '); SerialX.print(probe->adcValue, DEC);
#endif
    ++probe;
  }  /* while len */
#ifdef RFMANAGER_DEBUG
  Debug_end();
#endif
}

void RFManager::init(unsigned char band)
{
  if (!_initialized)
  {
    // The master is always node 1
    rf12_initialize(1, band);
    _initialized = true;
  }
}

void RFManager::freeStaleSources(void)
{
  for (unsigned char idx=0; idx<RF_SOURCE_COUNT; ++idx)
    if (_sources[idx].isStale())
    {
      if (_callback) _callback(_sources[idx], Remove);
      _sources[idx].setId(RFSOURCEID_NONE);
    }
}

unsigned char RFManager::findFreeSourceIdx(void)
{
  for (unsigned char idx=0; idx<RF_SOURCE_COUNT; ++idx)
    if (_sources[idx].isFree())
      return idx;
  return 0xff;
}

unsigned char RFManager::findSourceIdx(unsigned char srcId)
{
  // Get the index of the srcId, returns 0xff if it doesn't exist
  for (unsigned char idx=0; idx<RF_SOURCE_COUNT; ++idx)
    if (_sources[idx].getId() == srcId)
      return idx;
  return 0xff;
}

RFSource *RFManager::getSourceById(unsigned char srcId)
{
  unsigned char idx = findSourceIdx(srcId);
  return (idx != 0xff) ? &_sources[idx] : NULL;
}

void RFManager::status(void)
{
  unsigned long m = millis();

  // The first item in the list the manager but it has the same format as 
  // the other sources, which is: Id,Signal,TimeSinceLastReceive
  print_P(PSTR("A" CSV_DELIMITER "3300" CSV_DELIMITER));
  SerialX.print(_crcOk, DEC); // signal
  Serial_csv();
  SerialX.print((m - getLastReceive()) / 1000, DEC);

  for (unsigned char idx=0; idx<RF_SOURCE_COUNT; ++idx)
  {
    if (_sources[idx].isFree())
      continue;
    Serial_csv();
    Serial_char('A' + _sources[idx].getId() - 1);
    Serial_csv();
    SerialX.print(_sources[idx].getBatteryLevel(), DEC);
    Serial_csv();
    SerialX.print(_sources[idx].getSignalLevel(), DEC);
    Serial_csv();
    
    unsigned int since = (m - _sources[idx].getLastReceive()) / 1000;
    SerialX.print(since, DEC);
  }
}

boolean RFManager::doWork(void)
{
  boolean retVal = false;
  while (rf12_recvDone())
  {
    _lastReceive = millis();
    //print_P(PSTR("RF in")); Serial_nl();
    if ((rf12_crc == 0) && (rf12_len >= sizeof(rf12_probe_update_hdr_t)))
    {  
      if (_crcOk < 0xff) 
        ++_crcOk;
        
      // If this is a broadcast it should be a probe update
      if ((rf12_hdr & RF12_HDR_DST) == 0)
      {
        rf12_probe_update_hdr_t *hdr = (rf12_probe_update_hdr_t *)rf12_data;

        event e = Update;
        unsigned char srcId = rf12_hdr & RF12_HDR_MASK;
        unsigned char src = findSourceIdx(srcId);
        if (src == 0xff)
        {
          src = findFreeSourceIdx();
          e = static_cast<event>(Add | Update);
        }
        if (src != 0xff)
        {
          _sources[src].setId(srcId);
          _sources[src].update(hdr, rf12_len);
          if (_callback) _callback(_sources[src], e);
        }
      }  /* if broadcast */
    }  /* if crc ok */
    else if (_crcOk > 0)
    {
      //print_P(PSTR("RF ERR")); Serial_nl();
      --_crcOk;
    }
      
    retVal = true;
  }  /* while recvDone() */
 
  freeStaleSources();
  return retVal;
}


