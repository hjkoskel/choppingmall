#!/usr/bin/env python3
"""compare_audio.py ref.wav c.wav: peak/RMS, waveform correlation at best lag (+-30 ms), RMS-envelope and
spectral differences. Waveform match is meaningful only for noise-free scenarios; envelope/spectrum always."""
import sys, wave, numpy as np
def load(p):
    w = wave.open(p); n = w.getnframes(); x = np.frombuffer(w.readframes(n), dtype=np.int16).astype(np.float64) / 32768
    return x
a, b = load(sys.argv[1]), load(sys.argv[2]); n = min(len(a), len(b)); a, b = a[:n], b[:n]
rms = lambda x: float(np.sqrt(np.mean(x ** 2)) + 1e-12)
print(f"peak ref={np.abs(a).max():.4f} c={np.abs(b).max():.4f}   rms ref={rms(a):.5f} c={rms(b):.5f}  (c/ref = {rms(b)/rms(a):.3f})")
# best lag correlation
best = (-2, 0)
for lag in range(-1440, 1441, 4):
    x, y = (a[lag:], b[:n - lag]) if lag >= 0 else (a[:n + lag], b[-lag:])
    if len(x) < 100: continue
    c = np.dot(x, y) / (np.linalg.norm(x) * np.linalg.norm(y) + 1e-12)
    if c > best[0]: best = (c, lag)
print(f"waveform correlation {best[0]:.4f} at lag {best[1]} samples ({best[1]/48:.2f} ms)")
# 50 ms RMS envelope
win = 2400; m = n // win
ea = np.array([rms(a[i*win:(i+1)*win]) for i in range(m)]); eb = np.array([rms(b[i*win:(i+1)*win]) for i in range(m)])
sig = ea > 1e-3
d = 20 * np.log10(eb[sig] / ea[sig]) if sig.any() else np.array([0.0])
print(f"envelope (50 ms windows, {sig.sum()} active): mean diff {d.mean():+.2f} dB, max |diff| {np.abs(d).max():.2f} dB, p90 |diff| {np.percentile(np.abs(d),90):.2f} dB")
# spectrum over whole signal (Welch-like): compare average log spectrum in 1/3-octave bands
F = np.fft.rfftfreq(4096, 1/48000)
def spec(x):
    ps = np.zeros(len(F))
    for i in range(0, len(x) - 4096, 2048): ps += np.abs(np.fft.rfft(x[i:i+4096] * np.hanning(4096))) ** 2
    return ps
sa, sb = spec(a), spec(b)
centers = 100 * 2 ** (np.arange(0, 23) / 3)
diffs = []
for c in centers:
    lo, hi = c / 2 ** (1/6), c * 2 ** (1/6)
    m_ = (F >= lo) & (F < hi)
    if m_.sum() == 0: continue
    ea_, eb_ = sa[m_].sum(), sb[m_].sum()
    if ea_ > 1e-6 * sa.sum(): diffs.append((c, 10 * np.log10((eb_ + 1e-18) / ea_)))
if diffs:
    dd = np.array([x[1] for x in diffs])
    print(f"spectrum ({len(dd)} bands with energy): mean {dd.mean():+.2f} dB, max |diff| {np.abs(dd).max():.2f} dB at {diffs[int(np.argmax(np.abs(dd)))][0]:.0f} Hz")
