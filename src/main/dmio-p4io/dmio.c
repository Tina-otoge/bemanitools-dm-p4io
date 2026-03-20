/* dmio-p4io.dll - DrumMania IO provider backed by the P4IO USB device.
 *
 * Deploy this as dmio.dll alongside vigem-dmio.exe or midi-dmio.exe
 * to use a real P4IO cabinet as input.
 *
 * All drum pad inputs read from jamma[0] are active LOW.
 * System button inputs are active HIGH.
 *
 * Run aciotest (p4io mode) to discover the correct jamma bit positions
 * for your cabinet and update the JAMMA_BIT_* constants below.
 */

// clang-format off
// Don't format because the order is important here
#include <windows.h>
#include <setupapi.h>
// clang-format on

#include "bemanitools/dmio.h"
#include "p4iodrv/device.h"
#include "util/log.h"

/* ---------------------------------------------------------------------------
 * Jamma bit positions for drum pad inputs (active LOW in jamma[0]).
 * TODO: Run aciotest and hit each pad to confirm the correct bit numbers.
 * ---------------------------------------------------------------------------
 */

#define JAMMA_BIT_LEFT_CYMBAL 0 /* TODO */
#define JAMMA_BIT_HIHAT 1 /* TODO */
#define JAMMA_BIT_LEFT_PEDAL 8 /* TODO */
#define JAMMA_BIT_SNARE 2 /* TODO */
#define JAMMA_BIT_HI_TOM 3 /* TODO */
#define JAMMA_BIT_BASS_PEDAL 7 /* TODO */
#define JAMMA_BIT_LOW_TOM 4 /* TODO */
#define JAMMA_BIT_FLOOR_TOM 5 /* TODO */
#define JAMMA_BIT_RIGHT_CYMBAL 6 /* TODO */

/* ---------------------------------------------------------------------------
 * Jamma bit positions for system buttons (active HIGH in jamma[0]).
 * These match the standard P4IO assignment shared with Jubeat cabinets.
 * TODO: Verify on real hardware.
 * ---------------------------------------------------------------------------
 */

#define JAMMA_BIT_SERVICE 25
#define JAMMA_BIT_TEST 24
#define JAMMA_BIT_COIN 28
#define JAMMA_BIT_START 16 /* TODO */
#define JAMMA_BIT_UP 17 /* TODO */
#define JAMMA_BIT_DOWN 18 /* TODO */
#define JAMMA_BIT_LEFT 19 /* TODO */
#define JAMMA_BIT_RIGHT 20 /* TODO */
#define JAMMA_BIT_HELP 21 /* TODO */
#define JAMMA_BIT_EXTRA1 22 /* TODO */
#define JAMMA_BIT_EXTRA2 23 /* TODO */

static struct p4iodrv_ctx *p4io_ctx;
static uint16_t dm_pad_state;
static uint16_t dm_sys_state;

void dm_io_set_loggers(
    log_formatter_t misc,
    log_formatter_t info,
    log_formatter_t warning,
    log_formatter_t fatal)
{
    log_to_external(misc, info, warning, fatal);
}

bool dm_io_init(
    thread_create_t thread_create,
    thread_join_t thread_join,
    thread_destroy_t thread_destroy)
{
    p4io_ctx = p4iodrv_open();

    if (!p4io_ctx) {
        log_warning("Failed to open P4IO");
        return false;
    }

    return true;
}

void dm_io_fini(void)
{
    if (p4io_ctx) {
        p4iodrv_close(p4io_ctx);
        p4io_ctx = NULL;
    }
}

static bool bit_low(uint32_t val, int bit)
{
    return !((val >> bit) & 1);
}

static bool bit_high(uint32_t val, int bit)
{
    return (val >> bit) & 1;
}

bool dm_io_read_inputs(void)
{
    uint32_t jamma[4];

    if (!p4iodrv_read_jamma(p4io_ctx, jamma)) {
        return false;
    }

    uint32_t j0 = jamma[0];

    dm_pad_state = 0;
    dm_sys_state = 0;

    /* Pads are active low */

    if (bit_low(j0, JAMMA_BIT_LEFT_CYMBAL))
        dm_pad_state |= 1 << DM_IO_PAD_LEFT_CYMBAL;
    if (bit_low(j0, JAMMA_BIT_HIHAT))
        dm_pad_state |= 1 << DM_IO_PAD_HIHAT;
    if (bit_low(j0, JAMMA_BIT_LEFT_PEDAL))
        dm_pad_state |= 1 << DM_IO_PAD_LEFT_PEDAL;
    if (bit_low(j0, JAMMA_BIT_SNARE))
        dm_pad_state |= 1 << DM_IO_PAD_SNARE;
    if (bit_low(j0, JAMMA_BIT_HI_TOM))
        dm_pad_state |= 1 << DM_IO_PAD_HI_TOM;
    if (bit_low(j0, JAMMA_BIT_BASS_PEDAL))
        dm_pad_state |= 1 << DM_IO_PAD_BASS_PEDAL;
    if (bit_low(j0, JAMMA_BIT_LOW_TOM))
        dm_pad_state |= 1 << DM_IO_PAD_LOW_TOM;
    if (bit_low(j0, JAMMA_BIT_FLOOR_TOM))
        dm_pad_state |= 1 << DM_IO_PAD_FLOOR_TOM;
    if (bit_low(j0, JAMMA_BIT_RIGHT_CYMBAL))
        dm_pad_state |= 1 << DM_IO_PAD_RIGHT_CYMBAL;

    /* System buttons are active high */

    if (bit_high(j0, JAMMA_BIT_SERVICE))
        dm_sys_state |= 1 << DM_IO_SYS_SERVICE;
    if (bit_high(j0, JAMMA_BIT_TEST))
        dm_sys_state |= 1 << DM_IO_SYS_TEST;
    if (bit_high(j0, JAMMA_BIT_COIN))
        dm_sys_state |= 1 << DM_IO_SYS_COIN;
    if (bit_high(j0, JAMMA_BIT_START))
        dm_sys_state |= 1 << DM_IO_SYS_START;
    if (bit_high(j0, JAMMA_BIT_UP))
        dm_sys_state |= 1 << DM_IO_SYS_UP;
    if (bit_high(j0, JAMMA_BIT_DOWN))
        dm_sys_state |= 1 << DM_IO_SYS_DOWN;
    if (bit_high(j0, JAMMA_BIT_LEFT))
        dm_sys_state |= 1 << DM_IO_SYS_LEFT;
    if (bit_high(j0, JAMMA_BIT_RIGHT))
        dm_sys_state |= 1 << DM_IO_SYS_RIGHT;
    if (bit_high(j0, JAMMA_BIT_HELP))
        dm_sys_state |= 1 << DM_IO_SYS_HELP;
    if (bit_high(j0, JAMMA_BIT_EXTRA1))
        dm_sys_state |= 1 << DM_IO_SYS_EXTRA1;
    if (bit_high(j0, JAMMA_BIT_EXTRA2))
        dm_sys_state |= 1 << DM_IO_SYS_EXTRA2;

    return true;
}

uint16_t dm_io_get_pad_inputs(void)
{
    return dm_pad_state;
}

uint16_t dm_io_get_sys_inputs(void)
{
    return dm_sys_state;
}
