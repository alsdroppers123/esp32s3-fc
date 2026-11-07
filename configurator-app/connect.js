'use strict';
const $ = (id) => document.getElementById(id);
const status = (msg, err) => { $('status').textContent = msg || ''; $('status').className = err ? 'err' : ''; };

(async () => {
  const q = new URLSearchParams(location.search);
  if (q.get('error')) status(q.get('error'), true);
  try { $('host').value = await window.fc.getHost(); } catch (e) { $('host').value = '192.168.4.1'; }
  $('host').focus(); $('host').select();
})();

document.querySelectorAll('.chip').forEach((b) => { b.onclick = () => { $('host').value = b.dataset.h; $('host').focus(); }; });

$('f').onsubmit = async (e) => {
  e.preventDefault();
  $('go').disabled = true; status('Connecting...');
  try {
    const r = await window.fc.connect($('host').value);
    if (!r.ok) status(r.error || 'Could not connect.', true);       // on success the window navigates away
  } catch (err) { status('Could not connect.', true); }
  $('go').disabled = false;
};
