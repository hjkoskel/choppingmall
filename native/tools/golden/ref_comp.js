const { OfflineAudioContext } = require('node-web-audio-api'); const fs = require('fs');
const amp = Number(process.argv[2]), out = process.argv[3], secs = Number(process.argv[4] || 1.5), SR = 48000;
const c = new OfflineAudioContext(1, Math.ceil(secs * SR), SR), o = c.createOscillator(), g = c.createGain(), k = c.createDynamicsCompressor();
o.frequency.value = 440; g.gain.value = amp; o.connect(g); g.connect(k); k.connect(c.destination); o.start(0);
c.startRendering().then((b) => { const d = b.getChannelData(0), n = d.length, buf = Buffer.alloc(44 + n * 2);
  buf.write('RIFF', 0); buf.writeUInt32LE(36 + n * 2, 4); buf.write('WAVEfmt ', 8); buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20); buf.writeUInt16LE(1, 22);
  buf.writeUInt32LE(SR, 24); buf.writeUInt32LE(SR * 2, 28); buf.writeUInt16LE(2, 32); buf.writeUInt16LE(16, 34); buf.write('data', 36); buf.writeUInt32LE(n * 2, 40);
  for (let i = 0; i < n; i++) buf.writeInt16LE(Math.round(Math.max(-1, Math.min(1, d[i])) * 32767), 44 + i * 2); fs.writeFileSync(out, buf); });
