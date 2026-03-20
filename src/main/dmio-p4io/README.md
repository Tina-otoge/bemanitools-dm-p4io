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

## All-DLL rz-ghidra Sweep (New)

A broader sweep was run over all 45 DLL files in `docs/game-dll`.

### Setup note

`pdg` needed an explicit sleigh path in this environment:

- `e ghidra.sleighhome=/home/tina/.local/lib64/rizin/plugins/rz_ghidra_sleigh`

Without this, ghidra decompilation failed with:

- `No sleigh specification for x86:LE:64:default`

### New communication-path findings

1. P4IO transport ownership is centralized in `libdevice.dll`.

- `libdevice.dll` exports the full SCI surface:
  - `p4io_sci_boot`, `p4io_sci_setparam`, `p4io_sci_puts`, `p4io_sci_gets`,
    `p4io_sci_get_error`, `p4io_sci_clear_error`, `p4io_sci_flush`,
    `p4io_sci_flush_complete`, `p4io_sci_close`
- It also exports `devsci_*` and JAMMA helpers.

2. `libcardunit.dll` and `libledunit.dll` are SCI clients using `libdevice`.

- Both import `p4io_sci_*` from `libdevice.dll` and repeatedly call
  `p4io_sci_gets`.
- In ghidra output, both have very similar reader loops:
  - incremental `p4io_sci_gets` reads into rolling buffers
  - checks through `p4io_sci_get_error`
  - recovery through `p4io_sci_clear_error`
  - local error counters and retry behavior

3. `libsystem.dll` is the runtime input orchestrator.

- `sys_input_initialize` calls `device_set_jamma_asyncmode` early.
- `sys_input_update` combines:
  - `device_update` + `device_get_input` (JAMMA side)
  - `gfdm_unit_get_input` / `gfdm_unit2_get_input` (extio unit side)

This confirms the game merges multiple input layers instead of consuming only a
single raw JAMMA snapshot.

4. New SCI config clue from `p4io_sci_setparam` decompilation.

- `p4io_sci_setparam` contains explicit rate mapping branches for values:
  - `0x2580`, `0x4b00`, `0x9600`, `0xe100`, `0x1c200`

These are strong hints for serial parameter setup values expected by the SCI
path.

5. `p4io_sci_flush_complete` appears as a compatibility no-op in this build.

- ghidra pseudocode shows an immediate success return.

### Sweep coverage result

- No second independent low-level P4IO transport owner was found outside
  `libdevice.dll`.
- Additional P4IO hits in other DLLs were import-level usage or higher-level
  logic wrappers.

### Quality caveat

rz-ghidra pseudocode for some functions includes overlap/type warnings.
Use it primarily for control-flow and call-graph direction, and validate exact
structure with disassembly or runtime traces.

## Symbol/Address Appendix

Quick offsets used for the findings above.

### libdevice.dll

- `device_get_input`: `0x1800031b0`
- `device_get_jamma_history`: `0x180003820`
- `devsci_open`: `0x180003ac0`
- `devsci_read`: `0x180003ba0`
- `devsci_write`: `0x180003be0`
- `p4io_sci_boot`: `0x180004170`
- `p4io_sci_setparam`: `0x180004250`
- `p4io_sci_puts`: `0x180004300`
- `p4io_sci_gets`: `0x1800043c0`
- `p4io_sci_clear_error`: `0x180004460`
- `p4io_sci_get_error`: `0x180004480`
- `p4io_sci_flush_complete`: `0x1800021d0`

### libextio.dll

- `gfdm_unit_get_button_dm`: `0x1800018d0`
- `gfdm_unit_get_input`: `0x180001900`

### libgfdm_unit2.dll

- `gfdm_unit2_get_input`: `0x1800017a0`

### libsystem.dll

- `sys_input_initialize`: `0x180003bd0`
- `sys_input_update`: `0x180003c90`
- import thunk `device_set_jamma_asyncmode`: `0x1800103b8`
- import thunk `gfdm_unit_get_input`: `0x1800103e8`

### libcardunit.dll

- import thunk `p4io_sci_gets`: `0x180013318`
- representative SCI caller: `fcn.180004be0`
- representative framed read path: `fcn.180005370`

### libledunit.dll

- import thunk `p4io_sci_gets`: `0x180011310`
- representative SCI caller: `fcn.180002a80`
- representative framed read path: `fcn.180003210`

### Notes

- Addresses are image VAs as reported by rizin for this binary set.
- For fast re-check in rizin:
  - `s 0x1800043c0; pdf`
  - `s 0x180003c90; pdg`
  - `axt @ 0x180013318`

## Practical Notes

- Mappings in `dmio-p4io/dmio.c` are still marked TODO where cabinet-specific
  confirmation is needed.
- Use `aciotest p4io analog2` on real hardware to lock final bit assignments.
