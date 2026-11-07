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
a.fly{background:var(--cyan);color:#0b1117;text-decoration:none;font-weight:700;border-radius:99px;padding:4px 14px;font-size:13px;letter-spacing:.4px}
.hwgrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:10px 16px;margin-bottom:14px}
.hwgrid label{display:flex;flex-direction:column;gap:4px;font-size:12px;color:var(--dim)}.hwgrid select,.hwgrid input{width:100%}
.board{display:flex;flex-wrap:wrap;gap:14px;background:linear-gradient(160deg,#16261f,#0f1a15);border:1px solid #284236;border-radius:12px;padding:14px;margin-bottom:14px}
.hcol{flex:1 1 220px;min-width:200px}.hcol h4{margin:0 0 8px;font-size:11px;letter-spacing:1.2px;text-transform:uppercase;color:#8fb3a2;font-weight:600}
.pin{display:grid;grid-template-columns:22px 74px 1fr;align-items:center;gap:6px;padding:3px 6px;margin:3px 0;border-radius:6px;background:rgba(0,0,0,.22);border-left:3px solid transparent}
.pin .pad{width:14px;height:14px;border-radius:50%;background:#c9a24a;box-shadow:inset 0 0 0 4px #7d6224}
.pin .gp{font-size:13px}.pin .gp em{display:block;font-style:normal;font-size:10px;color:var(--dim)}
.pin select{width:100%;padding:3px 4px;font-size:13px}.pin.pwr{opacity:.55}.pin.pwr span.v{font-size:12px;color:var(--dim)}
.pin.bad{outline:1px solid var(--red)}.pin .w{grid-column:2/4;font-size:10px;color:var(--amber)}
.c-imu{border-left-color:var(--cyan)}.c-mot{border-left-color:var(--amber)}.c-rc{border-left-color:var(--ok)}.c-sta{border-left-color:#b48cff}.c-pwr{border-left-color:var(--red)}
.chips{display:flex;flex-wrap:wrap;gap:8px;margin:0 0 14px}.chip{padding:5px 10px;border-radius:99px;border:1px solid var(--line);font-size:12px;color:var(--dim)}
.chip.y{color:var(--ok);border-color:var(--ok)}.chip.n{color:var(--red);border-color:var(--red)}
.ncard{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:12px 14px}.ncard h4{margin:0 0 6px;font-size:11px;letter-spacing:1.2px;text-transform:uppercase;color:var(--dim);font-weight:600}
.nst{font-size:17px;font-weight:600}.nst.ok{color:var(--ok)}.nst.amb{color:var(--amber)}.nst.bad{color:var(--red)}.nd{color:var(--dim);font-size:13px;margin-top:4px;line-height:1.5}.nd a{color:var(--cyan)}
#nets{max-height:260px;overflow-y:auto}.net{display:flex;align-items:center;gap:10px;width:100%;background:none;border:0;border-bottom:1px solid var(--line);color:var(--fg);padding:8px 6px;font:inherit;cursor:pointer;text-align:left}
.net:hover{background:var(--panel)}.net.sel{color:var(--amber)}.net .nb{display:flex;gap:2px;align-items:flex-end;height:12px}.net .nb i{width:3px;background:var(--line)}.net .nb i.on{background:currentColor}
.net .nb{flex:none}.net>span:nth-child(2){flex:1}.net small{color:var(--dim)}
.det{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:10px 12px;margin-bottom:14px;font-size:14px}
</style></head><body>
<header><b>ESP32-S3 Flight Controller</b>
<span id="link" class="pill">offline</span><span id="arm" class="pill">DISARMED</span>
<span id="mode" class="pill">-</span><span id="src" class="pill">RC: none</span><span id="imup" class="pill">IMU: -</span><span id="bat" class="pill" style="display:none"></span><span id="loop" class="pill"></span><a class="fly" href="/fly">FLY &rsaquo;</a></header>
<nav id="nav"><button data-t="setup" class="on">Setup</button><button data-t="pid">PID &amp; Rates</button>
<button data-t="rx">Receiver</button><button data-t="mot">Motors</button><button data-t="pins">Pins &amp; IMU</button><button data-t="net">Wi-Fi &amp; OTA</button></nav>
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

<section id="rx" class="tab"><p style="color:var(--dim);font-size:13px;margin:0 0 10px">Receiver protocol (CRSF or PPM) and channel order are set in the Pins &amp; IMU tab. Move each stick and check the right bar moves the right way.</p><div id="rxbars"></div></section>

<section id="mot" class="tab">
<div class="warn"><b>Remove all propellers.</b> Motor test spins the motors directly. Limited to 30 % and stops 1 s after the last command, on disconnect, or when armed.</div>
<label><input type="checkbox" id="mten"> Enable motor test</label>
<div id="mbars" style="margin:12px 0"></div>
<button class="b danger" id="mstop">Stop all</button></section>

<section id="pins" class="tab">
<div class="det" id="det">Reading hardware&hellip;</div>
<div class="hwgrid">
<label>Board<select id="hwBoard"></select></label>
<label>IMU bus<select id="hwBus"><option value=0>Auto (SPI, then I2C)</option><option value=1>I2C</option><option value=2>SPI</option></select></label>
<label>IMU sensor<select id="hwImu"></select></label>
<label>Sensor orientation<select id="hwAlign"></select></label>
<label>I2C clock<select id="hwI2c"><option>100</option><option>400</option><option>1000</option></select></label>
<label>SPI clock (MHz)<select id="hwSpi"><option>2</option><option>4</option><option>8</option><option>10</option><option>16</option><option>20</option></select></label>
<label>Receiver protocol<select id="hwRxp"><option value=0>CRSF / ELRS (serial)</option><option value=1>PPM (CPPM, one wire)</option></select></label>
<label>Receiver channel order<select id="hwCho"><option value=0>AETR (ELRS, FrSky, Futaba)</option><option value=1>TAER (Spektrum, JR, many PPM)</option><option value=2>AERT</option><option value=3>ETAR</option></select></label>
<label>Status LED type<select id="hwLed"><option value=0>None</option><option value=1>RGB WS2812</option><option value=2>Plain, active high</option><option value=3>Plain, active low</option></select></label>
<label>Battery divider scale<input type="number" id="hwVs" min="1" max="50" step="0.01"></label>
<label style="flex-direction:row;align-items:center;gap:8px;margin-top:18px"><input type="checkbox" id="hwBi" style="width:auto"> Buzzer active-low</label>
</div>
<div class="chips" id="hwChk"></div>
<div class="board" id="hwBoardView"></div>
<button class="b pri" id="hwSave">Save pin map</button><button class="b" id="hwSaveR">Save &amp; reboot</button><button class="b" id="hwDef">Load board defaults</button><button class="b" id="hwReload">Discard changes</button>
<p style="color:var(--dim);font-size:13px">Pick a function for any pin. Pin changes apply after a reboot. If a bad pin map stops the board from working, hold the BOOT button for 5 s while disarmed to restore the board defaults. Pin order follows the vendor pinout; check your board's silkscreen.</p>
</section>

<section id="net" class="tab">
<div class="row" style="margin-bottom:14px">
<div class="ncard"><h4>FC hotspot</h4><div id="nAp" class="nst">-</div><div id="nApD" class="nd"></div></div>
<div class="ncard"><h4>Router</h4><div id="nSta" class="nst">-</div><div id="nStaD" class="nd"></div></div></div>
<table><caption>Join a router <button class="b" id="wscan" style="float:right;margin:0;padding:4px 12px">Scan</button></caption>
<tr><td colspan="2" style="padding:0"><div id="nets"></div></td></tr>
<tr><td style="width:130px">Network name</td><td><input type="text" id="ssid" autocomplete="off" autocapitalize="off" spellcheck="false"></td></tr>
<tr><td>Password</td><td><span class="ctl"><input type="password" id="pass" autocomplete="off"><button class="b" id="wshow" style="margin:0;padding:4px 10px">Show</button></span></td></tr>
<tr><td colspan="2"><label><input type="checkbox" id="apoff"> Turn the FC hotspot off while the router is connected <span style="color:var(--dim);font-size:13px">(it turns back on by itself if the router is lost for 10 s)</span></label></td></tr></table>
<button class="b pri" id="wsave">Save &amp; connect</button><button class="b" id="wforget">Forget router</button>
<p style="color:var(--dim);font-size:13px;margin:4px 0 0">With a router you get its range: put your phone on the same Wi-Fi and open the FC address shown above (or esp32fc.local). The joystick page and UDP port 8888 work the same over the router. Wi-Fi changes and scans are refused while armed.</p>
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
 DRN=['','Arm switch off','Receiver signal lost','Tilt limit exceeded','IMU read failure','Disarmed from web','Failsafe landing complete'];
let IMUS=['none'];
$('#pre').innerHTML=PA.map(p=>'<li>'+p+'</li>').join('');
$('#rxbars').innerHTML=CH.map((n,i)=>`<div class=bar><span>${n}</span><i><u id=rx${i}></u></i><span id=rv${i}>-</span></div>`).join('');
$('#mbars').innerHTML=MN.map((n,i)=>`<div class="bar m"><span>${n}</span><span class=ctl><input type=range id=ms${i} min=0 max=300 step=5 value=0 style="width:100%;accent-color:var(--amber)"></span><span id=mv${i}>0%</span></div>`).join('')
 +'<div style="color:var(--dim);font-size:13px">Actual output</div>'+[0,1,2,3].map(i=>`<div class="bar m"><span>M${i+1}</span><i><u id=mo${i}></u></i><span id=mp${i}></span></div>`).join('');
function onTel(d){T=d;const s=d.s,armed=s[0];
 $('#arm').textContent=armed?'ARMED':'DISARMED';$('#arm').className='pill '+(armed?'bad':'');
 $('#mode').textContent=['ACRO','ANGLE','HORIZON'][s[1]];$('#mode').className='pill amb';
 $('#src').textContent='RC: '+['none','CRSF','UDP','WEB','PPM'][s[2]];
 const b=d.b||[0,0,0,0];if(b[0]){$('#bat').style.display='';$('#bat').textContent=(b[0]/100).toFixed(2)+' V'}
 $('#imup').textContent='IMU: '+(IMUS[b[2]]||'-');$('#imup').className='pill '+(b[2]?'ok':'bad');$('#src').className='pill '+(s[2]?'ok':'bad');
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
 $$('.tab').forEach(s=>s.classList.toggle('on',s.id===b.dataset.t));if(b.dataset.t==='net')netInfo();if(b.dataset.t==='pins')hwLoad();if(b.dataset.t==='pid')drawRates()});
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

let netT=0;
async function netInfo(){clearTimeout(netT);try{const j=await(await fetch('/api/net')).json();
 $('#nAp').textContent=j.apon?'On':'Off';$('#nAp').className='nst '+(j.apon?'ok':'');
 $('#nApD').innerHTML=j.apon?`"${j.ap}" at <a href="http://${j.apip}/">${j.apip}</a> &middot; ${j.apn} connected`:'Off while the router link is up';
 const st=$('#nSta');if(j.sta){st.textContent='Connected';st.className='nst ok';
  $('#nStaD').innerHTML=`"${j.ssid}" &middot; ${j.rssi} dBm &middot; ch ${j.ch}<br>FC address <a href="http://${j.ip}/">${j.ip}</a> &middot; <a href="http://${j.host}/">${j.host}</a> &middot; <a href="http://${j.ip}/fly">fly</a>`}
 else{st.textContent=j.ssid?j.st[0].toUpperCase()+j.st.slice(1):'Not set';st.className='nst '+(j.ssid?(j.st==='connecting'?'amb':'bad'):'');$('#nStaD').textContent=j.ssid?'"'+j.ssid+'"':'Scan and pick your router below'}
 if(document.activeElement!==$('#ssid')&&!$('#ssid').value)$('#ssid').value=j.ssid||'';$('#apoff').checked=!!j.apoff;netInfo.ssid=j.ssid}catch(e){}
 if($('#net').classList.contains('on'))netT=setTimeout(netInfo,2500)}
async function scan(){const b=$('#wscan');b.textContent='Scanning...';b.disabled=true;
 try{for(let i=0;i<20;i++){const r=await fetch('/api/scan'),j=await r.json();if(j.err){toast(j.err);break}
  if(j.nets){$('#nets').innerHTML=j.nets.length?j.nets.map(n=>{const q=n.rssi>-55?4:n.rssi>-65?3:n.rssi>-75?2:1;
   return `<button class=net data-s="${encodeURIComponent(n.ssid)}"><span class=nb>${[1,2,3,4].map(k=>`<i class="${k<=q?'on':''}" style="height:${k*3}px"></i>`).join('')}</span><span>${n.ssid.replace(/</g,'&lt;')}</span><small>${n.lock?'&#128274; ':''}${n.rssi} dBm &middot; ch ${n.ch}</small></button>`}).join(''):'<div style="padding:8px;color:var(--dim)">No networks found</div>';
   $$('.net').forEach(x=>x.onclick=()=>{$$('.net').forEach(y=>y.classList.remove('sel'));x.classList.add('sel');$('#ssid').value=decodeURIComponent(x.dataset.s);$('#pass').value='';$('#pass').focus()});break}
  await new Promise(r=>setTimeout(r,700))}}catch(e){toast('Scan failed')}b.textContent='Scan';b.disabled=false}
$('#wscan').onclick=scan;
$('#wshow').onclick=()=>{const p=$('#pass');p.type=p.type==='password'?'text':'password';$('#wshow').textContent=p.type==='password'?'Show':'Hide'};
async function wifiPost(o){try{const r=await fetch('/api/wifi',{method:'POST',body:new URLSearchParams(o)});toast(await r.text());setTimeout(netInfo,1500)}catch(e){toast('Lost connection - if the FC moved to the router channel, rejoin its hotspot')}}
$('#wsave').onclick=()=>{const s=$('#ssid').value.trim(),o={ssid:s,pass:$('#pass').value,apoff:$('#apoff').checked?1:0};
 if(!s)return toast('Enter or pick a network');if(s===netInfo.ssid&&!o.pass)o.keep=1;wifiPost(o)};
$('#wforget').onclick=()=>{if(confirm('Forget the router and keep only the FC hotspot?')){$('#ssid').value='';$('#pass').value='';wifiPost({ssid:'',pass:'',apoff:0})}};
$('#ota').onclick=()=>{const f=$('#fw').files[0];if(!f)return toast('Choose a .bin file first');
 const x=new XMLHttpRequest(),fd=new FormData();x.open('POST','/update');
 x.upload.onprogress=e=>{$('#pb').value=e.loaded/e.total*100};x.onload=()=>toast(x.responseText);x.onerror=()=>toast('Upload failed');
 fd.append('firmware',f,f.name);x.send(fd)};

/* ---------- pins & IMU ---------- */
const R=(a,b)=>Array.from({length:b-a+1},(_,i)=>a+i);
const BOARDS=[
 {n:'Waveshare ESP32-S3-Zero',cols:[['Header 1',['5V','GND','3V3',1,2,3,4,5,6]],['Header 2',[43,44,13,12,11,10,9,8,7]],['Bottom pads',[14,15,16,17,18,38,39,40,41,42,45,47,48]],['On board',[21]]],note:{21:'RGB LED (WS2812)'}},
 {n:'ESP32-S3 Super Mini',cols:[['Header 1',['5V','GND','3V3',43,44,1,2,3,4]],['Header 2',[5,6,7,8,9,10,11,12,13]],['Bottom pads',[14,15,16,17,18,21,38,39,40,41,42,45,46,47]],['On board',[48]]],note:{48:'RGB LED (WS2812)'}},
 {n:'Seeed XIAO ESP32S3',cols:[['Left',[1,2,3,4,5,6,43]],['Right',['5V','GND','3V3',9,8,7,44]],['On board',[21]]],lab:{1:'D0',2:'D1',3:'D2',4:'D3',5:'D4',6:'D5',43:'D6',44:'D7',7:'D8',8:'D9',9:'D10'},note:{21:'User LED, active low'}},
 {n:'ESP32-S3-DevKitC-1',cols:[['J1',['3V3','3V3','RST',4,5,6,7,15,16,17,18,8,3,46,9,10,11,12,13,14,'5V','GND']],['J3',['GND',43,44,1,2,42,41,40,39,38,37,36,35,0,45,48,47,21,20,19,'GND','GND']]],note:{48:'RGB LED (v1.0)',38:'RGB LED (v1.1)'}},
 {n:'Generic ESP32-S3',cols:[['GPIO 0-18, 21',R(0,18).concat([21])],['GPIO 33-48',R(33,48)]],note:{}}];
const FN=[['sda','IMU SDA','imu'],['scl','IMU SCL','imu'],['sck','IMU SCK','imu'],['miso','IMU MISO','imu'],['mosi','IMU MOSI','imu'],['cs','IMU CS','imu'],['cs2','IMU CS2 (BMI088)','imu'],
 ['m1','Motor 1 \u00B7 RR','mot'],['m2','Motor 2 \u00B7 FR','mot'],['m3','Motor 3 \u00B7 RL','mot'],['m4','Motor 4 \u00B7 FL','mot'],
 ['rx','Receiver in (CRSF/PPM)','rc'],['tx','Receiver TX (CRSF)','rc'],['led','Status LED','sta'],['buz','Buzzer','sta'],['vbat','Battery ADC','pwr']];
const WARN={0:'BOOT button, strapping',3:'strapping pin',45:'strapping (flash voltage)',46:'strapping (boot mode)',35:'PSRAM on octal modules',36:'PSRAM on octal modules',37:'PSRAM on octal modules',43:'UART0 TX (boot log)',44:'UART0 RX'};
const NOUSE={19:'USB D-',20:'USB D+'};
const ALN=['0° (chip X to nose)','90° CW','180°','270° CW','0° flipped','90° CW flipped','180° flipped','270° CW flipped'];
let HW=null,HWDET=null;
$('#hwBoard').innerHTML=BOARDS.map((b,i)=>`<option value=${i}>${b.n}</option>`).join('');
$('#hwAlign').innerHTML=ALN.map((a,i)=>`<option value=${i}>${a}</option>`).join('');
async function hwLoad(def){try{const j=await(await fetch('/api/hw'+(def!==undefined?'?def='+def:''))).json();
 if(j.imus){IMUS=j.imus;$('#hwImu').innerHTML=j.imus.map((n,i)=>`<option value=${i}>${i?n:'Auto-detect'}</option>`).join('')}
 HW=j.hw;if(def===undefined)HWDET=j.det;hwFill();if(def!==undefined)toast('Loaded '+BOARDS[HW.board].n+' defaults - review, then save')}catch(e){toast('Could not read /api/hw')}}
function hwFill(){const h=HW;$('#hwBoard').value=h.board;$('#hwBus').value=h.bus;$('#hwImu').value=h.imu;$('#hwAlign').value=h.align;$('#hwI2c').value=h.i2c;$('#hwSpi').value=h.spi;
 $('#hwLed').value=h.ledt;$('#hwRxp').value=h.rxp||0;$('#hwCho').value=h.cho||0;$('#hwVs').value=h.vscale;$('#hwBi').checked=!!h.buzinv;
 const d=HWDET;if(d){const sc=d.scan.length?d.scan.map(a=>'0x'+a.toString(16).padStart(2,'0')).join(' '):'none';
  $('#det').innerHTML=d.type?`Detected <b style="color:var(--ok)">${d.imu}</b> on ${['','I2C','SPI'][d.bus]}${d.bus==1?' @ 0x'+d.addr.toString(16):''} (ID 0x${d.who.toString(16).padStart(2,'0')})${d.mag?(d.imu.indexOf('BNO')==0?' + on-chip magnetometer':' + AK8963 magnetometer'):''}<br><span style="color:var(--dim);font-size:13px">I2C devices seen at boot: ${sc}</span>`
  :`<b style="color:var(--red)">No IMU found</b> with the current pins. <span style="color:var(--dim);font-size:13px">I2C devices seen at boot: ${sc}. Check wiring, then set the bus pins below and reboot.</span>`}
 hwDraw()}
const pinFn=g=>{for(const [k] of FN)if(HW.pins[k]===g)return k;return ''};
function hwDraw(){const b=BOARDS[HW.board],v=$('#hwBoardView');
 const onBoard=new Set();b.cols.forEach(c=>c[1].forEach(p=>{if(typeof p==='number')onBoard.add(p)}));
 const count={};FN.forEach(([k])=>{const g=HW.pins[k];if(g>=0)count[g]=(count[g]||0)+1});
 v.innerHTML=b.cols.map(([t,ps])=>`<div class=hcol><h4>${t}</h4>`+ps.map(p=>{
  if(typeof p!=='number'||NOUSE[p])return `<div class="pin pwr"><i class=pad></i><span class=gp>${typeof p==='number'?'GP'+p:p}</span><span class=v>${NOUSE[p]||(p==='GND'?'ground':p==='RST'?'reset':'power')}</span></div>`;
  const f=pinFn(p),cat=f?FN.find(x=>x[0]===f)[2]:'',lab=(b.lab&&b.lab[p])?b.lab[p]+' · ':'',note=(b.note[p]||'')+((b.note[p]&&WARN[p])?', ':'')+(WARN[p]||'');
  const opts='<option value="">-</option>'+FN.filter(([k])=>k!=='vbat'||(p>=1&&p<=10)).map(([k,n])=>`<option value="${k}"${k===f?' selected':''}>${n}</option>`).join('');
  return `<div class="pin ${cat?'c-'+cat:''}${count[p]>1?' bad':''}"><i class=pad></i><span class=gp>GP${p}<em>${lab}${p>=1&&p<=10?'ADC1':''}</em></span><select data-g=${p}>${opts}</select>${note?`<span class=w>${note}</span>`:''}</div>`}).join('')+'</div>').join('');
 const off=FN.filter(([k])=>HW.pins[k]>=0&&!onBoard.has(HW.pins[k]));
 if(off.length)v.insertAdjacentHTML('beforeend','<div class=hcol><h4>Not on this board</h4>'+off.map(([k,n])=>`<div class="pin bad"><i class=pad></i><span class=gp>GP${HW.pins[k]}</span><span>${n} <a href="#" data-clr="${k}" style="color:var(--amber)">clear</a></span></div>`).join('')+'</div>');
 v.querySelectorAll('select').forEach(s=>s.onchange=()=>{const g=+s.dataset.g,old=pinFn(g),k=s.value;if(old)HW.pins[old]=-1;if(k)HW.pins[k]=g;hwDraw()});
 v.querySelectorAll('[data-clr]').forEach(a=>a.onclick=e=>{e.preventDefault();HW.pins[a.dataset.clr]=-1;hwDraw()});
 hwCheck()}
function hwCheck(){const p=HW.pins,has=k=>p[k]>=0,bus=+$('#hwBus').value;
 const i2c=has('sda')&&has('scl'),spi=['sck','miso','mosi','cs'].every(has),mot=['m1','m2','m3','m4'].every(has);
 const busOk=bus==1?i2c:bus==2?spi:(i2c||spi);const dup=Object.values(p).filter(g=>g>=0).length!==new Set(Object.values(p).filter(g=>g>=0)).size;
 const c=[[busOk,'IMU bus: '+(spi&&bus!=1?'SPI':i2c?'I2C':'missing')],[mot,'Motors: '+(mot?'4/4':'incomplete')],[has('rx'),'Receiver: '+(has('rx')?(['CRSF','PPM'][+$('#hwRxp').value]+' on GP'+p.rx):'web/UDP only')],
  [!dup,dup?'Pin used twice':'No conflicts'],[has('led'),'LED: '+(has('led')?'GP'+p.led:'none')],[has('vbat'),'Battery: '+(has('vbat')?'GP'+p.vbat:'none')]];
 $('#hwChk').innerHTML=c.map(([ok,t],i)=>`<span class="chip ${ok?'y':i<4?'n':''}">${t}</span>`).join('');return busOk&&mot&&!dup}
['#hwBus','#hwImu','#hwAlign','#hwI2c','#hwSpi','#hwLed','#hwVs','#hwBi','#hwRxp','#hwCho'].forEach(id=>$(id).onchange=()=>{const h=HW;h.bus=+$('#hwBus').value;h.imu=+$('#hwImu').value;h.align=+$('#hwAlign').value;
 h.i2c=+$('#hwI2c').value;h.spi=+$('#hwSpi').value;h.ledt=+$('#hwLed').value;h.rxp=+$('#hwRxp').value;h.cho=+$('#hwCho').value;h.vscale=+$('#hwVs').value;h.buzinv=$('#hwBi').checked?1:0;hwCheck()});
$('#hwBoard').onchange=()=>{if(confirm('Load the default pin map for '+BOARDS[+$('#hwBoard').value].n+'?'))hwLoad(+$('#hwBoard').value);else{HW.board=+$('#hwBoard').value;hwDraw()}};
$('#hwDef').onclick=()=>hwLoad(HW?HW.board:0);$('#hwReload').onclick=()=>hwLoad();
async function hwSave(reboot){if(!HW)return;if(!hwCheck()&&!confirm('The pin map has problems. Save anyway?'))return;
 const f=new URLSearchParams({board:HW.board,bus:HW.bus,imu:HW.imu,align:HW.align,ledt:HW.ledt,rxp:HW.rxp||0,cho:HW.cho||0,buzinv:HW.buzinv,i2c:HW.i2c,spi:HW.spi,vscale:HW.vscale});
 FN.forEach(([k])=>f.append(k,HW.pins[k]));
 try{const r=await fetch('/api/hw',{method:'POST',body:f}),j=await r.json();toast(j.ack||('Error: '+j.err));
  if(r.ok&&reboot)await fetch('/api/reboot',{method:'POST'})}catch(e){toast('Save failed')}}
$('#hwSave').onclick=()=>hwSave(false);$('#hwSaveR').onclick=()=>hwSave(true);
fetch('/api/hw').then(r=>r.json()).then(j=>{IMUS=j.imus||IMUS;HWDET=j.det}).catch(()=>{});
conn();
</script></body></html>)HTML";
