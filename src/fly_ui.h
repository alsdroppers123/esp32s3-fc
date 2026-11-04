// fly_ui.h - full-screen touch flight page served at /fly (Tello-style: dual sticks, HUD, slide-to-take-off).
// Self-contained (no CDN) so it works on the FC's own access point. Sends "J ..." RC frames at 50 Hz.
#pragma once
#include <pgmspace.h>
static const char FLY_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="theme-color" content="#06090d"><meta name="apple-mobile-web-app-capable" content="yes">
<title>FC Fly</title>
<style>
:root{--g:rgba(12,16,22,.46);--g2:rgba(12,16,22,.72);--ln:rgba(255,255,255,.16);--fg:#f4f8fb;--dim:rgba(244,248,251,.62);
--ac:#3cc8ff;--ok:#4be38a;--wa:#ffc93c;--bad:#ff4d4d;--r:999px;--sa-l:env(safe-area-inset-left,0px);--sa-r:env(safe-area-inset-right,0px)}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;height:100%;background:#06090d;color:var(--fg);overflow:hidden;overscroll-behavior:none;
font:500 14px/1.2 system-ui,-apple-system,"SF Pro Text","Segoe UI",Roboto,sans-serif;font-variant-numeric:tabular-nums;
-webkit-user-select:none;user-select:none;touch-action:none}
#hud{position:fixed;inset:0;width:100vw;height:100vh;display:block}
.glass{background:var(--g);border:1px solid var(--ln);-webkit-backdrop-filter:blur(14px) saturate(140%);backdrop-filter:blur(14px) saturate(140%)}
button{font:inherit;color:inherit;cursor:pointer}
.top{position:fixed;left:0;right:0;top:0;display:flex;align-items:center;gap:8px;padding:10px calc(14px + var(--sa-r)) 0 calc(14px + var(--sa-l));z-index:5}
.ib{width:40px;height:40px;border-radius:50%;display:grid;place-items:center;padding:0}
.ib svg{width:20px;height:20px;stroke:currentColor;fill:none;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.pill{height:34px;border-radius:var(--r);display:flex;align-items:center;gap:7px;padding:0 13px;white-space:nowrap;font-size:13px}
.dot{width:8px;height:8px;border-radius:50%;background:var(--dim);box-shadow:0 0 0 3px rgba(255,255,255,.06)}
.st-ok .dot{background:var(--ok);box-shadow:0 0 10px var(--ok)}.st-wa .dot{background:var(--wa)}.st-bad .dot{background:var(--bad);box-shadow:0 0 10px var(--bad)}
.st-arm{border-color:rgba(255,77,77,.6)}.st-arm .dot{background:var(--bad);animation:pulse 1s infinite}
@keyframes pulse{50%{opacity:.25}}
.sp{flex:1}
#stop{height:34px;border-radius:var(--r);padding:0 18px;background:rgba(255,77,77,.16);border:1.5px solid var(--bad);color:#fff;font-weight:700;letter-spacing:1.5px;font-size:12px}
#stop:active{background:var(--bad)}
.bars{display:flex;gap:2px;align-items:flex-end;height:13px}.bars i{width:3px;background:rgba(255,255,255,.25);border-radius:1px}
.bars i:nth-child(1){height:4px}.bars i:nth-child(2){height:7px}.bars i:nth-child(3){height:10px}.bars i:nth-child(4){height:13px}.bars i.on{background:#fff}
.bat{width:24px;height:12px;border:1.5px solid rgba(255,255,255,.8);border-radius:3px;position:relative;padding:1px}
.bat:after{content:"";position:absolute;right:-4px;top:3px;width:2px;height:4px;background:rgba(255,255,255,.8);border-radius:0 1px 1px 0}
.bat u{display:block;height:100%;width:0;background:var(--ok);border-radius:1px}
.rail{position:fixed;top:50%;transform:translateY(-58%);display:flex;flex-direction:column;gap:30px;z-index:5}
.rail.l{left:calc(16px + var(--sa-l))}.rail.r{right:calc(16px + var(--sa-r))}
.rb{width:56px;height:56px;border-radius:50%;display:grid;place-items:center;padding:0;position:relative}
.rb svg{width:26px;height:26px;stroke:#fff;fill:none;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round}
.rb small{position:absolute;bottom:-17px;left:50%;transform:translateX(-50%);font-size:10px;color:var(--dim);letter-spacing:.6px;white-space:nowrap}
.rb b{font-size:12px;letter-spacing:.8px}
.rb:active{transform:scale(.94)}.rb.hot{border-color:var(--ac);box-shadow:0 0 0 3px rgba(60,200,255,.18)}
.zone{position:fixed;top:64px;bottom:0;width:50%;z-index:2}.zone.l{left:0}.zone.r{right:0}
.stick{position:fixed;bottom:calc(22px + env(safe-area-inset-bottom,0px));width:var(--s);height:var(--s);z-index:3;pointer-events:none;--s:min(40vh,196px)}
.stick.l{left:calc(max(96px,13vw) + var(--sa-l))}.stick.r{right:calc(max(96px,13vw) + var(--sa-r))}
.ring{position:absolute;inset:0;border-radius:50%;background:radial-gradient(circle,rgba(255,255,255,.04) 0,rgba(255,255,255,.09) 62%,rgba(255,255,255,.02) 63%);
border:1.5px solid rgba(255,255,255,.28);transition:border-color .15s,box-shadow .15s}
.ring:before{content:"";position:absolute;inset:26%;border-radius:50%;border:1px dashed rgba(255,255,255,.14)}
.stick.on .ring{border-color:rgba(255,255,255,.6);box-shadow:0 0 30px rgba(60,200,255,.18)}
.chev{position:absolute;inset:0}.chev i{position:absolute;width:10px;height:10px;border:solid rgba(255,255,255,.45);border-width:2px 2px 0 0}
.chev i:nth-child(1){left:50%;top:9px;transform:translateX(-50%) rotate(-45deg)}.chev i:nth-child(2){left:50%;bottom:9px;transform:translateX(-50%) rotate(135deg)}
.chev i:nth-child(3){top:50%;left:9px;transform:translateY(-50%) rotate(-135deg)}.chev i:nth-child(4){top:50%;right:9px;transform:translateY(-50%) rotate(45deg)}
.lbl{position:absolute;left:50%;top:-20px;transform:translateX(-50%);font-size:10px;letter-spacing:1.2px;color:var(--dim);white-space:nowrap}
.knob{position:absolute;left:50%;top:50%;width:34%;height:34%;margin:-17% 0 0 -17%;border-radius:50%;
background:radial-gradient(circle at 35% 30%,#fff,#dfe8ef 60%,#b9c6d1);box-shadow:0 6px 18px rgba(0,0,0,.45),0 0 0 6px rgba(255,255,255,.08);will-change:transform}
.stick.on .knob{box-shadow:0 6px 18px rgba(0,0,0,.45),0 0 0 8px rgba(60,200,255,.22)}
.track{position:absolute;left:50%;top:12%;bottom:12%;width:4px;margin-left:-2px;border-radius:2px;background:rgba(255,255,255,.12);display:none}
.sticky .track{display:block}
.tele{position:fixed;left:50%;bottom:calc(16px + env(safe-area-inset-bottom,0px));transform:translateX(-50%);display:flex;gap:18px;padding:8px 18px;border-radius:14px;z-index:4}
.tele div{display:flex;flex-direction:column;align-items:center;min-width:44px}.tele small{font-size:9px;letter-spacing:1.2px;color:var(--dim)}.tele span{font-size:16px;font-weight:600}
.hdg{position:fixed;top:56px;left:50%;transform:translateX(-50%);font-size:12px;padding:4px 10px;border-radius:var(--r);z-index:4;letter-spacing:.5px}
.banner{position:fixed;top:96px;left:50%;transform:translateX(-50%);padding:9px 16px;border-radius:12px;z-index:6;display:none;font-size:13px;text-align:center;max-width:70vw}
.banner.show{display:block}.banner.bad{border-color:rgba(255,77,77,.7)}.banner.wa{border-color:rgba(255,201,60,.6)}
#toast{position:fixed;left:50%;top:50%;transform:translate(-50%,-50%) scale(.96);padding:12px 18px;border-radius:14px;z-index:20;opacity:0;transition:.18s;pointer-events:none;text-align:center;max-width:70vw;line-height:1.45}
#toast.show{opacity:1;transform:translate(-50%,-50%) scale(1)}
.sheet{position:fixed;inset:0;z-index:30;display:none;align-items:center;justify-content:center;background:rgba(0,0,0,.38)}
.sheet.show{display:flex}
.card{border-radius:22px;padding:22px 24px;min-width:min(380px,86vw);max-width:92vw;background:var(--g2)}
.card h3{margin:0 0 4px;font-size:17px}.card p{margin:0 0 16px;color:var(--dim);font-size:13px;line-height:1.45}
.slide{position:relative;height:60px;border-radius:var(--r);background:rgba(255,255,255,.08);border:1px solid var(--ln);overflow:hidden;touch-action:none}
.slide span{position:absolute;inset:0;display:grid;place-items:center;color:var(--dim);font-size:13px;letter-spacing:.8px;padding-left:40px}
.slide u{position:absolute;left:0;top:0;bottom:0;width:0;background:linear-gradient(90deg,rgba(60,200,255,.08),rgba(60,200,255,.35))}
.thumb{position:absolute;left:4px;top:4px;width:52px;height:52px;border-radius:50%;background:#fff;display:grid;place-items:center;box-shadow:0 4px 14px rgba(0,0,0,.4)}
.thumb svg{width:24px;height:24px;stroke:#0b1117;fill:none;stroke-width:2.2;stroke-linecap:round;stroke-linejoin:round}
.cancel{margin-top:14px;width:100%;height:42px;border-radius:12px;background:rgba(255,255,255,.07);border:1px solid var(--ln)}
.drawer{position:fixed;top:0;bottom:0;right:0;width:min(380px,92vw);z-index:31;transform:translateX(105%);transition:transform .22s ease;padding:18px 20px calc(18px + env(safe-area-inset-bottom,0px));overflow-y:auto;touch-action:pan-y;background:rgba(9,13,18,.95);-webkit-backdrop-filter:blur(18px);backdrop-filter:blur(18px);border-left:1px solid var(--ln)}
.drawer.show{transform:none}
.drawer h3{margin:4px 0 14px;font-size:17px;display:flex;align-items:center;justify-content:space-between}
.set{padding:12px 0;border-bottom:1px solid rgba(255,255,255,.08)}.set label{display:block;font-size:12px;color:var(--dim);margin-bottom:8px;letter-spacing:.3px}
.seg{display:flex;background:rgba(255,255,255,.07);border-radius:10px;padding:3px}.seg button{flex:1;height:32px;border:0;border-radius:8px;background:none;color:var(--dim);font-size:13px}
.seg button.on{background:#fff;color:#0b1117;font-weight:600}
input[type=range]{width:100%;accent-color:var(--ac)}.val{float:right;color:var(--fg)}
.lnk{display:block;margin-top:16px;text-align:center;color:var(--ac);text-decoration:none;font-size:13px}
#rot{position:fixed;inset:0;z-index:50;background:#06090d;display:none;flex-direction:column;align-items:center;justify-content:center;gap:16px;text-align:center;padding:30px;color:var(--dim)}
#rot svg{width:64px;height:64px;stroke:#fff;fill:none;stroke-width:1.5;animation:rot 2.2s ease-in-out infinite}
@keyframes rot{0%,30%{transform:rotate(0)}60%,100%{transform:rotate(-90deg)}}
@media (orientation:portrait) and (max-width:820px){#rot{display:flex}}
#rot button{margin-top:6px;background:none;border:1px solid var(--ln);border-radius:var(--r);padding:8px 16px;color:var(--dim)}
@media (max-height:380px){.tele{display:none}.rail{gap:24px}.rb{width:48px;height:48px}}
</style></head><body>
<canvas id="hud"></canvas>

<div class="top">
 <a href="/" class="ib glass" aria-label="Configurator" style="color:#fff"><svg viewBox="0 0 24 24"><path d="M15 18l-6-6 6-6"/></svg></a>
 <div id="st" class="pill glass"><i class="dot"></i><span id="stt">Connecting</span></div>
 <div class="sp"></div>
 <button id="stop" aria-label="Emergency stop">STOP</button>
 <div class="sp"></div>
 <div class="pill glass" title="Link"><span class="bars" id="sig"><i></i><i></i><i></i><i></i></span><span id="rssi">--</span></div>
 <div class="pill glass" title="Battery"><span class="bat"><u id="batu"></u></span><span id="batv">--</span></div>
 <button class="ib glass" id="gear" aria-label="Settings"><svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.7 1.7 0 0 0 .3 1.8l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-1.8-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 1 1-4 0v-.1a1.7 1.7 0 0 0-1.1-1.5 1.7 1.7 0 0 0-1.8.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.8 1.7 1.7 0 0 0-1.5-1H3a2 2 0 1 1 0-4h.1a1.7 1.7 0 0 0 1.5-1.1 1.7 1.7 0 0 0-.3-1.8l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.8.3H9a1.7 1.7 0 0 0 1-1.5V3a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.8-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.8V9a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z"/></svg></button>
</div>
<div class="hdg glass" id="hdg">HDG 000&deg;</div>
<div class="banner glass" id="ban"></div>

<div class="rail l">
 <button class="rb glass" id="tko"><svg id="tkoi" viewBox="0 0 24 24"></svg><small id="tkol">TAKE OFF</small></button>
 <button class="rb glass" id="mode"><b id="model">ANG</b><small>MODE</small></button>
</div>
<div class="rail r">
 <button class="rb glass" id="spd"><b id="spdl">SLOW</b><small>SPEED</small></button>
 <button class="rb glass" id="cal"><svg viewBox="0 0 24 24"><path d="M12 3v3M12 18v3M3 12h3M18 12h3"/><circle cx="12" cy="12" r="5"/></svg><small>CALIB</small></button>
</div>

<div class="zone l" id="zl"></div><div class="zone r" id="zr"></div>
<div class="stick l" id="sl"><div class="ring"></div><div class="chev"><i></i><i></i><i></i><i></i></div><div class="track"></div><div class="knob"></div><div class="lbl" id="lbl-l">THROTTLE &middot; YAW</div></div>
<div class="stick r" id="sr"><div class="ring"></div><div class="chev"><i></i><i></i><i></i><i></i></div><div class="track"></div><div class="knob"></div><div class="lbl" id="lbl-r">PITCH &middot; ROLL</div></div>

<div class="tele glass">
 <div><small>THR</small><span id="tthr">0%</span></div><div><small>ROLL</small><span id="troll">0.0</span></div>
 <div><small>PITCH</small><span id="tpit">0.0</span></div><div><small>RC</small><span id="tsrc">--</span></div>
</div>

<div class="sheet" id="sheet"><div class="card glass">
 <h3 id="shT">Ready to take off</h3><p id="shP">Clear the area around the drone. Motors will spin up to hover throttle.</p>
 <div class="slide" id="slide"><u id="slf"></u><span id="sltxt">Slide to take off</span><div class="thumb" id="thumb"><svg viewBox="0 0 24 24"><path d="M5 12h14M13 6l6 6-6 6"/></svg></div></div>
 <button class="cancel" id="shX">Cancel</button>
</div></div>

<div class="drawer" id="drw">
 <h3>Flight settings <button class="ib glass" id="drwX" aria-label="Close"><svg viewBox="0 0 24 24"><path d="M6 6l12 12M18 6L6 18"/></svg></button></h3>
 <div class="set"><label>Stick layout</label><div class="seg" data-k="smode"><button data-v="2">Mode 2</button><button data-v="1">Mode 1</button></div></div>
 <div class="set"><label>Throttle stick</label><div class="seg" data-k="thrm"><button data-v="c">Centred (hover assist)</button><button data-v="s">Manual (stays put)</button></div></div>
 <div class="set"><label>Hover throttle <span class="val" id="vhov"></span></label><input type="range" min="20" max="75" step="1" data-k="hover"></div>
 <div class="set"><label>Throttle range around hover <span class="val" id="vrng"></span></label><input type="range" min="10" max="45" step="1" data-k="range"></div>
 <div class="set"><label>Slow speed <span class="val" id="vslow"></span></label><input type="range" min="20" max="90" step="5" data-k="slow"></div>
 <div class="set"><label>Landing time <span class="val" id="vland"></span></label><input type="range" min="2" max="8" step="0.5" data-k="land"></div>
 <div class="set"><label>Haptics</label><div class="seg" data-k="hap"><button data-v="1">On</button><button data-v="0">Off</button></div></div>
 <p style="color:var(--dim);font-size:12px;line-height:1.5;margin:14px 0 0">There is no altitude hold: "Centred" maps the stick around the hover throttle you set here, it does not hold height. Losing this page for 0.5 s starts a failsafe landing.</p>
 <a class="lnk" href="/">Open configurator</a>
</div>

<div id="rot"><svg viewBox="0 0 24 24"><rect x="7" y="2" width="10" height="20" rx="2"/><path d="M11 18h2"/></svg>
 <div style="color:#fff;font-size:16px">Turn your phone sideways</div><div>The flight view is designed for landscape.</div><button id="rotX">Continue anyway</button></div>
<div id="toast" class="glass"></div>

<script>
const $=s=>document.querySelector(s),$$=s=>[...document.querySelectorAll(s)];
const IC={up:'<path d="M12 19V7M6 12l6-6 6 6"/><path d="M4 21h16"/>',down:'<path d="M12 5v12M6 12l6 6 6-6"/><path d="M4 3h16"/>'};
const DEF={smode:'2',thrm:'c',hover:45,range:25,slow:45,land:4,hap:'1',spd:0,mode:1};
let S=Object.assign({},DEF);try{Object.assign(S,JSON.parse(localStorage.getItem('fcfly')||'{}'))}catch(e){}
const save=()=>{try{localStorage.setItem('fcfly',JSON.stringify(S))}catch(e){}};
let ws,T=null,live=false,lastMsg=0,armReq=false,phase='idle',phT=0,base=0,landFrom=0,yawZero=null;
const buzz=ms=>{if(S.hap==='1'&&navigator.vibrate)try{navigator.vibrate(ms)}catch(e){}};
function toast(m,ms){const t=$('#toast');t.innerHTML=m;t.classList.add('show');clearTimeout(toast.h);toast.h=setTimeout(()=>t.classList.remove('show'),ms||2400)}
const send=s=>{if(ws&&ws.readyState===1)ws.send(s)};

/* ---------------- sticks ---------------- */
const ST={l:{x:0,y:0,el:$('#sl'),id:null},r:{x:0,y:0,el:$('#sr'),id:null}};
function thrSide(){return S.smode==='2'?'l':'r'}
function isSticky(k){return S.thrm==='s'&&k===thrSide()}
function layout(){
 ST.l.el.classList.toggle('sticky',isSticky('l'));ST.r.el.classList.toggle('sticky',isSticky('r'));
 $('#lbl-l').innerHTML=S.smode==='2'?'THROTTLE &middot; YAW':'PITCH &middot; YAW';
 $('#lbl-r').innerHTML=S.smode==='2'?'PITCH &middot; ROLL':'THROTTLE &middot; ROLL';
 const k=thrSide();if(isSticky(k)&&ST[k].id===null&&!armReq)ST[k].y=-1;if(!isSticky(k)&&ST[k].id===null)ST[k].y=0;
 draw('l');draw('r')}
function draw(k){const s=ST[k],r=s.el.offsetWidth*0.33;s.el.querySelector('.knob').style.transform=`translate(${s.x*r}px,${-s.y*r}px)`}
function bindZone(z,k){
 const s=ST[k];
 z.addEventListener('pointerdown',e=>{if(s.id!==null)return;goFull();s.id=e.pointerId;z.setPointerCapture(e.pointerId);
  s.px=e.clientX;s.py=e.clientY;s.sx=s.x;s.sy=s.y;s.el.classList.add('on');s.anim&&cancelAnimationFrame(s.anim)});
 z.addEventListener('pointermove',e=>{if(e.pointerId!==s.id)return;const r=s.el.offsetWidth*0.33;
  let x=s.sx+(e.clientX-s.px)/r,y=s.sy-(e.clientY-s.py)/r;
  if(isSticky(k)){x=Math.max(-1,Math.min(1,x));y=Math.max(-1,Math.min(1,y))}
  else{const m=Math.hypot(x,y);if(m>1){x/=m;y/=m}}
  s.x=x;s.y=y;draw(k)});
 const up=e=>{if(e.pointerId!==s.id)return;s.id=null;s.el.classList.remove('on');
  const sticky=isSticky(k),x0=s.x,y0=s.y,t0=performance.now();
  const step=t=>{const f=Math.min(1,(t-t0)/120),e2=1-Math.pow(1-f,3);s.x=x0*(1-e2);if(!sticky)s.y=y0*(1-e2);draw(k);if(f<1)s.anim=requestAnimationFrame(step)};
  s.anim=requestAnimationFrame(step)};
 z.addEventListener('pointerup',up);z.addEventListener('pointercancel',up)}
bindZone($('#zl'),'l');bindZone($('#zr'),'r');
const dz=v=>Math.abs(v)<0.04?0:(v-Math.sign(v)*0.04)/0.96;

/* ---------------- RC frame @ 50 Hz ---------------- */
function frame(){
 const now=performance.now(),th=ST[thrSide()];                  // Mode 2: L = thr/yaw, R = pitch/roll. Mode 1: L = pitch/yaw, R = thr/roll
 const spd=S.spd?1:S.slow/100;
 const roll=dz(ST.r.x),pitch=dz(S.smode==='2'?ST.r.y:ST.l.y),yaw=dz(ST.l.x);
 let thr=0;const hov=S.hover/100;
 if(phase==='spool'){const f=Math.min(1,(now-phT)/1400);base=hov*f;thr=S.thrm==='s'?Math.max(0,(th.y+1)/2):base;if(f>=1)phase='fly'}
 else if(phase==='land'){const f=Math.min(1,(now-phT)/(S.land*1000));thr=landFrom*(1-f);if(f>=1&&now-phT>S.land*1000+400){armReq=false;phase='idle';base=0;toast('Landed')}}
 else if(phase==='fly'){thr=S.thrm==='s'?(th.y+1)/2:base+dz(th.y)*S.range/100}
 else thr=S.thrm==='s'?Math.max(0,(th.y+1)/2):0;
 thr=Math.max(0,Math.min(1,thr));frame.thr=thr;
 const us=v=>Math.round(1500+v*500);
 const ch=[us(roll*spd),us(pitch*spd),Math.round(1000+thr*1000),us(yaw*Math.max(spd,.6)),armReq?2000:1000,[2000,1000,1500][S.mode],1000,1000];
 send('J '+ch.join(' '))}
setInterval(()=>{if(!document.hidden)frame()},20);

/* ---------------- take off / land ---------------- */
const PA=['Gyro not calibrated - keep the drone still','Drone is not level','Lower the throttle','No control link','Arm switch','Motor test or update running'];
function blockers(need){if(!T)return['Not connected'];const p=T.s[4],out=[];need.forEach(i=>{if(!(p>>i&1))out.push(PA[i])});return out}
function tkoBtn(){const air=armed();$('#tkoi').innerHTML=air?IC.down:IC.up;$('#tkol').textContent=air?'LAND':'TAKE OFF';$('#tko').classList.toggle('hot',air)}
const armed=()=>T&&T.s[0]===1;
$('#tko').onclick=()=>{
 if(armed()||phase==='spool'||phase==='fly'){if(phase!=='land'){landFrom=frame.thr||0;phase='land';phT=performance.now();toast('Landing&hellip;');buzz(30)}return}
 if(T&&(T.s[2]===1||T.s[2]===4))return toast('Your radio receiver has control.<br>Turn it off to fly from this page.');
 const b=blockers([0,1,5]);if(S.thrm==='s'&&ST[thrSide()].y>-0.9)b.push('Lower the throttle stick');
 if(b.length)return toast(b.join('<br>'),3200);
 $('#shT').textContent='Ready to take off';$('#shP').textContent=S.thrm==='s'?'Clear the area. Motors arm at idle; raise the throttle stick to climb.':'Clear the area. Motors spool up to '+S.hover+'% hover throttle. Use the left stick to adjust.';
 openSheet()};
function openSheet(){$('#sheet').classList.add('show');setSlide(0)}
function closeSheet(){$('#sheet').classList.remove('show')}
$('#shX').onclick=closeSheet;
function setSlide(f){const w=$('#slide').offsetWidth-60;$('#thumb').style.transform=`translateX(${f*w}px)`;$('#slf').style.width=(f*w+56)+'px';$('#sltxt').style.opacity=1-f}
(()=>{const th=$('#thumb'),sl=$('#slide');let id=null,x0=0;
 th.addEventListener('pointerdown',e=>{id=e.pointerId;th.setPointerCapture(id);x0=e.clientX});
 th.addEventListener('pointermove',e=>{if(e.pointerId!==id)return;const w=sl.offsetWidth-60;setSlide(Math.max(0,Math.min(1,(e.clientX-x0)/w)))});
 const end=e=>{if(e.pointerId!==id)return;id=null;const w=sl.offsetWidth-60,f=(e.clientX-x0)/w;
  if(f>0.88){closeSheet();doArm()}else{const f0=Math.max(0,f),t0=performance.now();const st=t=>{const k=Math.min(1,(t-t0)/180);setSlide(f0*(1-k));if(k<1)requestAnimationFrame(st)};requestAnimationFrame(st)}};
 th.addEventListener('pointerup',end);th.addEventListener('pointercancel',end)})();
function doArm(){armReq=true;phase='arming';phT=performance.now();buzz([20,40,20])}
function tick(){
 const now=performance.now();
 if(phase==='arming'){if(armed()){phase='spool';phT=now;buzz(60)}
  else if(now-phT>1500){armReq=false;phase='idle';const b=blockers([0,1,2,3,4,5]);toast('Could not arm'+(b.length?':<br>'+b.join('<br>'):''),3400)}}
 if((phase==='spool'||phase==='fly'||phase==='land')&&T&&!armed()&&now-phT>300){
  const DRN=['','Arm switch off','Signal lost','Tilt limit exceeded','IMU failure','Stopped','Failsafe landing complete'];
  if(phase!=='land')toast('Disarmed: '+(DRN[T.s[3]]||''),3000);armReq=false;phase='idle';base=0;layout()}
 tkoBtn()}
setInterval(tick,50);

$('#stop').onclick=()=>{send('D');armReq=false;phase='idle';base=0;buzz(120);toast('<b>Motors stopped</b>');layout()};
$('#mode').onclick=()=>{S.mode=(S.mode+1)%3;save();ui()};
$('#spd').onclick=()=>{S.spd=S.spd?0:1;save();ui()};
$('#cal').onclick=()=>{if(armed())return toast('Land first');send('K');toast('Calibrating gyro<br>keep the drone still');yawZero=null};
function ui(){$('#model').textContent=['ACRO','ANG','HOR'][S.mode];$('#mode').classList.toggle('hot',S.mode===0);
 $('#spdl').textContent=S.spd?'FAST':'SLOW';$('#spd').classList.toggle('hot',!!S.spd)}

/* ---------------- settings ---------------- */
$('#gear').onclick=()=>$('#drw').classList.add('show');$('#drwX').onclick=()=>$('#drw').classList.remove('show');
function bindSettings(){
 $$('.seg').forEach(g=>{const k=g.dataset.k;const r=()=>g.querySelectorAll('button').forEach(b=>b.classList.toggle('on',b.dataset.v==S[k]));r();
  g.querySelectorAll('button').forEach(b=>b.onclick=()=>{if(armed()&&(k==='smode'||k==='thrm'))return toast('Land before changing sticks');S[k]=b.dataset.v;save();r();layout()})});
 $$('.drawer input[type=range]').forEach(i=>{const k=i.dataset.k;i.value=S[k];const v=()=>{$('#v'+({hover:'hov',range:'rng',slow:'slow',land:'land'})[k]).textContent=k==='land'?S[k]+' s':S[k]+'%'};v();
  i.oninput=()=>{S[k]=+i.value;save();v()}})}
bindSettings();

/* ---------------- telemetry + link ---------------- */
function conn(){ws=new WebSocket('ws://'+location.host+'/ws');
 ws.onopen=()=>{live=true};ws.onclose=()=>{live=false;T=null;armReq=false;phase='idle';setTimeout(conn,800)};
 ws.onmessage=e=>{let d;try{d=JSON.parse(e.data)}catch(x){return}if(d.t){T=d;lastMsg=performance.now();status()}else if(d.err)toast(d.err)}}
function status(){
 const s=T.s,b=T.b||[0,0,0,0],st=$('#st');let txt,cl;
 if(b[3]){txt='Failsafe landing';cl='st-bad'}
 else if(s[0]){txt='Flying';cl='st-arm'}
 else if(!s[5]){txt='Calibrating';cl='st-wa'}
 else if(b[2]===0){txt='No IMU';cl='st-bad'}
 else if(!(s[4]&2)){txt='Not level';cl='st-wa'}
 else{txt='Ready to fly';cl='st-ok'}
 $('#stt').textContent=txt;st.className='pill glass '+cl;
 const r=b[1];$('#rssi').textContent=r?r+' dBm':'--';const n=!r?0:r>-55?4:r>-65?3:r>-75?2:1;$$('#sig i').forEach((x,i)=>x.classList.toggle('on',i<n));
 const v=b[0]/100;if(v>0.5){const c=Math.max(1,Math.ceil(v/4.35)),pc=Math.max(0,Math.min(1,(v/c-3.4)/0.8));
  $('#batv').textContent=v.toFixed(1)+' V';const u=$('#batu');u.style.width=pc*100+'%';u.style.background=pc<.2?'var(--bad)':pc<.4?'var(--wa)':'var(--ok)'}
 $('#tthr').textContent=Math.round((frame.thr||0)*100)+'%';$('#troll').textContent=T.t[0].toFixed(1);$('#tpit').textContent=T.t[1].toFixed(1);
 $('#tsrc').textContent=['--','CRSF','UDP','WEB','PPM'][s[2]]||'--';
 const ban=$('#ban');
 if(s[2]===1||s[2]===4){ban.textContent='Radio receiver has priority - on-screen sticks are ignored';ban.className='banner glass wa show'}
 else if(b[3]){ban.textContent='Link lost - failsafe landing in progress';ban.className='banner glass bad show'}
 else ban.className='banner glass'}
setInterval(()=>{if(live&&performance.now()-lastMsg>1500){$('#stt').textContent='No telemetry';$('#st').className='pill glass st-bad'}
 if(!live){$('#stt').textContent='Reconnecting';$('#st').className='pill glass st-bad'}},500);

/* ---------------- HUD ---------------- */
const cv=$('#hud'),g=cv.getContext('2d');let W=0,H=0,DPR=1,aR=0,aP=0,aY=0;
function rs(){DPR=Math.min(2,window.devicePixelRatio||1);W=innerWidth;H=innerHeight;cv.width=W*DPR;cv.height=H*DPR;g.setTransform(DPR,0,0,DPR,0,0);layout()}
addEventListener('resize',rs);
function hud(){requestAnimationFrame(hud);
 const tr=T?T.t[0]:0,tp=T?T.t[1]:0,ty=T?T.t[2]:0;aR+=(tr-aR)*.35;aP+=(tp-aP)*.35;let dy=((ty-aY+540)%360)-180;aY=(aY+dy*.35+360)%360;
 const cx=W/2,cy=H*.5,ppd=H/70;
 const R=Math.hypot(W,H),ar=Math.min(W,H)*.36;
 g.save();g.translate(cx,cy);g.rotate(-aR*Math.PI/180);g.translate(0,aP*ppd);
 let gr=g.createLinearGradient(0,-R,0,0);gr.addColorStop(0,'#05101d');gr.addColorStop(.7,'#0f3354');gr.addColorStop(1,'#1d5b86');g.fillStyle=gr;g.fillRect(-R,-R,2*R,R);
 gr=g.createLinearGradient(0,0,0,R);gr.addColorStop(0,'#2b2418');gr.addColorStop(.35,'#15110b');gr.addColorStop(1,'#070604');g.fillStyle=gr;g.fillRect(-R,0,2*R,R);
 g.strokeStyle='rgba(255,255,255,.85)';g.lineWidth=1.5;g.beginPath();g.moveTo(-R,0);g.lineTo(R,0);g.stroke();
 g.font='600 11px system-ui,sans-serif';g.textAlign='left';g.textBaseline='middle';
 g.save();g.translate(0,-aP*ppd);g.beginPath();g.arc(0,0,ar*.78,0,7);g.clip();g.translate(0,aP*ppd);   // ladder only inside the roll arc
 for(let a=-60;a<=60;a+=5){if(!a)continue;const y=-a*ppd,w=a%10?18:42;g.strokeStyle=a%10?'rgba(255,255,255,.28)':'rgba(255,255,255,.55)';g.lineWidth=1.2;
  g.beginPath();if(a<0)g.setLineDash([5,4]);g.moveTo(-w,y);g.lineTo(w,y);g.stroke();g.setLineDash([]);
  if(!(a%10)){g.fillStyle='rgba(255,255,255,.6)';g.fillText(Math.abs(a),w+6,y);g.textAlign='right';g.fillText(Math.abs(a),-w-6,y);g.textAlign='left'}}
 g.restore();g.restore();
 // roll arc
 g.save();g.translate(cx,cy);g.strokeStyle='rgba(255,255,255,.45)';g.lineWidth=1.3;
 g.beginPath();g.arc(0,0,ar,Math.PI*1.25,Math.PI*1.75);g.stroke();
 [-60,-45,-30,-20,-10,0,10,20,30,45,60].forEach(a=>{const t=(a-90)*Math.PI/180,l=a%30?7:13;g.beginPath();g.moveTo(Math.cos(t)*ar,Math.sin(t)*ar);g.lineTo(Math.cos(t)*(ar-l),Math.sin(t)*(ar-l));g.stroke()});
 g.rotate(-aR*Math.PI/180);g.fillStyle='#fff';g.beginPath();g.moveTo(0,-ar+16);g.lineTo(-7,-ar+28);g.lineTo(7,-ar+28);g.closePath();g.fill();g.restore();
 // aircraft symbol
 g.save();g.translate(cx,cy);g.strokeStyle='#ffc93c';g.lineWidth=3;g.lineCap='round';g.lineJoin='round';
 g.shadowColor='rgba(0,0,0,.6)';g.shadowBlur=6;g.beginPath();g.moveTo(-70,0);g.lineTo(-26,0);g.lineTo(-13,12);g.lineTo(0,0);g.lineTo(13,12);g.lineTo(26,0);g.lineTo(70,0);g.stroke();
 g.fillStyle='#ffc93c';g.beginPath();g.arc(0,0,3,0,7);g.fill();g.restore();
 // vignette
 const vg=g.createRadialGradient(cx,cy,Math.min(W,H)*.35,cx,cy,R*.62);vg.addColorStop(0,'rgba(0,0,0,0)');vg.addColorStop(1,'rgba(0,0,0,.55)');g.fillStyle=vg;g.fillRect(0,0,W,H);
 // heading
 if(yawZero===null&&T)yawZero=ty;const hd=Math.round(((aY-(yawZero||0))%360+360)%360);$('#hdg').textContent='HDG '+String(hd).padStart(3,'0')+'°'}

/* ---------------- fullscreen / orientation ---------------- */
let fullTried=false;function goFull(){if(fullTried)return;fullTried=true;const d=document.documentElement;
 try{const p=(d.requestFullscreen||d.webkitRequestFullscreen||function(){}).call(d);p&&p.then&&p.then(()=>screen.orientation&&screen.orientation.lock&&screen.orientation.lock('landscape').catch(()=>{})).catch(()=>{})}catch(e){}}
$('#rotX').onclick=()=>{$('#rot').style.display='none'};
document.addEventListener('visibilitychange',()=>{if(document.hidden&&(armed()||armReq)){/* frames stop -> FC failsafe lands */}});
document.addEventListener('contextmenu',e=>e.preventDefault());
ui();rs();hud();conn();
</script></body></html>)HTML";
