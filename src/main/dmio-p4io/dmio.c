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

#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "bemanitools/dmio.h"

#include "cconfig/cconfig-main.h"

#include "dmio-p4io/config.h"

#include "p4iodrv/device.h"
#include "util/log.h"

static struct p4iodrv_ctx *p4io_ctx;
static atomic_bool running;
static atomic_bool processing_io;
static uint16_t dm_pad_state;
static uint16_t dm_sys_state;
static uint8_t dm_pad_debounce[16];
static uint8_t dm_sys_debounce[16];
static uint8_t dm_read_fail_streak;
static uint8_t dm_pad_debounce_threshold;
static uint8_t dm_sys_debounce_threshold;
static uint8_t dm_max_read_fail_streak;
static bool dm_log_jamma;

struct dm_bit_map {
    uint8_t out_bit;
    uint8_t jamma_bit;
    bool active_high;
};

static struct dm_bit_map dm_pad_maps[] = {
    {DM_IO_PAD_LEFT_CYMBAL, 0, false},
    {DM_IO_PAD_HIHAT, 0, false},
    {DM_IO_PAD_LEFT_PEDAL, 0, false},
    {DM_IO_PAD_SNARE, 0, false},
    {DM_IO_PAD_HI_TOM, 0, false},
    {DM_IO_PAD_BASS_PEDAL, 0, false},
    {DM_IO_PAD_LOW_TOM, 0, false},
    {DM_IO_PAD_FLOOR_TOM, 0, false},
    {DM_IO_PAD_RIGHT_CYMBAL, 0, false},
};

static struct dm_bit_map dm_sys_maps[] = {
    {DM_IO_SYS_SERVICE, 0, true},
    {DM_IO_SYS_TEST, 0, true},
    {DM_IO_SYS_COIN, 0, true},
    {DM_IO_SYS_START, 0, true},
    {DM_IO_SYS_UP, 0, true},
    {DM_IO_SYS_DOWN, 0, true},
    {DM_IO_SYS_LEFT, 0, true},
    {DM_IO_SYS_RIGHT, 0, true},
    {DM_IO_SYS_HELP, 0, true},
    {DM_IO_SYS_EXTRA1, 0, true},
    {DM_IO_SYS_EXTRA2, 0, true},
};

static void dm_apply_config_maps(const struct dmio_p4io_config *config)
{
    for (size_t i = 0; i < DMIO_P4IO_PAD_COUNT; i++) {
        dm_pad_maps[i].jamma_bit = (uint8_t) config->pad_bits[i];
        dm_pad_maps[i].active_high = config->pad_active_high;
    }

    for (size_t i = 0; i < DMIO_P4IO_SYS_COUNT; i++) {
        dm_sys_maps[i].jamma_bit = (uint8_t) config->sys_bits[i];
        dm_sys_maps[i].active_high = config->sys_active_high;
    }
}

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
    struct cconfig *config;
    struct dmio_p4io_config config_dmio_p4io;

    config = cconfig_init();

    dmio_p4io_config_init(config);

    if (!cconfig_main_config_init(
            config,
            "--dmio-p4io-config",
            "dmio-p4io.conf",
            "--help",
            "-h",
            "dmio-p4io",
            CCONFIG_CMD_USAGE_OUT_STDOUT)) {
        cconfig_finit(config);
        exit(EXIT_FAILURE);
    }

    dmio_p4io_config_get(&config_dmio_p4io, config);

    cconfig_finit(config);

    dm_apply_config_maps(&config_dmio_p4io);
    dm_pad_debounce_threshold =
        (uint8_t) config_dmio_p4io.pad_debounce_threshold;
    dm_sys_debounce_threshold =
        (uint8_t) config_dmio_p4io.sys_debounce_threshold;
    dm_max_read_fail_streak = (uint8_t) config_dmio_p4io.max_read_fail_streak;
    dm_log_jamma = config_dmio_p4io.log_jamma;

    p4io_ctx = p4iodrv_open();

    if (!p4io_ctx) {
        log_warning("Failed to open P4IO");
        return false;
    }

    dm_pad_state = 0;
    dm_sys_state = 0;
    dm_read_fail_streak = 0;
    running = true;
    processing_io = false;

    memset(dm_pad_debounce, 0, sizeof(dm_pad_debounce));
    memset(dm_sys_debounce, 0, sizeof(dm_sys_debounce));

    return true;
}

void dm_io_fini(void)
{
    running = false;

    while (processing_io) {
        Sleep(1);
    }

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

    if (!running) {
        return false;
    }

    processing_io = true;

    if (!p4iodrv_read_jamma(p4io_ctx, jamma)) {
        if (dm_read_fail_streak < 255) {
            dm_read_fail_streak++;
        }

        /* libdevice keeps cached state; do the same across short read hiccups
         */
        processing_io = false;
        return dm_read_fail_streak <= dm_max_read_fail_streak;
    }

    dm_read_fail_streak = 0;

    uint32_t j0 = jamma[0];

    if (dm_log_jamma) {
        log_info("jamma[0]=0x%08X", j0);
    }

    uint16_t raw_pad = dm_map_raw(
        j0, dm_pad_maps, sizeof(dm_pad_maps) / sizeof(dm_pad_maps[0]));
    uint16_t raw_sys = dm_map_raw(
        j0, dm_sys_maps, sizeof(dm_sys_maps) / sizeof(dm_sys_maps[0]));

    dm_pad_state = dm_debounce_mask(
        dm_pad_state,
        raw_pad,
        dm_pad_debounce,
        dm_pad_debounce_threshold,
        DMIO_P4IO_PAD_COUNT);
    dm_sys_state = dm_debounce_mask(
        dm_sys_state,
        raw_sys,
        dm_sys_debounce,
        dm_sys_debounce_threshold,
        DMIO_P4IO_SYS_COUNT);

    processing_io = false;

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
