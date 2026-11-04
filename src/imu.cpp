// imu.cpp - IMU hardware abstraction.
//   Buses : I2C (Wire, any pins) or SPI (FSPI host, any pins via GPIO matrix), chosen at runtime from g_hw.
//   Chips : InvenSense MPU-6050/6000/6500/9250/6886, ICM-20602/20608/20689/20948, ICM-42688-P/42605,
//           Bosch BMI160/BMI088, ST LSM6DS3/DSL/DSO/DSR (+ISM330), QST QMI8658.
//   All sensors are configured for ~1 kHz+ output, gyro +-2000 dps and accel +-8 g (BMI088: +-12 g),
//   then converted to g and deg/s in the chip frame and rotated to the FLU body frame with g_hw.align.
#include "imu.h"
#include <Wire.h>
#include <SPI.h>

const char* const IMU_NAMES[IMU_TYPE_COUNT] = {
  "none", "MPU-6050", "MPU-6000", "MPU-6500", "MPU-9250", "MPU-6886",
  "ICM-20602", "ICM-20608", "ICM-20689", "ICM-20948",
  "ICM-42688-P", "ICM-42605",
  "BMI160", "BMI088",
  "LSM6DS3", "LSM6DSL/DS3TR-C", "LSM6DSO", "LSM6DSR/ISM330",
  "QMI8658"};

ImuInfo g_imuInfo;

// ================================================================= bus layer
struct Dev { bool spi; uint8_t addr; uint8_t cs; bool dummy; };   // dummy = BMI088 accel SPI read byte
static SPIClass* s_spi = nullptr;
static uint32_t  s_spiHz = 8000000;

static bool i2cWr(uint8_t a, uint8_t reg, uint8_t v) {
  Wire.beginTransmission(a); Wire.write(reg); Wire.write(v);
  return Wire.endTransmission() == 0;
}
static bool i2cRd(uint8_t a, uint8_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(a); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(a, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}
static bool wr(const Dev& d, uint8_t reg, uint8_t v) {
  if (!d.spi) return i2cWr(d.addr, reg, v);
  s_spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));     // MPU-6000 needs <= 1 MHz writes
  digitalWrite(d.cs, LOW); s_spi->transfer(reg & 0x7F); s_spi->transfer(v); digitalWrite(d.cs, HIGH);
  s_spi->endTransaction();
  delayMicroseconds(2);
  return true;
}
static bool rd(const Dev& d, uint8_t reg, uint8_t* b, uint8_t n, bool fast = false) {
  if (!d.spi) return i2cRd(d.addr, reg, b, n);
  s_spi->beginTransaction(SPISettings(fast ? s_spiHz : 1000000, MSBFIRST, SPI_MODE3));
  digitalWrite(d.cs, LOW);
  s_spi->transfer(reg | 0x80);
  if (d.dummy) s_spi->transfer(0);
  memset(b, 0, n); s_spi->transfer(b, n);
  digitalWrite(d.cs, HIGH);
  s_spi->endTransaction();
  return true;
}
static uint8_t rd8(const Dev& d, uint8_t reg) { uint8_t v = 0; if (!rd(d, reg, &v, 1)) return 0; return v; }
static bool present(const Dev& d) {             // I2C: device ACKs. SPI: a reply that is not a floating bus
  if (!d.spi) { Wire.beginTransmission(d.addr); return Wire.endTransmission() == 0; }
  uint8_t a = rd8(d, 0x75), b = rd8(d, 0x00), c = rd8(d, 0x0F);
  return !((a == 0 || a == 0xFF) && (b == 0 || b == 0xFF) && (c == 0 || c == 0xFF));
}

// ================================================================= sensor state
static Dev     s_dev, s_acc;                    // s_acc only used by BMI088 (separate accel die)
static uint8_t s_type = IMU_NONE;
static uint8_t s_reg, s_len, s_aOff, s_gOff; static bool s_be;
static float   s_aScale = 4096.0f, s_gScale = 16.4f;
static int8_t  s_rot[3][3];                     // chip -> FLU body rotation (entries -1/0/1)

