# TODO On Real Hardware

Goal: use aciotest on a real P4IO cabinet to extract trustworthy mappings and analog clues, then fold them back into dmio-p4io.

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
