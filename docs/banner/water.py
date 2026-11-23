# Drum hits as ripples on dark water, a sonar over it; one ink colour stippled on kraft paper.
import math, numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
rng = np.random.default_rng(11)
K = 2; W, H = 1600 * K, 520 * K
KRAFT = np.array([196, 156, 110], np.float32); INK = np.array([30, 24, 20], np.float32)
Y, X = np.mgrid[0:H, 0:W].astype(np.float32) / K           # css px coordinates

def fbm(scale, octaves=5, seed=0):
    r = np.random.default_rng(seed); out = np.zeros((H, W), np.float32); amp = 1.0; tot = 0
    for o in range(octaves):
        n = r.random((int(H / scale) + 2, int(W / scale) + 2)).astype(np.float32)
        img = Image.fromarray((n * 255).astype(np.uint8)).resize((W, H), Image.BICUBIC)
        out += amp * np.asarray(img, np.float32) / 255; tot += amp; amp *= 0.5; scale /= 2
    return out / tot
def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)

# ---- dark water: a cloudy mass on the right, fading out under the title ----
n = fbm(260 * K, 5, 3)
lean = smooth(560, 980, X) * (0.75 + 0.25 * smooth(0, 520, Y))
water = smooth(0.42, 0.62, n * 0.55 + lean * 0.62)

# ---- ripples: one source per drum hit (the beat of the grid), each voice its own rings ----
voice = {'BD': (34, 150, 1.0), 'SD': (24, 110, 0.8), 'CH': (13, 45, 0.45), 'OH': (16, 80, 0.55),
         'CP': (19, 90, 0.6), 'LT': (28, 110, 0.7), 'HT': (25, 100, 0.65), 'CY': (15, 130, 0.5)}
hits = [('BD', 1110, 300), ('BD', 1420, 170), ('SD', 1260, 420), ('CH', 980, 150), ('CH', 1190, 110),
        ('CH', 1340, 300), ('CH', 1530, 410), ('OH', 1500, 250), ('CP', 900, 380), ('LT', 1040, 470),
        ('HT', 1560, 80), ('CY', 820, 230)]
R = np.zeros((H, W), np.float32)
for name, sx, sy in hits:
    lam, tau, w = voice[name]
    d = np.hypot(X - sx, Y - sy)
    R += w * np.cos(2 * np.pi * d / lam) * np.exp(-d / tau)
crest = smooth(0.30, 0.62, R) * 0.75 + smooth(0.62, 0.9, R) * 0.25

ink = water * 0.88 * (1 - 0.92 * crest)                    # light rings on the dark water
ink += (1 - water) * 0.55 * crest * smooth(640, 900, X)    # faint rings where the water thins out
ink += (1 - water) * 0.06 * fbm(40 * K, 3, 5)              # paper speckle

# ---- the digital layer: a sonar (dashed rings, sweep, ticks), drawn as an inversion ----
son = Image.new('L', (W, H), 0); sd = ImageDraw.Draw(son)
cx, cy = 1180 * K, 280 * K
for i, r in enumerate(range(70, 560, 70)):
    rr = r * K; segs = 90 + i * 30
    for k in range(segs):
        if k % 3 == 2: continue
        a0, a1 = 360 * k / segs, 360 * (k + 0.55) / segs
        sd.arc((cx - rr, cy - rr, cx + rr, cy + rr), a0, a1, fill=255, width=max(1, int(1.7 * K)))
for a in range(0, 360, 10):                                 # bearing ticks on one ring
    ra = math.radians(a); r0, r1 = 420 * K, (434 if a % 30 else 448) * K
    sd.line((cx + r0 * math.cos(ra), cy + r0 * math.sin(ra), cx + r1 * math.cos(ra), cy + r1 * math.sin(ra)), fill=255, width=int(1.2 * K))
sd.line((cx - 600 * K, cy, cx + 600 * K, cy), fill=150, width=1); sd.line((cx, cy - 600 * K, cx, cy + 600 * K), fill=150, width=1)
son = np.asarray(son, np.float32) / 255
ang = (np.degrees(np.arctan2(Y - cy / K, X - cx / K)) + 360) % 360
sweep = np.clip(1 - ((-35 - ang) % 360) / 40, 0, 1) * smooth(560, 0, np.hypot(X - cx / K, Y - cy / K))   # fading wedge
ink = np.abs(ink - son)
ink = ink * (1 - 0.55 * sweep * water) + 0.35 * sweep * (1 - water)
# sparkle on the water
st = Image.new('L', (W, H), 0); s2 = ImageDraw.Draw(st)
for _ in range(26):
    x, y = rng.uniform(700, 1590) * K, rng.uniform(10, 510) * K; s = rng.uniform(2, 6) * K
    s2.line((x - s, y, x + s, y), fill=255, width=int(1.1 * K)); s2.line((x, y - s, x, y + s), fill=255, width=int(1.1 * K))
st = np.asarray(st, np.float32) / 255 * water
ink = np.clip(ink - st, 0, 1)

# ---- type, in the same stippled ink ----
F = '/usr/share/fonts/opentype/inter/'
tm = Image.new('L', (W, H), 0); td = ImageDraw.Draw(tm)
def tracked(x, y, t, f, track, fill=255):
    for c in t:
        td.text((x, y), c, font=f, fill=fill); x += td.textlength(c, font=f) + track * K
    return x
ft = ImageFont.truetype(F + 'Inter-ExtraBold.otf', 132 * K)
tracked(76 * K, 108 * K, 'Drum', ft, -4.5); tracked(76 * K, 228 * K, 'Mode', ft, -4.5)
fl = ImageFont.truetype(F + 'Inter-SemiBold.otf', 14 * K)
tracked(80 * K, 84 * K, 'DIGITONE MK1 · OS 1.44 · ELEKLOADER MOD', fl, 2.8)
fs = ImageFont.truetype(F + 'Inter-SemiBold.otf', 15 * K)
x = 80 * K
for p in ('8 VOICES PER MIDI TRACK', '/', 'FULLY INTEGRATED'):
    x = tracked(x, 404 * K, p, fs, 2.4, 255 if p != '/' else 120) + 26 * K
# a small row of 16 step marks, the digital touch under the title
for s in range(16):
    x0 = 80 * K + s * 15 * K + (s // 4) * 8 * K
    on = s in (0, 4, 8, 10, 12)
    td.rectangle((x0, 446 * K, x0 + 9 * K, 455 * K), fill=255 if on else 0, outline=255, width=int(1 * K))
tm = np.asarray(tm, np.float32) / 255
ink = np.maximum(ink, tm * 0.96)

# ---- stipple: ink lands as grains, denser where the ink is darker ----
grain = rng.random((H, W)).astype(np.float32)
coarse = np.asarray(Image.fromarray((rng.random((H // 2, W // 2)) * 255).astype(np.uint8)).resize((W, H), Image.NEAREST), np.float32) / 255
g = grain * 0.6 + coarse * 0.4
dots = (g < np.power(ink, 1.15)).astype(np.float32)
dots = np.asarray(Image.fromarray((dots * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.45)), np.float32) / 255
paper = KRAFT * (0.94 + 0.08 * fbm(120 * K, 4, 9))[..., None] + (rng.random((H, W, 1)) * 10 - 5)
img = paper * (1 - dots[..., None]) + INK * dots[..., None]
Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).save('drum-mode-banner.png', optimize=True)
print('ok')
