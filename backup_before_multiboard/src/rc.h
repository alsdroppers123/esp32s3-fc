// rc.h - CRSF/ELRS on UART1 with Wi-Fi UDP virtual-joystick fallback
#pragma once
#include "config.h"

void rcInit();                                     // call after Wi-Fi is up (UDP listener)
// Copies the 8 channels (us). Returns false if no source delivered data within RC_TIMEOUT_MS.
bool rcGet(uint16_t out[8], uint8_t* source);
