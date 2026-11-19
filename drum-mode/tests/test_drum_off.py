# SPDX-License-Identifier: GPL-2.0-or-later
"""Drum Mode in digiemu: what happens to a Drum Mode beat when Drum Mode is turned off, and on again.

    uv run --project $DIGIEMU python drum-mode/tests/test_drum_off.py out/Digitone_OS1.44_DrumMode.syx [workdir]

Programs a beat in GRID RECORDING (kick on 1, 5, 9, 13; snare on 5, 13; closed hat on every odd step, so steps 1, 5,
9 and 13 are chords), sets the kick's velocity to 90 and the snare's to 70, mutes the hat, and plays two bars after
each change:
1. Drum Mode on: no hats; every step at its loudest unmuted lane's velocity (90);
2. Drum Mode off (Mod Menu): the pattern data is unchanged, and the stock OS plays it: the same notes, the hats back,
   every note at the trig's own velocity (100); the pads are the stock trig keys again;
3. Drum Mode on again: as in 1 (the mute and the velocities were kept).
The sequencer prepares a step one step early, step 1 of a new run at the end of the run before; since 0.9.4 Drum Mode
has it prepare step 1 again after a change (seq-events' sq_rearm), so already the first bar after each change plays
the new way. Prints what each bar sent; results in docs/design.md.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu_session import Digitone          # noqa: E402

results = []
KICK, SNARE, HAT = 36, 38, 42


def check(name, ok, detail=''):
    results.append((name, ok))
    print('%s  %s%s' % ('PASS' if ok else 'FAIL', name, ('  (%s)' % detail) if detail else ''))


def main():
    syx = os.path.abspath(sys.argv[1])
    work = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else 'emu-work')
    os.makedirs(work, exist_ok=True)
    d = Digitone(syx, os.path.join(work, 'home'), audio=True)
    S = d.symbols

    def pattern():
        trk = d.peek32(S['fw_pattern']) + 976 * 4
        return d.mem(trk, 448 + 8 * 64)

    def steps(raw):
        out = {}
        for s in range(16):
            if int.from_bytes(raw[2 * s:2 * s + 2], 'big') & 1:
                sl = struct.unpack('8b', raw[448 + 8 * s:456 + 8 * s])
                out[s + 1] = sorted([sl[0]] + [sl[0] + iv for iv in sl[1:] if iv])
        return out

    STEP = 5400000                              # timeline units per 16th

    def wait_for(step_playing, phase):
        """Run until step_playing (0-15) plays and `phase` of it has passed; the sequencer fires a step one step
        before it plays, so step s plays from the fire of s + 1."""
        for _ in range(4000):
            st, _, t, _ = struct.unpack('>iiII', d.mem(S['dm_fire'], 16))
            if st == (step_playing + 1) % 16 and ((d.peek32(S['fw_timeline']) - t) & 0xFFFFFFFF) >= phase * STEP:
                return True
            d.wait(2)
        return False

    def ons():
        return [(m[1], m[2]) for m in d.take_midi() if (m[0] & 0xF0) == 0x90 and m[2] > 0]

    def play_bar():
        """One bar from PLAY: (step 1's note-ons, {note: [velocities]} of steps 2-16)."""
        d.take_midi()
        d.tap('PLAY', hold_ms=50, after_ms=0)
        wait_for(0, 0.5)
        first = sorted(ons())
        wait_for(14, 0.6)                       # step 15 played (16 is empty)
        d.tap('STOP', after_ms=400)
        rest = {}
        for n, v in ons():
            rest.setdefault(n, []).append(v)
        print('   step 1: %s; steps 2-16: %s' % (first, ', '.join('%d x%d vel %s' % (n, len(v), sorted(set(v)))
                                                               for n, v in sorted(rest.items()))))
        return first, rest

    def toggle_drum_mode():
        # Hold MIDI A's track key for the Mod Menu, a pause, then YES. Without the pause (YES straight after letting
        # go of the key, as mod_menu_first does) and right after a trig key press, digiemu's UI task stopped with the
        # menu open in one exact sequence of this test; see docs/design.md, "Open: a UI hang in the emulator".
        d.hold('T1', 700)
        d.wait(300)
        d.tap('YES', after_ms=800)
        d.wait(1500)

    def enabled():
        return d.mem(S['dm'] + 1, 1)[0] & 1

    d.setup_midi_track_a()
    toggle_drum_mode()
    check('Drum Mode on for MIDI A', enabled() == 1)

    # The beat, in GRID RECORDING, lane by lane (TRIG + pad selects).
    def lane(pad, keys):
        d.press('TRIG', after_ms=60)
        d.tap(pad, after_ms=200)
        d.release('TRIG', after_ms=300)
        for k in keys:
            d.tap(str(k), after_ms=120)
    d.tap('RECORD', after_ms=400)
    lane('1', (1, 5, 9, 13))
    lane('2', (5, 13))
    lane('3', (1, 3, 5, 7, 9, 11, 13, 15))
    d.tap('RECORD', after_ms=400)
    beat = steps(pattern())
    check('the beat is in the pattern (step 1 = kick + hat, 5 = kick + snare + hat)',
          beat.get(1) == [KICK, HAT] and beat.get(5) == [KICK, SNARE, HAT] and beat.get(3) == [HAT], str(beat))

    # Kick velocity 90, snare 70 (Drum Velocity page), hat muted.
    d.tap('LFO', after_ms=400)
    d.tap('LFO', after_ms=400)
    d.tap('LFO', after_ms=400)
    d.turn('A', -10)
    d.turn('B', -30)
    d.tap('TRIG', after_ms=400)
    d.press('FUNC', after_ms=40)
    d.tap('3', after_ms=200)
    d.release('FUNC', after_ms=300)
    d.wait(2600)
    vel = d.mem(S['dm'] + 40, 3)
    check('kick velocity 90, snare 70, hat muted', list(vel[:2]) == [90, 70] and d.mem(S['dm'] + 2, 1)[0] == 0b100,
          '%s, mutes %s' % (list(vel), bin(d.mem(S['dm'] + 2, 1)[0])))
    before = pattern()

    # Each setting change is followed by two bars. Step 1 of a run is prepared at the end of the run before; 0.9.4
    # has it prepared again after each change, so both bars show the new state (0.9.3 and before: the first bar's
    # step 1 still played the old way).
    print('Drum Mode on, hat muted, kick 90 / snare 70:')
    first, _ = play_bar()
    check('ON, 1st bar: step 1 already kick at 90, the hat muted (0.9.4)', first == [(KICK, 90)], str(first))
    first, rest = play_bar()
    check('ON, 2nd bar: step 1 is kick at 90, the hat muted', first == [(KICK, 90)], str(first))
    check('ON: steps 5, 9, 13: kick x3, snare x2, all at 90 (a step plays at its loudest lane\'s velocity), no hats',
          rest == {KICK: [90] * 3, SNARE: [90] * 2}, str(rest))
    on_bar = (first, rest)

    toggle_drum_mode()
    check('Drum Mode off for MIDI A', enabled() == 0)
    check('OFF: the pattern data is unchanged (all 64 steps, trigs and notes)', pattern() == before)
    print('Drum Mode off:')
    first, _ = play_bar()
    check('OFF, 1st bar: step 1 already the stock\'s: kick + hat at 100 (0.9.4)', first == [(KICK, 100), (HAT, 100)],
          str(first))
    first, rest = play_bar()
    check('OFF, 2nd bar: step 1 is the stock\'s: kick + hat at 100', first == [(KICK, 100), (HAT, 100)], str(first))
    check('OFF: the rest as programmed, every note at the trig\'s velocity (100): kick x3, snare x2, hat x7',
          rest == {KICK: [100] * 3, SNARE: [100] * 2, HAT: [100] * 7}, str(rest))
    d.take_midi()
    d.tap('1', after_ms=300)
    pad = ons()
    print('   OFF, trig key 1 pressed: %s' % (pad or 'nothing sent'))
    check('OFF: trig key 1 is a stock key again (no kick from the pad)', not any(n == KICK for n, v in pad), str(pad))

    toggle_drum_mode()
    check('Drum Mode on again', enabled() == 1)
    check('ON again: the mute and the velocities were kept', d.mem(S['dm'] + 2, 1)[0] == 0b100 and
          list(d.mem(S['dm'] + 40, 2)) == [90, 70])
    print('Drum Mode on again:')
    first, _ = play_bar()
    check('ON again, 1st bar: step 1 already kick at 90, the hat muted (0.9.4)', first == [(KICK, 90)], str(first))
    again = play_bar()
    check('ON again, 2nd bar: plays exactly as before turning it off', again == on_bar, str(again))
    check('the firmware is still running', not d.halted, str(d.halted))

    failed = [n for n, ok in results if not ok]
    print('\n%d passed, %d failed' % (len(results) - len(failed), len(failed)))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
