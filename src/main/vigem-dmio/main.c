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

    while (true) {
        if (!dm_io_read_inputs()) {
            log_warning("dm_io_read_inputs failed");
            Sleep(10);
            continue;
        }

        uint16_t pads = dm_io_get_pad_inputs();
        uint16_t sys = dm_io_get_sys_inputs();

        memset(&report, 0, sizeof(report));

        if (pads & (1 << DM_IO_PAD_LEFT_CYMBAL))
            report.bLeftTrigger = 255;
        if (pads & (1 << DM_IO_PAD_HIHAT))
            report.wButtons |= XUSB_GAMEPAD_LEFT_SHOULDER;
        if (pads & (1 << DM_IO_PAD_LEFT_PEDAL))
            report.wButtons |= XUSB_GAMEPAD_LEFT_THUMB;
        if (pads & (1 << DM_IO_PAD_SNARE))
            report.wButtons |= XUSB_GAMEPAD_A;
        if (pads & (1 << DM_IO_PAD_HI_TOM))
            report.wButtons |= XUSB_GAMEPAD_B;
        if (pads & (1 << DM_IO_PAD_BASS_PEDAL))
            report.bRightTrigger = 255;
        if (pads & (1 << DM_IO_PAD_LOW_TOM))
            report.wButtons |= XUSB_GAMEPAD_X;
        if (pads & (1 << DM_IO_PAD_FLOOR_TOM))
            report.wButtons |= XUSB_GAMEPAD_Y;
        if (pads & (1 << DM_IO_PAD_RIGHT_CYMBAL))
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

        Sleep(1);
    }

    vigem_target_remove(client, pad);
    vigem_target_free(pad);
    vigem_free(client);
    dm_io_fini();

    return 0;
}