static void setFormat(uint8_t reg, uint8_t len, uint8_t aOff, uint8_t gOff, bool be, float aLsb, float gLsb) {
  s_reg = reg; s_len = len; s_aOff = aOff; s_gOff = gOff; s_be = be; s_aScale = 1.0f / aLsb; s_gScale = 1.0f / gLsb;
}

static void buildRotation(uint8_t align) {
  // body = Rz(-theta) * F * chip, theta = clockwise mounting angle seen from above, F = flip about the roll axis.
  static const int8_t C[4] = {1, 0, -1, 0}, S[4] = {0, 1, 0, -1};
  const int q = align & 3; const int8_t c = C[q], s = S[q], f = (align & 4) ? -1 : 1;
  const int8_t R[3][3] = {{c, (int8_t)(s * f), 0}, {(int8_t)(-s), (int8_t)(c * f), 0}, {0, 0, f}};
  memcpy(s_rot, R, sizeof R);
}
static inline void toBody(const float c[3], float b[3]) {
  for (int i = 0; i < 3; i++) b[i] = s_rot[i][0] * c[0] + s_rot[i][1] * c[1] + s_rot[i][2] * c[2];
}

// ================================================================= identification
static uint8_t identify(const Dev& d, uint8_t& who) {
  if (d.spi) { rd8(d, 0x7F); delay(1); }        // BMI160: a dummy read switches the interface to SPI
  who = rd8(d, 0x75);                           // InvenSense WHO_AM_I
  switch (who) {
    case 0x68: return d.spi ? IMU_MPU6000 : IMU_MPU6050;
    case 0x72: return IMU_MPU6050;              // common GY-521 clone ID
    case 0x70: return IMU_MPU6500;
    case 0x71: case 0x73: return IMU_MPU9250;   // MPU-9250 / MPU-9255
    case 0x19: return IMU_MPU6886;
    case 0x11: case 0x12: return IMU_ICM20602;  // ICM-20600 / ICM-20602
    case 0xAF: return IMU_ICM20608;
    case 0x98: return IMU_ICM20689;
    case 0x47: return IMU_ICM42688;
    case 0x42: return IMU_ICM42605;
  }
  who = rd8(d, 0x00);                           // ICM-20948 (bank 0), Bosch and QST chip IDs
  switch (who) {
    case 0xEA: return IMU_ICM20948;
    case 0xD1: return IMU_BMI160;
    case 0x0F: return IMU_BMI088;               // BMI088 gyro die
    case 0x05: return IMU_QMI8658;
  }
  who = rd8(d, 0x0F);                           // ST WHO_AM_I
  switch (who) {
    case 0x69: return IMU_LSM6DS3;
    case 0x6A: return IMU_LSM6DSL;
    case 0x6C: return IMU_LSM6DSO;
    case 0x6B: return IMU_LSM6DSR;
  }
  return IMU_NONE;
}

