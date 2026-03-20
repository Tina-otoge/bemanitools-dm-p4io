# AGENT CONTEXT (dmio-p4io)

This file is a high-signal handoff for future AI agents working on the
DrumMania + P4IO effort in this repository.

Last updated: 2026-03-20

---

## 1) What This Workstream Is

Primary goal:

- Build a reliable DrumMania IO stack around a swappable `dmio.dll` interface,
  with one implementation backed by real P4IO hardware.

Secondary goals:

- Provide practical test/bridge tools (`aciotest`, `midi-dmio`, `vigem-dmio`)
  to iterate without full game runtime.
- Reverse-engineer communication and input shaping logic from game DLLs to
  improve emulation behavior.

---

## 2) Current Architecture (Important)

### Core API

- `src/main/bemanitools/dmio.h`
  - Defines pad/system bit enums and provider API.

### Provider DLLs

- `src/main/dmio/`:
  - default provider (geninput-based, no real P4IO required)
- `src/main/dmio-p4io/`:
  - real hardware provider
  - intended deployment: copy/rename to `dmio.dll` beside bridge executables

### Consumer tools

- `src/main/vigem-dmio/`:
  - exposes DM hits as virtual Xbox 360 controller
- `src/main/midi-dmio/`:
  - emits MIDI note events from DM hits

### Discovery / diagnostics

- `src/main/aciotest/`
  - `p4io` digital monitor
  - `p4io analog` and `p4io analog2` SCI probing
  - `p4io map` and `p4io map save` guided mapping capture

---

## 3) Files Most Relevant To Continue Work

- `src/main/dmio-p4io/dmio.c`
  - JAMMA bit mapping constants (`JAMMA_BIT_*`)
  - bit polarity logic
  - debounce and read-failure handling

- `src/main/aciotest/p4io.c`
  - digital bit monitor
  - analog SCI probes
  - decomp-informed DM bitfield guess
  - guided mapping capture and optional save output

- `src/main/aciotest/main.c`
  - CLI dispatch for p4io submodes

- `src/main/midi-dmio/main.c`
  - hit-based MIDI output strategy

- `src/main/vigem-dmio/main.c`
  - edge-pulse expansion strategy for gamepad output

- `src/main/dmio-p4io/README.md`
  - decomp notes and symbol/address appendix

- `src/main/dmio-p4io/TODO_ON_REAL_HARDWARE.md`
  - real-cab test protocol and artifact checklist

---

## 4) Verified Reverse-Engineering Findings

### High confidence

1. `libdevice.dll` is the low-level P4IO transport owner
   - exports `devsci_*`, `p4io_sci_*`, and JAMMA-facing helpers.

2. `libcardunit.dll` and `libledunit.dll` are SCI clients
   - they import and call `p4io_sci_*` from `libdevice.dll`.
   - they use framed/incremental read loops with error checks/clear.

3. `libsystem.dll` orchestrates input layers
   - initializes JAMMA async mode.
   - update path merges `device_*` (JAMMA side) and `gfdm_unit_*` (extio side).

4. `libextio.dll` DM input path is composite
   - `gfdm_unit_get_input` constructs bitfields with state/threshold behavior,
     not just raw one-frame line reads.

### Practical implication

- Input consumers expect stable and edge-friendly semantics.
- Pure raw mapping without temporal shaping will behave poorly in fast play.

---

## 5) Current Behavioral Choices In Code

### dmio-p4io

- Per-bit mapping tables for pads/system controls.
- Active low for pad lines, active high for system lines.
- Small per-bit debounce for both pad and system masks.
- Short read-failure tolerance (keep last stable state briefly).

### default dmio (geninput)

- Same debounce strategy for parity with dmio-p4io behavior.

### midi-dmio

- Rising-edge hit semantics.
- Retrigger guard for chatter control.
- Fixed note length note-off scheduling.
- Optional accent velocity for close successive hits.

### vigem-dmio

- Short pulse extension on rising edges to avoid hit loss.

---

## 6) Build / Mount Workflow (Critical In This Workspace)

Known required sequence before and after container builds:

1. unmount game-dll mirror
   - `umount docs/game-dll || true`

2. build
   - `podman run -v "$PWD":/bemanitools:z --entrypoint make localhost/bemanitools-build:latest dm-tools -j4`

3. remount
   - `fuse-overlayfs -o lowerdir=/mnt/ladys-grace/Games/Arcade/GITADORA/M32-002-2025021201/contents/modules /home/tina/Projects/p4io-dm-midi@tina/docs/game-dll && echo "Mounted OK"`

If this order is not followed, analysis/build context can drift.

---

