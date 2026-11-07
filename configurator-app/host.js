'use strict';
// Pure Node helpers (no Electron) so they can be unit-tested: address parsing and a reachability probe.
const http = require('http');

// Accepts "192.168.4.1", "esp32fc.local", "http://192.168.4.1/", "192.168.4.1:8080".
// Returns a clean "host[:port]" string, or null if the input is not a plain hostname / IPv4 address.
function normalizeHost(input) {
  if (typeof input !== 'string') return null;
  const s = input.trim().replace(/^https?:\/\//i, '').replace(/\/.*$/, '');
  if (!s || s.length > 255) return null;
  const label = '[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?';
  const m = new RegExp('^(' + label + '(?:\\.' + label + ')*)(?::(\\d{1,5}))?$').exec(s);
  if (!m) return null;
  const port = m[2] ? parseInt(m[2], 10) : null;
  if (port !== null && (port < 1 || port > 65535)) return null;
  return m[1].toLowerCase() + (port ? ':' + port : '');
}

function friendly(err) {
  switch (err && err.code) {
    case 'ENOTFOUND': case 'EAI_AGAIN':
      return 'Name not found. Try the IP address instead (the hotspot default is 192.168.4.1).';
    case 'ECONNREFUSED':
      return 'Connection refused. Something is at that address but it is not the flight controller web server.';
    case 'EHOSTUNREACH': case 'ENETUNREACH':
      return 'Host unreachable. Join the drone\'s Wi-Fi (or your router) first.';
    default:
      return 'Could not connect' + (err && err.code ? ' (' + err.code + ')' : '') + '.';
  }
}

// GET /api/net exists on the firmware; any 2xx means "this is probably the FC".
function probe(host, timeoutMs = 3000) {
  return new Promise((resolve) => {
    let url;
    try { url = new URL('http://' + host + '/api/net'); } catch (e) { return resolve({ ok: false, error: 'Invalid address.' }); }
    const req = http.get(url, { timeout: timeoutMs }, (res) => {
      res.resume();
      if (res.statusCode >= 200 && res.statusCode < 300) resolve({ ok: true });
      else resolve({ ok: false, error: 'The device answered with HTTP ' + res.statusCode + '. Is this the flight controller?' });
    });
    req.on('timeout', () => { req.destroy(); resolve({ ok: false, error: 'No answer (timeout). Check that you are on the drone\'s Wi-Fi and the board is powered.' }); });
    req.on('error', (e) => resolve({ ok: false, error: friendly(e) }));
  });
}

module.exports = { normalizeHost, probe, friendly };
