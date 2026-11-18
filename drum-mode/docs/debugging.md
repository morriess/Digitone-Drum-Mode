# Debugging Drum Mode on the unit

Drum Mode 0.8 passed every test in the emulator, but on a real Digitone the beats it recorded
didn't play back (known bug 1 in the [README](../README.md)). This page is the log of how we're
tracking that down: the debug builds, how to use them, and what each test showed.

## The debug builds

A debug build is the current Drum Mode (now 0.8.1, with the fix) with one extra line at the bottom of the screen. It shows what
Drum Mode's code sees while the pattern plays. Otherwise it works exactly like the normal build.

The changes live in [`debug/`](../debug) as patches against `src/`, so anyone can build the same
file from their own OS file (never share the result, same as the normal build):

    ./debug/build-debug.sh 1 /path/to/Digitone_and_Digitone_Keys_OS1.44.syx    # DRMD
    ./debug/build-debug.sh 2 /path/to/Digitone_and_Digitone_Keys_OS1.44.syx    # DRD2
    ./debug/build-debug.sh 3 /path/to/Digitone_and_Digitone_Keys_OS1.44.syx    # DRD3

The boot screen shows which one is running. The line appears on a Drum Mode track on the normal
pages (not the DRUM pages or menus) and updates while the pattern plays. Flash the normal build
(DRM8) back when done.

### Debug 1 (DRMD)

    G41 F4 I67/1 O67 V100>85        (the emulator, the test below)

- **G**: how many steps the sequencer has gone through on this track.
- **F**: how many of those steps reached Drum Mode with a note on them.
- **I67/1**: that step's first note, and how many notes it had.
- **O67**: the note Drum Mode passes on.
- **V100>85**: the velocity before and after Drum Mode (85: the lane's velocity).

I, O and V change only when F goes up, so they show the last step that counted.

### Debug 2 (DRD2)

    F4/4 N67 S12 W11 K67/67        (the emulator, the test below)

- **F4/4**: steps that reached Drum Mode with a note / all steps that reached Drum Mode,
  including those with no note.
- **N67**: the note the last step arrived with; -1 means no note.
- **S12**: the step Drum Mode last recorded into (1-16).
- **W11**: that step's stored word now, in hex. Bit 0 (an odd number) is the trig.
- **K67/67**: that step's note just after recording / that step's note now.

### Debug 3 (DRD3)

Two lines at the top of the screen:

    F4/4 N67 K0 O3780
    PE354 AE354 B4471 T0000         (the emulator on A01)

- **F4/4 N67**: as in debug 2.
- **K0**: the set of track edit objects Drum Mode writes through: on 0.8.2 the pattern's number
  (0 = A01 ... 127 = H16; 1000 + n if it had to search), **-1** if none points into the playing
  pattern. (Built on 0.8.1 it was 0 or 1, and -1 meant neither of the first two.)
- **O3780**: the object the Digitone's own trig edit used last (low 4 hex digits of its
  address), 0000 until you place a trig the normal way.
- **P**: where the playing pattern's data for this track is (low 4 hex digits).
- **A, B**: where A01's and A02's sets point.
- **T**: where the object of the last normal trig edit points.

## The test

The same steps for every debug build:

1. Flash the debug build and check the boot screen.
2. Turn Drum Mode on for a MIDI track (hold its track key, Mod Menu, Drum Mode), with the kick
   lane on note 67.
