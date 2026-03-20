#ifndef ACIOTEST_P4IO_H
#define ACIOTEST_P4IO_H

/**
 * Run the P4IO JAMMA input monitor loop.
 *
 * Opens the P4IO USB device, polls JAMMA data continuously, and prints
 * all 128 bits with changed bits highlighted. Does not return until the
 * process is killed.
 */
void aciotest_p4io_run(void);

#endif
