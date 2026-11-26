# SPDX-License-Identifier: GPL-2.0-or-later
"""Drum Mode 0.11 in digiemu: a pattern reload brings back the saved pattern's kit.

    uv run --project $DIGIEMU python tests/test_reload.py out/Digitone_OS1.44_DrumMode.syx [workdir]

A pattern save (FUNC + YES) keeps each track's kit number, so the kit it points to gets a pin: it is not edited in
place or freed afterwards. With the project saved, A01's kick 40 is saved, changed to 45 (a copy), and FUNC + NO
brings back 40, as often as it's done. Before the project is saved the stock refuses the reload ("Prj must be
re-saved!"): Drum Mode keeps what it has (0.10 lost the pattern's kit).
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

    def st():
        b = d.mem(S['dm'], 72)
        return b[1] & 1, b[8]

    def refs():
        return list(d.mem(S['dm_store'], 16)[8:16])

    def kick_note(c):
        d.press('1', after_ms=150)
        d.turn('A', c, after_ms=30)
        d.wait(150)
        d.release('1', after_ms=300)
        d.wait(2600)

    def func(k):
        d.press('FUNC', after_ms=60)
        d.tap(k, after_ms=300)
        d.release('FUNC', after_ms=300)
        d.wait(1500)

    d.setup_midi_track_a()
    d.mod_menu_first()
    d.wait(1500)
    kick_note(4)
    check('A01: Drum Mode on, kick 40', st() == (1, 40), str(st()))
    kick_note(5)                                # 45, with the project never saved
    func('NO')
    check('unsaved project: the refused reload changes nothing (kick 45)', st() == (1, 45), str(st()))
    kick_note(-5)                               # back to 40

    d.save_project(2)
    d.wait(1500)
    func('YES')
    check('saved: its kit pinned (two uses)', refs()[0] == 2, str(refs()))
    kick_note(5)
    check('changed to 45: a copy, the saved kit untouched', st() == (1, 45) and refs()[:2] == [1, 1], '%s %s' % (st(), refs()))
    func('NO')
    check('FUNC + NO brings back kick 40, the copy freed', st() == (1, 40) and refs()[1] == 0, '%s %s' % (st(), refs()))
    kick_note(3)
    func('NO')
    check('changed again and reloaded again: 40', st() == (1, 40), str(st()))
    check('the firmware is still running', not d.halted, str(d.halted))

    failed = [n for n, ok in results if not ok]
    print('\n%d passed, %d failed' % (len(results) - len(failed), len(failed)))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
