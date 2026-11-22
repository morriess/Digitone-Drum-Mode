# Digitone Drum Mode

Drum Mode, a custom-firmware mod for the **Elektron Digitone mk1** (OS 1.44) that turns a MIDI
track into an 8-lane drum sequencer, built on [elekloader](https://github.com/irpina/elekloader).

> **Beta.** Drum Mode 0.10 was tested on a real Digitone (OS 1.44) on 2026-11-22: almost
> everything works; the bugs found are listed in [drum-mode](drum-mode/README.md#known-bugs).
> 0.10.1 (first fix) waits for the unit.

| Mod | What it does | Status |
| --- | --- | --- |
| [drum-mode](drum-mode/) | Turns a MIDI track into an 8-lane drum sequencer on one MIDI channel ([manual](drum-mode/docs/MANUAL.md)) | 0.10.1, beta: **working on the unit** (0.10 tested 2026-11-22; a few bugs open) |

Looking for a MIDI-track arpeggiator? DigiArp MIDI by aleks-karachevtsev is in the
[elekloader Mod Shop](https://elekloader.github.io/shop/). To use it together with Drum Mode
it has to be rebuilt on core 5.3: that lives in its own fork,
[morriess/Digitone-MIDI-Arpeggiator, branch core-5.3](https://github.com/morriess/Digitone-MIDI-Arpeggiator/tree/core-5.3), with a build script for one
firmware with both mods. *To do later:* try the two together on the unit, then offer DigiArp's
author the core 5.3 change as a pull request (see the fork's
[CORE5.md](https://github.com/morriess/Digitone-MIDI-Arpeggiator/blob/core-5.3/CORE5.md#to-do-later)).

## Future ideas

- **A generative melody mod (separate from Drum Mode).** Turing-machine-style melodies that
  slowly change, for MIDI and synth tracks. The trigs give the rhythm and the mod gives the
  pitch: as the sequencer plays a trig, its note is replaced by the next value of a looping
  sequence (the same hook Drum Mode uses for its notes, so no extra timing work). Controls on a
  page of its own: LENGTH (how many notes before it repeats, 2–16), CHANGE (the chance a note
  mutates on each pass: 0 locked, 100% random), RANGE and SCALE (the keyboard's ROOT/SCALE
  could be used), and a key combo to lock a melody you like. On or off per track, so it can sit
  next to Drum Mode and DigiArp on other tracks. To work out: keeping each loop per pattern
  within core's 480 bytes of project data (a 16-note loop is small) or in the pattern itself.

## Rules for this repo

- Never commit Elektron's OS files (`.syx`) or firmware built from them. Each user
  builds with their own stock OS file. The `.gitignore` blocks these.
- Each mod lives in its own folder with its own README.

## Disclaimer

Unofficial, experimental firmware. Not affiliated with or supported by Elektron.
Back up your projects before flashing, and keep the stock OS file to restore.
