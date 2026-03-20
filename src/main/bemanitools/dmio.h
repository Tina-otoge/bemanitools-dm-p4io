#ifndef BEMANITOOLS_DMIO_H
#define BEMANITOOLS_DMIO_H

/* IO emulation provider for DrumMania. */

#include <stdbool.h>
#include <stdint.h>

#include "bemanitools/glue.h"

/* Bit positions for drum pad inputs returned by dm_io_get_pad_inputs().
   Order matches the default gitadora layout. */

enum dm_io_pad_bit {
    DM_IO_PAD_LEFT_CYMBAL = 0x00,
    DM_IO_PAD_HIHAT = 0x01,
    DM_IO_PAD_LEFT_PEDAL = 0x02, /* Hi-Hat foot pedal */
    DM_IO_PAD_SNARE = 0x03,
    DM_IO_PAD_HI_TOM = 0x04,
    DM_IO_PAD_BASS_PEDAL = 0x05,
    DM_IO_PAD_LOW_TOM = 0x06,
    DM_IO_PAD_FLOOR_TOM = 0x07,
    DM_IO_PAD_RIGHT_CYMBAL = 0x08,
};

/* Bit positions for system inputs returned by dm_io_get_sys_inputs(). */

enum dm_io_sys_bit {
    DM_IO_SYS_SERVICE = 0x00,
    DM_IO_SYS_TEST = 0x01,
    DM_IO_SYS_COIN = 0x02,
    DM_IO_SYS_START = 0x03,
    DM_IO_SYS_UP = 0x04,
    DM_IO_SYS_DOWN = 0x05,
    DM_IO_SYS_LEFT = 0x06,
    DM_IO_SYS_RIGHT = 0x07,
    DM_IO_SYS_HELP = 0x08,
    DM_IO_SYS_EXTRA1 = 0x09,
    DM_IO_SYS_EXTRA2 = 0x0A,
};

/* The first function that will be called on your DLL. You will be supplied
   with four function pointers that may be used to log messages to the
   game's log file. See comments in glue.h for further information. */

void dm_io_set_loggers(
    log_formatter_t misc,
    log_formatter_t info,
    log_formatter_t warning,
    log_formatter_t fatal);

/* Initialize your DM IO emulation DLL. Thread management functions are
   provided to you; you must use these functions to create your own threads
   if you want to make use of the logging functions provided to
   dm_io_set_loggers(). See glue.h for further details. */

bool dm_io_init(
    thread_create_t thread_create,
    thread_join_t thread_join,
    thread_destroy_t thread_destroy);

/* Shut down your DM IO emulation DLL */

void dm_io_fini(void);

/* Read input state from hardware into internal buffers.
   Call dm_io_get_pad_inputs() and dm_io_get_sys_inputs() afterwards. */

bool dm_io_read_inputs(void);

/* Get current drum pad state. Each set bit corresponds to a pressed pad.
   Bit positions are defined in enum dm_io_pad_bit. */

uint16_t dm_io_get_pad_inputs(void);

/* Get current system button state. Each set bit corresponds to a pressed
   button. Bit positions are defined in enum dm_io_sys_bit. */

uint16_t dm_io_get_sys_inputs(void);

#endif
