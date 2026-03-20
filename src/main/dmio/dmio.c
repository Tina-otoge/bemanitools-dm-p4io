/* Default dmio.dll implementation using the geninput keyboard/joystick
   mapper. Useful for testing without real P4IO hardware.

   If you want to implement support for DM IO hardware, create a DLL that
   implements the same dmio.h API and deploy it as dmio.dll. See
   dmio-p4io for an example that targets the P4IO USB device. */

// clang-format off
// Don't format because the order is important here
#include <windows.h>
#include <mmsystem.h>
// clang-format on

#include <string.h>

#include "bemanitools/dmio.h"
#include "bemanitools/input.h"

static uint16_t dm_io_pad_state;
static uint16_t dm_io_sys_state;
static uint8_t dm_io_pad_debounce[16];
static uint8_t dm_io_sys_debounce[16];

static uint16_t dm_debounce_mask(
    uint16_t stable,
    uint16_t raw,
    uint8_t counters[16],
    uint8_t threshold,
    uint8_t nbits)
{
    for (uint8_t i = 0; i < nbits; i++) {
        uint16_t mask = (uint16_t) (1U << i);
        bool stable_bit = (stable & mask) != 0;
        bool raw_bit = (raw & mask) != 0;

        if (stable_bit == raw_bit) {
            counters[i] = 0;
            continue;
        }

        if (++counters[i] < threshold) {
            continue;
        }

        counters[i] = 0;

        if (raw_bit) {
            stable |= mask;
        } else {
            stable &= (uint16_t) ~mask;
        }
    }

    return stable;
}

void dm_io_set_loggers(
    log_formatter_t misc,
    log_formatter_t info,
    log_formatter_t warning,
    log_formatter_t fatal)
{
    input_set_loggers(misc, info, warning, fatal);
}

bool dm_io_init(
    thread_create_t thread_create,
    thread_join_t thread_join,
    thread_destroy_t thread_destroy)
{
    timeBeginPeriod(1);
    input_init(thread_create, thread_join, thread_destroy);
    mapper_config_load("dm");

    dm_io_pad_state = 0;
    dm_io_sys_state = 0;
    memset(dm_io_pad_debounce, 0, sizeof(dm_io_pad_debounce));
    memset(dm_io_sys_debounce, 0, sizeof(dm_io_sys_debounce));

    return true;
}

void dm_io_fini(void)
{
    input_fini();
    timeEndPeriod(1);
}

bool dm_io_read_inputs(void)
{
    Sleep(1);

    uint32_t buttons = (uint32_t) mapper_update();
    uint16_t raw_pad;
    uint16_t raw_sys;

    /* bits 0-8: pad inputs (DM_IO_PAD_*), bits 16-26: sys inputs (DM_IO_SYS_*)
     */
    raw_pad = buttons & 0x01FF;
    raw_sys = (buttons >> 16) & 0x07FF;

    dm_io_pad_state =
        dm_debounce_mask(dm_io_pad_state, raw_pad, dm_io_pad_debounce, 2, 9);
    dm_io_sys_state =
        dm_debounce_mask(dm_io_sys_state, raw_sys, dm_io_sys_debounce, 2, 11);

    return true;
}

uint16_t dm_io_get_pad_inputs(void)
{
    return dm_io_pad_state;
}

uint16_t dm_io_get_sys_inputs(void)
{
    return dm_io_sys_state;
}
