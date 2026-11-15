# Drum Mode UI

How Drum Mode shows itself on the Digitone's screen, as of 0.5. All images are
screenshots of the real firmware running in [digiemu](https://github.com/irpina/digiemu).

Everything Drum Mode draws lives in the **top-right corner** of the screen, and only
while the selected MIDI track has Drum Mode on. Three things can be shown there, one at
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
| Filled | The pad is held, or was just hit (it stays lit about 0.1 s so short taps show) |
| Dash | Lane muted (FUNC + pad) |

A held pad on a muted lane shows filled: the pad still plays.

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
| `MIDI TRK ONLY` | DRUM MODE picked while a synth track is selected |

## Pad info

![The held pad's note and velocity](pad-info.png)

While you hold a pad and turn A (note) or B (velocity), or hold a pad for a moment, the
corner shows the pad's lane, note number and velocity, e.g. `BD 38 V100`. It is wider
than the strip (x 64–127) and stays while the pad is held, then fades out shortly after.

## Known limitations

- **The strip covers the tempo display** in the top-right corner of the main page while
  Drum Mode is on.
- **It follows the pads, not the sequencer:** recorded steps lighting up on playback are
  not shown.
- **It is small:** a glance indicator, not a replacement for real feedback.
- **No trig-key LEDs yet.** Lighting the trig keys per lane would be clearer; core's
  `ev_key_leds` lets a mod own the LEDs but exports no setter to light one, so that waits
  for the grid-editing milestone.

## Where it is in the code

`dm_draw` and `dm_tick` in [../../src/drummode.c](../../src/drummode.c): `dm_tick`
(the UI task's tick) counts down the flashes and messages and asks for a redraw;
`dm_draw` draws the corner.
