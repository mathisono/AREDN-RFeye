# OpenClaw Working Brief

## Project

AREDN-RFeye

RFeye is a node-side AREDN/OpenWrt RF spectrum visibility prototype for ath10k-based 802.11ac radios. The project goal is an AirView-like node test view, with heavier analysis and reporting deferred to a future Linux Workbench.

## Start here

**r13 is complete and deployed.** The intermittent capture stall is fixed and frame rate is ~1 fps sustained.

### Important: spectral capture is current-channel only (ath10k)

The ath10k spectral scan captures FFT data for the **current operating channel only** (typically 20 MHz on AREDN). Each frame is 72 FFT bins across that channel. This is NOT a wideband sweep across 5 GHz. Wideband capture would require channel hopping, which is prohibited on the production radio.

**Update (2026-05-27):** The ath9k WMAC on XC boards (phy1) CAN scan arbitrary channels including chanscan wideband sweep, without disrupting the ath10k mesh radio on phy0. The current-channel limitation applies only to the ath10k production radio.

### Next session focus

- ath9k WMAC integration: wideband sweep via chanscan mode
- Multi-channel waterfall display for WMAC data
- Keep existing ath10k single-channel pipeline working
- UI polish (r14 candidate)
- Storage monitoring for runs >5 minutes

### Recent history

- **r13** — fixed intermittent stall, 3× frame rate (0.37→1.09 fps), head-c capture, single-awk products, spectral re-prime
- **r12** — fixed stale parser packaging
- **r11** — GUI polish, intermittent acceptance

## Active milestone

r13 complete. WMAC (ath9k) wideband scanner integration in progress.

- Parser: ath9k type-1 HT20 / type-2 HT40 decoding added and tested
- Probe: ath9k spectral capability detection added
- Agent: driver auto-detection (ath10k/ath9k) added
- Next: chanscan wideband sweep pipeline, multi-channel waterfall display

## Guardrails

- No classifier work yet
- No channel hopping or channel changes on the ath10k production radio
- WMAC (ath9k) channel scanning is permitted — it is an independent radio
- Captures remain under `/tmp/rfeye`
- No continuous capture writes to flash
- End state after stop/reset must be `spectral_scan_ctl=disable`
- Do not add new features until the intermittent feed stall is understood

## Known-good state

### r10 — last fully proven stability milestone

Results on KJ6DZB-WSB-ACdish5:

- 5-minute manual run: PASS
- `frames_captured=80`
- `no_frame_count=0`
- `waterfall_rows=80`
- `/tmp/rfeye` about 772 KiB
- `soak_test 300 128 phy0`: PASS
- `soak_test frames_captured=100`
- `state_dir_bytes=823296`
- final `spectral_scan_ctl=disable`
- no channel hopping or channel changes

### r11 — GUI/display work

r11 added:

- page scrolling
- compact Controls / Radio / Diagnostics cards
- collapsible raw JSON
- waveform labels and trusted radio frequency context
- waterfall and ambient legends / no-data states
- auto/manual display scaling controls
- improved bundle metadata

r11 source/build validation passed, but live acceptance was intermittent.

### r12 — parser packaging sync fix

r12 fixed a release-blocking packaging issue.

The package build had been compiling stale parser source from:

```text
package/aredn-rfeye/src/rfeye_spectral_parse.c
```

instead of the newer canonical parser source:

```text
src/rfeye_spectral_parse.c
```

The stale packaged parser lacked:

```text
--probe
--resync
```

while `rfeye-agent` expected the resync-capable parser path.

r12 synced the package parser source from the canonical parser source and added a source-sync validation check.

Before building any IPK, run:

```sh
sh scripts/check-parser-source-sync.sh
```

On the node, verify:

```sh
/usr/lib/rfeye/rfeye-spectral-parse --help | grep -E -- '--probe|--resync'
```

## Last known runtime observation

After the parser sync fix, the system briefly recovered and produced:

```text
parser_mode=resync
frames_captured=16
frame_rate=0.52/s
last_frame_age=4s
final spectral_scan_ctl=disable
```

It later stalled again, so the remaining issue appears intermittent and runtime-related rather than a basic parser capability problem.

## Current blocker

Intermittent capture/feed stall.

Potential layers to distinguish:

1. `spectral_scan0` stops producing parseable data
2. capture loop exits or hangs
3. parser still works but pipeline stops updating
4. lock/pid/end files become stale
5. storage/memory issue
6. GUI/API polling issue
7. spectral scan state issue

## Node under test

```text
Node: KJ6DZB-WSB-ACdish5
IP: 10.188.138.222
SSH: port 2222
Web: http://10.188.138.222:8080
Jump host when needed: MSE-88 / 192.168.3.88
AREDN/OpenWrt: AREDN 4.26.1.0 r29087-d9c5716d1d
Kernel: Linux 6.6.119 mips
Radio: phy0 / wlan0 / IBSS AREDN-20-v3
Channel: 141
Frequency: 5705 MHz
Width: 20 MHz
```

