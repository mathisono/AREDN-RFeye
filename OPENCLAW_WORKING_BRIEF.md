# OpenClaw Working Brief

## Project
AREDN-RFeye

## Active milestone
r11 — GUI usability and display calibration (post-r10 stability pass)

## Guardrails
- No classifier work
- No channel hopping
- No channel changes
- Captures remain under `/tmp/rfeye`

## Completed in this run
- Confirmed starting point includes `f07ee34` and repo is up to date.
- Reworked node GUI page (`.../www/cgi-bin/apps/rfeye/user`) for:
  - Scrollable desktop/mobile layout
  - Compact top cards (Controls/Radio/Diagnostics)
  - Collapsible raw JSON
  - Improved waveform/waterfall/ambient rendering and no-data states
  - Display calibration controls (auto/manual/reset)
  - Safer polling (single in-flight request)
  - Heatmap legend and scale labels
- Extended `rfeye-agent heatmap_bundle` metadata with display/window counters and timing fields.
- Bumped package release to r11.
- Added `docs/UI_NOTES.md` and updated README milestone focus.

## Pending in this run
- Validation commands and parser/hardware tests.
- SDK build for `aredn-rfeye_0.1.0-r11_mips_24kc.ipk` + SHA256.
- Update `artifacts/ipk/BUILD_NOTES.md`.
- Node install/retest through MSE-88 and finalize r11 test report (currently blocked by SSH connectivity/auth from this environment).
- Commit and push.
