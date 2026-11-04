// rc.h - CRSF/ELRS on UART1, web joystick (WebSocket) and UDP virtual joystick. Priority: CRSF > web > UDP
#pragma once
#include "config.h"

void rcInit();                                     // call after Wi-Fi is up (UDP listener)
// Copies the 8 channels (us). Returns false if no source delivered data within RC_TIMEOUT_MS.
bool rcGet(uint16_t out[8], uint8_t* source);
void rcWebUpdate(const uint16_t ch[8]);           // web joystick frame (WebSocket "J ...")
void rcWebDrop();                                  // the joystick client disconnected -> stale immediately
