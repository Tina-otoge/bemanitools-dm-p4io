#include "cconfig/cconfig-util.h"

#include <stdio.h>

#include "util/log.h"

#include "config.h"

#define DMIO_P4IO_CONFIG_PAD_DEBOUNCE_KEY "dmio.p4io.pad.debounce_threshold"
#define DMIO_P4IO_CONFIG_SYS_DEBOUNCE_KEY "dmio.p4io.sys.debounce_threshold"
#define DMIO_P4IO_CONFIG_MAX_FAIL_KEY "dmio.p4io.max_read_fail_streak"
#define DMIO_P4IO_CONFIG_PAD_ACTIVE_HIGH_KEY "dmio.p4io.pad.active_high"
#define DMIO_P4IO_CONFIG_SYS_ACTIVE_HIGH_KEY "dmio.p4io.sys.active_high"
#define DMIO_P4IO_CONFIG_LOG_JAMMA_KEY "dmio.p4io.log_jamma"

#define DMIO_P4IO_CONFIG_PAD_DEBOUNCE_DEFAULT 2
#define DMIO_P4IO_CONFIG_SYS_DEBOUNCE_DEFAULT 2
#define DMIO_P4IO_CONFIG_MAX_FAIL_DEFAULT 5
#define DMIO_P4IO_CONFIG_PAD_ACTIVE_HIGH_DEFAULT false
#define DMIO_P4IO_CONFIG_SYS_ACTIVE_HIGH_DEFAULT true
#define DMIO_P4IO_CONFIG_LOG_JAMMA_DEFAULT false

static const int32_t dmio_p4io_pad_defaults[DMIO_P4IO_PAD_COUNT] = {
    0,
    1,
    8,
    2,
    3,
    7,
    4,
    5,
    6,
};

static const int32_t dmio_p4io_sys_defaults[DMIO_P4IO_SYS_COUNT] = {
    25,
    24,
    28,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
};

static const char *dmio_p4io_pad_keys[DMIO_P4IO_PAD_COUNT] = {
    "dmio.p4io.pad.left_cymbal_bit",
    "dmio.p4io.pad.hihat_bit",
    "dmio.p4io.pad.left_pedal_bit",
    "dmio.p4io.pad.snare_bit",
    "dmio.p4io.pad.hi_tom_bit",
    "dmio.p4io.pad.bass_pedal_bit",
    "dmio.p4io.pad.low_tom_bit",
    "dmio.p4io.pad.floor_tom_bit",
    "dmio.p4io.pad.right_cymbal_bit",
};

static const char *dmio_p4io_sys_keys[DMIO_P4IO_SYS_COUNT] = {
    "dmio.p4io.sys.service_bit",
    "dmio.p4io.sys.test_bit",
    "dmio.p4io.sys.coin_bit",
    "dmio.p4io.sys.start_bit",
    "dmio.p4io.sys.up_bit",
    "dmio.p4io.sys.down_bit",
    "dmio.p4io.sys.left_bit",
    "dmio.p4io.sys.right_bit",
    "dmio.p4io.sys.help_bit",
    "dmio.p4io.sys.extra1_bit",
    "dmio.p4io.sys.extra2_bit",
};

static void dmio_p4io_set_int_with_desc(
    struct cconfig *config,
    const char *key,
    int32_t value,
    const char *prefix,
    int32_t min_value,
    int32_t max_value)
{
    char desc[192];

    snprintf(
        desc, sizeof(desc), "%s (range: %d-%d)", prefix, min_value, max_value);

    desc[sizeof(desc) - 1] = '\0';

    cconfig_util_set_int(config, key, value, desc);
}

static int32_t dmio_p4io_get_int_with_bounds(
    struct cconfig *config,
    const char *key,
    int32_t default_value,
    int32_t min_value,
    int32_t max_value)
{
    int32_t value;

    if (!cconfig_util_get_int(config, key, &value, default_value)) {
        log_warning(
            "Invalid value for key '%s', fallback to default '%d'",
            key,
            default_value);

        return default_value;
    }

    if (value < min_value || value > max_value) {
        log_warning(
            "Out of range value '%d' for key '%s', fallback to default '%d'",
            value,
            key,
            default_value);

        return default_value;
    }

    return value;
}

