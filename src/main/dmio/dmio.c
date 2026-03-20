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

#include "bemanitools/dmio.h"
#include "bemanitools/input.h"

static uint16_t dm_io_pad_state;
static uint16_t dm_io_sys_state;

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

    /* bits 0-8: pad inputs (DM_IO_PAD_*), bits 16-26: sys inputs (DM_IO_SYS_*)
     */
    dm_io_pad_state = buttons & 0x01FF;
    dm_io_sys_state = (buttons >> 16) & 0x07FF;

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
