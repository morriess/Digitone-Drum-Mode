# SPDX-License-Identifier: GPL-2.0-or-later
"""Drum Mode 0.7 in digiemu: lane select, grid editing in GRID RECORDING, and the trig-key LEDs.

    uv run --project $DIGIEMU python drum-mode/tests/test_grid_edit.py out/Digitone_OS1.44_DrumMode.syx [workdir]

Selects a lane with TRIG + pad, places and removes it on steps in GRID RECORDING (several lanes on one step make a
chord, the last one out takes the trig), checks the LEDs show only the selected lane, edits page 2 of a 32-step track,
and the pads' LEDs outside GRID RECORDING (held, muted).
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

    def pattern(track=4):
        pat = d.peek32(S['fw_pattern'])
        trk = pat + 976 * track
        words = [int.from_bytes(d.mem(trk + 2 * s, 2), 'big') for s in range(64)]
        slots = [list(struct.unpack('8b', d.mem(trk + 448 + 8 * s, 8))) for s in range(64)]
        return words, slots

    def notes(step):
        words, slots = pattern()
        if not words[step] & 1:
            return []
        sl = slots[step]
        return sorted([sl[0]] + [sl[0] + iv for iv in sl[1:] if iv])

    def selected():
        return d.mem(S['dm'] + 6, 1)[0] & 7     # dm.selected[0], track A's nibble

    def select(pad):
        d.press('TRIG', after_ms=60)
        d.tap(pad, after_ms=200)
        d.release('TRIG', after_ms=300)

    def leds():
        d.wait(200)
        return d.leds()

    d.setup_midi_track_a()
    d.mod_menu_first()
    d.wait(1200)
    d.take_midi()

    # Lane select: TRIG + pad 3 picks the closed hat, silently.
    select('3')
    check('TRIG + pad 3 selects lane 3 (CH)', selected() == 2, str(selected()))
    sent = [m for m in d.take_midi() if (m[0] & 0xF0) == 0x90 and m[2] > 0]
    check('selecting a lane plays nothing', not sent, str(sent))
    d.wait(2600)                                # the EDIT message goes
    check('the corner underlines lane 3', d.pixel(96 + 4 * 2 + 1, 9) == 1 and d.pixel(97, 9) == 0,
          '%d %d' % (d.pixel(105, 9), d.pixel(97, 9)))

    # GRID RECORDING: the trig keys place the selected lane.
    d.tap('RECORD', after_ms=400)
    rec = d.peek32(0x419f0dd8)
    check('GRID RECORDING is on', d.mem(rec + 0x168, 1)[0] == 1)
    for key in ('1', '5', '9', '13'):
        d.tap(key, after_ms=150)
    words, _ = pattern()
    trigs = [s for s in range(16) if words[s] & 1]
    check('trig keys 1, 5, 9, 13 make trigs on steps 0, 4, 8, 12', trigs == [0, 4, 8, 12], str(trigs))
    check('each plays the closed hat (42)', all(notes(s) == [42] for s in (0, 4, 8, 12)),
          str([notes(s) for s in (0, 4, 8, 12)]))
    sent = [m for m in d.take_midi() if (m[0] & 0xF0) == 0x90 and m[2] > 0]
    check('placing steps plays nothing', not sent, str(sent))
    l = leds()
    check('LEDs: the hat\'s steps red, the rest dark',
          all(l[k] == (RED if k in (0, 4, 8, 12) else DARK) for k in range(16)), str(l))

    # A second lane on step 1 makes a chord; the LEDs follow the selection.
    select('1')
    check('TRIG + pad 1 selects the kick', selected() == 0, str(selected()))
    l = leds()
    check('LEDs: the kick has no steps yet, all dark', all(l[k] == DARK for k in range(16)), str(l))
    d.tap('1', after_ms=150)
    d.tap('3', after_ms=150)
    check('step 1 plays kick and hat', notes(0) == [36, 42], str(notes(0)))
    check('step 3 plays the kick', notes(2) == [36], str(notes(2)))
    l = leds()
    check('LEDs: the kick\'s steps 1 and 3 red', all(l[k] == (RED if k in (0, 2) else DARK) for k in range(16)),
          str(l))

    # Taking lanes out: the kick from step 1 leaves the hat; then the hat takes the trig with it.
    d.tap('1', after_ms=150)
    check('kick off step 1 leaves the hat', notes(0) == [42], str(notes(0)))
    select('3')
    d.tap('1', after_ms=150)
    words, _ = pattern()
    check('the hat off step 1 clears its trig', not (words[0] & 1), hex(words[0]))
    check('Drum Mode counted 8 grid edits', d.peek32(S['dm_grid_edits']) == 8, str(d.peek32(S['dm_grid_edits'])))

    # Page 2 of a 32-step track: the keys edit steps 17-32.
    d.press('FUNC')
    d.tap('PAGE', after_ms=500)
    d.release('FUNC', after_ms=400)
    d.turn('E', 16)
    d.tap('NO', after_ms=500)
    d.tap('PAGE', after_ms=400)
    check('the page shown is 2', d.peek32(0x41368ce8) == 1, str(d.peek32(0x41368ce8)))
    d.tap('2', after_ms=150)
    check('trig key 2 on page 2 places step 18', notes(17) == [42] and notes(1) == [], '%s %s' % (notes(17), notes(1)))
    l = leds()
    check('LEDs on page 2: key 2 red', l[1] == RED and l[4] == DARK, str(l))
    d.tap('PAGE', after_ms=400)
    l = leds()
    check('back on page 1: the hat on keys 5, 9, 13', all(l[k] == (RED if k in (4, 8, 12) else DARK) for k in range(16)),
          str(l))

    # Outside GRID RECORDING: the pads' LEDs.
    d.tap('RECORD', after_ms=400)
    check('GRID RECORDING is off', d.mem(rec + 0x168, 1)[0] == 0)
    l = leds()
    check('pads dark when idle, the selected lane (3) dim white (0.9)',
          all(l[k] == (DIM_WHITE if k == 2 else DARK) for k in range(16)), str(l))
    d.press('1', after_ms=250)
    l = d.leds()
    check('a held pad lights white', l[0] == WHITE, str(l))
    d.release('1', after_ms=300)
    check('and plays its note', any(m[:2] == (0x90, 36) for m in d.take_midi()))
    d.press('FUNC')
    d.tap('3', after_ms=200)
    d.release('FUNC', after_ms=300)
    l = leds()
    check('a muted lane\'s pad is dim red; pad 1, played, is now the selected lane (0.9)',
          l[2] == DIM_RED and l[0] == DIM_WHITE, str(l))
    check('the firmware is still running', not d.halted, str(d.halted))

    failed = [n for n, ok in results if not ok]
    print('\n%d passed, %d failed' % (len(results) - len(failed), len(failed)))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
