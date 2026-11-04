// main.cpp - boot sequence and the hard-real-time flight loop (Core 1, priority 24)
#include <Arduino.h>
#include "config.h"
#include "imu.h"
#include "pid.h"
#include "mixer.h"
#include "rc.h"
#include "wifi_server.h"

static inline float stickNorm(uint16_t us) {
  float v = ((float)us - 1500.0f) * (1.0f / 500.0f);
  if (fabsf(v) < 0.01f) return 0;
  return constrain(v, -1.0f, 1.0f);
}

static void flightTask(void*) {
  FcConfig cfg; cfgSnapshot(cfg);
  pidConfigure(cfg); imuSetMagOffset(cfg.magOff);

  Telemetry T; memset(&T, 0, sizeof T);
  bool armed = false, armLow = false, sat = false;
  uint8_t dr = DR_NONE, imuFail = 0, mode = MODE_ANGLE;
  float motors[4] = {0}, gyroF[3] = {0}, pidOut[3] = {0};
  uint16_t rc[8] = {0};
  uint32_t lastUs = micros(), work = 0, workSum = 0, workMax = 0, n = 0;
  TickType_t wake = xTaskGetTickCount();

  auto disarm = [&](uint8_t why) {
    armed = false; armLow = false; dr = why; sat = false;
    motorsStop(); for (int i = 0; i < 4; i++) motors[i] = 0;
  };

  for (;;) {
    vTaskDelayUntil(&wake, 1);                       // 1 kHz tick
    uint32_t t0 = micros();
    float dt = constrain((t0 - lastUs) * 1e-6f, 0.0005f, 0.003f);
    lastUs = t0;

    if (cfgConsume(cfg)) { pidConfigure(cfg); imuSetMagOffset(cfg.magOff); }
    if (g_cmdCalGyro) { g_cmdCalGyro = 0; if (!armed) imuGyroCalStart(); }
    if (g_cmdMagCal == 1) { g_cmdMagCal = 0; imuMagCalStart(); }

    // ---------------- sensors + attitude ----------------
    ImuData d;
    if (!imuRead(d, cfg.magFusion || imuMagCalActive())) {
      if (++imuFail >= 5 && armed) disarm(DR_IMU);
      if (!armed) motorsStop();
      continue;
    }
    imuFail = 0;
    imuUpdateAttitude(d, dt, cfg.mahonyKp, cfg.magFusion);
    float roll, pitch, yaw; imuGetAttitude(roll, pitch, yaw);

    // ---------------- receiver ----------------
    uint8_t src;
    bool rcOk = rcGet(rc, &src);
    float thr = rcOk ? constrain(((float)rc[CH_THR] - 1000.0f) * 0.001f, 0.0f, 1.0f) : 0.0f;
    bool armHi = rcOk && rc[CH_ARM] > 1700;
    if (rcOk && rc[CH_ARM] < 1300) armLow = true;    // switch must be seen LOW before arming is allowed

    uint16_t pa = 0;
    if (imuGyroCalibrated())                                     pa |= PA_GYRO;
    if (fabsf(roll) < PREARM_LEVEL_DEG && fabsf(pitch) < PREARM_LEVEL_DEG) pa |= PA_LEVEL;
    if (thr < ARM_THR_MAX)                                       pa |= PA_THR;
    if (rcOk)                                                    pa |= PA_RC;
    if (armLow)                                                  pa |= PA_SWITCH;
    if (!g_mtEnable && !g_busy)                                  pa |= PA_IDLE;

    if (armed) {
      if (g_cmdDisarm)                                disarm(DR_CMD);
      else if (!rcOk)                                 disarm(DR_RC_LOSS);        // failsafe: link lost > 500 ms
      else if (rc[CH_ARM] < 1500)                     disarm(DR_SWITCH);
      else if (fabsf(roll) > MAX_TILT_DEG || fabsf(pitch) > MAX_TILT_DEG) disarm(DR_TILT);
    } else if (armHi && pa == PA_ALL) {
      armed = true; dr = DR_NONE; pidReset();
    }
    g_cmdDisarm = 0;

    // ---------------- setpoints ----------------
    float stick[3] = {0, 0, 0};
    if (rcOk) {
      stick[0] = stickNorm(rc[CH_ROLL]);
      stick[1] = -stickNorm(rc[CH_PITCH]);           // stick forward (high) = nose down
      stick[2] = stickNorm(rc[CH_YAW]);
      mode = rc[CH_MODE] < 1300 ? MODE_ANGLE : (rc[CH_MODE] < 1700 ? MODE_HORIZON : MODE_ACRO);
    }
    float sp[3];
    for (int a = 0; a < 3; a++) sp[a] = rcApplyRates(stick[a], cfg.rcRate[a], cfg.superRate[a], cfg.expo[a]);
    if (mode != MODE_ACRO) {
      const float att[2] = {roll, pitch};
      float defl = fmaxf(fabsf(stick[0]), fabsf(stick[1]));
      for (int a = 0; a < 2; a++) {
        if (mode == MODE_ANGLE) {
          sp[a] = constrain((stick[a] * cfg.angleMax - att[a]) * cfg.angleStrength, -LEVEL_RATE_LIMIT, LEVEL_RATE_LIMIT);
        } else {                                      // HORIZON: self-level fades out with stick deflection
          float lv = constrain((0.0f - att[a]) * cfg.angleStrength, -LEVEL_RATE_LIMIT, LEVEL_RATE_LIMIT);
          sp[a] += (1.0f - defl) * lv;
        }
      }
    }

    // ---------------- PID + mixer + outputs ----------------
    pidUpdate(cfg, dt, sp, d.rate, armed && thr > 0.05f, sat, pidOut, gyroF);

    if (armed) {
      sat = mixerRun(thr, pidOut, cfg.airmode, cfg.idlePct, motors);
      motorsWrite(motors);
    } else if (g_mtEnable && (millis() - g_mtStamp) < MOTOR_TEST_TIMEOUT_MS) {
      for (int i = 0; i < 4; i++) motors[i] = g_mtVal[i] * 0.001f;
      motorsWrite(motors);
    } else {
      for (int i = 0; i < 4; i++) motors[i] = 0;
      motorsStop();
    }

    // ---------------- telemetry snapshot @ 100 Hz ----------------
    work = micros() - t0; workSum += work; if (work > workMax) workMax = work;
    if (++n >= 10) {
      T.roll = roll; T.pitch = pitch; T.yaw = yaw;
      for (int i = 0; i < 3; i++) { T.gyro[i] = gyroF[i]; T.pid[i] = pidOut[i]; }
      for (int i = 0; i < 4; i++) T.motor[i] = motors[i];
      for (int i = 0; i < 8; i++) T.rc[i] = rc[i];
      T.armed = armed; T.mode = mode; T.rcSrc = src; T.disarmReason = dr; T.preArm = pa;
      T.gyroCal = imuGyroCalibrated(); T.magCal = imuMagCalActive();
      T.loopAvgUs = workSum / n; T.loopMaxUs = workMax;
      telPublish(T);
      n = 0; workSum = 0; workMax = 0;
    }
  }
}

void setup() {
  Serial.begin(115200);
  FcConfig cfg; cfgLoad(cfg); cfgPublish(cfg);
  motorsInit(cfg.protocol);                          // outputs idle-low immediately

  if (imuInit()) {
    xTaskCreatePinnedToCore(flightTask, "flight", 8192, nullptr, FLIGHT_TASK_PRIO, nullptr, FLIGHT_CORE);
  } else {
    Serial.println("IMU init failed - flight task NOT started (motors stay off).");
  }
  wifiServerInit(cfg);                               // Core 0: Wi-Fi, HTTP, WebSocket, OTA
  rcInit();                                          // CRSF task + UDP joystick
  vTaskDelete(nullptr);                              // the Arduino loopTask is not needed
}

void loop() {}
