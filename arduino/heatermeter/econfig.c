#include <avr/eeprom.h>

__attribute__((noinline))
  void econfig_read_block(void *_dst, const void *_src, uint8_t n)
{
  const uint8_t *s = (const uint8_t *)_src;
  uint8_t *d = (uint8_t *)_dst;

  while (n--)
    *d++ = eeprom_read_byte(s++);
}

uint16_t econfig_read_word(const void *_src)
{
  unsigned int retVal;
  econfig_read_block(&retVal, _src, sizeof(retVal));
  return retVal;
}

__attribute__((noinline))
  void econfig_write_block(const void *_src, void *_dst, uint8_t n)
{
  const uint8_t *s = (uint8_t *)_src;
  uint8_t *d = (uint8_t *)_dst;

  while (n--)
    eeprom_write_byte(d++, *s++);
}

void econfig_write_word(void *_dst, uint16_t val)
{
  uint16_t lval = val;
  econfig_write_block(&lval, _dst, sizeof(lval));
}

void econfig_write_byte(void *_dst, uint8_t val)
{
  uint8_t lval = val;
  econfig_write_block(&lval, _dst, sizeof(lval));
}
