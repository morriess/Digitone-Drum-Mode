/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Drum Mode for the Digitone mk1 (core-dn1 5.3): a MIDI track becomes 8 drum lanes, each sending its own note on
 * the track's MIDI channel. See ../docs/design.md.
 *
 * 0.7, live lanes with assignable notes, live recording, lane mutes, the strip following playback, and grid editing:
 * - DRUM MODE in the Mod Menu (hold a track key) turns Drum Mode on or off for the active MIDI track (A-D).
 * - While a Drum Mode track is the active track, trig keys 1-8 are pads: each plays its lane's note on the track's
 *   MIDI channel (SRC's CHAN; nothing while it is off), several at once. Trig keys 9-16 do
 *   nothing yet. FUNC + a pad mutes its lane (below); FUNC + trig keys 9-16 are left to the stock.
 * - Hold a pad and turn A: its note (you hear each new note as it changes). Turn B: its velocity. While a pad is
 *   held the knobs are Drum Mode's, so the page underneath does not change. Holding a pad a moment, or turning,
 *   shows its note and velocity in the corner.
 * - The screen's top-right corner shows the 8 lanes; a lane lights while its pad is held, and blinks each time the
 *   sequencer plays it (muted lanes do not).
 * - Saved with the project (project data "DRUM"): each track's Drum Mode on/off and its 8 notes and velocities.
 * - Live recording (RECORD + PLAY): each pad hit is written into the pattern at the nearest step as a normal MIDI trig,
 *   its lane's note added to the step's notes (a step holds up to 8: a note and 7 intervals, the stock's chord), so
 *   recorded hits play, show, copy, clear and save as stock trigs. The pads bypass the stock's own recorder. A step whose notes are lanes plays at the loudest of their velocities.
 *
 * - FUNC + a pad mutes or unmutes its lane: the sequencer leaves a muted lane's note out of every step it plays (a step
 *   of muted lanes only plays nothing); its steps stay in the pattern. The pad itself still plays and records. The
 *   corner strip shows a muted lane as a dash. Saved with the project.
 *
 * - Hold the TRIG page key and press a pad: that lane is selected (no note plays). The corner strip underlines it.
 * - GRID RECORDING (RECORD while stopped) on a Drum Mode track: the 16 trig keys are the shown page's steps for the
 *   selected lane. A press adds the lane's note to the step (as live recording does) or takes it out (the step's
 *   trig goes when no note is left). The trig keys light red where the selected lane plays; the playing step is the
 *   stock's. Outside GRID RECORDING the pads light white while held or playing, dim red when muted.
 *
 * - 0.8: two DRUM pages behind the MIDI track's LFO key (LFO, then Drum Notes, then Drum Velocity): knobs
 *   A-H set lanes 1-8's notes (each new note plays, so it can be found by ear) and velocities.
 *
 * OS 1.44 only from 0.4: the live-record flag, the stock's step setters, and from 0.7 GRID RECORDING's flag, the shown
 * page and the LED setter are named by address (below). */
#include "digitone-mk1/core3.h"
#include "digitone-mk1/seq.h"

typedef void (*rect_fn)(void *bmp, int x0, int y0, int x1, int y1, int colour);
typedef void (*text_fn)(void *bmp, const void *font, int x, int y, int maxlen, const char *fmt, ...);

/* OS 1.44's live recording and step editing, found in its disassembly and checked in digiemu:
 * - REC_OBJ: a global pointing to the recorder's state; byte +0x169 is set while live recording (the stock's own
 *   getter, 0x400addc8, returns it while the sequencer plays; 0x400addf2 toggles it).
 * - TRACK_OBJ: each track's edit object. The project holds one set of them per pattern (128 sets, 0x38c bytes
 *   apart from fw_project + 0x11f4; track t's at + 0x58 * t): the active pattern's set is the one to edit it
 *   through (0.8 always took A01's, 0.8.1 A01's or A02's: on any other pattern recorded beats went into a pattern
 *   that wasn't playing). The set is the pattern's index, checked by its data pointer; failing that, all are searched.
 *   Its method 10 (+40 in the vtable, which returns +16) gives the track's pattern data (976 bytes: the step words
 *   at +2 * step, the step's 8 note slots at +448 + 8 * step, slot 0 the note and 1-7 intervals, 0 none).
 * - TRIG_SET (track obj, step, mask, on) and STEP_NOTE (track obj, step, note, slot): the stock's setters, which core
 *   hooks for ev_trig_set and ev_step_note; the stock's live recorder calls them in this order.
 * - The sequencer's MIDI track state, 158 bytes a MIDI track from 0x419c5b30: the output channel (SRC's CHAN, 0-15) at
 *   +38, followed live as the knob turns; CHAN on at bit 0 of the long at 0x419c5b10 + 4 * (186 + track).
 * - MIDI_NOTE_ON (channel, note, velocity) and MIDI_NOTE_OFF (channel, note): the port switch's note senders, which the
 *   stock KEYBOARD dispatcher uses for its own MIDI out. The pads send through these rather than fw_note_on: the live
 *   note routine queues every note it plays for the stock's recorder (0x400f050e), which would record the pads again,
 *   on steps of its own. */
