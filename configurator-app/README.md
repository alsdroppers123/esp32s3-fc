# ESP32-S3 FC Configurator (desktop)

A small Electron shell around the flight controller's built-in web configurator. It shows a connect screen, remembers the last board address, keeps navigation locked to the board, and can open the touch flight page (`/fly`) in its own window (**Connection -> Open Fly controller**, Ctrl+Shift+F). Because the UI is served by the board, the app always matches the firmware you flashed.

**Dev use only for now:** the builds are unsigned (Windows SmartScreen / macOS Gatekeeper will warn) and it has not been run on real hardware yet.

## Use it
1. Join the board's Wi-Fi (`ESP32S3-FC`) or your router if the board is in router mode.
2. Start the app, keep `192.168.4.1` (or use `esp32fc.local` / your router IP) and press **Connect**.
3. **Connection -> Change board address** (Ctrl+L) returns to the connect screen. If the link drops, the app goes back there with a message.

## Get a build
- **From GitHub:** Actions -> *Build configurator app* -> latest run -> download the artifact for your OS (Windows `.exe` installer + portable, Linux `.AppImage`, macOS `.dmg`). The workflow runs on every change in this folder and can be started by hand.
- **Locally:** needs Node 20+.
  ```
  cd configurator-app
  npm install
  npm start          # run it
  npm run dist       # build an installer into dist/
  npm test           # address parsing + reachability tests (no Electron needed)
  ```

## Files
| File | Role |
|---|---|
| `main.js` | window, menu, settings, connect/probe handler, navigation lock |
| `preload.js` | exposes `connect` / `getHost` to the bundled connect screen only |
| `host.js` | address parsing and the `GET /api/net` reachability probe (unit-tested) |
| `connect.html` / `connect.js` | connect screen |
