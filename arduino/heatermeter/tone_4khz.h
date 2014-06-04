#ifndef __TONE4KHZ_H__
#define __TONE4KHZ_H__

#define PIN_ALARM 6

void tone4khz_init(void);
void tone4khz_begin(unsigned char pin, unsigned char dur);
void tone4khz_end(void);

#endif // __TONE4KHZ_H__
