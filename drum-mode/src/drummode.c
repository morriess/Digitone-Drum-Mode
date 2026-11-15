/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Drum Mode for the Digitone mk1 (core-dn1 5.3): a MIDI track becomes 8 drum lanes, each sending its own note on
 * the track's MIDI channel. See ../docs/design.md.
 *
 * 0.3, live lanes with assignable notes:
 * - DRUM MODE in the Mod Menu (hold a track key) turns Drum Mode on or off for the active MIDI track (A-D).
 * - While a Drum Mode track is the active track, trig keys 1-8 are pads: each plays its lane's note on the track's
 *   channel, several at once, through the same stock routine KEYBOARD mode uses (fw_note_on). Trig keys 9-16 do
 *   nothing yet. FUNC + a trig key is left to the stock.
 * - Hold a pad and turn A: its note (you hear each new note as it changes). Turn B: its velocity. While a pad is
 *   held the knobs are Drum Mode's, so the page underneath does not change. Holding a pad a moment, or turning,
 *   shows its note and velocity in the corner.
 * - The screen's top-right corner shows the 8 lanes; a lane lights while it is held.
 * - Saved with the project (project data "DRUM"): each track's Drum Mode on/off and its 8 notes and velocities.
 *
 * Not yet: lane steps in the sequencer, lane select, per-lane trig LEDs. */
#include "digitone-mk1/core3.h"

typedef void (*rect_fn)(void *bmp, int x0, int y0, int x1, int y1, int colour);
typedef void (*text_fn)(void *bmp, const void *font, int x, int y, int maxlen, const char *fmt, ...);

#define DM_TRACKS       4               /* MIDI tracks A-D: firmware tracks 4-7 */
#define DM_FIRST        4
#define DM_LANES        8

#define KEY_STEP1       26              /* trig keys 1-16: 26-41 */
#define KEY_STEPS       16
#define ENC_A           1               /* ev_enc's encoders: 1-8, A-H */
#define ENC_B           2
#define COUNTS          4               /* encoder counts a detent */

#define SRC_PANEL       0x40            /* the source KEYBOARD mode passes for the trig keys */
#define NO_SOUND        (-1)

#define FLASH_TICKS     6               /* how long a lane stays lit after a hit, in UI ticks */
#define NOTE_TICKS      60              /* how long a corner message shows */
#define INFO_TICKS      90              /* how long a pad's note and velocity show after its pad is let go */

#define SAVE_VERSION    1

/* The default drum map (General MIDI), and the lanes' short names for the corner. */
static const uint8_t dm_default_notes[DM_LANES] = { 36, 38, 42, 46, 39, 45, 50, 49 };
static const char *const dm_names[DM_LANES] = { "BD", "SD", "CH", "OH", "CP", "LT", "HT", "CY" };

/* What the project keeps (project data "DRUM"). A project saved without Drum Mode loads as all zeros: version 0,
 * which dm_loaded turns into the defaults. */
struct dm_save {
    uint8_t version;
    uint8_t enabled;                    /* bit per track */
    uint8_t mutes[DM_TRACKS];           /* bit per lane (later milestones) */
    uint8_t pad[2];
    uint8_t note[DM_TRACKS][DM_LANES];
    uint8_t velocity[DM_TRACKS][DM_LANES];
};
static struct dm_save dm;

/* What each trig key is sounding, so its release ends the same note on the same track, whatever changed since. */
struct dm_held {
    int8_t track;                       /* firmware track 4-7, or -1: nothing held */
    uint8_t note;
    uint8_t lane;
};
static struct dm_held dm_held[KEY_STEPS];
static uint32_t dm_taken;               /* bit k: trig key k's press was ours; its repeats and release are too */
static int32_t dm_last_pad = -1;        /* the pad pressed last, while it is held: the knobs edit its lane */
static int32_t dm_enc_acc[2];           /* counts towards the next detent, A and B */

static uint8_t dm_flash[DM_TRACKS][DM_LANES];  /* ticks left lit, per lane */
static uint8_t dm_lit_held[DM_TRACKS];          /* bit per lane: held now */

static const char *dm_note;             /* the corner message */
static int32_t dm_note_arg, dm_note_ticks;
static int32_t dm_info_ticks;           /* > 0: the corner shows the held pad's note and velocity */
static int32_t dm_dirty;                /* the corner needs a redraw */

