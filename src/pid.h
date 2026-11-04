// pid.h - Betaflight-style rate controller, filters and rates
#pragma once
#include "config.h"

struct Pt1 {
  float k = 1, y = 0;
  void setCutoff(float fc, float dt) { if (fc <= 0) { k = 1; return; } float rc = 1.0f / (2.0f * PI * fc); k = dt / (rc + dt); }
  float apply(float x) { y += k * (x - y); return y; }
};

struct Biquad {                       // 2nd-order Butterworth low-pass, direct form II transposed
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
  void setLpf(float fc, float fs) {
    if (fc <= 0 || fc >= fs * 0.45f) { b0 = 1; b1 = b2 = a1 = a2 = 0; return; }
    float w = 2.0f * PI * fc / fs, sn = sinf(w), cs = cosf(w), al = sn / (2.0f * 0.70710678f);
    float a0 = 1.0f + al;
    b0 = (1.0f - cs) * 0.5f / a0; b1 = (1.0f - cs) / a0; b2 = b0; a1 = -2.0f * cs / a0; a2 = (1.0f - al) / a0;
  }
  float apply(float x) { float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

float rcApplyRates(float stick, float rcRate, float superRate, float expo);   // Betaflight "BETAFLIGHT" rates, deg/s
void  pidConfigure(const FcConfig& c);        // recompute filter coefficients (state kept)
void  pidReset();                              // clear I / D / feed-forward history
// spRaw/gyroRaw in deg/s (roll,pitch,yaw). out = normalised mixer input (+-1 ~ full range). gyroFilt = filtered gyro.
void  pidUpdate(const FcConfig& c, float dt, const float spRaw[3], const float gyroRaw[3],
                bool iActive, bool mixerSaturated, float out[3], float gyroFilt[3]);
