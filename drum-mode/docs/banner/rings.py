# The generative "circular sequencer": 8 rings (BD inside .. CY outside), 16 steps each, the same beat as the grid.
import math
lanes = [[1, 5, 9, 11, 13], [5, 13], [1, 3, 5, 7, 9, 11, 13, 15], [15], [13], [14], [16], [1]]
play, ACC, ON, OFF = 5, '#ff4a2a', '#ece7dd', '#2a2a2e'
S, C = 480, 240
def arc(r, a0, a1):
    p = lambda a: (C + r * math.cos(math.radians(a - 90)), C + r * math.sin(math.radians(a - 90)))
    (x0, y0), (x1, y1) = p(a0), p(a1)
    return 'M%.2f %.2f A%.2f %.2f 0 0 1 %.2f %.2f' % (x0, y0, r, r, x1, y1)
o = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d">' % (S, S, S, S)]
for i in range(36):                                   # hairline texture
    r = 26 + i * 6.2
    o.append('<circle cx="%d" cy="%d" r="%.1f" fill="none" stroke="#ece7dd" stroke-opacity="%.3f" stroke-width="0.6"/>' % (C, C, r, 0.03 + 0.03 * (i % 3 == 0)))
for k in range(64):                                   # outer tick ring
    a = math.radians(k * 360 / 64 - 90); r0, r1 = 226, 232 if k % 4 else 238
    o.append('<line x1="%.2f" y1="%.2f" x2="%.2f" y2="%.2f" stroke="%s" stroke-width="1.4"/>' % (
        C + r0 * math.cos(a), C + r0 * math.sin(a), C + r1 * math.cos(a), C + r1 * math.sin(a), ACC if k == (play - 1) * 4 else '#4a4844'))
for li, steps in enumerate(lanes):
    r = 66 + li * 21
    for s in range(1, 17):
        a0 = (s - 1) * 22.5 + 2.2; a1 = s * 22.5 - 2.2
        on, head = s in steps, s == play
        if on:
            col = ACC if head else ON
            o.append('<path d="%s" fill="none" stroke="%s" stroke-width="10" stroke-linecap="butt"/>' % (arc(r, a0, a1), col))
        else:
            o.append('<path d="%s" fill="none" stroke="%s" stroke-width="%s"/>' % (arc(r, a0, a1), ACC if head else OFF, 2 if head else 3))
a = math.radians((play - 0.5) * 22.5 - 90)            # the playhead
o.append('<line x1="%.2f" y1="%.2f" x2="%.2f" y2="%.2f" stroke="%s" stroke-width="1.2" stroke-opacity="0.8"/>' % (
    C + 40 * math.cos(a), C + 40 * math.sin(a), C + 222 * math.cos(a), C + 222 * math.sin(a), ACC))
o.append('<circle cx="%d" cy="%d" r="5" fill="%s"/>' % (C, C, ACC))
o.append('</svg>')
open('rings.svg', 'w').write('\n'.join(o))
import cairosvg
cairosvg.svg2png(url='rings.svg', write_to='rings.png', output_width=S * 2, output_height=S * 2)
print('ok')
