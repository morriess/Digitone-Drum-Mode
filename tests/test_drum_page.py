# SPDX-License-Identifier: GPL-2.0-or-later
"""Drum Mode 0.8 in digiemu: the DRUM pages behind LFO on a MIDI track.

    uv run --project $DIGIEMU python tests/test_drum_page.py out/Digitone_OS1.44_DrumMode.syx [workdir]

Steps LFO to Drum Notes and Drum Velocity, turns knobs there and checks the lanes' notes and velocities change (a new
note plays at once), the pads then play them, CHAN still turns on (FUNC + push A on MIDI Source), the LFO page underneath is untouched, the cells show the values,
and a MIDI track without Drum Mode shows a hint and ignores the knobs. Screenshots go to the work folder.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu_session import Digitone          # noqa: E402

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

    def note(t, l):
        return d.mem(S['dm'] + 8 + 8 * t + l, 1)[0]

    def vel(t, l):
        return d.mem(S['dm'] + 40 + 8 * t + l, 1)[0]

    def chan_a():
        return d.mem(0x419c5b30 + 38, 1)[0]

    def ons():
        return [(m[1], m[2]) for m in d.take_midi() if (m[0] & 0xF0) == 0x90 and m[2] > 0]

    def lit(x0, x1, y0, y1):
        return sum(d.pixel(x, y) for x in range(x0, x1) for y in range(y0, y1))

    d.setup_midi_track_a()
    d.mod_menu_first()
    d.wait(2500)
    chan = chan_a()

    check('CHAN turns on with the pages in (FUNC + push A on MIDI Source)', d.peek32(0x419c5b10 + 4 * 186) & 1)
    d.tap('LFO', after_ms=500)
    d.screen_png(os.path.join(work, 'page-lfo.png'))
    lfo_spd = d.mem(0x419c5b30, 158)
    d.tap('LFO', after_ms=500)
    d.screen_png(os.path.join(work, 'page-notes.png'))
    check('LFO twice shows Drum Notes: the cells are drawn', lit(21, 22, 13, 36) > 15, str(lit(21, 22, 13, 36)))
    d.take_midi()

    d.turn('A', 2)
    check('knob A steps the kick 36 -> 38', note(0, 0) == 38, str(note(0, 0)))
    played = ons()
    check('each new note plays at once (37, 38)', played == [(37, 100), (38, 100)], str(played))
    d.turn('H', -3)
    check('knob H steps the crash 49 -> 46', note(0, 7) == 46, str(note(0, 7)))
    d.turn('E', 1)
    check('knob E steps the clap 39 -> 40', note(0, 4) == 40, str(note(0, 4)))
    d.wait(300)
    d.screen_png(os.path.join(work, 'page-notes-edited.png'))

    d.tap('LFO', after_ms=500)
    d.screen_png(os.path.join(work, 'page-velocity.png'))
    d.take_midi()
    d.turn('B', -20, after_ms=30)               # one continuous turn: the stock's dead zone swallows the first
                                                # detents after a pause, which timing shifts can cause (0.10.1)
    check('on Drum Velocity knob B sets the snare to 80', vel(0, 1) == 80, str(vel(0, 1)))
    check('a velocity turn plays nothing', ons() == [])
    d.turn('A', 30, after_ms=30)
    check('velocity stops at 127', vel(0, 0) == 127, str(vel(0, 0)))

    d.tap('TRIG', after_ms=400)
    d.tap('1', after_ms=200)
    d.tap('2', after_ms=200)
    played = ons()
    check('the pads play the new notes and velocities', played == [(38, 127), (38, 80)], str(played))

    check('the knobs left MIDI A\'s settings (CHAN, LFO, ...) alone', d.mem(0x419c5b30, 158) == lfo_spd and chan_a() == chan)
    d.tap('LFO', after_ms=500)
    d.screen_png(os.path.join(work, 'page-lfo-again.png'))

    # A MIDI track without Drum Mode: the pages are there, with a hint, and the knobs do nothing.
    d.tap('T2', after_ms=400)                       # in MIDI mode a track key picks a MIDI track
    check('MIDI B is the active track', d.active_track() == 5, str(d.active_track()))
    d.tap('LFO', after_ms=500)
    d.tap('LFO', after_ms=500)
    d.screen_png(os.path.join(work, 'page-off.png'))
    before = [note(1, l) for l in range(8)]
    d.turn('A', 2)
    check('without Drum Mode the knobs change nothing', [note(1, l) for l in range(8)] == before)
    check('and the page shows the hint, not cells', lit(21, 22, 13, 36) == 0 and lit(28, 110, 20, 40) > 20,
          '%d %d' % (lit(21, 22, 13, 36), lit(28, 110, 20, 40)))
    check('the firmware is still running', not d.halted, str(d.halted))

    failed = [n for n, ok in results if not ok]
    print('\n%d passed, %d failed' % (len(results) - len(failed), len(failed)))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
