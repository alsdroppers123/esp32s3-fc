# ESP32-S3 Betaflight-inspired Flight Controller

Arduino-ESP32 2.0.x / PlatformIO. **Not flight-tested. Not compiled against the real SDK** (only the pure-logic modules were host-compiled and sanity-tested). Treat as a bring-up baseline; expect to fix small API mismatches.

## Layout
| File | Role |
|---|---|
| `config.h/.cpp` | pins, limits, `FcConfig`, NVS load/save, cross-core shared state |
| `imu.h/.cpp` | MPU-9250 (I2C 400 kHz) + AK8963, gyro bias calibration, Mahony AHRS |
| `pid.h/.cpp` | PT1/biquad filters, BF rates, PID + FF + anti-windup |
| `mixer.h/.cpp` | Quad-X mixer (airmode-style desaturation), DShot150/300 (RMT) or 400 Hz PWM (LEDC) |
| `rc.h/.cpp` | CRSF parser (UART1 420 kbaud) + UDP virtual joystick |
| `wifi_server.h/.cpp` | AP+STA, async HTTP, WebSocket (40 Hz telemetry), OTA, mDNS |
| `web_ui.h` | single-file UI (canvas 3D model, no CDN, works offline in AP mode) |
| `main.cpp` | boot + flight task |

## Tasks
- **Core 1, prio 24 `flight`**: IMU read -> Mahony -> failsafe/arming -> setpoints -> PID -> mixer -> motors, every 1 ms.
- **Core 0, prio 5**: `comm` (telemetry, reboot), `crsf`; Wi-Fi/lwIP/AsyncTCP also on Core 0.
- Config edits go Core0 -> Core1 through a spinlock-protected shadow copy; telemetry goes back the same way at 100 Hz.

## Conventions
- Body frame FLU internally; flight convention: roll right-down +, pitch nose-up +, yaw clockwise +.
- Motors (Betaflight order, props-in): M1 rear-right CCW, M2 front-right CW, M3 rear-left CW, M4 front-left CCW. GPIO 4/5/6/7.
- Channels AETR + AUX1..4: arm = AUX1 > 1700, mode = AUX2 (low Angle, mid Horizon, high Acro). Elevator high = stick forward.
- Mounting: edit `IMU_*_AXIS/SIGN` in `config.h` if the chip is not X-forward, Y-left, Z-up.

## Safety logic (flight task)
Arm requires: gyro calibrated, |roll|,|pitch| < 15 deg, throttle < 5 %, valid RC, arm switch seen low since boot/last disarm, no motor test/OTA/save active.
Disarm on: arm switch low, RC silent > 500 ms, |roll| or |pitch| > 70 deg, IMU read failing 5x, or web DISARM. Saving, OTA and reboot are refused while armed.

## UDP virtual joystick (port 8888)
19 bytes: `'R','C'`, 8 x uint16 little-endian microseconds, 1 byte = sum of the 16 payload bytes mod 256. CRSF has priority while fresh. You must still send an arm channel (index 4).
```python
import socket,struct
ch=[1500,1500,1000,1500,1000,1000,1000,1000]; p=struct.pack('<8H',*ch)
socket.socket(socket.AF_INET,socket.SOCK_DGRAM).sendto(b'RC'+p+bytes([sum(p)&255]),('192.168.4.1',8888))
```

## WebSocket protocol (text)
`C` get config | `U v0..v31` live update | `V` save NVS | `K` gyro cal | `G1`/`G0` mag cal | `E1`/`E0` motor test | `M i permille` | `D` disarm | `R` reboot.
Server pushes `{"cfg":[...]}` and 40 Hz `{"t","g","o","m","rc","s"}`. Config index order is in `cfgToArray()`.

## First bring-up (PROPS OFF)
1. Flash, join Wi-Fi `ESP32S3-FC` (password in `config.h`, change it), open `http://192.168.4.1/`.
2. Keep the board still ~2 s (gyro cal). Tilt it: the model must follow, roll right = right side down, nose up = positive pitch. Fix `IMU_*` otherwise.
3. Receiver tab: confirm channel order/direction. Motors tab: verify each motor number and spin direction.
4. In Angle mode tilt the board and confirm the correct motors speed up (the Motors bars on the Motors tab).
5. Only then fit props, tether, and tune from low gains. Defaults are placeholders, not a tune.

## Known limits / next steps
- 1 kHz loop (FreeRTOS tick). 2 kHz+ needs a hardware-timer/IMU-INT notify and SPI IMU.
- I2C read costs ~0.4 ms of each 1 ms; SPI is strongly recommended for serious use.
- Flash writes (NVS/OTA) stall both cores; hence disarmed-only.
- Not implemented: RPM/bidirectional DShot, I-term relax, TPA, battery sensing, blackbox, CRSF telemetry, web auth.
- Libraries: if `ESP32Async/*` versions do not resolve, loosen the pins; `availableForWriteAll()` may need replacing with a client-count check on old forks.