#define FW_REC_OBJ          (*(uint8_t *volatile *)0x419f0dd8)
#define REC_LIVE            0x169
#define TRACK_OBJ_FIRST     0x11f4
#define TRACK_OBJ_SETS      128
#define TRACK_OBJ_SET_STEP  0x38c
#define PATTERN_SIZE        0x1611d
#define TRACK_OBJ_STRIDE    0x58
#define TRACK_OBJ_DATA      16
#define TRACK_SLOTS         448
typedef void (*trig_set_fn)(void *obj, int32_t step, int32_t mask, int32_t on);
typedef void (*step_note_fn)(void *obj, int32_t step, int32_t note, int32_t slot);
#define FW_TRIG_SET         ((trig_set_fn)0x4003673a)
#define FW_STEP_NOTE        ((step_note_fn)0x40035858)
#define MIDI_TRACK_STATE    0x419c5b30
#define MIDI_TRACK_STRIDE   158
#define MIDI_TRACK_CHAN     38
#define MIDI_TRACK_ON       0x419c5b10
typedef int32_t (*midi_on_fn)(int32_t channel, int32_t note, int32_t velocity);
typedef int32_t (*midi_off_fn)(int32_t channel, int32_t note);
#define FW_MIDI_NOTE_ON     ((midi_on_fn)0x400ee232)
#define FW_MIDI_NOTE_OFF    ((midi_off_fn)0x400ee29e)
/* The parameter pages' class (TRIG, SYN1, SYN2, FLTR, AMP, LFO, on every track: one class in the firmware), by its
 * vtable: the corner is drawn only while one of them is the view on top (fw_view), not over the settings menus, the
 * project list, TEMPO or the pattern screens, which measured as other classes in digiemu. */
#define PAGE_VTABLE         0x401c3f34u
/* 0.8.3: after a pattern or bank selection (PTN/BANK + a trig key) one of the firmware's permanent views (its first
 * word 0x401c3cf0, built at boot by 0x400af2ec) stays on the main view stack above the page, drawing nothing; with
 * it on top fw_view() isn't the page, so Drum Mode drew nothing on any pattern picked that way (measured in digiemu:
 * on H02 the DRUM pages came up empty and the corner strip was gone). The stack is walked here past such views:
 * MainScreen (its pointer at 0x419f0e50 on OS 1.44, as core's fw_view reads it), the list's head at +0x54, the top
 * node at +0x58, a node's next at +4 and its view at +8. */
#define MAIN_SCREEN_PP      0x419f0e50u
#define IDLE_VIEW_VTABLE    0x401c3cf0u
/* 0.7: GRID RECORDING is byte +0x168 of the recorder's state, read only while +0x161 is set (the stock's getter,
 * 0x400adcb4); the page of steps the trig keys show is the long at 0x41368ce8 (0 = steps 1-16; measured in digiemu,
 * a press of PAGE on a 48-step track counting 0, 1, 2, 0); the LED setter, set(mgr, led, layer, colour), the one
 * DigiArp calls, refuses an LED someone set this frame, so what ev_key_leds sets stays over the stock views'.
 * The trig keys are LEDs 1-16 (the Digitakt II's numbering too: a trig key's code - 25); colours are the panel's palette (2 dark, 3 red, 5 dim red, 20 white). */
#define REC_GRID_ON         0x161
#define REC_GRID            0x168
#define FW_GRID_PAGE        (*(volatile int32_t *)0x41368ce8)
typedef void (*led_set_fn)(void *mgr, int32_t led, int32_t layer, int32_t colour);
#define FW_LED_SET          ((led_set_fn)0x40016e62)
#define LED_LAYER           2
#define LED_DARK            2
#define LED_RED             3
#define LED_DIM_RED         5
#define LED_WHITE           20

#define DM_TRACKS       4               /* MIDI tracks A-D: firmware tracks 4-7 */
#define DM_FIRST        4
#define DM_LANES        8

#define KEY_TRIG        20              /* the TRIG page key: held, a pad selects its lane */
#define KEY_STEP1       26              /* trig keys 1-16: 26-41 */
#define KEY_STEPS       16
#define ENC_A           1               /* ev_enc's encoders: 1-8, A-H */
#define ENC_B           2
#define COUNTS          4               /* encoder counts a detent */


