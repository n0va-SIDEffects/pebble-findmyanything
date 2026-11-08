"""Checks the beeps rendered by tools/beeper_test.c (WAV path as argument)."""
import sys
import wave

import numpy as np

w = wave.open(sys.argv[1])
x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(float)
print("peak %.1f %% of full scale" % (abs(x).max() / 327.67))

nz = np.flatnonzero(x)
click_at = nz[-1] - 200          # the click is the last sound
beeps = x[:click_at]
d = np.abs(np.diff(beeps))
limit = 2 * np.pi * 1800 / 16000 * 0.40 * 32767
i = int(d.argmax())
print("beeps: max step %.0f at %.1f ms (a 1800 Hz sine at 40 %% may step %.0f)" % (d.max(), i / 16, limit))

# Every beep must start and end at zero
segs, start = [], None
for k in range(len(beeps)):
    if beeps[k] != 0 and start is None:
        start = k
    if start is not None and k - start >= 960:
        segs.append(start)
        start = None if beeps[k] == 0 else k
print("beep starts (ms):", [round(s / 16) for s in segs])

# Harmonics of the 1350 Hz beep relative to the tone
s = segs[2]
seg = beeps[s:s + 960]
f = np.fft.rfft(seg * np.hanning(960), 32768)
p = np.abs(f) ** 2
fr = np.fft.rfftfreq(32768, 1 / 16000)


def band(c, bw=60):
    return p[(fr > c - bw) * (fr < c + bw)].sum()


F = band(1350, 200)
for h in (2, 3, 4, 5):
    c = 1350 * h
    if c > 8000:
        c = 16000 - c
    print("  harmonic %d (%d Hz): %.1f dB" % (h, c, 10 * np.log10(band(c) / F + 1e-30)))

# Oscillator alone (integer replica of beeper.c), constant amplitude
sine = np.round(32767 * np.sin(2 * np.pi * np.arange(257) / 256)).astype(np.int64)
inc = (1350 << 32) // 16000
ph = np.array([(n * inc) % (1 << 32) for n in range(16000)], dtype=np.int64)
idx = ph >> 24
frac = (ph >> 8) % 65536
y = sine[idx] + (((sine[idx + 1] - sine[idx]) * frac) >> 16)
Y = np.abs(np.fft.rfft(y * np.blackman(len(y)))) ** 2
fq = np.fft.rfftfreq(len(y), 1 / 16000)
fund = Y[(fq > 1335) * (fq < 1365)].sum()
print("oscillator alone: everything else %.1f dB below the tone" % (10 * np.log10(fund / (Y.sum() - fund))))
