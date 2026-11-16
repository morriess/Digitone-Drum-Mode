# Digitone DrumMode: design groundwork

As of 2026-11-15. Living version of this document: https://claude.ai/artifact/GM6crrTZ1ZKSKXTZcRedJm

## Goal and scope

DrumMode turns one Digitone mk1 MIDI track into a drum track with up to 8 lanes,
each lane sending its own note on that track's single MIDI channel. The point is
to make playing and programming several drum notes at once easy, with clear
visual feedback of which lanes trigger.

**In scope for the first version**

- Up to 8 lanes per drum-mode MIDI track (kick, snare, hats and so on), each with its own MIDI note.
- All lanes send only on the MIDI track's own channel.
- Live playing: trig keys play lanes, several at once.
- Step programming: the 16 trig keys show and edit the selected lane's steps.
- Visual feedback: the screen shows all lanes and flashes a lane when it fires.
- Saved with the pattern, like normal trigs.

**Out of scope for now**

- More than 8 lanes, or lanes on different channels.
- Per-lane track length or speed (BeatStep Pro style polyrhythm).
- Changes to the synth tracks 1–4.
- Digitone II and Digitone Keys-specific features. Target is Digitone mk1 on OS 1.44.

## Inspiration: Digitakt and BeatStep Pro

Both machines show one lane (or track) at a time on the 16 step keys, and use a
modifier to pick which one. DrumMode borrows that, and adds a whole-pattern view
on the screen.

| | Digitakt | BeatStep Pro drum sequencer | DrumMode takes |
| --- | --- | --- | --- |
| Lanes | 8 audio + 8 MIDI tracks, one shown on the trig keys at a time | 16 drum instruments on one track, on the pads | 8 lanes on one MIDI track |
| Picking a lane | Hold TRK, press a trig key | Drum + pad selects an instrument without triggering it | Hold a modifier, press trig 1–8 |
| Step keys | Show the selected track's trigs | Show the selected instrument's steps | Show the selected lane's steps |
| Live play | Trig keys play the active track | Pads play each instrument | Trig 1–8 play the 8 lanes at once |
| Muting | Mute mode per track | Drum + Mute + pad mutes one drum track | Modifier + key mutes a lane |
| MIDI | One channel per track | The drum track sends on one channel, with a note per instrument | One channel, a note per lane |

## How it works for the player

Key combinations are proposals, to be checked against what the Digitone mk1 leaves free.

1. **Turn it on.** Select a MIDI track (A–D) and use a shortcut to toggle DrumMode.
2. **Pick a lane.** Hold the modifier and press trig 1–8 to select lane 1–8, without sounding it.
3. **Program steps.** In GRID mode the 16 trig keys show the selected lane's steps. Press a key to toggle that step for this lane only.
4. **Play live.** In KEYBOARD mode trig 1–8 play the 8 lanes, each its own note. With LIVE REC on, hits are recorded into their lanes.
5. **Set up lanes.** A DRUM page sets each lane's note and default velocity with the encoders.
6. **Mute a lane.** Modifier + another key toggles a lane's mute.

**Visual feedback**

- Screen: an 8 × 16 grid, one row per lane. Selected lane highlighted, moving playhead, a lane's name flashes when it fires.
- Trig keys: lit for the selected lane's steps; the playing step blinks as usual.
- In KEYBOARD mode, keys 1–8 light up as their lane fires.

## Data model and MIDI output

| Item | Per | Size | Notes |
| --- | --- | --- | --- |
| Lane steps (on/off) | pattern, track, lane, step | 8 lanes × 64 steps = 64 bytes per track | Follows the track's own length |
| Lane note | project, track, lane | 8 bytes | Default GM map: 36 kick, 38 snare, 42 closed hat, 46 open hat, 39 clap, 45 low tom, 50 high tom, 49 crash |
| Lane velocity | project, track, lane | 8 bytes | One velocity per lane in v1 |
| Lane mute | project, track | 1 byte | Bit per lane |
| DrumMode on/off | pattern, track | 1 bit | Off = the track behaves stock |

- All lanes send on the MIDI track's channel, nothing else.
- On a step, all active and unmuted lanes fire together, up to 8 notes.
- Note length: the track's LEN for v1; note-offs are always sent.
- Per-step velocity / accents: v2, since the sequencer hook gives one velocity per step.

## Technical feasibility