// ================================================================= per-family setup
static bool setupMpu(const Dev& d, uint8_t t) {
  wr(d, 0x6B, 0x80); delay(100);                // device reset
  if (d.spi) { wr(d, 0x68, 0x07); delay(100); } // signal path reset (SPI parts)
  wr(d, 0x6B, 0x01); delay(20);                 // clock = PLL
  wr(d, 0x6A, d.spi ? 0x10 : 0x00);             // SPI: disable I2C interface; I2C: aux master off
  const bool legacy = (t == IMU_MPU6050 || t == IMU_MPU6000);
  wr(d, 0x1A, 0x00);                            // gyro DLPF 250/260 Hz, 8 kHz internal
  wr(d, 0x19, legacy ? 7 : 0);                  // 1 kHz output on 6050/6000 (newer parts ignore DIV here)
  wr(d, 0x1B, 0x18);                            // +-2000 dps
  wr(d, 0x1C, 0x10);                            // +-8 g
  if (!legacy) wr(d, 0x1D, 0x01);               // accel DLPF ~218 Hz
  if (!d.spi) wr(d, 0x37, 0x02);                // I2C bypass (exposes AK8963 on MPU-9250)
  delay(10);
  setFormat(0x3B, 14, 0, 8, true, 4096.0f, 16.4f);
  return (rd8(d, 0x1B) & 0x18) == 0x18;
}
static bool setupIcm426(const Dev& d) {
  wr(d, 0x76, 0x00);                            // register bank 0
  wr(d, 0x11, 0x01); delay(5);                  // soft reset
  wr(d, 0x4E, 0x0F); delay(2);                  // gyro + accel low-noise mode
  wr(d, 0x4F, 0x06);                            // gyro 2000 dps, 1 kHz
  wr(d, 0x50, 0x26);                            // accel 8 g, 1 kHz
  delay(50);
  setFormat(0x1F, 12, 0, 6, true, 4096.0f, 16.4f);
  return rd8(d, 0x4F) == 0x06;
}
static bool setupIcm20948(const Dev& d) {
  wr(d, 0x7F, 0x00);                            // bank 0
  wr(d, 0x06, 0x80); delay(60);                 // reset
  wr(d, 0x06, 0x01); delay(20);                 // auto clock, wake
  wr(d, 0x07, 0x00);                            // accel + gyro on
  if (d.spi) wr(d, 0x03, 0x10);                 // I2C interface off
  wr(d, 0x7F, 0x20);                            // bank 2
  wr(d, 0x00, 0x00);                            // gyro ODR 1.1 kHz
  wr(d, 0x01, 0x07);                            // 2000 dps, DLPF on (197 Hz)
  wr(d, 0x10, 0x00); wr(d, 0x11, 0x00);         // accel ODR 1.1 kHz
  wr(d, 0x14, 0x05);                            // 8 g, DLPF on
  uint8_t chk = rd8(d, 0x01);
  wr(d, 0x7F, 0x00);
  delay(20);
  setFormat(0x2D, 12, 0, 6, true, 4096.0f, 16.4f);
  return chk == 0x07;
}
static bool setupBmi160(const Dev& d) {
  wr(d, 0x7E, 0xB6); delay(100);                // soft reset
  if (d.spi) { rd8(d, 0x7F); delay(1); }
  wr(d, 0x7E, 0x11); delay(10);                 // accel normal mode
  wr(d, 0x7E, 0x15); delay(90);                 // gyro normal mode
  wr(d, 0x40, 0x2C);                            // accel 1600 Hz, normal filter
  wr(d, 0x41, 0x08);                            // +-8 g
  wr(d, 0x42, 0x2C);                            // gyro 1600 Hz
  wr(d, 0x43, 0x00);                            // +-2000 dps
  delay(10);
  setFormat(0x0C, 12, 6, 0, false, 4096.0f, 16.4f);
  return rd8(d, 0x43) == 0x00 && rd8(d, 0x00) == 0xD1;
}
static bool setupLsm6(const Dev& d) {
  wr(d, 0x12, 0x01); delay(20);                 // CTRL3_C: software reset
  wr(d, 0x12, 0x44);                            // BDU + register auto-increment
  wr(d, 0x10, 0x8C);                            // CTRL1_XL: 1.66 kHz, +-8 g
  wr(d, 0x11, 0x8C);                            // CTRL2_G : 1.66 kHz, +-2000 dps
  delay(20);
  setFormat(0x22, 12, 6, 0, false, 1.0f / 0.000244f, 1.0f / 0.070f);
  return rd8(d, 0x11) == 0x8C;
}
static bool setupQmi8658(const Dev& d) {
  wr(d, 0x60, 0xB0); delay(30);                 // soft reset
  wr(d, 0x02, 0x40);                            // CTRL1: address auto-increment, little endian, 4-wire SPI
  wr(d, 0x03, 0x23);                            // CTRL2: accel +-8 g, ~900 Hz
  wr(d, 0x04, 0x73);                            // CTRL3: gyro +-2048 dps, ~900 Hz
  wr(d, 0x06, 0x00);                            // CTRL5: on-chip LPF off (we filter)
  wr(d, 0x08, 0x03);                            // CTRL7: accel + gyro enable
  delay(50);
  setFormat(0x35, 12, 0, 6, false, 4096.0f, 16.0f);
  return rd8(d, 0x04) == 0x73;
}
static bool setupBmi088(const Dev& g, const Dev& a) {
  if (a.spi) rd8(a, 0x00);                      // rising CS edge -> accel enters SPI mode
  wr(a, 0x7E, 0xB6); delay(5);                  // accel soft reset
  if (a.spi) rd8(a, 0x00);
  wr(a, 0x7D, 0x04); delay(50);                 // accel on
  wr(a, 0x7C, 0x00); delay(5);                  // active mode
  wr(a, 0x40, 0xAC);                            // 1600 Hz, normal filter
  wr(a, 0x41, 0x02);                            // +-12 g
  wr(g, 0x14, 0xB6); delay(35);                 // gyro soft reset
  wr(g, 0x0F, 0x00);                            // +-2000 dps
  wr(g, 0x10, 0x01);                            // 2 kHz ODR, 230 Hz filter
  wr(g, 0x11, 0x00);                            // normal power
  delay(10);
  s_aScale = 1.0f / (32768.0f / 12.0f); s_gScale = 1.0f / 16.384f;
  return rd8(a, 0x00) == 0x1E && rd8(g, 0x0F) == 0x00;
}

