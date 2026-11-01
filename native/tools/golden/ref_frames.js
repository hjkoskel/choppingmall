#!/usr/bin/env node
/* Renders reference frames with the REAL index.html drawing code on a Skia canvas
 * (@napi-rs/canvas, the same engine family as Chrome's canvas).
 * usage: NODE_PATH=/home/claude/ref/node_modules node ref_frames.js index.html inputs seed outdir frame,frame,...
 * Writes outdir/bg.png and outdir/f<frame>.png. inputs may be '-' for none. */
const fs = require('fs');
const { createCanvas } = require('@napi-rs/canvas');
const [,, htmlPath, inputsPath, seedArg, outDir, framesArg] = process.argv;
const seed = Number(seedArg) >>> 0, DT = 1 / 60;
fs.mkdirSync(outDir, { recursive: true });
const script = fs.readFileSync(htmlPath, 'utf8').match(/<script>([\s\S]*)<\/script>/)[1];
function mulberry32(a) { return function () { a |= 0; a = (a + 0x6d2b79f5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
const main = createCanvas(960, 760);
main.addEventListener = () => {}; main.setPointerCapture = () => {}; main.style = {};
main.getBoundingClientRect = () => ({ left: 0, top: 0, width: 960, height: 760 });
const el = () => ({ getContext: () => ({}), style: {}, dataset: {}, classList: { add() {}, remove() {}, toggle() {} }, addEventListener() {}, setPointerCapture() {}, getBoundingClientRect: () => ({ left: 0, top: 0, width: 150, height: 150 }), querySelector: () => el(), textContent: '' });
const document = {
  getElementById: (id) => (id === 'c' ? main : el()),
  createElement: (t) => (t === 'canvas' ? createCanvas(1, 1) : el()),
  querySelectorAll: () => [], body: { classList: { add() {} } },
};
const handlers = {};
Math.random = mulberry32(seed);
const api = new Function('document', 'window', 'localStorage', 'matchMedia', 'performance', 'requestAnimationFrame', 'addEventListener',
  script + ';return { bg, update, draw, K, S };')(
  document, {}, { getItem: () => null, setItem() {} }, () => ({ matches: false }), { now: () => 0 }, () => {}, (n, f) => { handlers[n] = f; });
fs.writeFileSync(outDir + '/bg.png', api.bg.toBuffer('image/png'));
if (inputsPath !== '-' && framesArg) {
  const inputs = fs.readFileSync(inputsPath, 'utf8').trim().split('\n').map(Number);
  const want = new Set(framesArg.split(',').map(Number));
  const { K, S } = api;
  for (let f = 0; f < inputs.length && f <= Math.max(...want); f++) {
    const m = inputs[f];
    K.ArrowUp = m & 1 ? 1 : 0; K.ArrowDown = m & 2 ? 1 : 0; K.ArrowLeft = m & 4 ? 1 : 0; K.ArrowRight = m & 8 ? 1 : 0; K.Space = m & 16 ? 1 : 0;
    const ev = (code) => handlers.keydown({ code, repeat: false, preventDefault() {} });
    if (m & 32) ev('Digit1'); if (m & 64) ev('Digit2'); if (m & 128) ev('Digit3'); if (m & 256) ev('KeyR'); if (m & 512) ev('Enter');
    if (m & 1024) { S.ammo[0] = 60; S.ammo[1] = 24; S.ammo[2] = 6; }
    api.update(DT); api.draw();
    if (want.has(f)) fs.writeFileSync(`${outDir}/f${f}.png`, main.toBuffer('image/png'));
  }
}
