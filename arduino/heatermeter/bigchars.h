/*
  BigChar font by Michael Pilcher (digimike)
  http://arduino.cc/forum/index.php/topic,8882.15.html
*/
#ifndef __BIGCHARS_H__
#define __BIGCHARS_H__
const prog_char BIG_CHAR_PARTS[] PROGMEM = {
  // LT
  0b00111,
  0b01111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  // CT
  0b11111,
  0b11111,
  0b11111,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  // RT
  0b11100,
  0b11110,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  // LB
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b01111,
  0b00111,
  // CB
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b11111,
  0b11111,
  0b11111,
  // RB
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11110,
  0b11100,
  // UMB
  0b11111,
  0b11111,
  0b11111,
  0b00000,
  0b00000,
  0b00000,
  0b11111,
  0b11111,
  // BMB
  0b11111,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b11111,
  0b11111,
  0b11111,
};

#define C_BLK  32 
#define C_LT   0
#define C_CT   1
#define C_RT   2
#define C_LB   3
#define C_CB   4
#define C_RB   5
#define C_UMB  6
#define C_BMB  7

#define C_WIDTH 3

const prog_char NUMS[] PROGMEM = {
  C_LT, C_CT, C_RT,        C_LB, C_CB, C_RB, //0
  C_CT, C_RT, C_BLK,       C_BLK, C_RB, C_BLK, //1
  C_UMB, C_UMB, C_RT,      C_LB, C_BMB, C_BMB, //2
  C_UMB, C_UMB, C_RT,      C_BMB, C_BMB, C_RB, //3
  C_LB, C_CB, C_RT,        C_BLK, C_BLK, C_RB, //4
  255, C_UMB, C_UMB,       C_BMB, C_BMB, C_RB, //5
  C_LT, C_UMB, C_UMB,      C_LB, C_BMB, C_RB, //6
  C_CT, C_CT, C_RT,        C_BLK, C_LT, C_BLK, //7
  C_LT, C_UMB, C_RT,       C_LB, C_BMB, C_RB, //8
  C_LT, C_UMB, C_RT,       C_BLK, C_BLK, C_RB, //9
};
#endif /* __BIGCHARS_H__ */
