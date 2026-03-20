#ifndef DMIO_P4IO_CONFIG_H
#define DMIO_P4IO_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "cconfig/cconfig.h"

#define DMIO_P4IO_PAD_COUNT 9
#define DMIO_P4IO_SYS_COUNT 11

struct dmio_p4io_config {
    int32_t pad_bits[DMIO_P4IO_PAD_COUNT];
    int32_t sys_bits[DMIO_P4IO_SYS_COUNT];
    int32_t pad_debounce_threshold;
    int32_t sys_debounce_threshold;
    int32_t max_read_fail_streak;
    bool pad_active_high;
    bool sys_active_high;
    bool log_jamma;
};

void dmio_p4io_config_init(struct cconfig *config);

void dmio_p4io_config_get(
    struct dmio_p4io_config *config_dmio_p4io, struct cconfig *config);

#endif