## WMAC (ath9k) radio

Enabled by AREDN PR #2725 device tree patch.

- phy1 / ath9k / Atheros AR9550 Rev:0
- No factory caldata — uses ath9k template EEPROM fallback
- 3x3 MIMO, 2.4 GHz + 5 GHz bands
- TX power: 15 dBm (template default, receive-only use)
- Spectral scan: background, trigger, chanscan modes confirmed
- Independent of ath10k mesh radio — no disruption to mesh service
- debugfs: /sys/kernel/debug/ieee80211/phy1/ath9k/spectral_*
- TLV format: type 1 (HT20, 56 bins) and type 2 (HT40, 128 bins)
- Verified on PowerBeam 5AC 500 and Rocket 5AC Lite

## Local validation commands

Run from the repo root:

```sh
sh scripts/check-parser-source-sync.sh
sh -n package/aredn-rfeye/files/usr/sbin/rfeye-agent
sh -n package/aredn-rfeye/files/usr/sbin/rfeye-survey
sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/data/agent.sh
sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/user
cc -Wall -Wextra -o /tmp/rfeye-spectral-parse src/rfeye_spectral_parse.c
sh scripts/test-parser-smoke.sh
sh scripts/test-hardware-fixture-probe.sh
```

## Spectrum flow verification

Use this section to prove that spectrum data is actually flowing from the radio into the browser GUI.

Target flow:

```text
spectral_scan0 -> raw TLV capture -> parser/resync -> normalized frame -> waveform/waterfall/ambient files -> heatmap_bundle JSON -> CGI -> browser GUI
```

### 1. Confirm the installed parser is correct

On the node:

```sh
/usr/lib/rfeye/rfeye-spectral-parse --help | grep -E -- '--probe|--resync'
```

Expected: both `--probe` and `--resync` are present.

If missing, stop. The installed package has the wrong parser.

### 2. Confirm raw RF data exists

```sh
/usr/sbin/rfeye-agent reset
/usr/sbin/rfeye-agent raw_capture_test 12 128 phy0
/usr/sbin/rfeye-agent raw_inspect
```

Expected:

- `bytes_read > 0`
- `frames_emitted > 0`
- `/tmp/rfeye/raw-test.tlv` exists and is non-empty

Interpretation:

- `bytes_read=0`: spectral capture / driver / scan control issue
- `bytes_read>0` and `frames_emitted=0`: parser/framing issue
- `bytes_read>0` and `frames_emitted>0`: raw capture and parser are working

### 3. Confirm parser can decode live data

```sh
/usr/lib/rfeye/rfeye-spectral-parse \
  --resync \
  --stats \
  --input /tmp/rfeye/raw-test.tlv \
  --phy phy0 \
  --limit 10 \
  --bins 64
```

Expected: `frames_emitted > 0`.

This proves raw spectral capture can be decoded into FFT frames.

### 4. Confirm one full backend pipeline cycle

```sh
/usr/sbin/rfeye-agent pipeline_test 128 phy0
/usr/sbin/rfeye-agent pipeline_status
```

Expected signs of success:

- `raw_bytes > 0`
- `parser_frame_present=true`
- `normalized_bins_count=128`
- `waveform_written=true`
- `waterfall_rows >= 1`
- `frames_captured >= 1`
- `last_error=""`

This proves parser output is reaching normalized GUI product files.

### 5. Inspect the product files directly

```sh
ls -lh /tmp/rfeye/waveform.json
ls -lh /tmp/rfeye/waterfall.ndjson
ls -lh /tmp/rfeye/ambient.ndjson
ls -lh /tmp/rfeye/heatmap_bundle.json

cat /tmp/rfeye/waveform.json
tail -5 /tmp/rfeye/waterfall.ndjson
tail -5 /tmp/rfeye/ambient.ndjson
```

Expected:

- `waveform.json` has non-empty `bins`
- `waterfall.ndjson` has rows with bin data
- `ambient.ndjson` has current or historical rows when available
- `heatmap_bundle.json` exists and is non-empty

This proves the GUI source data exists on disk.

### 6. Confirm `heatmap_bundle` has usable data

```sh
/usr/sbin/rfeye-agent heatmap_bundle
```

Expected useful fields:

- `waveform.bins` non-empty
- `waterfall.rows` length greater than zero
- `ambient.rows` or an in-progress/current row when available
- `display_min_dbm`
- `display_max_dbm`
- `waveform_bin_count`
- `waterfall_row_count`
- `frame_rate`
- `last_frame_age_seconds`

This proves the backend bundle is ready for browser use.

### 7. Confirm CGI passes the bundle to the browser

From a machine that can reach the node web server:

```sh
curl 'http://10.188.138.222:8080/cgi-bin/apps/rfeye/data/agent.sh?action=heatmap_bundle'
curl 'http://10.188.138.222:8080/cgi-bin/apps/rfeye/data/agent.sh?action=pipeline_status'
curl 'http://10.188.138.222:8080/cgi-bin/apps/rfeye/data/agent.sh?action=capture_status'
```

