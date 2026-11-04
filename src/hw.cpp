#include "hw.h"
#include "config.h"
#include <Preferences.h>
#include <soc/gpio_reg.h>

const char* const HW_FN_KEYS[F_COUNT] = {"sda", "scl", "sck", "miso", "mosi", "cs", "cs2",
                                         "m1", "m2", "m3", "m4", "rx", "tx", "led", "buz", "vbat"};
const char* const CH_ORDER_NAMES[CHO_COUNT] = {"AETR", "TAER", "AERT", "ETAR"};
const char* const BOARD_NAMES[BOARD_COUNT] = {"Waveshare ESP32-S3-Zero", "ESP32-S3 Super Mini",
                                              "Seeed XIAO ESP32S3", "ESP32-S3-DevKitC-1", "Generic ESP32-S3"};

HwConfig g_hw;
static_assert(offsetof(HwConfig, rxProto) == HW_SIZE_V3, "v3 layout must stay a prefix of HwConfig");
volatile uint16_t g_vbatCv = 0;
volatile int8_t   g_linkRssi = 0;

// ------------------------------------------------------------------ presets
// Every preset only uses pins that are brought out on that board. Motors sit on adjacent header pins.
void hwDefaults(HwConfig& h, uint8_t board) {
  memset(&h, 0, sizeof h);
  memset(h.pin, PIN_NONE, sizeof h.pin);
  h.magic = HW_MAGIC; h.board = board < BOARD_COUNT ? board : (uint8_t)BOARD_GENERIC;
  h.imuBus = BUS_AUTO; h.imuType = 0; h.align = 0;
  h.i2cKhz = 400; h.spiMhz = 8; h.vbatScale = 11.0f; h.ledType = LED_NONE;
  h.rxProto = RXP_CRSF; h.chOrder = CHO_AETR;
  uint8_t* p = h.pin;
  switch (h.board) {
    case BOARD_ZERO:        // 4 MB flash / 2 MB PSRAM, WS2812 on GPIO21, no USB-UART chip -> 43/44 free
    case BOARD_SUPERMINI:   // same footprint, WS2812 on GPIO48
      p[F_VBAT] = 1; p[F_SDA] = 4; p[F_SCL] = 5;
      p[F_M1] = 7; p[F_M2] = 8; p[F_M3] = 9; p[F_M4] = 10;
      p[F_RX] = 44; p[F_TX] = 43;
      p[F_LED] = (h.board == BOARD_ZERO) ? 21 : 48; h.ledType = LED_WS2812;
      break;
    case BOARD_XIAO:        // D0-D3 = GPIO1-4 motors, D4/D5 = I2C, D6/D7 = UART, user LED GPIO21 (active low)
      p[F_M1] = 1; p[F_M2] = 2; p[F_M3] = 3; p[F_M4] = 4;
      p[F_SDA] = 5; p[F_SCL] = 6; p[F_TX] = 43; p[F_RX] = 44;
      p[F_LED] = 21; h.ledType = LED_ACTIVE_LOW;
      break;
    case BOARD_DEVKITC:     // 43/44 are wired to the USB-UART bridge -> receiver on UART1 17/18 instead
    default:
      p[F_SDA] = 9; p[F_SCL] = 10;
      p[F_M1] = 4; p[F_M2] = 5; p[F_M3] = 6; p[F_M4] = 7;
      p[F_RX] = 18; p[F_TX] = 17; p[F_VBAT] = 1;
      if (h.board == BOARD_DEVKITC) { p[F_LED] = 48; h.ledType = LED_WS2812; }   // v1.1 boards use GPIO38
      break;
  }
}

bool hwGpioUsable(uint8_t g) {
  // 19/20 = native USB (serial console + flashing), 22-25 do not exist, 26-32 = SPI flash / PSRAM.
  return g <= 18 || g == 21 || (g >= 33 && g <= 48);
}

