import os, sys, json, pickle
sys.path.insert(0, '/home/claude/digitone-mods/drum-mode/tests')
from emu_session import Digitone
syx, work = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
d = Digitone(syx, os.path.join(work, 'home'), audio=True)
mo = [x for x in d.s.sources if type(x).__name__ == 'MidiOut'][0]
wire = []
mo.sink = lambda x: wire.append((d.s.now_ms(), bytes(x) if not isinstance(x, int) else bytes((x,))))
d.setup_midi_track_a(); d.mod_menu_first(); d.wait(1500)

beat = {1: [1, 7, 9, 11], 2: [5, 13], 3: [1, 3, 5, 7, 9, 11, 13], 4: [15], 5: [13]}
for lane, steps in beat.items():
    d.tap(str(lane), hold_ms=60, after_ms=300)        # select the lane (plays it)
    d.tap('RECORD', after_ms=400)
    for s in steps:
        d.tap(str(s), hold_ms=40, after_ms=150)
    d.tap('RECORD', after_ms=400)

def grab():
    frame = bytes(d.s.frames[-1][1])
    return frame, d.leds()

frames = []
# scene 1: grid recording, flipping lanes
d.tap('1', hold_ms=60, after_ms=300)
d.tap('RECORD', after_ms=500)
for lane in (1, 2, 3, 4, 5):
    d.press('TRIG', after_ms=60); d.tap(str(lane), hold_ms=40, after_ms=120); d.release('TRIG', after_ms=300)
    for _ in range(9):
        d.wait(100); frames.append(('grid', lane) + grab() + ([],))
d.tap('RECORD', after_ms=500)
d.tap('3', hold_ms=60, after_ms=300)                 # select the closed hat for the play scene
# scene 2: playing
d.leds(); wire.clear()
d.tap('PLAY', hold_ms=50, after_ms=20)
t0 = d.s.now_ms()
buf = b''
last = t0
while d.s.now_ms() - t0 < 4600:
    d.wait(40)
    notes = []
    for t, b in wire:
        for x in b:
            buf += bytes((x,))
            if len(buf) >= 3 and buf[-3] & 0xF0 == 0x90 and buf[-1] > 0:
                notes.append(buf[-2])
    wire.clear()
    frames.append(('play', 0) + grab() + (notes,))
d.tap('STOP', after_ms=300)
pal = d.palette()
pickle.dump({'frames': frames, 'palette': pal}, open(os.path.join(work, 'frames.pkl'), 'wb'))
print(len(frames), 'frames; palette', pal)
