#include "wifi_server.h"
#include "imu.h"
#include "web_ui.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <ESPAsyncWebServer.h>

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static FcConfig s_cfg;                         // master copy edited by the UI (Core 0 only)
static volatile uint32_t s_rebootAt = 0;
static bool s_otaOk = false;

static bool isArmed() { Telemetry t; telGet(t); return t.armed; }

static void sendCfg(AsyncWebSocketClient* c) {
  float a[CFG_N]; int n = cfgToArray(s_cfg, a);
  char buf[640]; int p = snprintf(buf, sizeof buf, "{\"cfg\":[");
  for (int i = 0; i < n; i++) p += snprintf(buf + p, sizeof buf - p, "%s%.4g", i ? "," : "", a[i]);
  snprintf(buf + p, sizeof buf - p, "]}");
  c->text(buf);
}

static void handleCmd(AsyncWebSocketClient* c, char* m) {
  switch (m[0]) {
    case 'C': sendCfg(c); break;
    case 'U': {                                              // U v0 v1 ... v31  (live parameter update)
      float a[CFG_N]; int n = 0; char* p = m + 1; char* e;
      while (n < CFG_N) { float v = strtof(p, &e); if (e == p) break; a[n++] = v; p = e; }
      if (n == CFG_N) { cfgFromArray(s_cfg, a); cfgPublish(s_cfg); }
      break;
    }
    case 'V':                                                // save to NVS (disarmed only: flash writes stall the cores)
      if (isArmed()) { c->text("{\"err\":\"Disarm before saving\"}"); break; }
      g_busy = true; delay(2);
      if (!isArmed() && cfgSave(s_cfg)) c->text("{\"ack\":\"Saved to NVS\"}"); else c->text("{\"err\":\"Save failed\"}");
      g_busy = false;
      break;
    case 'K': if (!isArmed()) { g_cmdCalGyro = 1; c->text("{\"ack\":\"Gyro calibration started - keep still\"}"); } break;
    case 'G':                                                // magnetometer hard-iron calibration: G1 start, G0 stop
      if (m[1] == '1') { g_cmdMagCal = 1; c->text("{\"ack\":\"Rotate the board through all orientations, then stop\"}"); }
      else { imuMagCalFinish(s_cfg.magOff); cfgPublish(s_cfg); c->text("{\"ack\":\"Mag offsets updated (not saved yet)\"}"); }
      break;
    case 'E':                                                // motor test enable
      if (isArmed()) break;
      g_mtEnable = (m[1] == '1' || m[2] == '1');
      if (!g_mtEnable) for (int i = 0; i < 4; i++) g_mtVal[i] = 0;
      break;
    case 'M': {                                              // M <motor 0-3> <permille>
      int i = 0, v = 0;
      if (sscanf(m + 1, "%d %d", &i, &v) == 2 && i >= 0 && i < 4 && g_mtEnable && !isArmed()) {
        g_mtVal[i] = (uint16_t)constrain(v, 0, MOTOR_TEST_MAX_PERMILLE);
        g_mtStamp = millis();
      }
      break;
    }
    case 'D': g_cmdDisarm = 1; break;
    case 'R': if (!isArmed()) s_rebootAt = millis() + 300; break;
  }
}

static void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* c, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) { sendCfg(c); return; }
  if (type != WS_EVT_DATA) return;
  AwsFrameInfo* i = (AwsFrameInfo*)arg;
  if (!(i->final && i->index == 0 && i->len == len && i->opcode == WS_TEXT)) return;
  char buf[640]; size_t n = len < sizeof buf - 1 ? len : sizeof buf - 1;
  memcpy(buf, data, n); buf[n] = 0;
  handleCmd(c, buf);
}

