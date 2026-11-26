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
 * - FUNC + a pad mutes or unmutes its lane (0.9.3: not in any recording mode, where FUNC + trig keys are the stock's): the sequencer leaves a muted lane's note out of every step it plays (a step
 *   of muted lanes only plays nothing); its steps stay in the pattern. The pad itself still plays and records. The
 *   corner strip shows a muted lane as a dash. Saved with the project.
 *
 * - 0.9: a pad press selects its lane as it plays (not during live recording, where pads only play and record); its
 *   key stays lit dim white (red when muted). Holding the TRIG page key and pressing a pad still selects without a
 *   note. The corner strip underlines the selected lane.
 * - GRID RECORDING (RECORD while stopped) on a Drum Mode track: the 16 trig keys are the shown page's steps for the
 *   selected lane. A press adds the lane's note to the step (as live recording does) or takes it out (the step's
 *   trig goes when no note is left). The trig keys light red where the selected lane plays; the playing step is the
 *   stock's. Outside GRID RECORDING the pads light white while held or playing, dim red when muted, and the
 *   selected lane's dim white.
 * - 0.11.0: a pattern reload brings back the saved pattern's kit (pins; at most 8 kits); ratchets (ROLL, knob E of the TRIG page on held steps in GRID RECORDING: a drum plays 2-4 times in a
 *   step, sent by Drum Mode's own note task between steps) and note repeat (RPT, knob G: a held pad repeats at 1/16,
 *   1/32 or 1/64 while playing, recorded with its rolls in LIVE RECORDING).
 * - 0.10.6: the running light red for the whole step (the stock's playhead); the LFO page's title says "LFO" on a
 *   Drum Mode track, not "LFO (1/2)".
 * - 0.10.5: the DRUM page's title shows its own counter (Drum Notes 1/2, Drum Velocity 2/2); the running light uses
 *   all 16 keys, one per step, over the pads for its half step.
 * - 0.10: Drum Mode's settings per pattern and per track (on/off, mutes, kit), the kits (notes, velocities) in the
 *   project, at most 8 (10 until 0.11); a new pattern takes a copy of the one played before; FUNC + RECORD / STOP carries them.
 * - 0.9.4: a change to Drum Mode on/off, a mute, a lane note or a velocity reaches step 1 of the next run too.
 * - 0.9.2: the stock keeps its trig key functions: pattern/bank select and chains (while its select view is open,
 *   PTN or BANK is held, or a trig key it took is still held), and in GRID RECORDING a held step is the stock's held
 *   trig (locks, micro timing, copy/paste) while a quick tap still adds or takes out the lane.
 * - 0.9.1: while playing, keys 9-16 are a running light: each key 2 steps of the playing 16-step page, lit white for
 *   the first half of each of its steps (two blinks a key), dark when stopped.
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
#define REC_STEP            0x16a   /* 0.9.3: STEP RECORDING (RECORD + STOP; it sets REC_GRID too) */
#define FW_GRID_PAGE        (*(volatile int32_t *)0x41368ce8)
typedef void (*led_set_fn)(void *mgr, int32_t led, int32_t layer, int32_t colour);
#define FW_LED_SET          ((led_set_fn)0x40016e62)
#define LED_LAYER           2
#define LED_DARK            2
#define LED_RED             3
#define LED_DIM_RED         5
#define LED_WHITE           20
#define LED_DIM_WHITE       21      /* 0.9: the selected lane's pad (the stock's own dim white) */

#define DM_TRACKS       4               /* MIDI tracks A-D: firmware tracks 4-7 */
#define DM_FIRST        4
#define DM_LANES        8

#define KEY_TRIG        20              /* the TRIG page key: held, a pad selects its lane */
#define KEY_RECORD      10              /* 0.10: FUNC + RECORD copies the pattern, FUNC + STOP pastes it */
#define KEY_STOP        12
#define KEY_PTN         4               /* 0.9.2: PTN and BANK, held: the trig keys pick patterns and banks */
#define KEY_BANK        5
#define KEY_STEP1       26              /* trig keys 1-16: 26-41 */
#define KEY_STEPS       16
#define ENC_A           1               /* ev_enc's encoders: 1-8, A-H */
#define ENC_B           2
#define ENC_E           5
#define ENC_G           7
#define PAGE_MIDI_TRIG  19              /* the MIDI tracks' TRIG page: knob E's box is empty on the stock */
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

/* 0.10: Drum Mode's settings per pattern and per track. dm is the playing pattern's working copy; what's kept:
 * - in each pattern, one core lane parameter (id 191, a value per track, saved with the pattern): the track's Drum
 *   Mode on/off, its 8 lanes' mutes and the number of its kit (LV_ below); 0 means the pattern has none yet;
 * - in the project ("DRMK", a variable block: core keeps all mods' project data in 480 bytes, DigiArp's takes 272):
 *   up to 8 kits (8 notes and 8 velocities; 10 until 0.11), each with its number of uses (pattern tracks and pins), and the selected
 *   lanes. A kit used by one pattern track is edited in place, one used by more is copied first; identical kits
 *   are merged. A pattern without settings yet takes a copy of the one the player came from. */
#define DM_LANE_ID      191
#define LV_SET          0x2000          /* the pattern track has settings */
#define LV_ON           0x1000          /* Drum Mode on */
#define LV_KIT_SHIFT    8               /* kit number, 4 bits */
#define LV_MAX          0x3fff
#define MAX_KITS        8               /* 0.11: 8 (10 before), to make room for the saved patterns' pins */
#define DM_PATTERNS     128
#define KITS_VERSION    2               /* 0.11: pins; 1 is read and moved to 2 at load */
#define MAX_PINS        8
struct dm_kit {
    uint8_t note[DM_LANES];
    uint8_t velocity[DM_LANES];
};
struct dm_kitstore {
    uint32_t bytes;                     /* +0  what the block holds now (core's variable block) */
    uint8_t version;                    /* +4 */
    uint8_t nkits;                      /* +5  kits 0..nkits-1 are in the block */
    uint8_t selected[2];                /* +6  the selected lane per track, a nibble each */
    uint8_t refs[12];                   /* +8  uses of each kit (8 used): pattern tracks, and pins */
    uint8_t npins, pad[3];              /* +20 0.11 */
    uint16_t pins[MAX_PINS];            /* +24 a saved pattern track's kit: pattern << 2 | track, kit << 12 */
    struct dm_kit kit[9];               /* +40 8 used; the ninth keeps the block as big as 0.10's (180 bytes), so a
                                         * version 1 block loads whole before it is moved */
};
#define KITS_HEAD       40
#define KITS_HEAD_V1    20
/* 0.11, the saved patterns' kits (pins): a pattern save (FUNC + YES, SAVE TO PROJ) keeps each track's lane value,
 * the kit's number and not its notes. So the kit a saved pattern track uses gets a pin, which counts as a use: it is
 * never edited in place (an edit copies it) nor freed, and a reload (FUNC + NO, RELOAD FROM PROJ) brings it back as it
 * was. A pattern track saved again moves its pin to the new kit; with all 8 in use the oldest pin goes ("PINS FULL"),
 * and that saved copy's kit may change again. (0.10: a reload gave Drum Mode off or another pattern's newest kit.) */
static struct dm_kitstore dm_store;

/* The lane parameter itself (core_params): a track value only, on no page or knob; 0-0x3fff, 0 none yet. */
static void dm_lane_format(int32_t value, char *buf)
{
    static const char hex[] = "0123456789ABCDEF";
    value >>= 8;
    buf[0] = hex[(value >> 12) & 0xf];
    buf[1] = hex[(value >> 8) & 0xf];
    buf[2] = hex[(value >> 4) & 0xf];
    buf[3] = hex[value & 0xf];
    buf[4] = 0;
}
const struct core_param dm_lane_param = {
    DM_LANE_ID, 16, CORE_SLOT_LANE, 0, LV_MAX << 8, 0, 0, -1, -1, -1, 0,
    "Drum Mode", "Drum", "DRM", dm_lane_format, fw_str_empty, 0, CORE_LANE_NOLOCK
};

/* 0.11: ratchets. A second lane parameter (id 190) keeps, per step of a Drum Mode track, how many times each drum
 * plays within the step: 2 bits per lane (lane l at bits 2l..2l+1), the count - 1, so 0 is a plain hit and 3 four
 * hits. It is only ever a step's lock (core keeps it with the pattern as a lock row and drops it with its trig); the
 * track's own value stays 0. Set with knob E of the TRIG page on held steps in GRID RECORDING (roll_turn), for the
 * selected lane. */
#define DM_ROLL_ID      190
#define ROLL_MAX        4
static void dm_roll_format(int32_t value, char *buf)
{
    dm_lane_format(value, buf);
}
const struct core_param dm_roll_param = {
    DM_ROLL_ID, 16, CORE_SLOT_LANE, 0, 0xffff << 8, 0, 0, -1, -1, -1, 0,
    "Drum Roll", "Drum", "RLL", dm_roll_format, fw_str_empty, 0, 0
};
/* How many times lane plays on step of track (1-4). Interrupt level or UI task. */
static int32_t roll_get(int32_t track, int32_t step, int32_t lane)
{
    int32_t v = core_lane_lock(DM_ROLL_ID, track, step);
    return v < 0 ? 1 : ((v >> (2 * lane)) & 3) + 1;
}
/* UI task: lane plays n times on step (1 takes its bits out; a lock left at 0 is cleared by writing 0). */
static void roll_put(int32_t track, int32_t step, int32_t lane, int32_t n)
{
    int32_t v = core_lane_lock(DM_ROLL_ID, track, step);
    if (v < 0)
        v = 0;
    v = (v & ~(3 << (2 * lane))) | (((n < 1 ? 1 : n > ROLL_MAX ? ROLL_MAX : n) - 1) << (2 * lane));
    if (v || core_lane_lock(DM_ROLL_ID, track, step) >= 0)
        core_lane_set(DM_ROLL_ID, track, step, v);
}
static int8_t dm_kit_of[DM_TRACKS];     /* the kit each track of the playing pattern uses, -1 none yet */
static volatile uint8_t dm_unset;       /* bit t: the playing pattern has no settings for track t yet */
volatile int32_t dm_cur_pat = -1;       /* the pattern dm belongs to, -1 none yet (after a load) */

/* What each trig key is sounding, so its release ends the same note on the same track, whatever changed since. */
struct dm_held {
    int8_t track;                       /* firmware track 4-7, or -1: nothing held */
    int8_t channel;                     /* the channel its note went out on, or -1: none (CHAN off) */
    uint8_t note;
    uint8_t lane;
};
static struct dm_held dm_held[KEY_STEPS];
static uint32_t dm_taken;               /* bit k: trig key k's press was ours; its repeats and release are too */
/* 0.9.2: trig keys left to the stock. dm_stock: pressed while the stock's pattern/bank select or a chain had the
 * trig keys, their repeats and release go to the stock too. dm_grid_keys: steps held in GRID RECORDING, which the
 * stock sees held (its parameter locks, micro timing, trig copy/paste); dm_grid_long: held long or used while held,
 * so their release keeps the lane; dm_grid_remove: a quick tap takes the selected lane out on release. */
volatile uint32_t dm_stock, dm_grid_keys, dm_grid_long, dm_grid_remove;
volatile int32_t dm_ptn_held, dm_bank_held;
/* 0.10: the pattern FUNC + RECORD copied (-1 none), and a FUNC + STOP waiting to be checked: after the stock's paste
 * the target's steps must equal the copied pattern's before its Drum Mode settings follow. */
volatile int32_t dm_clip = -1, dm_paste_dst = -1;
static int32_t dm_paste_ticks;
#define PASTE_TICKS     15
/* A key left to the stock that sends nothing for this long counts as released: the stock's bank select swallows the
 * release of the bank's trig key (measured in digiemu), while a held key keeps sending repeats (every ~0.1 s after the
 * first ~0.4 s). In UI ticks. */
#define STOCK_KEY_TICKS 45
static uint8_t dm_stock_age[KEY_STEPS];
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
    return (dm_store.selected[t >> 1] >> (4 * (t & 1))) & 7;
}

