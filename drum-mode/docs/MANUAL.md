# Drum Mode user manual

**Drum Mode 0.9.1 for the Elektron Digitone mk1 / Digitone Keys, OS 1.44**

Drum Mode turns a Digitone MIDI track into an 8-lane drum sequencer. Trig keys 1–8
become drum pads, each playing its own note on the track's MIDI channel, so you can play,
record and program a whole kit on one MIDI track. The screen and the trig key LEDs show
each drum as it plays.

> **Beta.** 0.9.1 works on a real Digitone (OS 1.44), with a few known bugs. Back up your
> projects before installing. The stock OS can always be reinstalled (see
> [Uninstalling and recovery](#uninstalling-and-recovery)).

## Contents

1. [What you need](#what-you-need)
2. [Installing](#installing)
3. [Uninstalling and recovery](#uninstalling-and-recovery)
4. [Quick start](#quick-start)
5. [Playing the pads](#playing-the-pads)
6. [Setting each drum's note and velocity](#setting-each-drums-note-and-velocity)
7. [Live recording](#live-recording)
8. [Programming steps on the grid](#programming-steps-on-the-grid)
9. [Muting a drum](#muting-a-drum)
10. [What the screen and LEDs show](#what-the-screen-and-leds-show)
11. [Saving, loading and the stock OS](#saving-loading-and-the-stock-os)
12. [Quick reference](#quick-reference)
13. [Troubleshooting](#troubleshooting)
14. [Limitations](#limitations)

## What you need

- An **Elektron Digitone mk1** or **Digitone Keys** on **OS 1.44**. Drum Mode works with
  that version only.
- A computer with **Elektron Transfer** and a USB cable, to install it.
- A drum machine, sampler or drum plugin that plays drums on one MIDI channel, connected
  to the Digitone's MIDI OUT (or over USB).
- The **stock OS 1.44 file** from Elektron (`Digitone_and_Digitone_Keys_OS1.44.syx`, from
  Elektron's Digitone downloads page). You need it to build the firmware and to go back
  to stock.

## Installing

Drum Mode is installed as a modified OS: your stock OS 1.44 with Drum Mode added. You
send it to the Digitone the same way as an Elektron OS update.

### 1. Get the firmware

There is no download: the firmware contains Elektron's OS, so nobody shares it, not this
project either. You build it from your own copy of the stock OS file.

- **Build it yourself** (Linux, macOS or Windows with WSL). You need git, python3 and an
  m68k cross compiler (on Debian/Ubuntu: `sudo apt install gcc-m68k-linux-gnu`). Then:

  ```
  git clone https://github.com/morriess/Digitone-mods.git
  cd Digitone-mods/drum-mode
  ./build.sh /path/to/Digitone_and_Digitone_Keys_OS1.44.syx
  ```

  The firmware appears as `out/Digitone_OS1.44_DrumMode.syx` (it shows as OS version
  **D091**).
- **Later: the elekloader Mod Shop.** Once Drum Mode is listed there, you'll be able to
  tick it in the [Mod Shop](https://elekloader.github.io/shop/) and build the firmware in
  your browser from your own stock OS file.

The file you build is for your own Digitone: don't upload or pass it on, since it contains
Elektron's OS.

### 2. Back up

In Elektron Transfer, back up your projects and sounds. An OS update normally keeps
them, but a backup costs a minute and covers you if anything goes wrong.

### 3. Send it to the Digitone

1. Connect the Digitone to your computer over USB and open **Elektron Transfer**.
2. Select the Digitone and press **Connect**.
3. Drag `Digitone_OS1.44_DrumMode.syx` onto **Drop files here**.
4. Press **YES** on the Digitone when it asks.
5. Wait until the update finishes and the Digitone restarts. **Don't switch it off
   during the update.**

### 4. Check it

The Digitone starts as normal, and the OS version on its boot screen now reads **D091**
instead of 1.44.
Hold any track key on its own: a **Mod Menu** opens with a **DRUM MODE** tile. Press
**NO** to close it.

## Uninstalling and recovery

**To go back to stock:** send the stock `Digitone_and_Digitone_Keys_OS1.44.syx` the same
way as above. Your projects stay.

**If the Digitone won't start** after installing (unlikely, but this is beta software):

1. Switch it off.
2. Hold **FUNC** while switching it on. The startup menu appears.
3. Press **TRIG 4** for **OS UPGRADE**.
4. Send the stock OS 1.44 `.syx` with Elektron Transfer (its legacy OS upgrade mode).

The Digitone's startup menu and bootloader are never changed by a mod, so this always
works.

## Quick start

Your first beat in five steps:

1. **Pick a MIDI track:** hold **MIDI** and press **T1**. The track box on the left
   shows `MID 1` (MIDI track A).
2. **Give it a channel:** press **SYN1** (the MIDI Source page). If **CHAN** shows an X,
   hold **FUNC** and push encoder **A** to turn it on, then turn **A** to your drum
   machine's channel (10 for most GM drum kits).
3. **Turn on Drum Mode:** hold **T1** on its own until the Mod Menu opens, then press
   **YES** on **DRUM MODE**. The corner of the screen says `DRUM A ON`.
4. **Play:** trig keys **1–8** are now drum pads. Hit several at once.
5. **Record:** hold **RECORD** and press **PLAY**, play a beat on the pads, and press
   **STOP**. Press **PLAY** to hear it back.

## Playing the pads

With Drum Mode on, trig keys 1–8 are the eight drum lanes:

| Trig key | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Drum | Kick (BD) | Snare (SD) | Closed hat (CH) | Open hat (OH) | Clap (CP) | Low tom (LT) | High tom (HT) | Crash (CY) |
| Note | 36 | 38 | 42 | 46 | 39 | 45 | 50 | 49 |

These are the General MIDI drum notes, so most drum machines play the right sounds
straight away. If yours uses other notes, change them (next section). The names are only
labels: any lane can play any note.

- A pad sends a note-on when pressed and a note-off when released, on the MIDI track's
  channel, at that lane's velocity.
- Play as many pads at once as you like.
- Trig keys **9–16** do nothing outside GRID RECORDING.
- Drum Mode is set per MIDI track: you can have it on for MIDI A and off for MIDI B.
  To turn it off, choose DRUM MODE in the Mod Menu again (`DRUM A OFF`).

## Setting each drum's note and velocity

There are two ways: one drum at a time with a held pad, or all eight on the DRUM pages.

### With a held pad

Hold a pad and turn:

- **A** for its **note**. Each new note plays as you turn, so you can find the right
  sound on your drum machine by ear.
- **B** for its **velocity** (1–127).

The corner shows the pad, its note and velocity, e.g. `BD 38 V100`. While you hold the
pad, the knobs only change the pad, not the page underneath.

### On the DRUM pages

Press **LFO**, then press **LFO** again: after the LFO page come **Drum Notes** and
**Drum Velocity**.

![The Drum Notes page](ui/drum-notes-page.png)

The eight knobs match the eight cells:

|  | Knob A | Knob B | Knob C | Knob D |
| --- | --- | --- | --- | --- |
| **Top row** | BD | SD | CH | OH |
|  | **Knob E** | **Knob F** | **Knob G** | **Knob H** |
| **Bottom row** | CP | LT | HT | CY |

- **Drum Notes** shows each lane's note name and number (the Digitone's naming, where 60
  is C5). Turning a knob plays each new note.
- **Drum Velocity** shows each lane's velocity. A muted lane also says `MUTE`.
- The **selected lane** (see [Programming steps on the grid](#programming-steps-on-the-grid))
  has its cell inverted.

On a MIDI track without Drum Mode these pages say `DRUM MODE IS OFF`, and the knobs do
nothing there.

Notes and velocities belong to the track and are saved with the project. Velocity sets
how hard a lane plays, both when you hit its pad and when the sequencer plays it.

## Live recording

1. Hold **RECORD** and press **PLAY**.
2. Play the pads. Each hit is written to the nearest step: a hit in the first half of a
   step lands on that step, a hit in the second half on the next one.
3. Press **STOP** when you're done.

- Several drums on the same step become one trig with several notes (up to 8, so all
  pads fit on one step).
- When a step plays several drums, it plays them at the loudest of their velocities.
- Recording always snaps to the nearest step. The Digitone's own quantize setting for
  live recording doesn't apply to the pads.
- Recorded steps are normal MIDI trigs: they copy, clear and save like any trig.

## Programming steps on the grid

You can also place drums step by step, one drum at a time, like on a Digitakt.

1. **Select a drum:** press its pad (1–8). You hear it, and its key stays lit **dim
   white** (red if the drum is muted), with a small line under its box in the corner
   strip. During live recording a pad doesn't change the selection. To select a drum
   without hearing it, hold the **TRIG** page key and press its pad (the corner says, for
   example, `EDIT CH`).
2. **Enter GRID RECORDING:** press **RECORD** while the sequencer is stopped.
3. **Place steps:** the 16 trig keys are now the selected drum's 16 steps. Press a key to
   put the drum on that step; press it again to take it off. Keys light **red** where the
   selected drum plays.
4. **Switch drums:** leave GRID RECORDING and press another pad, or use TRIG + pad at any
   time. The keys then show the new drum's steps.
5. **Longer patterns:** press **PAGE** to reach steps 17–32, 33–48 and 49–64, as usual.
6. Press **RECORD** again to leave GRID RECORDING.

![The trig key LEDs in GRID RECORDING](ui/leds-grid.png)

- Other drums on the same steps stay where they are, and don't light up while another
  drum is selected.
- Taking the last drum off a step removes the step's trig.
- Grid editing and live recording write steps the same way, so you can mix them.
- On a Drum Mode track you can't p-lock a step by holding its trig key in GRID
  RECORDING, because the keys belong to the selected drum there.

## Muting a drum

Hold **FUNC** and press a pad to mute that drum; do it again to unmute it.

- The pattern plays on without the muted drum, and its steps stay in the pattern.
- On a step with several drums, only the muted one drops out.
- The corner says `BD MUTE` / `BD ON`. In the corner strip a muted drum shows as a dash,
  its pad lights dim red, and its cell on Drum Velocity says `MUTE`.
- The pad itself still plays (and records) when you hit it.
- Mutes are saved with the project.

## What the screen and LEDs show

### The corner strip

In the top-right corner of the screen, eight tiny boxes show the eight drums, in pad
order:

![The corner strip's looks, enlarged](ui/strip-zoom.png)

- **Outline:** the drum is on, not playing right now.
- **Filled:** its pad is held, or the sequencer just played it (a short blink, timed to
  the sound).
- **Dash:** muted.
- **Line under a box:** the selected drum.

Short messages (`DRUM A ON`, `BD MUTE`, `EDIT CH` …) replace the strip for about 2
seconds. While you hold or edit a pad, the corner shows its note and velocity instead.

### The trig key LEDs

Outside GRID RECORDING, pads 1–8 light **white** while held or when their drum plays,
and **dim red** when muted. The selected drum's pad stays **dim white** (**red** if it's
muted).

While the sequencer plays, keys **9–16** are a running light that shows where the
pattern is: each key stands for 2 steps and blinks **white** on each of them, so on a
16-step pattern key 9 blinks for steps 1 and 2, key 10 for steps 3 and 4, and so on up to
key 16. On a longer pattern the keys follow the page that plays: steps 17–32 start again
at key 9. On a shorter or odd length the keys after the last step stay dark, and a key
with only one step blinks once (13 steps: keys 9–14 twice, key 15 once). Dark when
stopped, and in GRID RECORDING keys 9–16 are steps as usual.

![The trig key LEDs while playing](ui/leds-pads.png)

In GRID RECORDING the keys show the selected drum's steps in **red** (see above). The
step currently playing shows the Digitone's normal playhead.

More detail, with every screen: [Drum Mode UI additions](ui/README.md).

## Saving, loading and the stock OS

- **Saved with the project:** which MIDI tracks have Drum Mode on, each track's 8 notes
  and velocities, and the mutes.
- **Saved in the patterns:** the steps themselves, as normal MIDI trigs.
- **A project made without Drum Mode** opens with Drum Mode off and the GM notes.
- **Back on the stock OS**, your projects still load, and the patterns play their steps as
  normal MIDI trigs with chords. Mutes and Drum Mode's settings don't apply there, but the
  stock OS keeps them, so they're back when you reinstall Drum Mode.

### Turning Drum Mode off

Turning Drum Mode off for a track (Mod Menu → DRUM MODE) leaves its pattern untouched, and
the Digitone keeps playing it as ordinary MIDI trigs. Tested in the emulator:

- **The same notes play**, so your drum machine plays the same drums.
- **Mutes stop applying:** a muted drum plays again. The mutes are kept, and work again when
  you turn Drum Mode back on.
- **Velocities change** to the trig's own velocity (VEL on the TRIG page, 100 by default).
  With Drum Mode on, each step plays at the loudest velocity among its drums.
- **The trig keys** are normal trig keys again.
- **The first step after PLAY** can still follow the old setting. The Digitone prepares each
  step one step ahead, and step 1 of a new run was prepared when the previous run stopped.
  So right after turning Drum Mode off, on, or changing a mute or velocity while stopped,
  step 1 of the next run plays the old way; from step 2, and from the following run on,
  everything follows the new setting.

## Quick reference

On a Drum Mode track:

| Action | How |
| --- | --- |
| Turn Drum Mode on/off for this MIDI track | hold a track key → **DRUM MODE** |
| Play a drum | trig keys **1–8** |
| Change a drum's note / velocity | hold its pad + turn **A** / **B** |
| Set all notes / velocities | **LFO**, **LFO** → Drum Notes, **LFO** → Drum Velocity; knobs **A–H** |
| Mute / unmute a drum | **FUNC** + its pad |
| Select a drum for grid editing | its pad (or hold **TRIG** + its pad: silent) |
| Program the selected drum's steps | **RECORD** (stopped), then trig keys **1–16**; **PAGE** for more steps |
| Live record | hold **RECORD** + **PLAY**, play the pads, **STOP** |

## Troubleshooting

**The pads make no sound on my drum machine.**
- Check **CHAN** on the MIDI track's SYN1 page (MIDI Source). With CHAN off (an X),
  nothing is sent: FUNC + push A turns it on, then set the channel with A.
- Check the Digitone's MIDI output settings: in **SETTINGS > MIDI CONFIG > PORT CONFIG**,
  OUTPUT TO should include the port you use (MIDI, USB, or both).
- Check that your drum machine listens on that channel.

**The wrong drums play.**
Your drum machine uses other notes than General MIDI. Set each pad's note on the Drum
Notes page or with pad + A, by ear.

**`MIDI TRK ONLY` appears when I choose DRUM MODE.**
A synth track is selected. Select a MIDI track first (hold MIDI, press a track key).

**The trig keys do nothing on my track.**
Drum Mode may be on but the track has no channel set: see the first answer. Or you're
in GRID RECORDING (keys edit steps, they don't play): press RECORD to leave it.

**Trig keys 9–16 don't work.**
Outside GRID RECORDING they're unused on a Drum Mode track. FUNC + 9–16 still do their
stock jobs.

**The Digitone won't start after installing.**
Reinstall the stock OS from the startup menu: see
[Uninstalling and recovery](#uninstalling-and-recovery).

**Something else behaves oddly.**
Please report it, ideally with a photo of the screen, at the repository's
[issues page](https://github.com/morriess/Digitone-mods/issues).

## Limitations

- **OS 1.44 only.** Drum Mode uses firmware addresses that only fit OS 1.44. A new
  Elektron OS needs a new Drum Mode build.
- **Beta.** Tested in an emulator and on a unit (0.8.2, 0.8.3; 0.9's lane select and 0.9.1's running light not yet); not everything has been tried on hardware yet. The LED colours in
  particular haven't been checked on a real unit.
- **The corner strip covers the tempo** in the top-right corner of the main screen while
  Drum Mode is on.
- **All MIDI tracks** get the two DRUM pages in their LFO list, including tracks without
  Drum Mode.
- **No p-locks by holding a trig key** in GRID RECORDING on a Drum Mode track.
- **Combining with DigiArp MIDI** needs the rebuilt DigiArp in this repo's
  [digiarp-core5](../../digiarp-core5/) folder: the Mod Shop's version is built on an older
  elekloader core and can't go into one firmware with Drum Mode. Other combinations are
  untested.

---

Drum Mode is free software (GPL-2.0-or-later), made by morriess and built on
[elekloader](https://github.com/irpina/elekloader). It's an unofficial modification, not
made or supported by Elektron. Using it is at your own risk.
