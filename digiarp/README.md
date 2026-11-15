# DigiArp MIDI

An arpeggiator for the Digitone mk1's **MIDI tracks A–D** (OS 1.44), made by
**aleks-karachevtsev**: [Digitone-MIDI-Arpeggiator](https://github.com/aleks-karachevtsev/Digitone-MIDI-Arpeggiator)
(GPL-2.0-or-later). All credit for the mod goes to its author.

This folder holds no DigiArp code. [build.sh](build.sh) fetches the author's
repository at a fixed commit and rebuilds the combination they tested:

| Part | Version |
| --- | --- |
| DigiArp | DA27 beta (`DigiArp-MIDI-DA27-0.9-dev11.elemod`), commit `2a853b7`, 2026-11-07 |
| elekloader core-dn1 | 4.1.1 (elekloader `v0.9.0`) |
| elekloader seq-events-dn1 | 0.1 |

## Build

```
./build.sh /path/to/Digitone_and_Digitone_Keys_OS1.44.syx
```

Writes `out/Digitone_OS1.44_DigiArpMIDI_DA27.syx`, shown as OS version **ARP1**
on the unit. The script checks the DigiArp module's SHA-256 against the
author's published file and stops if it differs.

## Use

- Select a MIDI track A–D and set its channel to your synth's.
- **FUNC + the note key above PTN** turns the arp on or off for that track (lit yellow when on).
- **FUNC + SYN1** opens the ARP page: MODE, SPEED, RANGE, LEN and the step editor now control the MIDI track.
- Synth tracks 1–4 keep their normal arp. Each MIDI track has its own arp settings, saved with the project.

See the author's `HARDWARE_CONTROLS.md` for the full controls.

## Known issues (from the author)

- Sometimes one held key with the sequencer stopped plays a repeating run of different notes.
- No LATCH. SPEED moves in fine steps, not musical divisions.

Our investigation of the stuck-notes bug is in [NOTES.md](NOTES.md).

## Restore

Hold FUNC while powering on, choose OS UPGRADE (TRIG 4), and send the stock OS 1.44 `.syx` with Elektron Transfer.
