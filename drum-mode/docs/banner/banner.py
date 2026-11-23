# Renders the Drum Mode header (the Design canvas "Drum Mode header", Main.dc.html) as a PNG at 2x.
from PIL import Image, ImageDraw, ImageFont, ImageFilter
K = 2
W, H = 1600, 520
F = '/usr/share/fonts/opentype/inter/'
def font(w, px): return ImageFont.truetype(F + 'Inter-%s.otf' % w, int(px * K))
BG = (11, 11, 12); ACC = (255, 74, 42)
im = Image.new('RGB', (W * K, H * K), BG)
d = ImageDraw.Draw(im)

def text_w(t, f, track):
    return sum(d.textlength(c, font=f) for c in t) + track * K * (len(t) - 1)
def draw_tracked(x, y, t, f, fill, track):
    for c in t:
        d.text((x, y), c, font=f, fill=fill)
        x += d.textlength(c, font=f) + track * K
    return x
def cap_top(f):                       # offset from the draw y to the top of capitals
    return f.getbbox('H')[1]

# ---- left column (vertically centred) ----
f_lab, f_title, f_sub = font('SemiBold', 14), font('ExtraBold', 124), font('Medium', 15)
lab_h, title_lh, sub_h = 17, 124 * 0.92, 18
left_h = lab_h + 22 + 2 * title_lh + 22 + sub_h
y0 = (H - left_h) / 2
x0 = 80
# label row: LED dot + text
dot = Image.new('RGBA', (60 * K, 60 * K), (0, 0, 0, 0))
ImageDraw.Draw(dot).rounded_rectangle((25 * K, 25 * K, 35 * K, 35 * K), 2 * K, fill=ACC + (255,))
glow = dot.filter(ImageFilter.GaussianBlur(6 * K))
im.paste(glow, (int((x0 - 25) * K), int((y0 + 3.5 - 25) * K)), glow)
im.paste(dot, (int((x0 - 25) * K), int((y0 + 3.5 - 25) * K)), dot)
d = ImageDraw.Draw(im)
lab = 'DIGITONE MK1 · OS 1.44 · ELEKLOADER MOD'
draw_tracked((x0 + 24) * K, (y0 + 2) * K - cap_top(f_lab) + 0, lab, f_lab, (141, 138, 132), 14 * 0.2)
# title, tight tracking
ty = y0 + lab_h + 22
for i, line in enumerate(('Drum', 'Mode')):
    yl = (ty + i * title_lh) * K + (title_lh * K - f_title.getbbox('Hg')[3]) / 2 + 6 * K
    draw_tracked(x0 * K - 4 * K, yl, line, f_title, (244, 239, 230), -124 * 0.035)
# sub line
sy = (ty + 2 * title_lh + 22) * K
x = x0 * K
parts = ['8 VOICES', '/', '1 MIDI CHANNEL', '/', '16 STEPS']
for p in parts:
    col = (74, 72, 68) if p == '/' else (185, 180, 171)
    x = draw_tracked(x, sy - cap_top(f_sub) + 2 * K, p, f_sub, col, 15 * 0.16) + 28 * K

# ---- right: the pattern grid ----
lanes = [('BD', [1, 5, 9, 11, 13]), ('SD', [5, 13]), ('CH', [1, 3, 5, 7, 9, 11, 13, 15]), ('OH', [15]),
         ('CP', [13]), ('LT', [14]), ('HT', [16]), ('CY', [1])]
play, size, gap, ggap = 5, 24, 6, 14
grid_w = 34 + 14 + 16 * size + 12 * gap + 3 * ggap
grid_h = 8 * size + 7 * 8 + 6 + 13
gx, gy = W - 80 - grid_w, (H - grid_h) / 2
f_lane, f_num = font('Bold', 14), font('SemiBold', 11)
glow_layer = Image.new('RGB', (W * K, H * K), (0, 0, 0)); gd = ImageDraw.Draw(glow_layer)
def cell_x(s):
    i = s - 1
    return gx + 34 + 14 + i * (size + gap) + (i // 4) * (ggap - gap)
for r, (name, steps) in enumerate(lanes):
    ry = gy + r * (size + 8)
    lc = ACC if play in steps else (141, 138, 132)
    tw = text_w(name, f_lane, 14 * 0.06)
    draw_tracked((gx + 34) * K - tw, (ry + size / 2) * K - (f_lane.getbbox('H')[1] + f_lane.getbbox('H')[3]) / 2, name, f_lane, lc, 14 * 0.06)
    for s in range(1, 17):
        on, head = s in steps, s == play
        bg, bd = (27, 27, 29), (38, 38, 42)
        if on: bg = bd = (236, 231, 221)
        if head: bd = ACC
        if on and head: bg = bd = ACC
        x1, y1 = cell_x(s) * K, ry * K
        box = (x1, y1, x1 + size * K - 1, y1 + size * K - 1)
        if on:
            gd.rounded_rectangle(box, 4 * K, fill=ACC if head else (90, 88, 84))
        d.rounded_rectangle(box, 4 * K, fill=bd)
        d.rounded_rectangle((box[0] + 2 * K, box[1] + 2 * K, box[2] - 2 * K, box[3] - 2 * K), 3 * K, fill=bg)
ny = gy + 8 * size + 7 * 8 + 6
for s in range(1, 17):
    t = str(s); col = ACC if s == play else (92, 90, 85)
    cx = (cell_x(s) + size / 2) * K
    d.text((cx - d.textlength(t, font=f_num) / 2, ny * K), t, font=f_num, fill=col)
glow_layer = glow_layer.filter(ImageFilter.GaussianBlur(7 * K))
out = Image.fromarray(__import__('numpy').clip(__import__('numpy').array(im).astype(int) + __import__('numpy').array(glow_layer).astype(int) * 0.55, 0, 255).astype('uint8'))
rings = Image.open('rings.png').convert('RGBA').resize((400 * K, 400 * K), Image.LANCZOS)
out.paste(rings, (int(559 * K), int(60 * K)), rings)
out.save('drum-mode-banner.png', optimize=True)
print(out.size)
