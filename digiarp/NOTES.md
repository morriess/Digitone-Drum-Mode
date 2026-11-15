# Notes: the "random notes from one held key" bug

Investigated 2026-11-14 against DigiArp DA27 and the disassembled Digitone OS 1.44.
No confirmed cause yet.

## Ruled out

- **Keyboard hook arguments.** The stock KEYBOARD note-on (0x4003b198) takes
  `(this, source, track, note, velocity)` and the note-off (0x4003b26e)
  `(this, source, track, note)`. DigiArp's `live_down` / `live_up` pass these to
  `live_input` correctly.
- **Garbled MIDI bytes.** The DIN send routine (0x40002a3a) masks interrupts
  (SR = 0x2700) while it copies each message, so messages can't interleave.
- **Release with a different note.** The firmware keeps a list of held keys
  (24-byte entries at `this+172`) and releases each with the track and note it
  recorded at press time.
- **ARP page vs engine.** The page's accessors read the same bytes
  (`+0x10f` mode, `+0x114` step mask) through the sound's method 10 that the engine
  reads, so the step offsets on screen are what the engine uses.
- **Engine maths.** With one note, RANGE 1 and zero offsets, `select_note` can only
  return that note.

## Remaining suspects

With one key held, different pitches mean the engine holds extra ("ghost") notes:

1. **Sequencer notes left after STOP.** `seq_notes` is cleared on stop only when the
   trig window was finite or the transport was running at the previous poll.
2. **A release DigiArp never sees.** The firmware's release-all path
   (0x4003bc0e) and entries flagged at `a3@(1)` don't all go through 0x4003b26e.
3. **Task priority.** DigiArp's worker runs at RTOS priority 1 (scheduler: higher
   number = higher priority; 0 is the init task), sharing the level with the task at
   0x4008cf00. That makes timing-dependent cases more likely on hardware than in the
   emulator.

## Next step

Reproduce with a MIDI monitor and note whether the extra notes are earlier played
notes (ghosts), octaves (RANGE), or on another channel (stock firmware). A
possible guard: while stopped with no keys held, clear the track's engine notes and
`seq_notes`.
