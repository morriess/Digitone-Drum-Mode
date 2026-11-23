# SPDX-License-Identifier: GPL-2.0-or-later
"""Drum Mode 0.9 in digiemu: a pad selects its lane as it plays, and its key stays lit.

    uv run --project $DIGIEMU python tests/test_lane_select.py out/Digitone_OS1.44_DrumMode.syx [workdir]

Pressing pad 1-8 (stopped, not recording) plays its drum and makes it the selected lane: its key stays lit dim white
(red while muted). GRID RECORDING then places and shows that lane's steps only. During live recording a pad only plays
and records, it doesn't change the selection.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu_session import Digitone          # noqa: E402

RED, DARK, DIM_RED, WHITE, DIM_WHITE = 3, 2, 5, 20, 21
results = []


def check(name, ok, detail=''):
    results.append((name, ok))
    print('%s  %s%s' % ('PASS' if ok else 'FAIL', name, ('  (%s)' % detail) if detail else ''))


def main():
    syx = os.path.abspath(sys.argv[1])
    work = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else 'emu-work')
    os.makedirs(work, exist_ok=True)
    d = Digitone(syx, os.path.join(work, 'home'))
    S = d.symbols

    def notes(step, track=4):
        trk = d.peek32(S['fw_pattern']) + 976 * track
        if not int.from_bytes(d.mem(trk + 2 * step, 2), 'big') & 1:
            return []
        sl = list(struct.unpack('8b', d.mem(trk + 448 + 8 * step, 8)))
        return sorted([sl[0] & 0xff] + [(sl[0] & 0xff) + iv for iv in sl[1:] if iv])

    def selected():
        return d.mem(S['dm_store'] + 6, 1)[0] & 7   # dm_store.selected[0], track A's nibble (0.10)

    def ons():
        return [m[1] for m in d.take_midi() if (m[0] & 0xF0) == 0x90 and m[2] > 0]

    def leds():
        d.wait(300)                             # past a hit's flash
        return d.leds()

    d.setup_midi_track_a()
    d.mod_menu_first()
    d.wait(1200)
    d.take_midi()
    check('the kick is selected to begin with', selected() == 0, str(selected()))

    # A pad selects its lane and still plays.
    d.tap('3', hold_ms=80, after_ms=200)
    check('pad 3 plays the closed hat (42)', ons() == [42])
    check('and selects lane 3', selected() == 2, str(selected()))
    l = leds()
    check('LEDs: pad 3 lit dim white, the others dark',
          all(l[k] == (DIM_WHITE if k == 2 else DARK) for k in range(16)), str(l))

    # Muted and selected: red. FUNC + pad mutes without changing the selection.
    d.press('FUNC', after_ms=60)
    d.tap('3', after_ms=200)
    d.release('FUNC', after_ms=300)
    l = leds()
    check('muted and selected: pad 3 red', l[2] == RED, str(l))
    d.press('FUNC', after_ms=60)
    d.tap('3', after_ms=200)
    d.release('FUNC', after_ms=300)
    d.press('FUNC', after_ms=60)
    d.tap('5', after_ms=200)
    d.release('FUNC', after_ms=300)
    l = leds()
    check('muted, not selected: pad 5 dim red; pad 3 dim white again', l[4] == DIM_RED and l[2] == DIM_WHITE, str(l))
    check('FUNC + pad leaves the selection alone', selected() == 2, str(selected()))
    d.press('FUNC', after_ms=60)
    d.tap('5', after_ms=200)
    d.release('FUNC', after_ms=300)
    d.wait(2600)                                # the messages go

    # GRID RECORDING places and shows the selected lane only.
    d.tap('RECORD', after_ms=400)
    d.take_midi()
    d.tap('5', after_ms=150)
    d.tap('9', after_ms=150)
    check('GRID RECORDING: keys 5 and 9 place the hat', notes(4) == [42] and notes(8) == [42],
          '%s %s' % (notes(4), notes(8)))
    l = leds()
    check('LEDs show the hat\'s steps only', all(l[k] == (RED if k in (4, 8) else DARK) for k in range(16)), str(l))
    d.tap('RECORD', after_ms=400)

    # Another pad selects the kick: GRID RECORDING now shows its (empty) steps.
    d.tap('1', hold_ms=80, after_ms=200)
    check('pad 1 plays the kick and selects it', ons() == [36] and selected() == 0, str(selected()))
    d.tap('RECORD', after_ms=400)
    l = leds()
    check('GRID RECORDING shows the kick\'s steps: none', all(l[k] == DARK for k in range(16)), str(l))
    d.tap('RECORD', after_ms=400)

    # Live recording: a pad plays and records, the selection stays.
    d.take_midi()
    d.press('RECORD')
    d.tap('PLAY', hold_ms=50, after_ms=50)
    d.release('RECORD', after_ms=300)
    d.tap('4', hold_ms=80, after_ms=400)
    d.tap('STOP', after_ms=500)
    played = ons()
    check('live recording: pad 4 plays the open hat (46)', 46 in played, str(played))
    check('and leaves the kick selected', selected() == 0, str(selected()))
    rec = [s for s in range(16) if 46 in notes(s)]
    check('the open hat is recorded', len(rec) == 1, str(rec))
    check('the firmware is still running', not d.halted, str(d.halted))

    failed = [n for n, ok in results if not ok]
    print('\n%d of %d passed' % (len(results) - len(failed), len(results)))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