static void select_lane(int32_t t, int32_t lane)
{
    uint8_t *b = &dm_store.selected[t >> 1];
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

/* 0.9.4: a setting the sequencer applies when it prepares a step changed (Drum Mode on/off, a lane's mute, note or
 * velocity). While stopped the sequencer prepared step 1 when the last run stopped, so step 1 of the next run would
 * still play the old way: seq-events prepares it again at the end of this UI tick. The UI task. */

static int32_t kit_same(const struct dm_kit *k, int32_t t)
{
    for (int32_t l = 0; l < DM_LANES; l++)
        if (k->note[l] != dm.note[t][l] || k->velocity[l] != dm.velocity[t][l])
            return 0;
    return 1;
}

static void kit_put(struct dm_kit *k, int32_t t)
{
    for (int32_t l = 0; l < DM_LANES; l++) {
        k->note[l] = dm.note[t][l];
        k->velocity[l] = dm.velocity[t][l];
    }
}

static void kit_get(const struct dm_kit *k, int32_t t)
{
    for (int32_t l = 0; l < DM_LANES; l++) {
        dm.note[t][l] = k->note[l] > 127 ? dm_default_notes[l] : k->note[l];
        dm.velocity[t][l] = k->velocity[l] < 1 || k->velocity[l] > 127 ? 100 : k->velocity[l];
    }
}

/* The block's size follows the last kit in use (core saves only what it holds). */
static void kits_trim(void)
{
    int32_t n = MAX_KITS;
    while (n > 0 && !dm_store.refs[n - 1])
        n--;
    dm_store.nkits = (uint8_t)n;
    dm_store.bytes = KITS_HEAD + sizeof(struct dm_kit) * (uint32_t)n;
}

/* A kit holding track t's notes and velocities (dm): an identical one, else a free slot. Its use count goes up.
 * -> its number, or -1 when all 8 are in use. */
static int32_t kit_take(int32_t t)
{
    int32_t free_k = -1;
    for (int32_t k = 0; k < MAX_KITS; k++) {
        if (dm_store.refs[k] && kit_same(&dm_store.kit[k], t)) {
            dm_store.refs[k]++;
            return k;
        }
        if (!dm_store.refs[k] && free_k < 0)
            free_k = k;
    }
    if (free_k < 0)
        return -1;
    kit_put(&dm_store.kit[free_k], t);
    dm_store.refs[free_k] = 1;
    kits_trim();
    return free_k;
}

static void kit_drop(int32_t k)
{
    if (k >= 0 && k < MAX_KITS && dm_store.refs[k]) {
        dm_store.refs[k]--;
        kits_trim();
    }
}

static int32_t pin_find(int32_t key)
{
    for (int32_t i = 0; i < dm_store.npins; i++)
        if ((dm_store.pins[i] & 0x1ff) == key)
            return i;
    return -1;
}

static void pin_drop(int32_t i)
{
    kit_drop(dm_store.pins[i] >> 12);
    for (int32_t j = i + 1; j < dm_store.npins; j++)
        dm_store.pins[j - 1] = dm_store.pins[j];
    dm_store.npins--;
}

/* A saved pattern track's kit: kit -1 (no settings) takes its pin away. The UI task. */
static void pin_set(int32_t pattern, int32_t t, int32_t kit)
{
    int32_t key = (pattern << 2) | t, i = pin_find(key);
    if (i >= 0) {
        if (kit >= 0 && (dm_store.pins[i] >> 12) == kit)
            return;
        pin_drop(i);
    }
    if (kit < 0 || kit >= MAX_KITS)
        return;
    if (dm_store.npins >= MAX_PINS) {
        pin_drop(0);                    /* the oldest */
        say("PINS FULL", 0);
    }
    dm_store.refs[kit]++;
    dm_store.pins[dm_store.npins++] = (uint16_t)(key | (kit << 12));
    kits_trim();
}

static int32_t lane_value(int32_t t)
{
    return LV_SET | (((dm.enabled >> t) & 1) ? LV_ON : 0) | ((dm_kit_of[t] & 0xf) << LV_KIT_SHIFT) | dm.mutes[t];
}

static int32_t pattern_index(void)
{
    uint32_t idx = (uint32_t)(fw_pattern - fw_patterns) / PATTERN_SIZE;
    return idx < DM_PATTERNS ? (int32_t)idx : -1;
}

/* Keep dm on the pattern that plays: when it changed, each track's settings come from the new pattern's lane value.
 * A track without any starts like a new project's: Drum Mode off, the default kit, no mutes (0.10.2: nothing copied
 * from the pattern it came from), and stays without settings until one is changed there (rearm -> settle_track).
 * From the UI tick and the sequencer's stages (interrupt level: reads only). -> 1 when it changed. */
static int32_t pattern_follow(void)
{
    int32_t idx = pattern_index();
    uint8_t unset = 0;
    if (idx < 0 || idx == dm_cur_pat)
        return 0;
    for (int32_t t = 0; t < DM_TRACKS; t++) {
        int32_t v = core_lane_track(DM_LANE_ID, DM_FIRST + t), k = (v >> LV_KIT_SHIFT) & 0xf;
        if (v < 0 || !(v & LV_SET) || k >= MAX_KITS || !dm_store.refs[k]) {
            unset |= (uint8_t)(1u << t);
            dm.enabled &= (uint8_t)~(1u << t);
            dm.mutes[t] = 0;
            dm_kit_of[t] = -1;
            for (int32_t l = 0; l < DM_LANES; l++) {
                dm.note[t][l] = dm_default_notes[l];
                dm.velocity[t][l] = 100;
            }
            continue;
        }
        dm.enabled = (uint8_t)((dm.enabled & ~(1u << t)) | ((v & LV_ON) ? 1u << t : 0));
        dm.mutes[t] = (uint8_t)(v & 0xff);
        dm_kit_of[t] = (int8_t)k;
        kit_get(&dm_store.kit[k], t);
    }
    dm_unset = unset;
    dm_cur_pat = idx;
    return 1;
}

/* 0.10.2: a setting changed on a track of the playing pattern that has none yet: it gets its own now (a kit for its
 * notes, the lane value). -> 1 when kept; 0 with all 8 kits in use (plays dm's settings, keeps none). The UI task. */
static int32_t settle_track(int32_t t)
{
    int32_t k = kit_take(t);
    if (k < 0) {
        say("KIT MEM FULL", 0);
        return 0;
    }
    dm_kit_of[t] = (int8_t)k;
    core_lane_set(DM_LANE_ID, DM_FIRST + t, -1, lane_value(t));
    dm_unset &= (uint8_t)~(1u << t);
    return 1;
}

/* A setting of track changed in dm (Drum Mode on/off, a mute, a lane's note or velocity): the playing pattern keeps
 * it. Its kit is edited in place if only this pattern track uses it, else copied; an identical kit is shared. With
 * all 8 kits in use a new note or velocity can't be kept: dm goes back to the kit and the corner says so. Then
 * step 1 is prepared again (0.9.4). The UI task. */
static void rearm(int32_t track)
{
    int32_t t = track - DM_FIRST, k;
    if (t < 0 || t >= DM_TRACKS)
        return;
    if (dm_cur_pat >= 0 && dm_cur_pat == pattern_index() && ((dm_unset >> t) & 1)) {
        settle_track(t);                /* its first setting (0.10.2) */
    } else if (dm_cur_pat >= 0 && dm_cur_pat == pattern_index()) {
        k = dm_kit_of[t];
        if (k < 0 || k >= MAX_KITS || !kit_same(&dm_store.kit[k], t)) {
            int32_t same = -1;
            for (int32_t m = 0; m < MAX_KITS; m++)
                if (m != k && dm_store.refs[m] && kit_same(&dm_store.kit[m], t)) {
                    same = m;
                    break;
                }
            if (same >= 0) {            /* an identical kit: share it */
                kit_drop(k);
                dm_store.refs[same]++;
                dm_kit_of[t] = (int8_t)same;
            } else if (k >= 0 && k < MAX_KITS && dm_store.refs[k] == 1) {
                kit_put(&dm_store.kit[k], t);   /* only this pattern track uses it: in place */
            } else {
                int32_t n = kit_take(t);
                if (n < 0) {
                    if (k >= 0 && k < MAX_KITS)
                        kit_get(&dm_store.kit[k], t);
                    say("KIT MEM FULL", 0);
                } else {
                    kit_drop(k);
                    dm_kit_of[t] = (int8_t)n;
                }
            }
        }
        core_lane_set(DM_LANE_ID, track, -1, lane_value(t));
    }
    sq_rearm(1u << track);
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

/* 0.11: notes between steps (ratchets, and the pads' note repeat). A note to send at a timeline time goes in dm_rq:
 * slots 0-31 are filled by the sequencer's fire (interrupt level), 32-63 by the UI task, so each range has one writer;
 * a slot is written, then marked waiting (state 1), and only the sender frees it. The render (ev_render_params, about
 * 1500 times a second) wakes Drum Mode's own task when a note is due, and the task sends it through the port switch,
 * as the pads do: the timeline moves on in steps of about 1/260 of a 16th (measured in digiemu), so a note goes out
 * within a render block of its time. The task and its semaphore are DigiArp's (its fast.s: the OS 1.44 routines). */
struct dm_rev {
    volatile uint8_t state;             /* 0 free, 1 waiting */
    int8_t channel;
    uint8_t note, vel;                  /* vel 0: the note off */
    uint32_t time;
};
#define RQ_SEQ          32
#define RQ_ALL          64
static struct dm_rev dm_rq[RQ_ALL];
extern int dm_os_task_create(void *tcb, void (*fn)(void), int prio, void *stack, int size);
extern void dm_os_task_start(void *tcb);
extern void dm_os_sem_init(void *sem, int count);
extern void dm_os_sem_pend(void *sem);
extern void dm_os_sem_post(void *sem);
static uint32_t dm_tcb[32], dm_stack[1024], dm_sem[4];
static volatile int32_t dm_task_on, dm_task_woken;
volatile int32_t dm_rq_sent;            /* notes the task sent, for the tests */
volatile uint32_t dm_rq_last_time;      /* the timeline when it sent the last, for the tests */

static void rq_put(int32_t from, int32_t to, uint32_t time, int32_t channel, int32_t note, int32_t vel)
{
    for (int32_t i = from; i < to; i++)
        if (!dm_rq[i].state) {
            dm_rq[i].channel = (int8_t)channel;
            dm_rq[i].note = (uint8_t)note;
            dm_rq[i].vel = (uint8_t)vel;
            dm_rq[i].time = time;
            dm_rq[i].state = 1;
            return;
        }
}

/* A hit and its note off, gate after it. */
static void rq_hit(int32_t from, int32_t to, uint32_t time, uint32_t gate, int32_t channel, int32_t note, int32_t vel)
{
    rq_put(from, to, time, channel, note, vel);
    rq_put(from, to, time + gate, channel, note, 0);
}

static void dm_task(void)
{
    for (;;) {
        dm_os_sem_pend(dm_sem);
        dm_task_woken = 0;
        for (int32_t i = 0; i < RQ_ALL; i++) {
            struct dm_rev *e = &dm_rq[i];
            if (!e->state)
                continue;
            if (fw_transport != 1 && e->vel) {
                e->state = 0;           /* stopped: a hit still to come is dropped, its note off still goes */
                continue;
            }
            if (fw_transport == 1 && (int32_t)(fw_timeline - e->time) < 0)
                continue;
            if (e->vel)
                FW_MIDI_NOTE_ON(e->channel, e->note, e->vel);
            else
                FW_MIDI_NOTE_OFF(e->channel, e->note);
            dm_rq_sent++;
            dm_rq_last_time = fw_timeline;
            e->state = 0;
        }
    }
}

static void task_start(void)
{
    if (dm_task_on)
        return;
    dm_os_sem_init(dm_sem, 0);
    dm_task_on = 1;
    dm_os_task_create(dm_tcb, dm_task, 1, dm_stack, sizeof dm_stack);
    dm_os_task_start(dm_tcb);
}

/* ev_render_params, interrupt level: wake the task when a note is due (or the transport stopped with notes left). */
void dm_render(int16_t *words)
{
    (void)words;
    if (!dm_task_on || dm_task_woken)
        return;
    for (int32_t i = 0; i < RQ_ALL; i++)
        if (dm_rq[i].state && (fw_transport != 1 || (int32_t)(fw_timeline - dm_rq[i].time) >= 0)) {
            dm_task_woken = 1;
            dm_os_sem_post(dm_sem);
            return;
        }
}

/* The rolls of the step fired last, waiting for it to play: 2 bits per lane (count - 1), per track. */
static volatile uint16_t dm_roll_next[DM_TRACKS];
static uint8_t dm_roll_note[DM_TRACKS][DM_LANES], dm_roll_velo[DM_TRACKS][DM_LANES];

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

/* 0.9.1: a running light while playing (not in GRID RECORDING). 0.10.5: one key per step over all 16 keys (step 1
 * key 1 ... step 16 key 16; a page of a longer track starts over at key 1); a short length leaves the keys after its
 * last step dark. 0.10.6: red for the whole step, as the stock's playhead (measured in digiemu: a MIDI track playing
 * shows its step red, colour 3, the whole step, moving straight on to the next key). On keys 1-8 it shows over the
 * pad's own light for that step, then the pad's state (held, played, selected, muted) shows again. (0.9.1-0.10.4:
 * keys 9-16, 2 steps per key; 0.9.1-0.10.5: white for the first half of each step.) The step playing is the one before the clock's (the fired one), it started
 * playing when the clock moved to it; a step's length is the time between two clock steps (the track's speed and
 * swing), else a 16th. -> the key 8-15 (0-based) that is lit now, or -1. */
volatile int32_t dm_run_key = -1;      /* the lit key 1-16 (0-based 0-15) or -1, for the tests */
volatile int32_t dm_run_len;            /* the track length it went by, for the tests */

/* The step clock seen from the UI tick: seq-events' sq_clock (every step, trig or not; the fired step, one before the
 * one playing), with the timeline when its step changed and the step length between two changes. */
static struct { int32_t step; uint32_t time, len; } dm_clock[DM_TRACKS];

static void clock_tick(int32_t track)
{
    int32_t t = track - DM_FIRST;
    uint32_t last = sq_clock[track].last, now = fw_timeline;
    if (fw_transport != 1 || last == SQ_CLOCK_NONE) {
        dm_clock[t].step = -1;
        return;
    }
    if ((int32_t)last != dm_clock[t].step) {
        uint32_t dd = now - dm_clock[t].time;
        if (dm_clock[t].step >= 0 && dd > TIMELINE_16TH / 8 && dd < TIMELINE_16TH * 16)
            dm_clock[t].len = dd;       /* the track's speed scale and swing, as it plays */
        if (!dm_clock[t].len)
            dm_clock[t].len = TIMELINE_16TH;
        dm_clock[t].step = (int32_t)last;
        dm_clock[t].time = now;
    }
}

static int32_t run_key(int32_t track)
{
    int32_t t = track - DM_FIRST, s = dm_clock[t].step, len, pos;
    struct sq_track info;
    if (fw_transport != 1 || s < 0)
        return -1;
    len = sq_track_info(track, &info, sizeof info) < 0 || info.len < 1 ? 16 : info.len > 64 ? 64 : info.len;
    dm_run_len = len;
    pos = (s - LEAD_STEPS) % len;
    if (pos < 0)
        pos += len;
    return pos % 16;                    /* 0.10.5: one key per step, 1-16 */
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
/* 0.9.3: any recording mode: GRID, STEP or LIVE RECORDING (armed or running). */
static int32_t any_recording(void)
{
    uint8_t *rec = FW_REC_OBJ;
    return rec && rec[REC_GRID_ON] && (rec[REC_GRID] || rec[REC_LIVE] || rec[REC_STEP]);
}

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

/* A trig key pressed in GRID RECORDING (0.9.2): the selected lane goes on that step of the shown page at once if it
 * isn't there; if it is, a quick tap takes it out on release (grid_remove), while a step held long or used while
 * held (the stock's parameter locks, micro timing, copy and paste) keeps it, as the stock treats its own trigs.
 * -> 1 when the step has a trig the stock can see held, 0 for a key past the track's length. */
static int32_t grid_press(int32_t track, int32_t key)
{
    int32_t t = track - DM_FIRST, lane = selected_lane(t), note = dm.note[t][lane];
    int32_t step = 16 * grid_page(track) + key;
    uint8_t *data = track_data(track_obj(track));
    uint32_t bit = 1u << key;
    if (!data || step >= track_len(track))
        return 0;
    dm_grid_long &= ~bit;
    if (step_has(data, step, note))
        dm_grid_remove |= bit;
    else {
        record_note(track, step, note);
        dm_grid_remove &= ~bit;
        dm_grid_edits++;
        dm_flash[t][lane] = FLASH_TICKS;
    }
    dm_dirty = 1;
    return 1;
}

static void grid_remove(int32_t track, int32_t key)
{
    int32_t t = track - DM_FIRST, lane = selected_lane(t), note = dm.note[t][lane];
    int32_t step = 16 * grid_page(track) + key;
    uint8_t *data = track_data(track_obj(track));
    if (!data || step >= track_len(track) || !step_has(data, step, note))
        return;
    remove_note(track, step, note);
    roll_put(track, step, lane, 1);     /* 0.11: its roll goes with it */
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
    pattern_follow();                   /* 0.10: a pattern change while playing reaches its first steps */
    f = &dm_fire[track - DM_FIRST];
    {                                   /* 0.11: the step fired before plays now: its rolls' extra hits */
        int32_t t = track - DM_FIRST, ch = midi_channel(track);
        uint16_t rolls = dm_roll_next[t];
        dm_roll_next[t] = 0;
        if (step == f->step)
            dm_roll_next[t] = rolls;    /* the same step fired again (the sequencer fires the first step when it
                                         * stops and again at PLAY, measured in digiemu): not played yet */
        else if (rolls && ch >= 0 && fw_transport == 1) {
            /* The step's length: the time between the two fires before, which with swing is a step of the same
             * parity (16ths alternate long and short), when they were two steps in a row; else a 16th. */
            uint32_t len = TIMELINE_16TH, now = fw_timeline;
            int32_t tl = track_len(track);
            if (f->step >= 0 && f->prev_step >= 0 && f->step == (f->prev_step + 1) % tl) {
                uint32_t dd = f->time - f->prev_time;
                if (dd > TIMELINE_16TH / 8 && dd < TIMELINE_16TH * 16)
                    len = dd;
            }
            for (int32_t l = 0; l < DM_LANES; l++) {
                int32_t n = ((rolls >> (2 * l)) & 3) + 1;
                uint32_t part = len / (uint32_t)n;
                for (int32_t i = 1; i < n; i++)
                    rq_hit(0, RQ_SEQ, now + part * (uint32_t)i, part / 2, ch, dm_roll_note[t][l], dm_roll_velo[t][l]);
            }
        }
    }
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
    pattern_follow();                   /* 0.10: the playing pattern's settings */
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
    {                                   /* 0.11: the playing lanes' rolls, for when this step plays */
        uint16_t rolls = 0;
        if (lanes) {
            int32_t v = core_lane_lock(DM_ROLL_ID, track, step);
            if (v > 0)
                for (int32_t l = 0; l < DM_LANES; l++)
                    if (((lanes >> l) & 1) && ((v >> (2 * l)) & 3)) {
                        rolls |= (uint16_t)(v & (3 << (2 * l)));
                        dm_roll_note[t][l] = dm.note[t][l];
                        dm_roll_velo[t][l] = dm.velocity[t][l];
                    }
        }
        dm_roll_next[t] = rolls;
    }
}

void dm_fire_vel(int32_t track, int32_t step, int32_t *vel)
{
    (void)step;
    if (drum_track(track) && dm_step_vel[track - DM_FIRST])
        *vel = dm_step_vel[track - DM_FIRST];
}

/* 0.11: note repeat. Knob G of the TRIG page on a Drum Mode track sets the track's rate (dm_rpt: 0 off, 1 = 1/16,
 * 2 = 1/32, 3 = 1/64; kept until power-off). While the sequencer plays, a held pad plays again on each 16th, 32nd or
 * 64th of the steps' grid (the first after half a beat of the rate has passed since the press), sent by the note
 * task. In LIVE RECORDING each step the hold passes is recorded: the pad's note, and at 1/32 or 1/64 its roll (2 or
 * 4), so the playback plays what was heard. */
static uint8_t dm_rpt[DM_TRACKS];
static uint32_t dm_rpt_next[KEY_STEPS];          /* the next repeat's timeline time, per held pad */
static int8_t dm_rpt_step[KEY_STEPS];            /* the step recorded last for it, -1 none */
static uint32_t dm_rpt_keys;                     /* bit k: pad k repeats */
static const char *const dm_rpt_names[4] = { "-", "16", "32", "64" };
volatile int32_t dm_rpt_hits;           /* repeats queued, for the tests */

/* The track's step length now (its speed and swing as measured), else a 16th; and the grid's origin, the last fire. */
static uint32_t step_len(int32_t track)
{
    struct dm_fire *f = &dm_fire[track - DM_FIRST];
    uint32_t dd = f->time - f->prev_time;
    return f->step >= 0 && f->prev_step >= 0 && dd > TIMELINE_16TH / 8 && dd < TIMELINE_16TH * 16 ? dd : TIMELINE_16TH;
}

static void rpt_start(int32_t key, int32_t track)
{
    int32_t t = track - DM_FIRST;
    uint32_t rate, now = fw_timeline, origin = dm_fire[t].time, k;
    if (!dm_rpt[t] || fw_transport != 1 || dm_fire[t].step < 0)
        return;
    rate = step_len(track) >> (dm_rpt[t] - 1);
    k = (now - origin + rate / 2) / rate + 1;        /* the first grid point at least half a rate away */
    dm_rpt_next[key] = origin + k * rate;
    dm_rpt_step[key] = -1;
    dm_rpt_keys |= 1u << key;
}

/* UI tick: queue each repeating pad's hits due within the next ~40 ms, and record in LIVE RECORDING. */
static void rpt_tick(void)
{
    for (int32_t k = 0; k < KEY_STEPS; k++) {
        struct dm_held *h = &dm_held[k];
        int32_t t, step;
        uint32_t rate;
        if (!((dm_rpt_keys >> k) & 1))
            continue;
        if (h->track < 0 || fw_transport != 1 || !dm_rpt[h->track - DM_FIRST]) {
            dm_rpt_keys &= ~(1u << k);
            continue;
        }
        t = h->track - DM_FIRST;
        rate = step_len(h->track) >> (dm_rpt[t] - 1);
        while ((int32_t)(dm_rpt_next[k] - fw_timeline) < (int32_t)(TIMELINE_16TH / 3)) {
            if (h->channel >= 0)
                rq_hit(RQ_SEQ, RQ_ALL, dm_rpt_next[k], rate / 2, h->channel, h->note, dm.velocity[t][h->lane]);
            dm_rpt_next[k] += rate;
            dm_rpt_hits++;
        }
        if (live_recording() && (step = hit_step(h->track)) >= 0 && step != dm_rpt_step[k]) {
            dm_rpt_step[k] = (int8_t)step;
            record_note(h->track, step, h->note);
            if (dm_rpt[t] > 1)
                roll_put(h->track, step, h->lane, dm_rpt[t] == 2 ? 2 : 4);
        }
    }
}

/* Knob G of the TRIG page: the repeat rate. */
static int32_t dm_rpt_acc;
static void rpt_turn(int32_t track, int32_t counts)
{
    int32_t t = track - DM_FIRST, r = dm_rpt[t];
    dm_rpt_acc += counts;
    while (dm_rpt_acc >= COUNTS) {
        dm_rpt_acc -= COUNTS;
        r++;
    }
    while (dm_rpt_acc <= -COUNTS) {
        dm_rpt_acc += COUNTS;
        r--;
    }
    r = r < 0 ? 0 : r > 3 ? 3 : r;
    if (r != dm_rpt[t]) {
        dm_rpt[t] = (uint8_t)r;
        say(r ? "RPT 1/%d" : "RPT OFF", 8 << r);
    }
    dm_dirty = 1;
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
    rpt_start(key, track);              /* 0.11: note repeat */
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
    if ((dm_rpt_keys >> key) & 1)       /* repeats already queued for it are dropped; their note offs still go */
        for (int32_t i = RQ_SEQ; i < RQ_ALL; i++)
            if (dm_rq[i].state && dm_rq[i].vel && dm_rq[i].note == h->note && dm_rq[i].channel == h->channel)
                dm_rq[i].state = 0;
    dm_rpt_keys &= ~(1u << key);
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
static void store_reset(void)
{
    fw_memset(&dm_store, 0, sizeof dm_store);
    dm_store.version = KITS_VERSION;
    kits_trim();
}

void dm_boot(void)
{
    for (int32_t k = 0; k < KEY_STEPS; k++)
        dm_held[k].track = -1;
    for (int32_t t = 0; t < DM_TRACKS; t++) {
        dm_fire[t].step = dm_fire[t].prev_step = -1;
        dm_kit_of[t] = -1;
    }
    if (dm.version != SAVE_VERSION)
        defaults();
    if (dm_store.version != KITS_VERSION)
        store_reset();
}

/* After every project load ("DRMK"): its kits and selected lanes; none (a new project, or one saved without 0.10):
 * no kits, Drum Mode off with the default kit, and the patterns' settings start from there. Each pattern's on/off,
 * mutes and kit number come back with the pattern itself (core's lanes); the first pattern_follow reads them.
 * The engine task, before the UI sees the project. */
static void dm_loaded(int32_t found)
{
    dm_cur_pat = -1;
    dm_unset = 0;
    defaults();
    for (int32_t t = 0; t < DM_TRACKS; t++)
        dm_kit_of[t] = -1;
    if (found && dm_store.version == 1 && dm_store.nkits <= 10) {
        /* 0.10's block: refs at +8, kits at +20. Its kits move to +40; an 8th's and later uses end (those pattern
         * tracks start without settings). No pins. */
        uint8_t *b = (uint8_t *)&dm_store;
        int32_t n = dm_store.nkits > MAX_KITS ? MAX_KITS : dm_store.nkits;
        for (int32_t i = (int32_t)sizeof(struct dm_kit) * n - 1; i >= 0; i--)
            b[KITS_HEAD + i] = b[KITS_HEAD_V1 + i];
        for (int32_t k = n; k < 12; k++)
            dm_store.refs[k] = 0;
        dm_store.npins = 0;
        dm_store.version = KITS_VERSION;
        kits_trim();
    }
    if (!found || dm_store.version != KITS_VERSION || dm_store.nkits > MAX_KITS || dm_store.npins > MAX_PINS)
        store_reset();
    else {
        for (int32_t k = dm_store.nkits; k < 12; k++)
            dm_store.refs[k] = 0;
        int32_t n = 0;
        for (int32_t i = 0; i < dm_store.npins; i++)      /* a pin to a kit that isn't there is dropped */
            if ((dm_store.pins[i] >> 12) < dm_store.nkits)
                dm_store.pins[n++] = dm_store.pins[i];
        dm_store.npins = (uint8_t)n;
    }
}
const struct core_projdata dm_projdata = { 0x44524d4bu /* "DRMK" */, CORE_PROJDATA_VARIABLE | sizeof dm_store,
                                           &dm_store, dm_loaded };

/* ---- the DRUM pages --------------------------------------------------------------------------- */
/* 0.10.4: one page, 27, holds both DRUM pages (Drum Notes, then Drum Velocity), so Drum Mode claims one of the four mod
 * pages (27-30: core takes 30, machines-dn1 28 and 29). LFO pressed on Drum Notes shows Drum Velocity on the same
 * page (taken, dm_page_vel_on); pressed there, it goes on to the stock's next page. Page 27 follows the MIDI tracks'
 * LFO page (23), so LFO pressed again on a MIDI track steps to it. Not after
 * MIDI Source (20): measured in digiemu, its FUNC + push A (CHAN on) stops working with any mod page after it; not
 * after TRIG (19): TRIG + pad selects a lane, and each TRIG press would step the page. Their knob boxes are empty
 * (id 0): Drum Mode draws the 8 lanes over them and takes the knobs while one is shown. */
#define PAGE_MIDI_LFO   23
#define KEY_LFO         25              /* the LFO page key */
struct core_page dm_page_notes = { 27, PAGE_MIDI_LFO, "DRUM", "Drum Notes", { 0 }, 0, 0, 0 };
static int32_t dm_page_vel_on;          /* page 27 shows Drum Velocity */
static int32_t dm_lfo_taken;            /* the LFO press that switched it: its repeats and release are ours too */

/* 1 the notes page, 2 the velocity page, 0 neither, on screen with nothing over it. core_page_shown compares the
 * view with the one core noted in the descriptor, the last built, so this asks the shown view itself (core-dn1's pages.s and lists.c): the Brain's view stack (+0x54, and
 * +0x5c the same while nothing is over the pages), its page manager (+8), the key view shown (+0xc0), and that
 * view's list (a std::vector at +124, +128) at its current place (+144). */
/* The page index shown (the key view's list at its place), or -1. */
static int32_t shown_page_idx(void)
{
    uint8_t *b = dm_brain, *top, *mgr, *v;
    int32_t *beg, *end, cur;
    if (!b)
        return -1;
    top = *(uint8_t **)(b + 0x54);
    if (!top || top != *(uint8_t **)(b + 0x5c))
        return -1;
    mgr = *(uint8_t **)(top + 8);
    if (!mgr || !(v = *(uint8_t **)(mgr + 0xc0)))
        return -1;
    beg = *(int32_t **)(v + 124);
    end = *(int32_t **)(v + 128);
    cur = *(int32_t *)(v + 144);
    if (!beg || cur < 0 || beg + cur >= end)
        return -1;
    return beg[cur];
}

static int32_t drum_page(void)
{
    int32_t idx = shown_page_idx();
    if (idx < 0)
        return 0;
    if (idx != dm_page_notes.idx) {
        dm_page_vel_on = 0;             /* left the page: Drum Notes first next time */
        dm_page_notes.title = "Drum Notes";
        return 0;
    }
    return dm_page_vel_on ? 2 : 1;
}

/* The DRUM page's title follows what it shows (the stock heads the screen with the descriptor's title). */
static void page_title(void)
{
    dm_page_notes.title = dm_page_vel_on ? "Drum Velocity" : "Drum Notes";
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
        rearm(track);
        ch = midi_channel(track);
        if (ch >= 0) {
            FW_MIDI_NOTE_ON(ch, n, dm.velocity[t][lane]);
            FW_MIDI_NOTE_OFF(ch, n);
        }
        dm_flash[t][lane] = FLASH_TICKS;
    } else {
        int32_t v = dm.velocity[t][lane] + steps;
        dm.velocity[t][lane] = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
        rearm(track);
    }
    dm_dirty = 1;
}

/* ---- keys ------------------------------------------------------------------------------------ */
/* The UI task, each key event. On a Drum Mode track a trig key's press is taken (FUNC + pad 1-8 toggles its lane's
 * mute; FUNC + 9-16 is left to the stock), and then that key's repeats and release, whatever changed meanwhile: a release always ends what its
 * press played, and a key the stock saw go down is never kept from it on the way up. */
static int32_t stock_keys(void);
static void clip_copy(void);

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
    if (key == KEY_LFO) {               /* 0.10.4: Drum Notes -> Drum Velocity on the one DRUM page */
        if (dm_lfo_taken) {
            if (up)
                dm_lfo_taken = 0;
            return 1;
        }
        if (!up && !(flags & CORE_KEY_REPEAT) && !(flags & CORE_KEY_FUNC) && drum_page() == 1) {
            dm_page_vel_on = 1;
            page_title();
            dm_lfo_taken = 1;
            dm_dirty = 1;
            return 1;
        }
        return 0;
    }
    if (key == KEY_PTN) {
        dm_ptn_held = !up;              /* watched, never taken */
        dm_dirty = 1;                   /* the trig key LEDs go to the stock and back (0.10.1) */
    }
    if (!up && !(flags & CORE_KEY_REPEAT) && (flags & CORE_KEY_FUNC) && !grid_recording()) {
        if (key == KEY_RECORD) {        /* the stock copies the active pattern (watched, never taken) */
            clip_copy();
        } else if (key == KEY_STOP && dm_clip >= 0) {
            dm_paste_dst = -1;          /* the stock pastes into the active pattern: checked a moment later */
            dm_paste_ticks = PASTE_TICKS;
        }
    }
    if (key == KEY_BANK) {
        dm_bank_held = !up;
        dm_dirty = 1;
    }
    if (key < KEY_STEP1 || key >= KEY_STEP1 + KEY_STEPS) {
        if (dm_grid_keys && !up)
            dm_grid_long |= dm_grid_keys;   /* a key with steps held (arrows, RECORD, STOP, PLAY): the stock's */
        return 0;
    }
    k = key - KEY_STEP1;
    bit = 1u << k;

    if ((dm_stock & bit) && !up && !(flags & CORE_KEY_REPEAT))
        dm_stock &= ~bit;               /* pressed again: its release was lost; decided afresh below */
    if (dm_stock & bit) {               /* the stock's press: its repeats and release too */
        dm_stock_age[k] = 0;
        if (up)
            dm_stock &= ~bit;
        return 0;
    }
    if (dm_grid_keys & bit) {           /* a step held in GRID RECORDING (0.9.2) */
        if (!up) {
            if (flags & CORE_KEY_REPEAT)
                dm_grid_long |= bit;
            return 0;
        }
        if (!(dm_grid_long & bit)) {    /* a quick tap: the stock must not take the step's trig away on release */
            uint8_t copy[32];
            for (int32_t i = 0; i < 32; i++)
                copy[i] = ((const uint8_t *)ev)[i];
            CORE_KEY_FLAGS(copy) = (flags & ~CORE_KEY_UP) | CORE_KEY_DOWN | CORE_KEY_REPEAT;
            fw_brain_key(brain, copy);
            if ((dm_grid_remove & bit) && drum_track(track))
                grid_remove(track, k);
        }
        dm_grid_keys &= ~bit;
        dm_grid_long &= ~bit;
        dm_grid_remove &= ~bit;
        return 0;
    }
    if (!up && !(dm_taken & bit) && stock_keys()) {
        dm_stock |= bit;                /* pattern/bank select, chains, the SCALE menu: the stock's (0.9.2, 0.10.1) */
        dm_stock_age[k] = 0;
        return 0;
    }

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
        if (k >= DM_LANES || any_recording())
            return 0;                   /* FUNC + 9-16, and FUNC + any trig key while recording: the stock's (0.9.3) */
        dm.mutes[t] ^= (uint8_t)(1u << k);
        rearm(track);
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
    if (grid_recording()) {             /* GRID RECORDING: the 16 keys are the selected lane's steps */
        if (grid_press(track, k)) {
            dm_grid_keys |= bit;        /* the stock sees the step held (0.9.2) */
            return 0;
        }
        dm_taken |= bit;
        return 1;
    }
    dm_taken |= bit;
    if (k < DM_LANES) {
        if (!live_recording() && selected_lane(track - DM_FIRST) != k) {
            select_lane(track - DM_FIRST, k);   /* 0.9: a pad selects its lane as it plays (not in LIVE RECORDING) */
            dm_dirty = 1;
        }
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
/* 0.11: knob E of the TRIG page, steps held in GRID RECORDING: each held step that plays the selected lane gets its
 * roll (1-4) turned by the knob's detents; the corner says the new count. */
static int32_t dm_roll_acc;
volatile int32_t dm_roll_edits;         /* for the tests */
static int32_t roll_turn(int32_t track, int32_t counts)
{
    int32_t t = track - DM_FIRST, lane = selected_lane(t), note = dm.note[t][lane], steps = 0, shown = -1;
    int32_t first = 16 * grid_page(track), len = track_len(track);
    uint8_t *data = track_data(track_obj(track));
    dm_roll_acc += counts;
    while (dm_roll_acc >= COUNTS) {
        dm_roll_acc -= COUNTS;
        steps++;
    }
    while (dm_roll_acc <= -COUNTS) {
        dm_roll_acc += COUNTS;
        steps--;
    }
    if (!steps || !data)
        return 0;
    for (int32_t k = 0; k < KEY_STEPS; k++) {
        int32_t step = first + k, n;
        if (!((dm_grid_keys >> k) & 1) || step >= len || !step_has(data, step, note))
            continue;
        n = roll_get(track, step, lane) + steps;
        n = n < 1 ? 1 : n > ROLL_MAX ? ROLL_MAX : n;
        roll_put(track, step, lane, n);
        dm_roll_edits++;
        if (shown < 0)
            shown = n;
    }
    if (shown > 0)
        say("ROLL %d", shown);
    dm_dirty = 1;
    return 1;
}

/* The roll knob E shows: the first held step's for the selected lane in GRID RECORDING, else 0 (a dash). */
static int32_t roll_shown(int32_t track)
{
    int32_t t = track - DM_FIRST, lane = selected_lane(t), first = 16 * grid_page(track);
    uint8_t *data;
    if (!grid_recording() || !dm_grid_keys || !(data = track_data(track_obj(track))))
        return 0;
    for (int32_t k = 0; k < KEY_STEPS; k++)
        if ((dm_grid_keys >> k) & 1)
            return step_has(data, first + k, dm.note[t][lane]) ? roll_get(track, first + k, lane) : 0;
    return 0;
}

int32_t dm_enc(void *brain, void *ev)
{
    int32_t enc = CORE_KEY_ID(ev), counts = (int32_t)CORE_KEY_FLAGS(ev), steps = 0;
    int32_t k = dm_last_pad, t, lane;
    struct dm_held *h;
    dm_brain = brain;

    if (dm_grid_keys) {                 /* steps held in GRID RECORDING: the knobs are the stock's locks (0.9.2) */
        dm_grid_long |= dm_grid_keys;
        if (enc == ENC_E && drum_track(fw_active_track) && shown_page_idx() == PAGE_MIDI_TRIG) {
            roll_turn(fw_active_track, counts);
            return 1;                   /* 0.11: knob E, the selected lane's roll on the held steps */
        }
        return 0;
    }
    if (k < 0 || !(dm_taken & (1u << k)) || dm_held[k].track < 0) {
        int32_t page = drum_track(fw_active_track) ? drum_page() : 0;
        if (enc == ENC_G && drum_track(fw_active_track) && shown_page_idx() == PAGE_MIDI_TRIG) {
            rpt_turn(fw_active_track, counts);
            return 1;                   /* 0.11: knob G, the repeat rate */
        }
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
            rearm(track);
            lane_off(k);                /* hear the new note: the old one off, the new one on */
            lane_on(k, track, lane);
        }
    } else {
        int32_t v = dm.velocity[t][lane] + steps;
        dm.velocity[t][lane] = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
        rearm(h->track);
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
    rearm(track);
    if (!(dm.enabled & bit))
        release_track(track);
    say((dm.enabled & bit) ? "DRUM %c ON" : "DRUM %c OFF", 'A' + track - DM_FIRST);
}

const struct item dm_menu = { "DRUM MODE", dm_open, CORE_MENU_ICON, dm_icon };

/* ---- the screen ------------------------------------------------------------------------------ */
/* The UI task, 60 times a second: count down the flashes and the messages, and ask for a redraw while anything
 * we draw changes. ctrl + 0x20: the redraw flag (as digisolo sets it). */
/* 0.10: the pattern menu's FUNC + NO reloads the active pattern from its saved copy, lanes included: its tracks
 * stop using their kits now, and take the reloaded ones a moment later (dm_reload_ticks, then reload_done). */
static int32_t dm_reload_ticks;
#define RELOAD_TICKS    10

void dm_pattern_op(int32_t op, int32_t index, int32_t what)
{
    if (op == CORE_PATTERN_SAVE && (what & CORE_PATTERN_SEQ) && dm_cur_pat >= 0 && index == dm_cur_pat) {
        for (int32_t t = 0; t < DM_TRACKS; t++)     /* 0.11: pin the kits the saved copy points to */
            pin_set(index, t, ((dm_unset >> t) & 1) ? -1 : dm_kit_of[t]);
        return;
    }
    if (op != CORE_PATTERN_RELOAD || !(what & CORE_PATTERN_SEQ) || dm_cur_pat < 0)
        return;
    dm_reload_ticks = RELOAD_TICKS;     /* read the pattern again a moment later */
}

/* 0.11: the kits the tracks used before are let go only now, after the new ones are taken: core calls ev_pattern_op
 * before the stock's checks, so a reload the stock refuses ("Prj must be re-saved!") reads the same settings back and
 * changes nothing (0.10 let them go at once, and a refused reload lost the pattern's kit). */
static void reload_done(void)
{
    int8_t old[DM_TRACKS];
    uint8_t old_unset = dm_unset;
    for (int32_t t = 0; t < DM_TRACKS; t++)
        old[t] = dm_kit_of[t];
    dm_cur_pat = -1;
    pattern_follow();
    for (int32_t t = 0; t < DM_TRACKS; t++)
        if (!((dm_unset >> t) & 1))
            dm_store.refs[dm_kit_of[t]]++;
    for (int32_t t = 0; t < DM_TRACKS; t++)
        if (!((old_unset >> t) & 1))
            kit_drop(old[t]);
    kits_trim();
    sq_rearm(0xf0u);
    dm_dirty = 1;
}

/* 0.10: what FUNC + RECORD copied, per track (only the playing pattern's lane values can be read, so they're kept
 * here): Drum Mode on/off, mutes and the kit's notes and velocities. */
static uint8_t dm_clip_on, dm_clip_mutes[DM_TRACKS];
static struct dm_kit dm_clip_kit[DM_TRACKS];

static void clip_copy(void)
{
    pattern_follow();
    dm_clip = dm_cur_pat;
    dm_clip_on = dm.enabled;
    for (int32_t t = 0; t < DM_TRACKS; t++) {
        dm_clip_mutes[t] = dm.mutes[t];
        kit_put(&dm_clip_kit[t], t);
    }
}

/* A pattern paste the stock did (FUNC + STOP): the playing pattern's 8 tracks now equal the copied pattern's, so its
 * Drum Mode settings follow. */
static void paste_check(void)
{
    int32_t dst = pattern_index(), src = dm_clip;
    const uint8_t *a, *b;
    if (src < 0 || dst < 0 || src == dst || dst != dm_cur_pat)
        return;
    a = fw_patterns + PATTERN_SIZE * src;
    b = fw_patterns + PATTERN_SIZE * dst;
    for (int32_t i = 0; i < 976 * 8; i++)
        if (a[i] != b[i])
            return;
    for (int32_t k = 0; k < KEY_STEPS; k++)
        if (dm_held[k].track >= 0)
            lane_off(k);
    dm.enabled = dm_clip_on;
    for (int32_t t = 0; t < DM_TRACKS; t++) {
        dm.mutes[t] = dm_clip_mutes[t];
        kit_get(&dm_clip_kit[t], t);
        rearm(DM_FIRST + t);
    }
    dm_paste_dst = dst;
    dm_dirty = 1;
}

void dm_tick(void *ctrl)
{
    task_start();                       /* 0.11: the task that sends notes between steps */
    rpt_tick();
    if (pattern_follow()) {             /* the pattern changed (0.10): its settings are in dm */
        sq_rearm(0xf0u);
        dm_dirty = 1;
    }
    if (dm_reload_ticks > 0 && --dm_reload_ticks == 0)
        reload_done();
    if (dm_paste_ticks > 0 && --dm_paste_ticks == 0)
        paste_check();
    for (int32_t k = 0; k < KEY_STEPS; k++)
        if (((dm_stock >> k) & 1) && ++dm_stock_age[k] >= STOCK_KEY_TICKS)
            dm_stock &= ~(1u << k);     /* its release never came */
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
    {                                   /* the LEDs follow GRID RECORDING, its page, the pattern and the stock's keys */
        static int32_t last_grid = -1, last_page = -1, last_track = -1, last_stock = -1;
        int32_t g = grid_recording(), track = fw_active_track;
        int32_t p = drum_track(track) ? grid_page(track) : 0, st = stock_keys();
        if (g != last_grid || p != last_page || track != last_track || st != last_stock)
            dm_dirty = 1;
        last_grid = g;
        last_page = p;
        last_track = track;
        last_stock = st;
    }
    {                                   /* the running light follows the playing step: an LED frame when the lit key changes */
        static int32_t last_run = -2;
        int32_t track = fw_active_track, r = -1;
        if (drum_track(track)) {
            clock_tick(track);
            if (!grid_recording())
                r = run_key(track);
        }
        if (r != last_run)
            dm_dirty = 1;               /* an LED frame (with a redraw: the frame alone isn't sent) */
        last_run = r;
        dm_run_key = r;
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
 * - else trig keys 1-8 as pads: white while held or just played, dim red when muted, dark otherwise; 9-16 dark; and
 *   over all 16 the running light (one key per step, red for the whole step, as the stock's playhead). */
void dm_leds(void *mgr)
{
    int32_t track = fw_active_track, t;
    if (!drum_track(track) || stock_keys())
        return;                         /* 0.10.1: pattern/bank select and chains show the stock's LEDs */
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
            FW_LED_SET(mgr, k + 1, LED_LAYER, !step_has(data, step, note) ? LED_DARK
                       : roll_get(track, step, selected_lane(t)) > 1 ? LED_WHITE : LED_RED);   /* 0.11: white, a roll */
        }
        return;
    }
    int32_t run = run_key(track);
    for (int32_t k = 0; k < KEY_STEPS; k++) {
        int32_t c = k == run ? LED_RED : LED_DARK, muted = (dm.mutes[t] >> k) & 1, sel = k == selected_lane(t);
        if (k < DM_LANES && k != run) {    /* 0.10.5: the running light shows over the pad for its step */
            if (((dm_lit_held[t] >> k) & 1) || dm_flash[t][k])
                c = LED_WHITE;
            else if (sel)
                c = muted ? LED_RED : LED_DIM_WHITE;    /* 0.9: the selected lane stays lit */
            else if (muted)
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

/* 0.9.2: the stock's pattern/bank select waiting for its trig key (after a press of PTN or BANK, until a pick, NO or
 * its four seconds): its view (first word 0x401c68dc, measured in digiemu) is on top of the main stack. */
#define SELECT_VIEW_VTABLE  0x401c68dcu
/* 0.10.1: the SCALE menu (FUNC + PAGE): PAGE sets the total length and the trig keys the number of steps, so its trig
 * keys are the stock's (measured in digiemu: this view on top of the main stack). */
#define SCALE_VIEW_VTABLE   0x401c6a84u

static int32_t top_view_is(uint32_t vtable)
{
    uint8_t *ms = *(uint8_t *const volatile *)MAIN_SCREEN_PP, *node, *v;
    if (!ms)
        return 0;
    node = *(uint8_t *const volatile *)(ms + 0x58);
    if (!node || node == ms + 0x54 || !(v = *(uint8_t *const volatile *)(node + 8)))
        return 0;
    return *(volatile uint32_t *)v == vtable;
}

static int32_t select_open(void)
{
    return top_view_is(SELECT_VIEW_VTABLE);
}

/* 0.10.1: the trig keys are the stock's now: PTN or BANK held, its pattern/bank select open, a chain's key held, or
 * the SCALE menu open (the trig keys set the length there). */
static int32_t stock_keys(void)
{
    return dm_ptn_held || dm_bank_held || dm_stock || select_open() || top_view_is(SCALE_VIEW_VTABLE);
}

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
#define DM_TITLE_Y      56
static void draw_drum_page(void *bmp, int32_t page, int32_t track)
{
    int32_t t = track - DM_FIRST, sel = selected_lane(t);
    ((rect_fn)fw_fillrect)(bmp, 20, 0, 127, 52, 0);
    if (!drum_track(track)) {
        ((text_fn)fw_textf)(bmp, (const void *)fw_font5, 30, 34, -1, "DRUM MODE IS OFF");
        ((text_fn)fw_textf)(bmp, (const void *)fw_font5, 30, 24, -1, "HOLD A TRACK KEY");
        return;
    }
    /* 0.10.5: Drum Mode writes the whole title on both views. The stock reads the title once, when the page opens, and
     * adds its own counter for the LFO group ("Drum Notes (2/2)": LFO 1, DRUM 2), so it said 2/2 on Drum Notes and,
     * once Drum Mode painted "Drum Velocity" over it, nothing there. DRUM counts its own two views: 1/2 and 2/2. The
     * bar is inverted (light, dark text): clear it dark, write light, invert; x 19-118 leaves the bar's rounded end. */
    ((rect_fn)fw_fillrect)(bmp, 19, 54, 118, 63, 0);
    ((text_fn)fw_textf)(bmp, (const void *)fw_font_title, 20, DM_TITLE_Y, -1, "%s (%d/2)",
                        page == 2 ? "Drum Velocity" : "Drum Notes", (int)page);
    ((rect_fn)fw_fillrect)(bmp, 19, 54, 118, 63, -1);
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
    if (!page && drum_track(track) && shown_page_idx() == PAGE_MIDI_LFO) {
        /* 0.10.6: the LFO page is one page; the stock says "LFO (1/2)", counting DRUM as page 2 of the LFO group. The
         * bar ends before the corner strip (x 94): clear it dark, write light, invert, as on the DRUM page. */
        ((rect_fn)fw_fillrect)(bmp, 19, 54, 92, 63, 0);
        ((text_fn)fw_textf)(bmp, (const void *)fw_font_title, 20, DM_TITLE_Y, -1, "LFO");
        ((rect_fn)fw_fillrect)(bmp, 19, 54, 92, 63, -1);
    }
    if (!page && drum_track(track) && shown_page_idx() == PAGE_MIDI_TRIG) {
        /* 0.11: knob E's empty box on the MIDI TRIG page: ROLL, the held step's count for the selected lane in GRID
         * RECORDING (x2-x4), else a dash. The box is the knob grid's fifth (bottom left), as the DRUM page's cells. */
        int32_t n = roll_shown(track);
        ((rect_fn)fw_fillrect)(bmp, 22, 0, 46, 24, 0);
        ((rect_fn)fw_framerect)(bmp, 26, 6, 42, 23, -1);              /* the box, as G's and H's (x 77-93, 103-119) */
        ((text_fn)fw_textf)(bmp, (const void *)fw_font5, n > 0 ? 30 : 32, 12, -1, n > 0 ? "x%d" : "-", (int)n);
        ((text_fn)fw_textf)(bmp, (const void *)fw_font5, 26, 0, -1, "ROLL");   /* the label under it, as LFO.T's */
        {                               /* knob G's box: REPEAT, the rate (16, 32, 64) or a dash */
            int32_t r = dm_rpt[track - DM_FIRST];
            ((rect_fn)fw_fillrect)(bmp, 74, 0, 96, 24, 0);
            ((rect_fn)fw_framerect)(bmp, 77, 6, 93, 23, -1);
            ((text_fn)fw_textf)(bmp, (const void *)fw_font5, r ? 81 : 83, 12, -1, dm_rpt_names[r]);
            ((text_fn)fw_textf)(bmp, (const void *)fw_font5, 78, 0, -1, "RPT");
        }
    }
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
