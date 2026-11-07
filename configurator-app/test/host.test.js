'use strict';
const assert = require('assert');
const http = require('http');
const { normalizeHost, probe } = require('../host');

// ---- normalizeHost
const good = [
  ['192.168.4.1', '192.168.4.1'],
  ['  192.168.4.1  ', '192.168.4.1'],
  ['http://192.168.4.1/', '192.168.4.1'],
  ['HTTP://ESP32FC.local/fly', 'esp32fc.local'],
  ['esp32fc.local', 'esp32fc.local'],
  ['192.168.1.50:8080', '192.168.1.50:8080'],
  ['my-board', 'my-board'],
];
for (const [inp, out] of good) assert.strictEqual(normalizeHost(inp), out, 'normalizeHost(' + inp + ')');
const bad = ['', '   ', null, undefined, 42, 'a b', 'host:0', 'host:70000', 'host:abc', '-bad.local', 'bad-.local',
  'javascript:alert(1)', 'file:///etc/passwd', '192.168.4.1@evil.com', '[::1]', 'a..b', 'x'.repeat(300)];
for (const inp of bad) assert.strictEqual(normalizeHost(inp), null, 'should reject ' + JSON.stringify(inp));
console.log('ok   normalizeHost (' + (good.length + bad.length) + ' cases)');

// ---- probe against fake boards
function server(handler) {
  return new Promise((resolve) => { const s = http.createServer(handler); s.listen(0, '127.0.0.1', () => resolve(s)); });
}
(async () => {
  const okSrv = await server((req, res) => { if (req.url === '/api/net') { res.writeHead(200, { 'content-type': 'application/json' }); res.end('{"ap":"1"}'); } else { res.writeHead(404); res.end(); } });
  let r = await probe('127.0.0.1:' + okSrv.address().port);
  assert.deepStrictEqual(r, { ok: true }); console.log('ok   probe: board answers 200');

  const notFc = await server((req, res) => { res.writeHead(404); res.end(); });
  r = await probe('127.0.0.1:' + notFc.address().port);
  assert.strictEqual(r.ok, false); assert.match(r.error, /HTTP 404/); console.log('ok   probe: other web server -> HTTP 404 message');

  const silent = await server(() => { /* never answers */ });
  const t0 = Date.now();
  r = await probe('127.0.0.1:' + silent.address().port, 300);
  assert.strictEqual(r.ok, false); assert.match(r.error, /timeout/i); assert.ok(Date.now() - t0 < 2000);
  console.log('ok   probe: silent board -> timeout message');

  const closedPort = notFc.address().port; await new Promise((res) => notFc.close(res));
  r = await probe('127.0.0.1:' + closedPort);
  assert.strictEqual(r.ok, false); assert.match(r.error, /refused/i); console.log('ok   probe: closed port -> refused message');

  r = await probe('definitely-not-a-real-host.invalid');
  assert.strictEqual(r.ok, false); assert.ok(r.error.length > 0); console.log('ok   probe: unknown name -> friendly error (' + r.error.slice(0, 40) + '...)');

  okSrv.close(); silent.closeAllConnections && silent.closeAllConnections(); silent.close();
  console.log('ALL PASSED');
})().catch((e) => { console.error('FAILED:', e); process.exit(1); });
