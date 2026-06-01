# AREDN RFeye

**AREDN RFeye** is an AREDN/OpenWrt node-side RF spectrum visibility app. It turns supported mesh nodes into lightweight network-attached spectral sensors for troubleshooting interference, watching the local noise floor, and eventually feeding real measured RF conditions into mesh planning tools.

The project is still in active hardware bring-up. Install and test it on bench nodes first.

## Current status

**Package:** `aredn-rfeye` `0.1.0-r18`  
**Primary target:** AREDN/OpenWrt MIPS ath79 Ubiquiti AC nodes  
**Current focus:** safe WMAC caldata provisioning, ath9k spectral parsing, and live browser rendering

RFeye currently provides:

- A node web UI at `/cgi-bin/apps/rfeye/user`.
- HTTP/CGI JSON endpoints through `/cgi-bin/apps/rfeye/data/agent.sh`.
- An agent CLI at `/usr/sbin/rfeye-agent`.
- A survey helper at `/usr/sbin/rfeye-survey`.
- A C spectral parser: `/usr/lib/rfeye/rfeye-spectral-parse`.
- A C WMAC provisioning helper: `/usr/lib/rfeye/rfeye-wmac-rebind`.
- A packaged WA-reference WMAC caldata file for bench testing.

The current UI is designed to survive incomplete hardware support. If the WebSocket live feed is not running, it falls back to HTTP polling. If no valid FFT frames decode yet, the page should still load and expose diagnostics instead of failing silently.

## What RFeye is for

AREDN link planning often assumes a static noise floor. In real mesh environments, the noise floor moves constantly because of Wi-Fi bursts, nearby devices, microwave links, radar/DFS activity, and site-specific interference. A static guess can make a link look viable on paper while the real band is unusable.

RFeye is intended to make every compatible AREDN node a remote spectrum sensor:

1. Capture spectral FFT data from the node radio hardware.
2. Convert raw bins into approximate/relative dBm values.
3. Show waveform, waterfall, and slower ambient/noise history in the browser.
4. Later, expose per-channel measured noise floor to mesh planning and RF modeling tools.

## Hardware support

### ath10k production radio

RFeye can use ath10k spectral debugfs support for current-channel visibility on AREDN nodes with QCA988x/QCA99xx-style radios.

Safety rule: RFeye does **not** channel-hop the production ath10k mesh radio.

### ath9k WMAC scanner radio

Ubiquiti XC boards based on QCA9558 include an on-chip 2.4 GHz WMAC that can run ath9k spectral scan. With AREDN PR #2730, the WMAC is enabled in DTS with `qca,no-eeprom`, which means the kernel expects RFeye or another app to provide caldata from the filesystem.

Initial r17/r18 target boards:

- PowerBeam 5AC 500 / PBE-5AC-500 XC
- Rocket 5AC Lite XC

Possible future target:

- NanoBeam AC XC, once DTS support is confirmed

## Why valid caldata matters — please help

The ath9k WMAC will not initialize correctly without calibration data. This is not optional.

On the tested XC boards, the expected WMAC ART location at `ART+0x1000` is blank. That means there is no obvious factory WMAC caldata to hand to ath9k. AREDN PR #2730 correctly avoids pretending there is valid EEPROM in flash by using `qca,no-eeprom`; RFeye must then place a valid firmware caldata blob under `/lib/firmware/` before binding the WMAC.

Important safety rules:

- Do **not** feed blank data.
- Do **not** feed random bytes.
- Do **not** use the PCI radio caldata at `ART+0x5000` as a production WMAC source.
- Treat substitute/reference caldata as bench-only until validated.
- Never write experimental caldata into ART/EEPROM flash.

RFeye r18 includes a C helper and shell wrapper that install caldata to:

```text
/lib/firmware/ath9k-eeprom-ahb-18100000.wmac.bin
```

Then RFeye safely rebinds only the WMAC platform device:

```text
/sys/bus/platform/drivers/ath9k/unbind
/sys/bus/platform/drivers/ath9k/bind
18100000.wmac
```

This avoids the earlier sledgehammer approach of `rmmod ath9k` on a live mesh node. Broad module reload remains disabled by default and is only available as an explicit last-resort escape hatch with `RFEYE_ALLOW_RMMOD=1`.

### Caldata help wanted

The project needs help finding, validating, and documenting correct WMAC caldata sources for each board family.

Helpful contributions:

