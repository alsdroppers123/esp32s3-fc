# ESP32-S3 Betaflight-inspired Flight Controller

Arduino-ESP32 2.0.x / PlatformIO. **Not flight-tested.** Compiles cleanly against arduino-esp32 2.0.17 (the core inside PlatformIO espressif32 6.9.0) for every env. Treat as a bring-up baseline.

One binary runs on any ESP32-S3 board. Pick an env (`zero` is the default) only to choose the first-boot pin preset; after that every pin is re-mappable from the web UI.

## Layout
| File | Role |
|---|---|
| `config.h/.cpp` | limits, `FcConfig`, NVS load/save, cross-core shared state |
| `hw.h/.cpp` | board presets, runtime pin map (NVS), validation, status LED (WS2812 or plain), buzzer, battery ADC |
| `imu.h/.cpp` | multi-IMU driver (I2C or SPI, auto-detect, incl. BNO085 sensor hub), orientation, gyro calibration, Mahony AHRS |
| `pid.h/.cpp` | PT1/biquad filters, BF rates, PID + FF + anti-windup |
| `mixer.h/.cpp` | Quad-X mixer (airmode-style desaturation), DShot150/300 (RMT) or 400 Hz PWM (LEDC) |
| `rc.h/.cpp` | CRSF parser (UART1 420 kbaud), web joystick, UDP joystick. Priority CRSF > web > UDP |
| `wifi_server.h/.cpp` | AP+STA, async HTTP, WebSocket (40 Hz telemetry), OTA, mDNS |
| `web_ui.h` | configurator at `/` (setup, PID, receiver, motors, **Pins & IMU**, Wi-Fi/OTA) |
| `fly_ui.h` | touch flight page at `/fly` (dual sticks, HUD, slide-to-take-off) |
| `main.cpp` | boot + flight task |

## Tasks
- **Core 1, prio 24 `flight`**: IMU read -> Mahony -> failsafe/arming -> setpoints -> PID -> mixer -> motors, every 1 ms.
- **Core 0, prio 5**: `comm` (telemetry, reboot), `crsf`; Wi-Fi/lwIP/AsyncTCP also on Core 0.
- Config edits go Core0 -> Core1 through a spinlock-protected shadow copy; telemetry goes back the same way at 100 Hz.

## Conventions
- Body frame FLU internally; flight convention: roll right-down +, pitch nose-up +, yaw clockwise +.
- Motors (Betaflight order, props-in): M1 rear-right CCW, M2 front-right CW, M3 rear-left CW, M4 front-left CCW. GPIOs come from the pin map.
- Channels AETR + AUX1..4: arm = AUX1 > 1700, mode = AUX2 (low Angle, mid Horizon, high Acro). Elevator high = stick forward.
- Mounting: set **Sensor orientation** in the Pins & IMU tab (0/90/180/270 deg clockwise, optionally flipped). Default = chip X to the nose, chip Z up.

## Safety logic (flight task)
Arm requires: gyro calibrated, |roll|,|pitch| < 15 deg, throttle < 5 %, valid RC, arm switch seen low since boot/last disarm, no motor test/OTA/save active.
Failsafe: RC silent > 500 ms -> **failsafe landing** (Angle mode, sticks centred, 85 % of the last throttle for `FS_LAND_MS` = 4 s, then disarm; control returns if the link comes back). Set `FS_LAND_MS 0` to drop instead.
Disarm on: arm switch low, failsafe landing finished, |roll| or |pitch| > 70 deg, IMU read failing 5x, or web DISARM. Saving, OTA and reboot are refused while armed.

## Supported boards (default pin presets)
| Board (env) | I2C SDA/SCL | Motors M1-M4 | CRSF RX/TX | LED | VBAT |
|---|---|---|---|---|---|
| Waveshare ESP32-S3-Zero (`zero`) | 4 / 5 | 7, 8, 9, 10 | 44 / 43 | 21 WS2812 | 1 |
| ESP32-S3 Super Mini (`supermini`) | 4 / 5 | 7, 8, 9, 10 | 44 / 43 | 48 WS2812 | 1 |
| Seeed XIAO ESP32S3 (`xiao`) | 5 / 6 (D4/D5) | 1, 2, 3, 4 (D0-D3) | 44 / 43 (D7/D6) | 21 (active low) | - |
| ESP32-S3-DevKitC-1 (`devkitc`) | 9 / 10 | 4, 5, 6, 7 | 18 / 17 | 48 WS2812 (v1.1: 38) | 1 |
| Generic ESP32-S3 (`generic`) | 9 / 10 | 4, 5, 6, 7 | 18 / 17 | - | 1 |

