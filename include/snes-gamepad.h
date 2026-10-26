#ifndef SNES_GAMEPAD_H
#define SNES_GAMEPAD_H

#include <stdint.h>
#include <stdbool.h>

enum {
  SnesGamepad_Report_XAxis = 0,
  SnesGamepad_Report_YAxis = 1,
  SnesGamepad_Report_ABXY = 5,
  SnesGamepad_Report_LR = 6,
  SnesGamepad_Report_StartSelect = 6,

  SnesGamepad_Mask_A = 0b0010 << 4,
  SnesGamepad_Mask_B = 0b0100 << 4,
  SnesGamepad_Mask_X = 0b0001 << 4,
  SnesGamepad_Mask_Y = 0b1000 << 4,
  SnesGamepad_Mask_L = 0b0001,
  SnesGamepad_Mask_R = 0b0010,
  SnesGamepad_Mask_Start = 0b0010 << 4,
  SnesGamepad_Mask_Select = 0b0001 << 4,
};

typedef union {
  struct {
    bool Left : 1;
    bool Right : 1;
    bool Up : 1;
    bool Down : 1;
    bool A : 1;
    bool B : 1;
    bool X : 1;
    bool Y : 1;
    bool L : 1;
    bool R : 1;
    bool Start : 1;
    bool Select : 1;
  };
  uint16_t word;
} SnesGamepad_StatusWord_t;

#endif // SNES_GAMEPAD_H