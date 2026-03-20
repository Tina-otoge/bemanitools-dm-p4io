# TODO On Real Hardware

Goal: use aciotest on a real P4IO cabinet to extract trustworthy mappings and analog clues, then fold them back into dmio-p4io.

## Driver Migration (Official Windows -> Custom Windows)

Use this when your custom Windows install has edrum software but not the game.

Known context for this project:

1. Source machine: Windows 7 Embedded (official game install).
2. Target machine: Windows 10 IoT (custom install).
3. Win7 Embedded images may have older/limited `pnputil` features and reduced GUI tooling.

Important:

1. Only move drivers between systems you own/control and where licensing permits.
2. Keep source and target OS architecture aligned (x64 to x64).
3. Prefer exporting/importing the full driver package (INF/CAT/SYS), not copying random files.

### A) Export the currently installed P4IO driver package on the official install (Win7 Embedded)

1. Connect P4IO and verify it appears in Device Manager.
2. Open an elevated Command Prompt.
3. Try modern `pnputil` flow first:

	- `pnputil /enum-devices /connected`
	- `pnputil /enum-drivers`
	- `mkdir C:\p4io-driver-export`
	- `pnputil /export-driver oem42.inf C:\p4io-driver-export`

4. If `/export-driver` is unavailable (common on Win7 Embedded), use legacy flow:

	- `pnputil -e > C:\p4io-drivers.txt`
	- find likely `oemXX.inf` entries in `C:\p4io-drivers.txt`
	- verify candidates by searching for P4IO strings:
		- `findstr /i /c:"p4io" C:\Windows\inf\oem*.inf`

5. Once you have the right `oemXX.inf`, collect package files:

	- copy `C:\Windows\inf\oemXX.inf`
	- copy matching catalog from `C:\Windows\System32\CatRoot\...` if referenced
	- copy the corresponding DriverStore folder from:
		- `C:\Windows\System32\DriverStore\FileRepository\...`

6. To locate the exact DriverStore folder for `oemXX.inf`, inspect:

	- `C:\Windows\inf\setupapi.dev.log`
	- search for `oemXX.inf` and note `Driver Store Path`

7. Optional full backup of all 3rd-party drivers (safety net):

	- `pnputil /export-driver * C:\driver-export-all`
	- if unsupported, keep a full copy of `C:\Windows\inf` + relevant `FileRepository` subfolders

Expected result:

1. A folder containing the P4IO INF/CAT/SYS package under `C:\p4io-driver-export`.

### B) Install that package on the custom Windows install (Win10 IoT)

1. Copy `C:\p4io-driver-export` to the custom install.
2. Plug in P4IO.
3. Open elevated Command Prompt.
4. Install/add the exported INF package(s) (modern command):

	- `pnputil /add-driver C:\p4io-driver-export\*.inf /install`

5. Legacy-compatible fallback if needed:

	- `pnputil -a C:\p4io-driver-export\oemXX.inf`
	- `pnputil -i -a C:\p4io-driver-export\oemXX.inf`

6. If multiple INF files exist, install all from the export folder.

Expected result:

1. `pnputil` reports the package was added and device was updated/installed.

### C) Verify from user space before running aciotest

1. Check that the device is present:

	- `pnputil /enum-devices /connected`

2. Confirm the driver package is installed:

	- `pnputil /enum-drivers`

3. Run:

	- `aciotest.exe p4io`

4. If `aciotest` prints P4IO discovery and JAMMA state, migration is successful.

### D) Troubleshooting

1. Driver install blocked by signature policy:
	- verify CAT file exists in export.
	- verify target clock/date is sane.
	- if needed, test with signature enforcement policy appropriate for your environment.
2. Device not picked up:
	- unplug/replug P4IO.
	- remove stale device entry in Device Manager and run install again.
	- reboot after install.
3. Multiple candidate drivers:
	- uninstall wrong candidate package (`pnputil /delete-driver oemXX.inf /uninstall`) and reinstall intended package.
4. Win7 Embedded missing commands/UI:
	- use `setupapi.dev.log` + `C:\Windows\inf\oem*.inf` inspection to identify exact package.
	- copy package manually from DriverStore when export commands are not available.
5. Target is Win10 IoT but still not binding driver:
	- in Device Manager, use Update Driver -> Browse my computer -> Let me pick -> Have Disk, then point to exported INF.
	- check Hardware Ids in Device Manager match IDs covered by INF.

### E) Preserve reproducibility

For each target machine, record:

1. exported `Published Name` and file hashes of INF/CAT/SYS,
2. target OS version/build,
3. whether `aciotest p4io` succeeded immediately or needed retries.

### F) Quick command checklists (copy/paste)

Use these when you want the shortest path from source machine to target machine.

Source machine (Win7 Embedded, elevated cmd):

1. `ver`
2. `mkdir C:\p4io-driver-export`
3. `pnputil /enum-devices /connected`
4. `pnputil /enum-drivers`
5. `pnputil /export-driver oem42.inf C:\p4io-driver-export`

