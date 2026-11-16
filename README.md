# Digitone mods

Custom-firmware mods for the **Elektron Digitone mk1** (OS 1.44), built on
[elekloader](https://github.com/irpina/elekloader).

| Mod | What it does | Status |
| --- | --- | --- |
| [drum-mode](drum-mode/) | Turns a MIDI track into an 8-lane drum sequencer on one MIDI channel ([manual](drum-mode/docs/MANUAL.md)) | 0.8, emulator-tested |

Looking for a MIDI-track arpeggiator? DigiArp MIDI by aleks-karachevtsev is in the
[elekloader Mod Shop](https://elekloader.github.io/shop/).

## Rules for this repo

- Never commit Elektron's OS files (`.syx`) or firmware built from them. Each user
  builds with their own stock OS file. The `.gitignore` blocks these.
- Each mod lives in its own folder with its own README.

## Disclaimer

Unofficial, experimental firmware. Not affiliated with or supported by Elektron.
Back up your projects before flashing, and keep the stock OS file to restore.
