#!/usr/bin/env python3
"""diff_png.py a.png b.png [out_diff.png]: mean/p99/max abs diff and fraction of pixels differing."""
import sys, numpy as np
from PIL import Image
a = np.asarray(Image.open(sys.argv[1]).convert('RGB')).astype(int); b = np.asarray(Image.open(sys.argv[2]).convert('RGB')).astype(int)
if a.shape != b.shape: print('shape mismatch', a.shape, b.shape); sys.exit(1)
d = np.abs(a - b).max(axis=2)
print(f"mean={d.mean():.3f} p99={np.percentile(d,99):.0f} max={d.max()} frac>24={(d>24).mean()*100:.3f}% frac>64={(d>64).mean()*100:.3f}%")
if len(sys.argv) > 3:
    Image.fromarray(np.clip(d * 4, 0, 255).astype(np.uint8)).save(sys.argv[3])
