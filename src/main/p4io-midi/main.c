/**
 * p4io-midi - Emit MIDI note events from P4IO drum inputs.
 *
 * Run p4iotest.exe first to discover the correct bit positions for your
 * cabinet, then update the DM_BIT_* constants below.
 *
 * All drum pad bits are active LOW in jamma[0].
 * System buttons (coin, service, test) are active HIGH in jamma[0].
 *
 * Uses the Windows multimedia MIDI API (no extra drivers needed).
 * MIDI notes are sent on channel 10 (the General MIDI percussion channel).
 *
 * GM drum note mapping used:
 *   Bass Drum 1      -> 36
 *   Snare (Acoustic) -> 38
 *   Hi-Hat Closed    -> 42
 *   Hi-Hat Pedal     -> 44
 *   Hi-Hat Open      -> 46
 *   Floor Tom        -> 43
 *   Low-Mid Tom      -> 47
 *   Hi Tom           -> 50
 *   Crash Cymbal 1   -> 49  (Left Cymbal)
 *   Ride Cymbal 1    -> 51  (Right Cymbal)
 *
 * Usage: p4io-midi.exe [midi-device-index]
 *   Run without arguments to list devices and use device 0.
 *
 * Hold Service + Test to exit.
 */

#define LOG_MODULE "p4io-midi"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <windows.h>
#include <mmsystem.h>

#include "p4iodrv/device.h"
#include "util/log.h"

/*
 * TODO: Run p4iotest.exe and hit each pad to find the right bit numbers.
 * These are bit positions within jamma[0]. Active LOW means the bit is 0
 * when the pad is pressed.
 */
#define DM_BIT_LEFT_CYMBAL 0  /* TODO */
#define DM_BIT_HIHAT 1        /* TODO */
#define DM_BIT_SNARE 2        /* TODO */
#define DM_BIT_HI_TOM 3       /* TODO */
#define DM_BIT_LOW_TOM 4      /* TODO */
#define DM_BIT_FLOOR_TOM 5    /* TODO */
#define DM_BIT_RIGHT_CYMBAL 6 /* TODO */
#define DM_BIT_BASS_PEDAL 7   /* TODO */
#define DM_BIT_HIHAT_PEDAL 8  /* TODO */

/* System buttons are active HIGH */
#define DM_BIT_SERVICE 25
#define DM_BIT_TEST 24

/* MIDI channel 10 (0-indexed = 9) is the GM percussion channel */
#define MIDI_CHANNEL 9
#define MIDI_VELOCITY 100

/* GM drum note numbers */
#define NOTE_BASS_DRUM 36
#define NOTE_SNARE 38
#define NOTE_HIHAT_CLOSED 42
#define NOTE_HIHAT_PEDAL 44
#define NOTE_HIHAT_OPEN 46
#define NOTE_FLOOR_TOM 43
#define NOTE_LOW_TOM 47
#define NOTE_HI_TOM 50
#define NOTE_LEFT_CYMBAL 49
#define NOTE_RIGHT_CYMBAL 51

struct pad_mapping
{
    int bit;
    uint8_t note;
    const char *name;
};

static const struct pad_mapping mappings[] = {
    {DM_BIT_LEFT_CYMBAL, NOTE_LEFT_CYMBAL, "Left Cymbal"},
    {DM_BIT_HIHAT, NOTE_HIHAT_CLOSED, "Hi-Hat"},
    {DM_BIT_SNARE, NOTE_SNARE, "Snare"},
    {DM_BIT_HI_TOM, NOTE_HI_TOM, "Hi Tom"},
    {DM_BIT_LOW_TOM, NOTE_LOW_TOM, "Low Tom"},
    {DM_BIT_FLOOR_TOM, NOTE_FLOOR_TOM, "Floor Tom"},
    {DM_BIT_RIGHT_CYMBAL, NOTE_RIGHT_CYMBAL, "Right Cymbal"},
    {DM_BIT_BASS_PEDAL, NOTE_BASS_DRUM, "Bass Pedal"},
    {DM_BIT_HIHAT_PEDAL, NOTE_HIHAT_PEDAL, "Hi-Hat Pedal"},
};