SPI IMUs: assign SCK, MISO, MOSI and CS (plus CS2 for a BMI088 accel) in the Pins tab. Usable GPIOs: 0-18, 21, 33-48 (19/20 are USB, 26-32 flash). Battery sense must be on ADC1 (GPIO1-10).
DevKitC note: GPIO43/44 go to the on-board USB-UART chip, so its preset puts the receiver on 17/18.

## Supported IMUs (auto-detected, I2C 0x68/0x69/0x6A/0x6B or SPI)
MPU-6050 / 6000 / 6500 / 9250 (+AK8963 mag on I2C) / 6886, ICM-20602 / 20608 / 20689 / 20948, ICM-42688-P / 42605, BMI160, BMI088, LSM6DS3 / DSL / DS3TR-C / DSO / DSR / ISM330, QMI8658, **BNO080 / BNO085 / BNO086 (I2C, with magnetometer)**.
All except the BNO08x are set to gyro +-2000 dps and accel +-8 g (BMI088 +-12 g) at about 1 kHz or more. If a clone reports an odd WHO_AM_I, force the type in **IMU sensor**. BMI270 is not supported (it needs an 8 kB config blob upload). The detected chip and an I2C scan are shown at the top of the Pins tab.

## BNO085 (BNO08x sensor hub) + magnetometer
**New and untested on hardware** (host-tested against a simulated hub only). I2C only, address 0x4A (default) or 0x4B; wire 3V3, GND, SDA, SCL (INT/RST/PS pins are not used - the driver polls and soft-resets; leave PS0/PS1 low for I2C). Auto-detect probes it last, after all register-based IMUs, so pick **BNO080/085/086** in **IMU sensor** to skip the other probes. It is shown with an *on-chip magnetometer* tag in the Pins tab.
- The hub does its own calibration. The driver reads its calibrated **gyro (400 Hz)**, **accel (100 Hz)** and **magnetic field (50 Hz)** reports and feeds the usual Mahony AHRS, so **Mag fusion** and the G1/G0 hard-iron routine work as with the MPU-9250. Between reports the last sample is held. Intervals: `BNO_GYRO_US`, `BNO_ACC_US`, `BNO_MAG_US` in `imu.cpp` (overridable with `-D`).
- **Mag fusion only uses samples once the hub reports medium/high magnetometer accuracy** (>= 2). Until then yaw behaves as with mag fusion off. Calibrate it the way the BNO08x expects: a few slow figure-8 rotations, then keep still. The manual G1/G0 min/max routine still records during accuracy 0-1.
- **Axes**: reports use the chip frame printed on the breakout (on Adafruit/SparkFun boards X right, Y toward the top edge, Z up). With the board's Y towards the nose use **Sensor orientation = 90 deg**; check by tilting as in the bring-up steps.
- If no gyro report arrives for 20 ms `imuRead` fails and the usual 5-failure disarm applies. If the hub restarts, the reports are re-enabled automatically.
- Limits: ~400 Hz gyro, I2C polling (use 400 kHz, not 1 MHz) and the hub's own filtering make it fine for Angle/Horizon but a poor choice for aggressive Acro. The BNO08x stretches the I2C clock; on a flaky bus try 100 kHz first. SPI/UART-RVC modes and the hub's own rotation-vector output are not used.

## Pins & IMU tab
Shows the selected board's headers pin by pin. Each GPIO has a function picker (IMU bus, M1-M4, CRSF RX/TX, LED, buzzer, battery). It flags duplicate pins, strapping/PSRAM/UART0 pins and incomplete buses. Save stores to NVS (disarmed only); reboot to apply. **Recovery:** hold BOOT (GPIO0) for 5 s while disarmed to restore the board's default pin map.

Status LED: green breathing = ready, amber blip = not ready, blue blink = calibrating gyro, red = armed (orange flashes = low battery), fast red = no IMU.

## Web joystick (`/fly`)
Open `http://192.168.4.1/fly` on a phone in landscape (the **FLY** button in the configurator). Inspired by the Tello app: full-screen artificial horizon, dual floating sticks (Mode 2 or Mode 1), slide-to-take-off, one-tap land, STOP button, flight mode (Angle/Horizon/Acro), slow/fast speed, gyro calibrate, battery and link strength.
- **Centred throttle** (default): the stick moves throttle +-range around the hover throttle you set. Take-off spools up to hover; land ramps down over the landing time, then disarms. **There is no altitude hold**, so set the hover value by test.
- **Manual throttle**: the throttle stick stays where you leave it, like a radio.
- The page sends `J roll pitch thr yaw aux1..aux4` (microseconds) over the WebSocket at 50 Hz. If the page closes or Wi-Fi drops for 0.5 s, failsafe landing starts. A CRSF receiver always has priority.

