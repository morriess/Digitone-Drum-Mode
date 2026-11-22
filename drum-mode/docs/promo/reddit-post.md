# Reddit announcement (draft)

**Title:** Drum Mode: turn a Digitone mk1 MIDI track into an 8-voice drum sequencer (free, open source, beta)

I've been building a mod for the Digitone mk1 (OS 1.44, on elekloader) that turns one MIDI track into a proper drum sequencer for an external drum machine or sampler, all on one MIDI channel.

**What it does**

- Trig keys 1–8 become drum pads: kick, snare, hats, clap, toms, cymbal (GM notes by default, change any of them)
- Live record finger drumming, several drums on one step
- Grid record one drum at a time: pick a voice and its steps light up
- A DRUM page behind LFO with all 8 notes and velocities at a glance
- FUNC + pad mutes a voice, and keys 9–16 show a running light while it plays
- Settings saved per pattern, and the stock trig key functions (pattern select, chains, length) still work

**Status:** beta. Most of it has been tested on my own unit, the latest fixes in the emulator so far. Back up your projects before trying it.

**Get it:** build it yourself from the repo with your own copy of the stock OS (instructions in the README). It's coming to the elekloader Shop soon.

GitHub: https://github.com/morriess/Digitone-Drum-Mode

Feedback and bug reports are very welcome, especially from people with different drum machines on the other end of the MIDI cable. 🥁

*(Images: overview, a demo GIF from the real firmware running in an emulator, and an artist impression.)*
