// config.h - pins, limits, configuration struct and state shared between the two cores
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>

// ============================ HARDWARE ============================
// Pins, IMU bus/type/orientation, LED and battery sensing are configured at runtime (hw.h, web "Pins" tab).
#include "hw.h"

// ============================ TIMING / SAFETY =====================
#define LOOP_HZ            1000      // flight loop rate (FreeRTOS tick = 1 kHz, see README)
#define RC_TIMEOUT_MS      500       // failsafe: no valid RC for this long -> failsafe landing
#define FS_LAND_MS         4000      // failsafe landing: level + throttle ramp to zero over this time, then disarm (0 = drop)
#define MAX_TILT_DEG       70.0f     // failsafe: |roll| or |pitch| beyond this -> disarm
#define PREARM_LEVEL_DEG   15.0f
#define ARM_THR_MAX        0.05f
#define LEVEL_RATE_LIMIT   400.0f    // deg/s cap of the angle-mode outer loop
#define MOTOR_TEST_MAX_PERMILLE 300  // motor-test slider limit (30 %)
#define MOTOR_TEST_TIMEOUT_MS   1000 // motor test stops if not refreshed

#define FLIGHT_CORE        1
#define FLIGHT_TASK_PRIO   24
#define COMM_CORE          0
#define COMM_TASK_PRIO     5

// ============================ NETWORK =============================
#define WIFI_AP_SSID  "ESP32S3-FC"
#define WIFI_AP_PASS  "flightctl123"   // >= 8 chars. CHANGE THIS.
#define UDP_RC_PORT   8888

// ============================ ENUMS ===============================
enum { CH_ROLL = 0, CH_PITCH, CH_THR, CH_YAW, CH_ARM, CH_MODE, CH_AUX3, CH_AUX4 };   // AETR + AUX1..4
enum { MODE_ACRO = 0, MODE_ANGLE = 1, MODE_HORIZON = 2 };
enum { DR_NONE = 0, DR_SWITCH, DR_RC_LOSS, DR_TILT, DR_IMU, DR_CMD, DR_FS_LAND };
enum { PA_GYRO = 1, PA_LEVEL = 2, PA_THR = 4, PA_RC = 8, PA_SWITCH = 16, PA_IDLE = 32, PA_ALL = 63 };
enum { RC_NONE = 0, RC_CRSF = 1, RC_UDP = 2, RC_WEB = 3, RC_PPM = 4 };
enum { PROTO_DSHOT300 = 0, PROTO_DSHOT150 = 1, PROTO_PWM = 2 };

// ============================ CONFIG ==============================
#define CFG_N     32
#define CFG_MAGIC 0xFC510003u

struct FcConfig {
  uint32_t magic;
  float pid[3][4];                 // [roll,pitch,yaw][P,I,D,FF]  (Betaflight-style units)
  float rcRate[3], superRate[3], expo[3];
  float gyroLpfHz, dtermLpfHz, rcSmoothHz;
  float angleStrength, angleMax, iTermLimit, idlePct, mahonyKp;
  float magOff[3];                 // hard-iron offsets (chip frame, uT)
  uint8_t airmode, magFusion, protocol, _pad;
};

void cfgDefaults(FcConfig& c);
void cfgLoad(FcConfig& c);
bool cfgSave(const FcConfig& c);
int  cfgToArray(const FcConfig& c, float* a);          // wire order used by the web UI (CFG_N values)
void cfgFromArray(FcConfig& c, const float* a);        // clamps every value

// Core0 -> Core1 hand-over of a new configuration (consumed at the top of the flight loop)
void cfgPublish(const FcConfig& c);
bool cfgConsume(FcConfig& out);
void cfgSnapshot(FcConfig& out);

// ============================ TELEMETRY ===========================
struct Telemetry {
  float roll, pitch, yaw;        // deg (roll right+, pitch nose-up+, yaw clockwise+)
  float gyro[3];                 // filtered, deg/s
  float pid[3];                  // normalised PID output
  float motor[4];                // 0..1 command
  uint16_t rc[8];                // microseconds
  uint8_t armed, mode, rcSrc, disarmReason, gyroCal, magCal, failsafe;
  uint16_t preArm, loopAvgUs, loopMaxUs;
};
void telPublish(const Telemetry& t);
void telGet(Telemetry& t);

// ============================ CROSS-CORE COMMANDS =================
extern volatile uint8_t  g_cmdCalGyro, g_cmdMagCal, g_cmdDisarm;
extern volatile bool     g_mtEnable;                 // motor-test enabled
extern volatile uint16_t g_mtVal[4];                 // permille
extern volatile uint32_t g_mtStamp;                  // millis() of last motor-test keep-alive
extern volatile bool     g_busy;                     // OTA / NVS write in progress -> no arming
extern portMUX_TYPE      g_mux;
