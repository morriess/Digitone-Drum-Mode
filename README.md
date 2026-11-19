# Digitone Drum Mode

Drum Mode, a custom-firmware mod for the **Elektron Digitone mk1** (OS 1.44) that turns a MIDI
track into an 8-lane drum sequencer, built on [elekloader](https://github.com/irpina/elekloader).

> **Beta.** Drum Mode 0.9.3 works on a real Digitone (OS 1.44), recorded beats included. A few
> smaller [known bugs](drum-mode/README.md#known-bugs-possible-fixes) remain, so back up your
> projects before you use it.

| Mod | What it does | Status |
| --- | --- | --- |
| [drum-mode](drum-mode/) | Turns a MIDI track into an 8-lane drum sequencer on one MIDI channel ([manual](drum-mode/docs/MANUAL.md)) | 0.9.3, beta: **working on the unit** (0.9's lane select, 0.9.1's running light and 0.9.2–0.9.3's stock trig keys still to try there) |

Looking for a MIDI-track arpeggiator? DigiArp MIDI by aleks-karachevtsev is in the
[elekloader Mod Shop](https://elekloader.github.io/shop/). To use it together with Drum Mode
it has to be rebuilt on core 5.3: that lives in its own fork,
[morriess/Digitone-MIDI-Arpeggiator, branch core-5.3](https://github.com/morriess/Digitone-MIDI-Arpeggiator/tree/core-5.3), with a build script for one
firmware with both mods.

## Rules for this repo

- Never commit Elektron's OS files (`.syx`) or firmware built from them. Each user
  builds with their own stock OS file. The `.gitignore` blocks these.
- Each mod lives in its own folder with its own README.

## Disclaimer

Unofficial, experimental firmware. Not affiliated with or supported by Elektron.
Back up your projects before flashing, and keep the stock OS file to restore.
