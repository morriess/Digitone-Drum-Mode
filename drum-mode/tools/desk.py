import numpy as np, random, math
from PIL import Image, ImageDraw, ImageFilter, ImageFont
random.seed(7); np.random.seed(7)
U = '/home/claude/digitone-mods/drum-mode/docs/ui/'
F = '/usr/share/fonts/opentype/inter/'
W, H = 1600, 1000

# ---- 1. the flat top panel (a generic groovebox, no brand) ----
PW, PH = 1400, 760
p = Image.new('RGB', (PW, PH), (52, 54, 60)); d = ImageDraw.Draw(p)
# brushed metal
arr = np.array(p).astype(np.float32)
arr += (np.random.randn(PH, 1, 1) * 2.2) + np.random.randn(PH, PW, 1) * 1.5
p = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)); d = ImageDraw.Draw(p)
d.rounded_rectangle((6, 6, PW - 6, PH - 6), 36, outline=(78, 80, 88), width=4)
# screen
scr = Image.open(U + 'drum-notes-page.png').convert('RGB').point(lambda v: 245 if v > 128 else 0)
scr = scr.resize((512, 256), Image.NEAREST)
glow = scr.filter(ImageFilter.GaussianBlur(4))
scr = Image.blend(scr, glow, 0.3).point(lambda v: min(255, int(v * 1.0)))
sx, sy = 470, 70
d.rounded_rectangle((sx - 22, sy - 22, sx + 534, sy + 278), 14, fill=(10, 10, 12), outline=(30, 30, 34), width=3)
p.paste(scr, (sx, sy))
# encoders
def knob(cx, cy, r):
    d.ellipse((cx - r - 4, cy - r - 4, cx + r + 4, cy + r + 4), fill=(26, 27, 30))
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(70, 72, 78), outline=(95, 97, 104), width=2)
    d.ellipse((cx - r + 8, cy - r + 8, cx + r - 8, cy + r - 8), fill=(58, 60, 66))
    a = random.uniform(-2.4, 2.4)
    d.line((cx, cy, cx + (r - 6) * math.sin(a), cy - (r - 6) * math.cos(a)), fill=(235, 235, 240), width=4)
for row in range(2):
    for c in range(4):
        knob(1110 + c * 70, 110 + row * 110, 26)
knob(300, 150, 52)   # big data knob
for c in range(2):
    knob(110 + c * 0, 110 + 0, 0) if False else None
# small buttons left
for r_ in range(3):
    for c in range(2):
        x, y = 70 + c * 90, 330 + r_ * 70
        d.rounded_rectangle((x, y, x + 60, y + 34), 8, fill=(36, 37, 41), outline=(80, 82, 90), width=2)
# track keys row
for c in range(8):
    x = 470 + c * 66; y = 400
    lit = c == 4
    d.rounded_rectangle((x, y, x + 48, y + 30), 7, fill=(255, 150, 60) if lit else (36, 37, 41), outline=(80, 82, 90), width=2)