// ---- AK8963 magnetometer (inside the MPU-9250, I2C bypass) ----
#define AK_ADDR 0x0C
static float s_akAdj[3] = {1, 1, 1};
static bool  s_akOk = false, s_haveMag = false;
static float s_magOff[3] = {0, 0, 0}, s_lastMag[3] = {0, 0, 0};
static float s_mMin[3], s_mMax[3];
static volatile bool s_magCalOn = false;

static void setupAk8963() {
  uint8_t who = 0;
  if (!(i2cRd(AK_ADDR, 0x00, &who, 1) && who == 0x48)) return;
  uint8_t asa[3];
  i2cWr(AK_ADDR, 0x0A, 0x00); delay(10);
  i2cWr(AK_ADDR, 0x0A, 0x0F); delay(10);         // fuse ROM
  i2cRd(AK_ADDR, 0x10, asa, 3);
  for (int i = 0; i < 3; i++) s_akAdj[i] = (asa[i] - 128) / 256.0f + 1.0f;
  i2cWr(AK_ADDR, 0x0A, 0x00); delay(10);
  i2cWr(AK_ADDR, 0x0A, 0x16); delay(10);         // 16-bit, continuous mode 2 (100 Hz)
  s_akOk = true;
}

static bool configure(uint8_t t) {
  switch (t) {
    case IMU_MPU6050: case IMU_MPU6000: case IMU_MPU6500: case IMU_MPU9250: case IMU_MPU6886:
    case IMU_ICM20602: case IMU_ICM20608: case IMU_ICM20689: return setupMpu(s_dev, t);
    case IMU_ICM20948: return setupIcm20948(s_dev);
    case IMU_ICM42688: case IMU_ICM42605: return setupIcm426(s_dev);
    case IMU_BMI160:   return setupBmi160(s_dev);
    case IMU_BMI088:   return setupBmi088(s_dev, s_acc);
    case IMU_LSM6DS3: case IMU_LSM6DSL: case IMU_LSM6DSO: case IMU_LSM6DSR: return setupLsm6(s_dev);
    case IMU_QMI8658:  return setupQmi8658(s_dev);
  }
  return false;
}

// Try one device: identify (or accept the forced type), locate the BMI088 accel die, configure.
static bool tryDev(const Dev& d, const HwConfig& hw) {
  if (!present(d)) return false;
  uint8_t who = 0, t = identify(d, who);
  if (hw.imuType) {
    if (t != hw.imuType) Serial.printf("IMU: forced %s, chip reports ID 0x%02X\n", IMU_NAMES[hw.imuType], who);
    t = hw.imuType;
  }
  if (t == IMU_NONE) { Serial.printf("IMU: unknown device (ID bytes 0x%02X)\n", who); return false; }
  s_dev = d;
  if (t == IMU_BMI088) {
    bool found = false;
    if (d.spi) {
      if (hw.pin[F_CS2] != PIN_NONE) { s_acc = {true, 0, hw.pin[F_CS2], true}; rd8(s_acc, 0x00); found = rd8(s_acc, 0x00) == 0x1E; }
    } else {
      for (uint8_t a : {0x18, 0x19}) { s_acc = {false, a, 0, false}; if (rd8(s_acc, 0x00) == 0x1E) { found = true; break; } }
    }
    if (!found) { Serial.println("IMU: BMI088 gyro found but accel die missing (SPI needs CS2)"); return false; }
  }
  if (!configure(t)) { Serial.printf("IMU: %s did not accept configuration\n", IMU_NAMES[t]); return false; }
  s_type = t;
  g_imuInfo.type = t; g_imuInfo.bus = d.spi ? BUS_SPI : BUS_I2C; g_imuInfo.addr = d.addr; g_imuInfo.whoami = who;
  return true;
}

