# SPDX-License-Identifier: GPL-2.0-or-later
"""Drum Mode 0.8.2 in digiemu: live recording on a pattern other than A01.

    uv run --project $DIGIEMU python drum-mode/tests/test_other_pattern.py out/Digitone_OS1.44_DrumMode.syx [workdir]

The project keeps a set of track edit objects per pattern, and the active pattern's set is the one to edit through.
0.8 always wrote through A01's set and 0.8.1 through A01's or A02's: on another pattern the recorded beats went into
a pattern that wasn't playing (the bug found on the unit, on H02). This switches to H02 (BANK + trig 16, then trig 2)
before turning Drum Mode on, live records four kicks, and checks they are in H02, not in A01 or A02, and that
playback sends them on the DIN wire. Then the DRUM pages must still be drawn there (0.8.3).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu_session import Digitone          # noqa: E402

TRACK = 4                                 # MIDI track A
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
    a01 = S['fw_patterns'] + 976 * TRACK

    def trig_steps(data):
        return [s + 1 for s in range(16) if int.from_bytes(data[2 * s:2 * s + 2], 'big') & 1]

    d.wait(1500)
    d.tap('BANK', after_ms=300)
    d.tap('16', after_ms=600)
    d.tap('2', after_ms=2500)
    pat = d.peek32(S['fw_pattern'])
    check('pattern H02 is active', pat == S['fw_patterns'] + 113 * 0x1611d, '%08x' % pat)
    h02 = pat + 976 * TRACK
    a02 = S['fw_patterns'] + 0x1611d + 976 * TRACK
    a01_before = d.mem(a01, 448 + 8 * 16)
    a02_before = d.mem(a02, 448 + 8 * 16)

    d.setup_midi_track_a()
    d.wait(800)
    d.mod_menu_first()
    d.wait(2600)
    d.tap('STOP', after_ms=500)

    d.take_midi()
    d.press('RECORD')
    d.tap('PLAY', hold_ms=50, after_ms=50)
    d.release('RECORD', after_ms=300)
    for _ in range(4):
        d.tap('1', hold_ms=80, after_ms=420)
    d.tap('STOP', after_ms=500)
    played = [m[1] for m in d.take_midi() if (m[0] & 0xF0) == 0x90 and m[2] > 0]
    kick = played[0] if played else -1
    check('the pad plays its note while recording', len(played) >= 4 and kick >= 0 and all(n == kick for n in played),
          str(played))

    rec = d.mem(h02, 448 + 8 * 16)
    steps = trig_steps(rec)
    check('four kicks are recorded in H02', len(steps) == 4, str(steps))
    notes = [rec[448 + 8 * (s - 1)] for s in steps]
    check('each with the pad\'s note', notes and all(n == kick for n in notes), str(notes))
    check('A01 and A02 are untouched', d.mem(a01, 448 + 8 * 16) == a01_before and d.mem(a02, 448 + 8 * 16) == a02_before)

    d.take_midi()
    d.din_sent.clear()
    mo = [x for x in d.s.sources if type(x).__name__ == 'MidiOut'][0]
    wire = []
    mo.sink = lambda x: wire.append(bytes(x) if not isinstance(x, int) else bytes((x,)))
    d.tap('PLAY', hold_ms=50, after_ms=2600)
    d.tap('STOP', after_ms=400)
    data = b''.join(wire)
    ons = [i for i in range(len(data) - 2) if data[i] & 0xF0 == 0x90 and data[i + 1] == kick and data[i + 2] > 0]
    check('playback sends the kicks on the DIN wire', len(ons) >= 4, data.hex(' ')[:200])   # a loop may wrap: 4 or more

    # 0.8.3: picking a pattern with BANK/PTN leaves an empty firmware view on top of the page; Drum Mode drew
    # nothing under it (the DRUM pages came up empty on the unit, on H02).
    d.tap('LFO', after_ms=500)
    d.tap('LFO', after_ms=500)
    d.screen_png(os.path.join(work, 'h02-drum-notes.png'))
    cells = sum(d.pixel(x, y) for x in range(21, 22) for y in range(13, 36))
    check('on H02, LFO twice shows Drum Notes with its cells drawn', cells > 15, str(cells))

    failed = [n for n, ok in results if not ok]
    print('\n%d of %d passed' % (len(results) - len(failed), len(results)))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
