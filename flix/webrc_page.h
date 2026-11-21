// Copyright (c) 2026 Pavel Arkhipov
// Repository: https://github.com/okalachev/flix

// Web remote control page, served by webrc.ino
#pragma once

const char WEBRC_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="theme-color" content="#0b0e13">
<title>Flix Remote</title>
<style>
:root{
	--bg:#0b0e13;--panel:rgba(255,255,255,.035);--line:rgba(255,255,255,.09);--text:#e9eef5;--dim:#7d8896;
	--accent:#4cc2ff;--ok:#3ddc97;--warn:#ffc857;--bad:#ff5470;
	--js:clamp(140px,min(40vw,62vh),360px);--pad:clamp(6px,1.6vmin,14px)
}
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent;user-select:none;-webkit-user-select:none}
html,body{height:100%;color:var(--text);font:14px/1.25 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;overflow:hidden;overscroll-behavior:none;
	background:radial-gradient(1200px 600px at 50% -10%,#1a2230 0,var(--bg) 60%) fixed,var(--bg)}
.num{font-variant-numeric:tabular-nums}
.wrap{position:relative;height:100vh;height:100dvh;display:flex;flex-direction:column;gap:var(--pad);
	padding:var(--pad) max(var(--pad),env(safe-area-inset-right)) max(var(--pad),env(safe-area-inset-bottom)) max(var(--pad),env(safe-area-inset-left))}
.wrap::after{content:"";position:fixed;inset:0;pointer-events:none;border-radius:4px;box-shadow:inset 0 0 0 0 transparent;transition:box-shadow .3s}
body.armed .wrap::after{box-shadow:inset 0 0 0 3px var(--bad),inset 0 0 40px rgba(255,84,112,.35)}

/* status bar */
.bar{display:flex;align-items:center;gap:6px;overflow-x:auto;scrollbar-width:none}
.bar::-webkit-scrollbar{display:none}
.brand{font-weight:800;letter-spacing:.12em;font-size:.8rem;color:var(--dim);padding:0 4px 0 2px;white-space:nowrap}
.brand b{color:var(--accent)}
.chip{display:flex;align-items:center;gap:6px;height:28px;padding:0 10px;border-radius:999px;background:var(--panel);border:1px solid var(--line);
	font-size:.78rem;white-space:nowrap}
.chip .k{color:var(--dim);font-size:.68rem;letter-spacing:.06em;text-transform:uppercase}
.dot{width:8px;height:8px;border-radius:50%;background:var(--bad);box-shadow:0 0 10px var(--bad)}
.dot.ok{background:var(--ok);box-shadow:0 0 10px var(--ok)}.dot.slow{background:var(--warn);box-shadow:0 0 10px var(--warn)}
#armChip{font-weight:800;letter-spacing:.08em;color:var(--dim)}
body.armed #armChip{background:var(--bad);border-color:var(--bad);color:#fff}
.batt{width:22px;height:11px;border:1.5px solid var(--dim);border-radius:3px;position:relative;padding:1px}
.batt::after{content:"";position:absolute;right:-4px;top:2.5px;width:2px;height:4px;background:var(--dim);border-radius:0 1px 1px 0}
.batt i{display:block;height:100%;width:0;border-radius:1px;background:var(--ok);transition:width .5s}
.sp{flex:1}
#battChip.lowbatt{border-color:var(--bad);color:var(--bad);animation:blink 1s steps(2) infinite}
@keyframes blink{50%{opacity:.35}}

/* main area */
.content{flex:1;min-height:0;display:grid;grid-template-columns:1fr auto 1fr;gap:var(--pad)}
.panel{background:var(--panel);border:1px solid var(--line);border-radius:22px;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:10px;min-width:0;min-height:0;position:relative;overflow:hidden}
/* stick panels shrink with the space left (e.g. when the console is open): size sticks by the panel, not the screen */
.side{container-type:size}
.side>.js{--s:min(var(--js),calc(100cqh - 44px),calc(100cqw - 24px))}
.stickrow{--s:min(var(--js),calc(100cqh - 44px),calc(100cqw - 48px))}
.cap{position:absolute;top:10px;left:14px;right:14px;display:flex;justify-content:space-between;color:var(--dim);font-size:.68rem;letter-spacing:.08em;text-transform:uppercase}
.cap b{color:var(--text);font-weight:600}

/* sticks */
.stickrow{display:flex;align-items:center;gap:10px}
.js{width:var(--s,var(--js));height:var(--s,var(--js));border-radius:50%;flex-shrink:0;position:relative;touch-action:none;
	background:radial-gradient(circle,rgba(76,194,255,.06) 0,rgba(255,255,255,.02) 55%,rgba(0,0,0,.25) 100%);
	border:1px solid var(--line);box-shadow:inset 0 0 30px rgba(0,0,0,.45)}
.js svg{position:absolute;inset:0;width:100%;height:100%;pointer-events:none}
.kn{position:absolute;left:50%;top:50%;width:30%;height:30%;border-radius:50%;transform:translate(-50%,-50%);pointer-events:none;
	background:radial-gradient(circle at 35% 30%,#ffffff 0,#cfe9ff 18%,#4cc2ff 60%,#1d6fa5 100%);
	box-shadow:0 6px 18px rgba(0,0,0,.55),0 0 0 6px rgba(76,194,255,.12)}
.js.active{border-color:rgba(76,194,255,.55);box-shadow:inset 0 0 30px rgba(0,0,0,.45),0 0 24px rgba(76,194,255,.25)}
.thr{width:10px;height:calc(var(--s,var(--js))*.8);flex-shrink:0;border-radius:6px;background:rgba(255,255,255,.06);border:1px solid var(--line);position:relative;overflow:hidden}
.thr i{position:absolute;left:0;right:0;bottom:0;height:0;background:linear-gradient(to top,var(--ok),var(--warn) 70%,var(--bad))}

/* center */
.center{width:clamp(210px,30vw,320px);padding:12px;container-type:size}
.instr{display:flex;gap:10px;align-items:center;justify-content:center;width:100%;flex-shrink:1;min-height:0}
.instr svg{width:min(42%,124px,30cqh);height:auto;aspect-ratio:1}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:clamp(4px,2cqh,8px);width:100%}
.b{height:clamp(30px,13cqh,58px);border-radius:14px;border:1px solid var(--line);background:rgba(255,255,255,.05);color:var(--text);
	font:700 .82rem system-ui;letter-spacing:.06em;display:flex;align-items:center;justify-content:center;gap:6px;touch-action:manipulation;transition:transform .08s,background .2s}
.b:active{transform:scale(.95)}
.b svg{width:18px;height:18px}
.b.arm{background:linear-gradient(180deg,rgba(61,220,151,.22),rgba(61,220,151,.08));border-color:rgba(61,220,151,.5);color:#bff5dc}
.b.stop{background:linear-gradient(180deg,#ff5470,#c8304b);border-color:#ff8095;color:#fff;box-shadow:0 6px 18px rgba(255,84,112,.3)}
.b.on{border-color:var(--accent);color:var(--accent);background:rgba(76,194,255,.12)}
.seg{display:flex;border:1px solid var(--line);border-radius:14px;overflow:hidden;height:clamp(26px,10cqh,44px)}
.seg button{flex:1;border:0;background:transparent;color:var(--dim);font:700 .75rem system-ui;letter-spacing:.08em}
.seg button.on{background:var(--accent);color:#04121c}
.wide{grid-column:span 2}
.row3{display:grid;grid-template-columns:1fr 1fr 1.7fr;gap:inherit}
.row3 .b{gap:4px;padding:0 4px;min-width:0}.row3 .b svg{width:16px;height:16px;flex-shrink:0}
/* tilt control: gain slider under the right stick, which then only shows the phone tilt */
.gain{display:none;position:absolute;left:14px;right:14px;bottom:8px;align-items:center;gap:8px;color:var(--dim);font-size:.68rem;letter-spacing:.08em;text-transform:uppercase}
.gain input{flex:1;min-width:0;accent-color:var(--accent);height:22px;touch-action:pan-x}
.gain b{color:var(--text);min-width:3.2em;text-align:right}
.side.tilt .gain,.side.phone .gain{display:flex}
.side.phone .stickrow{--s:min(var(--js),calc(100cqh - 84px),calc(100cqw - 48px))}
.gain button{border:1px solid var(--line);border-radius:999px;background:rgba(255,255,255,.05);color:var(--dim);font:700 .68rem system-ui;letter-spacing:.08em;padding:4px 10px}
.gain button.on{border-color:var(--ok);color:var(--ok);background:rgba(61,220,151,.12)}
.gain input:disabled{opacity:.3}
#toast{max-width:min(90vw,520px);text-align:center}
.side.tilt>.js{--s:min(var(--js),calc(100cqh - 84px),calc(100cqw - 24px));pointer-events:none}
.side.tilt .kn{background:radial-gradient(circle at 35% 30%,#fff 0,#d9ffe9 18%,#3ddc97 60%,#1d7a52 100%);box-shadow:0 6px 18px rgba(0,0,0,.55),0 0 0 6px rgba(61,220,151,.12)}

/* console */
.con{display:none;flex-direction:column;gap:6px;background:rgba(5,7,10,.92);border:1px solid var(--line);border-radius:16px;padding:8px;height:min(240px,40vh);flex-shrink:0}
.con.open{display:flex}
#log{flex:1;overflow-y:auto;font:12px/1.35 ui-monospace,"Cascadia Mono",Consolas,monospace;color:#9ff0c8;white-space:pre-wrap;word-break:break-word;user-select:text;-webkit-user-select:text;padding:2px 4px}
.row{display:flex;gap:6px;flex-wrap:wrap}
.row input{flex:1;min-width:0;background:rgba(255,255,255,.05);border:1px solid var(--line);border-radius:10px;color:var(--text);padding:8px 10px;font:13px ui-monospace,monospace;user-select:text;outline:none}
.row input:focus{border-color:var(--accent)}
.row button{border:1px solid var(--line);border-radius:10px;background:rgba(255,255,255,.05);color:var(--text);padding:5px 10px;font:600 .75rem system-ui}
.row button.send{background:var(--accent);border-color:var(--accent);color:#04121c}
#toast{position:fixed;top:max(52px,9%);left:50%;transform:translate(-50%,-8px);background:rgba(15,19,26,.92);border:1px solid var(--line);padding:10px 18px;border-radius:12px;
	font-weight:600;pointer-events:none;opacity:0;transition:opacity .25s,transform .25s;z-index:9;backdrop-filter:blur(8px)}
#toast.show{opacity:1;transform:translate(-50%,0)}

@media (orientation:portrait){
	:root{--js:clamp(120px,42vw,260px)}
	.bar{flex-wrap:wrap;justify-content:center}.bar .sp{display:none}
	.content{grid-template-columns:1fr 1fr;grid-template-rows:auto 1fr}
	.center{grid-column:1/3;grid-row:1;width:auto;container-type:normal}
	.instr svg{width:min(42%,124px)}.b{height:clamp(40px,7vh,58px)}.seg{height:clamp(34px,5vh,44px)}
	.thr{display:none}
}
@media (max-height:430px) and (orientation:landscape){
	.chip{height:24px;font-size:.7rem;padding:0 8px}.brand{display:none}
	.cap{top:6px}.center{padding:8px}.instr svg{width:min(40%,96px,30cqh)}
}
</style>
</head>
<body>
<div class="wrap">
	<div class="bar">
		<div class="brand">FLIX <b>RC</b></div>
		<div class="chip"><span class="dot" id="dot"></span><span id="conn">Connecting</span></div>
		<div class="chip" id="armChip">DISARMED</div>
		<div class="chip"><span class="k">Mode</span><b id="mode">–</b></div>
		<div class="chip" id="battChip"><span class="batt"><i id="battBar"></i></span><b class="num" id="v">–</b></div>
		<div class="sp"></div>
		<div class="chip num"><span class="k">Ping</span><span id="lat">–</span></div>
		<div class="chip num"><span class="k">Loss</span><span id="loss">0%</span></div>
		<div class="chip num"><span class="k">Loop</span><span id="hz">–</span></div>
	</div>

	<div class="content">
		<div class="panel side">
			<div class="cap"><span>Throttle · Yaw</span><span class="num"><b id="vt">0</b>% · <b id="vy">0</b></span></div>
			<div class="stickrow"><div class="thr"><i id="thrBar"></i></div><div class="js" id="jl"></div></div>
			<div class="gain"><button id="bLift">LIFT</button><input id="lgain" type="range" min="20" max="150" step="5" disabled><b class="num" id="lgainTxt"></b></div>
		</div>

		<div class="panel center">
			<div class="instr">
				<svg viewBox="-50 -50 100 100" aria-label="attitude">
					<defs><clipPath id="hc"><circle r="46"/></clipPath>
						<linearGradient id="sky" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#1b5e9e"/><stop offset="1" stop-color="#4fa3e0"/></linearGradient>
						<linearGradient id="gnd" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#8a5a2e"/><stop offset="1" stop-color="#4e3218"/></linearGradient></defs>
					<g clip-path="url(#hc)"><g id="horizon">
						<rect x="-150" y="-150" width="300" height="150" fill="url(#sky)"/><rect x="-150" y="0" width="300" height="150" fill="url(#gnd)"/>
						<line x1="-150" x2="150" stroke="#fff" stroke-width="1"/>
						<g stroke="#fff" stroke-opacity=".7" stroke-width=".8"><line x1="-8" x2="8" y1="-12" y2="-12"/><line x1="-8" x2="8" y1="12" y2="12"/><line x1="-14" x2="14" y1="-24" y2="-24"/><line x1="-14" x2="14" y1="24" y2="24"/></g>
					</g></g>
					<circle r="46" fill="none" stroke="rgba(255,255,255,.18)" stroke-width="2"/>
					<path d="M-26 0h14l5 6 5-6h0M7 0h5l0 0h14" fill="none" stroke="#ffc857" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>
					<circle r="2" fill="#ffc857"/>
					<text id="attTxt" y="40" text-anchor="middle" fill="#fff" font-size="8" font-family="system-ui" opacity=".85">–</text>
				</svg>
				<svg viewBox="-50 -50 100 100" aria-label="motors">
					<g stroke="rgba(255,255,255,.25)" stroke-width="4" stroke-linecap="round"><line x1="-27" y1="-27" x2="27" y2="27"/><line x1="27" y1="-27" x2="-27" y2="27"/></g>
					<rect x="-9" y="-9" width="18" height="18" rx="4" fill="#1a2230" stroke="rgba(255,255,255,.3)"/>
					<path d="M0 -15l5 6h-10z" fill="#4cc2ff"/>
					<g id="mot"></g>
				</svg>
			</div>
			<div class="grid">
				<button class="b arm" id="bArm"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round"><rect x="5" y="11" width="14" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 7.5-2"/></svg>ARM</button>
				<button class="b stop" id="bStop"><svg viewBox="0 0 24 24" fill="currentColor"><rect x="6" y="6" width="12" height="12" rx="2"/></svg>STOP</button>
				<div class="seg wide" id="modes"><button data-m="stab">STAB</button><button data-m="acro">ACRO</button><button data-m="raw">RAW</button></div>
				<div class="row3 wide">
					<button class="b" id="bSens"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2"><circle cx="12" cy="12" r="8"/><circle cx="12" cy="12" r="3"/></svg><span id="sens">60%</span></button>
					<button class="b" id="bTilt">TILT</button>
					<button class="b" id="bCon"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round"><path d="M5 8l4 4-4 4M12 17h7"/></svg>CONSOLE</button>
				</div>
			</div>
		</div>

		<div class="panel side">
			<div class="cap"><span>Pitch · Roll</span><span class="num"><b id="vp">0</b> · <b id="vr">0</b></span></div>
			<div class="js" id="jr"></div>
			<div class="gain"><span>Tilt gain</span><input id="gain" type="range" min="20" max="150" step="5"><b class="num" id="gainTxt"></b></div>
		</div>
	</div>

	<div class="con" id="con">
		<div id="log"></div>
		<div class="row" id="quick"></div>
		<form class="row" id="cf"><input id="cin" autocomplete="off" autocapitalize="off" spellcheck="false" placeholder="Command: st · imu · p NAME VALUE · help"><button class="send">Send</button></form>
	</div>
</div>
<div id="toast"></div>
<script>
const $ = id => document.getElementById(id);
const NS = 'http://www.w3.org/2000/svg';
const EXPO = 0.4;
const curve = v => Math.sign(v) * (Math.abs(v) * (1 - EXPO) + Math.abs(v) ** 3 * EXPO);
const st = {x1: 0, y1: -1, x2: 0, y2: 0, armed: false, mode: ''};
let active = false; // the page only watches telemetry until the user touches a stick or arms, so it never fights another remote
const SENS = [40, 60, 80, 100];
let sens = 60;
try { sens = +localStorage.getItem('sens') || 60; } catch (_) {}
$('sens').textContent = sens + '%';

function toast(t, ms) { const e = $('toast'); e.textContent = t; e.classList.add('show'); clearTimeout(e._t); e._t = setTimeout(() => e.classList.remove('show'), ms || 1800); }
const vib = ms => navigator.vibrate && navigator.vibrate(ms);

// Stick face: ring ticks drawn once
function face(el) {
	const s = document.createElementNS(NS, 'svg'); s.setAttribute('viewBox', '-50 -50 100 100');
	let h = '<circle r="34" fill="none" stroke="rgba(255,255,255,.07)"/><circle r="17" fill="none" stroke="rgba(255,255,255,.05)"/>';
	for (let a = 0; a < 360; a += 15) {
		const r1 = a % 90 ? 46 : 42, c = Math.cos(a * Math.PI / 180), n = Math.sin(a * Math.PI / 180);
		h += `<line x1="${c * r1}" y1="${n * r1}" x2="${c * 48}" y2="${n * 48}" stroke="rgba(255,255,255,${a % 90 ? .12 : .3})" stroke-width="${a % 90 ? .6 : 1.2}"/>`;
	}
	s.innerHTML = h; el.appendChild(s);
	const k = document.createElement('div'); k.className = 'kn'; el.appendChild(k); return k;
}
// Left Y (throttle) stays where released; everything else springs back to center.
function stick(el, holdY, set) {
	const kn = face(el);
	let id = null;
	const R = () => (el.clientWidth - kn.clientWidth) / 2;
	function place(x, y, anim) {
		kn.style.transition = anim ? 'transform .18s ease-out' : '';
		kn.style.transform = `translate(calc(-50% + ${x * R()}px), calc(-50% + ${-y * R()}px))`;
		set(x, y);
	}
	function move(e) {
		const b = el.getBoundingClientRect();
		let x = (e.clientX - b.left - b.width / 2) / R(), y = -(e.clientY - b.top - b.height / 2) / R();
		if (holdY) { x = Math.max(-1, Math.min(1, x)); y = Math.max(-1, Math.min(1, y)); }
		else { const d = Math.hypot(x, y); if (d > 1) { x /= d; y /= d; } }
		place(x, y);
	}
	el.addEventListener('pointerdown', e => { if (id !== null) return; e.preventDefault(); active = true; id = e.pointerId; el.setPointerCapture(id); el.classList.add('active'); move(e); });
	el.addEventListener('pointermove', e => { if (e.pointerId === id) move(e); });
	const end = e => { if (e.pointerId !== id) return; id = null; el.classList.remove('active'); place(0, holdY ? st.y1 : 0, true); };
	el.addEventListener('pointerup', end); el.addEventListener('pointercancel', end);
	el.addEventListener('touchstart', e => e.preventDefault(), {passive: false});
	const init = () => place(0, holdY ? st.y1 : 0); requestAnimationFrame(init); addEventListener('resize', init);
	return {place, get held() { return id !== null; }};
}
const L = stick($('jl'), true, (x, y) => { st.x1 = x; st.y1 = y; });
const R = stick($('jr'), false, (x, y) => { st.x2 = x; st.y2 = y; });

// Motor diagram: firmware order RL, RR, FR, FL
const MPOS = [[-27, 27], [27, 27], [27, -27], [-27, -27]], CW = [false, true, false, true];
const motEls = MPOS.map(([x, y], i) => {
	const g = document.createElementNS(NS, 'g'); g.setAttribute('transform', `translate(${x} ${y})`);
	g.innerHTML = `<circle r="15" fill="rgba(255,255,255,.04)" stroke="rgba(255,255,255,.18)" stroke-dasharray="${CW[i] ? '3 2' : '1 2'}"/>` +
		`<circle r="15" fill="none" stroke="#3ddc97" stroke-width="3.5" stroke-linecap="round" transform="rotate(-90)" pathLength="100" stroke-dasharray="0 100"/>` +
		`<text y="3" text-anchor="middle" fill="#e9eef5" font-size="8" font-family="system-ui">0</text>`;
	$('mot').appendChild(g); return g;
});

function values() {
	const k = sens / 100;
	return {th: (st.y1 + 1) / 2, y: curve(st.x1), r: curve(st.x2) * k, p: curve(st.y2) * k};
}

// Control link: one request in flight, ~20 Hz. If the page stops sending, the drone lands itself.
const rcUrl = v => `/rc?th=${v.th.toFixed(3)}&y=${v.y.toFixed(3)}&r=${v.r.toFixed(3)}&p=${v.p.toFixed(3)}`;
let sent = 0, lost = 0, fails = 0, lats = [];
async function loop() {
	for (;;) {
		const t0 = performance.now();
		const v = values();
		$('vt').textContent = Math.round(v.th * 100); $('vy').textContent = Math.round(v.y * 100);
		$('vr').textContent = Math.round(v.r * 100); $('vp').textContent = Math.round(v.p * 100);
		$('thrBar').style.height = v.th * 100 + '%';
		if (document.visibilityState === 'visible') {
			sent++;
			try {
				const ac = new AbortController(), to = setTimeout(() => ac.abort(), 600);
				const s = await (await fetch(active ? rcUrl(v) : '/status', {signal: ac.signal, cache: 'no-store'})).json();
				clearTimeout(to); fails = 0;
				lats.push(performance.now() - t0); if (lats.length > 10) lats.shift();
				status(s);
				record(s, v, performance.now() - t0);
			} catch (e) { lost++; if (++fails >= 3) conn(false); }
			$('loss').textContent = (lost / sent * 100).toFixed(1) + '%';
		}
		await new Promise(r => setTimeout(r, Math.max(5, 50 - (performance.now() - t0))));
	}
}
function conn(ok, lat) {
	$('dot').className = 'dot' + (ok ? (lat > 100 ? ' slow' : ' ok') : '');
	$('conn').textContent = ok ? (active ? 'Connected' : 'View only') : 'No link';
}
function status(s) {
	const lat = lats.reduce((a, b) => a + b, 0) / lats.length;
	conn(true, lat); $('lat').textContent = Math.round(lat) + ' ms'; $('hz').textContent = Math.round(s.hz) + ' Hz';
	if (s.armed != st.armed) { st.armed = !!s.armed; toast(st.armed ? 'Armed — motors live' : 'Disarmed'); vib(st.armed ? 80 : 30); }
	document.body.classList.toggle('armed', st.armed); $('armChip').textContent = st.armed ? 'ARMED' : 'DISARMED';
	if (st.mode && s.mode !== st.mode) toast('Mode: ' + s.mode);
	st.mode = s.mode; $('mode').textContent = s.mode;
	for (const b of $('modes').children) b.classList.toggle('on', b.textContent === s.mode);
	st.v = s.v; st.vlow = s.vlow;
	if (s.low && !st.low) { toast('Battery low — landing'); vib([300, 100, 300]); }
	st.low = !!s.low; $('battChip').classList.toggle('lowbatt', st.low);
	if (s.v > 0.5) {
		const pct = Math.max(0, Math.min(1, (s.v - 3.3) / 0.9)); // 1S battery
		$('v').textContent = s.v.toFixed(2) + ' V'; $('battBar').style.width = pct * 100 + '%';
		$('battBar').style.background = pct > .4 ? 'var(--ok)' : pct > .15 ? 'var(--warn)' : 'var(--bad)';
	} else $('v').textContent = '–';
	$('attTxt').textContent = `${s.roll.toFixed(0)}° ${s.pitch.toFixed(0)}°`;
	horizon(s.roll, s.pitch);
	s.mot.forEach((m, i) => {
		const [ring, arc, txt] = [motEls[i].children[0], motEls[i].children[1], motEls[i].children[2]];
		arc.setAttribute('stroke-dasharray', `${m * 100} 100`); arc.setAttribute('stroke-opacity', m > .01 ? 1 : 0); arc.setAttribute('stroke',m > .85 ? '#ff5470' : m > .6 ? '#ffc857' : '#3ddc97');
		txt.textContent = Math.round(m * 100);
	});
}
function horizon(roll, pitch) {
	$('horizon').setAttribute('transform', `rotate(${-roll}) translate(0 ${Math.max(-40, Math.min(40, pitch * 1.2))})`);
}

$('bArm').onclick = async () => {
	if (st.armed) return;
	if (values().th > 0.05) { toast('Lower the throttle to arm'); vib(200); return; }
	if (st.v > 0.5 && st.v <= st.vlow) { toast(`Battery too low to arm (${st.v.toFixed(2)} V)`); vib(200); return; }
	active = true;
	try { await fetch(rcUrl(values()), {cache: 'no-store'}); await fetch('/arm'); } catch (e) {} // fresh sticks first
	setTimeout(() => { if (!st.armed) toast('Arming refused — see console'); }, 800);
};
$('bStop').onclick = () => { fetch('/disarm'); L.place(0, -1, true); vib(50); };
for (const b of $('modes').children) b.onclick = () => fetch('/mode?m=' + b.dataset.m);
$('bSens').onclick = () => {
	sens = SENS[(SENS.indexOf(sens) + 1) % SENS.length] || 60; $('sens').textContent = sens + '%'; toast('Stick sensitivity ' + sens + '%');
	try { localStorage.setItem('sens', sens); } catch (_) {}
};
document.addEventListener('contextmenu', e => e.preventDefault());

// Tilt control: the phone's tilt replaces the right stick (roll/pitch); throttle and yaw stay on the left stick.
// The pose of the phone at the moment the mode is switched on is the neutral one.
const TILT_FULL = 30; // phone tilt that gives full stick deflection at 100% gain, degrees
let tilt = false, tilt0 = null, tiltSeen = 0, gain = 100;
try { gain = +localStorage.getItem('gain') || 100; } catch (_) {}
$('gain').value = gain; $('gainTxt').textContent = gain + '%';
$('gain').oninput = e => { gain = +e.target.value; $('gainTxt').textContent = gain + '%'; try { localStorage.setItem('gain', gain); } catch (_) {} };
function onTilt(e) {
	if (!tilt || e.beta === null || e.gamma === null) return;
	tiltSeen = performance.now();
	const a = (screen.orientation ? screen.orientation.angle : window.orientation) || 0;
	// tilt to the right and forward as seen by the user, for the current screen rotation
	const right = a === 90 ? e.beta : a === 270 || a === -90 ? -e.beta : a === 180 ? -e.gamma : e.gamma;
	const fwd = a === 90 ? e.gamma : a === 270 || a === -90 ? -e.gamma : a === 180 ? e.beta : -e.beta;
	if (!tilt0) tilt0 = {right, fwd};
	const k = gain / 100 / TILT_FULL, lim = v => Math.max(-1, Math.min(1, Math.abs(v) < 0.05 ? 0 : v));
	R.place(lim((right - tilt0.right) * k), lim((fwd - tilt0.fwd) * k));
}
function setTilt(on) {
	tilt = on; tilt0 = null;
	$('bTilt').classList.toggle('on', on); $('jr').parentNode.classList.toggle('tilt', on);
	$('jl').closest('.side').classList.toggle('phone', on); if (!on) setLift(false);
	R.place(0, 0, true); dispatchEvent(new Event('resize'));
	if (on) { active = true; toast('Tilt control: hold the phone as you like, this pose is neutral'); }
}
$('bTilt').onclick = async () => {
	if (tilt) return setTilt(false);
	try {
		if (window.DeviceOrientationEvent && DeviceOrientationEvent.requestPermission && await DeviceOrientationEvent.requestPermission() !== 'granted') return toast('Motion sensors: permission denied');
	} catch (e) {}
	addEventListener('deviceorientation', onTilt);
	tiltSeen = 0; setTilt(true);
	setTimeout(() => { if (tilt && !tiltSeen) { setTilt(false); toast(NO_SENSORS, 9000); } }, 1500);
};
setInterval(() => { if (tilt && tiltSeen && performance.now() - tiltSeen > 500) R.place(0, 0, true); }, 250); // sensor stalled: center
addEventListener('orientationchange', () => { tilt0 = null; });
const NO_SENSORS = "No motion sensor data: browsers give it to secure (HTTPS) pages only. Chrome on Android: open chrome://flags, find 'Insecure origins treated as secure', add http://192.168.4.1, enable and relaunch.";

// Lift control (experimental, part of the tilt control): raising or lowering the phone changes the throttle.
// The height is integrated from the accelerometer, so it is approximate: the velocity is zeroed whenever
// the phone is still to stop the drift. A finger on the left stick always wins and sets the new base.
const LIFT_FULL = 0.23; // phone travel for the full throttle range at 100% gain, m
let lift = false, lv = 0, lz = 0, lbase = 0, lt = 0, lquiet = 0, liftSeen = 0, lgain = 100;
try { lgain = +localStorage.getItem('lgain2') || 100; } catch (_) {}
$('lgain').value = lgain; $('lgainTxt').textContent = lgain + '%';
$('lgain').oninput = e => { lgain = +e.target.value; $('lgainTxt').textContent = lgain + '%'; try { localStorage.setItem('lgain2', lgain); } catch (_) {} };
function onMotion(e) {
	const a = e.acceleration, g = e.accelerationIncludingGravity;
	if (!lift || !a || a.x === null || !g || g.x === null) return;
	liftSeen = performance.now();
	const dt = Math.min(0.05, Math.max(0, (e.timeStamp - lt) / 1000)); lt = e.timeStamp;
	const gx = g.x - a.x, gy = g.y - a.y, gz = g.z - a.z, gn = Math.hypot(gx, gy, gz); // gravity reaction: points up
	if (gn < 5) return;
	let up = (a.x * gx + a.y * gy + a.z * gz) / gn; // vertical acceleration, m/s/s
	if (Math.abs(up) < 0.12) { up = 0; lquiet += dt; } else lquiet = 0;
	if (lquiet > 0.15) lv = 0; // the phone is still: cancel the velocity drift
	lv += up * dt; lz += lv * dt;
	if (L.held) { lbase = (st.y1 + 1) / 2; lz = 0; lv = 0; return; }
	const k = lgain / 100 / LIFT_FULL;
	const th = Math.max(0, Math.min(1, lbase + lz * k));
	lz = (th - lbase) / k; // no windup beyond the limits
	L.place(st.x1, th * 2 - 1);
}
function setLift(on) {
	lift = on; lv = 0; lz = 0; lquiet = 0; lbase = (st.y1 + 1) / 2;
	$('bLift').classList.toggle('on', on); $('lgain').disabled = !on;
}
$('bLift').onclick = async () => {
	if (lift) return setLift(false);
	try {
		if (window.DeviceMotionEvent && DeviceMotionEvent.requestPermission && await DeviceMotionEvent.requestPermission() !== 'granted') return toast('Motion sensors: permission denied');
	} catch (e) {}
	addEventListener('devicemotion', onMotion);
	liftSeen = 0; setLift(true); toast('Lift control: raise or lower the phone to change the throttle', 3000);
	setTimeout(() => { if (lift && !liftSeen) { setLift(false); toast('No accelerometer data on this device', 4000); } }, 1500);
};

// Console
let since = 0, ctimer = null, busy = false;
const hist = []; let hi = -1;
async function poll(cmd) {
	if (busy && !cmd) return; busy = true;
	try {
		const txt = await (await fetch(`/console?since=${since}` + (cmd ? '&cmd=' + encodeURIComponent(cmd) : ''), {cache: 'no-store'})).text();
		const nl = txt.indexOf('\n'); since = +txt.slice(0, nl);
		const add = txt.slice(nl + 1);
		if (add) { const l = $('log'); l.textContent = (l.textContent + add).slice(-30000); l.scrollTop = l.scrollHeight; }
	} catch (e) {}
	busy = false;
	if (cmd) setTimeout(poll, 150);
}
for (const c of ['help', 'st', 'imu', 'rc', 'mot', 'pw', 'p', 'sys', 'wifi']) {
	const b = document.createElement('button'); b.type = 'button'; b.textContent = c === 'p' ? 'params' : c; b.onclick = () => poll(c); $('quick').appendChild(b);
}
{ const b = document.createElement('button'); b.type = 'button'; b.textContent = 'clear'; b.onclick = () => $('log').textContent = ''; $('quick').appendChild(b); }

// Flight recorder: keeps the telemetry the page receives (20 Hz, up to ~1 hour) for "save log"
const REC_HEAD = 'time,drone_t,armed,mode,voltage,roll,pitch,yaw,stick_thr,stick_yaw,stick_roll,stick_pitch,mot_rl,mot_rr,mot_fr,mot_fl,loop_hz,ping_ms';
const rec = [];
function record(s, v, ping) {
	rec.push([(performance.now() / 1000).toFixed(3), s.t.toFixed(3), s.armed, s.mode, s.v.toFixed(2), s.roll.toFixed(1), s.pitch.toFixed(1), s.yaw.toFixed(1),
		v.th.toFixed(3), v.y.toFixed(3), v.r.toFixed(3), v.p.toFixed(3), ...s.mot.map(m => m.toFixed(3)), Math.round(s.hz), Math.round(ping)].join(','));
	if (rec.length > 72000) rec.splice(0, 1000);
}
{
	const b = document.createElement('button'); b.type = 'button'; b.textContent = 'save log';
	b.onclick = () => {
		if (!rec.length) return toast('Nothing recorded yet');
		const d = new Date(), p = n => String(n).padStart(2, '0');
		const a = document.createElement('a');
		a.href = URL.createObjectURL(new Blob([REC_HEAD + '\n' + rec.join('\n') + '\n'], {type: 'text/csv'}));
		a.download = `flix-${d.getFullYear()}${p(d.getMonth() + 1)}${p(d.getDate())}-${p(d.getHours())}${p(d.getMinutes())}${p(d.getSeconds())}.csv`;
		document.body.appendChild(a); a.click(); a.remove();
		toast(`Saved ${Math.round(rec.length / 20 / 6) / 10} min of telemetry`);
	};
	$('quick').appendChild(b);
}
$('bCon').onclick = () => {
	const open = $('con').classList.toggle('open'); $('bCon').classList.toggle('on', open);
	clearInterval(ctimer); if (open) { poll(); ctimer = setInterval(poll, 500); }
	dispatchEvent(new Event('resize'));
};
$('cf').onsubmit = e => {
	e.preventDefault(); const v = $('cin').value.trim(); if (!v) return;
	hist.unshift(v); if (hist.length > 30) hist.pop(); hi = -1; $('cin').value = ''; poll(v);
};
$('cin').addEventListener('keydown', e => {
	if (e.key === 'ArrowUp' && hi < hist.length - 1) { e.preventDefault(); $('cin').value = hist[++hi]; }
	if (e.key === 'ArrowDown') { e.preventDefault(); hi = Math.max(-1, hi - 1); $('cin').value = hi < 0 ? '' : hist[hi]; }
});

if (location.hash === '#console') $('bCon').click(); // handy for tests
loop();
</script>
</body>
</html>
)rawliteral";
