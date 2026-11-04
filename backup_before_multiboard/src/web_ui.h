// web_ui.h - single-file web app served from flash (no external dependencies, works offline in AP mode)
#pragma once
#include <pgmspace.h>
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-S3 FC</title>
<style>
:root{--bg:#14181e;--panel:#1c222b;--line:#2b3440;--fg:#dde4ec;--dim:#8794a3;--amber:#ffb224;--cyan:#56c8f0;--red:#ff5d4d;--ok:#6fd08c}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.4 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;font-variant-numeric:tabular-nums}
header{display:flex;flex-wrap:wrap;gap:8px;align-items:center;padding:10px 14px;border-bottom:1px solid var(--line)}
header b{font-size:17px;margin-right:auto;letter-spacing:.2px}
.pill{padding:2px 10px;border:1px solid var(--line);border-radius:99px;color:var(--dim);font-size:13px}
.pill.ok{color:var(--ok);border-color:var(--ok)}.pill.bad{color:var(--red);border-color:var(--red)}.pill.amb{color:var(--amber);border-color:var(--amber)}
nav{display:flex;overflow-x:auto;border-bottom:1px solid var(--line)}
nav button{flex:none;background:none;border:0;border-bottom:3px solid transparent;color:var(--dim);padding:11px 16px;font:inherit;cursor:pointer}
nav button.on{color:var(--amber);border-bottom-color:var(--amber)}
main{padding:14px;max-width:980px;margin:auto}
.tab{display:none}.tab.on{display:block}
.row{display:flex;flex-wrap:wrap;gap:18px}.row>*{flex:1 1 300px}
canvas{width:100%;max-width:460px;background:var(--panel);border:1px solid var(--line);border-radius:6px}
button.b{background:var(--panel);color:var(--fg);border:1px solid var(--line);border-radius:5px;padding:8px 13px;font:inherit;cursor:pointer;margin:0 6px 8px 0}
button.b:hover{border-color:var(--amber)}button.b.pri{background:var(--amber);color:#14181e;border-color:var(--amber);font-weight:600}
button.b.danger{background:var(--red);border-color:var(--red);color:#14181e;font-weight:600}
.att{display:flex;gap:22px;margin:4px 0 12px}.att div{font-size:12px;color:var(--dim)}.att span{display:block;font-size:26px;color:var(--cyan)}
ul.pa{list-style:none;padding:0;margin:0 0 12px}ul.pa li{padding:2px 0;color:var(--dim)}ul.pa li.y{color:var(--ok)}ul.pa li:before{content:"\2717  "}ul.pa li.y:before{content:"\2713  "}
table{border-collapse:collapse;width:100%;margin-bottom:16px}caption{text-align:left;color:var(--amber);padding:6px 0;font-weight:600}
th{color:var(--dim);font-weight:400;font-size:12px;text-align:left}td,th{padding:4px 6px;border-bottom:1px solid var(--line)}
.ctl{display:flex;gap:6px;align-items:center}.ctl input[type=range]{flex:1;min-width:70px;accent-color:var(--amber)}
input[type=number],input[type=text],input[type=password],select{background:var(--panel);color:var(--fg);border:1px solid var(--line);border-radius:4px;padding:4px 6px;font:inherit;width:78px}
input[type=text],input[type=password]{width:100%;max-width:280px}
.bar{display:grid;grid-template-columns:130px 1fr 56px;gap:10px;align-items:center;margin:7px 0}
.bar i{display:block;height:12px;background:var(--panel);border:1px solid var(--line);border-radius:3px;position:relative;overflow:hidden}
.bar i u{position:absolute;left:0;top:0;bottom:0;background:var(--cyan);width:0}.bar.m i u{background:var(--amber)}
.warn{border-left:4px solid var(--red);background:var(--panel);padding:10px 12px;margin:0 0 14px}
#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:var(--amber);color:#14181e;padding:8px 16px;border-radius:5px;opacity:0;transition:opacity .2s;pointer-events:none;max-width:90%}
progress{width:100%;max-width:340px;accent-color:var(--amber)}
</style></head><body>
<header><b>ESP32-S3 Flight Controller</b>
<span id="link" class="pill">offline</span><span id="arm" class="pill">DISARMED</span>
<span id="mode" class="pill">-</span><span id="src" class="pill">RC: none</span><span id="loop" class="pill"></span></header>
<nav id="nav"><button data-t="setup" class="on">Setup</button><button data-t="pid">PID &amp; Rates</button>
<button data-t="rx">Receiver</button><button data-t="mot">Motors</button><button data-t="net">Wi-Fi &amp; OTA</button></nav>
<main>

<section id="setup" class="tab on"><div class="row">
<canvas id="cv" width="460" height="320"></canvas>
<div>
<div class="att"><div>Roll<span id="aR">0</span></div><div>Pitch<span id="aP">0</span></div><div>Yaw<span id="aY">0</span></div></div>
<ul class="pa" id="pre"></ul>
<div id="dr" style="color:var(--amber);margin-bottom:10px"></div>
<button class="b" id="zy">Zero yaw view</button>
<button class="b" id="cal">Calibrate gyro</button>
<button class="b" id="mc">Start mag cal</button>
<button class="b danger" id="kill">DISARM NOW</button>
<p style="color:var(--dim);font-size:13px">Arm: AUX1 high with throttle low, level, switch seen low first.<br>Mode (AUX2): low = Angle, mid = Horizon, high = Acro.</p>
</div></div></section>

<section id="pid" class="tab"><div id="pidbox"></div>
<p style="color:var(--dim);margin:0 0 6px">Rate curves (deg/s over stick travel): roll, pitch, yaw</p>
<canvas id="rv" width="460" height="200" style="margin-bottom:12px"></canvas><br>
<button class="b pri" id="save">Save to NVS</button><button class="b" id="reload">Reload from FC</button>
<span style="color:var(--dim);font-size:13px">Changes apply live. Output protocol applies after reboot.</span></section>

<section id="rx" class="tab"><div id="rxbars"></div></section>

<section id="mot" class="tab">
<div class="warn"><b>Remove all propellers.</b> Motor test spins the motors directly. Limited to 30 % and stops 1 s after the last command, on disconnect, or when armed.</div>
<label><input type="checkbox" id="mten"> Enable motor test</label>
<div id="mbars" style="margin:12px 0"></div>
<button class="b danger" id="mstop">Stop all</button></section>

<section id="net" class="tab">
<table><caption>Wi-Fi</caption><tr><td id="netinfo" colspan="2" style="color:var(--dim)">...</td></tr>
<tr><td>Network name</td><td><input type="text" id="ssid"></td></tr>
<tr><td>Password</td><td><input type="password" id="pass"></td></tr></table>
<button class="b pri" id="wsave">Save Wi-Fi</button>
<table style="margin-top:18px"><caption>Firmware update</caption>
<tr><td><input type="file" id="fw" accept=".bin"></td></tr>
<tr><td><progress id="pb" max="100" value="0"></progress></td></tr></table>
<button class="b" id="ota">Upload firmware</button><button class="b" id="reboot">Reboot</button>
</section>
</main><div id="toast"></div>

<script>
const $=s=>document.querySelector(s),$$=s=>[...document.querySelectorAll(s)];
let ws,cfg=[],loaded=false,tm=0,yawOff=0,mtOn=false,T=null;const ctl={};
const send=s=>{if(ws&&ws.readyState===1)ws.send(s)};
function toast(m){const t=$('#toast');t.textContent=m;t.style.opacity=1;clearTimeout(toast.h);toast.h=setTimeout(()=>t.style.opacity=0,2600)}
function push(){if(!loaded)return;clearTimeout(tm);tm=setTimeout(()=>send('U '+cfg.join(' ')),120);drawRates()}

/* ---------- controls bound to cfg[i] ---------- */
function num(i,min,max,step){const d=document.createElement('span');d.className='ctl';
 d.innerHTML=`<input type=range min=${min} max=${max} step=${step}><input type=number min=${min} max=${max} step=${step}>`;
 const[r,n]=d.children;
 r.oninput=()=>{n.value=r.value;cfg[i]=+r.value;push()};
 n.onchange=()=>{r.value=n.value;cfg[i]=+n.value;push()};
 ctl[i]={set:v=>{r.value=n.value=v}};return d}
function chk(i){const c=document.createElement('input');c.type='checkbox';c.onchange=()=>{cfg[i]=c.checked?1:0;push()};ctl[i]={set:v=>c.checked=!!v};return c}
function sel(i,o){const s=document.createElement('select');s.style.width='auto';s.innerHTML=o.map((x,k)=>`<option value=${k}>${x}</option>`).join('');
 s.onchange=()=>{cfg[i]=+s.value;push()};ctl[i]={set:v=>s.value=v};return s}
function tbl(title,heads,rows){const t=document.createElement('table');
 t.innerHTML='<caption>'+title+'</caption><tr><th></th>'+heads.map(h=>'<th>'+h+'</th>').join('')+'</tr>';
 rows.forEach(([n,cells])=>{const r=t.insertRow();r.insertCell().textContent=n;cells.forEach(c=>r.insertCell().appendChild(c))});return t}
const AX=['Roll','Pitch','Yaw'],box=$('#pidbox');
box.append(tbl('PID gains (Betaflight units)',['P','I','D','FF'],AX.map((n,a)=>[n,[0,1,2,3].map(k=>num(a*4+k,0,250,1))])));
box.append(tbl('Rates (Betaflight type)',['RC rate','Super rate','Expo'],AX.map((n,a)=>[n,[num(12+a,.1,3,.01),num(15+a,0,.95,.01),num(18+a,0,1,.01)]])));
box.append(tbl('Filters and leveling',['Value'],[['Gyro low-pass (Hz)',[num(21,20,400,5)]],['D-term low-pass (Hz)',[num(22,20,300,5)]],['RC smoothing (Hz)',[num(23,10,150,5)]],
 ['Angle strength',[num(24,1,12,.1)]],['Max angle (deg)',[num(25,10,60,1)]],['I-term limit',[num(26,50,500,10)]],['Motor idle (%)',[num(27,0,15,.5)]],['Mahony Kp',[num(29,.1,5,.1)]]]));
box.append(tbl('Options',['Value'],[['Airmode',[chk(28)]],['Use magnetometer in yaw',[chk(30)]],['Motor output (applies after reboot)',[sel(31,['DShot300','DShot150','PWM 400 Hz'])]]]));

function rate(s,rr,su,ex){const a=Math.abs(s);if(ex>0)s=s*a*a*a*ex+s*(1-ex);if(rr>2)rr+=14.54*(rr-2);let r=200*rr*s;if(su>0)r/=Math.min(1,Math.max(.01,1-a*su));return Math.max(-1998,Math.min(1998,r))}
function drawRates(){if(!loaded)return;const c=$('#rv'),g=c.getContext('2d'),w=c.width,h=c.height;g.clearRect(0,0,w,h);
 g.strokeStyle='#2b3440';g.beginPath();g.moveTo(0,h/2);g.lineTo(w,h/2);g.moveTo(w/2,0);g.lineTo(w/2,h);g.stroke();
 ['#ff5d4d','#6fd08c','#56c8f0'].forEach((col,a)=>{g.strokeStyle=col;g.lineWidth=2;g.beginPath();
  for(let k=0;k<=100;k++){const x=k/100*w,y=h/2-rate(k/50-1,cfg[12+a],cfg[15+a],cfg[18+a])/1998*h/2;k?g.lineTo(x,y):g.moveTo(x,y)}g.stroke()});
 g.fillStyle='#8794a3';g.fillText('2000 deg/s',4,12)}

/* ---------- 3D quad (canvas, no libraries) ---------- */
function rot(v,r,p,y){const[x,yy,z]=v,cr=Math.cos(r),sr=Math.sin(r),cp=Math.cos(p),sp=Math.sin(p),cy=Math.cos(y),sy=Math.sin(y);
 const y1=yy*cr-z*sr,z1=yy*sr+z*cr,x2=x*cp+z1*sp,z2=-x*sp+z1*cp;return[x2*cy-y1*sy,x2*sy+y1*cy,z2]}      // NED: x fwd, y right, z down
function proj(v,w,h){const a=.5,up=-v[2],dp=v[0],su=up*Math.cos(a)+dp*Math.sin(a),dd=dp*Math.cos(a)-up*Math.sin(a)+5,s=5/dd,k=Math.min(w,h)*.2;
 return[w/2+v[1]*s*k,h/2+18-su*s*k,s]}
function drawQuad(){const c=$('#cv'),g=c.getContext('2d'),w=c.width,h=c.height;g.clearRect(0,0,w,h);if(!T)return;
 const d=Math.PI/180,R=T.t[0]*d,P=T.t[1]*d,Y=(T.t[2]-yawOff)*d,tf=v=>proj(rot(v,R,P,Y),w,h);
 const M=[[-1,1],[1,1],[-1,-1],[1,-1]],col=['#8794a3','#ffb224','#8794a3','#ffb224'];   // M1 RR, M2 FR, M3 RL, M4 FL
 const o=tf([0,0,0]);
 g.lineWidth=5;g.lineCap='round';
 M.forEach((m,i)=>{const p=tf([m[0],m[1],0]);g.strokeStyle='#3a4655';g.beginPath();g.moveTo(o[0],o[1]);g.lineTo(p[0],p[1]);g.stroke();
  const rad=26*p[2];g.lineWidth=2;g.strokeStyle=col[i];g.beginPath();g.arc(p[0],p[1],rad,0,7);g.stroke();
  g.fillStyle=col[i];g.globalAlpha=.15+.6*(T.m[i]||0);g.fill();g.globalAlpha=1;g.lineWidth=5});
 const f=tf([1.7,0,0]);g.strokeStyle='#56c8f0';g.lineWidth=3;g.beginPath();g.moveTo(o[0],o[1]);g.lineTo(f[0],f[1]);g.stroke();
 g.fillStyle='#56c8f0';g.beginPath();g.arc(f[0],f[1],5,0,7);g.fill();
 g.fillStyle='#dde4ec';g.beginPath();g.arc(o[0],o[1],9,0,7);g.fill()}

/* ---------- telemetry ---------- */
const PA=['Gyro calibrated','Level','Throttle low','Receiver link','Arm switch seen low','No motor test / OTA'],
 CH=['Roll (aileron)','Pitch (elevator)','Throttle','Yaw (rudder)','AUX1 arm','AUX2 mode','AUX3','AUX4'],
 MN=['M1 rear right, CCW','M2 front right, CW','M3 rear left, CW','M4 front left, CCW'],
 DRN=['','Arm switch off','Receiver signal lost','Tilt limit exceeded','IMU read failure','Disarmed from web'];
$('#pre').innerHTML=PA.map(p=>'<li>'+p+'</li>').join('');
$('#rxbars').innerHTML=CH.map((n,i)=>`<div class=bar><span>${n}</span><i><u id=rx${i}></u></i><span id=rv${i}>-</span></div>`).join('');
$('#mbars').innerHTML=MN.map((n,i)=>`<div class="bar m"><span>${n}</span><span class=ctl><input type=range id=ms${i} min=0 max=300 step=5 value=0 style="width:100%;accent-color:var(--amber)"></span><span id=mv${i}>0%</span></div>`).join('')
 +'<div style="color:var(--dim);font-size:13px">Actual output</div>'+[0,1,2,3].map(i=>`<div class="bar m"><span>M${i+1}</span><i><u id=mo${i}></u></i><span id=mp${i}></span></div>`).join('');
function onTel(d){T=d;const s=d.s,armed=s[0];
 $('#arm').textContent=armed?'ARMED':'DISARMED';$('#arm').className='pill '+(armed?'bad':'');
 $('#mode').textContent=['ACRO','ANGLE','HORIZON'][s[1]];$('#mode').className='pill amb';
 $('#src').textContent='RC: '+['none','CRSF','UDP'][s[2]];$('#src').className='pill '+(s[2]?'ok':'bad');
 $('#loop').textContent='loop '+s[7]+'/'+s[8]+' us';
 $('#aR').textContent=d.t[0].toFixed(1);$('#aP').textContent=d.t[1].toFixed(1);$('#aY').textContent=(((d.t[2]-yawOff)%360+540)%360-180).toFixed(0);
 $$('#pre li').forEach((li,i)=>li.className=(s[4]>>i&1)?'y':'');
 $('#dr').textContent=(!armed&&s[3])?'Last disarm: '+DRN[s[3]]:'';$('#mc').textContent=s[6]?'Finish mag cal':'Start mag cal';
 d.rc.forEach((v,i)=>{$('#rx'+i).style.width=Math.max(0,Math.min(100,(v-1000)/10))+'%';$('#rv'+i).textContent=v||'-'});
 d.m.forEach((v,i)=>{$('#mo'+i).style.width=v*100+'%';$('#mp'+i).textContent=Math.round(v*100)+'%'});
 drawQuad()}

/* ---------- websocket ---------- */
function conn(){ws=new WebSocket('ws://'+location.host+'/ws');
 ws.onopen=()=>{$('#link').textContent='online';$('#link').className='pill ok'};
 ws.onclose=()=>{$('#link').textContent='offline';$('#link').className='pill';mtOn=false;$('#mten').checked=false;setTimeout(conn,1000)};
 ws.onmessage=e=>{const d=JSON.parse(e.data);
  if(d.cfg){cfg=d.cfg;loaded=true;for(const i in ctl)ctl[i].set(cfg[i]);drawRates()}
  else if(d.t)onTel(d);else if(d.ack)toast(d.ack);else if(d.err)toast('Error: '+d.err)}}

/* ---------- ui wiring ---------- */
$$('#nav button').forEach(b=>b.onclick=()=>{$$('#nav button').forEach(x=>x.classList.toggle('on',x===b));
 $$('.tab').forEach(s=>s.classList.toggle('on',s.id===b.dataset.t));if(b.dataset.t==='net')netInfo();if(b.dataset.t==='pid')drawRates()});
$('#zy').onclick=()=>{if(T)yawOff=T.t[2]};
$('#cal').onclick=()=>send('K');
$('#mc').onclick=()=>send(T&&T.s[6]?'G0':'G1');
$('#kill').onclick=()=>send('D');
$('#save').onclick=()=>send('V');
$('#reload').onclick=()=>send('C');
$('#reboot').onclick=()=>send('R');
$('#mten').onchange=e=>{mtOn=e.target.checked;send('E'+(mtOn?1:0));if(!mtOn)stopAll()};
$('#mstop').onclick=stopAll;
function stopAll(){for(let i=0;i<4;i++){$('#ms'+i).value=0;$('#mv'+i).textContent='0%'}}
for(let i=0;i<4;i++)$('#ms'+i).oninput=e=>$('#mv'+i).textContent=e.target.value/10+'%';
setInterval(()=>{if(mtOn)for(let i=0;i<4;i++)send('M '+i+' '+$('#ms'+i).value)},150);
document.addEventListener('visibilitychange',()=>{if(document.hidden&&mtOn){$('#mten').checked=false;mtOn=false;send('E0');stopAll()}});

async function netInfo(){try{const j=await(await fetch('/api/net')).json();
 $('#netinfo').textContent='Access point '+j.ap+' at '+j.apip+(j.sta?'  |  Joined '+j.ssid+' at '+j.ip+' ('+j.rssi+' dBm)':'  |  Not joined to a network');$('#ssid').value=j.ssid||''}catch(e){}}
$('#wsave').onclick=async()=>{const r=await fetch('/api/wifi',{method:'POST',body:new URLSearchParams({ssid:$('#ssid').value,pass:$('#pass').value})});toast(await r.text())};
$('#ota').onclick=()=>{const f=$('#fw').files[0];if(!f)return toast('Choose a .bin file first');
 const x=new XMLHttpRequest(),fd=new FormData();x.open('POST','/update');
 x.upload.onprogress=e=>{$('#pb').value=e.loaded/e.total*100};x.onload=()=>toast(x.responseText);x.onerror=()=>toast('Upload failed');
 fd.append('firmware',f,f.name);x.send(fd)};
conn();
</script></body></html>)HTML";
