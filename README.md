# AREDN RFeye

AREDN RFeye is a lightweight RF spectrum-visibility app for AREDN/OpenWrt nodes using ath10k-based 802.11ac radios.

The project goal is an AirView-like node-side test view for AREDN operators while keeping heavier replay, reporting, and classifier work on a future Linux Workbench.

## Current status

RFeye is in active bench-node development.

Implemented so far:

- AREDN/OpenWrt package: `aredn-rfeye`
- Node GUI at `/cgi-bin/apps/rfeye/user`
- Safe node controller: `rfeye-agent`
- Survey/utilization helper: `rfeye-survey`
- ath10k spectral parser: `rfeye-spectral-parse`
- Trusted radio info from `iw dev <iface> info`
- Capture/session products under `/tmp/rfeye`
- Waveform, waterfall, and ambient heat-map views
- JSON API endpoint for low-rate UI updates
- Raw capture inspection and parser probe diagnostics
- Test `.ipk` artifacts under `artifacts/ipk/`

## Milestone status

- **r10** is the last fully proven stability milestone. It passed 5-minute node testing on KJ6DZB-WSB-ACdish5 with stable frame capture, bounded `/tmp/rfeye` growth, populated waterfall data, and final `spectral_scan_ctl=disable`.
- **r11** added GUI and display improvements: page scrolling, compact Controls / Radio / Diagnostics cards, improved waveform/waterfall/ambient rendering, display scale controls, and improved bundle metadata. Live acceptance was intermittent.
- **r12** fixed a release-blocking packaging issue: the package build had been compiling a stale parser source from `package/aredn-rfeye/src/rfeye_spectral_parse.c` instead of the canonical parser in `src/rfeye_spectral_parse.c`.

Current open work:

- Finish intermittent capture/feed stall triage.
- Update the working brief and related docs with the r12 state.
- Decide whether a minimal watchdog/re-prime fix is warranted.
- Avoid new RF features until the stall behavior is understood.

## Safety model

RFeye is designed to be conservative on a mesh node:

- No automatic channel hopping
- No channel changes
- Current-channel/background spectral scan only
- Capture data stays under `/tmp/rfeye`
- No continuous flash writes
- Runtime and byte counts are capped
- Unsupported hardware returns JSON diagnostics instead of failing silently
- Final state after stop/reset must be `spectral_scan_ctl=disable`

## Parser source sync requirement

There are two parser source locations:

```text
src/rfeye_spectral_parse.c
package/aredn-rfeye/src/rfeye_spectral_parse.c
```

The package build compiles from `package/aredn-rfeye/src/`. These files must stay synchronized.

Before building an IPK, run the parser source sync check:

```text
scripts/check-parser-source-sync.sh
```

The installed node parser must support both `--probe` and `--resync`.

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
Result: r10 PASS for 5-minute stability; r11 GUI built; r12 parser packaging fixed; intermittent stall triage ongoing
```

## Node GUI

Open the node app at:

```text
/cgi-bin/apps/rfeye/user
```

Current UI panels:

1. **Waveform** — spectrum-style trace with display min/max labels and trusted radio frequency context.
2. **Waterfall** — rolling recent-frame heat map with legend and visible row count.
3. **Ambient** — slower minute-peak history, including in-progress row visibility when available.

Top cards are compact, raw JSON is collapsed by default, and page scrolling is enabled for desktop/mobile.

Display scaling is intended for visual contrast only; values are approximate/relative and not calibrated lab RF measurements.

## Radio frequency display

RFeye treats `iw dev <iface> info` as authoritative for the node operating channel, frequency, width, and IBSS/SSID.

FFT frame frequency metadata is kept for debugging only. Implausible FFT metadata, such as `768 MHz`, is marked invalid and must not be displayed as the real node frequency.

## Current development focus

Do not add classifier work or channel-hopping features yet. The immediate focus is reliability:

1. Finish the intermittent capture/feed stall report.
2. Identify whether stalls are spectral-source, parser, capture-loop, product-writer, or GUI/API related.
3. Add only a minimal watchdog/re-prime fix if evidence supports it.
4. Keep capture data under `/tmp/rfeye` and ensure final `spectral_scan_ctl=disable`.

## Documentation

Key docs:

- `OPENCLAW_WORKING_BRIEF.md`
- `docs/BUILD_AND_NODE_TEST.md`
- `docs/LONG_RUN_TESTING.md`
- `docs/UI_NOTES.md`
- `docs/NODE_TEST_REPORT_TEMPLATE.md`
- `docs/TEST_RESULTS_KJ6DZB_WSB_ACDISH5_R10.md`
- `docs/TEST_RESULTS_KJ6DZB_WSB_ACDISH5_R11.md`

## License

GPL-3.0-or-later
