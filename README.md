# AREDN RFeye

AREDN RFeye is a lightweight RF spectrum-visibility app for AREDN/OpenWrt nodes using ath10k-based 802.11ac radios.

The project goal is an AirView-like **node-side test view** for AREDN operators while keeping heavier replay, reporting, and classifier work on a future Linux Workbench.

## Current status

RFeye is in active bench-node development.

Implemented so far:

- AREDN/OpenWrt package: `aredn-rfeye`
- Node GUI: `/cgi-bin/apps/rfeye/user`
- Safe node controller: `rfeye-agent`
- Survey/utilization helper: `rfeye-survey`
- ath10k spectral parser: `rfeye-spectral-parse`
- Trusted radio info from `iw dev <iface> info`
- Capture/session model under `/tmp/rfeye`
- Server-side products for waveform, waterfall, and ambient heat-map views
- JSON API endpoint for low-rate UI updates
- Raw capture inspection and parser probe diagnostics
- Test `.ipk` artifacts under `artifacts/ipk/`

Current milestone:

- r10 passed 5-minute stability testing on KJ6DZB-WSB-ACdish5 with bounded `/tmp/rfeye` growth and final `spectral_scan_ctl=disable`.
- r11 focuses on GUI usability + display calibration: scroll behavior, compact top cards, meaningful waveform/waterfall/ambient rendering, and lightweight scaling controls.

## Current field-test limitations

- This is still a node test tool, not a calibrated analyzer.
- ath10k spectral availability and survey counters vary by hardware/driver build.
- Ambient panel currently uses a lightweight minute-peak rollup, not long-term calibrated noise analytics.
- Heavy analytics/replay/classification are intentionally deferred to Linux Workbench workflows.
- Raw framing varies by node/driver build; use `raw_inspect`, `parser_probe`, and `--resync` to identify the active layout (`tlv3_be` and fixed-stride marker formats have both been observed in bench captures).
- On some timed runs, parser stats may show emitted frames while heatmap rows remain empty; treat this as an acquisition-to-session plumbing issue, not necessarily a raw-format decode failure.

## Safety model

RFeye is designed to be conservative on a mesh node:

- No automatic channel hopping
- No channel changes
- Current-channel/background spectral scan only
- Capture data stays under `/tmp/rfeye`
- No continuous flash writes
- Runtime and byte counts are capped
- Unsupported hardware returns JSON diagnostics instead of failing silently

## Tested node so far

First bench node:

```text
Node: KJ6DZB-WSB-ACdish5
AREDN/OpenWrt: AREDN 4.26.1.0 r29087-d9c5716d1d
Kernel: Linux 6.6.119 mips
Radio: phy0 / wlan0 / IBSS AREDN-20-v3
Channel: 141
Frequency: 5705 MHz
Width: 20 MHz
Result: r9 PASS; first live waterfall milestone; r10 targets 5-minute stability
```

See the test reports in `docs/` for details.

## Quick local test

Run the parser smoke test from the repo root:

```sh
scripts/test-parser-smoke.sh
```

Expected:

```text
RFeye parser smoke test passed
```

## Build an OpenWrt/AREDN package

Copy the package into an AREDN/OpenWrt or SDK tree:

```sh
RFeye=$HOME/src/AREDN-RFeye
AREDN=$HOME/src/aredn

rsync -av --delete "$RFeye/package/aredn-rfeye/" \
  "$AREDN/package/aredn-rfeye/"
```

Build only RFeye:

```sh
cd "$AREDN"
make package/aredn-rfeye/clean V=s
make package/aredn-rfeye/compile V=s
find bin -name 'aredn-rfeye*.ipk' -print
```

Install on a bench node:

```sh
scp bin/packages/*/*/aredn-rfeye_*.ipk root@localnode.local.mesh:/tmp/
ssh root@localnode.local.mesh
opkg install /tmp/aredn-rfeye_*.ipk
```

## Basic node checks

Run these before enabling any service:

```sh
/usr/sbin/rfeye-agent status
/usr/sbin/rfeye-agent radio_info
/usr/sbin/rfeye-survey survey
/usr/sbin/rfeye-survey utilization
/usr/sbin/rfeye-agent raw_capture_test 10 128 phy0
/usr/sbin/rfeye-agent raw_inspect
/usr/sbin/rfeye-agent parser_probe
```

For r10 long-run stability, the most important commands are:

```sh
/usr/sbin/rfeye-agent storage_status
/usr/sbin/rfeye-agent soak_test 300 128 phy0
/usr/sbin/rfeye-agent capture_status
/usr/sbin/rfeye-agent pipeline_status
/usr/sbin/rfeye-agent heatmap_bundle
```

## Node GUI

Open:

```text
http://localnode.local.mesh/cgi-bin/apps/rfeye/user
```

Current UI panels:

1. **Waveform** — spectrum-style trace with display min/max labels and trusted radio frequency context.
2. **Waterfall** — rolling recent-frame heat map with legend and visible row count.
3. **Ambient** — slower minute-peak history, including in-progress row visibility when available.

Top cards are compact (Controls / Radio / Diagnostics), raw JSON is collapsed by default, and page scrolling is enabled for desktop/mobile.

Display scaling (auto/manual) is intended for **visual contrast only**; values are approximate/relative and not calibrated lab RF measurements.

## JSON API

The CGI bridge is:

```text
/cgi-bin/apps/rfeye/data/agent.sh
```

Useful actions:

```text
action=status
action=radio_info
action=ui_state
action=start&seconds=10&bins=128&phy=phy0
action=stop
action=reset
action=capture_status
action=heatmap_bundle
action=waveform
action=waterfall
action=ambient
action=survey
action=utilization
action=survey_raw
action=raw_inspect
action=parser_probe
action=storage_status
action=acquisition_debug
```

Example:

```sh
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=heatmap_bundle'
```

## Radio frequency display

RFeye treats `iw dev <iface> info` as authoritative for the node operating channel, frequency, width, and IBSS/SSID.

FFT frame frequency metadata is kept for debugging only. Implausible FFT metadata, such as `768 MHz`, is marked invalid and must not be displayed as the real node frequency.

## Current development focus

r11 prioritizes operator usability + visualization calibration after r10 stability:

- Keep 5-minute stability behavior and bounded `/tmp/rfeye` products.
- Improve web UI scrolling and compact top cards.
- Make waveform/waterfall/ambient visually meaningful with legends/labels.
- Add lightweight auto/manual display scale controls.
- Keep current-channel-only behavior (`no channel hopping`, `no channel changes`) and safe final `spectral_scan_ctl=disable`.

5-minute soak test:

```sh
/usr/sbin/rfeye-agent reset
/usr/sbin/rfeye-agent soak_test 300 128 phy0
```

See `docs/LONG_RUN_TESTING.md` and `docs/UI_NOTES.md`.

## Documentation

Key docs:

- `docs/ARCHITECTURE.md`
- `docs/ROADMAP.md`
- `docs/BUILD_AND_NODE_TEST.md`
- `docs/UI_NOTES.md`
- `docs/OPENCLAW_IPK_TASK.md`
- `docs/NODE_TEST_REPORT_TEMPLATE.md`
- `docs/TEST_RESULTS_KJ6DZB_WSB_ACDISH5_R10.md`
- `docs/TEST_RESULTS_KJ6DZB_WSB_ACDISH5_R11.md`

## License

GPL-3.0-or-later
