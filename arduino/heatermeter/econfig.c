#include <util/atomic.h>
#include "econfig.h"

uint8_t econfig_read_byte(const void *_src)
{
  do {} while (eeprom_is_busy());

  // Read current value
  EEAR = (uint16_t)_src;
  EECR = _BV(EERE);
  return EEDR;
}

__attribute__((noinline))
  void econfig_read_block(void *_dst, const void *_src, uint8_t n)
{
  const uint8_t *s = (const uint8_t *)_src;
  uint8_t *d = (uint8_t *)_dst;

  while (n--)
    *d++ = econfig_read_byte(s++);
}

uint16_t econfig_read_word(const void *_src)
{
  unsigned int retVal;
  econfig_read_block(&retVal, _src, sizeof(retVal));
  return retVal;
}

void econfig_write_byte(void *_dst, uint8_t val)
{
  // This code assumes econfig_read_byte() will wait for EEPROM idle and set EEAR
  uint8_t curv = econfig_read_byte(_dst);

  // check for differences
  uint8_t difv = val ^ curv;
  if (difv == 0)
    return;

  // Set new value
  EEDR = val;
  ATOMIC_BLOCK(ATOMIC_FORCEON)
  {
    // Any bits need to be erased?
    if (difv & val)
    {
      // Any bits need to be programmed?
      if (val != 0xff)
      {
        // combined erase and write
        EECR = _BV(EEMPE);
        EECR |= _BV(EEPE);
      }
      else
      {
        // erase only (to 0xff)
        EECR = _BV(EEMPE) | _BV(EEPM0);
        EECR |= _BV(EEPE);
      }
    }
    else // no bits need erase
    {
      // Any bits need to be programmed? YES (difv check above)
      // write only
      EECR = _BV(EEMPE) | _BV(EEPM1);
      EECR |= _BV(EEPE);
    }
  }
}

__attribute__((noinline))
  void econfig_write_block(const void *_src, void *_dst, uint8_t n)
{
  const uint8_t *s = (uint8_t *)_src;
  uint8_t *d = (uint8_t *)_dst;

  while (n--)
    econfig_write_byte(d++, *s++);
}

void econfig_write_word(void *_dst, uint16_t val)
{
  uint16_t lval = val;
  econfig_write_block(&lval, _dst, sizeof(lval));
}
