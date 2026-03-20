/**
 * p4io-xinput - Emulate an XInput (Xbox 360) controller from P4IO drum inputs.
 *
 * Run p4iotest first to discover the correct bit positions for your cabinet,
 * then update the DM_BIT_* constants below.
 *
 * All drum pad bits are active LOW in jamma[0].
 * System buttons (coin, service, test) are active HIGH in jamma[0].
 *
 * Requires ViGEmBus driver: https://github.com/nefarius/ViGEmBus
 *
 * XInput mapping:
 *   Hi-Hat        -> LB   (left bumper)
 *   Snare         -> A
 *   Hi Tom        -> B
 *   Low Tom       -> X
 *   Floor Tom     -> Y
 *   Left Cymbal   -> LT   (left trigger, full press)
 *   Right Cymbal  -> RB   (right bumper)
 *   Bass Pedal    -> RT   (right trigger, full press)
 *   Hi-Hat Pedal  -> Left Stick Click
 *   Start         -> Start
 *   Select/Help   -> Back
 *   Service       -> Right Stick Click
 */

#define LOG_MODULE "p4io-xinput"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <windows.h>

#include "ViGEm/Client.h"

#include "p4iodrv/device.h"
#include "util/log.h"
#include "vigemstub/helper.h"

/*
 * TODO: Run p4iotest.exe and hit each pad to find the right bit numbers.
 * These are bit positions within jamma[0]. Active LOW means the bit is 0
 * when the pad is pressed (all other bits default to 1).
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
#define DM_BIT_START 16       /* TODO */
#define DM_BIT_SELECT 17      /* TODO */

/* System buttons are active HIGH */
#define DM_BIT_COIN 28
#define DM_BIT_SERVICE 25
#define DM_BIT_TEST 24

static bool bit_low(uint32_t jamma0, int bit)
{
    return !((jamma0 >> bit) & 1);
}

static bool bit_high(uint32_t jamma0, int bit)
{
    return (jamma0 >> bit) & 1;
}

int main(int argc, char **argv)
{
    log_to_writer(log_writer_stdout, NULL);

    log_info("Opening P4IO...");
    struct p4iodrv_ctx *ctx = p4iodrv_open();
    if (!ctx)
    {
        fprintf(stderr, "Failed to open P4IO.\n");
        return 1;
    }

    log_info("Connecting to ViGEmBus...");
    PVIGEM_CLIENT client = vigem_helper_setup();
    if (!client)
    {
        p4iodrv_close(ctx);
        return 1;
    }

    PVIGEM_TARGET pad = vigem_helper_add_pad(client);
    if (!pad)
    {
        vigem_free(client);
        p4iodrv_close(ctx);
        return 1;
    }

    log_info("Virtual Xbox 360 controller created. Starting poll loop.");
    log_info("Hold Service + Test together to exit.");

    uint32_t jamma[4];
    XUSB_REPORT report;

    while (true)
    {
        if (!p4iodrv_read_jamma(ctx, jamma))
        {
            log_warning("JAMMA read failed");
            Sleep(10);
            continue;
        }

        uint32_t j0 = jamma[0];

        memset(&report, 0, sizeof(report));

        if (bit_low(j0, DM_BIT_HIHAT))
            report.wButtons |= XUSB_GAMEPAD_LEFT_SHOULDER;
        if (bit_low(j0, DM_BIT_SNARE))
            report.wButtons |= XUSB_GAMEPAD_A;
        if (bit_low(j0, DM_BIT_HI_TOM))
            report.wButtons |= XUSB_GAMEPAD_B;
        if (bit_low(j0, DM_BIT_LOW_TOM))
            report.wButtons |= XUSB_GAMEPAD_X;
        if (bit_low(j0, DM_BIT_FLOOR_TOM))
            report.wButtons |= XUSB_GAMEPAD_Y;
        if (bit_low(j0, DM_BIT_LEFT_CYMBAL))
            report.bLeftTrigger = 255;
        if (bit_low(j0, DM_BIT_RIGHT_CYMBAL))
            report.wButtons |= XUSB_GAMEPAD_RIGHT_SHOULDER;
        if (bit_low(j0, DM_BIT_BASS_PEDAL))
            report.bRightTrigger = 255;
        if (bit_low(j0, DM_BIT_HIHAT_PEDAL))
            report.wButtons |= XUSB_GAMEPAD_LEFT_THUMB;
        if (bit_low(j0, DM_BIT_START))
            report.wButtons |= XUSB_GAMEPAD_START;
        if (bit_low(j0, DM_BIT_SELECT))
            report.wButtons |= XUSB_GAMEPAD_BACK;
        if (bit_high(j0, DM_BIT_SERVICE))
            report.wButtons |= XUSB_GAMEPAD_RIGHT_THUMB;

        vigem_target_x360_update(client, pad, report);

        if (bit_high(j0, DM_BIT_SERVICE) && bit_high(j0, DM_BIT_TEST))
        {
            log_info("Service + Test pressed, exiting.");
            break;
        }

        Sleep(1);
    }

    vigem_target_remove(client, pad);
    vigem_target_free(pad);
    vigem_free(client);
    p4iodrv_close(ctx);

    return 0;
}