void dmio_p4io_config_init(struct cconfig *config)
{
    for (int32_t i = 0; i < DMIO_P4IO_PAD_COUNT; i++) {
        dmio_p4io_set_int_with_desc(
            config,
            dmio_p4io_pad_keys[i],
            dmio_p4io_pad_defaults[i],
            "JAMMA bit index for this drum pad",
            0,
            31);
    }

    for (int32_t i = 0; i < DMIO_P4IO_SYS_COUNT; i++) {
        dmio_p4io_set_int_with_desc(
            config,
            dmio_p4io_sys_keys[i],
            dmio_p4io_sys_defaults[i],
            "JAMMA bit index for this system button",
            0,
            31);
    }

    dmio_p4io_set_int_with_desc(
        config,
        DMIO_P4IO_CONFIG_PAD_DEBOUNCE_KEY,
        DMIO_P4IO_CONFIG_PAD_DEBOUNCE_DEFAULT,
        "Consecutive polls required before committing a pad state change",
        1,
        10);

    dmio_p4io_set_int_with_desc(
        config,
        DMIO_P4IO_CONFIG_SYS_DEBOUNCE_KEY,
        DMIO_P4IO_CONFIG_SYS_DEBOUNCE_DEFAULT,
        "Consecutive polls required before committing a system state change",
        1,
        10);

    dmio_p4io_set_int_with_desc(
        config,
        DMIO_P4IO_CONFIG_MAX_FAIL_KEY,
        DMIO_P4IO_CONFIG_MAX_FAIL_DEFAULT,
        "Allowed consecutive JAMMA read failures before dm_io_read_inputs "
        "fails",
        0,
        255);

    cconfig_util_set_bool(
        config,
        DMIO_P4IO_CONFIG_PAD_ACTIVE_HIGH_KEY,
        DMIO_P4IO_CONFIG_PAD_ACTIVE_HIGH_DEFAULT,
        "Interpret pad bits as active-high (default false for P4IO drums)");

    cconfig_util_set_bool(
        config,
        DMIO_P4IO_CONFIG_SYS_ACTIVE_HIGH_KEY,
        DMIO_P4IO_CONFIG_SYS_ACTIVE_HIGH_DEFAULT,
        "Interpret system bits as active-high");

    cconfig_util_set_bool(
        config,
        DMIO_P4IO_CONFIG_LOG_JAMMA_KEY,
        DMIO_P4IO_CONFIG_LOG_JAMMA_DEFAULT,
        "Log raw jamma[0] each poll for mapping/debug sessions");
}

void dmio_p4io_config_get(
    struct dmio_p4io_config *config_dmio_p4io, struct cconfig *config)
{
    for (int32_t i = 0; i < DMIO_P4IO_PAD_COUNT; i++) {
        config_dmio_p4io->pad_bits[i] = dmio_p4io_get_int_with_bounds(
            config, dmio_p4io_pad_keys[i], dmio_p4io_pad_defaults[i], 0, 31);
    }

    for (int32_t i = 0; i < DMIO_P4IO_SYS_COUNT; i++) {
        config_dmio_p4io->sys_bits[i] = dmio_p4io_get_int_with_bounds(
            config, dmio_p4io_sys_keys[i], dmio_p4io_sys_defaults[i], 0, 31);
    }

    config_dmio_p4io->pad_debounce_threshold = dmio_p4io_get_int_with_bounds(
        config,
        DMIO_P4IO_CONFIG_PAD_DEBOUNCE_KEY,
        DMIO_P4IO_CONFIG_PAD_DEBOUNCE_DEFAULT,
        1,
        10);

    config_dmio_p4io->sys_debounce_threshold = dmio_p4io_get_int_with_bounds(
        config,
        DMIO_P4IO_CONFIG_SYS_DEBOUNCE_KEY,
        DMIO_P4IO_CONFIG_SYS_DEBOUNCE_DEFAULT,
        1,
        10);

    config_dmio_p4io->max_read_fail_streak = dmio_p4io_get_int_with_bounds(
        config,
        DMIO_P4IO_CONFIG_MAX_FAIL_KEY,
        DMIO_P4IO_CONFIG_MAX_FAIL_DEFAULT,
        0,
        255);

    if (!cconfig_util_get_bool(
            config,
            DMIO_P4IO_CONFIG_PAD_ACTIVE_HIGH_KEY,
            &config_dmio_p4io->pad_active_high,
            DMIO_P4IO_CONFIG_PAD_ACTIVE_HIGH_DEFAULT)) {
        log_warning(
            "Invalid value for key '%s', fallback to default '%d'",
            DMIO_P4IO_CONFIG_PAD_ACTIVE_HIGH_KEY,
            DMIO_P4IO_CONFIG_PAD_ACTIVE_HIGH_DEFAULT);
    }

    if (!cconfig_util_get_bool(
            config,
            DMIO_P4IO_CONFIG_SYS_ACTIVE_HIGH_KEY,
            &config_dmio_p4io->sys_active_high,
            DMIO_P4IO_CONFIG_SYS_ACTIVE_HIGH_DEFAULT)) {
        log_warning(
            "Invalid value for key '%s', fallback to default '%d'",
            DMIO_P4IO_CONFIG_SYS_ACTIVE_HIGH_KEY,
            DMIO_P4IO_CONFIG_SYS_ACTIVE_HIGH_DEFAULT);
    }

    if (!cconfig_util_get_bool(
            config,
            DMIO_P4IO_CONFIG_LOG_JAMMA_KEY,
            &config_dmio_p4io->log_jamma,
            DMIO_P4IO_CONFIG_LOG_JAMMA_DEFAULT)) {
        log_warning(
            "Invalid value for key '%s', fallback to default '%d'",
            DMIO_P4IO_CONFIG_LOG_JAMMA_KEY,
            DMIO_P4IO_CONFIG_LOG_JAMMA_DEFAULT);
    }
}
