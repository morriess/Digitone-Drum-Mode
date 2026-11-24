# SPDX-License-Identifier: GPL-2.0-or-later
"""Drum Mode in digiemu: the running light while playing (0.10.5: keys 1-16, one per step).

    uv run --project $DIGIEMU python tests/test_run_light.py out/Digitone_OS1.44_DrumMode.syx [workdir]

Key n stands for step n of the 16-step page that plays, lit red for the whole step, as the stock's playhead (0.10.6). A track's page of
fewer steps (a short length, or the last page of a longer track) leaves the keys after its last step dark. On keys 1-8
the light shows over the pad's own light for its half step, and the pad's state (the selected lane's dim white) shows
again after. Dark when stopped. Checked on 16 steps, 13-14 and 17-20 (a second page of a few steps), the blinks a 16th
apart and key 1's on step 1's note. (0.9.1-0.10.4: keys 9-16, 2 steps a key; until 0.10.5 white for half a step.)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu_session import Digitone          # noqa: E402

WHITE = 20
RED = 3
DIM_WHITE = 21
results = []


def check(name, ok, detail=''):
    results.append((name, ok))
    print('%s  %s%s' % ('PASS' if ok else 'FAIL', name, ('  (%s)' % detail) if detail else ''))


def expected(length):
    """The keys (1-16) that blink over one loop of a track of this length."""
    return [1 + s % 16 for s in range(length)]


def main():
    syx = os.path.abspath(sys.argv[1])
    work = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else 'emu-work')
    os.makedirs(work, exist_ok=True)
    d = Digitone(syx, os.path.join(work, 'home'), audio=True)

    mo = [x for x in d.s.sources if type(x).__name__ == 'MidiOut'][0]
    wire = []
    mo.sink = lambda x: wire.append((d.s.now_ms(), bytes(x) if not isinstance(x, int) else bytes((x,))))

    def play(ms):
        """Play for ms; -> the blinks (time, key 1-16), step 1's note-on times, and whether every lit key was white
        and the selected pad (key 1) showed dim white between its blinks."""
        d.leds()
        wire.clear()
        t0 = d.s.now_ms()
        d.tap('PLAY', hold_ms=50, after_ms=20)
        blinks, cur, seen, pad_dim = [], None, set(), False
        lit_n, total = [0], [0]
        while d.s.now_ms() - t0 < ms:
            d.wait(8)
            l = d.leds()
            r = d.peek32(d.symbols['dm_run_key'])
            r = r - (1 << 32) if r >= 1 << 31 else r
            k = r + 1 if r >= 0 else None
            if k is not None and l.get(k - 1) == RED:
                seen.add(k)
            if k != 1 and l.get(0) == DIM_WHITE:
                pad_dim = True
            if k is not None:
                lit_n[0] += 1
            total[0] += 1
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
        d.lit_share = lit_n[0] / max(1, total[0])
        return blinks, ons, seen, pad_dim

    def loop_of(blinks, length):
        """One full loop of keys, from the second key-1 start."""
        keys = [k for _, k in blinks]
        starts = [i for i in range(len(keys)) if keys[i] == 1 and (i == 0 or keys[i - 1] != 1)]
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
    d.tap('RECORD', after_ms=300)               # a kick on step 1, to time key 1 by
    d.tap('1', after_ms=150)
    d.tap('RECORD', after_ms=300)

    l = d.leds()
    check('stopped: keys 9-16 dark, key 1 the selected pad (dim white)',
          all(l[k] != RED for k in range(8, 16)) and l.get(0) == DIM_WHITE, str([l.get(k) for k in range(16)]))

    # 16 steps: 1 2 3 ... 16.
    blinks, ons, white, pad_dim = play(6500)
    loop = loop_of(blinks, 16)
    check('16 steps: keys 1-16 blink once each, in order', loop == expected(16), str(loop))
    check('the running light is red on every key, 1-8 too', white >= set(range(1, 17)), str(sorted(white)))
    check('key 1 shows the selected pad (dim white) between its blinks', pad_dim)
    check('a key is lit the whole step, no dark gaps (steady, as the stock)', d.lit_share > 0.95, '%.2f' % d.lit_share)
    gaps = [b[0] - a[0] for a, b in zip(blinks[1:], blinks[2:])]
    avg = sum(gaps) / len(gaps) if gaps else 0
    check('a blink each 16th (86 BPM: 174 ms)', 160 < avg < 190, '%.0f ms' % avg)
    starts9 = [t for i, (t, k) in enumerate(blinks) if k == 1 and (i == 0 or blinks[i - 1][1] != 1)]
    later = [n for n in ons if n > 1000]       # the loops after the first: the first step starts with PLAY
    near = [min((abs(s - n) for s in starts9), default=999) for n in later]
    check('key 1 lights with step 1\'s note (within 60 ms)', later and all(x < 60 for x in near),
          'notes %s, key 1 %s' % ([round(x) for x in ons], [round(x) for x in starts9]))
    l = d.leds()
    check('stopped again: no running light (keys 2-16 dark)', all(l.get(k) != RED for k in range(1, 16)), str([l.get(k) for k in range(16)]))

    # A shorter length (the knob gives 13-14 steps): keys 1-n, the keys after it dark.
    set_length(-12)
    blinks, _, _, _ = play(6500)
    n = d.peek32(d.symbols['dm_run_len'])
    loop = loop_of(blinks, n)
    check('%d steps: keys 1-%d once each' % (n, n), 8 < n < 16 and loop == expected(n), str(loop))
    check('%d steps: keys %d-16 stay dark' % (n, n + 1), all(k <= n for _, k in blinks),
          str(sorted({k for _, k in blinks})))

    # Longer than a page (the knob gives 17-20 steps): page 1 on keys 1-16, then page 2's few steps from key 1.
    set_length(24)
    blinks, _, _, _ = play(9000)
    n = d.peek32(d.symbols['dm_run_len'])
    keys = [k for _, k in blinks]
    want = expected(n)
    found = any(keys[i:i + n] == want for i in range(len(keys) - n + 1))
    check('%d steps: a page of 16, then page 2\'s %d steps from key 1' % (n, n - 16), 16 < n < 32 and found,
          str(keys))
    check('the firmware is still running', not d.halted, str(d.halted))

    failed = [n for n, ok in results if not ok]
    print('\n%d of %d passed' % (len(results) - len(failed), len(results)))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