bool imuInit(const HwConfig& hw) {
  memset(&g_imuInfo, 0, sizeof g_imuInfo);
  buildRotation(hw.align);
  s_spiHz = (uint32_t)hw.spiMhz * 1000000u;
  const uint8_t* p = hw.pin;
  const bool haveSpi = p[F_SCK] != PIN_NONE && p[F_MISO] != PIN_NONE && p[F_MOSI] != PIN_NONE && p[F_CS] != PIN_NONE;
  const bool haveI2c = p[F_SDA] != PIN_NONE && p[F_SCL] != PIN_NONE;
  bool ok = false;

  if (haveSpi && hw.imuBus != BUS_I2C) {
    pinMode(p[F_CS], OUTPUT); digitalWrite(p[F_CS], HIGH);
    if (p[F_CS2] != PIN_NONE) { pinMode(p[F_CS2], OUTPUT); digitalWrite(p[F_CS2], HIGH); }
    s_spi = new SPIClass(FSPI);
    s_spi->begin(p[F_SCK], p[F_MISO], p[F_MOSI], -1);
    delay(5);
    ok = tryDev({true, 0, p[F_CS], false}, hw);
  }
  if (!ok && haveI2c && hw.imuBus != BUS_SPI) {
    Wire.begin(p[F_SDA], p[F_SCL], (uint32_t)hw.i2cKhz * 1000u);
    Wire.setTimeOut(5);
    for (uint8_t a = 0x08; a < 0x78 && g_imuInfo.scanN < sizeof g_imuInfo.scan; a++) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) g_imuInfo.scan[g_imuInfo.scanN++] = a;
    }
    uint8_t scanN = g_imuInfo.scanN, scan[16]; memcpy(scan, g_imuInfo.scan, sizeof scan);
    for (uint8_t a : {0x68, 0x69, 0x6A, 0x6B}) {
      if (tryDev({false, a, 0, false}, hw)) { ok = true; break; }
    }
    g_imuInfo.scanN = scanN; memcpy(g_imuInfo.scan, scan, sizeof scan);
  }
  if (!ok) { Serial.println("IMU: no supported sensor found - check the Pins tab"); return false; }

  Serial.printf("IMU: %s on %s (ID 0x%02X)\n", IMU_NAMES[s_type], g_imuInfo.bus == BUS_SPI ? "SPI" : "I2C", g_imuInfo.whoami);
  if (s_type == IMU_MPU9250 && !s_dev.spi) setupAk8963();
  g_imuInfo.mag = s_akOk;
  imuGyroCalStart();
  return true;
}

// ================================================================= gyro calibration
#define CAL_N 1500
static float  s_gyroOff[3];
static bool   s_gyroCal = false;
static int    s_calN = 0;
static double s_calSum[3], s_calSq[3];

void imuGyroCalStart() {
  s_gyroCal = false; s_calN = 0;
  for (int i = 0; i < 3; i++) { s_calSum[i] = s_calSq[i] = 0; }
}
bool imuGyroCalibrated() { return s_gyroCal; }

void imuMagCalStart() {
  for (int i = 0; i < 3; i++) { s_mMin[i] = 1e9f; s_mMax[i] = -1e9f; }
  s_magCalOn = true;
}
bool imuMagCalActive() { return s_magCalOn; }
void imuMagCalFinish(float off[3]) {
  s_magCalOn = false;
  for (int i = 0; i < 3; i++) off[i] = (s_mMax[i] > s_mMin[i]) ? 0.5f * (s_mMax[i] + s_mMin[i]) : 0.0f;
}
void imuSetMagOffset(const float off[3]) { for (int i = 0; i < 3; i++) s_magOff[i] = off[i]; }