- Stock WA board dumps where ART+0x1000 contains real WMAC data.
- Stock XC board reports showing whether `/proc/device-tree` exposes useful WMAC hints.
- `/lib/firmware` listings from stock Ubiquiti firmware.
- Confirmation of what AirView/ubntspecd uses on stock firmware.
- Bench test results comparing WA-reference caldata vs. other safe sources.
- RF sanity checks: noise floor, spectral bins, stability, and whether the WMAC behaves consistently across channels.

Do not post private node credentials, private keys, or sensitive RF site access details in GitHub issues.

## Install the IPK

Builds are normal OpenWrt/AREDN `.ipk` packages.

Copy the package to a bench node:

```sh
scp artifacts/ipk/aredn-rfeye_0.1.0-r18_mips_24kc.ipk root@localnode.local.mesh:/tmp/
```

Install or reinstall it:

```sh
ssh root@localnode.local.mesh
opkg install --force-reinstall /tmp/aredn-rfeye_0.1.0-r18_mips_24kc.ipk
```

If your artifact name differs, use the file actually built by your SDK:

```sh
opkg install --force-reinstall /tmp/aredn-rfeye_*.ipk
```

After install:

```sh
/usr/sbin/rfeye-agent status
/usr/sbin/rfeye-agent radio_info
/usr/sbin/rfeye-agent wmac_status
```

For WMAC provisioning on a supported bench node:

```sh
/usr/sbin/rfeye-agent wmac_status
/usr/sbin/rfeye-agent wmac_provision
/usr/sbin/rfeye-agent wmac_status
```

The status JSON should report whether the helper exists, whether caldata is installed, whether the WMAC platform device is present, and whether spectral scan is ready.

## Use the web UI

Open the node UI:

```text
http://localnode.local.mesh/cgi-bin/apps/rfeye/user
```

Or by IP:

```text
http://<node-ip>:8080/cgi-bin/apps/rfeye/user
```

Main controls:

- **Start** — starts a timed capture using the selected duration.
- **Stop** — stops capture and disables spectral scanning.
- **Reset** — resets RFeye runtime state under `/tmp/rfeye`.
- **Mock stream** — browser-side test mode that simulates a 56-bin ath9k stream at about 10 fps. Use this to test Canvas rendering without the C WebSocket daemon.
- **Band selector** — selects how the UI labels/displays the frequency range.
- **Auto scale** — 5th/95th percentile display scaling over visible bins, with a minimum 20 dB span.
- **Manual min/max** — fixed display range for visual comparison.

Panels:

- **Waveform** — latest spectrum trace.
- **Waterfall** — scrolling recent history, newest row at the bottom.
- **Ambient / slower noise history** — minute-style peak/noise history.
- **Raw JSON** — collapsed diagnostic output from the agent/API.

Live transport behavior:

1. Browser tries WebSocket first: `ws://<node-host>:8080/rfeye`.
2. If WebSocket fails, the UI falls back to HTTP polling.
3. The status badge shows `ws live`, `polling`, `mock`, or `offline`.

The current C WebSocket daemon is not yet committed as a production service. The UI path is ready first so frontend rendering can be proven independently.

## CLI basics

Useful commands on the node:

```sh
/usr/sbin/rfeye-agent status
/usr/sbin/rfeye-agent radio_info
/usr/sbin/rfeye-agent heatmap_bundle
/usr/sbin/rfeye-agent start 10 128 phy0
/usr/sbin/rfeye-agent capture_status
/usr/sbin/rfeye-agent stop
/usr/sbin/rfeye-agent reset
```

WMAC/caldata commands:

```sh
/usr/sbin/rfeye-agent wmac_status
/usr/sbin/rfeye-agent wmac_install
/usr/sbin/rfeye-agent wmac_reload
/usr/sbin/rfeye-agent wmac_provision
/usr/sbin/rfeye-agent wmac_remove
```

Direct C helper commands:

```sh
/usr/lib/rfeye/rfeye-wmac-rebind --status
/usr/lib/rfeye/rfeye-wmac-rebind --install
/usr/lib/rfeye/rfeye-wmac-rebind --rebind
/usr/lib/rfeye/rfeye-wmac-rebind --provision
```

## Test suite summary

The active test suite has three layers: local build validation, node CLI/API validation, and hardware/parser validation.

### 1. Local validation before building

Run from the repository root:

```sh
sh scripts/check-parser-source-sync.sh
sh -n package/aredn-rfeye/files/usr/sbin/rfeye-agent
sh -n package/aredn-rfeye/files/usr/sbin/rfeye-survey
sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/data/agent.sh
sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/user
cc -Wall -Wextra -o /tmp/rfeye-spectral-parse src/rfeye_spectral_parse.c
/tmp/rfeye-spectral-parse --help | grep -E -- '--probe|--resync'
scripts/test-parser-smoke.sh
scripts/test-hardware-fixture-probe.sh
```

Expected local result:

```text
Parser source sync check passed
RFeye parser smoke test passed
```

### 2. Build validation

Inside the AREDN/OpenWrt build tree or SDK:

```sh
make package/aredn-rfeye/clean V=s
make package/aredn-rfeye/compile V=s
find bin -name 'aredn-rfeye*.ipk' -print
```

The package Makefile currently builds both C binaries:

- `rfeye-spectral-parse`
- `rfeye-wmac-rebind`

### 3. Node install/API validation

On the bench node:

```sh
/usr/sbin/rfeye-agent status
/usr/sbin/rfeye-agent radio_info
/usr/sbin/rfeye-survey survey
/usr/sbin/rfeye-survey utilization
/usr/sbin/rfeye-agent wmac_status
```

Expected:

- Valid JSON from each command.
- Clear diagnostic JSON if hardware support is incomplete.
- Correct trusted radio info: PHY, interface, channel, frequency, width, and mode.

### 4. Parser/framing diagnostic flow

This is the critical hardware bring-up path when raw data exists but no frames decode:

```sh
/usr/sbin/rfeye-agent reset
/usr/sbin/rfeye-agent raw_capture_test 10 128 phy0
/usr/sbin/rfeye-agent raw_inspect
/usr/sbin/rfeye-agent parser_probe
/usr/lib/rfeye/rfeye-spectral-parse --probe --input /tmp/rfeye/raw-test.tlv
/usr/lib/rfeye/rfeye-spectral-parse --debug --stats --input /tmp/rfeye/raw-test.tlv --phy phy0 --limit 5 --bins 64
hexdump -C /tmp/rfeye/raw-test.tlv | head -40
```

Interpretation:

- `bytes_read = 0` means spectral capture is not producing data.
- `bytes_read > 0` and `frames_emitted = 0` means parser/framing/layout still needs work.
- Valid emitted frames allow heatmap/capture-loop testing.

### 5. GUI validation

Open:

```text
http://localnode.local.mesh/cgi-bin/apps/rfeye/user
```

Verify:

- Page loads on desktop and mobile.
- Start/Stop/Reset are visible.
- Raw JSON is collapsed by default.
- Waveform/waterfall/ambient panels stay alive even with no decoded frames.
- Mock stream paints waveform and waterfall at about 10 fps.
- HTTP polling fallback works when the WebSocket daemon is absent.

### Current expected test result

Until real hardware parser/framing and WMAC caldata validation are complete, a realistic bench result is:

```text
Package install: PASS
CLI JSON: PASS
CGI/API: PASS
GUI load: PASS
Mock stream rendering: PASS
WMAC caldata provisioning path: PARTIAL / bench-only
Raw spectral capture: hardware-dependent
Parsed FFT frames: hardware/parser-dependent
Overall: active bring-up
```

## Development roadmap

### r17 — WMAC caldata self-provisioning

Goal: make AREDN PR #2730 usable from RFeye without requiring manual router surgery.

Status:

- Add packaged reference caldata for bench use.
- Install caldata to the ath9k firmware path.
- Avoid ART/EEPROM writes.
- Avoid default `rmmod ath9k`.
- Use WMAC-only platform bind/unbind for `18100000.wmac`.
- Add JSON status and error reporting.

### r18 — C helper and UI live-ingest foundation

Goal: remove fragile shell hot paths and prepare the browser for a C daemon feed.

Status:

- Build and install `rfeye-wmac-rebind`.
- Provisioning script prefers the C helper and falls back to shell sysfs only if needed.
- UI supports WebSocket-first ingest with HTTP polling fallback.
- UI includes a mock 56-bin stream for Canvas stress testing.
- Autoscale remains browser-side and throttled.

### r19 — C spectral WebSocket daemon

Goal: stream parsed spectral frames directly from node C code to the browser without writing hot FFT data to flash.

Planned:

- Open ath9k debugfs/relayfs spectral stream.
- Parse TLV/framing safely on MIPS.
- Apply the audited ath9k bin power formula.
- Emit compact JSON frames such as:

```json
{"bins":[-105.2,-104.1,-90.5]}
```

- Broadcast over a lightweight WebSocket server.
- Keep persistent data out of flash; hot data stays in RAM.

### r20+ — calibration, noise floor API, and mesh aggregation

Planned:

- Validate board-specific WMAC caldata sources.
- Add per-channel correction/offset tables.
- Expose rolling noise-floor JSON API.
- Aggregate spectral/noise data from multiple mesh nodes.
- Feed CloudRF, SPLAT!, Radio Mobile, or custom mesh planning tools with measured noise instead of static guesses.

## Contribute hardware access over the mesh

The most useful contribution right now is safe, temporary access to real hardware across the mesh.

Preferred model:

```text
Developer workstation
  -> mesh supernode / jump host
    -> target AREDN node web UI and SSH
```

Please provide access out-of-band, not in public GitHub issues.

Useful access details:

- Node model and exact board family.
- AREDN/OpenWrt version.
- Kernel version.
- Node IP and hostname on the mesh.
- Whether access is through a supernode, jump host, VPN, or local mesh segment.
- Which ports are reachable: web UI, SSH, or both.
- Whether the node is bench-only or production-adjacent.
- Any no-touch rules, such as do not reboot, do not change channel, or do not run provisioning.

Suggested read-only commands for contributors who do not want to provide shell access:

```sh
cat /tmp/sysinfo/model
cat /etc/board.json
uname -a
ls -R /proc/device-tree 2>/dev/null | head -200
ls -R /lib/firmware 2>/dev/null
ls /sys/bus/platform/devices | grep -E 'wmac|18100000'
ls /sys/kernel/debug/ieee80211 2>/dev/null
```

For deeper caldata research, coordinate first. Do not publish full EEPROM/ART dumps publicly unless you understand what is inside and are comfortable sharing it.

## Safety model

RFeye should be boring and safe on a live mesh node.

Rules:

- No channel hopping on the production ath10k mesh radio.
- No continuous writes to flash.
- Hot capture data stays under `/tmp/rfeye`.
- Stop/reset should leave `spectral_scan_ctl=disable`.
- Unsupported hardware should return clear JSON errors.
- WMAC provisioning must not write ART/EEPROM flash.
- WMAC rebind should target only `18100000.wmac`.
- Broad ath9k module reload is opt-in only with `RFEYE_ALLOW_RMMOD=1`.

## Repository map

```text
package/aredn-rfeye/                     OpenWrt/AREDN package
package/aredn-rfeye/src/                 C sources used by the IPK build
package/aredn-rfeye/files/usr/sbin/      rfeye-agent and rfeye-survey
package/aredn-rfeye/files/usr/lib/rfeye/ helper scripts and packaged caldata
package/aredn-rfeye/files/www/cgi-bin/   web UI and JSON CGI endpoint
docs/                                    roadmap, testing, architecture, research notes
scripts/                                 local validation helpers
artifacts/                               build/test artifacts when intentionally committed
```

Package build note: the IPK compiles from `package/aredn-rfeye/src/`. Keep any top-level parser source copies synchronized before building.

## Key documentation

- [`docs/ROADMAP.md`](docs/ROADMAP.md) — detailed development roadmap.
- [`docs/BUILD_AND_NODE_TEST.md`](docs/BUILD_AND_NODE_TEST.md) — build and test procedure.
- [`docs/UI_NOTES.md`](docs/UI_NOTES.md) — UI layout and display behavior.
- [`docs/SPECTRAL_MATH_AUDIT_REPORT.md`](docs/SPECTRAL_MATH_AUDIT_REPORT.md) — ath9k spectral math audit.
- [`docs/WMAC_CALDATA_RESEARCH_AND_TESTING_PLAN.md`](docs/WMAC_CALDATA_RESEARCH_AND_TESTING_PLAN.md) — caldata research and test plan.
- [`docs/ONCHIP_SCANNER_RADIO_RESEARCH.md`](docs/ONCHIP_SCANNER_RADIO_RESEARCH.md) — QCA9558 WMAC notes.
- [`docs/The XC vs. WA Divide.md`](docs/The%20XC%20vs.%20WA%20Divide.md) — XC vs. WA hardware/caldata research.

## License

GPL-3.0-or-later
