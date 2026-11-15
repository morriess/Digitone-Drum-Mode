/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Drum Mode for the Digitone mk1 (core-dn1 5.3): a MIDI track becomes 8 drum lanes, each sending its own note on
 * the track's MIDI channel. See ../docs/design.md.
 *
 * Milestone 2, live lanes:
 * - DRUM MODE in the Mod Menu (hold a track key) turns Drum Mode on or off for the active MIDI track (A-D).
 * - While a Drum Mode track is the active track, trig keys 1-8 are pads: each plays its lane's note on the track's
 *   channel, several at once, through the same stock routine KEYBOARD mode uses (fw_note_on). Trig keys 9-16 do
 *   nothing yet. FUNC + a trig key is left to the stock.
 * - The screen's top-right corner shows the 8 lanes; a lane lights while it is held.
 *
 * Not yet: lane steps in the sequencer, lane select, the DRUM page, saving. Drum Mode is off after a power cycle. */
#include "digitone-mk1/core3.h"

typedef void (*rect_fn)(void *bmp, int x0, int y0, int x1, int y1, int colour);
typedef void (*text_fn)(void *bmp, const void *font, int x, int y, int maxlen, const char *fmt, ...);

#define DM_TRACKS       4               /* MIDI tracks A-D: firmware tracks 4-7 */
#define DM_FIRST        4
#define DM_LANES        8

#define KEY_STEP1       26              /* trig keys 1-16: 26-41 */
#define KEY_STEPS       16

#define SRC_PANEL       0x40            /* the source KEYBOARD mode passes for the trig keys */
#define NO_SOUND        (-1)

#define FLASH_TICKS     6               /* how long a lane stays lit after a hit, in UI ticks */
#define NOTE_TICKS      60              /* how long a corner message shows */

/* The default drum map (General MIDI). */
static const uint8_t dm_default_notes[DM_LANES] = { 36, 38, 42, 46, 39, 45, 50, 49 };

struct dm_lane {
    uint8_t note;
    uint8_t velocity;
};

struct dm_track {
    uint8_t enabled;                    /* 0: the track plays as stock */
    uint8_t mutes;                      /* bit per lane (later milestones) */
    struct dm_lane lanes[DM_LANES];
};

static struct dm_track dm_tracks[DM_TRACKS];

/* What each trig key is sounding, so its release ends the same note on the same track, whatever changed since. */
struct dm_held {
    int8_t track;                       /* firmware track 4-7, or -1: nothing held */
    uint8_t note;
    uint8_t lane;
};
static struct dm_held dm_held[KEY_STEPS];

static uint8_t dm_flash[DM_TRACKS][DM_LANES];  /* ticks left lit, per lane */
static uint8_t dm_lit_held[DM_TRACKS];          /* bit per lane: held now */

static const char *dm_note;             /* the corner message */
static int32_t dm_note_arg, dm_note_ticks;
static int32_t dm_dirty;                /* the corner needs a redraw */

static void say(const char *what, int32_t arg)
{
    dm_note = what;
    dm_note_arg = arg;
    dm_note_ticks = NOTE_TICKS;
    dm_dirty = 1;
}

static int32_t drum_track(int32_t track)
{
    return track >= DM_FIRST && track < DM_FIRST + DM_TRACKS && dm_tracks[track - DM_FIRST].enabled;
}

static void lane_on(int32_t key, int32_t track, int32_t lane)
{
    struct dm_lane *l = &dm_tracks[track - DM_FIRST].lanes[lane];
    fw_note_on(track, l->note, l->velocity, SRC_PANEL, 0, NO_SOUND);
    dm_held[key].track = (int8_t)track;
    dm_held[key].note = l->note;
    dm_held[key].lane = (uint8_t)lane;
    dm_lit_held[track - DM_FIRST] |= (uint8_t)(1u << lane);
    dm_flash[track - DM_FIRST][lane] = FLASH_TICKS;
    dm_dirty = 1;
}

static void lane_off(int32_t key)
{
    struct dm_held *h = &dm_held[key];
    if (h->track < 0)
        return;
    fw_note_off(h->track, h->note, SRC_PANEL);
    dm_lit_held[h->track - DM_FIRST] &= (uint8_t)~(1u << h->lane);
    h->track = -1;
    dm_dirty = 1;
}

static void release_track(int32_t track)
{
    for (int32_t k = 0; k < KEY_STEPS; k++)
        if (dm_held[k].track == track)
            lane_off(k);
}

/* ---- boot ------------------------------------------------------------------------------------ */
void dm_boot(void)
{
    for (int32_t t = 0; t < DM_TRACKS; t++) {
        dm_tracks[t].enabled = 0;
        dm_tracks[t].mutes = 0;
        for (int32_t l = 0; l < DM_LANES; l++) {
            dm_tracks[t].lanes[l].note = dm_default_notes[l];
            dm_tracks[t].lanes[l].velocity = 100;
        }
    }
    for (int32_t k = 0; k < KEY_STEPS; k++)
        dm_held[k].track = -1;
}