// ================================================================= sampling
static inline float s16(const uint8_t* b, bool be) {
  return (float)(int16_t)(be ? ((b[0] << 8) | b[1]) : ((b[1] << 8) | b[0]));
}

static bool readRaw(float acc[3], float gyr[3]) {
  uint8_t b[14];
  if (s_type == IMU_BMI088) {
    if (!rd(s_dev, 0x02, b, 6, true) || !rd(s_acc, 0x12, b + 6, 6, true)) return false;
    for (int i = 0; i < 3; i++) { gyr[i] = s16(b + 2 * i, false) * s_gScale; acc[i] = s16(b + 6 + 2 * i, false) * s_aScale; }
    return true;
  }
  if (!rd(s_dev, s_reg, b, s_len, true)) return false;
  for (int i = 0; i < 3; i++) {
    acc[i] = s16(b + s_aOff + 2 * i, s_be) * s_aScale;
    gyr[i] = s16(b + s_gOff + 2 * i, s_be) * s_gScale;
  }
  return true;
}

bool imuRead(ImuData& d, bool wantMag) {
  float acc[3], gyr[3];
  if (s_type == IMU_NONE || !readRaw(acc, gyr)) return false;
  if (acc[0] == 0 && acc[1] == 0 && acc[2] == 0 && gyr[0] == 0 && gyr[1] == 0 && gyr[2] == 0) return false;  // bus stuck
  toBody(acc, d.accFlu);
  toBody(gyr, d.gyroFlu);

  if (!s_gyroCal) {                                // collect bias while the board is still
    for (int i = 0; i < 3; i++) { s_calSum[i] += d.gyroFlu[i]; s_calSq[i] += (double)d.gyroFlu[i] * d.gyroFlu[i]; }
    if (++s_calN >= CAL_N) {
      bool still = true; float mean[3];
      for (int i = 0; i < 3; i++) {
        mean[i] = (float)(s_calSum[i] / CAL_N);
        double var = s_calSq[i] / CAL_N - (double)mean[i] * mean[i];
        if (sqrt(var > 0 ? var : 0) > 2.0 || fabsf(mean[i]) > 30.0f) still = false;
      }
      if (still) { for (int i = 0; i < 3; i++) s_gyroOff[i] = mean[i]; s_gyroCal = true; }
      else imuGyroCalStart();                      // moved -> try again
    }
    for (int i = 0; i < 3; i++) d.gyroFlu[i] = 0;
  } else {
    for (int i = 0; i < 3; i++) d.gyroFlu[i] -= s_gyroOff[i];
  }
  d.rate[0] =  d.gyroFlu[0];                       // roll  : right-side-down positive
  d.rate[1] = -d.gyroFlu[1];                       // pitch : nose-up positive
  d.rate[2] = -d.gyroFlu[2];                       // yaw   : clockwise (from above) positive

  d.magFresh = false;
  if (wantMag && s_akOk) {
    static uint8_t div = 0;
    if (++div >= 20) {                             // 50 Hz
      div = 0;
      uint8_t m[8];
      if (i2cRd(AK_ADDR, 0x02, m, 8) && (m[0] & 1) && !(m[7] & 0x08)) {
        const float k = 0.15f;
        float ak[3] = {(int16_t)((m[2] << 8) | m[1]) * k * s_akAdj[0],
                       (int16_t)((m[4] << 8) | m[3]) * k * s_akAdj[1],
                       (int16_t)((m[6] << 8) | m[5]) * k * s_akAdj[2]};
        float chip[3] = {ak[1], ak[0], -ak[2]};    // AK8963 axes -> MPU axes
        if (s_magCalOn) for (int i = 0; i < 3; i++) { if (chip[i] < s_mMin[i]) s_mMin[i] = chip[i]; if (chip[i] > s_mMax[i]) s_mMax[i] = chip[i]; }
        for (int i = 0; i < 3; i++) chip[i] -= s_magOff[i];
        toBody(chip, s_lastMag);
        s_haveMag = true; d.magFresh = true;
      }
    }
  }
  for (int i = 0; i < 3; i++) d.magFlu[i] = s_lastMag[i];
  return true;
}

// ---- attitude ----
static float q0 = 1, q1 = 0, q2 = 0, q3 = 0;
static float s_roll = 0, s_pitch = 0, s_yaw = 0;
static uint32_t s_warm = 0;

