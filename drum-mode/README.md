# Drum Mode

A custom-firmware mod for the **Elektron Digitone mk1 (OS 1.44)** that turns one
MIDI track into a drum track with up to **8 lanes**. Each lane sends its own note
on the track's single MIDI channel, so you can play and program several drum
notes at once, and see each lane trigger.

Inspired by the drum sequencers of the Elektron Digitakt and the Arturia
BeatStep Pro. Built on [elekloader](https://github.com/irpina/elekloader)'s
Digitone mk1 core and its sequencer-events mod.

> **Status: milestone 2, live lanes.** Trig keys 1–8 play 8 drum lanes on a MIDI
> track. No step sequencing or saving yet. See [docs/design.md](docs/design.md)
> for the full design and build plan.

## Using it (milestone 2)

1. Select a MIDI track (hold MIDI, press a track key) and give it a channel: on its
   SRC page (SYN1), **FUNC + push encoder A** enables CHAN, then turn A to your
   drum machine's channel. With CHAN off (the default) nothing is sent.
2. **Hold a track key** to open the Mod Menu and pick **DRUM MODE** (YES or its
   trig key). The corner shows `DRUM A ON`.
3. Trig keys **1–8** now play the 8 lanes, several at once:

   | Trig | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
   | --- | --- | --- | --- | --- | --- | --- | --- | --- |
   | Lane | Kick 36 | Snare 38 | Closed hat 42 | Open hat 46 | Clap 39 | Low tom 45 | High tom 50 | Crash 49 |

   The screen's top-right corner shows 8 small boxes; a lane's box lights while it plays.
4. Pick DRUM MODE again to turn it off.

While Drum Mode is on for the selected track, trig keys 9–16 do nothing and you can't
place trigs on that track (step programming comes in milestones 3–5). FUNC + a trig key
still does what it normally does. Drum Mode is off again after a power cycle.

## What it will do

- Turn DrumMode on per MIDI track (A–D).
- Pick one of 8 lanes with a modifier + trig 1–8.
- GRID mode: the 16 trig keys show and edit the selected lane's steps.
- KEYBOARD mode: trig 1–8 play the 8 lanes, several at once; LIVE REC records them.
- Screen: an 8 × 16 grid of all lanes, with the playhead and a flash when a lane fires.
- All lanes send only on the MIDI track's own channel.

## Milestones

1. Groundwork: build setup on core 5.x, an empty mod that loads and changes nothing
2. Live lanes: trig 1–8 send lane notes in KEYBOARD mode
3. Lane playback through the sequencer hook
4. Lane storage in the pattern, drum map in the project
5. Grid editing, lane select and per-lane trig LEDs
6. Screen grid with playhead and lane flashes
7. Polish: lane mute, DRUM page, LIVE REC

## Building

You need your own copy of the stock Digitone OS 1.44 `.syx` from Elektron, plus
git, python3 and an m68k cross toolchain (`m68k-elf-gcc` or `m68k-linux-gnu-gcc`;
on Debian/Ubuntu: `apt install gcc-m68k-linux-gnu`).

```
./build.sh /path/to/Digitone_and_Digitone_Keys_OS1.44.syx
```

This fetches [elekloader](https://github.com/irpina/elekloader) at a fixed commit
(core-dn1 5.3, seq-events-dn1 0.1), builds them and the mod in `src/`, checks they
combine, and writes `out/Digitone_OS1.44_DrumMode.syx`, shown as OS version
**DRM1** on the unit.

**Never commit Elektron's OS files or built firmware to this repo**: `.gitignore`
blocks `*.syx`, `*.bin` and `*.elemod`.

## Testing in the emulator

The tests run the real firmware in [digiemu](https://github.com/irpina/digiemu),
press keys and check the MIDI the Digitone sends:

```
git clone https://github.com/irpina/digiemu ~/digiemu
cd ~/digiemu && uv sync && tools/install-patched-unicorn.sh
DIGIEMU=~/digiemu uv run --project ~/digiemu python tests/test_live_lanes.py out/Digitone_OS1.44_DrumMode.syx
```

The first run sets up the firmware (a few minutes); screenshots land in the work folder.

## Layout

| Path | What |
| --- | --- |
| `src/mod.json` | Mod manifest: events it subscribes to, what it needs |
| `src/drummode.c` | The mod |
| `build.sh` | Builds the firmware from your stock OS file |
| `tests/emu_session.py` | Drives the firmware in digiemu: keys, MIDI out, screen |
| `tests/test_live_lanes.py` | Milestone 2's tests |
| `docs/design.md` | Design and build plan |

## Disclaimer

Unofficial, experimental firmware. Not affiliated with or supported by Elektron.
Back up your projects before flashing, and keep the stock OS file to restore.
