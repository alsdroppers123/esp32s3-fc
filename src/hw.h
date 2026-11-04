// hw.h - runtime hardware description: board presets, per-pin function map, IMU bus/orientation,
//        status LED / buzzer / battery sensing. Stored in NVS, edited from the web "Pins" tab.
//        Pin changes take effect after a reboot (peripherals are claimed once at boot).
#pragma once
#include <Arduino.h>

#define PIN_NONE 255
#define HW_MAGIC 0x48570003u

#ifndef FC_BOARD                     // default board preset, set per PlatformIO env (-DFC_BOARD=n)
#define FC_BOARD 0                   // 0 = Waveshare ESP32-S3-Zero
#endif

enum { BOARD_ZERO = 0, BOARD_SUPERMINI, BOARD_XIAO, BOARD_DEVKITC, BOARD_GENERIC, BOARD_COUNT };
enum { BUS_AUTO = 0, BUS_I2C, BUS_SPI };
enum { LED_NONE = 0, LED_WS2812, LED_ACTIVE_HIGH, LED_ACTIVE_LOW };
enum { RXP_CRSF = 0, RXP_PPM = 1 };                       // receiver protocol on the RX pin
enum { CHO_AETR = 0, CHO_TAER, CHO_AERT, CHO_ETAR, CHO_COUNT };   // order of the first four receiver channels
extern const char* const CH_ORDER_NAMES[CHO_COUNT];

// Assignable pin functions (index into HwConfig::pin). Keep in sync with HW_FN_KEYS and the web UI.
enum {
  F_SDA = 0, F_SCL, F_SCK, F_MISO, F_MOSI, F_CS, F_CS2,   // IMU bus (CS2 = BMI088 accel chip-select)
  F_M1, F_M2, F_M3, F_M4,                                   // motors (Betaflight numbering)
  F_RX, F_TX,                                               // CRSF / ELRS receiver UART
  F_LED, F_BUZ, F_VBAT,                                     // status LED, buzzer, battery ADC (ADC1: GPIO1-10)
  F_COUNT
};
extern const char* const HW_FN_KEYS[F_COUNT];
extern const char* const BOARD_NAMES[BOARD_COUNT];

struct HwConfig {
  uint32_t magic;
  uint8_t  board;
  uint8_t  imuBus;        // BUS_AUTO / BUS_I2C / BUS_SPI
  uint8_t  imuType;       // 0 = auto-detect, otherwise a specific ImuType (imu.h)
  uint8_t  align;         // 0..3 = CW 0/90/180/270, 4..7 = same but board flipped (upside down)
  uint8_t  pin[F_COUNT];  // GPIO number or PIN_NONE
  uint8_t  ledType;       // LED_*
  uint8_t  buzInvert;     // 1 = buzzer active-low
  uint16_t i2cKhz;        // 100 / 400 / 1000
  uint16_t spiMhz;        // IMU SPI read clock (register writes always use 1 MHz)
  float    vbatScale;     // volts at pin * scale = battery volts (10k:1k divider -> 11.0)
  // ---- added in v4 (older saved maps are upgraded with defaults) ----
  uint8_t  rxProto;       // RXP_CRSF / RXP_PPM
  uint8_t  chOrder;       // CHO_* : how the receiver orders roll/pitch/throttle/yaw
  uint8_t  _r[2];
};
#define HW_SIZE_V3 36     // sizeof(HwConfig) before rxProto/chOrder were added

extern HwConfig g_hw;                         // loaded once at boot, read-only afterwards
extern volatile uint16_t g_vbatCv;            // battery voltage, centivolts (0 = no sensor)
extern volatile int8_t   g_linkRssi;          // best RSSI of a station on our AP (or STA RSSI), dBm

void hwDefaults(HwConfig& h, uint8_t board);
void hwLoad(HwConfig& h);
bool hwSave(const HwConfig& h);
bool hwValidate(const HwConfig& h, char* err, size_t errLen);   // false + reason if unusable
bool hwGpioUsable(uint8_t gpio);                                // exists on ESP32-S3 and is not flash/USB
int  hwToJson(const HwConfig& h, char* buf, size_t len);

// Status LED / buzzer / battery. statusInit() after hwLoad(); the task runs on Core 0.
void statusInit();
