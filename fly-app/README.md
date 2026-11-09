# FC Fly (Android)

Full-screen landscape wrapper around the board's `/fly` page (dual sticks, horizon HUD, slide-to-take-off). The page is served by the firmware, so the app always matches what you flashed.

**Dev build, untested on a phone yet.** It is a debug-signed APK (install with "install unknown apps" allowed).

What the wrapper adds over a browser tab:
- binds the app to the Wi-Fi network, so Android does not drop or bypass a hotspot that has no internet
- keeps the screen on, hides the system bars, locks to the board's address
- connect screen that remembers the address and probes `GET /api/net` first; Back returns to it, and it also reappears if the link drops

**Keep the app in the foreground while flying.** The FC lands the drone when no control packets arrive for ~0.5 s, and a backgrounded or locked phone stops sending.

## Get the APK
- **GitHub:** Actions -> *Build fly app (APK)* -> latest run -> download `fc-fly-debug-apk`, unzip, install the `.apk` on the phone.
- **Locally:** open `fly-app/` in Android Studio, or run `gradle assembleDebug` (JDK 17, Gradle 8.7, Android SDK 34).

## Not included (yet)
Bluetooth/USB gamepad input and the UDP joystick link (port 8888); the page uses its WebSocket like the browser version does.