static void commTask(void*) {
  uint32_t lastClean = 0;
  char buf[512];
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(25));                           // 40 Hz telemetry
    if (ws.count() && ws.availableForWriteAll()) {           // drop frames rather than queue them
      Telemetry t; telGet(t);
      snprintf(buf, sizeof buf,
        "{\"t\":[%.1f,%.1f,%.1f],\"g\":[%.0f,%.0f,%.0f],\"o\":[%.3f,%.3f,%.3f],\"m\":[%.2f,%.2f,%.2f,%.2f],"
        "\"rc\":[%u,%u,%u,%u,%u,%u,%u,%u],\"s\":[%u,%u,%u,%u,%u,%u,%u,%u,%u]}",
        t.roll, t.pitch, t.yaw, t.gyro[0], t.gyro[1], t.gyro[2], t.pid[0], t.pid[1], t.pid[2],
        t.motor[0], t.motor[1], t.motor[2], t.motor[3],
        t.rc[0], t.rc[1], t.rc[2], t.rc[3], t.rc[4], t.rc[5], t.rc[6], t.rc[7],
        t.armed, t.mode, t.rcSrc, t.disarmReason, t.preArm, t.gyroCal, t.magCal, t.loopAvgUs, t.loopMaxUs);
      ws.textAll(buf);
    }
    if (millis() - lastClean > 1000) { ws.cleanupClients(); lastClean = millis(); }
    if (s_rebootAt && (int32_t)(millis() - s_rebootAt) > 0) ESP.restart();
  }
}

void wifiServerInit(const FcConfig& cfg) {
  s_cfg = cfg;

  Preferences np; np.begin("net", true);
  String ssid = np.getString("ssid", ""), pass = np.getString("pass", "");
  np.end();

  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);                                      // lowest latency
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, 6, 0, 2);
  WiFi.setAutoReconnect(true);
  if (ssid.length()) WiFi.begin(ssid.c_str(), pass.c_str());
  MDNS.begin("esp32fc");

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) { r->send_P(200, "text/html", INDEX_HTML); });

  server.on("/api/net", HTTP_GET, [](AsyncWebServerRequest* r) {
    char b[256];
    snprintf(b, sizeof b, "{\"ap\":\"%s\",\"apip\":\"%s\",\"ssid\":\"%s\",\"sta\":%d,\"ip\":\"%s\",\"rssi\":%d}",
             WIFI_AP_SSID, WiFi.softAPIP().toString().c_str(), WiFi.SSID().c_str(),
             WiFi.status() == WL_CONNECTED, WiFi.localIP().toString().c_str(), WiFi.RSSI());
    r->send(200, "application/json", b);
  });

  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!r->hasParam("ssid", true)) { r->send(400, "text/plain", "ssid missing"); return; }
    String s = r->getParam("ssid", true)->value();
    String p = r->hasParam("pass", true) ? r->getParam("pass", true)->value() : String();
    Preferences np; np.begin("net", false); np.putString("ssid", s); np.putString("pass", p); np.end();
    if (s.length()) WiFi.begin(s.c_str(), p.c_str()); else WiFi.disconnect();
    r->send(200, "text/plain", "Wi-Fi settings saved");
  });

  // OTA: multipart upload of the firmware .bin. Refused while armed.
  server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest* r) {
      bool ok = s_otaOk && !Update.hasError();
      g_busy = false;
      AsyncWebServerResponse* resp = r->beginResponse(ok ? 200 : 409, "text/plain", ok ? "Update OK - rebooting" : "Update refused/failed (disarm first)");
      resp->addHeader("Connection", "close");
      r->send(resp);
      if (ok) s_rebootAt = millis() + 800;
    },
    [](AsyncWebServerRequest*, String, size_t index, uint8_t* data, size_t len, bool final) {
      if (index == 0) {
        s_otaOk = false;
        if (isArmed()) return;
        g_busy = true;
        s_otaOk = Update.begin(UPDATE_SIZE_UNKNOWN);
      }
      if (s_otaOk && Update.isRunning()) { if (Update.write(data, len) != len) s_otaOk = false; }
      if (final && s_otaOk) s_otaOk = Update.end(true);
    });

  server.onNotFound([](AsyncWebServerRequest* r) { r->send(404, "text/plain", "not found"); });
  server.begin();

  xTaskCreatePinnedToCore(commTask, "comm", 6144, nullptr, COMM_TASK_PRIO, nullptr, COMM_CORE);
  Serial.printf("AP \"%s\"  http://%s/\n", WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
}
