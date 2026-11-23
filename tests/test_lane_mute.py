# SPDX-License-Identifier: GPL-2.0-or-later
"""Drum Mode 0.5 in digiemu: muting lanes.

    uv run --project $DIGIEMU python tests/test_lane_mute.py out/Digitone_OS1.44_DrumMode.syx [workdir]

Records a kick, a snare and a kick + open hat chord, then mutes lanes with FUNC + pad and checks playback leaves out
exactly the muted lanes' notes (a chord keeps its other notes, a step of muted lanes plays nothing), that the pads
still play, that the corner shows a muted lane as a dash, and that unmuting brings everything back. Audio on: the
Digitone's sequencer is clocked by it.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu_session import Digitone          # noqa: E402

STEP = 5400000
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

    def wait_for(step_playing, phase):
        for _ in range(4000):
            s, _, t, _ = struct.unpack('>iiII', d.mem(S['dm_fire'], 16))
            if s == (step_playing + 1) % 16 and (timeline() - t) >= phase * STEP:
                return True
            d.wait(2)
        return False

    def mutes():
        return d.mem(S['dm'] + 2, 1)[0]

    def func_pad(pad):
        d.press('FUNC', after_ms=40)
        d.tap(pad, hold_ms=60, after_ms=60)
        d.release('FUNC', after_ms=150)

    def play_bar():
        d.take_midi()
        d.tap('PLAY', hold_ms=50, after_ms=50)
        d.wait(2300)
        d.tap('STOP', after_ms=300)
        return [m[1] for m in d.take_midi() if (m[0] & 0xF0) == 0x90 and m[2] > 0]

    d.setup_midi_track_a()
    d.mod_menu_first()
    d.wait(1200)

    # Record: kick on step 2, snare on 6, kick + open hat on 12.
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
    base = play_bar()
    check('the recorded bar plays kick, snare, kick + open hat', sorted(base) == [36, 36, 38, 46], str(base))

    # Mute the kick: FUNC + pad 1. No note goes out for it.
    d.take_midi()
    func_pad('1')
    sent = [m for m in d.take_midi() if (m[0] & 0xF0) == 0x90 and m[2] > 0]
    check('FUNC + pad 1 mutes the kick', mutes() == 0x01, 'mutes=%#x' % mutes())
    check('and plays nothing', not sent, str(sent))
    d.wait(2600)                              # the BD MUTE message times out
    d.screen_png(os.path.join(work, 'kick-muted.png'))
    x0 = 96                                   # lane 1's box in the corner strip
    check('the corner shows the muted kick as a dash',
          d.pixel(x0 + 1, 4) == 1 and d.pixel(x0 + 1, 2) == 0 and d.pixel(x0 + 4 + 1, 2) == 1,
          'centre %d top %d, snare top %d' % (d.pixel(x0 + 1, 4), d.pixel(x0 + 1, 2), d.pixel(x0 + 5, 2)))

    played = play_bar()
    check("with the kick muted: snare and the chord's open hat only", sorted(played) == [38, 46], str(played))

    # Mute the open hat too: step 12 plays nothing.
    func_pad('4')
    played = play_bar()
    check('kick and open hat muted: only the snare', played == [38], str(played))

    # The pads still play muted lanes.
    d.take_midi()
    d.tap('1')
    check("a muted lane's pad still plays", [m[1] for m in d.take_midi() if (m[0] & 0xF0) == 0x90 and m[2] > 0] == [36])

    # Unmute both: everything back.
    func_pad('1')
    func_pad('4')
    check('FUNC + the pads again unmute them', mutes() == 0, 'mutes=%#x' % mutes())
    played = play_bar()
    check('unmuted: the whole bar again', sorted(played) == [36, 36, 38, 46], str(played))

    check('the firmware is still running', not d.halted, str(d.halted))
    failed = [n for n, ok in results if not ok]
    print('\n%d passed, %d failed' % (len(results) - len(failed), len(failed)))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
