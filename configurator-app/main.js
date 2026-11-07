'use strict';
// ESP32-S3 FC Configurator - desktop shell.
// The board serves its own configurator (http://<board>/). This app adds a connect screen, remembers the
// address, keeps navigation locked to the board, and can open the touch flight page (/fly) in its own window.
const { app, BrowserWindow, Menu, ipcMain } = require('electron');
const path = require('path');
const fs = require('fs');
const { normalizeHost, probe } = require('./host');

const DEFAULT_HOST = '192.168.4.1';          // the board's own hotspot address
let win = null;
let currentHost = null;                      // set while the board's page is shown

const settingsFile = () => path.join(app.getPath('userData'), 'settings.json');
function loadSettings() { try { return JSON.parse(fs.readFileSync(settingsFile(), 'utf8')); } catch (e) { return {}; } }
function saveSettings(s) { try { fs.writeFileSync(settingsFile(), JSON.stringify(s)); } catch (e) { /* not fatal */ } }

function showConnect(error) {
  currentHost = null;
  if (!win) return;
  win.loadFile(path.join(__dirname, 'connect.html'), { query: error ? { error } : {} });
  buildMenu();
}

async function connectTo(input) {
  const host = normalizeHost(input);
  if (!host) return { ok: false, error: 'Enter an IP address or name, for example 192.168.4.1 or esp32fc.local.' };
  const r = await probe(host);
  if (!r.ok) return r;
  saveSettings({ host });
  currentHost = host;
  win.loadURL('http://' + host + '/');
  buildMenu();
  return { ok: true };
}

// Only the bundled connect page may talk to these handlers.
function fromConnectPage(event) {
  return !!(event.senderFrame && typeof event.senderFrame.url === 'string' && event.senderFrame.url.startsWith('file://'));
}
ipcMain.handle('fc:get-host', (event) => (fromConnectPage(event) ? (loadSettings().host || DEFAULT_HOST) : DEFAULT_HOST));
ipcMain.handle('fc:connect', (event, host) => (fromConnectPage(event) ? connectTo(host) : { ok: false, error: 'Not allowed.' }));

function lockToBoard(contents) {
  contents.on('will-navigate', (e, url) => {
    let u; try { u = new URL(url); } catch (err) { e.preventDefault(); return; }
    if (u.host !== currentHost) e.preventDefault();
  });
  contents.setWindowOpenHandler(({ url }) => {
    try { const u = new URL(url); if (u.protocol === 'http:' && u.host === currentHost) return { action: 'allow' }; } catch (e) { /* deny */ }
    return { action: 'deny' };
  });
}

function openFly() {
  if (!currentHost) return;
  const f = new BrowserWindow({
    width: 1000, height: 520, title: 'FC - Fly', autoHideMenuBar: true,
    webPreferences: { contextIsolation: true, sandbox: true },
  });
  lockToBoard(f.webContents);
  f.loadURL('http://' + currentHost + '/fly');
}

function buildMenu() {
  const connected = !!currentHost;
  Menu.setApplicationMenu(Menu.buildFromTemplate([
    { label: 'Connection', submenu: [
      { label: 'Change board address...', accelerator: 'CmdOrCtrl+L', click: () => showConnect() },
      { label: 'Open Fly controller', accelerator: 'CmdOrCtrl+Shift+F', enabled: connected, click: openFly },
      { label: 'Reload', accelerator: 'CmdOrCtrl+R', click: () => win && win.webContents.reload() },
      { type: 'separator' },
      { role: 'quit' },
    ] },
    { label: 'View', submenu: [
      { role: 'resetZoom' }, { role: 'zoomIn' }, { role: 'zoomOut' }, { type: 'separator' },
      { role: 'togglefullscreen' }, { role: 'toggleDevTools' },
    ] },
  ]));
}

function createWindow() {
  win = new BrowserWindow({
    width: 1280, height: 820, minWidth: 900, minHeight: 600, title: 'ESP32-S3 FC Configurator',
    backgroundColor: '#0b0f14',
    webPreferences: { preload: path.join(__dirname, 'preload.js'), contextIsolation: true, nodeIntegration: false, sandbox: true },
  });
  lockToBoard(win.webContents);
  win.webContents.on('did-fail-load', (e, code, desc, url, isMainFrame) => {
    if (!isMainFrame || code === -3 || !/^https?:/i.test(url)) return;      // -3 = navigation aborted by us
    showConnect('Lost the connection to ' + currentHost + ' (' + desc + ').');
  });
  win.webContents.on('page-title-updated', (e, title) => { if (currentHost) { e.preventDefault(); win.setTitle(title + ' - ' + currentHost); } });
  win.on('closed', () => { win = null; });
  showConnect();
}

app.whenReady().then(() => {
  createWindow();
  app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow(); });
});
app.on('window-all-closed', () => { if (process.platform !== 'darwin') app.quit(); });
