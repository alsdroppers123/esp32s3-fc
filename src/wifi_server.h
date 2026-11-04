// wifi_server.h - Wi-Fi AP/STA manager, async web server, WebSocket telemetry, OTA (all on Core 0)
#pragma once
#include "config.h"
void wifiServerInit(const FcConfig& cfg);