#define FLASH_TICKS     6               /* how long a lane stays lit after a hit, in UI ticks */
#define SEQ_FLASH_TICKS 3               /* and after the sequencer plays it: shorter, so 16ths blink apart */
#define NOTE_TICKS      60              /* how long a corner message shows */
#define INFO_TICKS      90              /* how long a pad's note and velocity show after its pad is let go */

#define SAVE_VERSION    1

/* The default drum map (General MIDI), and the lanes' short names for the corner. */
static const uint8_t dm_default_notes[DM_LANES] = { 36, 38, 42, 46, 39, 45, 50, 49 };
static const char *const dm_names[DM_LANES] = { "BD", "SD", "CH", "OH", "CP", "LT", "HT", "CY" };
static const char *const dm_mute_msg[DM_LANES] = { "BD MUTE", "SD MUTE", "CH MUTE", "OH MUTE",
                                                   "CP MUTE", "LT MUTE", "HT MUTE", "CY MUTE" };
static const char *const dm_unmute_msg[DM_LANES] = { "BD ON", "SD ON", "CH ON", "OH ON",
                                                     "CP ON", "LT ON", "HT ON", "CY ON" };
static const char *const dm_select_msg[DM_LANES] = { "EDIT BD", "EDIT SD", "EDIT CH", "EDIT OH",
                                                     "EDIT CP", "EDIT LT", "EDIT HT", "EDIT CY" };

/* What the project keeps (project data "DRUM"). A project saved without Drum Mode loads as all zeros: version 0,
 * which dm_loaded turns into the defaults. */
struct dm_save {
    uint8_t version;
    uint8_t enabled;                    /* bit per track */
    uint8_t mutes[DM_TRACKS];           /* bit per lane: left out of what the sequencer plays */
    uint8_t selected[2];                /* the selected lane per track, a nibble each (A low, B high, ...); 0.6 kept 0 */
    uint8_t note[DM_TRACKS][DM_LANES];
    uint8_t velocity[DM_TRACKS][DM_LANES];
};
static struct dm_save dm;

