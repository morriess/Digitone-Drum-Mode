# SPDX-License-Identifier: GPL-2.0-or-later
"""Drum Mode 0.9.1 in digiemu: keys 9-16 as a running light while playing.

    uv run --project $DIGIEMU python drum-mode/tests/test_run_light.py out/Digitone_OS1.44_DrumMode.syx [workdir]

Each of keys 9-16 stands for 2 steps of the 16-step page that plays, lit white for the first half of each of its
steps: two blinks a key. A track's page of fewer steps (a short or odd length, or the last page of a longer track)
leaves the keys after its last step dark, and a key with one step blinks once. Dark when stopped. Checked on 16 steps,
13 (odd) and 17-20 (a second page of a few steps), the blinks a 16th apart and key 9's on step 1's note.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu_session import Digitone          # noqa: E402

WHITE = 20
results = []


def check(name, ok, detail=''):
    results.append((name, ok))
    print('%s  %s%s' % ('PASS' if ok else 'FAIL', name, ('  (%s)' % detail) if detail else ''))


def expected(length):
    """The keys (9-16) that blink over one loop of a track of this length."""
    return [9 + (s % 16) // 2 for s in range(length)]


def main():
    syx = os.path.abspath(sys.argv[1])
    work = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else 'emu-work')
    os.makedirs(work, exist_ok=True)
    d = Digitone(syx, os.path.join(work, 'home'), audio=True)

    mo = [x for x in d.s.sources if type(x).__name__ == 'MidiOut'][0]
    wire = []
    mo.sink = lambda x: wire.append((d.s.now_ms(), bytes(x) if not isinstance(x, int) else bytes((x,))))

    def play(ms):
        """Play for ms; -> the blinks (time, key 9-16) and step 1's note-on times."""
        d.leds()
        wire.clear()
        t0 = d.s.now_ms()
        d.tap('PLAY', hold_ms=50, after_ms=20)
        blinks, cur = [], None
        while d.s.now_ms() - t0 < ms:
            d.wait(8)
            l = d.leds()
            lit = [k for k in range(8, 16) if l[k] == WHITE]
            k = lit[0] + 1 if lit else None
            if k != cur and k is not None:
                blinks.append((d.s.now_ms() - t0, k))
            cur = k
        d.tap('STOP', after_ms=400)
        ons, buf = [], b''
        for t, b in wire:
            for x in b:
                buf += bytes((x,))
                if len(buf) >= 3 and buf[-3] & 0xF0 == 0x90 and buf[-1] > 0:
                    ons.append(t - t0)
        return blinks, ons

    def loop_of(blinks, length):
        """One full loop of keys, from the second key-9 start."""
        keys = [k for _, k in blinks]
        starts = [i for i in range(len(keys)) if keys[i] == 9 and (i == 0 or keys[i - 1] != 9)]
        if len(starts) < 2:
            return []
        return keys[starts[1]:starts[1] + length]

    def set_length(turn):
        d.press('FUNC')
        d.tap('PAGE', after_ms=500)
        d.release('FUNC', after_ms=400)
        d.turn('E', turn)
        d.wait(300)
        d.tap('NO', after_ms=500)

    d.setup_midi_track_a()
    d.mod_menu_first()
    d.wait(1500)
    d.tap('RECORD', after_ms=300)               # a kick on step 1, to time key 9 by
    d.tap('1', after_ms=150)
    d.tap('RECORD', after_ms=300)

    l = d.leds()
    check('stopped: keys 9-16 dark', all(l[k] != WHITE for k in range(8, 16)), str([l[k] for k in range(8, 16)]))

    # 16 steps: 9 9 10 10 ... 16 16.
    blinks, ons = play(6500)
    loop = loop_of(blinks, 16)
    check('16 steps: keys 9-16 blink twice each, in order', loop == expected(16), str(loop))
    gaps = [b[0] - a[0] for a, b in zip(blinks[1:], blinks[2:])]
    avg = sum(gaps) / len(gaps) if gaps else 0
    check('a blink each 16th (86 BPM: 174 ms)', 160 < avg < 190, '%.0f ms' % avg)
    starts9 = [t for i, (t, k) in enumerate(blinks) if k == 9 and (i == 0 or blinks[i - 1][1] != 9)]
    later = [n for n in ons if n > 1000]       # the loops after the first: the first step starts with PLAY
    near = [min((abs(s - n) for s in starts9), default=999) for n in later]
    check('key 9 lights with step 1\'s note (within 60 ms)', later and all(x < 60 for x in near),
          'notes %s, key 9 %s' % ([round(x) for x in ons], [round(x) for x in starts9]))
    l = d.leds()
    check('stopped again: keys 9-16 dark', all(l[k] != WHITE for k in range(8, 16)))

    # 13 steps (odd): keys 9-14 twice, 15 once, 16 dark.
    set_length(-12)
    blinks, _ = play(6500)
    loop = loop_of(blinks, 13)
    check('13 steps: keys 9-14 twice, key 15 once', loop == expected(13), str(loop))
    check('13 steps: key 16 stays dark', all(k != 16 for _, k in blinks), str(sorted({k for _, k in blinks})))

    # Longer than a page (the knob gives 17-20 steps): page 1 on keys 9-16, then page 2's few steps from key 9.
    set_length(24)
    blinks, _ = play(9000)
    n = d.peek32(d.symbols['dm_run_len'])
    keys = [k for _, k in blinks]
    want = expected(n)
    found = any(keys[i:i + n] == want for i in range(len(keys) - n + 1))
    check('%d steps: a page of 16, then page 2\'s %d steps from key 9' % (n, n - 16), 16 < n < 32 and found,
          str(keys))
    check('the firmware is still running', not d.halted, str(d.halted))

    failed = [n for n, ok in results if not ok]
    print('\n%d of %d passed' % (len(results) - len(failed), len(results)))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
