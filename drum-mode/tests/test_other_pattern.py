# SPDX-License-Identifier: GPL-2.0-or-later
"""Drum Mode 0.8.1 in digiemu: live recording on a pattern other than A01.

    uv run --project $DIGIEMU python drum-mode/tests/test_other_pattern.py out/Digitone_OS1.44_DrumMode.syx [workdir]

The project keeps two sets of track edit objects, one for each of the firmware's two pattern buffers, and which set
edits the active pattern swaps with pattern changes. 0.8 always wrote through the first set: right on A01, but on
another pattern the recorded beats went into A01 while the active pattern played nothing (the bug found on the unit).
This switches to A02 before turning Drum Mode on, live records four kicks, and checks they are in A02, not in A01,
and that playback sends them on the DIN wire.
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
    d.press('PTN')
    d.tap('2', after_ms=300)
    d.release('PTN', after_ms=2000)
    pat = d.peek32(S['fw_pattern'])
    check('pattern A02 is active', pat == S['fw_patterns'] + 0x1611d, '%08x' % pat)
    a02 = pat + 976 * TRACK
    a01_before = d.mem(a01, 448 + 8 * 16)

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
    check('the pad plays its note while recording', len(played) == 4 and kick >= 0, str(played))

    rec = d.mem(a02, 448 + 8 * 16)
    steps = trig_steps(rec)
    check('four kicks are recorded in A02', len(steps) == 4, str(steps))
    notes = [rec[448 + 8 * (s - 1)] for s in steps]
    check('each with the pad\'s note', notes and all(n == kick for n in notes), str(notes))
    check('A01 is untouched', d.mem(a01, 448 + 8 * 16) == a01_before)

    d.take_midi()
    d.din_sent.clear()
    mo = [x for x in d.s.sources if type(x).__name__ == 'MidiOut'][0]
    wire = []
    mo.sink = lambda x: wire.append(bytes(x) if not isinstance(x, int) else bytes((x,)))
    d.tap('PLAY', hold_ms=50, after_ms=2600)
    d.tap('STOP', after_ms=400)
    data = b''.join(wire)
    ons = [i for i in range(len(data) - 2) if data[i] & 0xF0 == 0x90 and data[i + 1] == kick and data[i + 2] > 0]
    check('playback sends the kicks on the DIN wire', len(ons) == 4, data.hex(' ')[:200])

    failed = [n for n, ok in results if not ok]
    print('\n%d of %d passed' % (len(results) - len(failed), len(results)))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
