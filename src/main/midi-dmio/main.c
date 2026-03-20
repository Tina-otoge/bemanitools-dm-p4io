/**
 * midi-dmio - Emit MIDI note events from DrumMania IO inputs.
 *
 * Requires dmio.dll in the same directory. Use dmio-p4io.dll (renamed
 * to dmio.dll) to read inputs from a real P4IO cabinet.
 *
 * Uses the Windows multimedia MIDI API (no extra drivers needed).
 * MIDI notes are sent on channel 10 (the General MIDI percussion channel).
 *
 * GM drum note mapping:
 *   Left Cymbal   -> 49  (Crash Cymbal 1)
 *   Hi-Hat        -> 42  (Closed Hi-Hat)
 *   Left Pedal    -> 44  (Hi-Hat Pedal)
 *   Snare         -> 38  (Acoustic Snare)
 *   Hi Tom        -> 50  (High Tom)
 *   Bass Pedal    -> 36  (Bass Drum 1)
 *   Low Tom       -> 47  (Low-Mid Tom)
 *   Floor Tom     -> 43  (High Floor Tom)
 *   Right Cymbal  -> 51  (Ride Cymbal 1)
 *
 * Usage: midi-dmio.exe [midi-device-index]
 *   Run without arguments to list available devices and use device 0.
 *
 * Hold Service + Test to exit.
 */

#define LOG_MODULE "midi-dmio"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <mmsystem.h>

#include "bemanitools/dmio.h"
#include "util/log.h"
#include "util/thread.h"

/* MIDI channel 10 (0-indexed = 9) is the GM percussion channel */
#define MIDI_CHANNEL 9
#define MIDI_VELOCITY 100
#define MIDI_VELOCITY_ACCENT 120
#define MIDI_NOTE_LENGTH_MS 45
#define MIDI_RETRIGGER_GUARD_MS 20

/* GM drum note numbers */
#define NOTE_LEFT_CYMBAL 49
#define NOTE_HIHAT_CLOSED 42
#define NOTE_HIHAT_PEDAL 44
#define NOTE_SNARE 38
#define NOTE_HI_TOM 50
#define NOTE_BASS_DRUM 36
#define NOTE_LOW_TOM 47
#define NOTE_FLOOR_TOM 43
#define NOTE_RIGHT_CYMBAL 51

struct pad_mapping {
    enum dm_io_pad_bit bit;
    uint8_t note;
    const char *name;
};

static const struct pad_mapping mappings[] = {
    {DM_IO_PAD_LEFT_CYMBAL, NOTE_LEFT_CYMBAL, "Left Cymbal"},
    {DM_IO_PAD_HIHAT, NOTE_HIHAT_CLOSED, "Hi-Hat"},
    {DM_IO_PAD_LEFT_PEDAL, NOTE_HIHAT_PEDAL, "Left Pedal (Hi-Hat)"},
    {DM_IO_PAD_SNARE, NOTE_SNARE, "Snare"},
    {DM_IO_PAD_HI_TOM, NOTE_HI_TOM, "Hi Tom"},
    {DM_IO_PAD_BASS_PEDAL, NOTE_BASS_DRUM, "Bass Pedal"},
    {DM_IO_PAD_LOW_TOM, NOTE_LOW_TOM, "Low Tom"},
    {DM_IO_PAD_FLOOR_TOM, NOTE_FLOOR_TOM, "Floor Tom"},
    {DM_IO_PAD_RIGHT_CYMBAL, NOTE_RIGHT_CYMBAL, "Right Cymbal"},
};

#define NUM_PADS (sizeof(mappings) / sizeof(mappings[0]))

static void midi_note_on_velocity(HMIDIOUT out, uint8_t note, uint8_t velocity)
{
    DWORD msg = (((DWORD) velocity) << 16) | (((DWORD) note) << 8) |
        (0x90 | MIDI_CHANNEL);
    midiOutShortMsg(out, msg);
}

static void midi_note_off(HMIDIOUT out, uint8_t note)
{
    DWORD msg = (note << 8) | (0x80 | MIDI_CHANNEL);
    midiOutShortMsg(out, msg);
}

