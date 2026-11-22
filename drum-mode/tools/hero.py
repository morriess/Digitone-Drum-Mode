from PIL import Image, ImageDraw, ImageFont, ImageFilter
U = '/home/claude/digitone-mods/drum-mode/docs/ui/'
F = '/usr/share/fonts/opentype/inter/'
ft = lambda n, s: ImageFont.truetype(F + n, s)
BG = (18, 19, 22); CARD = (34, 36, 42); ACC = (255, 122, 69); TXT = (238, 238, 242); MUT = (160, 162, 172)
W, H = 1200, 675
im = Image.new('RGB', (W, H), BG); d = ImageDraw.Draw(im)
d.text((56, 44), 'Drum Mode', font=ft('Inter-Bold.otf', 58), fill=TXT)
d.text((58, 116), 'Turn a Digitone mk1 MIDI track into an 8-voice drum sequencer', font=ft('Inter-Medium.otf', 26), fill=ACC)
d.text((58, 152), 'for your external drum machine or sampler, on one MIDI channel', font=ft('Inter-Regular.otf', 20), fill=MUT)

def screen(path, w):
    s = Image.open(path).convert('RGB')
    s = s.point(lambda v: 240 if v > 128 else 0)
    s = s.resize((w, w // 2), Image.LANCZOS)
    return s

cards = [('drum-notes-page.png', 'All 8 notes at a glance', 'LFO page: knobs A-H set each voice'),
         ('drum-velocity-page.png', 'Velocity and mutes', 'FUNC + pad mutes a voice'),
         ('mod-menu.png', 'One switch per track', 'Mod Menu, per pattern')]
cw, cx0, cy = 340, 56, 206
for i, (f, t1, t2) in enumerate(cards):
    x = cx0 + i * (cw + 34)
    d.rounded_rectangle((x, cy, x + cw, cy + 268), 16, fill=CARD)
    s = screen(U + f, cw - 28)
    d.rounded_rectangle((x + 10, cy + 10, x + cw - 10, cy + 20 + s.height), 8, fill=(6, 6, 8))
    im.paste(s, (x + 14, cy + 14))
    d.text((x + 16, cy + 196), t1, font=ft('Inter-SemiBold.otf', 21), fill=TXT)
    d.text((x + 16, cy + 228), t2, font=ft('Inter-Regular.otf', 17), fill=MUT)

feats = ['Pads 1-8 play and select voices', 'Live record, several drums per step',
         'Grid edit one voice at a time', 'Running light on keys 9-16',
         'Settings saved per pattern', 'Stock trig functions stay']
fy = 506
for i, t in enumerate(feats):
    x = 56 + (i % 3) * 374; y = fy + (i // 3) * 40
    d.ellipse((x, y + 8, x + 10, y + 18), fill=ACC)
    d.text((x + 22, y), t, font=ft('Inter-Medium.otf', 20), fill=TXT)
d.line((56, 600, W - 56, 600), fill=(52, 54, 62), width=1)
d.text((56, 616), 'Free & open source (GPL)  ·  beta  ·  OS 1.44  ·  built on elekloader  ·  github.com/morriess/Digitone-Drum-Mode',
       font=ft('Inter-Regular.otf', 18), fill=MUT)
im.save('./drum-mode-overview.png')
