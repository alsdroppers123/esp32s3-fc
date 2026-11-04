#include "wifi_server.h"
#include "imu.h"
#include "web_ui.h"
#include "fly_ui.h"
#include "rc.h"
#include <esp_wifi.h>
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
static volatile uint32_t s_joyClient = 0;
// ---- router (station) link ----
static bool     s_apOff = false;                // user option: hotspot off while the router link is up
static bool     s_apOn = true;                  // current hotspot state
static uint32_t s_staUpSince = 0, s_staDownSince = 0;
static String   s_ssid;

static void apStart() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, 6, 0, 4);
  s_apOn = true;
}
// Called once a second from commTask: hotspot off after the router link is stable,
// hotspot back on as a fallback if the router is lost for 10 s (so the FC is never unreachable).
static void netManage() {
  const uint32_t now = millis();
  const bool up = WiFi.status() == WL_CONNECTED;
  if (up) { if (!s_staUpSince) s_staUpSince = now; s_staDownSince = 0; }
  else    { s_staUpSince = 0; if (!s_staDownSince) s_staDownSince = now; }
  if (s_apOff && s_apOn && up && now - s_staUpSince > 8000 && WiFi.softAPgetStationNum() == 0) {
    WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA); s_apOn = false;
    Serial.printf("Router link stable - hotspot off. FC at http://%s/\n", WiFi.localIP().toString().c_str());
  }
  if (!s_apOn && (!s_ssid.length() || (s_staDownSince && now - s_staDownSince > 10000))) {
    apStart(); Serial.println("Router lost - hotspot back on");
  }
}      // id of the WebSocket client currently flying with the joystick

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
    case 'J': {                                              // J roll pitch thr yaw aux1..aux4 (microseconds), ~50 Hz
      uint16_t ch[8]; int n = 0; char* p = m + 1; char* e;
      while (n < 8) { long v = strtol(p, &e, 10); if (e == p) break; ch[n++] = (uint16_t)v; p = e; }
      if (n == 8) { rcWebUpdate(ch); s_joyClient = c->id(); }
      break;
    }
    case 'D': g_cmdDisarm = 1; break;
    case 'R': if (!isArmed()) s_rebootAt = millis() + 300; break;
  }
}

static void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* c, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) { sendCfg(c); return; }
  if (type == WS_EVT_DISCONNECT) { if (c->id() == s_joyClient) { rcWebDrop(); s_joyClient = 0; } return; }
  if (type != WS_EVT_DATA) return;
  AwsFrameInfo* i = (AwsFrameInfo*)arg;
  if (!(i->final && i->index == 0 && i->len == len && i->opcode == WS_TEXT)) return;
  char buf[640]; size_t n = len < sizeof buf - 1 ? len : sizeof buf - 1;
  memcpy(buf, data, n); buf[n] = 0;
  handleCmd(c, buf);
}

static void commTask(void*) {
  uint32_t lastClean = 0;
  char buf[700];
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(25));                           // 40 Hz telemetry
    if (ws.count() && ws.availableForWriteAll()) {           // drop frames rather than queue them
      Telemetry t; telGet(t);
      snprintf(buf, sizeof buf,
        "{\"t\":[%.1f,%.1f,%.1f],\"g\":[%.0f,%.0f,%.0f],\"o\":[%.3f,%.3f,%.3f],\"m\":[%.2f,%.2f,%.2f,%.2f],"
        "\"rc\":[%u,%u,%u,%u,%u,%u,%u,%u],\"s\":[%u,%u,%u,%u,%u,%u,%u,%u,%u],\"b\":[%u,%d,%u,%u]}",
        t.roll, t.pitch, t.yaw, t.gyro[0], t.gyro[1], t.gyro[2], t.pid[0], t.pid[1], t.pid[2],
        t.motor[0], t.motor[1], t.motor[2], t.motor[3],
        t.rc[0], t.rc[1], t.rc[2], t.rc[3], t.rc[4], t.rc[5], t.rc[6], t.rc[7],
        t.armed, t.mode, t.rcSrc, t.disarmReason, t.preArm, t.gyroCal, t.magCal, t.loopAvgUs, t.loopMaxUs,
        g_vbatCv, (int)g_linkRssi, g_imuInfo.type, t.failsafe);
      ws.textAll(buf);
    }
    if (millis() - lastClean > 1000) {
      ws.cleanupClients(); lastClean = millis();
      wifi_sta_list_t sl; int best = 0;                      // signal of the phone/PC on our access point
      if (esp_wifi_ap_get_sta_list(&sl) == ESP_OK) for (int i = 0; i < sl.num; i++) if (!best || sl.sta[i].rssi > best) best = sl.sta[i].rssi;
      if (!best && WiFi.status() == WL_CONNECTED) best = WiFi.RSSI();
      g_linkRssi = (int8_t)best;
      netManage();
    }
    if (s_rebootAt && (int32_t)(millis() - s_rebootAt) > 0) ESP.restart();
  }
}

