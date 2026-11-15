/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Drum Mode for the Digitone mk1: a MIDI track becomes 8 drum lanes, each sending its own note on the
 * track's MIDI channel. See ../docs/design.md.
 *
 * Milestone 1: the mod loads and changes nothing the unit does. It only holds the lane defaults that the
 * next milestones will use, and notes that the boot reached the UI. */
#include <stdint.h>

#define DM_TRACKS   4   /* MIDI tracks A-D */
#define DM_LANES    8

/* The default drum map (General MIDI): kick, snare, closed hat, open hat, clap, low tom, high tom, crash. */
static const uint8_t dm_default_notes[DM_LANES] = { 36, 38, 42, 46, 39, 45, 50, 49 };

struct dm_lane {
    uint8_t note;
    uint8_t velocity;
};

struct dm_track {
    uint8_t enabled;            /* 0: the track plays as stock */
    uint8_t selected_lane;
    uint8_t mutes;              /* bit per lane */
    struct dm_lane lanes[DM_LANES];
};

static struct dm_track dm_tracks[DM_TRACKS];
static volatile uint32_t dm_booted;

/* The UI task is about to start. Set every track to the defaults, Drum Mode off: nothing changes yet. */
void dm_boot(void)
{
    for (int32_t t = 0; t < DM_TRACKS; t++) {
        dm_tracks[t].enabled = 0;
        dm_tracks[t].selected_lane = 0;
        dm_tracks[t].mutes = 0;
        for (int32_t l = 0; l < DM_LANES; l++) {
            dm_tracks[t].lanes[l].note = dm_default_notes[l];
            dm_tracks[t].lanes[l].velocity = 100;
        }
    }
    dm_booted = 1;
}
