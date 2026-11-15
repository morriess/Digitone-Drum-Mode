# SPDX-License-Identifier: GPL-2.0-or-later
"""Drum Mode 0.3 in digiemu: assigning each pad's note and velocity, and keeping them with the project.

    uv run --project $DIGIEMU python drum-mode/tests/test_assign_notes.py out/Digitone_OS1.44_DrumMode.syx [workdir]

Holds a pad and turns A (its note, heard as it changes) and B (its velocity), checks the knobs are left to the page
when no pad is held, then saves the project, changes everything, loads it back and checks the notes and Drum Mode
came back; a project saved without Drum Mode loads with the GM defaults.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu_session import Digitone          # noqa: E402

GM = [36, 38, 42, 46, 39, 45, 50, 49]
results = []


def check(name, ok, detail=''):
    results.append((name, ok))
    print('%s  %s%s' % ('PASS' if ok else 'FAIL', name, ('  (%s)' % detail) if detail else ''))


def notes_on(msgs):
    return [(m[0] & 0x0F, m[1], m[2]) for m in msgs if (m[0] & 0xF0) == 0x90 and m[2] > 0]


def notes_off(msgs):
    return [(m[0] & 0x0F, m[1]) for m in msgs
            if (m[0] & 0xF0) == 0x80 or ((m[0] & 0xF0) == 0x90 and m[2] == 0)]


def main():
    syx = os.path.abspath(sys.argv[1])
    work = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else 'emu-work')
    with open(syx + '.map.json') as fh:
        sym = json.load(fh)
    sym = sym.get('symbols', sym)
    addr = {k: int(v, 16) if isinstance(v, str) else v for k, v in sym.items()}
    os.makedirs(work, exist_ok=True)
    d = Digitone(syx, os.path.join(work, 'home'))

    # struct dm_save: version, enabled, mutes[4], pad[2], note[4][8], velocity[4][8]
    def saved():
        b = d.mem(addr['dm'], 8 + 64)
        return {'version': b[0], 'enabled': b[1],
                'note': [list(b[8 + 8 * t:16 + 8 * t]) for t in range(4)],
                'velocity': [list(b[40 + 8 * t:48 + 8 * t]) for t in range(4)]}

    def pad(key, hold_ms=100):
        d.take_midi()
        d.press(key, after_ms=hold_ms)
        on = notes_on(d.take_midi())
        d.release(key, after_ms=150)
        return on

    d.setup_midi_track_a()
    s = saved()
    check('a new session starts with the GM drum map', s['version'] == 1 and s['note'][0] == GM, str(s['note'][0]))
    d.mod_menu_first()
    d.wait(1200)
    check('Drum Mode is on for track A', saved()['enabled'] & 1 == 1)

    # Hold pad 1, turn A two detents up: 36 -> 37 -> 38, each heard (the old note off, the new one on).
    d.take_midi()
    d.press('1', after_ms=100)
    d.take_midi()
    d.turn('A', 2)
    msgs = d.take_midi()
    on, off = notes_on(msgs), notes_off(msgs)
    d.wait(100)
    d.screen_png(os.path.join(work, 'pad1-note-38.png'))
    d.release('1', after_ms=150)
    off_release = notes_off(d.take_midi())
    check('turning A while holding pad 1 plays each new note', [n[1] for n in on] == [37, 38], str(on))
    check('and ends each old one', [n[1] for n in off] == [36, 37], str(off))
    check('letting go ends the last note', [n[1] for n in off_release] == [38], str(off_release))
    check('pad 1 is now note 38', saved()['note'][0][0] == 38, str(saved()['note'][0]))
    check('pad 1 now plays 38', [n[1] for n in pad('1')] == [38])

    # Down past the bottom: clamped at 0. Then back to a real value.
    d.press('1', after_ms=100)
    d.turn('A', -45)
    d.release('1', after_ms=150)
    check('a note stops at 0', saved()['note'][0][0] == 0, str(saved()['note'][0][0]))
    d.press('1', after_ms=100)
    d.turn('A', 40)
    d.release('1', after_ms=150)
    check('and turns back up', saved()['note'][0][0] == 40, str(saved()['note'][0][0]))

    # Hold pad 2, turn B ten detents down: velocity 90.
    d.press('2', after_ms=100)
    d.turn('B', -10)
    d.release('2', after_ms=150)
    check('turning B while holding pad 2 sets its velocity', saved()['velocity'][0][1] == 90,
          str(saved()['velocity'][0]))
    played = pad('2')
    check('pad 2 now plays velocity 90', played and played[0][2] == 90, str(played))
    check('other pads keep theirs', saved()['velocity'][0][0] == 100 and saved()['note'][0][2] == 42)

    # Held a moment without turning: the corner shows the pad's note.
    d.press('3', after_ms=900)
    d.screen_png(os.path.join(work, 'pad3-held.png'))
    shown = d.peek32(addr['dm_info_ticks'])
    d.release('3', after_ms=150)
    d.take_midi()
    check('holding pad 3 a moment shows its note in the corner', shown > 0, 'info ticks %d' % shown)

    # No pad held: the knobs are the page's, Drum Mode's notes do not move.
    before = saved()
    d.turn('C', 3)
    check('with no pad held, a knob leaves the drum map alone', saved()['note'] == before['note'])
    d.turn('C', -3)

    # Saved with the project: save to slot 2, change pad 4 and turn Drum Mode off, load slot 2.
    d.save_project(2)
    d.select_midi_a()
    d.press('4', after_ms=100)
    d.turn('A', 5)
    d.release('4', after_ms=150)
    d.mod_menu_first()
    d.wait(1200)
    changed = saved()
    check('before loading: pad 4 moved and Drum Mode is off',
          changed['note'][0][3] == 51 and not (changed['enabled'] & 1), str(changed['note'][0]))
    d.load_project(2)
    back = saved()
    check('loading the project brings back its drum map', back['note'][0] == [40, 38, 42, 46, 39, 45, 50, 49],
          str(back['note'][0]))
    check('and its velocities', back['velocity'][0][1] == 90, str(back['velocity'][0]))
    check('and Drum Mode on for track A', back['enabled'] & 1 == 1, 'enabled=%#x' % back['enabled'])
    d.select_midi_a()
    d.take_midi()
    check('after the load pad 1 plays note 40', [n[1] for n in pad('1')] == [40])

    # A project saved without Drum Mode (slot 1, the presets): the defaults, Drum Mode off.
    d.load_project(1)
    fresh = saved()
    check('a project without Drum Mode loads the GM defaults, off',
          fresh['note'][0] == GM and fresh['enabled'] == 0, '%s enabled=%#x' % (fresh['note'][0], fresh['enabled']))

    check('the firmware is still running', not d.halted, str(d.halted))
    failed = [n for n, ok in results if not ok]
    print('\n%d passed, %d failed' % (len(results) - len(failed), len(failed)))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
