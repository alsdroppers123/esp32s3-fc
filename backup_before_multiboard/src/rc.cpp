#include "rc.h"
#include <AsyncUDP.h>

struct RcSrc { uint16_t ch[8]; uint32_t stamp; bool seen; };
static RcSrc s_crsf, s_udp;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static AsyncUDP s_asyncUdp;

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
  store(s_crsf, ch);
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
  Serial1.setRxBufferSize(512);
  Serial1.begin(420000, SERIAL_8N1, PIN_CRSF_RX, PIN_CRSF_TX);
  xTaskCreatePinnedToCore(crsfTask, "crsf", 3072, nullptr, COMM_TASK_PRIO, nullptr, COMM_CORE);

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
  if (s_crsf.seen && (int32_t)(now - s_crsf.stamp) < RC_TIMEOUT_MS)     { memcpy(out, s_crsf.ch, 16); *src = RC_CRSF; ok = true; }
  else if (s_udp.seen && (int32_t)(now - s_udp.stamp) < RC_TIMEOUT_MS)  { memcpy(out, s_udp.ch, 16);  *src = RC_UDP;  ok = true; }
  portEXIT_CRITICAL(&s_mux);
  if (!ok) memset(out, 0, 16);
  return ok;
}