bool hwValidate(const HwConfig& h, char* err, size_t n) {
  if (h.board >= BOARD_COUNT || h.imuBus > BUS_SPI || h.align > 7 || h.ledType > LED_ACTIVE_LOW || h.rxProto > RXP_PPM || h.chOrder >= CHO_COUNT) {
    snprintf(err, n, "Invalid option value"); return false;
  }
  for (int i = 0; i < F_COUNT; i++) {
    uint8_t g = h.pin[i];
    if (g == PIN_NONE) continue;
    if (!hwGpioUsable(g)) { snprintf(err, n, "GPIO%u cannot be used (%s)", g, HW_FN_KEYS[i]); return false; }
    for (int j = 0; j < i; j++)
      if (h.pin[j] == g) { snprintf(err, n, "GPIO%u assigned twice (%s and %s)", g, HW_FN_KEYS[j], HW_FN_KEYS[i]); return false; }
  }
  for (int m = F_M1; m <= F_M4; m++)
    if (h.pin[m] == PIN_NONE) { snprintf(err, n, "All four motor outputs must be assigned"); return false; }
  if (h.pin[F_VBAT] != PIN_NONE && (h.pin[F_VBAT] < 1 || h.pin[F_VBAT] > 10)) {
    snprintf(err, n, "Battery sense needs an ADC1 pin (GPIO1-10); ADC2 is blocked by Wi-Fi"); return false;
  }
  bool i2c = h.pin[F_SDA] != PIN_NONE && h.pin[F_SCL] != PIN_NONE;
  bool spi = h.pin[F_SCK] != PIN_NONE && h.pin[F_MISO] != PIN_NONE && h.pin[F_MOSI] != PIN_NONE && h.pin[F_CS] != PIN_NONE;
  if (h.imuBus == BUS_I2C && !i2c) { snprintf(err, n, "I2C IMU needs SDA and SCL"); return false; }
  if (h.imuBus == BUS_SPI && !spi) { snprintf(err, n, "SPI IMU needs SCK, MISO, MOSI and CS"); return false; }
  if (h.imuBus == BUS_AUTO && !i2c && !spi) { snprintf(err, n, "Assign an IMU bus (SDA+SCL or SCK+MISO+MOSI+CS)"); return false; }
  if (h.i2cKhz < 100 || h.i2cKhz > 1000 || h.spiMhz < 1 || h.spiMhz > 20) { snprintf(err, n, "Bus clock out of range"); return false; }
  if (!(h.vbatScale > 1.0f && h.vbatScale < 50.0f)) { snprintf(err, n, "Battery scale must be 1..50"); return false; }
  return true;
}

void hwLoad(HwConfig& h) {
  Preferences p; bool ok = false;
  if (p.begin("fc", true)) {
    size_t n = p.getBytesLength("hw");
    if (n == sizeof(HwConfig) || n == HW_SIZE_V3) {                    // v3 maps: keep pins, default the new fields
      memset(&h, 0, sizeof h); p.getBytes("hw", &h, n); ok = h.magic == HW_MAGIC;
      if (n == HW_SIZE_V3) { h.rxProto = RXP_CRSF; h.chOrder = CHO_AETR; }
    }
    p.end();
  }
  char e[96];
  if (ok && !hwValidate(h, e, sizeof e)) { Serial.printf("HW config invalid (%s) - using board defaults\n", e); ok = false; }
  if (!ok) hwDefaults(h, FC_BOARD);
}

bool hwSave(const HwConfig& h) {
  Preferences p;
  if (!p.begin("fc", false)) return false;
  bool ok = p.putBytes("hw", &h, sizeof h) == sizeof h;
  p.end();
  return ok;
}