// Mahony AHRS (x-io formulation, Kp only; gyro bias is removed by calibration).
void imuUpdateAttitude(const ImuData& d, float dt, float kp, bool useMag) {
  const float D2R = 0.01745329252f;
  float gx = d.gyroFlu[0] * D2R, gy = d.gyroFlu[1] * D2R, gz = d.gyroFlu[2] * D2R;
  float ax = d.accFlu[0], ay = d.accFlu[1], az = d.accFlu[2];
  if (s_warm < 2000) { s_warm++; kp = 10.0f; }     // fast convergence during the first 2 s
  float an = sqrtf(ax * ax + ay * ay + az * az);
  if (an > 0.7f && an < 1.3f) {                    // ignore accel during hard manoeuvres
    float inv = 1.0f / an; ax *= inv; ay *= inv; az *= inv;
    float q0q0 = q0 * q0, q0q1 = q0 * q1, q0q2 = q0 * q2, q0q3 = q0 * q3;
    float q1q1 = q1 * q1, q1q2 = q1 * q2, q1q3 = q1 * q3, q2q2 = q2 * q2, q2q3 = q2 * q3, q3q3 = q3 * q3;
    float hvx = q1q3 - q0q2, hvy = q0q1 + q2q3, hvz = q0q0 - 0.5f + q3q3;
    float ex = ay * hvz - az * hvy, ey = az * hvx - ax * hvz, ez = ax * hvy - ay * hvx;
    if (useMag && s_haveMag) {
      float mx = d.magFlu[0], my = d.magFlu[1], mz = d.magFlu[2];
      float mn = sqrtf(mx * mx + my * my + mz * mz);
      if (mn > 1e-3f) {
        mx /= mn; my /= mn; mz /= mn;
        float hx = 2.0f * (mx * (0.5f - q2q2 - q3q3) + my * (q1q2 - q0q3) + mz * (q1q3 + q0q2));
        float hy = 2.0f * (mx * (q1q2 + q0q3) + my * (0.5f - q1q1 - q3q3) + mz * (q2q3 - q0q1));
        float bx = sqrtf(hx * hx + hy * hy);
        float bz = 2.0f * (mx * (q1q3 - q0q2) + my * (q2q3 + q0q1) + mz * (0.5f - q1q1 - q2q2));
        float wx = bx * (0.5f - q2q2 - q3q3) + bz * (q1q3 - q0q2);
        float wy = bx * (q1q2 - q0q3) + bz * (q0q1 + q2q3);
        float wz = bx * (q0q2 + q1q3) + bz * (0.5f - q1q1 - q2q2);
        ex += my * wz - mz * wy; ey += mz * wx - mx * wz; ez += mx * wy - my * wx;
      }
    }
    gx += 2.0f * kp * ex; gy += 2.0f * kp * ey; gz += 2.0f * kp * ez;
  }
  gx *= 0.5f * dt; gy *= 0.5f * dt; gz *= 0.5f * dt;
  float qa = q0, qb = q1, qc = q2;
  q0 += -qb * gx - qc * gy - q3 * gz;
  q1 +=  qa * gx + qc * gz - q3 * gy;
  q2 +=  qa * gy - qb * gz + q3 * gx;
  q3 +=  qa * gz + qb * gy - qc * gx;
  float n = 1.0f / sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
  q0 *= n; q1 *= n; q2 *= n; q3 *= n;

  const float R2D = 57.29577951f;
  s_roll = atan2f(2.0f * (q0 * q1 + q2 * q3), 1.0f - 2.0f * (q1 * q1 + q2 * q2)) * R2D;
  float s = 2.0f * (q0 * q2 - q3 * q1); s = s > 1 ? 1 : (s < -1 ? -1 : s);
  s_pitch = -asinf(s) * R2D;                                                    // FLU pitch is nose-down+, flip
  s_yaw = -atan2f(2.0f * (q0 * q3 + q1 * q2), 1.0f - 2.0f * (q2 * q2 + q3 * q3)) * R2D;
}

void imuGetAttitude(float& r, float& p, float& y) { r = s_roll; p = s_pitch; y = s_yaw; }