| Need | Building block | Status |
| --- | --- | --- |
| Fire up to 8 notes on a MIDI track step | seq-events-dn1: up to 8 notes per step; generated trigs | Exists |
| Live play: trig keys send lane notes | Keyboard note hooks (0x4003b198 / 0x4003b26e, OS 1.44) | Proven by DigiArp |
| Trig LEDs per lane | core-dn1 `ev_key_leds` (since 3.3) | Exists |
| DRUM page with encoders | core mod pages and knobs | Exists |
| 8 × 16 grid on screen | custom drawing on a mod page | To investigate |
| Lane settings saved with the project | core project data (480 bytes shared by all mods) | Fits (~70 bytes) |
| Lane steps saved per pattern | trig + mod parameter (core 5.1 lane parameters) | Main design question |

**Saving the lane steps.** Keep a normal trig on every step where at least one
lane plays, and store which lanes play as two mod parameters on that trig (a
p-lock value holds about 7 bits). Patterns then save, load, copy and clear like
stock. To verify: copy/paste behaviour of those parameters.

**Combining with DigiArp.** DigiArp (from the elekloader Mod Shop) is built on
core 4.1; DrumMode needs core 5.3. One firmware takes one core, so combining
them needs a test build.

## Open questions

- Which drum machine or synth, and which notes does it expect?
- One velocity per lane, or an accent lane in v1?
- Which modifier for picking a lane?
- Note length: track LEN or always short?
- DigiArp + DrumMode in one firmware, or DrumMode-only first?
- License?

## Build plan

