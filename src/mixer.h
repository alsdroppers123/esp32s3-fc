// mixer.h - Quad-X mixer + motor output (DShot via RMT; 1000-2000 us PWM or plain 0-100 % duty PWM via LEDC)
#pragma once
#include "config.h"

void motorsInit(uint8_t protocol);
// throttle 0..1, pid = normalised roll/pitch/yaw. out[i] = 0..1 motor command (idle..1).
// Returns true if the mixer had to desaturate (used for I-term anti-windup).
bool mixerRun(float throttle, const float pid[3], bool airmode, float idlePct, float out[4]);
void motorsWrite(const float v[4]);     // v <= 0 -> motor stopped, otherwise 0..1 of the throttle range
void motorsStop();
void motorsSetRange(float minPct, float capPct);   // start point (0..90 %: where the motor begins to turn) and ceiling (20..100 %), applied to every output incl. motor test
