#include "mixer.h"
#include "driver/rmt.h"

// Axis convention: +roll = right side down, +pitch = nose up, +yaw = clockwise from above.
// Props-in layout (Betaflight default): M1 RR CCW, M2 FR CW, M3 RL CW, M4 FL CCW.
// A CCW prop pushes the airframe clockwise, so +yaw speeds up the CCW motors (M1, M4).
static const float MIX[4][3] = {
  {-1, -1, +1},   // M1 rear right
  {-1, +1, -1},   // M2 front right
  {+1, -1, -1},   // M3 rear left
  {+1, +1, +1},   // M4 front left
};

bool mixerRun(float thr, const float pid[3], bool airmode, float idlePct, float out[4]) {
  const float idle = idlePct * 0.01f;
  if (thr < 0.02f && !airmode) { for (int i = 0; i < 4; i++) out[i] = idle; return false; }

  float att = (!airmode && thr < 0.10f) ? thr * 10.0f : 1.0f;   // fade authority in at low throttle
  float mix[4], mn = 0, mx = 0;
  for (int i = 0; i < 4; i++) {
    mix[i] = att * (MIX[i][0] * pid[0] + MIX[i][1] * pid[1] + MIX[i][2] * pid[2]);
    if (i == 0 || mix[i] < mn) mn = mix[i];
    if (i == 0 || mix[i] > mx) mx = mix[i];
  }
  bool sat = false;
  float range = mx - mn;
  if (range > 1.0f) { float s = 1.0f / range; for (int i = 0; i < 4; i++) mix[i] *= s; mn *= s; mx *= s; sat = true; }
  float t = constrain(thr, -mn, 1.0f - mx);                     // shift throttle so nothing clips (airmode behaviour)
  if (t != thr) sat = true;
  for (int i = 0; i < 4; i++) out[i] = idle + constrain(t + mix[i], 0.0f, 1.0f) * (1.0f - idle);
  return sat;
}

// ---------------------------------------------------------------- outputs
static uint8_t s_proto = PROTO_DSHOT300;
static rmt_item32_t s_items[4][17];
static uint16_t T_BIT, T1H, T0H;                  // RMT ticks @ 80 MHz (12.5 ns)

void motorsInit(uint8_t proto) {
  s_proto = proto;
  if (proto == PROTO_PWM) {
    for (int i = 0; i < 4; i++) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
      ledcAttachChannel(MOTOR_PINS[i], 400, 13, i);
#else
      ledcSetup(i, 400, 13); ledcAttachPin(MOTOR_PINS[i], i);
#endif
    }
  } else {
    if (proto == PROTO_DSHOT150) { T_BIT = 533; T1H = 400; T0H = 200; }
    else                         { T_BIT = 267; T1H = 200; T0H = 100; }   // DShot300
    for (int i = 0; i < 4; i++) {
      rmt_config_t c = RMT_DEFAULT_CONFIG_TX((gpio_num_t)MOTOR_PINS[i], (rmt_channel_t)i);
      c.clk_div = 1; c.mem_block_num = 1;
      c.tx_config.idle_output_en = true; c.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
      rmt_config(&c);
      rmt_driver_install(c.channel, 0, 0);
    }
  }
  motorsStop();
}

static void dshotSend(int i, uint16_t value) {
  uint16_t p = value << 1;                                      // telemetry bit = 0
  uint16_t frame = (p << 4) | ((p ^ (p >> 4) ^ (p >> 8)) & 0x0F);
  for (int b = 0; b < 16; b++) {
    bool bit = (frame >> (15 - b)) & 1;
    rmt_item32_t it; it.level0 = 1; it.duration0 = bit ? T1H : T0H; it.level1 = 0; it.duration1 = T_BIT - it.duration0;
    s_items[i][b] = it;
  }
  s_items[i][16].val = 0;                                       // end marker
  rmt_write_items((rmt_channel_t)i, s_items[i], 17, false);
}

void motorsWrite(const float v[4]) {
  for (int i = 0; i < 4; i++) {
    float x = v[i];
    if (s_proto == PROTO_PWM) {
      float us = (x <= 0) ? 1000.0f : 1000.0f + constrain(x, 0.0f, 1.0f) * 1000.0f;
      uint32_t duty = (uint32_t)(us * 3.2768f);                 // 400 Hz, 13 bit: 8192 / 2500 us
#if ESP_ARDUINO_VERSION_MAJOR >= 3
      ledcWrite(MOTOR_PINS[i], duty);
#else
      ledcWrite(i, duty);
#endif
    } else {
      uint16_t d = (x <= 0) ? 0 : (uint16_t)(48 + constrain(x, 0.0f, 1.0f) * 1999.0f);   // 48..2047, 0 = stop
      dshotSend(i, d);
    }
  }
}

void motorsStop() { const float z[4] = {0, 0, 0, 0}; motorsWrite(z); }