static void say(const char *what, int32_t arg)
{
    dm_note = what;
    dm_note_arg = arg;
    dm_note_ticks = NOTE_TICKS;
    dm_info_ticks = 0;
    dm_dirty = 1;
}

static void defaults(void)
{
    fw_memset(&dm, 0, sizeof dm);
    dm.version = SAVE_VERSION;
    for (int32_t t = 0; t < DM_TRACKS; t++)
        for (int32_t l = 0; l < DM_LANES; l++) {
            dm.note[t][l] = dm_default_notes[l];
            dm.velocity[t][l] = 100;
        }
}

static int32_t drum_track(int32_t track)
{
    return track >= DM_FIRST && track < DM_FIRST + DM_TRACKS && ((dm.enabled >> (track - DM_FIRST)) & 1);
}

static void lane_on(int32_t key, int32_t track, int32_t lane)
{
    int32_t t = track - DM_FIRST;
    fw_note_on(track, dm.note[t][lane], dm.velocity[t][lane], SRC_PANEL, 0, NO_SOUND);
    dm_held[key].track = (int8_t)track;
    dm_held[key].note = dm.note[t][lane];
    dm_held[key].lane = (uint8_t)lane;
    dm_lit_held[t] |= (uint8_t)(1u << lane);
    dm_flash[t][lane] = FLASH_TICKS;
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

/* ---- boot and the project -------------------------------------------------------------------- */
/* The project may load before the UI starts (dm_loaded first): only the held keys are reset here. */
void dm_boot(void)
{
    for (int32_t k = 0; k < KEY_STEPS; k++)
        dm_held[k].track = -1;
    if (dm.version != SAVE_VERSION)
        defaults();
}

/* After every project load: the project's Drum Mode, or the defaults for a project that has none (all zeros).
 * The engine task, before the UI sees the project. A note or velocity out of range is put back to its default. */
static void dm_loaded(int32_t found)
{
    if (!found || dm.version != SAVE_VERSION) {
        defaults();
        return;
    }
    for (int32_t t = 0; t < DM_TRACKS; t++)
        for (int32_t l = 0; l < DM_LANES; l++) {
            if (dm.note[t][l] > 127)
                dm.note[t][l] = dm_default_notes[l];
            if (dm.velocity[t][l] < 1 || dm.velocity[t][l] > 127)
                dm.velocity[t][l] = 100;
        }
    dm.enabled &= (1u << DM_TRACKS) - 1;
}
const struct core_projdata dm_projdata = { 0x4452554du /* "DRUM" */, sizeof dm, &dm, dm_loaded };

/* ---- keys ------------------------------------------------------------------------------------ */
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
            if (dm_last_pad == k)
                dm_last_pad = -1;       /* its info fades out (dm_tick) */
        } else if ((flags & CORE_KEY_REPEAT) && dm_last_pad == k) {
            dm_info_ticks = INFO_TICKS; /* held a moment: show its note */
            dm_note_ticks = 0;
            dm_dirty = 1;
        }
        return 1;                       /* a repeat of ours, or our release */
    }
    if (up || (flags & (CORE_KEY_FUNC | CORE_KEY_REPEAT)) || !drum_track(track))
        return 0;
    dm_taken |= bit;
    if (k < DM_LANES) {
        lane_on(k, track, k);
        if (dm_last_pad != k)
            dm_info_ticks = 0;
        dm_last_pad = k;
        dm_enc_acc[0] = dm_enc_acc[1] = 0;
    }
    return 1;                           /* 9-16: taken, nothing yet */
}

/* ---- knobs ----------------------------------------------------------------------------------- */
/* The UI task, each encoder turn (ev+12 the encoder 1-8, ev+16 the turn in counts, four a detent). While a pad is
 * held every knob is taken: A steps its lane's note and plays the new note at once, B its velocity. */