Expected:

- HTTP 200
- valid JSON
- same row/frame counts as CLI

If CLI works but CGI does not, the problem is CGI routing or environment.

### 8. Confirm the browser receives data

Open:

```text
http://10.188.138.222:8080/cgi-bin/apps/rfeye/user
```

If possible, check browser developer tools:

- Network tab shows `heatmap_bundle` requests
- Requests return HTTP 200
- Response JSON has `waveform.bins` and `waterfall.rows`
- No JavaScript errors

If CGI JSON has data but the canvas stays blank, the issue is JavaScript rendering/scaling, not RF capture.

### 9. Run a short live GUI/backend capture

On the node:

```sh
/usr/sbin/rfeye-agent reset
/usr/sbin/rfeye-agent start 60 128 phy0
sleep 20
/usr/sbin/rfeye-agent capture_status
/usr/sbin/rfeye-agent pipeline_status
/usr/sbin/rfeye-agent heatmap_bundle
```

In the browser, verify:

- frame count increases
- last frame age updates
- waterfall rows accumulate
- waveform changes or remains visibly stable
- ambient panel shows current/in-progress data when available

Stop safely:

```sh
/usr/sbin/rfeye-agent stop
cat /sys/kernel/debug/ieee80211/phy0/ath10k/spectral_scan_ctl
```

Expected final state: `disable`.

### 10. Spectrum flow decision tree

- `raw_capture_test` has 0 bytes -> spectral_scan0 / driver / scan control issue
- `raw_capture_test` has bytes but 0 frames -> parser/framing issue
- parser emits frames but `pipeline_test` fails -> normalization/product-writer issue
- `pipeline_test` passes but `heatmap_bundle` is empty -> bundle rendering/state assembly issue
- CLI `heatmap_bundle` has data but CGI does not -> CGI bridge issue
- CGI has data but GUI is blank -> browser JavaScript/canvas/scaling issue
- GUI works briefly then stalls -> intermittent capture/feed stall; capture `capture_status`, `pipeline_status`, `heatmap_bundle`, and `raw_capture_test` at the stall moment

## Node triage commands

When a stall is observed, capture state immediately:

```sh
date
uptime
cat /sys/kernel/debug/ieee80211/phy0/ath10k/spectral_scan_ctl 2>/dev/null || true
ps w | grep -E 'rfeye|spectral|dd|parse' | grep -v grep || true
ls -lah /tmp/rfeye
du -ah /tmp/rfeye | sort -h | tail -30

/usr/sbin/rfeye-agent capture_status
/usr/sbin/rfeye-agent pipeline_status
/usr/sbin/rfeye-agent storage_status
/usr/sbin/rfeye-agent acquisition_debug
/usr/sbin/rfeye-agent heatmap_bundle
/usr/sbin/rfeye-agent parser_probe
```

Then test whether raw capture and parser still work:

```sh
/usr/sbin/rfeye-agent raw_capture_test 12 128 phy0
/usr/sbin/rfeye-agent raw_inspect
/usr/sbin/rfeye-agent parser_probe
/usr/lib/rfeye/rfeye-spectral-parse --resync --stats --input /tmp/rfeye/raw-test.tlv --phy phy0 --limit 10 --bins 64
```

Safe reset:

```sh
/usr/sbin/rfeye-agent stop
/usr/sbin/rfeye-agent reset
cat /sys/kernel/debug/ieee80211/phy0/ath10k/spectral_scan_ctl
```

## Interpretation guide

- `raw_capture_test` works after stall: timed capture loop or pipeline state is likely the issue.
- `raw_capture_test` reads bytes but emits 0 frames: live spectral stream may have stalled or changed format.
- `raw_capture_test` reads 0 bytes: spectral scan/debugfs/driver acquisition issue.
- capture PID dead but `capture_status` says running: stale PID/session cleanup bug.
- capture PID alive but frame count frozen: capture loop hang or parser call stall.
- waveform/waterfall files update but GUI does not: CGI/GUI polling issue.

## Suggested next report

Create:

```text
docs/TRIAGE_INTERMITTENT_STALL_KJ6DZB_WSB_ACDISH5.md
```

Include:

- last known good frame count and frame rate
- exact time stall was observed
- `capture_status`
- `pipeline_status`
- `storage_status`
- `acquisition_debug`
- `parser_probe`
- parser help/version check
- raw capture result after stall
- whether capture PID was alive
- whether stale PID/end files existed
- whether reset recovered capture
- final `spectral_scan_ctl` state
- suspected layer
- recommended smallest fix

## Definition of done for next patch

- Triage report completed
- Root layer identified or narrowed
- Any code fix is minimal and justified by evidence
- Parser source sync check remains in validation path
- No channel hopping
- No channel changes
- Captures remain under `/tmp/rfeye`
- Final `spectral_scan_ctl=disable`
