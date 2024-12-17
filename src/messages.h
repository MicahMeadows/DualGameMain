#ifndef MESSAGES_H
#define MESSAGES_H
#include <Arduino.h>

// messages must include the first field as "type" which should be an uint8_t to it can be used to identify the message type 

typedef struct shoot_message {
  uint8_t type = 1;
  bool isRed;
} shoot_message;

// Buttons Ids:
// 0 - Trigger
// 1 - Hammer
// 2 - Safety

typedef struct btn_press_message {
  uint8_t type = 2;
  uint8_t button;
  bool value;
} btn_press_message;

#endif