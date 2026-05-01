# AREDN RFeye Project Brief

## Summary

AREDN RFeye is an AREDN-native RF visibility and spectrum-analysis project for supported ath10k-based 802.11ac nodes. It is intended to bring an AirView-like troubleshooting experience into the AREDN node itself without requiring an external SDR.

The project uses OpenWrt/Atheros spectral scan support through `PACKAGE_ATH_SPECTRAL`, exposed by ath10k debugfs paths such as:

```text
/sys/kernel/debug/ieee80211/phy*/ath10k/spectral_scan_ctl
/sys/kernel/debug/ieee80211/phy*/ath10k/spectral_scan0
/sys/kernel/debug/ieee80211/phy*/ath10k/spectral_bins
/sys/kernel/debug/ieee80211/phy*/ath10k/spectral_count
```

The first implementation stage is deliberately conservative: prove node compatibility, expose a small AREDN-style app page, and show spectral/survey support status before implementing binary FFT parsing and live waterfall display.

## AREDN fit

RFeye is designed as an AREDN app, not as a generic LuCI application. The initial scaffold installs app endpoints under:

```text
/www/cgi-bin/apps/rfeye/user
/www/cgi-bin/apps/rfeye/admin
/www/cgi-bin/apps/rfeye/data
/www/apps/rfeye/icon.svg
```

This makes the tool appear through the AREDN Apps menu and lets it reuse the node UI look by loading:

```text
/a/css/theme.css
/a/css/user.css
/a/css/mobile.css
```

## Target hardware

Initial target: AREDN-supported Ubiquiti/Qualcomm Atheros 802.11ac devices using ath10k/ath10k-ct radios, especially QCA988x-family devices.

The project must detect unsupported nodes gracefully. If spectral scan files are missing, the UI should report that RFeye is unavailable rather than failing silently.

## Core data sources

### ath10k spectral scan

Used for FFT bin energy, waterfall, max-hold, average trace, and interference-shape detection.

### iw/nl80211 survey counters

Used for channel utilization and external busy estimates. RFeye should compute deltas from active, busy, RX, TX, and noise counters where available.

### AREDN node context

Future versions should incorporate channel width, center frequency, SSID/network role, LQM/mesh neighbor information, and node identity where helpful.

## Planned views

- Support/probe status
- Current FFT trace
- Waterfall
- Average and max-hold trace
- Channel overlays for 20/40/80 MHz widths
- Noise floor trend
- Channel utilization
- External busy estimate
- Interference classification cards
- Capture/replay for troubleshooting and classifier testing

## Interference classes

Initial rule-based classifier targets:

- Wi-Fi-like OFDM energy
- Narrowband fixed carrier
- Wideband high-duty interferer
- Frequency hopper
- Burst/impulse source
- Possible DFS/radar-like candidate, clearly marked as non-regulatory
- Unknown non-Wi-Fi energy

## Important limits

RFeye is a mesh troubleshooting tool, not a calibrated analyzer. The UI should clearly state:

- FFT values are relative/driver-derived unless calibrated.
- DFS/radar labels are hints only, not regulatory detection.
- Continuous high-rate capture must not be written to flash.
- Production AP channel hopping must require explicit operator action.

## Development stages

1. AREDN app shell and probe script.
2. JSON status endpoint.
3. ath10k TLV parser and capture fixtures.
4. Low-rate live FFT view.
5. Waterfall renderer.
6. Survey-based utilization model.
7. Rule-based classifier.
8. Capture/replay and documentation.
