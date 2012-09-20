// HeaterMeter Copyright 2011 Bryan Mayland <bmayland@capnbry.net> 
#include "strings.h"
#include "rfmanager.h"
#include "hmcore.h"

void RFSource::setId(unsigned char id)
{
  if (_id == id) return;

  _id = id;
  Value = 0;
}

void RFSource::update(rf12_packet_t *pkt)
{
  _lastReceive = millis();

  unsigned char newFlags = 0;
  if ((pkt->byte1 & 0x10) == 0)
    newFlags |= NativeItPlus;
  if ((pkt->byte1 & 0x20) != 0)
    newFlags |= RecentReset;
  if ((pkt->hygro & 0x80) != 0)
    newFlags |= LowBattery;
  if (rf12_rssi() == 0)
    newFlags |= LowSignal;
  _flags = newFlags;

  if (isNative())
    Value = (((pkt->byte1 & 0x0f) * 100) + ((pkt->byte2 >> 4) * 10) + (pkt->byte2 & 0x0f)) - 400;
  else
    Value = ((pkt->byte1 & 0x0f) << 8) | pkt->byte2;
}

void RFManager::init(unsigned char band)
{
  if (!_initialized)
  {
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
      _sources[idx].setId(RFSOURCEID_ANY);
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
  if (!_initialized)
    return;

  // The first item in the list the manager RFSOURCEID_NONE,CrcOk
  print_P(PSTR("HMRF" CSV_DELIMITER "255" CSV_DELIMITER));
  SerialX.print(_crcOk, DEC); // signalish
  //Serial_csv();
  //unsigned long m = millis();
  //SerialX.print((m - getLastReceive()) / 1000, DEC);

  // The rest of the items are Id,Flags
  for (unsigned char idx=0; idx<RF_SOURCE_COUNT; ++idx)
  {
    if (_sources[idx].isFree())
      continue;
    Serial_csv();
    SerialX.print(_sources[idx].getId(),DEC);
    Serial_csv();
    SerialX.print(_sources[idx].getFlags(), DEC);
    //Serial_csv();
    //unsigned int since = (m - _sources[idx].getLastReceive()) / 1000;
    //SerialX.print(since, DEC);
  }
  Serial_nl();
}

boolean RFManager::doWork(void)
{
  if (!_initialized)
    return false;

  boolean retVal = false;
  while (rf12_recvDone())
  {
    _lastReceive = millis();
    //print_P(PSTR("RF in")); Serial_nl();
    if (rf12_crc == 0)
    {  
      if (_crcOk < 0xff) 
        ++_crcOk;
        
      rf12_packet_t *pkt = (rf12_packet_t *)rf12_buf;

      event e = Update;
      unsigned char srcId = ((pkt->byte0 & 0x0f) << 2) | (pkt->byte1 >> 6);
      unsigned char srcIdx = findSourceIdx(srcId);
      if (srcIdx == 0xff)
      {
        srcIdx = findFreeSourceIdx();
        e = static_cast<event>(Add | Update);
      }
      if (srcIdx != 0xff)
      {
        _sources[srcIdx].setId(srcId);
        _sources[srcIdx].update(pkt);
        if (_callback) _callback(_sources[srcIdx], e);
      }
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


