# Shop thumbnail: the README header as 128x64 1-bit line art (the shop refuses dithered pictures).
# Usage: python3 thumb.py /usr/share/fonts/opentype/inter/   (writes thumb.png and thumb-preview.png here)
# Shop thumbnail (128x64, 1-bit, drawn): the README header's elements as line art.
import math
from PIL import Image, ImageDraw, ImageFont
import sys
F = sys.argv[1]
im = Image.new('1', (128, 64), 0); d = ImageDraw.Draw(im); d.fontmode = '1'
# ripples: dashed rings around a drop, right side
def ring(cx, cy, r, dash, gap, phase=0):
    circ = 2 * math.pi * r; n = max(1, int(circ / (dash + gap)))
    for i in range(n):
        a0 = (i * (dash + gap) + phase) / r; a1 = a0 + dash / r
        pts = [(cx + r * math.cos(a), cy + r * math.sin(a)) for a in [a0 + (a1 - a0) * t / 6 for t in range(7)]]
        d.line(pts, fill=1, width=1)
cx, cy = 100, 30
d.ellipse((cx - 1, cy - 1, cx + 1, cy + 1), fill=1)
d.ellipse((cx - 5, cy - 5, cx + 5, cy + 5), outline=1)
ring(cx, cy, 10, 4, 2)
ring(cx, cy, 16, 5, 3, 2)
ring(cx, cy, 23, 3, 3)
ring(cx, cy, 31, 2, 3, 1)
# a second, smaller drop, top right
c2 = (121, 8)
d.point(c2, fill=1); d.ellipse((c2[0] - 3, c2[1] - 3, c2[0] + 3, c2[1] + 3), outline=1); ring(*c2, 7, 3, 2)
# crosshair line from the left edge of the rings
for x in range(58, 72, 2): d.point((x, cy), fill=1)
# clear behind the title, then the title
ft = ImageFont.truetype(F + 'Inter-ExtraBold.otf', 19)
d.rectangle((0, 0, 60, 63), fill=0)
d.text((3, 5), 'Drum', font=ft, fill=1)
d.text((3, 24), 'Mode', font=ft, fill=1)
# the 16 step marks, 4 groups of 4, first of each filled (like the header)
d.rectangle((0, 50, 95, 57), fill=0)
x = 4
for g in range(4):
    for s in range(4):
        if s == 0 or (g == 2 and s == 2): d.rectangle((x, 52, x + 3, 55), fill=1)
        else: d.rectangle((x, 52, x + 3, 55), outline=1)
        x += 5
    x += 3
im.save('thumb.png')
im.convert('L').resize((512, 256), Image.NEAREST).save('thumb-preview.png')
print(sorted(set(im.convert('L').getdata())))