# 16 trig keys with LEDs (one running-light frame: kick, snare, hat lit, key 11 running)
cols = {0: (255, 255, 255), 2: (230, 230, 230), 1: (120, 120, 120), 10: (255, 255, 255)}
kw, gap, ky = 62, 18, 560
x0 = (PW - (16 * kw + 15 * gap + 30)) // 2
leds = []
for k in range(16):
    x = x0 + k * (kw + gap) + (30 if k >= 8 else 0)
    c = cols.get(k)
    d.rounded_rectangle((x, ky, x + kw, ky + kw), 12, fill=(30, 31, 35), outline=(86, 88, 96), width=3)
    if c:
        d.rounded_rectangle((x + 6, ky + 6, x + kw - 6, ky + kw - 6), 9, fill=c)
        leds.append((x + kw // 2, ky + kw // 2, c))
    else:
        d.rounded_rectangle((x + 6, ky + 6, x + kw - 6, ky + kw - 6), 9, fill=(44, 45, 50))
# LED bloom on the panel
bloom = Image.new('RGB', (PW, PH), (0, 0, 0)); bd = ImageDraw.Draw(bloom)
for cx, cy, c in leds:
    bd.ellipse((cx - 46, cy - 46, cx + 46, cy + 46), fill=tuple(int(v * 0.7) for v in c))
bd.rectangle((sx - 10, sy - 10, sx + 522, sy + 266), fill=(40, 42, 50))
bloom = bloom.filter(ImageFilter.GaussianBlur(26))
p = Image.fromarray(np.clip(np.array(p).astype(int) + np.array(bloom).astype(int) * 0.6, 0, 255).astype(np.uint8))

# ---- 2. the scene ----
# wall + bokeh
yy = np.linspace(0, 1, H)[:, None]
wall = np.zeros((H, W, 3), np.float32)
wall[..., 0] = 26 + 30 * (1 - yy); wall[..., 1] = 20 + 18 * (1 - yy); wall[..., 2] = 24 + 22 * (1 - yy)
scene = Image.fromarray(np.clip(wall, 0, 255).astype(np.uint8))
bok = Image.new('RGB', (W, H), (0, 0, 0)); bkd = ImageDraw.Draw(bok)
for _ in range(38):
    x, y, r = random.randint(0, W), random.randint(30, 420), random.randint(14, 60)
    c = random.choice([(255, 150, 70), (255, 190, 120), (120, 150, 255), (255, 110, 90)])
    bkd.ellipse((x - r, y - r, x + r, y + r), fill=tuple(int(v * random.uniform(0.25, 0.6)) for v in c))
bok = bok.filter(ImageFilter.GaussianBlur(10))
scene = Image.fromarray(np.clip(np.array(scene).astype(int) + np.array(bok), 0, 255).astype(np.uint8))
# desk (wood) from y 470
DY = 470
desk = np.zeros((H - DY, W, 3), np.float32)
xs = np.linspace(0, 1, W)[None, :]; ys = np.linspace(0, 1, H - DY)[:, None]
grain = np.sin((ys ** 0.6) * 160 + np.sin(xs * 7 + ys * 3) * 1.2 + xs * 4) * 0.5 + 0.5
noise = np.random.rand(H - DY, W) * 0.12
base = np.array([104, 66, 40], np.float32)
shade = (0.55 + 0.45 * ys) * (0.85 + 0.15 * grain + noise)
desk = base[None, None, :] * shade[..., None]
scene_arr = np.array(scene).astype(np.float32); scene_arr[DY:] = desk
scene = Image.fromarray(np.clip(scene_arr, 0, 255).astype(np.uint8))
sd = ImageDraw.Draw(scene)
sd.line((0, DY, W, DY), fill=(140, 96, 62), width=3)

# perspective: top face quad (back-left, back-right, front-right, front-left)
quad = [(360, 470), (1240, 470), (1450, 790), (150, 790)]
def coeffs(src, dst):
    m = []
    for (x, y), (X, Y) in zip(dst, src):
        m.append([x, y, 1, 0, 0, 0, -X * x, -X * y]); m.append([0, 0, 0, x, y, 1, -Y * x, -Y * y])
    A = np.array(m, float); B = np.array([c for pt in src for c in pt], float)
    return np.linalg.solve(A, B).tolist()
src = [(0, 0), (PW, 0), (PW, PH), (0, PH)]
top = p.transform((W, H), Image.PERSPECTIVE, coeffs(src, quad), Image.BICUBIC)
mask = Image.new('L', (W, H), 0); ImageDraw.Draw(mask).polygon(quad, fill=255)
# shadow
sh = Image.new('L', (W, H), 0)
ImageDraw.Draw(sh).polygon([(340, 500), (1270, 500), (1500, 850), (110, 850)], fill=200)
sh = sh.filter(ImageFilter.GaussianBlur(28))
scene = Image.composite(Image.new('RGB', (W, H), (12, 8, 6)), scene, sh)
# front face (thickness)
fd = ImageDraw.Draw(scene)
fd.polygon([(150, 790), (1450, 790), (1446, 842), (156, 842)], fill=(30, 31, 35))
fd.line((150, 790, 1450, 790), fill=(110, 112, 120), width=3)
# side faces
fd.polygon([(150, 790), (360, 470), (360, 500), (156, 842)], fill=(38, 39, 44))
fd.polygon([(1450, 790), (1240, 470), (1240, 500), (1446, 842)], fill=(24, 25, 28))
scene.paste(top, (0, 0), mask)
# LED / screen light spilling on the desk
spill = Image.new('RGB', (W, H), (0, 0, 0)); spd = ImageDraw.Draw(spill)
spd.ellipse((420, 820, 1180, 960), fill=(70, 66, 70))
spill = spill.filter(ImageFilter.GaussianBlur(40))
scene = Image.fromarray(np.clip(np.array(scene).astype(int) + np.array(spill), 0, 255).astype(np.uint8))
# depth of field: blur the far part a little
far = scene.filter(ImageFilter.GaussianBlur(3))
dof = Image.new('L', (W, H)); dpx = np.zeros((H, W), np.uint8)
for y in range(H):
    v = 0 if 520 <= y <= 900 else min(255, int(abs(y - (520 if y < 520 else 900)) * 2.2))
    dpx[y] = v
dof = Image.fromarray(dpx)
scene = Image.composite(far, scene, dof)
# warm lamp light + vignette
L = np.array(scene).astype(np.float32)
X, Y = np.meshgrid(np.linspace(-1, 1, W), np.linspace(-1, 1, H))
lamp = np.exp(-((X - 0.55) ** 2 + (Y + 0.2) ** 2) * 1.6)
L *= (0.72 + 0.45 * lamp)[..., None]
L[..., 0] *= 1.04; L[..., 2] *= 0.96
vig = 1 - 0.45 * (X ** 2 + Y ** 2) ** 1.3
L *= vig[..., None]
scene = Image.fromarray(np.clip(L, 0, 255).astype(np.uint8))
# caption
cd = ImageDraw.Draw(scene)
cd.text((60, 52), 'Drum Mode', font=ImageFont.truetype(F + 'Inter-Bold.otf', 54), fill=(244, 244, 248))
cd.text((62, 118), 'eight drum voices from one Digitone MIDI track', font=ImageFont.truetype(F + 'Inter-Medium.otf', 26), fill=(255, 140, 80))
cd.text((W - 400, H - 44), 'artist impression  ·  screen from the real firmware', font=ImageFont.truetype(F + 'Inter-Regular.otf', 15), fill=(170, 160, 150))
scene.save('./drum-mode-desk.png')
print('ok')
