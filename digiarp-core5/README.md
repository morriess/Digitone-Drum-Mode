# DigiArp MIDI for core 5.3, together with Drum Mode

[DigiArp MIDI](https://github.com/aleks-karachevtsev/Digitone-MIDI-Arpeggiator) by
aleks-karachevtsev is a MIDI arpeggiator for the Digitone mk1's MIDI tracks. The version in
the elekloader Mod Shop (0.9.0-beta, `0.9-dev11`) is built on elekloader's **core 4.1**, so it
can't go into one firmware with Drum Mode, which needs **core 5.3**.

This folder rebuilds DigiArp for core 5.3 with a two-name change, and builds one firmware
with both mods.

> **Status: tested in the emulator, not yet on a unit.** A personal build: DigiArp's
> author hasn't reviewed this change. See [Sharing](#sharing).

## Why the two didn't combine

Building them together, elekloader refused:

```
core 5.3 and digiarp-midi 0.9-dev11 both export fw_note_off
core 5.3 and digiarp-midi 0.9-dev11 both export fw_note_on
```

- DigiArp names the Digitone's MIDI send routines itself: `fw_note_on` = 0x400ee232
  `(channel, note, velocity)` and `fw_note_off` = 0x400ee29e `(channel, note)`. Core 4.1 had
  no such names.
- Core 5.3 added `fw_note_on` and `fw_note_off` of its own, for a **different** firmware
  routine: the track-level note on/off that KEYBOARD mode uses, with six and three arguments.
- One name can't point to two routines, so the build tool refuses.

The two routines aren't the same, so DigiArp can't simply use core's versions.

## The change

[digiarp-core5.patch](digiarp-core5.patch), against DigiArp commit `2a853b7` (the Mod Shop's
build):

| File | Change |
| --- | --- |
| `firmware/trig-hook.s` | `fw_note_on` / `fw_note_off` renamed to `digiarp_midi_on` / `digiarp_midi_off` (same two `jmp`s to 0x400ee232 / 0x400ee29e) |
| `firmware/live.c` | its one call site uses the new names |
| `mod.json` | version `0.9-dev11-core5`; core `5.2.1` (5.2.1 moved two of core's own hooks to make room for DigiArp's; the rename covers 5.3) |

Nothing else changes: the same code, the same 12 firmware sites, the same settings.

## Building

```
./build.sh /path/to/Digitone_and_Digitone_Keys_OS1.44.syx
```

It builds Drum Mode first (its build fetches elekloader at the pinned commit), then fetches
DigiArp at `2a853b7`, applies the patch, and writes:

| File | Contents | OS version on the unit |
| --- | --- | --- |
| `out/Digitone_OS1.44_DrumMode_DigiArp.syx` | core 5.3 + seq-events 0.1 + Drum Mode 0.8 + DigiArp | **DMA1** |
| `out/Digitone_OS1.44_DigiArp_core5.syx` | core 5.3 + DigiArp only | **DAC5** |

Install as in the [Drum Mode manual](../drum-mode/docs/MANUAL.md#installing). Both files are
built from Elektron's OS: keep them for yourself.

## Using both

They work on separate MIDI tracks:

- **DigiArp:** on a MIDI track, FUNC + KEYBOARD switches the arp on or off; then held keys are
  arpeggiated. See DigiArp's own
  [HARDWARE_CONTROLS.md](https://github.com/aleks-karachevtsev/Digitone-MIDI-Arpeggiator/blob/main/HARDWARE_CONTROLS.md).
- **Drum Mode:** on another MIDI track, hold its track key → DRUM MODE. See the
  [manual](../drum-mode/docs/MANUAL.md).

For example, an arp on MIDI A for a bass synth and drums on MIDI B for a drum machine, both
playing from the same pattern.

**Both on the same track:** Drum Mode's pads take the trig keys, so the arp gets no keys and
stays quiet on that track. Use separate tracks.

DigiArp adds no Mod Menu tile, so DRUM MODE is still the menu's only entry. The LEDs don't
clash either: DigiArp lights only the KEYBOARD key, Drum Mode only the 16 trig keys.

## Tested (emulator)

All in [digiemu](https://github.com/irpina/digiemu) with the combined firmware (DMA1):

| Test | Result |
| --- | --- |
| DigiArp's own 8 emulator tests (`tests/*27.py` in its repo: arp patterns of length 1–16, chord release, all modes' shortcut, 4 tracks, switching off mid-note, gate lengths, synth tracks untouched, 5 s stress at 300 BPM, saving to and loading from +Drive) | all pass, on both the original build and the combined one |
| Their output compared line by line, original vs combined | same notes and checks; differences are only timing jitter of a few ms, code addresses, and the saved project's checksum (it now holds Drum Mode's settings too) |
| [tests/test_with_drum_mode.py](tests/test_with_drum_mode.py): arp on A, Drum Mode on B, then both at once while playing, then both on one track | 13 of 13 pass |
| Drum Mode's 7 test sets | 119 of 119 pass |

See [NOTES.md](NOTES.md) for the numbers.

## Sharing

DigiArp is GPL-2.0-or-later, so this patch and the build are allowed. Still, the cleanest
route for everyone is the same rename in DigiArp itself, so the official version works with
core 5.3 and can be combined in the Mod Shop. Suggesting it to the author is the next step
before sharing a combined build more widely.
