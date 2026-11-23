# Manual banner: Drum Mode as an FM algorithm (8 voice operators into one MIDI track). Run from this folder; writes drum-mode-manual.png.
# Drum Mode as an FM algorithm: 8 voice "operators" (each with its own envelope) routed into one carrier, the MIDI
# track; behind them a field of harmonic bars. One ink colour stippled on kraft, like the water banner.
import math, numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
rng = np.random.default_rng(5)
K = 2; W, H = 1600 * K, 520 * K
KRAFT = np.array([196, 156, 110], np.float32); INK = np.array([30, 24, 20], np.float32)
F = '/usr/share/fonts/opentype/inter/'
def font(w, px): return ImageFont.truetype(F + 'Inter-%s.otf' % w, int(px * K))
Y, X = np.mgrid[0:H, 0:W].astype(np.float32) / K
def fbm(scale, octaves=4, seed=0):
    r = np.random.default_rng(seed); out = np.zeros((H, W), np.float32); amp = 1.0; tot = 0
    for o in range(octaves):
        n = r.random((int(H / scale) + 2, int(W / scale) + 2)).astype(np.float32)
        out += amp * np.asarray(Image.fromarray((n * 255).astype(np.uint8)).resize((W, H), Image.BICUBIC), np.float32) / 255
        tot += amp; amp *= 0.5; scale /= 2
    return out / tot
def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)

ink = np.zeros((H, W), np.float32)

# ---- background: harmonic bars (the sound of the 8 voices together), soft stipple ----
bars = Image.new('L', (W, H), 0); bd = ImageDraw.Draw(bars)
x0, x1, base = 600, 1590, 470
n = 78
for i in range(n):
    t = i / (n - 1)
    h = (300 * math.exp(-t * 3.2) + 80 * math.exp(-((t - 0.55) / 0.08) ** 2) + 55 * math.exp(-((t - 0.82) / 0.05) ** 2)) \
        * (0.55 + 0.45 * rng.random())
    x = x0 + t * (x1 - x0)
    bd.rectangle((x * K, (base - h) * K, (x + 7) * K, base * K), fill=255)
bars = np.asarray(bars, np.float32) / 255
fade = smooth(560, 760, X) * smooth(0, 120, Y)
ink += bars * 0.30 * fade * (0.75 + 0.5 * fbm(60 * K, 3, 2))
ink += 0.10 * smooth(0.45, 0.8, fbm(200 * K, 4, 7)) * fade      # a faint cloud of grain


# ripples spreading from the output, faint, like the water banner
dist = np.hypot(X - 1586, Y - 260)
rings = 0.5 + 0.5 * np.cos(dist / 9.0 * 2 * math.pi / 2.2)
ink += 0.22 * smooth(0.82, 0.98, rings) * smooth(20, 60, dist) * (1 - smooth(120, 330, dist)) * smooth(1380, 1500, X)
# fine dot grid behind the diagram (technical paper)
grid = ((np.mod(X - 650, 20) < 1.3) & (np.mod(Y - 8, 20) < 1.3)).astype(np.float32)
ink += 0.35 * grid * smooth(600, 680, X) * (1 - smooth(1560, 1600, X))

# ---- the algorithm diagram ----
dg = Image.new('L', (W, H), 0); d = ImageDraw.Draw(dg)
lw = lambda px: max(1, int(px * K))
voices = [  # name, attack, decay, end level (0..1), dotted output
    ('BD', 0.02, 0.85, 0.00, False), ('SD', 0.02, 0.45, 0.10, False), ('CH', 0.01, 0.12, 0.00, True),
    ('OH', 0.01, 0.55, 0.20, True), ('CP', 0.05, 0.35, 0.00, False), ('LT', 0.02, 0.60, 0.05, False),
    ('HT', 0.02, 0.50, 0.05, False), ('CY', 0.03, 0.95, 0.35, True)]
bw, bh = 92, 70
cols = [650 + c * 122 for c in range(4)]
rows = [88, 362]
bus_y = 260
car = (1360, 205, 1480, 315)                              # the carrier: the MIDI track
f_lab, f_small = font('Bold', 13), font('SemiBold', 11)
def dashed(p, q, on=7, off=5, w=1.6):
    (xa, ya), (xb, yb) = p, q; L = math.hypot(xb - xa, yb - ya); k = 0.0
    while k < L:
        e = min(L, k + on)
        d.line(((xa + (xb - xa) * k / L) * K, (ya + (yb - ya) * k / L) * K,
                (xa + (xb - xa) * e / L) * K, (ya + (yb - ya) * e / L) * K), fill=255, width=lw(w))
        k += on + off