/* What each trig key is sounding, so its release ends the same note on the same track, whatever changed since. */
struct dm_held {
    int8_t track;                       /* firmware track 4-7, or -1: nothing held */
    int8_t channel;                     /* the channel its note went out on, or -1: none (CHAN off) */
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
static int32_t dm_trig_held;            /* the TRIG page key is down */
static void *dm_brain;                  /* the UI's Brain, from the key and knob events: for core_page_shown */
static int32_t dm_page_acc[8];          /* counts towards the next detent on the DRUM pages, A-H */
volatile int32_t dm_grid_edits;         /* steps toggled in GRID RECORDING, for the tests */

static int32_t selected_lane(int32_t t)
{
    return (dm.selected[t >> 1] >> (4 * (t & 1))) & 7;
}

static void select_lane(int32_t t, int32_t lane)
{
    uint8_t *b = &dm.selected[t >> 1];
    *b = (uint8_t)((*b & ~(0xf << (4 * (t & 1)))) | (lane << (4 * (t & 1))));
}

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

/* ---- live recording ------------------------------------------------------------------------- */
#define TIMELINE_16TH   5400000u        /* fw_timeline units a 16th */
#define LEAD_STEPS      1               /* the sequencer fires a step this many steps before it plays */

/* Each Drum Mode track's last fired steps, from seq-events' trig stage (interrupt level): the step and the timeline
 * at its fire, and the one before, for the step's length. */
struct dm_fire {
    volatile int32_t step, prev_step;
    volatile uint32_t time, prev_time;
};
static struct dm_fire dm_fire[DM_TRACKS];
static volatile uint8_t dm_step_vel[DM_TRACKS];        /* the velocity the step being fired plays at, 0 stock's */
/* The strip follows playback. A step is fired one step before it plays: its lanes wait in dm_seq_next until the next
 * fire, when the step plays and they move to dm_seq_hits, which the UI tick turns into blinks. Bit per lane. */
static volatile uint8_t dm_seq_next[DM_TRACKS];
static volatile uint8_t dm_seq_hits[DM_TRACKS];

volatile int32_t dm_rec_count;          /* hits recorded, for the tests */
volatile int32_t dm_rec_last_step = -1;

static int32_t live_recording(void)
{
    uint8_t *rec = FW_REC_OBJ;
    return rec && rec[REC_LIVE] && fw_transport == 1;
}

static void *track_obj(int32_t track)
{
    uint8_t *first = fw_project + TRACK_OBJ_FIRST + TRACK_OBJ_STRIDE * track;
    uint8_t *want = fw_pattern + 976 * track;
    uint32_t idx = (uint32_t)(fw_pattern - fw_patterns) / PATTERN_SIZE;
    uint8_t *obj;
    if (idx < TRACK_OBJ_SETS) {
        obj = first + TRACK_OBJ_SET_STEP * idx;
        if (*(void *const *)obj == *(void *const *)first && *(uint8_t *const volatile *)(obj + TRACK_OBJ_DATA) == want)
            return obj;
    }
    for (int32_t k = 0; k < TRACK_OBJ_SETS; k++) {
        obj = first + TRACK_OBJ_SET_STEP * k;
        if (*(void *const *)obj == *(void *const *)first && *(uint8_t *const volatile *)(obj + TRACK_OBJ_DATA) == want)
            return obj;
    }
    return first;
}

static uint8_t *track_data(void *obj)
{
    void *const *vt = *(void *const *const *)obj;
    return (uint8_t *)((uint32_t (*)(void *))vt[10])(obj);
}

/* The step a hit at the timeline's now belongs to: the nearest step to the one playing. The last fired step S
 * plays LEAD_STEPS steps after its fire, so the playing position is S - LEAD_STEPS + (now - fire) / length. */
static int32_t hit_step(int32_t track)
{
    struct dm_fire *f = &dm_fire[track - DM_FIRST];
    struct sq_track info;
    int32_t s = f->step, len, k;
    uint32_t t = f->time, d = TIMELINE_16TH;
    int32_t x;
    if (s < 0 || sq_track_info(track, &info, sizeof info) < 0)
        return -1;
    len = info.len < 1 ? 1 : info.len;
    if (f->prev_step >= 0 && (f->prev_step + 1) % len == s) {
        uint32_t dd = t - f->prev_time;
        if (dd > TIMELINE_16TH / 8 && dd < TIMELINE_16TH * 16)
            d = dd;                     /* the track's speed scale, and swing, as it plays */
    }
    x = (int32_t)(fw_timeline - t) - (int32_t)(LEAD_STEPS * d) + (int32_t)(d / 2);
    k = x >= 0 ? x / (int32_t)d : -((-x + (int32_t)d - 1) / (int32_t)d);
    s += k;
    s %= len;
    if (s < 0)
        s += len;
    return s;
}

/* Add a note to a step of a track as the stock's live recorder does: a trig if there is none (the note in slot 0,
 * the intervals cleared), else the note as the first free interval, unless the step already plays it or is full. */
static void record_note(int32_t track, int32_t step, int32_t note)
{
    void *obj = track_obj(track);
    uint8_t *data = track_data(obj), *slots;
    int32_t base, free_slot = -1;
    if (!data || step < 0 || step > 63)
        return;
    slots = data + TRACK_SLOTS + 8 * step;
    if (!(*(volatile uint16_t *)(data + 2 * step) & FW_STEP_TRIG)) {
        FW_TRIG_SET(obj, step, FW_STEP_TRIG, 1);
        FW_STEP_NOTE(obj, step, note, 0);
        for (int32_t k = 1; k < 8; k++)
            FW_STEP_NOTE(obj, step, 0, k);
    } else {
        base = slots[0];
        if (base == note)
            return;
        for (int32_t k = 1; k < 8; k++) {
            int8_t iv = (int8_t)slots[k];
            if (iv && base + iv == note)
                return;
            if (!iv && free_slot < 0)
                free_slot = k;
        }
        if (free_slot < 0 || note - base < -127 || note - base > 127)
            return;
        FW_STEP_NOTE(obj, step, note - base, free_slot);
    }
    dm_rec_count++;
    dm_rec_last_step = step;
}

/* A MIDI track's output channel (0-15), or -1 when its CHAN is off. track: the firmware's, 4-7. */
static int32_t midi_channel(int32_t track)
{
    int32_t m = track - DM_FIRST, ch;
    if (!(*(volatile uint32_t *)(MIDI_TRACK_ON + 4 * (186 + m)) & 1))
        return -1;
    ch = *(volatile int8_t *)(MIDI_TRACK_STATE + MIDI_TRACK_STRIDE * m + MIDI_TRACK_CHAN);
    return ch >= 0 && ch < 16 ? ch : -1;
}

/* ---- grid editing -------------------------------------------------------------------------- */
static int32_t grid_recording(void)
{
    uint8_t *rec = FW_REC_OBJ;
    return rec && rec[REC_GRID_ON] && rec[REC_GRID];
}

static int32_t track_len(int32_t track)
{
    struct sq_track info;
    if (sq_track_info(track, &info, sizeof info) < 0 || info.len < 1)
        return 16;
    return info.len > 64 ? 64 : info.len;
}

static int32_t grid_page(int32_t track)
{
    int32_t p = FW_GRID_PAGE, pages = (track_len(track) + 15) / 16;
    return p >= 0 && p < pages ? p : 0;
}

/* Whether a step of a track's pattern data plays note (a trig, and the note in its slot 0 or as an interval). */
static int32_t step_has(const uint8_t *data, int32_t step, int32_t note)
{
    const uint8_t *slots = data + TRACK_SLOTS + 8 * step;
    int32_t base = slots[0];
    if (!(*(const volatile uint16_t *)(data + 2 * step) & FW_STEP_TRIG))
        return 0;
    if (base == note)
        return 1;
    for (int32_t k = 1; k < 8; k++)
        if ((int8_t)slots[k] && base + (int8_t)slots[k] == note)
            return 1;
    return 0;
}

/* Take a note out of a step: its other notes are written back (the first as slot 0, the rest as intervals from it),
 * and the trig goes with the last note, as the stock's own trig key would take it. */
static void remove_note(int32_t track, int32_t step, int32_t note)
{
    void *obj = track_obj(track);
    uint8_t *data = track_data(obj), *slots;
    int32_t keep[8], n = 0, base;
    if (!data)
        return;
    slots = data + TRACK_SLOTS + 8 * step;
    base = slots[0];
    if (base != note)
        keep[n++] = base;
    for (int32_t k = 1; k < 8; k++) {
        int32_t iv = (int8_t)slots[k];
        if (iv && base + iv != note && base + iv >= 0 && base + iv <= 127)
            keep[n++] = base + iv;
    }
    if (!n) {
        FW_TRIG_SET(obj, step, FW_STEP_TRIG, 0);
        FW_TRIG_SET(obj, step, 2, 0);
        return;
    }
    FW_STEP_NOTE(obj, step, keep[0], 0);
    for (int32_t k = 1; k < 8; k++)
        FW_STEP_NOTE(obj, step, k < n ? keep[k] - keep[0] : 0, k);
}

/* A trig key in GRID RECORDING: the selected lane on or off on that step of the shown page. */
static void grid_toggle(int32_t track, int32_t key)
{
    int32_t t = track - DM_FIRST, lane = selected_lane(t), note = dm.note[t][lane];
    int32_t step = 16 * grid_page(track) + key;
    uint8_t *data = track_data(track_obj(track));
    if (!data || step >= track_len(track))
        return;
    if (step_has(data, step, note))
        remove_note(track, step, note);
    else
        record_note(track, step, note);
    dm_grid_edits++;
    dm_flash[t][lane] = FLASH_TICKS;
    dm_dirty = 1;
}

/* seq-events, interrupt level. The trig stage: note each Drum Mode track's fired step and its time (first read only;
 * the trig is left as it is). */
void dm_gen_trig(int32_t track, int32_t step, int32_t first, uint32_t *trig)
{
    struct dm_fire *f;
    (void)trig;
    if (!first || track < DM_FIRST || track >= DM_FIRST + DM_TRACKS)
        return;
    f = &dm_fire[track - DM_FIRST];
    f->prev_step = f->step;
    f->prev_time = f->time;
    f->time = fw_timeline;
    f->step = step;
    dm_seq_hits[track - DM_FIRST] |= dm_seq_next[track - DM_FIRST];   /* the step fired before plays now */
    dm_seq_next[track - DM_FIRST] = 0;
}

/* The notes stage: on a Drum Mode track, the notes of muted lanes are taken out of the step (a step left with none
 * plays nothing), and a step whose notes are lanes plays at the loudest of their velocities. */
void dm_fire_notes(int32_t track, int32_t step, int8_t notes[SQ_NOTES])
{
    int32_t t = track - DM_FIRST, vel = 0, n_keep = 0;
    int32_t keep[SQ_NOTES];
    uint8_t mutes, lanes = 0;
    (void)step;
    if (!drum_track(track))
        return;
    mutes = dm.mutes[t];
    if (notes[0] >= 0)
        for (int32_t i = 0; i < SQ_NOTES; i++) {
            int32_t n, muted = 0;
            if (i && !notes[i])
                continue;
            n = i ? notes[0] + notes[i] : notes[0];
            for (int32_t l = 0; l < DM_LANES; l++)
                if (dm.note[t][l] == n) {
                    if ((mutes >> l) & 1) {
                        muted = 1;
                    } else {
                        if (dm.velocity[t][l] > vel)
                            vel = dm.velocity[t][l];
                        lanes |= (uint8_t)(1u << l);
                    }
                }
            if (!muted && n >= 0 && n <= 127)
                keep[n_keep++] = n;
        }
    if (mutes && notes[0] >= 0) {               /* rebuild: the first kept note, the others as intervals from it */
        notes[0] = (int8_t)(n_keep ? keep[0] : -1);
        for (int32_t i = 1; i < SQ_NOTES; i++)
            notes[i] = (int8_t)(i < n_keep ? keep[i] - keep[0] : 0);
    }
    dm_step_vel[t] = (uint8_t)vel;
    dm_seq_next[t] = lanes;
}

void dm_fire_vel(int32_t track, int32_t step, int32_t *vel)
{
    (void)step;
    if (drum_track(track) && dm_step_vel[track - DM_FIRST])
        *vel = dm_step_vel[track - DM_FIRST];
}

static void lane_on(int32_t key, int32_t track, int32_t lane)
{
    int32_t t = track - DM_FIRST, ch = midi_channel(track);
    if (ch >= 0)
        FW_MIDI_NOTE_ON(ch, dm.note[t][lane], dm.velocity[t][lane]);
    if (live_recording())
        record_note(track, hit_step(track), dm.note[t][lane]);
    dm_held[key].channel = (int8_t)ch;
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
    if (h->channel >= 0)
        FW_MIDI_NOTE_OFF(h->channel, h->note);
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
    for (int32_t t = 0; t < DM_TRACKS; t++)
        dm_fire[t].step = dm_fire[t].prev_step = -1;
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

/* ---- the DRUM pages --------------------------------------------------------------------------- */
/* Pages 27 and 28 follow the MIDI tracks' LFO page (23), so LFO pressed again on a MIDI track steps to them. Not after
 * MIDI Source (20): measured in digiemu, its FUNC + push A (CHAN on) stops working with any mod page after it; not
 * after TRIG (19): TRIG + pad selects a lane, and each TRIG press would step the page. Their knob boxes are empty
 * (id 0): Drum Mode draws the 8 lanes over them and takes the knobs while one is shown. */
#define PAGE_MIDI_LFO   23
struct core_page dm_page_notes = { 27, PAGE_MIDI_LFO, "DRMN", "Drum Notes", { 0 }, 0, 0, 0 };
struct core_page dm_page_vel = { 28, PAGE_MIDI_LFO, "DRMV", "Drum Velocity", { 0 }, 0, 0, 0 };

/* 1 the notes page, 2 the velocity page, 0 neither, on screen with nothing over it. core_page_shown compares the
 * view with the one core noted in the descriptor, the last built, so this asks the shown view itself (core-dn1's pages.s and lists.c): the Brain's view stack (+0x54, and
 * +0x5c the same while nothing is over the pages), its page manager (+8), the key view shown (+0xc0), and that
 * view's list (a std::vector at +124, +128) at its current place (+144). */
static int32_t drum_page(void)
{
    uint8_t *b = dm_brain, *top, *mgr, *v;
    int32_t *beg, *end, cur;
    if (!b)
        return 0;
    top = *(uint8_t **)(b + 0x54);
    if (!top || top != *(uint8_t **)(b + 0x5c))
        return 0;
    mgr = *(uint8_t **)(top + 8);
    if (!mgr || !(v = *(uint8_t **)(mgr + 0xc0)))
        return 0;
    beg = *(int32_t **)(v + 124);
    end = *(int32_t **)(v + 128);
    cur = *(int32_t *)(v + 144);
    if (!beg || cur < 0 || beg + cur >= end)
        return 0;
    return beg[cur] == dm_page_notes.idx ? 1 : beg[cur] == dm_page_vel.idx ? 2 : 0;
}

/* A knob on a DRUM page: knob A-H is lane 1-8. A new note plays at once (on, then off), as when a pad is held. */
static void page_turn(int32_t page, int32_t enc, int32_t counts)
{
    int32_t track = fw_active_track, t = track - DM_FIRST, lane = enc - 1, steps = 0, ch;
    int32_t *acc = &dm_page_acc[lane];
    *acc += counts;
    while (*acc >= COUNTS) {
        *acc -= COUNTS;
        steps++;
    }
    while (*acc <= -COUNTS) {
        *acc += COUNTS;
        steps--;
    }
    if (!steps)
        return;
    if (page == 1) {
        int32_t n = dm.note[t][lane] + steps;
        n = n < 0 ? 0 : n > 127 ? 127 : n;
        if (n == dm.note[t][lane])
            return;
        dm.note[t][lane] = (uint8_t)n;
        ch = midi_channel(track);
        if (ch >= 0) {
            FW_MIDI_NOTE_ON(ch, n, dm.velocity[t][lane]);
            FW_MIDI_NOTE_OFF(ch, n);
        }
        dm_flash[t][lane] = FLASH_TICKS;
    } else {
        int32_t v = dm.velocity[t][lane] + steps;
        dm.velocity[t][lane] = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
    }
    dm_dirty = 1;
}

/* ---- keys ------------------------------------------------------------------------------------ */
/* The UI task, each key event. On a Drum Mode track a trig key's press is taken (FUNC + pad 1-8 toggles its lane's
 * mute; FUNC + 9-16 is left to the stock), and then that key's repeats and release, whatever changed meanwhile: a release always ends what its
 * press played, and a key the stock saw go down is never kept from it on the way up. */
int32_t dm_key(void *brain, void *ev)
{
    int32_t key = CORE_KEY_ID(ev);
    uint32_t flags = CORE_KEY_FLAGS(ev);
    int32_t up = (flags & CORE_KEY_UP) || !(flags & CORE_KEY_DOWN);
    int32_t k, track = fw_active_track;
    uint32_t bit;
    dm_brain = brain;

    if (key == KEY_TRIG) {
        dm_trig_held = !up;             /* watched, never taken: the page key still works */
        return 0;
    }
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
    if (up || (flags & CORE_KEY_REPEAT) || !drum_track(track))
        return 0;
    if (flags & CORE_KEY_FUNC) {
        int32_t t = track - DM_FIRST;
        if (k >= DM_LANES)
            return 0;                   /* FUNC + 9-16: the stock's */
        dm.mutes[t] ^= (uint8_t)(1u << k);
        dm_taken |= bit;                /* its release is ours too; no note */
        say(((dm.mutes[t] >> k) & 1) ? dm_mute_msg[k] : dm_unmute_msg[k], 0);
        return 1;
    }
    if (dm_trig_held && k < DM_LANES) {    /* TRIG + pad: select the lane, silently */
        select_lane(track - DM_FIRST, k);
        dm_taken |= bit;
        say(dm_select_msg[k], 0);
        return 1;
    }
    dm_taken |= bit;
    if (grid_recording()) {             /* GRID RECORDING: the 16 keys are the selected lane's steps */
        grid_toggle(track, k);
        return 1;
    }
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
    dm_brain = brain;

    if (k < 0 || !(dm_taken & (1u << k)) || dm_held[k].track < 0) {
        int32_t page = drum_track(fw_active_track) ? drum_page() : 0;
        if (!page || enc < 1 || enc > 8)
            return 0;
        page_turn(page, enc, counts);
        return 1;                       /* a DRUM page's knob: ours */
    }
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
    for (int32_t t = 0; t < DM_TRACKS; t++) {
        uint8_t hits = dm_seq_hits[t];
        if (!hits)
            continue;
        dm_seq_hits[t] &= (uint8_t)~hits;
        if (fw_transport != 1)
            continue;                   /* stopped since: nothing plays */
        for (int32_t l = 0; l < DM_LANES; l++)
            if (((hits >> l) & 1) && dm_flash[t][l] < SEQ_FLASH_TICKS)
                dm_flash[t][l] = SEQ_FLASH_TICKS;
        dm_dirty = 1;
    }
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
    {                                   /* the LEDs follow GRID RECORDING, its page and the pattern */
        static int32_t last_grid = -1, last_page = -1, last_track = -1;
        int32_t g = grid_recording(), track = fw_active_track;
        int32_t p = drum_track(track) ? grid_page(track) : 0;
        if (g != last_grid || p != last_page || track != last_track)
            dm_dirty = 1;
        last_grid = g;
        last_page = p;
        last_track = track;
    }
    if (dm_dirty || dm_note_ticks > 0) {
        *((unsigned char *)ctrl + 0x20) = 1;    /* redraw */
        *((unsigned char *)ctrl + 0x21) = 1;    /* and an LED frame */
        dm_dirty = 0;
    }
}

/* The key-LED frame (core's ev_key_leds, before the stock views set theirs): on a Drum Mode track,
 * - in GRID RECORDING the shown page's steps: red where the selected lane plays, dark elsewhere; the playing step and
 *   steps past the track's length are left to the stock;
 * - else trig keys 1-8 as pads: white while held or just played, dim red when muted, dark otherwise; 9-16 dark. */
void dm_leds(void *mgr)
{
    int32_t track = fw_active_track, t;
    if (!drum_track(track))
        return;
    t = track - DM_FIRST;
    if (grid_recording()) {
        uint8_t *data = track_data(track_obj(track));
        int32_t len = track_len(track), first = 16 * grid_page(track), note = dm.note[t][selected_lane(t)];
        int32_t playing = -1;
        if (!data)
            return;
        if (fw_transport == 1 && dm_fire[t].step >= 0)
            playing = (dm_fire[t].step - LEAD_STEPS + len) % len;
        for (int32_t k = 0; k < KEY_STEPS; k++) {
            int32_t step = first + k;
            if (step >= len || step == playing)
                continue;
            FW_LED_SET(mgr, k + 1, LED_LAYER, step_has(data, step, note) ? LED_RED : LED_DARK);
        }
        return;
    }
    for (int32_t k = 0; k < KEY_STEPS; k++) {
        int32_t c = LED_DARK;
        if (k < DM_LANES) {
            if (((dm_lit_held[t] >> k) & 1) || dm_flash[t][k])
                c = LED_WHITE;
            else if ((dm.mutes[t] >> k) & 1)
                c = LED_DIM_RED;
        }
        FW_LED_SET(mgr, k + 1, LED_LAYER, c);
    }
}

/* The top-right corner of the parameter pages (not over menus: on_page), drawn by inverting a cleared box so it shows on any page:
 * - a message (x 84-127, y 54-63, the box digisolo uses);
 * - a pad's lane, note and velocity ("BD 36 V100", x 64-127) while it is edited or held a moment, and a little after;
 * - else on a Drum Mode track the 8 lanes as small boxes (x 94-127), filled while held or just hit, kept narrow so
 *   the title bar's text beside it stays readable. */
static int32_t dm_info_lane = -1, dm_info_track = -1;

static void *shown_view(void)
{
    uint8_t *ms = *(uint8_t *const volatile *)MAIN_SCREEN_PP, *head, *node, *v;
    if (!ms)
        return fw_view();
    head = ms + 0x54;
    node = *(uint8_t *const volatile *)(ms + 0x58);
    for (int32_t i = 0; i < 4 && node && node != head; i++) {
        v = *(uint8_t *const volatile *)(node + 8);
        if (!v || *(volatile uint32_t *)v != IDLE_VIEW_VTABLE)
            return v;
        node = *(uint8_t *const volatile *)(node + 4);
    }
    return 0;
}

static int32_t on_page(void)
{
    void *v = shown_view();
    return v && *(volatile uint32_t *)v == PAGE_VTABLE;
}

static const char *const dm_note_names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

/* A DRUM page's body, over its empty knob boxes: 4 x 2 cells (x 20-127, the screen's rows 12-63), lane 1-8 in knob
 * order, each with the lane's name and its note (name and number) or velocity; a muted lane's velocity cell says
 * MUTE, and the selected lane's cell is inverted. Coordinates: x from the left, y from the bottom (a 5-pixel text's
 * y is its bottom row), so a row r from the top is y 63 - r. */
static void draw_drum_page(void *bmp, int32_t page, int32_t track)
{
    int32_t t = track - DM_FIRST, sel = selected_lane(t);
    ((rect_fn)fw_fillrect)(bmp, 20, 0, 127, 52, 0);
    if (!drum_track(track)) {
        ((text_fn)fw_textf)(bmp, (const void *)fw_font5, 30, 34, -1, "DRUM MODE IS OFF");
        ((text_fn)fw_textf)(bmp, (const void *)fw_font5, 30, 24, -1, "HOLD A TRACK KEY");
        return;
    }
    for (int32_t l = 0; l < DM_LANES; l++) {
        int32_t x = 20 + 27 * (l & 3), top = 12 + 26 * (l >> 2);     /* the cell's top row from the screen's top */
        int32_t y0 = 63 - (top + 24), y1 = 63 - (top + 1);
        ((rect_fn)fw_framerect)(bmp, x + 1, y0, x + 25, y1, -1);
        ((text_fn)fw_textf)(bmp, (const void *)fw_font5, x + 4, 59 - (top + 3), -1, dm_names[l]);
        if (page == 1) {
            int32_t n = dm.note[t][l];
            ((text_fn)fw_textf)(bmp, (const void *)fw_font5, x + 4, 59 - (top + 11), -1, "%s%d",
                                dm_note_names[n % 12], n / 12);
            ((text_fn)fw_textf)(bmp, (const void *)fw_font5, x + 4, 59 - (top + 18), -1, "%d", n);
        } else {
            ((text_fn)fw_textf)(bmp, (const void *)fw_font5, x + 4, 59 - (top + 11), -1, "%d",
                                (int)dm.velocity[t][l]);
            if ((dm.mutes[t] >> l) & 1)
                ((text_fn)fw_textf)(bmp, (const void *)fw_font5, x + 4, 59 - (top + 18), -1, "MUTE");
        }
        if (l == sel)
            ((rect_fn)fw_fillrect)(bmp, x + 2, y0 + 1, x + 24, y1 - 1, -1);
    }
}

void dm_draw(void *bmp, void *ctrl)
{
    int32_t track = fw_active_track, t, page;
    (void)ctrl;
    if (!on_page())
        return;                         /* a menu or another screen is on top: leave it alone */
    page = (track >= DM_FIRST && track < DM_FIRST + DM_TRACKS) ? drum_page() : 0;
    if (page) {
        draw_drum_page(bmp, page, track);
        if (!(dm_note_ticks > 0 && dm_note))
            return;                     /* the page shows the lanes: no strip over its title */
    }
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
        else if ((dm.mutes[t] >> l) & 1)
            ((rect_fn)fw_fillrect)(bmp, x0, 58, x1, 59, -1);     /* muted: a dash */
        else
            ((rect_fn)fw_framerect)(bmp, x0, 56, x1, 61, -1);
    }
    {                                   /* the selected lane: a line under its box (y counts up from the bottom) */
        int32_t x0 = 96 + 4 * selected_lane(t);
        ((rect_fn)fw_fillrect)(bmp, x0, 54, x0 + 2, 54, -1);
    }
}
