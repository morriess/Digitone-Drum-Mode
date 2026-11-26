# SPDX-License-Identifier: GPL-2.0-or-later
"""Drum Mode 0.10 in digiemu: Drum Mode's settings per pattern and per track.

    uv run --project $DIGIEMU python tests/test_pattern_settings.py out/Digitone_OS1.44_DrumMode.syx [workdir]

Each pattern keeps its own Drum Mode on/off, mutes, notes and velocities for each track. A pattern that has none yet
takes a copy of the pattern the player came from (the choice being tried out). FUNC + RECORD copies the active
pattern and FUNC + STOP pastes it: the Drum Mode settings come along.
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

    def pattern():
        return (d.peek32(S['fw_pattern']) - S['fw_patterns']) // 0x1611d

    def state():
        """MIDI A's (on, mutes, kick note, snare velocity) from the working copy."""
        b = d.mem(S['dm'], 72)
        return (b[1] & 1, b[2], b[8], b[40 + 1])

    def go(key):
        d.tap('PTN', after_ms=300)
        d.tap(key, after_ms=1200)

    def kick_note(counts):
        d.press('1', after_ms=150)
        d.turn('A', counts, after_ms=30)        # one continuous turn: the stock's dead zone swallows a slow one
        d.wait(150)
        d.release('1', after_ms=300)

    def snare_vel(counts):
        d.press('2', after_ms=150)
        d.turn('B', counts)
        d.wait(150)
        d.release('2', after_ms=300)

    def toggle():                               # Drum Mode on/off for MIDI A (hold T1, YES in the Mod Menu)
        d.hold('T1', 700)
        d.tap('YES', after_ms=800)

    def mute(pad):
        d.press('FUNC', after_ms=60)
        d.tap(pad, after_ms=200)
        d.release('FUNC', after_ms=300)

    d.setup_midi_track_a()
    d.mod_menu_first()
    d.wait(1500)
    check('A01, Drum Mode on: kick 36, snare velocity 100, nothing muted', pattern() == 0 and state() == (1, 0, 36, 100),
          str(state()))

    kick_note(4)                                # 36 -> 40
    snare_vel(-10)                              # 100 -> 90
    mute('3')
    d.wait(2600)
    a01 = state()
    check('A01 set up: kick 40, snare velocity 90, hat muted', a01 == (1, 0b100, 40, 90), str(a01))

    store = S['dm_store']
    def kits():
        b = d.mem(store, 20)                    # 0.11: 8 kits, refs at +8, the kits after a 40-byte head
        return b[5], sum(1 for r in b[8:16] if r), int.from_bytes(b[0:4], 'big')
    used_a01 = kits()[1]
    go('2')
    check('A02 is active', pattern() == 1, str(pattern()))
    check('A02, new (0.10.3): Drum Mode off, default notes, nothing muted, nothing copied', state() == (0, 0, 36, 100),
          str(state()))
    d.wait(1500)
    check('and visiting it keeps no settings (no kit used)', kits()[1] == used_a01, str(kits()))

    toggle()                                    # Drum Mode on for A02 only
    check('Drum Mode on on A02: still the default notes', state() == (1, 0, 36, 100), str(state()))
    snare_vel(-10)                              # 100 -> 90
    mute('4')                                   # the open hat off
    d.wait(2600)
    a02 = state()
    check('A02 changed: kick 36, open hat muted instead', a02 == (1, 0b1000, 36, 90), str(a02))

    go('1')
    check('back on A01: its own settings again', pattern() == 0 and state() == a01, str(state()))
    go('2')
    check('and A02 its own', pattern() == 1 and state() == a02, str(state()))

    # Drum Mode on/off is per pattern too.
    toggle()
    d.wait(2600)
    check('Drum Mode off on A02', state()[0] == 0, str(state()))
    go('1')
    check('A01 still has Drum Mode on', state()[0] == 1, str(state()))

    # Copy A01 (FUNC + RECORD), paste over a new pattern A03 (FUNC + STOP): its settings come along.
    d.press('FUNC', after_ms=60)
    d.tap('RECORD', after_ms=300)
    d.release('FUNC', after_ms=400)
    go('3')
    check('A03, new: Drum Mode off, default notes (not A01\'s)', pattern() == 2 and state() == (0, 0, 36, 100),
          str(state()))
    d.press('FUNC', after_ms=60)
    d.tap('STOP', after_ms=300)
    d.release('FUNC', after_ms=1500)
    check('FUNC + STOP pasted A01\'s Drum Mode settings onto A03', state() == a01, str(state()))

    # Saved with the project: save to slot 2, change A01, load slot 2, and each pattern has its settings back.
    d.save_project(2)
    go('1')
    kick_note(4)                                # A01: kick 44
    d.wait(300)
    check('before loading: A01\'s kick moved to 44', state()[2] == 44, str(state()))
    d.load_project(2)
    d.wait(1000)
    go('1')
    check('after loading: A01 has kick 40, hat muted, Drum Mode on again', state() == a01, str(state()))
    go('2')
    a02_off = (0,) + a02[1:]
    check('and A02 its own: Drum Mode off, kick 36, open hat muted', state() == a02_off, str(state()))

    # The kits: at most 8 different note/velocity sets per project (10 until 0.11; core's project data is small).
    # New kicks on A04-A16 make new kits until all 8 are in use; then a new note can't be kept and stays as it was.
    go('1')
    refused = None
    kept = {36, 40}                             # the kick notes kits already hold (default/A02/A03, A01)
    for i, key in enumerate(('4', '5', '6', '7', '8', '9', '10', '11', '12', '13', '14', '15', '16')):
        go(key)
        toggle()                                # new patterns start off (0.10.3)
        full = kits()[1] == 8
        kick_note(1 + i)                        # 37, 38, ... (40 is shared with A01's kit)
        d.wait(300)
        note = state()[2]
        if full and refused is None and note in kept:
            refused = (key, note)               # no new kit: the note stayed on one a kit holds
        kept.add(note)
        n, used, size = kits()
        if used > 8 or size > 40 + 16 * 8:
            break
    n, used, size = kits()
    check('never more than 8 kits, the block at most 168 bytes', used <= 8 and size <= 168,
          'kits %d, used %d, %d bytes' % (n, used, size))
    check('with all 8 in use a new note is refused (it stays as it was)', refused is not None and used == 8,
          'refused on trig %s' % (refused,))
    check('the firmware is still running', not d.halted, str(d.halted))

    failed = [n for n, ok in results if not ok]
    print('\n%d of %d passed' % (len(results) - len(failed), len(results)))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
