# SPDX-License-Identifier: GPL-2.0-or-later
"""Drive a Digitone mk1 firmware in digiemu (https://github.com/irpina/digiemu) from a script.

Run with digiemu's Python:

    uv run --project $DIGIEMU python drum-mode/tests/<test>.py <firmware.syx>

DIGIEMU must point at a digiemu checkout with the patched Unicorn installed
(tools/install-patched-unicorn.sh).
"""
import json
import os
import subprocess
import sys

DIGIEMU = os.environ.get('DIGIEMU', os.path.expanduser('~/digiemu'))
sys.path.insert(0, DIGIEMU)


class Digitone:
    """A booted Digitone: keys by name, MIDI out captured, memory and the screen readable."""

    def __init__(self, syx, home):
        self.symbols = {}
        if os.path.exists(syx + '.map.json'):
            with open(syx + '.map.json') as fh:
                self.symbols = {k: int(v, 16) for k, v in json.load(fh).items()
                                if isinstance(v, str) and v.startswith('0x')}
        if not os.path.isdir(os.path.join(home, 'firmware')):
            subprocess.run([sys.executable, '-m', 'emu.portable', '--home', home, '--add', syx, '--yes'],
                           cwd=DIGIEMU, check=True, stdout=subprocess.DEVNULL)
        from emu.bootstrap import FirmwarePaths
        from emu.session import Session
        from emu import midi
        root = os.path.join(home, 'firmware')
        (slug,) = os.listdir(root)
        with open(os.path.join(root, slug, 'firmware.json')) as fh:
            name = json.load(fh)['release']['syx_name']
        paths = FirmwarePaths(os.path.join(root, slug), name, os.path.join(DIGIEMU, 'devices'))
        os.environ.update(paths.env())
        self.midi_sent = []
        self.s = None
        # Hooks must go in before the emulator translates any code (a block translated without a hook never
        # calls it), so they are added from the session's on_machine callback.
        self.s = Session(paths.gui, paths.syx, audio=False, on_machine=self._hook_midi_send)
        self.wait(500)

    # Where OS 1.44 hands MIDI out, logged at each routine's entry whatever MIDI CONFIG's ports say:
    # the port switch's send (0x400ee1a6, f(buf, len, flags)), and the sequencer MIDI engine's two
    # append routines (0x400f3e70, 0x400f3c1a, f(len, msg)), which carry the MIDI tracks' notes.
    MIDI_SEND = 0x400ee1a6
    SEQ_MIDI = (0x400f3e70, 0x400f3c1a)

    def _hook_midi_send(self, session):
        from unicorn.m68k_const import UC_M68K_REG_A7

        def on_send(uc, address, size, data):
            sp = uc.reg_read(UC_M68K_REG_A7)
            buf = int.from_bytes(uc.mem_read(sp + 4, 4), 'big')
            n = int.from_bytes(uc.mem_read(sp + 8, 4), 'big')
            if 0 < n <= 16:
                self.midi_sent.append(bytes(uc.mem_read(buf, n)))
        session.at(self.MIDI_SEND, on_send)

        def on_append(uc, address, size, data):
            sp = uc.reg_read(UC_M68K_REG_A7)
            n = int.from_bytes(uc.mem_read(sp + 4, 4), 'big')
            buf = int.from_bytes(uc.mem_read(sp + 8, 4), 'big')
            if 0 < n <= 16:
                self.midi_sent.append(bytes(uc.mem_read(buf, n)))
        for a in self.SEQ_MIDI:
            session.at(a, on_append)
        # And the DIN port itself (UART9), as digiemu's window listens to it.
        from emu import midi
        self.din_sent = []
        out = midi.MidiOut(self.din_sent.append)
        out.install(session.m)
        out.attach(session.m, session.ips)
        session.sources = tuple(session.sources) + (out,)

    # -- keys ---------------------------------------------------------------------------------
    def code(self, name):
        return self.s.code(name)

    def press(self, name, after_ms=60):
        self.s.press(self.code(name))
        self.wait(after_ms)

    def release(self, name, after_ms=60):
        self.s.release(self.code(name))
        self.wait(after_ms)

    def tap(self, name, hold_ms=100, after_ms=300):
        self.s.tap(self.code(name), hold_ms=hold_ms, after_ms=after_ms)

    def hold(self, name, ms):
        self.press(name, after_ms=ms)
        self.release(name, after_ms=300)

    def turn(self, encoder, detents, after_ms=60):
        """Turn an encoder (A-H) one detent at a time: a burst is swallowed by its dead zone."""
        for _ in range(abs(detents)):
            self.s.turn(self.s.encoder(encoder), 1 if detents > 0 else -1, after_ms=after_ms)
        self.wait(250)

    def wait(self, ms):
        self.s.run_ms(ms)

    def _top(self, presses=12):
        for _ in range(presses):
            self.tap('UP', after_ms=60)

    def midi_out_to_din(self):
        """digiemu's setup leaves SETTINGS > MIDI CONFIG > PORT CONFIG > OUTPUT TO at USB, which the
        emulator does not carry: set it to MIDI+USB (its last choice) so MIDI out reaches DIN."""
        self.tap('GLOBAL')
        self._top()
        for key in ('DOWN',) * 4 + ('YES',):            # MIDI CONFIG
            self.tap(key)
        self._top()
        self.tap('DOWN')
        self.tap('YES')                                 # PORT CONFIG
        self._top()
        for key in ('DOWN',) * 4 + ('RIGHT',) * 3:      # OUTPUT TO
            self.tap(key)
        for _ in range(6):
            self.tap('NO', after_ms=400)

    def settle(self, quiet_ms=250, limit_ms=2000):
        """Run until the screen has not changed for quiet_ms."""
        start = self.s.ms
        while self.s.ms - start < limit_ms:
            self.s.run_ms(50)
            if not self.s.frames or self.s.ms - self.s.frames[-1][0] >= quiet_ms:
                return

    def _project_menu(self):
        self.tap('GLOBAL')
        self._top()
        self.tap('YES')
        self._top()

    def _leave_settings(self):
        for _ in range(6):
            self.tap('NO', after_ms=400)

    def save_project(self, slot):
        """SETTINGS > PROJECT > SAVE PROJECT AS slot (2-128; 1 holds the write-protected presets)."""
        self._project_menu()
        self.tap('DOWN')
        self.tap('YES')
        self._top()
        for _ in range(slot - 1):
            self.tap('DOWN')
        self.tap('YES')                 # the name editor
        self.tap('YES')                 # save
        self.settle(quiet_ms=5000, limit_ms=120000)
        self._leave_settings()

    def load_project(self, slot):
        """SETTINGS > PROJECT > LOAD PROJECT slot, answering yes to losing changes."""
        self._project_menu()
        self.tap('YES')
        self._top()
        for _ in range(slot - 1):
            self.tap('DOWN')
        self.tap('YES')
        self.tap('YES')
        self.settle(quiet_ms=20000, limit_ms=300000)
        self._leave_settings()

    def setup_midi_track_a(self):
        """MIDI out to DIN, MIDI track A selected, and its CHAN enabled (the default project has it OFF:
        FUNC + push A on its SRC page). Leaves the TRIG page shown."""
        self.midi_out_to_din()
        self.select_midi_a()
        self.tap('SYN1', after_ms=400)
        self.press('FUNC')
        self.tap('A')
        self.release('FUNC', after_ms=400)
        self.tap('TRIG', after_ms=300)

    def active_track(self):
        return self.peek32(self.symbols['fw_active_track'])

    def select_midi_a(self):
        """MIDI track A (hold MIDI, press T1), unless it is already the active track: MIDI + T1 again would
        go back to the synth tracks."""
        if self.symbols and self.active_track() == 4:
            return
        self.press('MIDI')
        self.tap('T1')
        self.release('MIDI', after_ms=300)

    def mod_menu_first(self):
        """Hold a track key for the Mod Menu and pick its first entry (DRUM MODE, the build's only one)."""
        self.hold('T1', 700)
        self.tap('YES', after_ms=500)

    # -- state --------------------------------------------------------------------------------
    def mem(self, addr, size):
        return bytes(self.s.m.uc.mem_read(addr, size))

    def peek32(self, addr):
        return int.from_bytes(self.mem(addr, 4), 'big')

    def take_midi(self):
        """Every message the Digitone sent since the last call, as tuples of bytes (real-time bytes dropped)."""
        data = b''.join(bytes(x) if not isinstance(x, int) else bytes((x,)) for x in self.midi_sent)
        self.midi_sent.clear()
        self.din_sent.clear()
        msgs, i = [], 0
        while i < len(data):
            b = data[i]
            if b >= 0xF8:                       # clock, active sensing
                i += 1
            elif 0x80 <= b < 0xC0 or 0xE0 <= b < 0xF0:
                msgs.append(tuple(data[i:i + 3]))
                i += 3
            elif 0xC0 <= b < 0xE0:
                msgs.append(tuple(data[i:i + 2]))
                i += 2
            else:
                i += 1
        return msgs

    def pixel(self, x, y):
        """The screen's pixel at x 0-127, y 0-63 (0 the top row), as the harness reads a frame."""
        frame = self.s.frames[-1][1]
        return (frame[7 - y // 8 + 8 * x] >> (y % 8)) & 1

    def screen_png(self, path, scale=4):
        from emu import screen
        w, h = 128 * scale, 64 * scale
        px = bytearray(w * h)
        for y in range(64):
            for x in range(128):
                if self.pixel(x, y):
                    for dy in range(scale):
                        row = (y * scale + dy) * w + x * scale
                        px[row:row + scale] = b'\xff' * scale
        with open(path, 'wb') as fh:
            fh.write(screen.png(px, w, h))

    @property
    def halted(self):
        return getattr(self.s, 'halted', None)

    def close(self):
        self.s.close()
