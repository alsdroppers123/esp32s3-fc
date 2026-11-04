// imu.h - multi-vendor IMU driver (I2C or SPI, auto-detected) + Mahony attitude estimator
#pragma once
#include "config.h"

enum ImuType : uint8_t {
  IMU_NONE = 0,
  IMU_MPU6050, IMU_MPU6000, IMU_MPU6500, IMU_MPU9250, IMU_MPU6886,
  IMU_ICM20602, IMU_ICM20608, IMU_ICM20689, IMU_ICM20948,
  IMU_ICM42688, IMU_ICM42605,
  IMU_BMI160, IMU_BMI088,
  IMU_LSM6DS3, IMU_LSM6DSL, IMU_LSM6DSO, IMU_LSM6DSR,
  IMU_QMI8658,
  IMU_TYPE_COUNT
};
extern const char* const IMU_NAMES[IMU_TYPE_COUNT];

struct ImuInfo {
  uint8_t type;            // ImuType
  uint8_t bus;             // BUS_I2C / BUS_SPI (0 = not found)
  uint8_t addr;            // I2C address (gyro address for BMI088)
  uint8_t whoami;
  bool    mag;             // AK8963 magnetometer found (MPU-9250 on I2C)
  uint8_t scan[16];        // I2C addresses that answered during detection
  uint8_t scanN;
};
extern ImuInfo g_imuInfo;

struct ImuData {
  float rate[3];     // deg/s in flight convention: roll right+, pitch nose-up+, yaw clockwise+ (bias removed)
  float gyroFlu[3];  // deg/s, FLU body frame (bias removed)
  float accFlu[3];   // g, FLU body frame (flat -> z = +1)
  float magFlu[3];   // uT, FLU body frame (hard-iron corrected)
  bool  magFresh;    // true on the iteration a new magnetometer sample arrived
};

bool imuInit(const HwConfig& hw);       // probes the configured bus(es), configures the first sensor found
bool imuRead(ImuData& d, bool wantMag);
void imuUpdateAttitude(const ImuData& d, float dt, float kp, bool useMag);
void imuGetAttitude(float& rollDeg, float& pitchDeg, float& yawDeg);
bool imuGyroCalibrated();
void imuGyroCalStart();
void imuMagCalStart();
bool imuMagCalActive();
void imuMagCalFinish(float off[3]);
void imuSetMagOffset(const float off[3]);
