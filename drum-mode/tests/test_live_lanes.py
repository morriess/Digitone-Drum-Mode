# SPDX-License-Identifier: GPL-2.0-or-later
"""Milestone 2 in digiemu: Drum Mode's live lanes.

    uv run --project $DIGIEMU python drum-mode/tests/test_live_lanes.py out/Digitone_OS1.44_DrumMode.syx [workdir]

Selects MIDI track A, turns Drum Mode on from the Mod Menu, plays trig keys and checks the MIDI the
Digitone sends: each of trig 1-8 its lane's note on track A's channel, several at once, and the
note-offs; stock behaviour with Drum Mode off and on another track.
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

    def active():
        return d.peek32(addr['fw_active_track'])

    def enabled(m):
        # struct dm_save { u8 version; u8 enabled (bit per track); ... }
        return (d.mem(addr['dm'] + 1, 1)[0] >> m) & 1

    # Setup: MIDI out to DIN (digiemu leaves it at USB), MIDI track A selected (hold MIDI, press T1), and its
    # CHAN enabled (FUNC + push A on its SRC page: the default project has it OFF, so nothing is sent).
    d.midi_out_to_din()
    d.press('MIDI')
    d.tap('T1')
    d.release('MIDI', after_ms=300)
    check('MIDI track A is active', active() == 4, 'active track %d' % active())
    d.tap('SYN1', after_ms=400)
    d.press('FUNC')
    d.tap('A')
    d.release('FUNC', after_ms=400)
    d.tap('TRIG', after_ms=300)

    # The stock KEYBOARD mode on track A, for its channel: what Drum Mode must send on.
    d.tap('KEYBOARD')
    d.take_midi()
    d.tap('9')
    stock = notes_on(d.take_midi())
    d.tap('KEYBOARD')
    check('stock KEYBOARD mode sends on track A', len(stock) == 1, str(stock))
    chan = stock[0][0] if stock else None

    # Drum Mode off: trig 1 plays no lane note.
    d.take_midi()
    d.tap('1')
    msgs = d.take_midi()
    check('Drum Mode off: trig 1 sends no drum note', not [n for n in notes_on(msgs) if n[1] in GM], str(msgs))
    d.tap('1')                                  # put back whatever the stock did (a trig toggled)

    d.take_midi()
    # Mod Menu: hold T1, YES picks the first entry (DRUM MODE, the only mod entry in this build).
    d.hold('T1', 700)
    d.tap('YES', after_ms=500)
    check('DRUM MODE toggles Drum Mode on for track A', enabled(0) == 1, 'enabled=%d active=%d' % (enabled(0), active()))
    d.screen_png(os.path.join(work, 'menu-toggled.png'))
    d.wait(1200)                                # the corner message times out

    # Trig 1: kick on press, its note-off on release.
    d.take_midi()
    d.press('1', after_ms=100)
    din = list(d.din_sent)
    on = notes_on(d.take_midi())
    d.screen_png(os.path.join(work, 'lane1-held.png'))
    d.release('1', after_ms=100)
    off = notes_off(d.take_midi())
    check('trig 1 sends the kick (36) note-on', len(on) == 1 and on[0][1] == 36 and on[0][2] == 100, str(on))
    if chan is not None and on:
        check('on track A\'s channel', on[0][0] == chan, 'sent on %d, track A is %d' % (on[0][0], chan))
    check('release sends the kick\'s note-off', len(off) == 1 and off[0][1] == 36, str(off))
    check('and it leaves the DIN port', any(b[:1] and b[0] & 0xF0 == 0x90 and len(b) > 1 and b[1] == 36 for b in din), str(din))

    # Each of trig 1-8 its own lane.
    played = []
    for k in range(8):
        d.press(str(k + 1), after_ms=60)
        played += [n[1] for n in notes_on(d.take_midi())]
        d.release(str(k + 1), after_ms=60)
        d.take_midi()
    check('trig 1-8 play the 8 GM lanes', played == GM, str(played))

    # Several at once: hold 1, 3 and 5 together.
    for k in ('1', '3', '5'):
        d.press(k, after_ms=40)
    d.wait(150)                                 # the MIDI engine sends on its next ticks
    on = sorted(n[1] for n in notes_on(d.take_midi()))
    d.wait(150)                                 # a frame or two for the screen
    d.screen_png(os.path.join(work, 'lanes-1-3-5-held.png'))
    lit = [d.pixel(96 + 4 * lane + 1, 4) for lane in range(8)]     # each lane box's centre, top-right corner
    check('the screen lights lanes 1, 3 and 5 while held', lit == [1, 0, 1, 0, 1, 0, 0, 0], str(lit))
    for k in ('1', '3', '5'):
        d.release(k, after_ms=40)
    d.wait(300)
    off = sorted(n[1] for n in notes_off(d.take_midi()))
    check('three pads at once: three note-ons', on == [36, 39, 42], str(on))
    check('and three note-offs', off == [36, 39, 42], str(off))

    # FUNC + pad 6 mutes lane 6 (no note), even when FUNC is let go before the pad; again unmutes it.
    def mutes():
        return d.mem(addr['dm'] + 2, 1)[0]      # struct dm_save: version, enabled, mutes[track]...
    d.press('FUNC', after_ms=60)
    d.press('6', after_ms=60)
    d.release('FUNC', after_ms=60)
    d.release('6', after_ms=150)
    sent = notes_on(d.take_midi())
    taken = d.peek32(addr['dm_taken'])
    check('FUNC + pad 6 mutes lane 6 and plays nothing', mutes() == 0x20 and not sent and taken == 0,
          'mutes=%#x sent=%s taken=%#x' % (mutes(), sent, taken))
    d.press('FUNC', after_ms=60)
    d.tap('6')
    d.release('FUNC', after_ms=150)
    check('FUNC + pad 6 again unmutes it', mutes() == 0, 'mutes=%#x' % mutes())
    d.take_midi()

    # Trig 9-16: nothing yet.
    d.tap('9')
    check('trig 9 sends nothing', not notes_on(d.take_midi()))

    # A pad held while the track changes still gets its note-off.
    d.press('2', after_ms=60)
    d.take_midi()
    d.tap('MIDI')                               # back to the synth tracks
    d.tap('T1')
    d.release('2', after_ms=100)
    off = notes_off(d.take_midi())
    check('a pad released after a track change still sends its note-off', [n[1] for n in off] == [38], str(off))
    check('synth track 1 is active', active() == 0, 'active track %d' % active())

    # On synth track 1 the trig keys are stock.
    d.tap('3')
    check('on a synth track trig 3 sends no drum note', not [n for n in notes_on(d.take_midi()) if n[1] in GM])
    d.tap('3')

    # Back to MIDI A, toggle Drum Mode off: stock again.
    d.press('MIDI')
    d.tap('T1')
    d.release('MIDI', after_ms=300)
    d.hold('T1', 700)
    d.tap('YES', after_ms=500)
    check('DRUM MODE again turns it off', enabled(0) == 0, 'enabled=%d' % enabled(0))
    d.wait(1200)
    d.take_midi()
    d.tap('4')
    check('Drum Mode off again: trig 4 sends no drum note', not [n for n in notes_on(d.take_midi()) if n[1] in GM])
    d.tap('4')

    check('the firmware is still running', not d.halted, str(d.halted))
    failed = [n for n, ok in results if not ok]
    print('\n%d passed, %d failed' % (len(results) - len(failed), len(failed)))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
