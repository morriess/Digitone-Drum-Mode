# SPDX-License-Identifier: GPL-2.0-or-later
"""Drum Mode 0.11 in digiemu: ratchets (ROLL, knob E of the TRIG page) and note repeat (RPT, knob G).

    uv run --project $DIGIEMU python tests/test_roll.py out/Digitone_OS1.44_DrumMode.syx [workdir]

In GRID RECORDING the closed hat goes on steps 1 and 9; step 1 held, knob E turned up gives it roll 4 (its LED white,
the box x4). Playing, step 1 sends the hat four times, a quarter of a step apart, from the first bar on; step 9 once.
Then knob G sets the repeat rate: a pad held while playing repeats on the grid and stops when let go; held in LIVE
RECORDING, its steps are recorded with their roll and play back the same.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu_session import Digitone          # noqa: E402

L16 = 5400000
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

    def peek(n):
        v = d.peek32(S[n])
        return v - (1 << 32) if v >= 1 << 31 else v

    def tl():
        return d.peek32(S['fw_timeline'])

    def hits(ms, note, tl0):
        log, t0 = [], d.s.now_ms()
        while d.s.now_ms() - t0 < ms:
            d.wait(2)
            for m in d.take_midi():
                if (m[0] & 0xF0) == 0x90 and m[2] > 0 and m[1] == note:
                    log.append(round((tl() - tl0) / L16, 2))
        return log

    d.setup_midi_track_a()
    d.mod_menu_first()
    d.wait(1500)
    d.tap('3', hold_ms=80, after_ms=400)        # the closed hat (42) selected
    d.tap('RECORD', after_ms=400)
    d.tap('1', hold_ms=60, after_ms=300)
    d.tap('9', hold_ms=60, after_ms=300)
    d.press('1', after_ms=400)
    d.turn('E', 12, after_ms=30)
    d.wait(300)
    d.screen_png(os.path.join(work, 'trig-roll.png'))
    leds = d.leds()
    check('knob E on held step 1: roll edits made', peek('dm_roll_edits') > 0, str(peek('dm_roll_edits')))
    check('step 1 white (a roll), step 9 red (plain)', leds.get(0) == 20 and leds.get(8) == 3,
          str([leds.get(k) for k in range(16)]))
    d.release('1', after_ms=400)
    d.tap('RECORD', after_ms=400)

    d.take_midi()
    d.tap('PLAY', hold_ms=30, after_ms=0)
    tl0 = tl()
    got = hits(4300, 42, tl0)
    d.tap('STOP', after_ms=300)
    d.tap('STOP', after_ms=300)
    print('hat hits (16ths):', got)
    bar1 = [x for x in got if x < 1.0]
    bar2 = [x for x in got if 15.5 < x < 17.0]
    step9 = [x for x in got if 7.5 < x < 8.5]

    def even(xs):
        gaps = [b - a for a, b in zip(xs[1:], xs[2:])]
        return len(xs) == 4 and all(0.2 <= g <= 0.3 for g in gaps)

    check('bar 1, step 1: four hits, evenly a quarter step apart', even(bar1), str(bar1))
    check('bar 2, step 1: four again', even(bar2), str(bar2))
    check('step 9: one hit', len(step9) == 1, str(step9))

    d.turn('G', 4, after_ms=30)                 # repeat on
    d.wait(400)
    d.screen_png(os.path.join(work, 'trig-rpt.png'))
    rate = peek('dm_rpt_hits')
    d.take_midi()
    d.tap('PLAY', hold_ms=30, after_ms=500)
    tl0 = tl()
    d.press('1', after_ms=0)
    held = hits(1000, 36, tl0)
    d.release('1', after_ms=0)
    after = hits(500, 36, tl0)
    d.tap('STOP', after_ms=300)
    d.tap('STOP', after_ms=300)
    gaps = [round(b - a, 2) for a, b in zip(held[1:], held[2:])]
    print('kick held:', held, 'gaps', gaps)
    check('a held pad repeats, evenly', len(held) >= 4 and gaps and max(gaps) - min(gaps) < 0.12, str(gaps))
    check('and stops when let go', not after, str(after))

    rec0 = peek('dm_rec_count')
    d.press('RECORD')
    d.tap('PLAY', hold_ms=30, after_ms=30)
    d.release('RECORD', after_ms=600)
    d.press('2', after_ms=700)                  # the snare (38), held through a few steps
    d.release('2', after_ms=300)
    d.wait(2500)
    d.tap('STOP', after_ms=300)
    d.tap('STOP', after_ms=300)
    check('held in LIVE RECORDING: its steps are recorded', peek('dm_rec_count') - rec0 >= 3,
          str(peek('dm_rec_count') - rec0))
    d.take_midi()
    d.tap('PLAY', hold_ms=30, after_ms=0)
    tl0 = tl()
    pb = hits(3200, 38, tl0)
    d.tap('STOP', after_ms=300)
    print('snare played back:', pb)
    check('and play back as heard (more than one hit a step)', len(pb) > peek('dm_rec_count') - rec0, str(pb))

    failed = [n for n, ok in results if not ok]
    print('\n%d passed, %d failed' % (len(results) - len(failed), len(failed)))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