3. On an empty pattern, live record a few kicks (RECORD + PLAY, hit the kick's key), then STOP.
4. PLAY again and let it run a bar or two.
5. Photograph the bottom line.

Optionally, the same with a trig placed the normal way (trig keys, with Drum Mode off), then
Drum Mode back on.

## Results

### 18 Nov 2026, debug 1 on the unit

- Recorded beats: **G counts, F stays 0.** The sequencer goes through the steps, but none of the
  recorded steps reaches Drum Mode with a note.
- Trigs placed the normal way, with Drum Mode back on: **F counts, I67 O67.** Drum Mode's playback
  side works on the unit: it receives normal trigs and passes the note on unchanged.

So the problem is in what Drum Mode's recording stores, not in its playback. (Holding a recorded
step shows ROOT C5 in the emulator too, where it plays: ROOT doesn't show the step's note, so that
isn't a clue.)

The emulator, same session: `G41 F4 I67/1 O67 V100>85` and the four notes on the DIN wire. The
emulator's recording stores the trig and the note (slot 0 = 67), and the unit's apparently doesn't.

### 18 Nov 2026, debug 2 on the unit

`F0/0 N-9 S13 W1 K67/67`: Drum Mode recorded into step 13, and that step holds a trig (W1)
with note 67, unchanged since recording (K67/67). But no step ever reached Drum Mode's playback,
not even without a note (F0/0, N still at its start value -9). So the recorded data was right,
but the sequencer was reading a different copy of the pattern.

### The cause, found in the emulator

Drum Mode wrote through the firmware's track edit objects (`fw_project + 0x11f4 + 0x58 *
track`); their data pointer (+16) says which pattern they edit. The project holds two such sets
(the second at `+ 0x1580`), one for each of the firmware's two pattern buffers, and which set
edits the active pattern swaps with pattern changes. On A01, where every emulator test ran, the
first set is the right one. After switching to A02 in the emulator, the first set still pointed
into A01, while the stock trig keys wrote through the second set (found by hooking the stock
trig setter). Recording there put the beats in A01 and playback sent nothing, as on the unit.

**Fix (0.8.1):** Drum Mode uses the set whose data pointer is the active pattern's track
(`fw_pattern + 976 * track`). `tests/test_other_pattern.py` records on A02: it fails on 0.8 the
way the unit did, and passes on 0.8.1; all the other tests still pass.

### Next: 0.8.1 on the unit

Build 0.8.1 (shows as **DR81**) and run the test above on the pattern you were on. The beats
should play back.

### 18 Nov 2026, 0.8.1 on the unit

Still no sound: after live recording the steps show on the grid, but they don't play. So the
two-sets fix alone doesn't cover what happens on the unit. *Next:* debug 3 (DRD3), on the same
pattern: live record and PLAY (photo), then place a trig the normal way with Drum Mode off and
turn Drum Mode back on (photo). It shows which object the unit's own trig keys use, where it
points, and where Drum Mode writes.

### 18 Nov 2026, debug 3 (on 0.8.1) on the unit

On pattern H02, MIDI track D:

    1. after live recording:            F0/0 N-9 K-1 O0000 PCC91 AEEC4 B4FE1 T0000
    2. a normal trig, Drum Mode on:     F1/1 N67 K-1 O0000 PCC91 AEEC4 B4FE1 T0000

Worked out from the addresses: P is H02's data for track 8, A is A01's and B is A02's. So
neither set Drum Mode looked at was the playing pattern's (K-1): it wrote into A01. The normal
trig plays (F1/1), but the trig-edit listener didn't see it (O and T stayed 0000), so maybe it
was placed before, or another way.

### The real cause, found in the emulator

Switched to H02 in the emulator (BANK + trig 16, then trig 2) and hooked the stock trig setter:
the stock grid edit wrote through an object at `fw_project + 0x11f4 + 0x38c * 113 + 0x58 *
track`. 113 is H02's number. The project holds **one set of track edit objects per pattern**,
128 sets 0x38c bytes apart, each pointing into its own pattern; there are no "two buffers". 0.8.1
looked at A01's and A02's sets only, which is why it fixed A02 in the emulator and not H02 on
the unit.

**Fix (0.8.2):** Drum Mode takes the active pattern's own set (its number from `fw_pattern`),
checked by its data pointer, and searches all 128 if that doesn't match.
`tests/test_other_pattern.py` now records on H02: it fails on 0.8.1 as on the unit, and passes
on 0.8.2.

### 18 Nov 2026, 0.8.2 on the unit: fixed

On H02, live recorded beats now play back. Solved.

What it took: the emulator only ever ran on A01, where the bug can't show. Debug 1 placed the
fault in recording, debug 2 showed the stored step was right but never played, debug 3's
addresses showed which pattern Drum Mode was writing into, and the emulator, set up on the same
pattern, showed the object the stock uses. The debug builds stay in `debug/` for next time.

### For reference: what debug 2's readings meant

Debug 2 tells apart why the recorded steps don't count:

| Bottom line | Meaning |
|---|---|
| The second F number counts, N is -1 | The trig is stored but not the note: the step plays nothing |
| K reads 67/-1 or another number | The note is written, then something replaces it |
| Both F numbers stay 0 | The sequencer doesn't see the trig at all: W shows what was stored |

## Other ways to look

- **A MIDI monitor** on the computer (Snoize's MIDI Monitor on macOS), with the Digitone's
  OUTPUT TO including USB, shows exactly what the unit sends.
- **The emulator's limits:** what digiemu can and can't check is in
  [design.md, "What the emulator can't show"](design.md#what-the-emulator-cant-show). In short,
  the main CPU's path to the DIN jack is checked down to the bytes on the wire, so whatever differs
  on the unit is outside it (the boot flags' paths, timing, or the unit's own project).
