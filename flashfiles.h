#ifndef __FLASHFILES_H__
#define __FLASHFILES_H__

#define DATAFLASH_PAGE_BYTES 528

const char FNAME000[] PROGMEM = "favicon.ico";
const char FNAME001[] PROGMEM = "index.html";

const struct flash_file_t {
  const char *fname;
  const unsigned int page;
  const unsigned int size;
} FLASHFILES[] = {
  { FNAME000,   0, 1150 },
  { FNAME001,   3, 807 },
  { 0, 0, 0},
};

#endif /* __FLASHFILES_H__ */

