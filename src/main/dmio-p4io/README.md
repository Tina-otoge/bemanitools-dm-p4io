# dmio-p4io decomp notes

This document records reverse-engineering findings used to shape
`dmio-p4io.dll` behavior, plus a validation rerun.

## Scope

Binaries inspected (from `docs/game-dll`):

- `libextio.dll`
- `libdevice.dll`

Tools used:

- `rizin` (`afl`, `pdf`)
- `objdump` (earlier symbol/import export pass)

## Findings Used In Implementation

### 1) libdevice keeps cached input tables

Function: `device_get_input(int)`

Observed decomp pattern:

- index from `ecx`
- direct dword load from global table
- immediate return

Interpretation:

- game-facing reads are table-driven and cheap
- transient hardware read failures should not hard-fail every poll

Implementation impact in `dmio-p4io`:

- keep last stable states (`dm_pad_state`, `dm_sys_state`)
- tolerate short jamma read failure streaks before returning failure

### 2) libdevice SCI wrappers gate and sanitize reads

Functions:

- `devsci_read(int, unsigned char *, int)`
- `p4io_sci_gets(...)`

Observed pattern:

- checks guard flags/state
- returns `-1` on explicit guarded error paths
- has paths that return `0` instead of propagating hard error
- tracks an error counter in `p4io_sci_gets`

Interpretation:

- SCI is wrapped with defensive behavior
- callers are expected to poll repeatedly and handle soft failure

Implementation impact:

- `aciotest p4io analog*` modes continue polling and keep history/min-max,
  instead of treating any single read failure as fatal

### 3) libextio DM entrypoints are mixed digital/threshold style

Functions:

- `gfdm_unit_get_button_dm(enum DM_BUTTON_DEFINITION)`
- `gfdm_unit_get_input(int)`

Observed pattern:

- `gfdm_unit_get_button_dm`:
  - special handling for enum values `8` and `9`
  - otherwise indexed dword fetch from table
- `gfdm_unit_get_input`:
  - builds composite bitfield using bit-set operations
  - contains state/transition style logic and threshold checks

Interpretation:

- returned DM input is not just a raw one-frame JAMMA snapshot
- button and system semantics rely on stable, edge-friendly bits

Implementation impact:

- added per-bit debounce in `dmio-p4io` and default `dmio`
- edge-friendly hit behavior added in bridge tools:
  - `vigem-dmio`: short pulse extension for rising edges
  - `midi-dmio`: retrigger guard + fixed note length + velocity accenting
- `aciotest p4io analog2` now prints a libextio-like DM bitfield guess from
  current JAMMA bits to speed cabinet mapping work

## Validation Rerun

Rerun commands were executed after implementation updates.

### Command set

- `rizin -q -A -c "afl~gfdm_unit_get_button_dm;afl~gfdm_unit_get_input;pdf @ sym.libextio.dll_int___cdecl_gfdm_unit_get_button_dm_enum_DM_BUTTON_DEFINITION;q" docs/game-dll/libextio.dll`
- `rizin -q -A -c "afl~device_get_input;afl~device_get_jamma_history;pdf @ sym.libdevice.dll_unsigned_int___cdecl_device_get_input_int;q" docs/game-dll/libdevice.dll`
- `rizin -q -A -c "afl~devsci_read;afl~p4io_sci_gets;pdf @ sym.libdevice.dll_int___cdecl_devsci_read_int__unsigned_char_____ptr64__int;pdf @ sym.libdevice.dll_p4io_sci_gets;q" docs/game-dll/libdevice.dll`

### Compare: findings vs rerun

Matches:

- `device_get_input` still resolves to a direct indexed table load.
- `devsci_read` and `p4io_sci_gets` still show guarded paths with `-1` and
  soft-return behavior.
- `gfdm_unit_get_button_dm` still has special-case enum `8` and `9` plus table
  access for others.
- `gfdm_unit_get_input` symbol and size remain consistent with a larger
  composite logic routine.

Still unconfirmed (requires deeper lifting/live captures):

- exact one-to-one DM bit numbering for every pad/system control
- exact analog scaling/packing for hit intensity channels

## Practical Notes

- Mappings in `dmio-p4io/dmio.c` are still marked TODO where cabinet-specific
  confirmation is needed.
- Use `aciotest p4io analog2` on real hardware to lock final bit assignments.
