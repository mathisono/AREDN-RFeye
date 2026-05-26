# AREDN RFeye

Node-side RF spectrum visibility for AREDN/OpenWrt mesh nodes with ath10k 802.11ac radios.

RFeye shows a real-time spectral view of your node's **current operating channel** using the ath10k hardware FFT engine. Each frame captures 72 FFT bins across the channel bandwidth (typically 20 MHz). This is a single-channel diagnostic tool — it does not scan or hop across the 5 GHz band.

## Current release: r13

**r13** fixes the intermittent capture stall from r11/r12 and improves frame rate from ~0.4 fps to **~1 fps sustained** on MIPS hardware.

### Quick start

```sh
# Copy IPK to the node, then:
opkg install --force-reinstall /tmp/aredn-rfeye_0.1.0-r13_mips_24kc.ipk

# Open the GUI:
# http://<node-ip>:8080/cgi-bin/apps/rfeye/user
```

### Requirements

- AREDN node with ath10k radio (e.g., QCA9880, QCA9882)
- OpenWrt kernel built with `ATH_DEBUG` and `ATH_SPECTRAL` (standard on AREDN)
- `iw` and `uhttpd` packages (standard on AREDN)

### What it shows

- **Waveform** — live spectrum trace of the current channel FFT
- **Waterfall** — rolling time-vs-frequency heat map
- **Ambient** — slower minute-peak noise history
- All displays are approximate/relative dBm, not lab-calibrated

### What it does NOT do

- No wideband sweep across the 5 GHz band — captures current channel only
- No channel hopping or channel changes
- No classifier or signal identification
- No continuous writes to flash — all data stays in `/tmp/rfeye`

## Milestone history

- **r13** — fixed intermittent stall, 3× frame rate improvement (head -c capture, single-awk products, fast append, spectral re-prime)
- **r12** — fixed stale parser packaging (source sync issue)
- **r11** — GUI polish (scrolling, compact cards, display scaling, legends)
- **r10** — stability milestone (5-minute soak PASS)

### Performance (r13 on KJ6DZB-WSB-ACdish5)

| Metric | Value |
|---|---|
| Frame rate (sustained) | ~1.0 fps |
| 300s soak test | 336 frames, zero stalls |
| Storage (5 min run) | ~2.2 MB in /tmp/rfeye |
| Memory impact | <5 MB additional |

## Safety model

- No channel hopping or channel changes
- Current-channel background spectral scan only
- All capture data in `/tmp/rfeye` (tmpfs, no flash writes)
- Runtime and byte counts are capped
- Final state after stop/reset: `spectral_scan_ctl=disable`
- Unsupported hardware returns JSON errors, never fails silently

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

## Development focus

No classifier or channel-hopping features yet. Current focus:

1. UI polish and documentation
2. Storage monitoring for longer runs (>5 minutes)
3. Production hardening (logging, error paths)
4. Keep capture data under `/tmp/rfeye` and ensure final `spectral_scan_ctl=disable`

## Documentation

- [`OPENCLAW_WORKING_BRIEF.md`](OPENCLAW_WORKING_BRIEF.md) — current working brief and task context
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — system architecture
- [`docs/TRIAGE_INTERMITTENT_STALL_KJ6DZB_WSB_ACDISH5.md`](docs/TRIAGE_INTERMITTENT_STALL_KJ6DZB_WSB_ACDISH5.md) — r13 stall root-cause analysis
- [`docs/BUILD_AND_NODE_TEST.md`](docs/BUILD_AND_NODE_TEST.md) — build and test instructions
- [`artifacts/ipk/BUILD_NOTES.md`](artifacts/ipk/BUILD_NOTES.md) — IPK build history

## License

GPL-3.0-or-later
