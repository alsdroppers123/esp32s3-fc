#include "rc.h"
#include <AsyncUDP.h>

struct RcSrc { uint16_t ch[8]; uint32_t stamp; bool seen; };
static RcSrc s_crsf, s_udp, s_web;          // s_crsf = the hardware receiver (CRSF or PPM)
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static AsyncUDP s_asyncUdp;

// Receiver channel order -> internal AETR. ORDER[o][k] = index in the incoming frame of internal channel k.
static const uint8_t ORDER[CHO_COUNT][4] = {
  {0, 1, 2, 3},   // AETR  (ELRS, FrSky, Futaba)
  {1, 2, 0, 3},   // TAER  (Spektrum, JR, many PPM receivers)
  {0, 1, 3, 2},   // AERT
  {2, 0, 1, 3},   // ETAR
};
static void reorder(uint16_t* ch) {
  if (g_hw.chOrder == CHO_AETR || g_hw.chOrder >= CHO_COUNT) return;
  uint16_t in[4]; memcpy(in, ch, sizeof in);
  for (int k = 0; k < 4; k++) ch[k] = in[ORDER[g_hw.chOrder][k]];
}

static void store(RcSrc& s, const uint16_t* ch) {
  portENTER_CRITICAL(&s_mux);
  memcpy(s.ch, ch, 16); s.stamp = millis(); s.seen = true;
  portEXIT_CRITICAL(&s_mux);
}

static uint8_t crc8(const uint8_t* p, uint8_t n) {
  uint8_t c = 0;
  while (n--) { c ^= *p++; for (int i = 0; i < 8; i++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0xD5) : (uint8_t)(c << 1); }
  return c;
}

static void crsfFrame(const uint8_t* f) {          // f[0]=addr f[1]=len f[2]=type f[3..]=payload
  if (f[2] != 0x16 || f[1] != 24) return;          // RC_CHANNELS_PACKED only
  uint16_t ch[8]; uint32_t bits = 0; int nb = 0, n = 0;
  for (int i = 0; i < 11 && n < 8; i++) {
    bits |= (uint32_t)f[3 + i] << nb; nb += 8;
    while (nb >= 11 && n < 8) { uint16_t v = bits & 0x7FF; bits >>= 11; nb -= 11;
      int us = 1500 + ((int)v - 992) * 5 / 8; ch[n++] = (uint16_t)constrain(us, 988, 2012); }
  }
  reorder(ch);
  store(s_crsf, ch);
}

// ---------------------------------------------------------------- PPM (CPPM / PPM-sum)
// One pin, up to 12 channels. Each channel = time between two rising edges (750..2250 us);
// a gap > 2.7 ms marks the frame start. Measuring edge-to-edge of the same polarity makes it
// work with both normal and inverted PPM. The ISR runs on Core 0 (attached from ppmTask).
#define PPM_MAX_CH 12
static volatile uint32_t s_ppmLast = 0;
static volatile uint16_t s_ppmBuf[PPM_MAX_CH], s_ppmFrame[PPM_MAX_CH];
static volatile uint8_t  s_ppmIdx = 0xFF, s_ppmN = 0;
static volatile bool     s_ppmNew = false;
static volatile uint32_t s_ppmFrames = 0;

static void IRAM_ATTR ppmIsr() {
  const uint32_t now = micros(), d = now - s_ppmLast;
  s_ppmLast = now;
  if (d > 2700) {                                     // sync gap
    if (s_ppmIdx != 0xFF && s_ppmIdx >= 4) {
      for (uint8_t i = 0; i < s_ppmIdx; i++) s_ppmFrame[i] = s_ppmBuf[i];
      s_ppmN = s_ppmIdx; s_ppmNew = true; s_ppmFrames++;
    }
    s_ppmIdx = 0;
  } else if (s_ppmIdx != 0xFF) {
    if (d >= 750 && d <= 2250 && s_ppmIdx < PPM_MAX_CH) s_ppmBuf[s_ppmIdx++] = (uint16_t)d;
    else s_ppmIdx = 0xFF;                             // glitch -> drop frame, wait for next sync
  }
}