int hwToJson(const HwConfig& h, char* b, size_t n) {
  int k = snprintf(b, n, "{\"board\":%u,\"bus\":%u,\"imu\":%u,\"align\":%u,\"ledt\":%u,\"buzinv\":%u,\"i2c\":%u,\"spi\":%u,\"vscale\":%.2f,\"rxp\":%u,\"cho\":%u,\"pins\":{",
                   h.board, h.imuBus, h.imuType, h.align, h.ledType, h.buzInvert, h.i2cKhz, h.spiMhz, h.vbatScale, h.rxProto, h.chOrder);
  for (int i = 0; i < F_COUNT && k < (int)n; i++)
    k += snprintf(b + k, n - k, "%s\"%s\":%d", i ? "," : "", HW_FN_KEYS[i], h.pin[i] == PIN_NONE ? -1 : h.pin[i]);
  if (k < (int)n) k += snprintf(b + k, n - k, "}}");
  return k;
}

// ------------------------------------------------------------------ WS2812 (bit-banged)
// All four RMT TX channels of the S3 are taken by DShot, so the single status pixel is bit-banged
// from Core 0 (about 30 us with interrupts off on that core only; the flight loop on Core 1 is unaffected).
static portMUX_TYPE s_ledMux = portMUX_INITIALIZER_UNLOCKED;
static void IRAM_ATTR ws2812(uint8_t pin, uint8_t r, uint8_t g, uint8_t b) {
  volatile uint32_t *set, *clr; uint32_t mask;
  if (pin < 32) { set = (volatile uint32_t*)GPIO_OUT_W1TS_REG;  clr = (volatile uint32_t*)GPIO_OUT_W1TC_REG;  mask = 1u << pin; }
  else          { set = (volatile uint32_t*)GPIO_OUT1_W1TS_REG; clr = (volatile uint32_t*)GPIO_OUT1_W1TC_REG; mask = 1u << (pin - 32); }
  const uint32_t mhz = ESP.getCpuFreqMHz();
  const uint32_t t0 = mhz * 35 / 100, t1 = mhz * 75 / 100, per = mhz * 125 / 100;
  const uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
  portENTER_CRITICAL(&s_ledMux);
  for (int i = 23; i >= 0; i--) {
    uint32_t th = (grb >> i) & 1 ? t1 : t0, s = ESP.getCycleCount();
    *set = mask;
    while (ESP.getCycleCount() - s < th) {}
    *clr = mask;
    while (ESP.getCycleCount() - s < per) {}
  }
  portEXIT_CRITICAL(&s_ledMux);
}

static void ledSet(uint8_t r, uint8_t g, uint8_t b) {
  const uint8_t pin = g_hw.pin[F_LED];
  if (pin == PIN_NONE) return;
  switch (g_hw.ledType) {
    case LED_WS2812:      ws2812(pin, r, g, b); break;
    case LED_ACTIVE_HIGH: digitalWrite(pin, (r | g | b) ? HIGH : LOW); break;
    case LED_ACTIVE_LOW:  digitalWrite(pin, (r | g | b) ? LOW : HIGH); break;
  }
}
static void buzz(bool on) {
  const uint8_t pin = g_hw.pin[F_BUZ];
  if (pin != PIN_NONE) digitalWrite(pin, (on ^ (g_hw.buzInvert != 0)) ? HIGH : LOW);
}

extern volatile bool g_imuOk;   // main.cpp

