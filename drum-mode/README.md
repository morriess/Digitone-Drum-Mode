# Drum Mode

A custom-firmware mod for the **Elektron Digitone mk1 (OS 1.44)** that turns one
MIDI track into a drum track with up to **8 lanes**. Each lane sends its own note
on the track's single MIDI channel, so you can play and program several drum
notes at once, and see each lane trigger.

Inspired by the drum sequencers of the Elektron Digitakt and the Arturia
BeatStep Pro. Built on [elekloader](https://github.com/irpina/elekloader)'s
Digitone mk1 core and its sequencer-events mod.

> **Beta: working on the unit.** First hardware tests on OS 1.44 (2026-11-17/18, 0.8.2): pads,
> notes, velocities, DRUM pages, mutes and recorded beats work. The features added since (0.9 to
> 0.10) are tested in the emulator and wait for the unit; see [Known bugs](#known-bugs) and
> [docs/debugging.md](docs/debugging.md).

**Docs:** the [user manual](docs/MANUAL.md) (installing, every feature, troubleshooting), the
[UI additions](docs/ui/README.md) (every screen, key and LED Drum Mode adds), the
[design notes](docs/design.md) (how it works inside, findings) and the
[debugging log](docs/debugging.md).

## What it does (0.10)

- **Drum Mode per MIDI track and per pattern:** hold the track key → Mod Menu → DRUM MODE.
- **8 drum pads** on trig keys 1–8, each lane with its own note and velocity, all on the
  track's MIDI channel (GM notes to start with); a pad also selects its lane.
- **Live recording** writes the pads into the pattern as normal MIDI trigs (several drums on
  one step become the trig's chord notes).
- **Grid editing per lane:** in GRID RECORDING the 16 trig keys place and show the selected
  lane's steps; a held step takes the stock's locks, micro timing and copy/paste.
- **Lane mutes** with FUNC + pad (outside recording).
- **Two DRUM pages** behind the MIDI track's LFO key for all lanes' notes and velocities.
- **On screen and LEDs:** a lane strip that follows playback, the selected lane's pad lit, and
  keys 9–16 as a running light in time with the pattern.
- **Settings per pattern and per track,** kits (notes and velocities) shared in the project,
  at most 10; a new pattern takes a copy of the one you came from (being tried out).
- **The stock's own trig key functions stay:** pattern and bank select, chains, FUNC + trig in
  recording.

## Plans

**Nice to have**
- **Screen overview of all lanes:** an 8 × 16 grid, e.g. shown while TRIG is held, the
  selected lane highlighted.
- **A revised DRUM settings page** (its place behind LFO, layout, what it shows), after use on
  the unit.
- **Drum kit presets:** save a track's setup as a kit and load it onto any MIDI track. Within a
  project this fits the 0.10 kits; across projects it needs storage outside the project (a SysEx
  dump to the computer and back would be the workaround).
- **Synth tracks as extra drum tracks, or as normal MIDI tracks:** a synth track you don't need
  sends its steps as MIDI (its voice silent), for up to 8 drum or MIDI tracks. Not more tracks:
  the format has exactly 4 synth + 4 MIDI. The hard part is timing each note and its note-off.

**Possible additions after v1** (ideas from 2026-11-19, once the current features work on the
unit)
- **Ratchets / rolls per step:** a step plays 2, 3 or 4 times within its 16th, for hi-hat rolls
  and snare fills. The Digitone mk1 has no retrigs (the Digitakt does), so this fills a real
  gap. Needs notes sent between steps, which DigiArp shows is possible.
- **Note repeat on the pads:** hold a pad and it repeats at 1/8, 1/16 or 1/32 in time with the
  tempo (MPC-style), and is recorded during live recording.
- **Euclidean / fill generator per voice:** pick a voice, set e.g. 5 hits over 16 steps and an
  offset, and Drum Mode writes the steps.
- **Different lengths per voice:** e.g. kick over 16 steps, hats over 12, percussion over 7, for
  polyrhythms. Ambitious: a Digitone track has a single length.
- **Probability and humanize per voice:** e.g. hats play 70% of the time, or small random
  velocity and timing shifts. The stock trig conditions work per step, not per voice.

**To decide after testing**
- A new pattern copies the last pattern's Drum Mode settings (the user's choice, 2026-11-19), or
  starts from the defaults.

**Done** (asked by the user): a pad selects its lane, lit (0.9); keys 9–16 as a running light
(0.9.1); settings per pattern and per track (0.10).

## Known bugs

**Open**
- **Pattern copy/paste with PTN + trig key** (then RECORD / STOP) on a Drum Mode track: not
  checked; the trig keys may play pads, and the Drum Mode settings don't follow this route yet
  (FUNC + RECORD / FUNC + STOP does both).
- **MUTE mode** (FUNC + BANK, then the trig keys mute tracks) and **STEP RECORDING** (RECORD +
  STOP) on a Drum Mode track: not checked; the trig keys may play pads (in step recording that
  may even be wanted).
- **Trig keys while a menu is open** go to Drum Mode on a Drum Mode track. Not decided; first
  to find out whether any stock menu uses the trig keys.
- **The DRUM pages clash with DigiTables** (both claim mod page 27). DigiTables is OS 1.43 only
  so far. *Possible fix:* pages 28 and 29 (29 clashes with CTRL-PLUS, which needs an older
  core anyway; 30 is core's second TRIG page once a mod puts knobs there).
- **ROOT on a held step shows the step's first note, not the selected lane's** (later). Since
  0.9.2 a step held in GRID RECORDING shows its note in the TRIG page's ROOT box, but on a step
  with several drums it's the first one (e.g. the kick, with the snare selected). *Planned:* draw
  the selected lane's note in the ROOT box while a step is held. Tried on 2026-11-19 (it worked
  in the emulator) and set aside for later.
- **The UI froze with the Mod Menu open (emulator only, cause unknown),** in one exact sequence
  of `tests/test_drum_off.py`. *Next:* watch for it on the unit. Details in
  [docs/design.md](docs/design.md) ("Open: a UI hang in the emulator").

**To check on the unit**
- **The lane strip sometimes disappeared when switching pages** (0.8). Probably the same cause
  as the empty DRUM pages, fixed in 0.8.3.
- **The 1.5-second rule (0.9.2):** after a bank pick (BANK + trig 16) the emulator never sent
  that key's release, so a trig key Drum Mode left to the stock counts as released after 1.5 s
  without events. Check that the release really goes missing on the unit, that the trig keys
  come back to Drum Mode after a bank pick, that a chain held past 1.5 s still works, and
  whether a cleaner way exists (the firmware's own held-key state).
- Everything from 0.9 to 0.10.

**Known limits**
- A **NOTE lock** on a drum step changes its notes; a **velocity lock** is kept, but Drum Mode
  plays the step at its lanes' velocity, so it has no effect.
- **At most 10 kits** per project (`KIT MEM FULL`), and the per-pattern settings take one of a
  pattern's 80 lock rows.
- **Saving on the stock OS** drops Drum Mode's settings (the steps stay).

**Fixed** (details in the [debugging log](docs/debugging.md) and the
[design notes](docs/design.md))
- Recorded beats didn't play back on the unit: written into another pattern's data (0.8.2,
  confirmed on the unit).
- The DRUM pages came up empty and the corner strip vanished after picking a pattern with BANK
  or PTN: an empty firmware view stayed on top (0.8.3).
- Pattern and bank select, chains and held grid steps didn't reach the stock on a Drum Mode
  track (0.9.2); FUNC + pad muted a lane while recording (0.9.3).
- Step 1 after PLAY followed the setting from before a change made while stopped (0.9.4).
- Per-pattern settings weren't saved: the first design didn't fit core's 480 bytes of project
  data (redone before release, 0.10).

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
**D100** on the unit.

**Never commit Elektron's OS files or built firmware to this repo**: `.gitignore`
blocks `*.syx`, `*.bin` and `*.elemod`.

## Testing in the emulator

The tests run the real firmware in [digiemu](https://github.com/irpina/digiemu),
press keys and check the MIDI the Digitone sends:

```
git clone https://github.com/irpina/digiemu ~/digiemu
cd ~/digiemu && uv sync && tools/install-patched-unicorn.sh
for t in tests/test_*.py; do
    DIGIEMU=~/digiemu uv run --project ~/digiemu python "$t" out/Digitone_OS1.44_DrumMode.syx
done
```

Each test set takes a few minutes; screenshots land in its work folder. All 13 sets
(210 checks) pass on 0.10.

## Layout

| Path | What |
| --- | --- |
| `src/mod.json` | Mod manifest: events it subscribes to, what it needs |
| `src/drummode.c` | The mod |
| `build.sh` | Builds the firmware from your stock OS file |
| `tests/emu_session.py` | Drives the firmware in digiemu: keys, MIDI out, screen |
| `tests/test_live_lanes.py` | The pads: notes, chords, note-offs, FUNC + pad, the corner (21 checks) |
| `tests/test_assign_notes.py` | Setting notes and velocities, saving with the project (21 checks) |
| `tests/test_live_record.py` | Live recording: step timing, chords, playback, no stray stock trigs (15 checks) |
| `tests/test_lane_mute.py` | Lane mutes: playback without muted lanes, chords, the strip (10 checks) |
| `tests/test_strip_playback.py` | The strip blinking on playback, on time; not over menus (11 checks) |
| `tests/test_grid_edit.py` | Lane select, grid editing with pages, the trig key LEDs (26 checks) |
| `tests/test_drum_page.py` | The DRUM pages: knobs set notes and velocities, the pages underneath untouched (15 checks) |
| `tests/test_drum_off.py` | A beat with Drum Mode on, off and on again: what plays, what's kept, step 1 at once (17 checks) |
| `tests/test_other_pattern.py` | Recording and the DRUM pages on a pattern far from A01 (H02) (7 checks) |
| `tests/test_lane_select.py` | A pad selects its lane, its key lit; not in live recording (15 checks) |
| `tests/test_run_light.py` | Keys 9–16 as the running light: 16, odd and longer lengths, on time (9 checks) |
| `tests/test_stock_keys.py` | Pattern/bank select, chains, held grid steps, FUNC + trig in recording (25 checks) |
| `tests/test_pattern_settings.py` | Settings per pattern: copy on first visit, paste, save/load, the 10-kit limit (18 checks) |
| `debug/` | The debug builds used on the unit (patches and a build script; see the debugging log) |
| `docs/MANUAL.md` | The user manual |
| `docs/design.md` | Design, build plan and findings |
| `docs/debugging.md` | The log of debugging on the unit |
| `docs/ui/` | The screen UI explained, with screenshots |

## Disclaimer

Unofficial, experimental firmware. Not affiliated with or supported by Elektron.
Back up your projects before flashing, and keep the stock OS file to restore.
