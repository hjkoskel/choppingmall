#!/usr/bin/env node
/* Renders a sound scenario with the REAL audio code of index.html on an OfflineAudioContext
 * (node-web-audio-api, an independent Web Audio implementation).
 * usage: NODE_PATH=/home/claude/ref/node_modules node ref_audio.js index.html scenario seconds out.wav */
const fs = require('fs');
const { OfflineAudioContext } = require('node-web-audio-api');
const [,, htmlPath, scenario, secsArg, outPath] = process.argv;
const SR = 48000, secs = Number(secsArg);
const script = fs.readFileSync(htmlPath, 'utf8').match(/<script>([\s\S]*)<\/script>/)[1];
const real = new OfflineAudioContext(2, Math.ceil(secs * SR), SR);
let fake = 0;
const AC = new Proxy(real, {
  get(t, p) {
    if (p === 'currentTime') return fake;
    if (p === 'state') return 'running';
    if (p === 'resume') return () => {};
    const v = t[p];
    return typeof v === 'function' ? v.bind(t) : v;
  },
});
const grad = { addColorStop() {} };
const ctx2d = new Proxy({}, { get: (t, p) => (p in t ? t[p] : String(p).includes('Gradient') ? () => grad : () => {}), set: (t, p, v) => ((t[p] = v), true) });
const el = () => ({ getContext: () => ctx2d, style: {}, dataset: {}, classList: { add() {}, remove() {}, toggle() {} }, addEventListener() {}, getBoundingClientRect: () => ({}), querySelector: () => el() });
const handlers = {};
const api = new Function('document', 'window', 'localStorage', 'matchMedia', 'performance', 'requestAnimationFrame', 'addEventListener', 'setInterval',
  script + ';return { sfx, clank, playJingle, startMusic, mlPump, engineApply, setEng(v, f) { engV = v; engF = f; }, setAC(a) { AC = a; } };')(
  { getElementById: () => el(), createElement: () => el(), querySelectorAll: () => [], body: { classList: { add() {} } } },
  { AudioContext: function () { return AC; } }, { getItem: () => null }, () => ({ matches: false }), { now: () => 0 }, () => {},
  (n, f) => { handlers[n] = f; }, () => {});
api.setAC(AC);
const num = (s) => s.split(',').map(Number);
const pump = (until) => { for (let t = 0; t <= until; t += 0.05) { fake = t; api.mlPump(); } };
if (scenario.startsWith('sfx:')) { const [f, d, ty, v, f2] = scenario.slice(4).split(','); api.sfx(+f, +d, ty, +v, +f2); }
else if (scenario.startsWith('clank:')) api.clank(+scenario.slice(6));
else if (scenario.startsWith('engine:')) { const [v, f] = num(scenario.slice(7)); api.setEng(v, f); for (let k = 0; k * (1 / 60) < secs; k++) { fake = k / 60; api.engineApply(); } }
else if (scenario === 'music') { fake = 0; api.startMusic(); pump(secs); }
else if (scenario === 'win') api.playJingle('win');
else if (scenario === 'lose') api.playJingle('lose');
else if (scenario === 'mix') { fake = 0; api.startMusic(); pump(3); fake = 3; api.playJingle('win'); pump(secs); }
real.startRendering().then((buf) => {
  const d = buf.getChannelData(0), n = d.length, out = Buffer.alloc(44 + n * 2);
  out.write('RIFF', 0); out.writeUInt32LE(36 + n * 2, 4); out.write('WAVEfmt ', 8); out.writeUInt32LE(16, 16);
  out.writeUInt16LE(1, 20); out.writeUInt16LE(1, 22); out.writeUInt32LE(SR, 24); out.writeUInt32LE(SR * 2, 28);
  out.writeUInt16LE(2, 32); out.writeUInt16LE(16, 34); out.write('data', 36); out.writeUInt32LE(n * 2, 40);
  for (let i = 0; i < n; i++) out.writeInt16LE(Math.round(Math.max(-1, Math.min(1, d[i])) * 32767), 44 + i * 2);
  fs.writeFileSync(outPath, out);
  console.log(scenario, 'ref rendered', n, 'samples');
});