for i, (name, a, dc, end, dotted) in enumerate(voices):
    r, c = divmod(i, 4)
    bx, by = cols[c], rows[r]
    d.rectangle((bx * K, by * K, (bx + bw) * K, (by + bh) * K), outline=255, width=lw(2))
    d.text(((bx + 7) * K, (by + 6) * K), name, font=f_lab, fill=255)
    # the envelope: attack, decay, end level, like the manual's operator envelopes
    ex0, ex1, ey0, ey1 = bx + 10, bx + bw - 8, by + bh - 9, by + 24
    pts = []
    for j in range(60):
        t = j / 59
        if t < a: v = t / a
        elif t < a + dc: v = end + (1 - end) * math.exp(-4.2 * (t - a) / dc)
        else: v = end * (1 - 0.15 * (t - a - dc))
        pts.append(((ex0 + t * (ex1 - ex0)) * K, (ey0 - v * (ey0 - ey1)) * K))
    d.line(pts, fill=255, width=lw(2.2), joint='curve')
    d.line((ex0 * K, ey0 * K, ex1 * K, ey0 * K), fill=150, width=lw(1))
    # routing: down/up to the bus, solid (through the envelope) or dotted (direct), as the manual draws outputs
    sx = bx + bw / 2
    sy = by + bh if r == 0 else by
    (dashed if dotted else (lambda p, q, **kw: d.line((p[0] * K, p[1] * K, q[0] * K, q[1] * K), fill=255, width=lw(1.8))))((sx, sy), (sx, bus_y))
    d.ellipse(((sx - 3) * K, (bus_y - 3) * K, (sx + 3) * K, (bus_y + 3) * K), fill=255)
d.line((cols[0] + bw / 2) * K, 0, 0, 0) if False else None
d.line(((cols[0] + bw / 2) * K, bus_y * K, car[0] * K, bus_y * K), fill=255, width=lw(2.4))
# feedback loop on the kick, the manual's little loop in a corner
fx, fy = cols[0], rows[0]
d.line(((fx - 14) * K, (fy + 18) * K, (fx - 14) * K, (fy - 14) * K, (fx + 30) * K, (fy - 14) * K, (fx + 30) * K, fy * K), fill=255, width=lw(1.6))
d.polygon([((fx + 26) * K, (fy - 6) * K), ((fx + 34) * K, (fy - 6) * K), ((fx + 30) * K, fy * K)], fill=255)
d.line(((fx - 14) * K, (fy + 18) * K, fx * K, (fy + 18) * K), fill=255, width=lw(1.6))
# the carrier
d.rectangle((car[0] * K, car[1] * K, car[2] * K, car[3] * K), outline=255, width=lw(3))
d.text(((car[0] + 12) * K, (car[1] + 10) * K), 'MIDI', font=font('Bold', 15), fill=255)
d.text(((car[0] + 12) * K, (car[1] + 30) * K), 'TRACK', font=font('Bold', 15), fill=255)
for k in range(8):                                       # 8 notes leave on one channel
    yy = car[1] + 64 + k * 5
    d.line(((car[0] + 12) * K, yy * K, (car[0] + 12 + 14 + (k * 37 % 60)) * K, yy * K), fill=255, width=lw(2))
d.line((car[2] * K, bus_y * K, 1572 * K, bus_y * K), fill=255, width=lw(2.4))
d.polygon([(1572 * K, (bus_y - 7) * K), (1586 * K, bus_y * K), (1572 * K, (bus_y + 7) * K)], fill=255)
d.text((1500 * K, (bus_y - 26) * K), 'OUT', font=f_small, fill=255)
d.text((1086 * K, (bus_y - 19) * K), '8 → 1', font=f_small, fill=255)

