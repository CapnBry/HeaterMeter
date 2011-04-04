#ifndef __FLASHFILES_H__
#define __FLASHFILES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <avr\pgmspace.h>

#define DATAFLASH_PAGE_BYTES 528

const char FNAME000[] PROGMEM = "favicon.ico";
const char FNAME001[] PROGMEM = "fire.png";
const char FNAME002[] PROGMEM = "";

const struct flash_file_t {
  const char *fname;
  const unsigned int page;
  const unsigned int size;
} FLASHFILES[] PROGMEM = {
  { FNAME000,   3, 1150 },
  { FNAME001,   6, 11824 },
  { FNAME002,  29, 5073 },
  { 0, 0, 0},
};

#ifdef __cplusplus
}
#endif

#endif /* __FLASHFILES_H__ */

