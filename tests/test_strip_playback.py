# SPDX-License-Identifier: GPL-2.0-or-later
"""Drum Mode 0.6 in digiemu: the lane strip blinks as the sequencer plays each lane.

    uv run --project $DIGIEMU python tests/test_strip_playback.py out/Digitone_OS1.44_DrumMode.syx [workdir]

Records a kick on step 2, a snare on 6 and a kick + open hat on 12, mutes the open hat, then plays and watches the
corner strip frame by frame: each lane's box must light when its step plays (not when the sequencer fires it, one
step earlier), the muted lane and the lanes never played must not, and the boxes must go dark again between hits.
Audio on: the Digitone's sequencer is clocked by it.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu_session import Digitone          # noqa: E402

STEP = 5400000
NAMES = ['BD', 'SD', 'CH', 'OH', 'CP', 'LT', 'HT', 'CY']
results = []


def check(name, ok, detail=''):
    results.append((name, ok))
    print('%s  %s%s' % ('PASS' if ok else 'FAIL', name, ('  (%s)' % detail) if detail else ''))


def main():
    syx = os.path.abspath(sys.argv[1])
    work = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else 'emu-work')
    os.makedirs(work, exist_ok=True)
    d = Digitone(syx, os.path.join(work, 'home'), audio=True)
    S = d.symbols

    def timeline():
        return int.from_bytes(d.mem(0x80004614, 4), 'big')

    def fired():
        s, _, t, _ = struct.unpack('>iiII', d.mem(S['dm_fire'], 16))
        return s, t

    def wait_for(step_playing, phase):
        for _ in range(4000):
            s, t = fired()
            if s == (step_playing + 1) % 16 and (timeline() - t) >= phase * STEP:
                return True
            d.wait(2)
        return False

    def lit(lane):
        """A box is lit when filled: its centre and its top edge both on (an outline has the top only, a muted
        lane's dash the centre only)."""
        x = 96 + 4 * lane + 1
        return d.pixel(x, 4) == 1 and d.pixel(x, 2) == 1

    d.setup_midi_track_a()
    d.mod_menu_first()
    d.wait(1200)

    d.press('RECORD')
    d.tap('PLAY', hold_ms=50, after_ms=50)
    d.release('RECORD', after_ms=50)
    for pads, step in ((('1',), 2), (('2',), 6), (('1', '4'), 12)):
        wait_for(step, 0.2)
        for p in pads:
            d.press(p, after_ms=5)
        d.wait(20)
        for p in pads:
            d.release(p, after_ms=5)
    d.wait(400)
    d.tap('STOP', after_ms=300)
    d.tap('STOP', after_ms=300)

    d.press('FUNC', after_ms=40)                # mute the open hat
    d.tap('4', hold_ms=60, after_ms=60)
    d.release('FUNC', after_ms=200)
    d.wait(2600)                                # its message times out
    check('nothing lit before playing', not any(lit(l) for l in range(8)), str([lit(l) for l in range(8)]))

    # Play one bar, watching the strip: where each box starts to light, by the step playing then.
    d.tap('PLAY', hold_ms=30, after_ms=10)
    was = [False] * 8
    onsets = {l: [] for l in range(8)}
    dark_between = True
    samples = 0
    for _ in range(int(2050 / 6)):
        d.wait(6)
        s, _ = fired()
        playing = (s - 1) % 16
        now = [lit(l) for l in range(8)]
        for l in range(8):
            if now[l] and not was[l]:
                onsets[l].append(playing)
        was = now
        samples += 1
        if playing in (4, 9) and now[0]:        # well between the kick's steps 2 and 12
            dark_between = False
    d.tap('STOP', after_ms=300)
    print('onsets by lane:', {NAMES[l]: v for l, v in onsets.items() if v})

    def near(got, want):
        return len(got) == len(want) and all(abs(g - w) <= 1 for g, w in zip(got, want))

    check('the kick blinks as steps 2 and 12 play', near(onsets[0], [2, 12]), str(onsets[0]))
    check('the snare blinks as step 6 plays', near(onsets[1], [6]), str(onsets[1]))
    check('the muted open hat does not blink', not onsets[3], str(onsets[3]))
    check('lanes never played stay dark', not any(onsets[l] for l in (2, 4, 5, 6, 7)),
          str({NAMES[l]: onsets[l] for l in (2, 4, 5, 6, 7)}))
    check('not one step early (the fire, not the play)', all(o != 1 and o != 11 for o in onsets[0]), str(onsets[0]))
    check('the kick\'s box goes dark between its hits', dark_between)
    check('the strip was sampled through the bar', samples > 200, str(samples))
    # Not over the menus: in SETTINGS the idle strip's pattern (8 box tops lit, centres dark) is not drawn.
    d.tap('GLOBAL', after_ms=500)
    tops = [d.pixel(96 + 4 * l + 1, 2) for l in range(8)]
    centres = [d.pixel(96 + 4 * l + 1, 4) for l in range(8)]
    d.screen_png(os.path.join(work, 'settings-menu.png'))
    check('the strip is not drawn over the settings menu', not (all(tops) and not any(centres)), 'tops %s centres %s' % (tops, centres))
    for _ in range(4):
        d.tap('NO', after_ms=300)
    d.tap('TRIG', after_ms=400)
    tops = [d.pixel(96 + 4 * l + 1, 2) for l in range(8)]
    check('and is back on the TRIG page (the muted open hat a dash)', tops == [1, 1, 1, 0, 1, 1, 1, 1], str(tops))
    check('the firmware is still running', not d.halted, str(d.halted))

    failed = [n for n, ok in results if not ok]
    print('\n%d passed, %d failed' % (len(results) - len(failed), len(failed)))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