# ---- eye candy ----
# MIDI note per voice, small, right in the box header
notes = [36, 38, 42, 46, 39, 45, 50, 49]
pats = [(0,4,8,12),(4,12),(0,2,4,6,8,10,12,14),(2,6,10,14),(4,12,15),(7,11),(3,13),(0,)]
f_tiny = font('SemiBold', 9)
for i in range(8):
    r, c = divmod(i, 4); bx, by = cols[c], rows[r]
    d.text(((bx + bw - 22) * K, (by + 7) * K), str(notes[i]), font=f_tiny, fill=170)
    # the voice's own 16-step pattern, outside the box (above the top row, below the bottom row)
    py = by - 13 if r == 0 else by + bh + 6
    for st in range(16):
        sx0 = bx + st * 5.75
        if st in pats[i]: d.rectangle((sx0 * K, py * K, (sx0 + 4) * K, (py + 7) * K), fill=255)
        else: d.rectangle(((sx0 + 1.5) * K, (py + 3) * K, (sx0 + 2.5) * K, (py + 4) * K), fill=200)
    # velocity ladder at the bus junction
    sx = bx + bw / 2
    for k in range(5):
        on = k < [5, 4, 2, 3, 4, 3, 3, 4][i]
        yy = bus_y + (10 + k * 5 if r == 1 else -10 - k * 5)
        d.line(((sx + 7) * K, yy * K, (sx + 7 + 4 + k * 2) * K, yy * K), fill=255 if on else 110, width=lw(1.6))
# modulation arcs: kick and snare reach across, like the manual's modulator links
def arc(cx, cy, rx, ry, a0, a1, w=1.4, dash=False):
    pts = [((cx + rx * math.cos(math.radians(a))) * K, (cy + ry * math.sin(math.radians(a))) * K) for a in np.linspace(a0, a1, 90)]
    if dash:
        for q in range(0, len(pts) - 3, 6): d.line(pts[q:q + 3], fill=200, width=lw(w))
    else: d.line(pts, fill=255, width=lw(w))