// ------------------------------------------------------------------ status task (Core 0, 50 Hz)
static void statusTask(void*) {
  uint32_t tick = 0, bootHeld = 0, beepUntil = 0, beepGap = 0; uint8_t beepsLeft = 0;
  bool wasArmed = false; float vf = 0; uint8_t cells = 0;
  bool useBoot = true;                                    // GPIO0 = BOOT button, unless reassigned
  for (int i = 0; i < F_COUNT; i++) if (g_hw.pin[i] == 0) useBoot = false;
  if (useBoot) pinMode(0, INPUT_PULLUP);

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(20)); tick++;
    Telemetry t; telGet(t);
    const uint32_t now = millis();

    // ---- battery (ADC1, 10 Hz, PT1 smoothed)
    if (g_hw.pin[F_VBAT] != PIN_NONE && tick % 5 == 0) {
      float v = analogReadMilliVolts(g_hw.pin[F_VBAT]) * 0.001f * g_hw.vbatScale;
      vf = vf < 0.5f ? v : vf + 0.2f * (v - vf);
      g_vbatCv = vf < 0.5f ? 0 : (uint16_t)(vf * 100.0f);
      if (!cells && vf > 2.5f) cells = (uint8_t)ceilf(vf / 4.35f);
    }
    const bool lowBat = cells && vf < 3.45f * cells;

    // ---- buzzer events
    if (t.armed != wasArmed) { beepsLeft = t.armed ? 1 : 2; beepGap = now; wasArmed = t.armed; }
    if (t.armed && !t.rcSrc && !beepsLeft && tick % 10 == 0) { beepsLeft = 1; beepGap = now; }          // failsafe
    if (t.armed && lowBat && !beepsLeft && tick % 100 == 0) { beepsLeft = 3; beepGap = now; }         // low battery
    if (beepsLeft && now >= beepGap) { beepUntil = now + (t.armed ? 220 : 90); beepGap = now + 260; beepsLeft--; }
    buzz(now < beepUntil);

    // ---- BOOT button held 5 s while disarmed -> restore board pin defaults and reboot
    if (useBoot && !t.armed && digitalRead(0) == LOW) {
      if (++bootHeld >= 250) {
        HwConfig d; hwDefaults(d, g_hw.board); hwSave(d);
        for (int i = 0; i < 6; i++) { ledSet(40, 40, 40); delay(80); ledSet(0, 0, 0); delay(80); }
        ESP.restart();
      }
      if (bootHeld > 50) { ledSet((tick & 4) ? 40 : 0, (tick & 4) ? 40 : 0, (tick & 4) ? 40 : 0); continue; }
    } else bootHeld = 0;

    // ---- LED pattern
    if (g_hw.pin[F_LED] == PIN_NONE || tick % 2) continue;                       // 25 Hz refresh
    const uint32_t ph = now % 1000;
    if (!g_imuOk)                 ledSet((now % 200) < 100 ? 60 : 0, 0, 0);        // IMU missing: fast red
    else if (t.armed)             ledSet(lowBat && ph < 500 ? 60 : 50, lowBat && ph < 500 ? 30 : 0, 0);
    else if (!t.gyroCal)          ledSet(0, 0, (now % 250) < 125 ? 60 : 0);       // calibrating: blue blink
    else if (t.preArm == PA_ALL || (t.preArm | PA_SWITCH | PA_THR | PA_RC) == PA_ALL) {
      float br = 0.5f - 0.5f * cosf(ph * 0.006283f);                              // ready: green breathing
      ledSet(0, (uint8_t)(4 + br * 40), (uint8_t)(br * 10));
    } else                        ledSet(ph < 120 ? 50 : 0, ph < 120 ? 30 : 0, 0); // not ready: amber blip
  }
}

void statusInit() {
  uint8_t lp = g_hw.pin[F_LED], bp = g_hw.pin[F_BUZ];
  if (lp != PIN_NONE && g_hw.ledType != LED_NONE) { pinMode(lp, OUTPUT); digitalWrite(lp, g_hw.ledType == LED_ACTIVE_LOW ? HIGH : LOW); }
  if (bp != PIN_NONE) { pinMode(bp, OUTPUT); buzz(false); }
  if (g_hw.pin[F_VBAT] != PIN_NONE) { analogReadResolution(12); analogSetPinAttenuation(g_hw.pin[F_VBAT], ADC_11db); }
  if (g_hw.ledType == LED_WS2812) ledSet(0, 0, 0);
  xTaskCreatePinnedToCore(statusTask, "status", 3072, nullptr, COMM_TASK_PRIO - 1, nullptr, COMM_CORE);
}