#define NUM_PADS (sizeof(mappings) / sizeof(mappings[0]))

static void midi_note_on(HMIDIOUT out, uint8_t note)
{
    DWORD msg =
        (MIDI_VELOCITY << 16) | (note << 8) | (0x90 | MIDI_CHANNEL);
    midiOutShortMsg(out, msg);
}

static void midi_note_off(HMIDIOUT out, uint8_t note)
{
    DWORD msg = (note << 8) | (0x80 | MIDI_CHANNEL);
    midiOutShortMsg(out, msg);
}

static bool bit_low(uint32_t jamma0, int bit)
{
    return !((jamma0 >> bit) & 1);
}

static bool bit_high(uint32_t jamma0, int bit)
{
    return (jamma0 >> bit) & 1;
}

static HMIDIOUT open_midi_device(int device_index)
{
    UINT num_devs = midiOutGetNumDevs();
    if (num_devs == 0)
    {
        fprintf(stderr, "No MIDI output devices found.\n");
        return NULL;
    }

    printf("Available MIDI output devices:\n");
    for (UINT i = 0; i < num_devs; i++)
    {
        MIDIOUTCAPSA caps;
        if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
        {
            printf("  [%u] %s\n", i, caps.szPname);
        }
    }

    if (device_index < 0 || (UINT)device_index >= num_devs)
    {
        printf(
            "Using default MIDI device (index 0). Pass device index as "
            "first argument to choose.\n");
        device_index = 0;
    }

    HMIDIOUT out;
    MMRESULT result = midiOutOpen(&out, device_index, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR)
    {
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
    if (!midi_out)
    {
        return 1;
    }

    log_info("Opening P4IO...");
    struct p4iodrv_ctx *ctx = p4iodrv_open();
    if (!ctx)
    {
        fprintf(stderr, "Failed to open P4IO.\n");
        midiOutClose(midi_out);
        return 1;
    }

    log_info("Ready. Drumming will emit MIDI notes on channel 10.");
    log_info("Hold Service + Test to exit.");
    printf("\n%-16s  MIDI note\n", "Pad");
    printf("--------------------------------\n");
    for (size_t i = 0; i < NUM_PADS; i++)
    {
        printf("%-16s  %u\n", mappings[i].name, mappings[i].note);
    }
    printf("\n");

    uint32_t jamma[4];
    bool was_pressed[NUM_PADS];
    memset(was_pressed, 0, sizeof(was_pressed));

    while (true)
    {
        if (!p4iodrv_read_jamma(ctx, jamma))
        {
            log_warning("JAMMA read failed");
            Sleep(10);
            continue;
        }

        uint32_t j0 = jamma[0];

        for (size_t i = 0; i < NUM_PADS; i++)
        {
            bool pressed = bit_low(j0, mappings[i].bit);

            if (pressed && !was_pressed[i])
            {
                midi_note_on(midi_out, mappings[i].note);
                printf(
                    "HIT: %-16s (note %u)\n",
                    mappings[i].name,
                    mappings[i].note);
            }
            else if (!pressed && was_pressed[i])
            {
                midi_note_off(midi_out, mappings[i].note);
            }

            was_pressed[i] = pressed;
        }

        if (bit_high(j0, DM_BIT_SERVICE) && bit_high(j0, DM_BIT_TEST))
        {
            log_info("Service + Test pressed, exiting.");
            break;
        }

        Sleep(1);
    }

    for (size_t i = 0; i < NUM_PADS; i++)
    {
        if (was_pressed[i])
        {
            midi_note_off(midi_out, mappings[i].note);
        }
    }

    midiOutClose(midi_out);
    p4iodrv_close(ctx);

    return 0;
}
