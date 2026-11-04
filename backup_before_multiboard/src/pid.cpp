#include "pid.h"

// Betaflight scaling so that the familiar P/I/D numbers behave similarly
#define PTERM_SCALE 0.032029f
#define ITERM_SCALE 0.244381f
#define DTERM_SCALE 0.000529f
#define FF_SCALE    0.0015f          // own scaling: FF 100 ~ 0.15 output per 1000 deg/s^2 of setpoint change

static Biquad s_gyroLpf[3];
static Pt1    s_dLpf[3], s_spLpf[3];
static float  s_iterm[3], s_prevGyro[3], s_prevSp[3];
static bool   s_first = true;

float rcApplyRates(float s, float rcRate, float superRate, float expo) {
  float a = fabsf(s);
  if (expo > 0) s = s * a * a * a * expo + s * (1.0f - expo);
  float rr = rcRate;
  if (rr > 2.0f) rr += 14.54f * (rr - 2.0f);
  float rate = 200.0f * rr * s;
  if (superRate > 0) { float d = 1.0f - a * superRate; if (d < 0.01f) d = 0.01f; rate /= d; }
  return constrain(rate, -1998.0f, 1998.0f);
}

void pidConfigure(const FcConfig& c) {
  const float dt = 1.0f / LOOP_HZ;
  for (int a = 0; a < 3; a++) {
    s_gyroLpf[a].setLpf(c.gyroLpfHz, LOOP_HZ);
    s_dLpf[a].setCutoff(c.dtermLpfHz, dt);
    s_spLpf[a].setCutoff(c.rcSmoothHz, dt);
  }
}

void pidReset() {
  for (int a = 0; a < 3; a++) { s_iterm[a] = 0; s_dLpf[a].y = 0; }
  s_first = true;
}

void pidUpdate(const FcConfig& c, float dt, const float spRaw[3], const float gyroRaw[3],
               bool iActive, bool sat, float out[3], float gyroFilt[3]) {
  for (int a = 0; a < 3; a++) {
    const float* K = c.pid[a];
    float g  = s_gyroLpf[a].apply(gyroRaw[a]);
    float sp = s_spLpf[a].apply(spRaw[a]);                // RC smoothing; also makes d(sp)/dt usable for FF
    if (s_first) { s_prevGyro[a] = g; s_prevSp[a] = sp; s_spLpf[a].y = sp; }
    gyroFilt[a] = g;

    float err = sp - g;
    float P = K[0] * PTERM_SCALE * err;

    if (iActive) {                                         // anti-windup: clamp + conditional integration
      float inc = K[1] * ITERM_SCALE * err * dt;
      if (sat && ((inc > 0 && s_iterm[a] > 0) || (inc < 0 && s_iterm[a] < 0))) inc = 0;
      s_iterm[a] = constrain(s_iterm[a] + inc, -c.iTermLimit, c.iTermLimit);
    } else {
      s_iterm[a] = 0;
    }

    float D = 0;                                           // D on measurement, then PT1 smoothing
    if (K[2] > 0) D = s_dLpf[a].apply(K[2] * DTERM_SCALE * (-(g - s_prevGyro[a]) / dt));
    float F = K[3] * FF_SCALE * (sp - s_prevSp[a]) / dt;   // feed-forward from setpoint derivative

    s_prevGyro[a] = g; s_prevSp[a] = sp;
    float lim = (a == 2) ? 400.0f : 500.0f;
    out[a] = constrain(P + s_iterm[a] + D + F, -lim, lim) * 0.001f;
  }
  s_first = false;
}