## PPM receiver
In **Pins & IMU** set **Receiver protocol = PPM** and wire the receiver's PPM/CPPM output to the pin you've set as **Receiver in** (Zero default GPIO44). Save and reboot. Up to 12 channels are decoded and the first 8 are used. Channels a receiver doesn't send read as 1000 us, so arm stays low and the mode is Angle. Normal and inverted PPM both work.
- **Channel order**: pick AETR (FrSky/Futaba) or TAER (Spektrum/JR and many PPM receivers), or AERT/ETAR. It applies to CRSF too. Check the result in the Receiver tab.
- **Failsafe**: set the receiver's own failsafe to *no pulses*. A receiver set to "hold" keeps sending the last frame, and the FC can't tell the link is gone.
- **Voltage**: ESP32-S3 pins are 3.3 V only. If the receiver outputs 5 V PPM, use a divider (e.g. 1k series + 2k to GND) or a level shifter.
- Telemetry shows the source as `PPM` (RC source 4). Priority is hardware receiver > web joystick > UDP.

## Router mode (wider range)
In **Wi-Fi & OTA**, press **Scan**, pick your router, enter the password, then **Save & connect**. The FC joins the router (AP+STA) at full TX power and shows its address (`http://<ip>/` or `http://esp32fc.local/`). Put the phone on the same router and open that address (or `/fly`). The range is then your router's coverage instead of the small onboard hotspot.
- Optional **hotspot off while connected**: once the router link has been up for 8 s and nobody is on the hotspot, the FC hotspot turns off. It turns back on by itself if the router is lost for 10 s, so the FC never becomes unreachable.
- While connected, the hotspot follows the router's channel (the ESP32 has one radio), so hotspot clients may reconnect once.
- Scans and Wi-Fi changes are refused while armed. Wi-Fi drops still trigger the 0.5 s failsafe landing; for long range use an ELRS receiver.
- Serial monitor prints the router IP at boot.

## UDP virtual joystick (port 8888)
19 bytes: `'R','C'`, 8 x uint16 little-endian microseconds, 1 byte = sum of the 16 payload bytes mod 256. CRSF has priority while fresh. You must still send an arm channel (index 4).
```python
import socket,struct
ch=[1500,1500,1000,1500,1000,1000,1000,1000]; p=struct.pack('<8H',*ch)
socket.socket(socket.AF_INET,socket.SOCK_DGRAM).sendto(b'RC'+p+bytes([sum(p)&255]),('192.168.4.1',8888))
```

## WebSocket protocol (text)
`C` get config | `U v0..v31` live update | `V` save NVS | `K` gyro cal | `G1`/`G0` mag cal | `E1`/`E0` motor test | `M i permille` | `J ch0..ch7` web joystick | `D` disarm | `R` reboot.
Server pushes `{"cfg":[...]}` and 40 Hz `{"t","g","o","m","rc","s","b"}` (`b` = battery cV, link RSSI, IMU type, failsafe).
HTTP: `GET /api/net`, `GET /api/scan` (poll until `nets`), `POST /api/wifi` (ssid, pass, apoff, keep), `GET /api/hw[?def=board]`, `POST /api/hw` (pin map, form fields named as in `HW_FN_KEYS`, -1 = unused), `POST /api/reboot`. Config index order is in `cfgToArray()`.

## First bring-up (PROPS OFF)
1. Flash the env for your board (`pio run -e zero -t upload`), join Wi-Fi `ESP32S3-FC` (password in `config.h`, change it), open `http://192.168.4.1/`.
2. Open **Pins & IMU** and check the IMU was detected. If not, fix the bus pins, save and reboot. Keep the board still ~2 s (gyro cal). Tilt it: the model must follow, roll right = right side down, nose up = positive pitch. Change **Sensor orientation** otherwise.
3. Receiver tab: confirm channel order/direction. Motors tab: verify each motor number and spin direction.
4. In Angle mode tilt the board and confirm the correct motors speed up (the Motors bars on the Motors tab).
5. Only then fit props, tether, and tune from low gains. Defaults are placeholders, not a tune.

## Known limits / next steps
- 1 kHz loop (FreeRTOS tick). 2 kHz+ needs a hardware-timer/IMU-INT notify and SPI IMU.
- I2C read costs ~0.4 ms of each 1 ms; SPI is strongly recommended for serious use.
- Flash writes (NVS/OTA) stall both cores; hence disarmed-only.
- Not implemented: RPM/bidirectional DShot, I-term relax, TPA, battery sensing, blackbox, CRSF telemetry, web auth.
- Libraries: if `ESP32Async/*` versions do not resolve, loosen the pins; `availableForWriteAll()` may need replacing with a client-count check on old forks.
