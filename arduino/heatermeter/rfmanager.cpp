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

boolean RFSource::update(rf12_packet_t *pkt)
{
  _lastReceive = millis();

  unsigned char newFlags = 0;
  if ((pkt->byte1 & 0x20) != 0)
    newFlags |= RFSOURCEFLAG_RecentReset;
  if ((pkt->hygro & 0x80) != 0)
    newFlags |= RFSOURCEFLAG_LowBattery;
   /* Hygro value of 6A=NoHygro 7D=Secondary Unit?(TX25U) 7F=lmremote */
  if ((pkt->hygro & 0x7f) < 0x7f)
    newFlags |= RFSOURCEFLAG_NativeItPlus;
  _flags = newFlags;
  _rssi = rf12_rssi();

  if (isNative())
  {
    /* When there is nothing connected it sends AAA for the 3 nibbles */
    /* TX25U secondary: 95 9A AA 7D */
    if (pkt->byte2 == 0xAA)
      return false;
    /* Otherwise the value degrees C in BCD (10x)(1x)(0.1x) with a 40.0 degree offset */
    /* TX25U primary:   95 96 20 6A */
    Value = (((pkt->byte1 & 0x0f) * 100) + ((pkt->byte2 >> 4) * 10) + (pkt->byte2 & 0x0f)) - 400;
    //Debug_begin(); SerialX.print("RF12N "); SerialX.print(Value, DEC); Serial_nl();
  }
  else
  {
    Value = ((pkt->byte1 & 0x0f) << 8) | pkt->byte2;
    //Debug_begin(); SerialX.print("RF12L "); SerialX.print(Value, DEC); Serial_nl();
  }
  return true;
}

void RFManager::init(unsigned char band)
{
  if (!_initialized)
  {
    rf12_initialize(band);
    _initialized = true;
  }
}

void RFManager::freeStaleSources(void)
{
  for (unsigned char idx=0; idx<RF_SOURCE_COUNT; ++idx)
    if (_sources[idx].isStale())
    {
      if (_callback) _callback(_sources[idx], RFEVENT_Remove);
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
  if (!_initialized)
    return;

  // The first item in the list the manager RFSOURCEID_NONE,CrcOk
  print_P(PSTR("HMRF" CSV_DELIMITER "255" CSV_DELIMITER "0" CSV_DELIMITER));
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
    Serial_csv();
    SerialX.print(_sources[idx].getRssi(),DEC);
    //Serial_csv();
    //unsigned int since = (m - _sources[idx].getLastReceive()) / 1000;
    //SerialX.print(since, DEC);
  }
  Serial_nl();
}

void RFManager::sendUpdate(unsigned char val)
{
#if RF_SEND_INTERVAL
  if (!_initialized)
    return;
  if (++_txCounter < RF_SEND_INTERVAL)
    return;
  _txCounter = 0;

#define NODEID_MASTER      0x3F

#define HYGRO_BATTERY_OK   0x00
#define HYGRO_BATTERY_LOW  0x80
#define HYGRO_NO_HYGRO     0x6A
#define HYGRO_SECOND_PROBE 0x7D
#define HYGRO_LMREMOTE_KEY 0x7F

  unsigned char outbuf[4];
  const unsigned char nodeId = NODEID_MASTER;
  const unsigned char _isRecent = 0;

  outbuf[0] = 0x90 | ((nodeId & 0x3f) >> 2);
  outbuf[1] = ((nodeId & 0x3f) << 6) | _isRecent | (val >> 8);
  outbuf[2] = (val & 0xff);
  outbuf[3] = HYGRO_LMREMOTE_KEY | HYGRO_BATTERY_OK;

  rf12_sendStart(outbuf, sizeof(outbuf));
#endif
}

boolean RFManager::doWork(void)
{
  if (!_initialized)
    return false;

  boolean retVal = false;
  while (rf12_recvDone())
  {
    _lastReceive = millis();
    /*
    Debug_begin(); print_P(PSTR("RF in "));
    SerialX.print(rf12_buf[0], HEX); SerialX.print(' ');
    SerialX.print(rf12_buf[1], HEX); SerialX.print(' ');
    SerialX.print(rf12_buf[2], HEX); SerialX.print(' ');
    SerialX.print(rf12_buf[3], HEX); SerialX.print(' ');
    Serial_nl();
    */
    if (rf12_crc == 0)
    {  
      if (_crcOk < 0xff) 
        ++_crcOk;
        
      rf12_packet_t *pkt = (rf12_packet_t *)rf12_buf;

      unsigned char event = RFEVENT_Update;
      unsigned char srcId = ((pkt->byte0 & 0x0f) << 2) | (pkt->byte1 >> 6);
      unsigned char srcIdx = findSourceIdx(srcId);
      if (srcIdx == 0xff)
      {
        srcIdx = findFreeSourceIdx();
        event = RFEVENT_Add | RFEVENT_Update;
      }
      if (srcIdx != 0xff)
      {
        _sources[srcIdx].setId(srcId);
        if (_sources[srcIdx].update(pkt))
          if (_callback) _callback(_sources[srcIdx], event);
      }
    }  /* if crc ok */
    else if (_crcOk > 0)
    {
      //Debug_begin(); print_P(PSTR("RF ERR")); Debug_end();
      --_crcOk;
    }
      
    retVal = true;
  }  /* while recvDone() */
 
  freeStaleSources();
  return retVal;
}


