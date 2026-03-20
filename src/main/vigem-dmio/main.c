/**
 * vigem-dmio - Emulate an XInput (Xbox 360) controller from DrumMania IO.
 *
 * Requires dmio.dll in the same directory. Use dmio-p4io.dll (renamed
 * to dmio.dll) to read inputs from a real P4IO cabinet.
 *
 * Requires ViGEmBus driver: https://github.com/nefarius/ViGEmBus
 *
 * XInput mapping:
 *   Left Cymbal   -> LT   (left trigger, full press)
 *   Hi-Hat        -> LB   (left bumper)
 *   Left Pedal    -> Left Stick Click
 *   Snare         -> A
 *   Hi Tom        -> B
 *   Bass Pedal    -> RT   (right trigger, full press)
 *   Low Tom       -> X
 *   Floor Tom     -> Y
 *   Right Cymbal  -> RB   (right bumper)
 *   Start         -> Start
 *   Up/Down/Left/Right -> DPad
 *   Help          -> Back
 *   Service       -> Right Stick Click
 */

#define LOG_MODULE "vigem-dmio"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <windows.h>

#include "ViGEm/Client.h"

#include "bemanitools/dmio.h"
#include "util/log.h"
#include "util/thread.h"
#include "vigemstub/helper.h"

#define DM_PAD_COUNT 9
#define PAD_HIT_PULSE_FRAMES 2

int main(int argc, char **argv)
{
    log_to_writer(log_writer_stdout, NULL);

    dm_io_set_loggers(
        log_impl_misc, log_impl_info, log_impl_warning, log_impl_fatal);

    if (!dm_io_init(crt_thread_create, crt_thread_join, crt_thread_destroy)) {
        log_warning("Initializing dmio failed");
        return -1;
    }

    PVIGEM_CLIENT client = vigem_helper_setup();

    if (!client) {
        dm_io_fini();
        return -1;
    }

    PVIGEM_TARGET pad = vigem_helper_add_pad(client);

    if (!pad) {
        vigem_free(client);
        dm_io_fini();
        return -1;
    }

    log_info("Virtual Xbox 360 controller created. Starting poll loop.");
    log_info("Hold Service + Test together to exit.");

    XUSB_REPORT report;
    uint16_t prev_pads = 0;
    uint8_t pad_pulse[DM_PAD_COUNT];

    memset(pad_pulse, 0, sizeof(pad_pulse));

    const enum dm_io_pad_bit pad_index[DM_PAD_COUNT] = {
        DM_IO_PAD_LEFT_CYMBAL,
        DM_IO_PAD_HIHAT,
        DM_IO_PAD_LEFT_PEDAL,
        DM_IO_PAD_SNARE,
        DM_IO_PAD_HI_TOM,
        DM_IO_PAD_BASS_PEDAL,
        DM_IO_PAD_LOW_TOM,
        DM_IO_PAD_FLOOR_TOM,
        DM_IO_PAD_RIGHT_CYMBAL,
    };

    while (true) {
        if (!dm_io_read_inputs()) {
            log_warning("dm_io_read_inputs failed");
            Sleep(10);
            continue;
        }

        uint16_t pads = dm_io_get_pad_inputs();
        uint16_t sys = dm_io_get_sys_inputs();

        for (size_t i = 0; i < DM_PAD_COUNT; i++) {
            uint16_t mask = (uint16_t) (1U << pad_index[i]);
            bool pressed = (pads & mask) != 0;
            bool was_pressed = (prev_pads & mask) != 0;

            if (pressed && !was_pressed) {
                pad_pulse[i] = PAD_HIT_PULSE_FRAMES;
            } else if (pad_pulse[i] > 0) {
                pad_pulse[i]--;
            }
        }

        bool left_cymbal =
            (pads & (1U << DM_IO_PAD_LEFT_CYMBAL)) || pad_pulse[0] > 0;
        bool hihat = (pads & (1U << DM_IO_PAD_HIHAT)) || pad_pulse[1] > 0;
        bool left_pedal =
            (pads & (1U << DM_IO_PAD_LEFT_PEDAL)) || pad_pulse[2] > 0;
        bool snare = (pads & (1U << DM_IO_PAD_SNARE)) || pad_pulse[3] > 0;
        bool hi_tom = (pads & (1U << DM_IO_PAD_HI_TOM)) || pad_pulse[4] > 0;
        bool bass = (pads & (1U << DM_IO_PAD_BASS_PEDAL)) || pad_pulse[5] > 0;
        bool low_tom = (pads & (1U << DM_IO_PAD_LOW_TOM)) || pad_pulse[6] > 0;
        bool floor_tom =
            (pads & (1U << DM_IO_PAD_FLOOR_TOM)) || pad_pulse[7] > 0;
        bool right_cymbal =
            (pads & (1U << DM_IO_PAD_RIGHT_CYMBAL)) || pad_pulse[8] > 0;

        memset(&report, 0, sizeof(report));

        if (left_cymbal)
            report.bLeftTrigger = 255;
        if (hihat)
            report.wButtons |= XUSB_GAMEPAD_LEFT_SHOULDER;
        if (left_pedal)
            report.wButtons |= XUSB_GAMEPAD_LEFT_THUMB;
        if (snare)
            report.wButtons |= XUSB_GAMEPAD_A;
        if (hi_tom)
            report.wButtons |= XUSB_GAMEPAD_B;
        if (bass)
            report.bRightTrigger = 255;
        if (low_tom)
            report.wButtons |= XUSB_GAMEPAD_X;
        if (floor_tom)
            report.wButtons |= XUSB_GAMEPAD_Y;
        if (right_cymbal)
            report.wButtons |= XUSB_GAMEPAD_RIGHT_SHOULDER;

        if (sys & (1 << DM_IO_SYS_START))
            report.wButtons |= XUSB_GAMEPAD_START;
        if (sys & (1 << DM_IO_SYS_UP))
            report.wButtons |= XUSB_GAMEPAD_DPAD_UP;
        if (sys & (1 << DM_IO_SYS_DOWN))
            report.wButtons |= XUSB_GAMEPAD_DPAD_DOWN;
        if (sys & (1 << DM_IO_SYS_LEFT))
            report.wButtons |= XUSB_GAMEPAD_DPAD_LEFT;
        if (sys & (1 << DM_IO_SYS_RIGHT))
            report.wButtons |= XUSB_GAMEPAD_DPAD_RIGHT;
        if (sys & (1 << DM_IO_SYS_HELP))
            report.wButtons |= XUSB_GAMEPAD_BACK;
        if (sys & (1 << DM_IO_SYS_SERVICE))
            report.wButtons |= XUSB_GAMEPAD_RIGHT_THUMB;

        vigem_target_x360_update(client, pad, report);

        if ((sys & (1 << DM_IO_SYS_SERVICE)) && (sys & (1 << DM_IO_SYS_TEST))) {
            log_info("Service + Test pressed, exiting.");
            break;
        }

        prev_pads = pads;

        Sleep(1);
    }

    vigem_target_remove(client, pad);
    vigem_target_free(pad);
    vigem_free(client);
    dm_io_fini();

    return 0;
}
