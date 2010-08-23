#ifndef __FLASHFILES_H__
#define __FLASHFILES_H__

#define DATAFLASH_PAGE_BYTES 528

const char FNAME000[] PROGMEM = "favicon.ico";
const char FNAME001[] PROGMEM = "fire.png";
const char FNAME002[] PROGMEM = "";

const struct flash_file {
  const char *fname;
  const unsigned int page;
  const unsigned int size;
} FLASHFILES[] PROGMEM = {
  { FNAME000,   3, 1625 },
  { FNAME001,   7, 11824 },
  { FNAME002,  30, 4218 },
  { 0, 0, 0},
};

#endif /* __FLASHFILES_H__ */

