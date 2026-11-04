// imu.h - MPU-9250 driver (I2C) + Mahony attitude estimator
#pragma once
#include "config.h"

struct ImuData {
  float rate[3];     // deg/s in flight convention: roll right+, pitch nose-up+, yaw clockwise+ (bias removed)
  float gyroFlu[3];  // deg/s, FLU body frame (bias removed)
  float accFlu[3];   // g, FLU body frame (flat -> z = +1)
  float magFlu[3];   // uT, FLU body frame (hard-iron corrected)
  bool  magFresh;    // true on the iteration a new magnetometer sample arrived
};

bool imuInit();
bool imuRead(ImuData& d, bool wantMag);
void imuUpdateAttitude(const ImuData& d, float dt, float kp, bool useMag);
void imuGetAttitude(float& rollDeg, float& pitchDeg, float& yawDeg);
bool imuGyroCalibrated();
void imuGyroCalStart();
void imuMagCalStart();
bool imuMagCalActive();
void imuMagCalFinish(float off[3]);
void imuSetMagOffset(const float off[3]);