## 7) rz-ghidra Environment Notes

Problem encountered:

- `pdg` failed with: `No sleigh specification for x86:LE:64:default`

Fix:

- set ghidra sleigh home explicitly in rizin command:
  - `e ghidra.sleighhome=/home/tina/.local/lib64/rizin/plugins/rz_ghidra_sleigh`

Example:

- `rizin -q -A -c "e ghidra.sleighhome=/home/tina/.local/lib64/rizin/plugins/rz_ghidra_sleigh;s sym.libdevice.dll_p4io_sci_gets;pdg;q" docs/game-dll/libdevice.dll`

Caveat:

- ghidra pseudocode has frequent overlap/type warnings on these binaries.
- treat it as control-flow guidance; cross-check with `pdf` and import/xref data.

---

## 8) Known Pitfalls / Gotchas

1. Header order regression risk in `midi-dmio/main.c`
   - keep `#include <windows.h>` before `#include <mmsystem.h>`.

2. Mapping constants in `dmio-p4io/dmio.c` are still partly tentative
   - they should be validated from real-cabinet capture.

3. Aciotest `p4io map` output can still produce unresolved lines
   - rerun and compare multiple sessions before committing final mapping.

4. Some DLL files in docs are not parseable by objdump/rizin
   - e.g. copy files or non-PE blobs.
   - do not assume every file is a valid analysis target.

5. Do not over-trust single-run SCI observations
   - noise and timing can create false channel candidates.

---

## 9) Real Hardware Plan (Use This First)

Follow:

- `src/main/dmio-p4io/TODO_ON_REAL_HARDWARE.md`

Minimum useful capture set:

1. `aciotest.exe p4io map save` (at least 2 runs)
2. `aciotest.exe p4io analog2` with low/medium/high strikes
3. quick validation pass in `midi-dmio.exe` and `vigem-dmio.exe`

Data to preserve:

- generated define files
- session notes (cab type, game build, anomalies)
- final control -> bit -> polarity table with confidence
- analog candidate channels and confidence

---

## 10) Next Priority Tasks For New Agent

### Priority A: lock digital mapping

1. run hardware `p4io map save` multiple times
2. update `JAMMA_BIT_*` in `src/main/dmio-p4io/dmio.c`
3. rebuild and validate all DM tools

### Priority B: reduce false positives

1. tune debounce thresholds per line class if needed
2. verify fast-roll behavior in midi/vigem bridges

### Priority C: analog confidence improvements

1. correlate analog2 candidate channels with controlled hit force tests
2. refine decode/print heuristics in `aciotest/p4io.c`
3. document confirmed channel semantics in README

### Priority D: richer reverse validation

1. build a compact call graph for
   - `libsystem` input update chain
   - `libcardunit` SCI framing path
   - `libledunit` SCI framing path
2. append addresses to README when confirming new claims

---

## 11) Suggested Commit Hygiene

When updating mapping constants or behavior:

1. include capture evidence references in commit message
2. keep mapping-only commits separate from algorithmic changes
3. run `dm-tools` build before finalizing

---

## 12) Quick Command Snippets

### Build

- `umount docs/game-dll || true`
- `podman run -v "$PWD":/bemanitools:z --entrypoint make localhost/bemanitools-build:latest dm-tools -j4`
- `fuse-overlayfs -o lowerdir=/mnt/ladys-grace/Games/Arcade/GITADORA/M32-002-2025021201/contents/modules /home/tina/Projects/p4io-dm-midi@tina/docs/game-dll && echo "Mounted OK"`

### Aciotest modes

- `build/bin/indep-32/aciotest.exe p4io`
- `build/bin/indep-32/aciotest.exe p4io map`
- `build/bin/indep-32/aciotest.exe p4io map save`
- `build/bin/indep-32/aciotest.exe p4io analog`
- `build/bin/indep-32/aciotest.exe p4io analog2`

### rz-ghidra examples

- `rizin -q -A -c "e ghidra.sleighhome=/home/tina/.local/lib64/rizin/plugins/rz_ghidra_sleigh;s 0x1800043c0;pdg;q" docs/game-dll/libdevice.dll`
- `rizin -q -A -c "axt @ 0x180013318;q" docs/game-dll/libcardunit.dll`
- `rizin -q -A -c "s 0x180003c90;pdg;q" docs/game-dll/libsystem.dll`

---

## 13) Confidence Snapshot

- Transport ownership (`libdevice`) : high confidence
- libsystem orchestration role      : high confidence
- DM bitfield shaping expectations  : high confidence
- exact cabinet line mapping        : medium until real-cab confirmation
- analog intensity channel mapping  : low/medium until force-correlated traces