/* ---- keys ------------------------------------------------------------------------------------ */
static uint32_t dm_taken;               /* bit k: trig key k's press was ours; its repeats and release are too */

/* The UI task, each key event. On a Drum Mode track a trig key's press is taken (FUNC + a trig key is left to the
 * stock), and then that key's repeats and release, whatever changed meanwhile: a release always ends what its
 * press played, and a key the stock saw go down is never kept from it on the way up. */
int32_t dm_key(void *brain, void *ev)
{
    int32_t key = CORE_KEY_ID(ev);
    uint32_t flags = CORE_KEY_FLAGS(ev);
    int32_t up = (flags & CORE_KEY_UP) || !(flags & CORE_KEY_DOWN);
    int32_t k, track = fw_active_track;
    uint32_t bit;
    (void)brain;

    if (key < KEY_STEP1 || key >= KEY_STEP1 + KEY_STEPS)
        return 0;
    k = key - KEY_STEP1;
    bit = 1u << k;

    if (dm_taken & bit) {
        if (up) {
            lane_off(k);
            dm_taken &= ~bit;
        }
        return 1;                       /* a repeat of ours, or our release */
    }
    if (up || (flags & (CORE_KEY_FUNC | CORE_KEY_REPEAT)) || !drum_track(track))
        return 0;
    dm_taken |= bit;
    if (k < DM_LANES)
        lane_on(k, track, k);
    return 1;                           /* 9-16: taken, nothing yet */
}

/* ---- the Mod Menu ---------------------------------------------------------------------------- */
struct item {
    const char *name;
    void (*open)(void *brain, void *ev, int32_t track);
    unsigned long tag;
    const unsigned short *icon;
};

static const unsigned short dm_icon[16] = {             /* a 4 x 3 grid of pads, some lit */
    0x0000, 0x0000, 0x7777, 0x7575, 0x7777, 0x0000, 0x7777, 0x5757,
    0x7777, 0x0000, 0x7777, 0x7557, 0x7777, 0x0000, 0xffff, 0x0000
};

static void dm_open(void *brain, void *event, int32_t track)
{
    struct dm_track *d;
    (void)brain; (void)event; (void)track;
    track = fw_active_track;
    if (track < DM_FIRST || track >= DM_FIRST + DM_TRACKS) {
        say("MIDI TRK ONLY", 0);
        return;
    }
    d = &dm_tracks[track - DM_FIRST];
    d->enabled = !d->enabled;
    if (!d->enabled)
        release_track(track);
    say(d->enabled ? "DRUM %c ON" : "DRUM %c OFF", 'A' + track - DM_FIRST);
}

const struct item dm_menu = { "DRUM MODE", dm_open, CORE_MENU_ICON, dm_icon };

/* ---- the screen ------------------------------------------------------------------------------ */
/* The UI task, 60 times a second: count down the flashes and the message, and ask for a redraw while anything
 * we draw changes. ctrl + 0x20: the redraw flag (as digisolo sets it). */
void dm_tick(void *ctrl)
{
    for (int32_t t = 0; t < DM_TRACKS; t++)
        for (int32_t l = 0; l < DM_LANES; l++)
            if (dm_flash[t][l]) {
                if (--dm_flash[t][l] == 0)
                    dm_dirty = 1;
            }
    if (dm_note_ticks > 0) {
        if (--dm_note_ticks == 0)
            dm_dirty = 1;
    }
    if (dm_dirty || dm_note_ticks > 0) {
        *((unsigned char *)ctrl + 0x20) = 1;
        dm_dirty = 0;
    }
}

/* The top-right corner: a message (x 84-127, y 54-63, the box digisolo uses), or on a Drum Mode track the 8 lanes
 * as small boxes (x 94-127), filled while held or just hit. Drawn by inverting a cleared box, so the lit parts show
 * on any page; the strip is kept narrow so the title bar's text beside it stays readable. */
void dm_draw(void *bmp, void *ctrl)
{
    int32_t track = fw_active_track, t;
    (void)ctrl;
    if (dm_note_ticks > 0 && dm_note) {
        ((rect_fn)fw_fillrect)(bmp, 84, 54, 127, 63, 0);
        ((text_fn)fw_textf)(bmp, (const void *)fw_font5, 87, 57, -1, dm_note, (int)dm_note_arg);
        ((rect_fn)fw_fillrect)(bmp, 84, 54, 127, 63, -1);
        return;
    }
    if (!drum_track(track))
        return;
    t = track - DM_FIRST;
    ((rect_fn)fw_fillrect)(bmp, 94, 54, 127, 63, 0);
    for (int32_t l = 0; l < DM_LANES; l++) {
        int32_t x0 = 96 + 4 * l, x1 = x0 + 2;
        if ((dm_lit_held[t] >> l) & 1 || dm_flash[t][l])
            ((rect_fn)fw_fillrect)(bmp, x0, 56, x1, 61, -1);
        else
            ((rect_fn)fw_framerect)(bmp, x0, 56, x1, 61, -1);
    }
}