static void ppmTask(void*) {
  pinMode(g_hw.pin[F_RX], INPUT);
  attachInterrupt(digitalPinToInterrupt(g_hw.pin[F_RX]), ppmIsr, RISING);
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(2));
    if (!s_ppmNew) continue;
    uint16_t ch[8]; uint8_t n;
    noInterrupts(); n = s_ppmN; for (int i = 0; i < 8; i++) ch[i] = i < n ? s_ppmFrame[i] : 0; s_ppmNew = false; interrupts();
    for (int i = n; i < 8; i++) ch[i] = 1000;         // channels the receiver does not send: AUX low (disarmed, Angle)
    for (int i = 0; i < 8; i++) ch[i] = (uint16_t)constrain((int)ch[i], 900, 2100);
    reorder(ch);
    // Signal loss is detected when pulses stop (RC_TIMEOUT_MS). Set the receiver's failsafe to "no pulses";
    // a receiver set to "hold" keeps sending the last frame and the FC cannot tell the link is gone.
    store(s_crsf, ch);
  }
}

static void crsfTask(void*) {
  uint8_t buf[64]; uint8_t idx = 0;
  for (;;) {
    while (Serial1.available()) {
      uint8_t c = Serial1.read();
      if (idx == 0)      { if (c == 0xC8 || c == 0xEE) buf[idx++] = c; }
      else if (idx == 1) { if (c >= 2 && c <= 62) buf[idx++] = c; else idx = 0; }
      else {
        buf[idx++] = c;
        if (idx == buf[1] + 2) { if (crc8(&buf[2], buf[1] - 1) == buf[idx - 1]) crsfFrame(buf); idx = 0; }
      }
    }
    vTaskDelay(1);
  }
}

void rcInit() {
  if (g_hw.pin[F_RX] != PIN_NONE && g_hw.rxProto == RXP_PPM) {
    xTaskCreatePinnedToCore(ppmTask, "ppm", 2560, nullptr, COMM_TASK_PRIO + 1, nullptr, COMM_CORE);
  } else if (g_hw.pin[F_RX] != PIN_NONE) {           // CRSF; receiver is optional (web joystick / UDP only)
    Serial1.setRxBufferSize(512);
    Serial1.begin(420000, SERIAL_8N1, g_hw.pin[F_RX], g_hw.pin[F_TX] == PIN_NONE ? -1 : g_hw.pin[F_TX]);
    xTaskCreatePinnedToCore(crsfTask, "crsf", 3072, nullptr, COMM_TASK_PRIO, nullptr, COMM_CORE);
  }

  // UDP packet (19 bytes): 'R','C', 8 x uint16 little-endian microseconds (1000..2000), checksum = sum(bytes 2..17) & 0xFF
  if (s_asyncUdp.listen(UDP_RC_PORT)) {
    s_asyncUdp.onPacket([](AsyncUDPPacket p) {
      if (p.length() != 19) return;
      const uint8_t* d = p.data();
      if (d[0] != 'R' || d[1] != 'C') return;
      uint8_t sum = 0; for (int i = 2; i < 18; i++) sum += d[i];
      if (sum != d[18]) return;
      uint16_t ch[8];
      for (int i = 0; i < 8; i++) ch[i] = (uint16_t)constrain((int)(d[2 + 2 * i] | (d[3 + 2 * i] << 8)), 900, 2100);
      store(s_udp, ch);
    });
  }
}

bool rcGet(uint16_t out[8], uint8_t* src) {
  uint32_t now = millis(); bool ok = false; *src = RC_NONE;
  portENTER_CRITICAL(&s_mux);
  if (s_crsf.seen && (int32_t)(now - s_crsf.stamp) < RC_TIMEOUT_MS)     { memcpy(out, s_crsf.ch, 16); *src = g_hw.rxProto == RXP_PPM ? RC_PPM : RC_CRSF; ok = true; }
  else if (s_web.seen && (int32_t)(now - s_web.stamp) < RC_TIMEOUT_MS)  { memcpy(out, s_web.ch, 16);  *src = RC_WEB;  ok = true; }
  else if (s_udp.seen && (int32_t)(now - s_udp.stamp) < RC_TIMEOUT_MS)  { memcpy(out, s_udp.ch, 16);  *src = RC_UDP;  ok = true; }
  portEXIT_CRITICAL(&s_mux);
  if (!ok) memset(out, 0, 16);
  return ok;
}

void rcWebUpdate(const uint16_t ch[8]) {
  uint16_t c[8];
  for (int i = 0; i < 8; i++) c[i] = (uint16_t)constrain((int)ch[i], 900, 2100);
  store(s_web, c);
}
void rcWebDrop() { portENTER_CRITICAL(&s_mux); s_web.seen = false; portEXIT_CRITICAL(&s_mux); }