1. **Groundwork.** Build setup on core 5.x, an empty mod that loads and changes nothing. *Done.*
2. **Live lanes.** Trig 1–8 send the 8 lane notes. *Done, tested in digiemu (20 checks).*
   **0.3 adds:** hold a pad + turn A/B to set its note/velocity; the drum map and on/off state
   are saved as project data "DRUM" (pulled forward from milestone 4; 21 more checks).
   **0.4 adds live recording** (milestones 3 and 4 by another route, below; 15 more checks).
   **0.5 adds lane mutes:** FUNC + pad; seq-events' notes stage drops muted lanes' notes from each step
   the sequencer plays (a step left with none plays nothing); saved in project data (10 more checks).
   **0.6:** the lane strip blinks as the sequencer plays each lane (a step's lanes, seen when it is
   fired, blink at the next fire, when it plays), and the corner is drawn only on the parameter pages
   (the view on top has the page class's vtable, 0x401c3f34 in OS 1.44), not over menus (11 more checks).
   **0.7 is milestone 5:** TRIG (page key) + pad selects a lane; in GRID RECORDING the 16 trig keys toggle
   the selected lane on the shown page's steps (added as live recording adds a note, removed by rewriting
   the step's notes, the trig going with the last); the trig key LEDs show the selected lane in GRID
   RECORDING and the pads (held, playing, muted) outside it (26 more checks).
   **0.8 finishes milestone 7:** two DRUM pages (mod pages 27 and 28) behind the MIDI tracks' LFO key,
   knobs A–H = lanes 1–8, Drum Notes and Drum Velocity, drawn by Drum Mode over empty knob boxes and
   edited through ev_enc while shown (15 more checks).
3. **Lane playback.** Steps fire lanes through the sequencer hook (hard-coded test pattern).
4. **Lane storage.** Lane steps in the pattern, drum map in the project.
5. **Grid editing and LEDs.** Lane select, step toggling, per-lane LEDs. *Done in 0.7.*
6. **Screen grid.** *Nice to have, not scheduled.* The LEDs show one lane, the strip shows what plays;
   a grid would add the whole pattern at a glance. A full-screen grid needs its own view and costs the
   page underneath, so the lighter idea is an overview drawn only while TRIG is held (the lane-select
   key), with the selected lane highlighted. Decide after testing 0.7 on a unit.
7. **Polish.** Lane mute, DRUM page, LIVE REC; then decide on v2. *Done: 0.5, 0.8, 0.4.*
   *Nice to have:* revisit the DRUM pages after hardware use (where they sit, layout, what they show);
   0.8's version stands until then.
   *Nice to have:* drum kit presets (a track's 8 notes, velocities and mutes, about 17 bytes). Within a
   project: N kit slots in the "DRUM" project data (it shares 480 bytes with other mods; a variable-size
   block, core-dn1 4.2's CORE_PROJDATA_VARIABLE, would only save the slots in use), with save/load from
   the Mod Menu or a DRUM page. Across projects: core-dn1 has no storage outside the project (the
   Octatrack's core has card files and battery RAM, the Rytm's sound/kit/global slots; the Digitone mk1's
   only the project's 480-byte gap), so that needs either a new core feature or a SysEx dump/receive of
   a kit through the computer.

## Findings while building

- **Turning it on.** v0.2 toggles Drum Mode from the Mod Menu (DRUM MODE), and then the trig
  keys are pads on that track whatever the GRID/KEYBOARD state. Tying pads to KEYBOARD mode
  needs the firmware's KEYBOARD flag, which core does not export; the KEYBOARD key is code 3,
  so the flag can be found in digiemu by diffing memory around a KEYBOARD press. Decided in
  0.7: the pads play in both modes; grid editing happens in GRID RECORDING instead (below), which is
  where the stock's trig keys place trigs too. (KEYBOARD on: byte +0x167 of the recorder's state.)
- **Sending notes.** The stock KEYBOARD dispatcher (0x4003b198) plays a MIDI track's note
  through 0x400eae5c, which core exports as `fw_note_on(track, note, vel, source, skip, sound)`
  (tracks 4–7 are MIDI A–D). Drum Mode calls it with source 0x40, as the trig keys do.
- **Key LEDs (0.7).** `ev_key_leds` gives the LedManager; the setter is 0x40016e62 in OS 1.44,
  `set(mgr, led, layer, colour)`, found through DigiArp MIDI (which lights the KEYBOARD key with it)
  and matching Modwerk's notes for the Digitakt II's `fw_led_set`. It refuses an LED already set this
  frame, so what a handler sets before the stock views stays. The trig keys are LEDs 1–16 (code − 25).
  Colours are palette indices: 2 dark, 3 red (a stock trig), 5 dim red, 20 white, 21 the stock's
  dim white. A mod asks for an LED frame by setting the view controller's byte +0x21 (ev_tick's ctrl).
  digiemu decodes the panel's LED stream (emu/panelleds.py), which the tests read.
- **GRID RECORDING and pages (0.7).** GRID RECORDING is byte +0x168 of the recorder's state
  (`*(0x419f0dd8)`), valid while +0x161 is set (the stock getter 0x400adcb4); +0x169 is live recording.
  The page the trig keys show is the long at 0x41368ce8 (0 = steps 1–16), found by diffing memory
  around PAGE presses on a 48-step track; the stock's own trig key then sets step 16 × page + key.
- **MIDI track channel.** A MIDI track's CHAN is off in the default project ("Func+Press to
  enable" on its SRC page); with it off nothing is sent, stock or Drum Mode.
- **Live recording (0.4).** Recorded hits are stored as stock MIDI trigs: a trig plus the step's 8 note
  slots (slot 0 the note, 1–7 intervals), the stock's chord format, written with the stock's setters
  TRIG_SET (0x4003673a) and STEP_NOTE (0x40035858) on the track's edit object (fw_project + 0x11f4 +
  0x58 × track). So the lane-parameter storage planned above was not needed: patterns save, copy and
  clear the hits like any trig, and the stock sequencer plays them. Live recording is byte +0x169 of the
  object a global at 0x419f0dd8 points to (the stock's getter 0x400addc8). A hit's step: the sequencer
  fires a step one step before it plays (measured in digiemu), so the playing position is the last fired
  step − 1 plus the time since its fire; the hit goes to the nearest step.
- **Bypassing the stock recorder.** The live note routine (fw_note_on) queues every note for the stock's
  recorder (0x400f050e), which recorded the pads again on steps of its own. From 0.4 the pads send
  MIDI through the port switch's note senders (0x400ee232 / 0x400ee29e) on the MIDI track's channel,
  read from the sequencer's MIDI track state (0x419c5b30 + 158 × track + 38; CHAN on: bit 0 of
  0x419c5b10 + 4 × (186 + track)).
- **The sequencer needs the audio engine.** In digiemu the Digitone's sequencer only runs with audio
  on (it is clocked by the render), so recording and playback tests start the session with audio.
- **DRUM pages (0.8).** The firmware's 26 page records sit at 0x4136c558 + 44 × index in 1.44 (short title,
  title, 8 knob ids, kind); the MIDI tracks' pages are 19 TRIG, 20 MIDI Source (on both SYN1 and SYN2),
  21 CC Value (FLTR), 22 CC Select (AMP), 23 LFO. A mod page after 20 stops FUNC + push A from turning CHAN
  on (measured in digiemu; after 19, 21 and 23 it works), so the DRUM pages follow 23. The pages' knob ids
  are 0, so Drum Mode needs none of the 10 mod parameter ids other mods share; it draws the cells itself
  and takes the knobs in ev_enc. core_page_shown compares against the one view core noted in the
  descriptor, so Drum Mode reads the shown view's page list itself (Brain +0x54/+0x5c, page manager +8,
  view +0xc0, list +124/+128, current +144; core-dn1's pages.s and lists.c).

