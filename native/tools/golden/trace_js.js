#!/usr/bin/env node
/* Runs the REAL game code from index.html under node with a stubbed DOM, a seeded
 * RNG and a fixed dt, driven by a scripted bot. Writes:
 *   <out>.inputs  one integer bitmask per frame
 *   <out>.trace   state snapshots (key=value lines)
 * usage: node trace_js.js index.html out_prefix scenario seed frames
 * scenarios: fighter | random | afk | molotov | fighter2 */
const fs = require('fs');
const [,, htmlPath, outPrefix, scenario = 'fighter', seedArg = '1', framesArg = '6000'] = process.argv;
const seed = Number(seedArg) >>> 0, FRAMES = Number(framesArg), DT = 1 / 60, EVERY = 10;
const html = fs.readFileSync(htmlPath, 'utf8');
const script = html.match(/<script>([\s\S]*)<\/script>/)[1];

function mulberry32(a) {
  return function () {
    a |= 0; a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
const grad = { addColorStop() {} };
const ctxStub = new Proxy({}, {
  get(t, p) {
    if (p in t) return t[p];
    if (p === 'createRadialGradient' || p === 'createLinearGradient') return () => grad;
    if (p === 'measureText') return () => ({ width: 0 });
    return () => {};
  },
  set(t, p, v) { t[p] = v; return true; },
});
const el = () => ({
  getContext: () => ctxStub, width: 0, height: 0, style: {}, dataset: {},
  classList: { add() {}, remove() {}, toggle() {} }, addEventListener() {}, setPointerCapture() {},
  getBoundingClientRect: () => ({ left: 0, top: 0, width: 150, height: 150 }),
  querySelector: () => el(), textContent: '',
});
const document = { getElementById: () => el(), createElement: () => el(), querySelectorAll: () => [], body: { classList: { add() {} } } };
const handlers = {};
const addEventListener = (n, f) => { handlers[n] = f; };
Math.random = mulberry32(seed);

const harness = `
;return {
  S, P, R, K, N, OB, AI, AO, CX, CY, TAU,
  update, draw, polar, valid, clearShot, maxDot, WN, MAXA, SH,
  get bul() { return bul; }, get rb() { return rb; }, get fires() { return fires; },
  get mols() { return mols; }, get parts() { return parts; }, get T() { return T; },
  get engV() { return engV; }, get engF() { return engF; },
};`;
const api = new Function('document', 'window', 'localStorage', 'matchMedia', 'performance',
  'requestAnimationFrame', 'addEventListener', script + harness)(
  document, {}, { getItem: () => null, setItem() {} }, () => ({ matches: false }),
  { now: () => 0 }, () => {}, addEventListener);
const { S, P, R, K } = api;

// ---- input encoding: bit0 up, 1 down, 2 left, 3 right, 4 space (held);
//      bit5/6/7 digit 1/2/3, bit8 R, bit9 Enter (edge events this frame),
//      bit10 = TEST HOOK: refill all ammo, bit11 = TEST HOOK: robot hp = min(hp, 0.5)
//      (both applied identically by the C replayer)
const B = { up: 1, down: 2, left: 4, right: 8, space: 16, d1: 32, d2: 64, d3: 128, r: 256, enter: 512, cheat: 1024, cheat2: 2048 };
const f = (v) => (Number.isFinite(v) ? Number(v.toPrecision(12)) : v);
function snap(frame) {
  const a = [
    ['f', frame], ['mode', S.mode], ['mt', f(S.mt)], ['score', f(S.score)], ['lives', S.lives], ['level', S.level],
    ['time', f(S.time)], ['lt', f(S.lt)], ['kills', S.kills], ['w', S.w],
    ['a0', S.ammo[0]], ['a1', S.ammo[1]], ['a2', S.ammo[2]],
    ['c0', f(S.cool[0])], ['c3', f(S.cool[3])], ['c5', f(S.cool[5])], ['tt', f(S.tt)], ['shake', f(S.shake)],
    ['px', f(P.x)], ['py', f(P.y)], ['php', f(P.hp)], ['pface', f(P.face)], ['paim', f(P.aim)], ['pcd', f(P.cd)],
    ['pinv', f(P.inv)], ['pflash', f(P.flash)],
    ['rx', f(R.x)], ['ry', f(R.y)], ['rhp', f(R.hp)], ['rface', f(R.face)], ['rslow', f(R.slow)], ['rcd', f(R.cd)],
    ['rpause', f(R.pause)], ['rdir', R.dir], ['rdead', R.dead ? 1 : 0], ['rsh', f(R.sh)], ['rwind', f(R.wind)],
    ['rck', f(R.ck)], ['rrev', f(R.rev)], ['rside', R.side], ['rstk', f(R.stk)],
    ['nbul', api.bul.length], ['nrb', api.rb.length], ['nmol', api.mols.length], ['nfire', api.fires.length],
    ['nparts', api.parts.length], ['T', f(api.T)],
  ];
  return a.map(([k, v]) => k + '=' + v).join(' ');
}

// ---- bots (they read the live game state; the recorded inputs are what C replays)
const rnd2 = mulberry32(seed ^ 0x9e3779b9); // bot's own randomness (separate from the game's)
let botTarget = null, botSide = 1, botDwell = 0, lastBotChoice = 0;
function dirBits(dx, dy) {
  let m = 0;
  const l = Math.hypot(dx, dy) || 1; dx /= l; dy /= l;
  if (dx > 0.38) m |= B.right; else if (dx < -0.38) m |= B.left;
  if (dy > 0.38) m |= B.down; else if (dy < -0.38) m |= B.up;
  return m;
}
function ringWaypoint(x, y, sgn) { // a point further along the hallway centre line
  const a = Math.atan2(y - api.CY, x - api.CX) + sgn * 0.45, ux = Math.cos(a), uy = Math.sin(a);
  let m = -9; for (const n of api.N) m = Math.max(m, ux * n.x + uy * n.y);
  return [api.CX + (ux * 225) / m, api.CY + (uy * 225) / m];
}
function angDiff(a, b) { let d = b - a; while (d > Math.PI) d -= api.TAU; while (d < -Math.PI) d += api.TAU; return d; }
let stuckN = 0, sideN = 0, lastPx = 0, lastPy = 0, sideSgn = 1;
function goTo(tx, ty) { // with stuck detection: sidestep for a while when wedged on an obstacle
  if (Math.hypot(P.x - lastPx, P.y - lastPy) < 0.5) stuckN++; else stuckN = 0;
  lastPx = P.x; lastPy = P.y;
  if (stuckN > 12 && sideN <= 0) { sideN = 40; sideSgn = -sideSgn; stuckN = 0; }
  const bits = goToRaw(tx, ty);
  if (sideN > 0) {
    sideN--;
    const d = [(bits & B.right ? 1 : 0) - (bits & B.left ? 1 : 0), (bits & B.down ? 1 : 0) - (bits & B.up ? 1 : 0)];
    return dirBits(-d[1] * sideSgn + 0.4 * d[0], d[0] * sideSgn + 0.4 * d[1]);
  }
  return bits;
}
function goToRaw(tx, ty) { // follow the ring toward target (x,y), taking the shorter way round
  const ap = Math.atan2(P.y - api.CY, P.x - api.CX), at = Math.atan2(ty - api.CY, tx - api.CX);
  const d = angDiff(ap, at);
  if (Math.abs(d) < 0.3) return dirBits(tx - P.x, ty - P.y);
  const [wx, wy] = ringWaypoint(P.x, P.y, Math.sign(d));
  return dirBits(wx - P.x, wy - P.y);
}
function shopPos(k) { return api.polar(k, api.AI + 26); }
function botFighter(frame, variant) {
  let m = 0;
  if (S.mode === 'title') return B.enter;
  if (S.mode === 'over' || S.mode === 'win') return S.mt <= 0 ? B.r : 0;
  if (S.mode !== 'play') return 0;
  const dist = Math.hypot(R.x - P.x, R.y - P.y), shot = api.clearShot(P.x, P.y, R.x, R.y);
  // ammo choice
  let want = -1;
  if (variant === 2 && S.ammo[2] < 3 && S.cool[1] <= 0) want = 1;
  else if (S.ammo[0] < 18) want = [3, 5][(frame >> 9) & 1];
  else if (variant === 1 && S.ammo[1] < 6 && S.cool[0] <= 0) want = 0;
  else if (P.hp < 60 && S.cool[2] <= 0) want = 2;
  const wantWeapon = variant === 2 && S.ammo[2] > 0 ? 2 : variant === 1 && S.ammo[1] > 0 ? 1 : 0;
  if (S.w !== wantWeapon) m |= [B.d1, B.d2, B.d3][wantWeapon];
  if (variant === 2 && S.ammo[2] === 0) m |= B.cheat; // test hook instead of a gas run
  const fight = shot && dist < (wantWeapon === 2 ? 230 : 460) && S.ammo[wantWeapon] > 0;
  if (fight) { return m | B.space; }
  if (want >= 0 && S.cool[want] <= 0 && !(dist < 150)) {
    const [sx, sy] = shopPos(want);
    return m | goTo(sx, sy);
  }
  if (dist < 170) { // flee away along the ring
    const ap = Math.atan2(P.y - api.CY, P.x - api.CX), ar = Math.atan2(R.y - api.CY, R.x - api.CX);
    const [wx, wy] = ringWaypoint(P.x, P.y, -Math.sign(angDiff(ap, ar)) || 1);
    return m | dirBits(wx - P.x, wy - P.y);
  }
  if (S.ammo[wantWeapon] > 0) return m | goTo(R.x, R.y); // hunt
  return m | goTo(...shopPos(3));
}
function botRandom(frame) {
  if (S.mode === 'title') return B.enter;
  if (S.mode === 'over' || S.mode === 'win') return S.mt <= 0 ? B.r : 0;
  if (frame % 15 === 0) lastBotChoice = Math.floor(rnd2() * 1024) & 31 | (rnd2() < 0.25 ? 0 : 0);
  let m = lastBotChoice & 31;
  const e = rnd2();
  if (e < 0.02) m |= B.d1; else if (e < 0.04) m |= B.d2; else if (e < 0.06) m |= B.d3;
  if (e > 0.995) m |= B.r;
  return m;
}
function botAfk(frame) {
  if (S.mode === 'title') return B.enter;
  if (S.mode === 'over') return S.mt <= 0 && frame > 3000 ? B.r : 0;
  return 0;
}
function botMedic(frame) { // stand still until hurt, then run to the FIRST AID door (shop 2)
  if (S.mode === 'title') return B.enter;
  if (S.mode === 'over' || S.mode === 'win') return S.mt <= 0 ? B.r : 0;
  if (S.mode !== 'play') return 0;
  if (P.hp >= 100 || S.cool[2] > 0) return 0;
  return goTo(...shopPos(2));
}
let chaosDir = 0, chaosFire = 0, chaosW = 0;
function botChaos(frame) { // random movement, weapon swaps and fire holds, regular ammo refills
  if (S.mode === 'title') return B.enter;
  if (S.mode === 'over' || S.mode === 'win') return S.mt <= 0 ? B.r : 0;
  if (frame % 17 === 0) { chaosDir = [0, 1, 2, 4, 8, 5, 9, 6, 10][Math.floor(rnd2() * 9)]; chaosFire = rnd2() < 0.5 ? 16 : 0; if (rnd2() < 0.3) chaosW = Math.floor(rnd2() * 3); }
  let m = (chaosFire ? 16 : chaosDir) | ([B.d1, B.d2, B.d3][chaosW] * ((frame % 17) === 0 ? 1 : 0));
  if (frame % 211 === 0) m |= B.cheat;
  return m;
}
function botFireKill(frame) { // molotov bot that also weakens the robot while any fire burns
  let m = botFighter(frame, 2);
  if (S.mode === 'play' && api.fires.length > 0) m |= B.cheat2;
  return m;
}
function pick(frame) {
  switch (scenario) {
    case 'fighter': return botFighter(frame, 0);
    case 'fighter2': return botFighter(frame, 1);
    case 'molotov': return botFighter(frame, 2);
    case 'random': return botRandom(frame);
    case 'afk': return botAfk(frame);
    case 'medic': return botMedic(frame);
    case 'chaos': return botChaos(frame);
    case 'firekill': return botFireKill(frame);
  }
  throw new Error('unknown scenario ' + scenario);
}

const inputs = [], lines = [];
let maxes = { bul: 0, rb: 0, mols: 0, fires: 0, parts: 0 };
const modes = new Set();
for (let frame = 0; frame < FRAMES; frame++) {
  const m = pick(frame);
  inputs.push(m);
  K.ArrowUp = m & B.up ? 1 : 0; K.ArrowDown = m & B.down ? 1 : 0;
  K.ArrowLeft = m & B.left ? 1 : 0; K.ArrowRight = m & B.right ? 1 : 0; K.Space = m & B.space ? 1 : 0;
  const ev = (code) => handlers.keydown({ code, repeat: false, preventDefault() {} });
  if (m & B.d1) ev('Digit1'); if (m & B.d2) ev('Digit2'); if (m & B.d3) ev('Digit3');
  if (m & B.r) ev('KeyR'); if (m & B.enter) ev('Enter');
  if (m & B.cheat) { S.ammo[0] = api.MAXA[0]; S.ammo[1] = api.MAXA[1]; S.ammo[2] = api.MAXA[2]; }
  if (m & B.cheat2) R.hp = Math.min(R.hp, 0.5);
  K.KeyR = K.Enter = 0; // keydown set these; they are never read
  api.update(DT);
  api.draw();
  modes.add(S.mode);
  maxes.bul = Math.max(maxes.bul, api.bul.length); maxes.rb = Math.max(maxes.rb, api.rb.length);
  maxes.mols = Math.max(maxes.mols, api.mols.length); maxes.fires = Math.max(maxes.fires, api.fires.length);
  maxes.parts = Math.max(maxes.parts, api.parts.length);
  if (frame % EVERY === 0 || frame === FRAMES - 1) lines.push(snap(frame));
}
fs.writeFileSync(outPrefix + '.inputs', inputs.join('\n') + '\n');
fs.writeFileSync(outPrefix + '.trace', `# scenario=${scenario} seed=${seed} frames=${FRAMES} dt=1/60\n` + lines.join('\n') + '\n');
console.log(`${scenario} seed=${seed}: level=${S.level} kills=${S.kills} lives=${S.lives} mode=${S.mode} score=${Math.floor(S.score)}`,
  'modes=' + [...modes].join(','), 'max=' + JSON.stringify(maxes));