void wifiServerInit(const FcConfig& cfg) {
  s_cfg = cfg;

  Preferences np; np.begin("net", true);
  s_ssid = np.getString("ssid", ""); String pass = np.getString("pass", "");
  s_apOff = np.getBool("apoff", false);
  np.end();

  WiFi.persistent(false);                                    // we keep credentials in our own NVS keys
  WiFi.setHostname("esp32fc");
  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) {
    Serial.printf("Router: connected to \"%s\", FC at http://%s/  (http://esp32fc.local/)\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  apStart();
  WiFi.setSleep(false);                                      // lowest latency
  WiFi.setTxPower(WIFI_POWER_19_5dBm);                       // maximum range
  WiFi.setAutoReconnect(true);
  if (s_ssid.length()) WiFi.begin(s_ssid.c_str(), pass.c_str());
  if (MDNS.begin("esp32fc")) MDNS.addService("http", "tcp", 80);

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) { r->send_P(200, "text/html", INDEX_HTML); });

  server.on("/fly", HTTP_GET, [](AsyncWebServerRequest* r) { r->send_P(200, "text/html", FLY_HTML); });

  // ---- hardware / pin map -------------------------------------------------------------
  server.on("/api/hw", HTTP_GET, [](AsyncWebServerRequest* r) {
    HwConfig h = g_hw;
    bool def = r->hasParam("def");
    if (def) hwDefaults(h, (uint8_t)r->getParam("def")->value().toInt());
    else hwLoad(h);                                          // saved map (may differ from the running one until reboot)
    char b[900]; int k = snprintf(b, sizeof b, "{\"hw\":");
    k += hwToJson(h, b + k, sizeof b - k);
    k += snprintf(b + k, sizeof b - k, ",\"det\":{\"imu\":\"%s\",\"type\":%u,\"bus\":%u,\"addr\":%u,\"who\":%u,\"mag\":%d,\"scan\":[",
                  IMU_NAMES[g_imuInfo.type], g_imuInfo.type, g_imuInfo.bus, g_imuInfo.addr, g_imuInfo.whoami, g_imuInfo.mag);
    for (int i = 0; i < g_imuInfo.scanN; i++) k += snprintf(b + k, sizeof b - k, "%s%u", i ? "," : "", g_imuInfo.scan[i]);
    k += snprintf(b + k, sizeof b - k, "]},\"running\":%d,\"imus\":[", !def);
    for (int i = 0; i < IMU_TYPE_COUNT; i++) k += snprintf(b + k, sizeof b - k, "%s\"%s\"", i ? "," : "", IMU_NAMES[i]);
    snprintf(b + k, sizeof b - k, "]}");
    r->send(200, "application/json", b);
  });

  server.on("/api/hw", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (isArmed()) { r->send(409, "application/json", "{\"err\":\"Disarm first\"}"); return; }
    HwConfig h = g_hw;
    auto num = [&](const char* k, int def) { return r->hasParam(k, true) ? r->getParam(k, true)->value().toInt() : def; };
    h.board = num("board", h.board); h.imuBus = num("bus", h.imuBus); h.imuType = num("imu", h.imuType);
    h.align = num("align", h.align); h.ledType = num("ledt", h.ledType); h.buzInvert = num("buzinv", h.buzInvert) ? 1 : 0;
    h.i2cKhz = num("i2c", h.i2cKhz); h.spiMhz = num("spi", h.spiMhz);
    h.rxProto = num("rxp", h.rxProto); h.chOrder = num("cho", h.chOrder);
    if (r->hasParam("vscale", true)) h.vbatScale = r->getParam("vscale", true)->value().toFloat();
    if (h.imuType >= IMU_TYPE_COUNT) h.imuType = 0;
    for (int i = 0; i < F_COUNT; i++) { int v = num(HW_FN_KEYS[i], h.pin[i] == PIN_NONE ? -1 : h.pin[i]); h.pin[i] = (v < 0 || v > 48) ? PIN_NONE : (uint8_t)v; }
    char err[120], b[180];
    if (!hwValidate(h, err, sizeof err)) { snprintf(b, sizeof b, "{\"err\":\"%s\"}", err); r->send(400, "application/json", b); return; }
    g_busy = true; bool ok = hwSave(h); g_busy = false;
    r->send(ok ? 200 : 500, "application/json", ok ? "{\"ack\":\"Pin map saved - reboot to apply\"}" : "{\"err\":\"NVS write failed\"}");
  });

  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (isArmed()) { r->send(409, "text/plain", "Disarm first"); return; }
    s_rebootAt = millis() + 400; r->send(200, "text/plain", "Rebooting");
  });

  server.on("/api/net", HTTP_GET, [](AsyncWebServerRequest* r) {
    static const char* const ST[] = {"idle", "not found", "scan done", "connected", "wrong password or refused", "lost", "connecting"};
    int st = WiFi.status(); const char* sts = (st >= 0 && st <= 6) ? ST[st] : "connecting";
    if (st == WL_DISCONNECTED || st == WL_IDLE_STATUS) sts = s_ssid.length() ? "connecting" : "not configured";
    char b[420];
    snprintf(b, sizeof b, "{\"ap\":\"%s\",\"apon\":%d,\"apip\":\"%s\",\"apn\":%d,\"apoff\":%d,\"ssid\":\"%s\",\"sta\":%d,\"st\":\"%s\","
             "\"ip\":\"%s\",\"gw\":\"%s\",\"rssi\":%d,\"ch\":%d,\"host\":\"esp32fc.local\"}",
             WIFI_AP_SSID, s_apOn, WiFi.softAPIP().toString().c_str(), s_apOn ? WiFi.softAPgetStationNum() : 0, s_apOff,
             s_ssid.c_str(), st == WL_CONNECTED, sts, WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str(),
             st == WL_CONNECTED ? WiFi.RSSI() : 0, WiFi.channel());
    r->send(200, "application/json", b);
  });

  // Async scan: first call starts it and returns {"busy":1}; poll until the list arrives. Refused while armed
  // (a scan leaves the channel for ~2 s and would interrupt web/UDP control).
  server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (isArmed()) { r->send(409, "application/json", "{\"err\":\"Disarm before scanning\"}"); return; }
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) { WiFi.scanNetworks(true, false, false, 150); r->send(200, "application/json", "{\"busy\":1}"); return; }
    if (n == WIFI_SCAN_RUNNING) { r->send(200, "application/json", "{\"busy\":1}"); return; }
    String o = "{\"nets\":["; int k = 0;
    for (int i = 0; i < n && k < 24; i++) {
      String id = WiFi.SSID(i); if (!id.length()) continue;
      bool dup = false; for (int j = 0; j < i; j++) if (WiFi.SSID(j) == id) { dup = true; break; }   // list is sorted by RSSI
      if (dup) continue;
      id.replace("\\", "\\\\"); id.replace("\"", "\\\"");
      char e[40]; snprintf(e, sizeof e, "\",\"rssi\":%d,\"ch\":%d,\"lock\":%d}", WiFi.RSSI(i), WiFi.channel(i), WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
      o += (k++ ? ",{\"ssid\":\"" : "{\"ssid\":\""); o += id; o += e;
    }
    o += "]}"; WiFi.scanDelete();
    r->send(200, "application/json", o);
  });

  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (isArmed()) { r->send(409, "text/plain", "Disarm before changing Wi-Fi"); return; }
    if (!r->hasParam("ssid", true)) { r->send(400, "text/plain", "ssid missing"); return; }
    String s = r->getParam("ssid", true)->value();
    String p = r->hasParam("pass", true) ? r->getParam("pass", true)->value() : String();
    bool keepPass = r->hasParam("keep", true) && s == s_ssid;            // re-save options without retyping the password
    s_apOff = r->hasParam("apoff", true) && r->getParam("apoff", true)->value() == "1";
    Preferences np; np.begin("net", false);
    if (!keepPass) { np.putString("ssid", s); np.putString("pass", p); }
    else p = np.getString("pass", "");
    np.putBool("apoff", s_apOff); np.end();
    s_ssid = s;
    if (!s_apOn && (!s.length() || !s_apOff)) apStart();
    if (s.length()) { WiFi.disconnect(); WiFi.begin(s.c_str(), p.c_str()); }
    else WiFi.disconnect();
    r->send(200, "text/plain", s.length() ? "Connecting to " + s + "..." : "Router link removed");
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
