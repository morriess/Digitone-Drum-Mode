# Drum Mode UI

How Drum Mode shows itself on the Digitone's screen, as of 0.8. All images are
screenshots of the real firmware running in [digiemu](https://github.com/irpina/digiemu).

Everything Drum Mode draws lives in the **top-right corner** of the screen, only while
the selected MIDI track has Drum Mode on, and only on the parameter pages (TRIG, SYN1,
SYN2, FLTR, AMP, LFO): never over the settings menus, the project list, TEMPO or the
pattern screens. Three things can be shown there, one at
a time, in this order of priority:

1. a short message, for about 2 seconds;
2. the held pad's note and velocity, while you edit it;
3. otherwise, the lane strip.

## The lane strip

![The lane strip in three states](lane-strip.png)

Eight tiny boxes, one per pad, in pad order:

| Pad | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Lane | BD (kick) | SD (snare) | CH (closed hat) | OH (open hat) | CP (clap) | LT (low tom) | HT (high tom) | CY (crash) |

Each box has three looks:

| Look | Meaning |
| --- | --- |
| Outline (a small "0") | Lane active, not playing right now |
| Filled | The pad is held or was just hit, or the sequencer just played that lane (a short blink) |
| Dash | Lane muted (FUNC + pad) |

**The selected lane (0.7)** has a short line under its box (row 9), so you can see which
lane the trig keys edit in GRID RECORDING. Select a lane with TRIG + its pad.

A held pad on a muted lane shows filled: the pad still plays. A muted lane does not blink
on playback, because the sequencer leaves it out.

**Following playback (0.6).** The sequencer prepares each step one step before it plays it,
which is where Drum Mode sees the step's notes. So a step's lanes are kept until the next
step is prepared, which is when the step actually plays, and blink then. The blink is
shorter than a pad hit's (about 0.1 s), so lanes on consecutive 16ths blink apart.

**Geometry.** Each box is 3 × 6 pixels with 1 pixel between boxes, 32 pixels in all, at
x 96–126, y 2–7 of the 128 × 64 screen (y 0 the top row). The strip clears x 94–127,
rows 0–9, first. It was narrower than the first version (0.2), which covered the title
bar's `P:+0` text.

**Drawing.** The corner is cleared to black, then the boxes are drawn by inverting
pixels, so they show the same on every page without knowing what the stock page drew
underneath.

**Updates.** The corner is redrawn only when something it shows changes: a hit, a release,
a mute, a flash running out.

## Messages

![A corner message](corner-message.png)

Short confirmations replace the strip for about 2 seconds, in a white box with black
text:

| Message | When |
| --- | --- |
| `DRUM A ON` / `DRUM A OFF` | DRUM MODE picked in the Mod Menu (A–D: the MIDI track) |
| `BD MUTE` / `BD ON` | FUNC + a pad mutes or unmutes its lane |
| `EDIT BD` | TRIG + a pad selects its lane for grid editing |
| `MIDI TRK ONLY` | DRUM MODE picked while a synth track is selected |

## Pad info

![The held pad's note and velocity](pad-info.png)

While you hold a pad and turn A (note) or B (velocity), or hold a pad for a moment, the
corner shows the pad's lane, note number and velocity, e.g. `BD 38 V100`. It is wider
than the strip (x 64–127) and stays while the pad is held, then fades out shortly after.

## The DRUM pages (0.8)

Behind the LFO page on a MIDI track (press LFO twice): **Drum Notes** and **Drum Velocity**.

![Drum Notes](drum-notes-page.png) ![Drum Velocity](drum-velocity-page.png)

Eight cells in knob order (A–D top, E–H bottom), each with the lane's name and its note
(name and number, as the Digitone names notes: 60 is C5) or velocity. The selected lane's
cell is inverted; a muted lane's velocity cell says `MUTE`. Drum Mode clears the page's
empty knob boxes and draws the cells itself; the corner strip is not drawn on these pages,
so the title stays readable. Each cell is 25 × 24 pixels, x 20–127, rows 12–63.

## The trig key LEDs (0.7)

On a Drum Mode track Drum Mode also owns the 16 trig key LEDs:

| Mode | Key | LED |
| --- | --- | --- |
| GRID RECORDING | a step where the selected lane plays | red |
| GRID RECORDING | any other step | dark (other lanes don't show) |
| GRID RECORDING | the playing step, steps past the track's length | the stock's |
| Otherwise | pad 1–8 held, or its lane just played | white |
| Otherwise | pad 1–8 of a muted lane | dim red |
| Otherwise | any other key | dark |

## Known limitations

- **The strip covers the tempo display** in the top-right corner of the main page while
  Drum Mode is on.
- **It is small:** a glance indicator, not a replacement for real feedback.
- **The LED colours are from the emulator's palette.** They should match the unit, but
  that hasn't been checked on hardware yet.

## Where it is in the code

`dm_draw`, `dm_tick` and `dm_leds` in [../../src/drummode.c](../../src/drummode.c): `dm_tick`
(the UI task's tick) turns the sequencer's played lanes into blinks, counts down the
flashes and messages and asks for a redraw; `dm_draw` draws the corner (only when
`on_page()`: the view on top is a parameter page). The played lanes come from
`dm_fire_notes` and `dm_gen_trig`, at the sequencer's interrupt level.
