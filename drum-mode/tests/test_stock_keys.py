# SPDX-License-Identifier: GPL-2.0-or-later
"""Drum Mode 0.9.3 in digiemu: the stock's own trig key functions on a Drum Mode track.

    uv run --project $DIGIEMU python drum-mode/tests/test_stock_keys.py out/Digitone_OS1.44_DrumMode.syx [workdir]

Pattern select (PTN then a trig key, or PTN held + a trig key), bank select (BANK, a trig key 9-16, then a pattern)
and chains (PTN held + a trig key, then more trig keys while one is held) go to the stock, not to the pads. In GRID
RECORDING a step is the stock's held trig too: a quick tap still adds or takes out the selected lane, a long hold
keeps it, and a held step takes the stock's parameter locks, micro timing and trig copy/paste. FUNC + a trig key in
any recording mode is the stock's (a lock trig in GRID RECORDING), not a lane mute.
"""
import os
import struct
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

    def pattern():
        return (d.peek32(S['fw_pattern']) - S['fw_patterns']) // 0x1611d

    def name(p):
        return '%s%02d' % ('ABCDEFGH'[p // 16], p % 16 + 1)

    def notes(step, track=4):
        trk = d.peek32(S['fw_pattern']) + 976 * track
        if not int.from_bytes(d.mem(trk + 2 * step, 2), 'big') & 1:
            return []
        sl = list(struct.unpack('8b', d.mem(trk + 448 + 8 * step, 8)))
        return sorted([sl[0] & 0xff] + [(sl[0] & 0xff) + iv for iv in sl[1:] if iv])

    def ons():
        return [m[1] for m in d.take_midi() if (m[0] & 0xF0) == 0x90 and m[2] > 0]

    d.setup_midi_track_a()
    d.mod_menu_first()
    d.wait(1500)
    d.take_midi()

    # Pattern select: PTN, then a trig key.
    d.tap('PTN', after_ms=300)
    d.tap('2', after_ms=800)
    check('PTN then trig 2 picks A02', pattern() == 1, name(pattern()))
    check('and plays no pad', ons() == [])

    # 0.10.1: while PTN or BANK is held, the trig keys show the stock's LEDs (the active pattern red), not the pads.
    d.wait(300)
    free = d.leds()
    d.press('PTN', after_ms=400)
    held = d.leds()
    pal = d.palette()
    d.press('NO', after_ms=100)
    d.release('NO', after_ms=100)
    d.release('PTN', after_ms=600)
    mid = d.leds()
    d.wait(4500)                                # the select's four seconds, if NO didn't close it
    back = d.leds()
    r, g, b = pal.get(held[1], (0, 0, 0))[:3] if isinstance(pal.get(held[1]), (tuple, list)) else (0, 0, 0)
    check('PTN held: key 2 (A02, active) shows the stock pattern LED', held[1] != free[1],
          'held %s free %s rgb %s' % (held[1], free[1], pal.get(held[1])))
    check('PTN released: the pad LEDs come back', back == free, '%s / %s' % (back, free))
    d.press('BANK', after_ms=400)
    bheld = d.leds()
    d.press('NO', after_ms=100)
    d.release('NO', after_ms=100)
    d.release('BANK', after_ms=600)
    check('BANK held: the trig keys show the stock LEDs', bheld != free, '%s / %s' % (bheld, free))
    d.take_midi()

    # PTN held + a trig key.
    d.press('PTN', after_ms=200)
    d.tap('3', after_ms=300)
    d.release('PTN', after_ms=800)
    check('PTN held + trig 3 picks A03', pattern() == 2, name(pattern()))
    check('and plays no pad', ons() == [])

    # Bank select: BANK, trig 16 (bank H), then trig 2.
    d.tap('BANK', after_ms=300)
    d.tap('16', after_ms=500)
    d.tap('2', after_ms=1500)
    check('BANK, trig 16, trig 2 picks H02', pattern() == 113, name(pattern()))
    check('and plays no pad', ons() == [])

    # A chain: PTN held + trig 1, then trig 2 and 3 while trig 1 is held.
    d.press('PTN', after_ms=200)
    d.press('1', after_ms=200)
    d.release('PTN', after_ms=200)
    d.tap('2', after_ms=200)
    d.tap('3', after_ms=200)
    d.release('1', after_ms=800)
    check('a chain picks H01 first', pattern() == 112, name(pattern()))
    check('and its trig keys play no pads', ons() == [])
    d.wait(4500)                                # any select's four seconds
    d.tap('1', hold_ms=80, after_ms=300)
    check('afterwards pad 1 plays the kick again', ons() == [36])

    # GRID RECORDING: quick taps add and take out the selected lane (the kick).
    d.tap('RECORD', after_ms=400)
    d.tap('5', after_ms=200)
    check('tap: the kick goes on step 5', notes(4) == [36], str(notes(4)))
    d.tap('5', after_ms=200)
    check('tap again: it comes off, and the trig with it', notes(4) == [], str(notes(4)))
    d.press('5', after_ms=1000)
    d.release('5', after_ms=300)
    check('pressed and held: the kick stays', notes(4) == [36], str(notes(4)))
    d.press('5', after_ms=1000)
    d.release('5', after_ms=300)
    check('an existing kick held long: it stays', notes(4) == [36], str(notes(4)))

    # A held step takes the stock's locks and micro timing (track data: velocity locks at +128 + step, micro timing
    # at +256 + step; knob B on the MIDI TRIG page is VEL; the first RIGHT opens the MICRO TIMING menu).
    def trk_byte(off):
        return d.mem(d.peek32(S['fw_pattern']) + 976 * 4 + off, 1)[0]
    d.press('5', after_ms=150)
    d.turn('B', 8)
    d.wait(150)
    d.release('5', after_ms=300)
    check('held + knob B: a velocity lock on step 5', trk_byte(128 + 4) != 0xff, str(trk_byte(128 + 4)))
    check('and the step stays', notes(4) == [36], str(notes(4)))
    d.press('5', after_ms=150)
    for _ in range(3):
        d.tap('RIGHT', after_ms=200)
    d.release('5', after_ms=300)
    check('held + RIGHT: micro timing on step 5', trk_byte(256 + 4) != 0, str(trk_byte(256 + 4)))
    check('and the step stays', notes(4) == [36], str(notes(4)))

    # Trig copy and paste: copy step 5 (the kick), select the hat, paste over step 9.
    d.press('5', after_ms=150)
    d.tap('RECORD', after_ms=300)
    d.release('5', after_ms=300)
    check('hold step 5 + RECORD (copy): GRID RECORDING still on', d.mem(d.peek32(0x419f0dd8) + 0x168, 1)[0] == 1)
    d.press('TRIG', after_ms=60)
    d.tap('3', after_ms=200)
    d.release('TRIG', after_ms=300)
    d.press('9', after_ms=150)
    d.tap('STOP', after_ms=300)
    d.release('9', after_ms=300)
    check('hold step 9 + STOP (paste): step 9 plays the copied kick, not the hat', notes(8) == [36], str(notes(8)))
    check('with step 5\'s velocity lock', trk_byte(128 + 8) == trk_byte(128 + 4), '%d %d' % (trk_byte(128 + 8), trk_byte(128 + 4)))

    # 0.9.3: FUNC + a trig key in any recording mode is the stock's, not a lane mute. In GRID RECORDING the stock
    # places a lock trig (bit 1 of the step word) on an empty step.
    def mutes():
        return d.mem(S['dm'] + 2, 1)[0]         # dm.mutes[0], track A
    def word(step, track=4):
        return int.from_bytes(d.mem(d.peek32(S['fw_pattern']) + 976 * track + 2 * step, 2), 'big')
    d.press('FUNC', after_ms=60)
    d.tap('3', after_ms=200)
    d.release('FUNC', after_ms=300)
    check('GRID RECORDING: FUNC + trig 3 mutes nothing', mutes() == 0, bin(mutes()))
    check('and the stock places a lock trig on step 3', word(2) & 2 and not word(2) & 1, hex(word(2)))
    d.tap('RECORD', after_ms=400)
    d.press('RECORD')
    d.tap('PLAY', hold_ms=50, after_ms=50)
    d.release('RECORD', after_ms=300)
    d.press('FUNC', after_ms=60)
    d.tap('4', after_ms=200)
    d.release('FUNC', after_ms=300)
    check('LIVE RECORDING: FUNC + trig 4 mutes nothing', mutes() == 0, bin(mutes()))
    d.tap('STOP', after_ms=500)
    d.press('FUNC', after_ms=60)
    d.tap('4', after_ms=200)
    d.release('FUNC', after_ms=300)
    check('not recording: FUNC + trig 4 still mutes lane 4', mutes() == 8, bin(mutes()))
    check('the firmware is still running', not d.halted, str(d.halted))

    failed = [n for n, ok in results if not ok]
    print('\n%d of %d passed' % (len(results) - len(failed), len(results)))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
