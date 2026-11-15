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

**Combining with DigiArp.** DigiArp is built on core 4.1.1; DrumMode needs core
5.1+. Core 5.2.1 moved two hooks so DigiArp fits beside it; needs a test build.

## Open questions

- Which drum machine or synth, and which notes does it expect?
- One velocity per lane, or an accent lane in v1?
- Which modifier for picking a lane?
- Note length: track LEN or always short?
- DigiArp + DrumMode in one firmware, or DrumMode-only first?
- License?

## Build plan

1. **Groundwork.** Build setup on core 5.x, an empty mod that loads and changes nothing.
2. **Live lanes.** KEYBOARD mode: trig 1–8 send the 8 lane notes.
3. **Lane playback.** Steps fire lanes through the sequencer hook (hard-coded test pattern).
4. **Lane storage.** Lane steps in the pattern, drum map in the project.
5. **Grid editing and LEDs.** Lane select, step toggling, per-lane LEDs.
6. **Screen grid.** 8 × 16 grid, playhead, lane flashes.
7. **Polish.** Lane mute, DRUM page, LIVE REC; then decide on v2.
