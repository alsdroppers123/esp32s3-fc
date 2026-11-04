#include "config.h"
#include <Preferences.h>

portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
volatile uint8_t  g_cmdCalGyro = 0, g_cmdMagCal = 0, g_cmdDisarm = 0;
volatile bool     g_mtEnable = false, g_busy = false;
volatile uint16_t g_mtVal[4] = {0, 0, 0, 0};
volatile uint32_t g_mtStamp = 0;

static FcConfig  s_shadow;
static volatile bool s_dirty = false;
static Telemetry s_tel;

void cfgDefaults(FcConfig& c) {
  memset(&c, 0, sizeof c);
  c.magic = CFG_MAGIC;
  // Conservative placeholders - NOT a tune for your airframe.
  const float pid[3][4] = {{35, 60, 22, 60}, {38, 64, 24, 60}, {40, 70, 0, 60}};
  memcpy(c.pid, pid, sizeof pid);
  for (int i = 0; i < 3; i++) { c.rcRate[i] = 1.0f; c.superRate[i] = 0.70f; c.expo[i] = 0.0f; }
  c.gyroLpfHz = 150; c.dtermLpfHz = 80; c.rcSmoothHz = 50;
  c.angleStrength = 5.0f; c.angleMax = 45; c.iTermLimit = 300; c.idlePct = 5.5f; c.mahonyKp = 1.0f;
  c.airmode = 0; c.magFusion = 0; c.protocol = PROTO_DSHOT300;
}

static float cl(float v, float lo, float hi) { if (!(v >= lo)) return lo; return v > hi ? hi : v; }

int cfgToArray(const FcConfig& c, float* a) {
  int n = 0;
  for (int r = 0; r < 3; r++) for (int k = 0; k < 4; k++) a[n++] = c.pid[r][k];   // 0..11
  for (int i = 0; i < 3; i++) a[n++] = c.rcRate[i];                                // 12..14
  for (int i = 0; i < 3; i++) a[n++] = c.superRate[i];                             // 15..17
  for (int i = 0; i < 3; i++) a[n++] = c.expo[i];                                  // 18..20
  a[n++] = c.gyroLpfHz;  a[n++] = c.dtermLpfHz;  a[n++] = c.rcSmoothHz;            // 21..23
  a[n++] = c.angleStrength; a[n++] = c.angleMax; a[n++] = c.iTermLimit;            // 24..26
  a[n++] = c.idlePct; a[n++] = c.airmode; a[n++] = c.mahonyKp;                     // 27..29
  a[n++] = c.magFusion; a[n++] = c.protocol;                                       // 30..31
  return n;
}

void cfgFromArray(FcConfig& c, const float* a) {
  int n = 0;
  for (int r = 0; r < 3; r++) for (int k = 0; k < 4; k++) c.pid[r][k] = cl(a[n++], 0, 250);
  for (int i = 0; i < 3; i++) c.rcRate[i]    = cl(a[n++], 0.1f, 3.0f);
  for (int i = 0; i < 3; i++) c.superRate[i] = cl(a[n++], 0.0f, 0.95f);
  for (int i = 0; i < 3; i++) c.expo[i]      = cl(a[n++], 0.0f, 1.0f);
  c.gyroLpfHz = cl(a[n++], 20, 400);  c.dtermLpfHz = cl(a[n++], 20, 300);  c.rcSmoothHz = cl(a[n++], 10, 150);
  c.angleStrength = cl(a[n++], 1, 12); c.angleMax = cl(a[n++], 10, 60); c.iTermLimit = cl(a[n++], 50, 500);
  c.idlePct = cl(a[n++], 0, 15); c.airmode = a[n++] > 0.5f; c.mahonyKp = cl(a[n++], 0.1f, 5);
  c.magFusion = a[n++] > 0.5f; c.protocol = (uint8_t)cl(a[n++], 0, 2);
}

void cfgLoad(FcConfig& c) {
  Preferences p;
  bool ok = false;
  if (p.begin("fc", true)) {
    if (p.getBytesLength("cfg") == sizeof(FcConfig)) {
      p.getBytes("cfg", &c, sizeof c);
      ok = (c.magic == CFG_MAGIC);
    }
    p.end();
  }
  if (!ok) cfgDefaults(c);
}

bool cfgSave(const FcConfig& c) {
  Preferences p;
  if (!p.begin("fc", false)) return false;
  bool ok = p.putBytes("cfg", &c, sizeof c) == sizeof c;
  p.end();
  return ok;
}

void cfgPublish(const FcConfig& c) {
  portENTER_CRITICAL(&g_mux); s_shadow = c; s_dirty = true; portEXIT_CRITICAL(&g_mux);
}
bool cfgConsume(FcConfig& out) {
  if (!s_dirty) return false;
  portENTER_CRITICAL(&g_mux); out = s_shadow; s_dirty = false; portEXIT_CRITICAL(&g_mux);
  return true;
}
void cfgSnapshot(FcConfig& out) { portENTER_CRITICAL(&g_mux); out = s_shadow; portEXIT_CRITICAL(&g_mux); }

void telPublish(const Telemetry& t) { portENTER_CRITICAL(&g_mux); s_tel = t; portEXIT_CRITICAL(&g_mux); }
void telGet(Telemetry& t)           { portENTER_CRITICAL(&g_mux); t = s_tel; portEXIT_CRITICAL(&g_mux); }