If step 5 is unsupported, fallback:

1. `pnputil -e > C:\p4io-drivers.txt`
2. `findstr /i /c:"p4io" C:\Windows\inf\oem*.inf`
3. open matching `oemXX.inf` and note `CatalogFile=` and copied `.sys` references
4. search `C:\Windows\inf\setupapi.dev.log` for `oemXX.inf` to find Driver Store path
5. copy `oemXX.inf` + matching CAT + matching FileRepository folder into `C:\p4io-driver-export`

Target machine (Win10 IoT, elevated cmd):

1. `ver`
2. `pnputil /add-driver C:\p4io-driver-export\*.inf /install`
3. `pnputil /enum-drivers`
4. `pnputil /enum-devices /connected`
5. `aciotest.exe p4io`

If add-driver path fails, fallback:

1. `pnputil -a C:\p4io-driver-export\oemXX.inf`
2. `pnputil -i -a C:\p4io-driver-export\oemXX.inf`
3. rerun `aciotest.exe p4io`

Manual UI fallback on target:

1. Device Manager -> target device -> Update Driver
2. Browse my computer for drivers
3. Let me pick from a list
4. Have Disk
5. point to `C:\p4io-driver-export\oemXX.inf`

## Preconditions

1. Connect cabinet P4IO and verify driver access.
2. Build tools so the latest aciotest is available.
3. Keep a log file for each run (date, game build, cabinet type, notes).

## Step 1: Baseline Digital Bit Discovery

Run:

- aciotest.exe p4io

Do:

1. Hit one input at a time.
2. Record which jamma[0] bit flips for each control.
3. Repeat each control 3 to 5 times to confirm consistency.

Capture:

- Left Cymbal
- Hi-Hat
- Left Pedal
- Snare
- Hi Tom
- Bass Pedal
- Low Tom
- Floor Tom
- Right Cymbal
- Service
- Test
- Coin
- Start
- Up, Down, Left, Right
- Help
- Extra1, Extra2

Expected output value:

- stable bit index per control with known polarity (pad bits usually active low, system bits usually active high)

## Step 2: Guided Mapping Capture

Run:

- aciotest.exe p4io map
- aciotest.exe p4io map save

Do:

1. Follow prompts exactly (idle baseline, then hold each control).
2. Use save mode for a timestamped artifact.
3. Re-run at least twice to detect unstable or ambiguous controls.

Capture:

- generated #define JAMMA_BIT_* lines
- unresolved lines, if any
- controls that produce multiple candidate bits

Expected output value:

- paste-ready mapping block for src/main/dmio-p4io/dmio.c
- reproducibility check across runs

## Step 3: Analog Channel Discovery

Run:

- aciotest.exe p4io analog
- aciotest.exe p4io analog2

Do:

1. Strike each drum surface at low, medium, high force.
2. For each hit type, record SCI bytes that change and their min/max windows.
3. Compare changes while touching unrelated controls to identify noise vs signal.

Capture:

- per-pattern SCI payload lengths
- changing byte indexes by instrument
- 16-bit candidate words and decoded values shown by analog2
- whether values are monotonic with hit strength

Expected output value:

- shortlist of likely intensity channels
- confidence level per channel (high, medium, low)

## Step 4: Cross-Validation Through Consumers

Run:

- midi-dmio.exe
- vigem-dmio.exe

Do:

1. Verify each physical control maps to the intended MIDI note and XInput output.
2. Check repeated-hit behavior (double strokes, fast rolls, pedal repeats).
3. Check Service+Test exit combo still works reliably.

Capture:

- any missed hits
- any false positives
- any wrong control mapping
- latency observations

Expected output value:

- practical validation of dmio behavior under gameplay-like input

## Step 5: Feed Findings Back Into Code

Update:

1. src/main/dmio-p4io/dmio.c constants JAMMA_BIT_* from confirmed map results.
2. If needed, debounce thresholds for noisy lines.
3. If needed, aciotest analog2 probe patterns from confirmed SCI behavior.

Then rebuild and retest:

1. Build dm-tools.
2. Re-run p4io map save and analog2 to confirm no regressions.

## Data Quality Checklist

Use this checklist before declaring mapping done:

1. Every control mapped at least twice with identical bit index.
2. No unresolved JAMMA_BIT_* lines remain.
3. Service, Test, Coin verified in both idle and active gameplay states.
4. Fast repeated hits do not drop disproportionately.
5. Analog candidate channels show stable directionality with hit force.

## Suggested Artifact Set Per Session

1. One p4io map save output file.
2. One human-readable session note with environment details.
3. One table of control -> jamma bit -> polarity -> confidence.
4. One table of candidate analog channels -> byte index/word -> confidence.

## Priority Order If Time Is Limited

1. Complete p4io map save and lock digital mappings.
2. Validate in midi-dmio and vigem-dmio.
3. Run analog2 only for top-priority pads (snare, bass pedal, hi-hat, cymbals).
