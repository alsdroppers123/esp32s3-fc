#include "imu.h"
#include <Wire.h>

#define MPU_ADDR 0x68
#define AK_ADDR  0x0C

static bool wr(uint8_t a, uint8_t reg, uint8_t v) {
  Wire.beginTransmission(a); Wire.write(reg); Wire.write(v);
  return Wire.endTransmission() == 0;
}
static bool rd(uint8_t a, uint8_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(a); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(a, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

static inline void toBody(const float c[3], float b[3]) {
  b[0] = IMU_F_SIGN * c[IMU_F_AXIS];
  b[1] = IMU_L_SIGN * c[IMU_L_AXIS];
  b[2] = IMU_U_SIGN * c[IMU_U_AXIS];
}

// ---- gyro calibration ----
#define CAL_N 1500
static float  s_gyroOff[3];
static bool   s_gyroCal = false;
static int    s_calN = 0;
static double s_calSum[3], s_calSq[3];

// ---- magnetometer ----
static float s_akAdj[3] = {1, 1, 1};
static bool  s_akOk = false, s_haveMag = false;
static float s_magOff[3] = {0, 0, 0}, s_lastMag[3] = {0, 0, 0};
static float s_mMin[3], s_mMax[3];
static volatile bool s_magCalOn = false;

// ---- attitude ----
static float q0 = 1, q1 = 0, q2 = 0, q3 = 0;
static float s_roll = 0, s_pitch = 0, s_yaw = 0;
static uint32_t s_warm = 0;

bool imuInit() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  Wire.setTimeOut(5);
  wr(MPU_ADDR, 0x6B, 0x80); delay(100);          // reset
  wr(MPU_ADDR, 0x6B, 0x01); delay(20);           // clock = PLL
  uint8_t id = 0;
  if (!rd(MPU_ADDR, 0x75, &id, 1) || (id != 0x71 && id != 0x73 && id != 0x70)) {
    Serial.printf("IMU: WHO_AM_I failed (0x%02X)\n", id);
    return false;
  }
  wr(MPU_ADDR, 0x6A, 0x00);                      // I2C master off
  wr(MPU_ADDR, 0x1A, 0x00);                      // gyro DLPF_CFG=0 -> 250 Hz BW, 8 kHz internal rate
  wr(MPU_ADDR, 0x19, 7);                         // SMPLRT_DIV=7 -> 1 kHz output
  wr(MPU_ADDR, 0x1B, 0x18);                      // gyro +-2000 dps (16.4 LSB/dps)
  wr(MPU_ADDR, 0x1C, 0x10);                      // accel +-8 g (4096 LSB/g)
  wr(MPU_ADDR, 0x1D, 0x01);                      // accel DLPF 184 Hz
  wr(MPU_ADDR, 0x37, 0x02);                      // bypass: AK8963 visible on the main bus
  delay(10);

  uint8_t who = 0;
  if (rd(AK_ADDR, 0x00, &who, 1) && who == 0x48) {
    uint8_t asa[3];
    wr(AK_ADDR, 0x0A, 0x00); delay(10);
    wr(AK_ADDR, 0x0A, 0x0F); delay(10);          // fuse ROM
    rd(AK_ADDR, 0x10, asa, 3);
    for (int i = 0; i < 3; i++) s_akAdj[i] = (asa[i] - 128) / 256.0f + 1.0f;
    wr(AK_ADDR, 0x0A, 0x00); delay(10);
    wr(AK_ADDR, 0x0A, 0x16); delay(10);          // 16-bit, continuous mode 2 (100 Hz)
    s_akOk = true;
  } else {
    Serial.println("IMU: AK8963 not found (mag disabled)");
  }
  imuGyroCalStart();
  return true;
}

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

bool imuRead(ImuData& d, bool wantMag) {
  uint8_t b[14];
  if (!rd(MPU_ADDR, 0x3B, b, 14)) return false;
  auto s16 = [&](int i) { return (float)(int16_t)((b[i] << 8) | b[i + 1]); };
  float acc[3] = {s16(0) / 4096.0f, s16(2) / 4096.0f, s16(4) / 4096.0f};
  float gyr[3] = {s16(8) / 16.4f, s16(10) / 16.4f, s16(12) / 16.4f};
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
      if (rd(AK_ADDR, 0x02, m, 8) && (m[0] & 1) && !(m[7] & 0x08)) {
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
