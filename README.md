# Digitone mods

Custom-firmware mods for the **Elektron Digitone mk1** (OS 1.44), built on
[elekloader](https://github.com/irpina/elekloader).

> **⚠️ Not working yet: in the debugging phase.** Drum Mode boots and partly works on a real
> Digitone, but recorded beats don't play back on the unit (see
> [Known bugs](drum-mode/README.md#known-bugs-possible-fixes)). Don't rely on it for music yet.

| Mod | What it does | Status |
| --- | --- | --- |
| [drum-mode](drum-mode/) | Turns a MIDI track into an 8-lane drum sequencer on one MIDI channel ([manual](drum-mode/docs/MANUAL.md)) | 0.8.1, **debugging on hardware**: fix for recorded beats not playing back, to confirm on the unit |
| [digiarp-core5](digiarp-core5/) | DigiArp MIDI (by aleks-karachevtsev) rebuilt for core 5.3, and one firmware with it and Drum Mode | emulator-tested |

Looking for a MIDI-track arpeggiator? DigiArp MIDI by aleks-karachevtsev is in the
[elekloader Mod Shop](https://elekloader.github.io/shop/).

## Rules for this repo

- Never commit Elektron's OS files (`.syx`) or firmware built from them. Each user
  builds with their own stock OS file. The `.gitignore` blocks these.
- Each mod lives in its own folder with its own README.

## Disclaimer

Unofficial, experimental firmware. Not affiliated with or supported by Elektron.
Back up your projects before flashing, and keep the stock OS file to restore.