static HMIDIOUT open_midi_device(int device_index)
{
    UINT num_devs = midiOutGetNumDevs();

    if (num_devs == 0) {
        fprintf(stderr, "No MIDI output devices found.\n");
        return NULL;
    }

    printf("Available MIDI output devices:\n");
    for (UINT i = 0; i < num_devs; i++) {
        MIDIOUTCAPSA caps;
        if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            printf("  [%u] %s\n", i, caps.szPname);
        }
    }

    if (device_index < 0 || (UINT) device_index >= num_devs) {
        printf(
            "Using default MIDI device (index 0). "
            "Pass device index as first argument to choose.\n");
        device_index = 0;
    }

    HMIDIOUT out;
    MMRESULT result = midiOutOpen(&out, device_index, 0, 0, CALLBACK_NULL);

    if (result != MMSYSERR_NOERROR) {
        fprintf(stderr, "midiOutOpen failed: error %u\n", result);
        return NULL;
    }

    MIDIOUTCAPSA caps;
    midiOutGetDevCapsA(device_index, &caps, sizeof(caps));
    printf("Opened MIDI device [%d]: %s\n", device_index, caps.szPname);

    return out;
}

int main(int argc, char **argv)
{
    log_to_writer(log_writer_stdout, NULL);

    int midi_device = (argc > 1) ? atoi(argv[1]) : 0;

    HMIDIOUT midi_out = open_midi_device(midi_device);
    if (!midi_out) {
        return 1;
    }

    dm_io_set_loggers(
        log_impl_misc, log_impl_info, log_impl_warning, log_impl_fatal);

    if (!dm_io_init(crt_thread_create, crt_thread_join, crt_thread_destroy)) {
        log_warning("Initializing dmio failed");
        midiOutClose(midi_out);
        return -1;
    }

    log_info("Ready. Drumming will emit MIDI notes on channel 10.");
    log_info("Hold Service + Test to exit.");

    printf("\n%-24s  MIDI note\n", "Pad");
    printf("----------------------------------\n");
    for (size_t i = 0; i < NUM_PADS; i++) {
        printf("%-24s  %u\n", mappings[i].name, mappings[i].note);
    }
    printf("\n");

    bool was_pressed[NUM_PADS];
    bool note_active[NUM_PADS];
    uint32_t note_off_at[NUM_PADS];
    uint32_t can_retrigger_at[NUM_PADS];
    uint32_t last_hit_at[NUM_PADS];

    memset(was_pressed, 0, sizeof(was_pressed));
    memset(note_active, 0, sizeof(note_active));
    memset(note_off_at, 0, sizeof(note_off_at));
    memset(can_retrigger_at, 0, sizeof(can_retrigger_at));
    memset(last_hit_at, 0, sizeof(last_hit_at));

    while (true) {
        if (!dm_io_read_inputs()) {
            log_warning("dm_io_read_inputs failed");
            Sleep(10);
            continue;
        }

        uint16_t pads = dm_io_get_pad_inputs();
        uint16_t sys = dm_io_get_sys_inputs();
        uint32_t now_ms = GetTickCount();

        for (size_t i = 0; i < NUM_PADS; i++) {
            bool pressed = (pads >> mappings[i].bit) & 1;
            bool hit =
                pressed && !was_pressed[i] && now_ms >= can_retrigger_at[i];

            if (hit) {
                uint8_t velocity = MIDI_VELOCITY;

                if (last_hit_at[i] != 0 && (now_ms - last_hit_at[i]) <= 120) {
                    velocity = MIDI_VELOCITY_ACCENT;
                }

                if (note_active[i]) {
                    midi_note_off(midi_out, mappings[i].note);
                    note_active[i] = false;
                }

                midi_note_on_velocity(midi_out, mappings[i].note, velocity);

                note_active[i] = true;
                note_off_at[i] = now_ms + MIDI_NOTE_LENGTH_MS;
                can_retrigger_at[i] = now_ms + MIDI_RETRIGGER_GUARD_MS;
                last_hit_at[i] = now_ms;

                printf(
                    "HIT: %-24s (note %u vel %u)\n",
                    mappings[i].name,
                    mappings[i].note,
                    velocity);
            }

            if (note_active[i] && now_ms >= note_off_at[i]) {
                midi_note_off(midi_out, mappings[i].note);
                note_active[i] = false;
            }

            was_pressed[i] = pressed;
        }

        if ((sys & (1 << DM_IO_SYS_SERVICE)) && (sys & (1 << DM_IO_SYS_TEST))) {
            log_info("Service + Test pressed, exiting.");
            break;
        }

        Sleep(1);
    }

    for (size_t i = 0; i < NUM_PADS; i++) {
        if (note_active[i] || was_pressed[i]) {
            midi_note_off(midi_out, mappings[i].note);
        }
    }

    midiOutClose(midi_out);
    dm_io_fini();

    return 0;
}
