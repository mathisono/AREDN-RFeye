# OpenClaw Working Brief

## Project
AREDN-RFeye

## Active milestone
r11 — GUI usability + live backend regression triage

## Guardrails
- No classifier work
- No channel hopping
- No channel changes
- Captures remain under `/tmp/rfeye`
- End state must be `spectral_scan_ctl=disable`

## Current validated state
- r11 GUI/source work committed and pushed (`3f34455`).
- r11 live acceptance report updated and pushed (`cca8762`) with FAIL due to zero parsed frames.
- Live connectivity path via MSE-88 is now functional.

## Focused backend triage findings (no production code changes)

Live node: `10.188.138.222` via `MSE-88`.

### Acquisition layer
- Raw capture works: `raw_capture_test` produced `bytes_read=262144` and `/tmp/rfeye/raw-test.tlv` present.
- During timed start, `/tmp/rfeye/latest.tlv` observed at 32768 bytes.
- So capture activation and ingest are not completely dead.

### Parser layer
- `raw_capture_test` result: `ok:false`, `frames_emitted:0`, `error:no parsed frames`.
- `pipeline_test 128 phy0`: parser stage fails (`parse_resync:false`).
- Node parser usage output lacks `--resync` and `--probe` options.
- Node parser SHA256: `ec5ae34d464d7fd40d2c76d357b7d2cc10452ff7e487f3f664c57b45f9981881`.

### r10 vs r11 package parser diff
- Extracted `r10` IPK parser binary strings include `--probe` and `--resync`.
- Extracted `r11` IPK parser binary strings do **not** include these options.
- Repo sources diverged:
  - `src/rfeye_spectral_parse.c` = newer parser (supports `--probe/--resync`)
  - `package/aredn-rfeye/src/rfeye_spectral_parse.c` = older parser (no `--probe/--resync`)
- Build path compiles parser from `package/aredn-rfeye/src/...`, causing r11 package to ship incompatible parser for current `rfeye-agent` invocation.

## Regression layer and candidate root cause
- **Identified regression layer:** Parser packaging/build-input mismatch (not radio channel config; not final cleanup path).
- **Primary root-cause candidate:** stale parser source under `package/aredn-rfeye/src/` compiled into r11 IPK while agent expects `--resync/--probe` capable parser.

## Safety confirmation
- Final live state confirmed: `spectral_scan_ctl=disable`.
- Artifacts remained under `/tmp/rfeye`.
- No channel hopping/channel changes performed.

## Next minimal code-change target (not yet applied)
1. Synchronize parser source used by package build (ensure compiled source includes `--resync/--probe`).
2. Rebuild IPK and verify parser `--help` on node includes `--resync/--probe`.
3. Re-run backend + GUI live acceptance.

## AirView Architecture Finding (2026-05-26)

Behavioral observation of a Rocket 5AC Lite confirmed that Ubiquiti AirView uses
a **dedicated second on-chip radio** (wifi1/AHB) for spectral scanning while the
main PCI radio (wifi0) continues serving the AP. The scanner sweeps ~5100–5900 MHz
and stitches per-channel FFT slices into a wideband display.

Implications for RFeye:

- **RFeye production mode remains current-channel only.** Single-radio AREDN
  devices (PBE-5AC-500, etc.) cannot replicate AirView wideband without dropping
  the AP link.
- **AirView research mode is separate and bench-only.** Any retune-and-stitch
  experimentation is restricted to lab hardware not serving live mesh traffic.
- **WMI knobs remain useful** for improving per-channel FFT quality.
- **PBE-5AC-500 needs hardware verification** — check whether it exposes a second
  radio before assuming AirView-like scanning is possible.

See: `docs/AIRVIEW_ARCHITECTURE_FINDINGS.md` and `docs/WIDEBAND_SPECTRAL_WORKING_BRIEF.md`