arc((cols[1] + cols[2] + bw) / 2, rows[0], (cols[2] - cols[1]) / 2 + 10, 34, 200, 340, dash=True)
arc((cols[2] + cols[3] + bw) / 2, rows[1] + bh, (cols[3] - cols[2]) / 2 + 10, 30, 20, 160, dash=True)
# top ruler: 16 steps across the diagram, 4 bars
rx0, rx1, ry = 650, 1480, 26
d.line((rx0 * K, ry * K, rx1 * K, ry * K), fill=200, width=lw(1))
for st in range(65):
    xx = rx0 + st * (rx1 - rx0) / 64
    hgt = 9 if st % 16 == 0 else (6 if st % 4 == 0 else 3)
    d.line((xx * K, ry * K, xx * K, (ry + hgt) * K), fill=220, width=lw(1))
    if st % 16 == 0 and st < 64: d.text(((xx + 3) * K, (ry + 4) * K), str(st // 16 + 1), font=f_tiny, fill=200)
d.text(((rx1 - 46) * K, (ry - 13) * K), '1/16 · 64', font=f_tiny, fill=180)

# scope: the summed drum loop, above the bus between the voices and the carrier
sx0, sx1, sy0, sy1 = 1090, 1336, 168, 236
dashed((sx0, sy0), (sx1, sy0), 3, 4, 1); dashed((sx0, sy1), (sx1, sy1), 3, 4, 1)
dashed((sx0, sy0), (sx0, sy1), 3, 4, 1); dashed((sx1, sy0), (sx1, sy1), 3, 4, 1)
d.text(((sx0 + 4) * K, (sy0 + 3) * K), 'SUM', font=f_tiny, fill=190)
decs = [0.85, 0.45, 0.12, 0.55, 0.35, 0.6, 0.5, 0.95]; freqs = [0.35, 1.6, 4.5, 4.1, 2.3, 0.6, 0.9, 3.7]
N = 900; mid = (sy0 + sy1) / 2 + 4; sig = np.zeros(N)
tt = np.arange(N) / N * 16
for v in range(8):
    for st in pats[v]:
        dt = tt - st
        m = dt >= 0
        sig[m] += np.exp(-dt[m] / (decs[v] * 1.4)) * np.sin(dt[m] * freqs[v] * 40) * (1.0 if v < 2 else 0.5)
sig /= np.abs(sig).max()
wp = [((sx0 + 6 + j / (N - 1) * (sx1 - sx0 - 12)) * K, (mid - sig[j] * 26) * K) for j in range(N)]
d.line(wp, fill=255, width=lw(1))
for st in range(17):
    xx = sx0 + 6 + st / 16 * (sx1 - sx0 - 12)
    d.line((xx * K, (sy1 - 4) * K, xx * K, sy1 * K), fill=200, width=lw(1))
# mini drum grid: 8 voices x 16 steps, below the bus (the Drum Mode grid)
gx0, gy0, cw, ch = 1096, 280, 14.6, 7.2
d.text(((gx0 - 2) * K, (gy0 - 14) * K), 'GRID 8 × 16', font=f_tiny, fill=190)
for v in range(8):
    for st in range(16):
        x = gx0 + st * cw + (st // 4) * 1.5; y = gy0 + v * ch
        if st in pats[v]: d.rectangle((x * K, y * K, (x + cw - 4) * K, (y + ch - 2.5) * K), fill=255)
        else: d.rectangle(((x + cw / 2 - 2.5) * K, (y + ch / 2 - 1.5) * K, (x + cw / 2 - 1) * K, (y + ch / 2) * K), fill=170)

# registration crosses in the corners of the diagram field
for (cx, cy) in ((620, 40), (1580, 40), (1580, 490), (620, 490)):
    d.line(((cx - 7) * K, cy * K, (cx + 7) * K, cy * K), fill=230, width=lw(1)); d.line((cx * K, (cy - 7) * K, cx * K, (cy + 7) * K), fill=230, width=lw(1))
    d.ellipse(((cx - 3.5) * K, (cy - 3.5) * K, (cx + 3.5) * K, (cy + 3.5) * K), outline=230, width=lw(1))
# carrier detail: channel label and a small LED row
d.text(((car[0] + 70) * K, (car[1] + 12) * K), 'CH 1', font=f_tiny, fill=180)
for k in range(8):
    cx = car[0] + 16 + k * 12
    (d.ellipse if k in (0, 2, 4) else d.rectangle)  # keep simple
    d.ellipse(((cx - 3) * K, (car[3] - 14) * K, (cx + 3) * K, (car[3] - 8) * K), fill=255 if k in (0, 2, 3, 6) else 0, outline=255, width=lw(1))

dg = np.asarray(dg, np.float32) / 255
# the diagram sits on a lighter field so it reads crisply over the bars
halo = np.asarray(Image.fromarray((dg * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(7)).filter(ImageFilter.GaussianBlur(3 * K)), np.float32) / 255
boxes = Image.new('L', (W, H), 0); bxd = ImageDraw.Draw(boxes)
for i in range(8):
    r, c = divmod(i, 4)
    bxd.rectangle((cols[c] * K, rows[r] * K, (cols[c] + bw) * K, (rows[r] + bh) * K), fill=255)
bxd.rectangle((car[0] * K, car[1] * K, car[2] * K, car[3] * K), fill=255)
boxes = np.asarray(boxes, np.float32) / 255
ink = ink * (1 - np.maximum(0.85 * halo, boxes))
ink = np.maximum(ink, dg * 0.97)

# ---- type, as on the water banner ----
tm = Image.new('L', (W, H), 0); td = ImageDraw.Draw(tm)
def tracked(x, y, t, f, track, fill=255):
    for ch in t:
        td.text((x, y), ch, font=f, fill=fill); x += td.textlength(ch, font=f) + track * K
    return x
ft = font('ExtraBold', 132)
tracked(76 * K, 108 * K, 'Drum', ft, -4.5); tracked(76 * K, 228 * K, 'Mode', ft, -4.5)
tracked(80 * K, 84 * K, 'DIGITONE MK1 · OS 1.44 · ELEKLOADER MOD', font('SemiBold', 14), 2.8)
x = 80 * K
for p in ('8 VOICES PER MIDI TRACK', '/', 'FULLY INTEGRATED'):
    x = tracked(x, 404 * K, p, font('SemiBold', 15), 2.4, 255 if p != '/' else 120) + 26 * K
for s in range(16):
    x0s = 80 * K + s * 15 * K + (s // 4) * 8 * K
    td.rectangle((x0s, 446 * K, x0s + 9 * K, 455 * K), fill=255 if s in (0, 4, 8, 10, 12) else 0, outline=255, width=lw(1))
tm = np.asarray(tm, np.float32) / 255
ink = np.maximum(ink, tm * 0.96)

# ---- stipple ----
grain = rng.random((H, W)).astype(np.float32)
coarse = np.asarray(Image.fromarray((rng.random((H // 2, W // 2)) * 255).astype(np.uint8)).resize((W, H), Image.NEAREST), np.float32) / 255
dots = (grain * 0.6 + coarse * 0.4 < np.power(np.clip(ink, 0, 1), 1.15)).astype(np.float32)
dots = np.asarray(Image.fromarray((dots * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.45)), np.float32) / 255
paper = KRAFT * (0.94 + 0.08 * fbm(120 * K, 4, 9))[..., None] + (rng.random((H, W, 1)) * 10 - 5)
img = paper * (1 - dots[..., None]) + INK * dots[..., None]
Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).save('drum-mode-manual.png', optimize=True)
print('ok')
