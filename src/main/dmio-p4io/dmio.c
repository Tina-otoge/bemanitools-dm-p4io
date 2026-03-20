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

#include <stddef.h>
#include <string.h>

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
static uint8_t dm_pad_debounce[16];
static uint8_t dm_sys_debounce[16];
static uint8_t dm_read_fail_streak;

struct dm_bit_map {
    uint8_t out_bit;
    uint8_t jamma_bit;
    bool active_high;
};

static const struct dm_bit_map dm_pad_maps[] = {
    {DM_IO_PAD_LEFT_CYMBAL, JAMMA_BIT_LEFT_CYMBAL, false},
    {DM_IO_PAD_HIHAT, JAMMA_BIT_HIHAT, false},
    {DM_IO_PAD_LEFT_PEDAL, JAMMA_BIT_LEFT_PEDAL, false},
    {DM_IO_PAD_SNARE, JAMMA_BIT_SNARE, false},
    {DM_IO_PAD_HI_TOM, JAMMA_BIT_HI_TOM, false},
    {DM_IO_PAD_BASS_PEDAL, JAMMA_BIT_BASS_PEDAL, false},
    {DM_IO_PAD_LOW_TOM, JAMMA_BIT_LOW_TOM, false},
    {DM_IO_PAD_FLOOR_TOM, JAMMA_BIT_FLOOR_TOM, false},
    {DM_IO_PAD_RIGHT_CYMBAL, JAMMA_BIT_RIGHT_CYMBAL, false},
};

static const struct dm_bit_map dm_sys_maps[] = {
    {DM_IO_SYS_SERVICE, JAMMA_BIT_SERVICE, true},
    {DM_IO_SYS_TEST, JAMMA_BIT_TEST, true},
    {DM_IO_SYS_COIN, JAMMA_BIT_COIN, true},
    {DM_IO_SYS_START, JAMMA_BIT_START, true},
    {DM_IO_SYS_UP, JAMMA_BIT_UP, true},
    {DM_IO_SYS_DOWN, JAMMA_BIT_DOWN, true},
    {DM_IO_SYS_LEFT, JAMMA_BIT_LEFT, true},
    {DM_IO_SYS_RIGHT, JAMMA_BIT_RIGHT, true},
    {DM_IO_SYS_HELP, JAMMA_BIT_HELP, true},
    {DM_IO_SYS_EXTRA1, JAMMA_BIT_EXTRA1, true},
    {DM_IO_SYS_EXTRA2, JAMMA_BIT_EXTRA2, true},
};

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

    dm_pad_state = 0;
    dm_sys_state = 0;
    dm_read_fail_streak = 0;

    memset(dm_pad_debounce, 0, sizeof(dm_pad_debounce));
    memset(dm_sys_debounce, 0, sizeof(dm_sys_debounce));

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

static uint16_t
dm_map_raw(uint32_t jamma0, const struct dm_bit_map *map, size_t nmap)
{
    uint16_t out = 0;

    for (size_t i = 0; i < nmap; i++) {
        bool active = map[i].active_high ? bit_high(jamma0, map[i].jamma_bit) :
                                           bit_low(jamma0, map[i].jamma_bit);

        if (active) {
            out |= (uint16_t) (1U << map[i].out_bit);
        }
    }

    return out;
}

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

bool dm_io_read_inputs(void)
{
    uint32_t jamma[4];

    if (!p4iodrv_read_jamma(p4io_ctx, jamma)) {
        if (dm_read_fail_streak < 255) {
            dm_read_fail_streak++;
        }

        /* libdevice keeps cached state; do the same across short read hiccups
         */
        return dm_read_fail_streak <= 5;
    }

    dm_read_fail_streak = 0;

    uint32_t j0 = jamma[0];
    uint16_t raw_pad = dm_map_raw(
        j0, dm_pad_maps, sizeof(dm_pad_maps) / sizeof(dm_pad_maps[0]));
    uint16_t raw_sys = dm_map_raw(
        j0, dm_sys_maps, sizeof(dm_sys_maps) / sizeof(dm_sys_maps[0]));

    dm_pad_state =
        dm_debounce_mask(dm_pad_state, raw_pad, dm_pad_debounce, 2, 9);
    dm_sys_state =
        dm_debounce_mask(dm_sys_state, raw_sys, dm_sys_debounce, 2, 11);

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
