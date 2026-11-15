# SPDX-License-Identifier: GPL-2.0-or-later
"""Drum Mode 0.4 in digiemu: live recording the 8 pads.

    uv run --project $DIGIEMU python drum-mode/tests/test_live_record.py out/Digitone_OS1.44_DrumMode.syx [workdir]

Starts live recording on a Drum Mode track, hits pads at known moments of known steps (early in a step: that step;
late: the next), a chord, then stops and checks the pattern holds normal MIDI trigs with the lanes' notes as the
step's chord, that the stock's recorder left nothing behind, and that playback sends the recorded drums at the lanes'
velocities. Runs with digiemu's audio engine on: the Digitone's sequencer is clocked by it.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu_session import Digitone          # noqa: E402

STEP = 5400000                            # fw_timeline units a 16th
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

    def fired():                          # MIDI A's last fired step and its time
        step, prev, t, pt = struct.unpack('>iiII', d.mem(S['dm_fire'], 16))
        return step, t

    def wait_for(step_playing, phase):
        """Run until step_playing is playing and `phase` (0-1) of it has passed. The sequencer fires a step one
        step before it plays, so step s plays from the fire of s + 1."""
        for _ in range(4000):
            s, t = fired()
            if s == (step_playing + 1) % 16 and (timeline() - t) >= phase * STEP:
                return True
            d.wait(2)
        return False

    def pattern(track=4):
        pat = d.peek32(S['fw_pattern'])
        trk = pat + 976 * track
        words = [int.from_bytes(d.mem(trk + 2 * s, 2), 'big') for s in range(16)]
        slots = [list(struct.unpack('8b', d.mem(trk + 448 + 8 * s, 8))) for s in range(16)]
        return words, slots

    def notes_of(slots):
        base = slots[0]
        return sorted([base] + [base + iv for iv in slots[1:] if iv])

    d.setup_midi_track_a()
    words, _ = pattern()
    check('track A starts with no trigs', not any(w & 1 for w in words), str(words))
    d.mod_menu_first()
    d.wait(1200)

    # Pad 2's velocity to 80, pad 1 stays 100: a chord of both plays at 100, pad 2 alone at 80.
    d.press('2', after_ms=60)
    d.turn('B', -20)
    d.release('2', after_ms=200)
    d.take_midi()

    # Pads played while not recording are not recorded.
    d.tap('1')
    words, _ = pattern()
    check('pads played without recording write nothing', not any(w & 1 for w in words), str(words))

    # Live recording: RECORD + PLAY.
    d.press('RECORD')
    d.tap('PLAY', hold_ms=50, after_ms=50)
    d.release('RECORD', after_ms=50)
    rec = d.peek32(0x419f0dd8)
    check('live recording is on', d.mem(rec + 0x169, 1)[0] == 1)

    hits = [('1', 2, 0.15, 2), ('2', 5, 0.8, 6), ('3', 9, 0.4, 9)]     # pad, playing step, phase -> expected step
    for pad, step, phase, _ in hits:
        ok = wait_for(step, phase)
        d.press(pad, after_ms=20)
        d.release(pad, after_ms=20)
        if not ok:
            check('reached step %d' % step, False)
    # A chord on step 12: pads 1 and 4 together.
    wait_for(12, 0.2)
    d.press('1', after_ms=5)
    d.press('4', after_ms=20)
    d.release('1', after_ms=5)
    d.release('4', after_ms=20)
    d.wait(400)
    d.tap('STOP', after_ms=300)
    d.tap('STOP', after_ms=300)

    words, slots = pattern()
    trigs = [s for s in range(16) if words[s] & 1]
    check('the hits made trigs on steps 2, 6, 9 and 12', trigs == [2, 6, 9, 12], str(trigs))
    for pad, step, phase, want in hits:
        lane_note = {'1': 36, '2': 38, '3': 42}[pad]
        check('pad %s hit %d%% into step %d recorded on step %d' % (pad, int(phase * 100), step, want),
              words[want] & 1 and notes_of(slots[want]) == [lane_note], 'step %d notes %s' % (want, slots[want]))
    check('the chord recorded both lanes on step 12', notes_of(slots[12]) == [36, 46], str(slots[12]))
    check('the stock recorder left nothing on step 0', not (words[0] & 1), str(words[0]))
    check('Drum Mode counted 5 recorded notes', d.peek32(S['dm_rec_count']) == 5, str(d.peek32(S['dm_rec_count'])))

    # Playback: the recorded drums, at the lanes' velocities.
    d.take_midi()
    d.tap('PLAY', hold_ms=50, after_ms=50)
    d.wait(2300)
    d.tap('STOP', after_ms=300)
    played = [(m[1], m[2]) for m in d.take_midi() if (m[0] & 0xF0) == 0x90 and m[2] > 0]
    check('playback sends the kick at 100', (36, 100) in played, str(played))
    check('the snare at its pad velocity 80', (38, 80) in played, str(played))
    check('the closed hat', (42, 100) in played, str(played))
    check('and the chord: kick and open hat', (46, 100) in played and played.count((36, 100)) >= 2, str(played))
    check('the firmware is still running', not d.halted, str(d.halted))

    failed = [n for n, ok in results if not ok]
    print('\n%d passed, %d failed' % (len(results) - len(failed), len(failed)))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
