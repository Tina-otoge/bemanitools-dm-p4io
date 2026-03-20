#ifndef ACIOTEST_P4IO_H
#define ACIOTEST_P4IO_H

/**
 * Run the P4IO JAMMA input monitor loop.
 *
 * Opens the P4IO USB device, polls JAMMA data continuously, and prints
 * all 128 bits with changed bits highlighted. Does not return until the
 * process is killed.
 */
void aciotest_p4io_run_digital(void);

/**
 * Run the P4IO analog discovery loop.
 *
 * Polls JAMMA plus SCI_UPDATE payloads to surface channels that may carry
 * analog drum hit amplitudes. Prints raw bytes and rolling min/max values.
 */
void aciotest_p4io_run_analog(void);

/**
 * Run a second P4IO analog discovery loop with spice-informed SCI probes.
 *
 * Sends multiple SCI_UPDATE request patterns and decodes candidate 16-bit
 * channels using a J32D-like 10-bit packing guess.
 */
void aciotest_p4io_run_analog2(void);

/**
 * Run guided JAMMA mapping capture for dmio-p4io.
 *
 * Asks the operator to hold each DM control one by one and emits ready-to-paste
 * #define JAMMA_BIT_* lines.
 */
void aciotest_p4io_run_map(bool save_to_file);

#endif
