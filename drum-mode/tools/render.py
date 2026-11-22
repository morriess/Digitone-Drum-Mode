import pickle, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter
D = pickle.load(open('./frames.pkl', 'rb'))
pal = D['palette']
F = '/usr/share/fonts/opentype/inter/'
f_title = ImageFont.truetype(F + 'Inter-Bold.otf', 30)
f_sub = ImageFont.truetype(F + 'Inter-Medium.otf', 18)
f_small = ImageFont.truetype(F + 'Inter-SemiBold.otf', 14)
f_tiny = ImageFont.truetype(F + 'Inter-Medium.otf', 12)
NAMES = ['BD', 'SD', 'CH', 'OH', 'CP', 'LT', 'HT', 'CY']
NOTE2 = {36: 'BD', 38: 'SD', 42: 'CH', 46: 'OH', 39: 'CP', 45: 'LT', 50: 'HT', 49: 'CY'}
BG = (18, 19, 22); PANEL = (44, 46, 52); ACC = (255, 122, 69)
W, H = 760, 520

def screen_img(frame, scale=4):
    im = Image.new('RGB', (128, 64))
    px = im.load()
    for x in range(128):
        for y in range(64):
            if (frame[7 - y // 8 + 8 * x] >> (y % 8)) & 1:
                px[x, y] = (236, 240, 255)
    im = im.resize((128 * scale, 64 * scale), Image.NEAREST)
    glow = im.filter(ImageFilter.GaussianBlur(3))
    return Image.blend(im, glow, 0.25)

def led_rgb(i):
    r, g, b = pal.get(i, (1, 1, 1))[:3]
    return tuple(min(255, int(c * 8.2)) for c in (r, g, b))

def render(kind, lane, frame, leds, notes, hit_memory):
    im = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(im)
    d.text((32, 22), 'Drum Mode', font=f_title, fill=(245, 245, 245))
    d.text((32 + d.textlength('Drum Mode', font=f_title) + 12, 33), 'for the Digitone mk1', font=f_sub, fill=(170, 172, 180))
    cap = ('GRID RECORDING: pick a voice, its steps light up' if kind == 'grid'
           else 'Playing: 8 drum voices on one MIDI track')
    d.text((32, 64), cap, font=f_sub, fill=ACC)
    # panel
    d.rounded_rectangle((24, 100, W - 24, H - 24), 18, fill=PANEL)
    sx, sy = (W - 512) // 2, 118
    d.rounded_rectangle((sx - 10, sy - 10, sx + 522, sy + 266), 10, fill=(8, 8, 10))
    im.paste(screen_img(frame), (sx, sy))
    # trig keys
    ky = sy + 256 + 34
    kw, gap = 34, 8
    total = 16 * kw + 15 * gap + 18
    x0 = (W - total) // 2
    for k in range(16):
        x = x0 + k * (kw + gap) + (18 if k >= 8 else 0)
        c = led_rgb(leds.get(k, 2))
        lit = sum(c) > 60
        if lit:
            gl = Image.new('RGBA', (kw + 30, kw + 30), (0, 0, 0, 0))
            ImageDraw.Draw(gl).rounded_rectangle((15, 15, 15 + kw, 15 + kw), 7, fill=c + (150,))
            gl = gl.filter(ImageFilter.GaussianBlur(7))
            im.paste(gl, (x - 15, ky - 15), gl)
            d = ImageDraw.Draw(im)
        d.rounded_rectangle((x, ky, x + kw, ky + kw), 7, fill=c if lit else (62, 64, 72), outline=(90, 92, 100))
        lab = str(k + 1)
        d.text((x + kw / 2 - d.textlength(lab, font=f_tiny) / 2, ky + kw + 5), lab, font=f_tiny, fill=(150, 152, 160))
        if k < 8:
            hot = NAMES[k] in hit_memory
            nm = NAMES[k]
            d.text((x + kw / 2 - d.textlength(nm, font=f_small) / 2, ky - 22), nm, font=f_small,
                   fill=ACC if hot else (200, 202, 210))
    if kind == 'play':
        d.text((x0 + 8 * (kw + gap) + 18, ky - 22), 'running light', font=f_small, fill=(200, 202, 210))
    return im

out = []
mem = {}
for kind, lane, frame, leds, notes in D['frames']:
    for n in notes:
        if n in NOTE2: mem[NOTE2[n]] = 3
    hot = {k for k, v in mem.items() if v > 0}
    out.append(render(kind, lane, frame, leds, notes, hot))
    for k in list(mem): mem[k] -= 1
grid = out[:45]; play = out[45:]
dur = [100] * len(grid) + [40] * len(play)
seq = grid + play
pal_im = seq[0].quantize(colors=128, method=Image.Quantize.MEDIANCUT)
q = [f.quantize(palette=pal_im, dither=Image.Dither.NONE) for f in seq]
q[0].save('./drum-mode-demo.gif', save_all=True, append_images=q[1:], duration=dur, loop=0, optimize=True)
seq[60].save('./frame_play.png'); seq[20].save('./frame_grid.png')
print('ok', len(seq))