int32_t dm_enc(void *brain, void *ev)
{
    int32_t enc = CORE_KEY_ID(ev), counts = (int32_t)CORE_KEY_FLAGS(ev), steps = 0;
    int32_t k = dm_last_pad, t, lane;
    struct dm_held *h;
    (void)brain;

    if (k < 0 || !(dm_taken & (1u << k)) || dm_held[k].track < 0)
        return 0;
    h = &dm_held[k];
    t = h->track - DM_FIRST;
    lane = h->lane;
    if (enc != ENC_A && enc != ENC_B)
        return 1;                       /* the other knobs: nothing, but not the page's either */

    dm_enc_acc[enc - 1] += counts;
    while (dm_enc_acc[enc - 1] >= COUNTS) {
        dm_enc_acc[enc - 1] -= COUNTS;
        steps++;
    }
    while (dm_enc_acc[enc - 1] <= -COUNTS) {
        dm_enc_acc[enc - 1] += COUNTS;
        steps--;
    }
    if (!steps)
        return 1;

    if (enc == ENC_A) {
        int32_t n = dm.note[t][lane] + steps;
        n = n < 0 ? 0 : n > 127 ? 127 : n;
        if (n != dm.note[t][lane]) {
            int32_t track = h->track;
            dm.note[t][lane] = (uint8_t)n;
            lane_off(k);                /* hear the new note: the old one off, the new one on */
            lane_on(k, track, lane);
        }
    } else {
        int32_t v = dm.velocity[t][lane] + steps;
        dm.velocity[t][lane] = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
    }
    dm_info_ticks = INFO_TICKS;
    dm_note_ticks = 0;
    dm_dirty = 1;
    return 1;
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
    uint8_t bit;
    (void)brain; (void)event; (void)track;
    track = fw_active_track;
    if (track < DM_FIRST || track >= DM_FIRST + DM_TRACKS) {
        say("MIDI TRK ONLY", 0);
        return;
    }
    bit = (uint8_t)(1u << (track - DM_FIRST));
    dm.enabled ^= bit;
    if (!(dm.enabled & bit))
        release_track(track);
    say((dm.enabled & bit) ? "DRUM %c ON" : "DRUM %c OFF", 'A' + track - DM_FIRST);
}

const struct item dm_menu = { "DRUM MODE", dm_open, CORE_MENU_ICON, dm_icon };

/* ---- the screen ------------------------------------------------------------------------------ */
/* The UI task, 60 times a second: count down the flashes and the messages, and ask for a redraw while anything
 * we draw changes. ctrl + 0x20: the redraw flag (as digisolo sets it). */
void dm_tick(void *ctrl)
{
    for (int32_t t = 0; t < DM_TRACKS; t++)
        for (int32_t l = 0; l < DM_LANES; l++)
            if (dm_flash[t][l]) {
                if (--dm_flash[t][l] == 0)
                    dm_dirty = 1;
            }
    if (dm_note_ticks > 0 && --dm_note_ticks == 0)
        dm_dirty = 1;
    if (dm_info_ticks > 0 && dm_last_pad < 0 && --dm_info_ticks == 0)
        dm_dirty = 1;                   /* the info stays while its pad is held, then fades */
    if (dm_dirty || dm_note_ticks > 0) {
        *((unsigned char *)ctrl + 0x20) = 1;
        dm_dirty = 0;
    }
}

/* The top-right corner, drawn by inverting a cleared box so it shows on any page:
 * - a message (x 84-127, y 54-63, the box digisolo uses);
 * - a pad's lane, note and velocity ("BD 36 V100", x 64-127) while it is edited or held a moment, and a little after;
 * - else on a Drum Mode track the 8 lanes as small boxes (x 94-127), filled while held or just hit, kept narrow so
 *   the title bar's text beside it stays readable. */
static int32_t dm_info_lane = -1, dm_info_track = -1;

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
    if (dm_last_pad >= 0 && dm_held[dm_last_pad].track >= 0) {
        dm_info_lane = dm_held[dm_last_pad].lane;
        dm_info_track = dm_held[dm_last_pad].track - DM_FIRST;
    }
    if (dm_info_ticks > 0 && dm_info_lane >= 0) {
        ((rect_fn)fw_fillrect)(bmp, 64, 54, 127, 63, 0);
        ((text_fn)fw_textf)(bmp, (const void *)fw_font5, 67, 57, -1, "%s %d V%d", dm_names[dm_info_lane],
                            (int)dm.note[dm_info_track][dm_info_lane], (int)dm.velocity[dm_info_track][dm_info_lane]);
        ((rect_fn)fw_fillrect)(bmp, 64, 54, 127, 63, -1);
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
