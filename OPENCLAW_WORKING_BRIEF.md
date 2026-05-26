# OpenClaw Working Brief

## Project

AREDN-RFeye

## Current status

RFeye is a node-side AREDN/OpenWrt RF spectrum visibility prototype for ath10k-based 802.11ac radios.

The project has moved from initial scaffold to a working short-run node prototype. The current priority is reliability and intermittent capture/feed stall triage, not new RF features.

## Active milestone

r12 — parser packaging sync fix and intermittent stall triage

## Guardrails

- No classifier work yet
- No channel hopping
- No channel changes
- Captures remain under `/tmp/rfeye`
- No continuous capture writes to flash
- End state after stop/reset must be `spectral_scan_ctl=disable`
- Do not add new features until the intermittent feed stall is understood

## Known-good state

### r10

r10 is the last fully proven stability milestone.

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

### r11

r11 added GUI and display improvements:

- page scrolling
- compact Controls / Radio / Diagnostics cards
- collapsible raw JSON
- waveform labels and trusted radio frequency context
- waterfall and ambient legends / no-data states
- auto/manual display scaling controls
- improved bundle metadata

r11 source/build validation passed, but live acceptance was intermittent.

## r12 parser packaging fix

A release-blocking issue was found after r11.

The package build had been compiling the stale parser source here:

```text
package/aredn-rfeye/src/rfeye_spectral_parse.c
```

instead of the newer canonical parser here:

```text
src/rfeye_spectral_parse.c
```

The stale packaged parser did not support:

```text
--probe
--resync
```

while `rfeye-agent` expected the resync-capable parser path.

r12 fixed this by syncing the package parser source from the canonical parser source and adding a source-sync validation check.

Before building any IPK, run:

```sh
sh scripts/check-parser-source-sync.sh
```

The installed parser on the node must show `--probe` and `--resync` in help output.

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

## Immediate next work

1. Finish the intermittent stall triage report.
2. Capture current state when the stall occurs:
   - `capture_status`
   - `pipeline_status`
   - `storage_status`
   - `acquisition_debug`
   - `parser_probe`
   - `raw_capture_test`
3. Determine whether the stall is spectral-source, parser, loop, product-writer, or GUI/API related.
4. Decide whether a minimal watchdog/re-prime fix is warranted.
5. Avoid feature work until this is understood.

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

## Useful commands

Parser sync:

```sh
sh scripts/check-parser-source-sync.sh
```

Local validation:

```sh
sh -n package/aredn-rfeye/files/usr/sbin/rfeye-agent
sh -n package/aredn-rfeye/files/usr/sbin/rfeye-survey
sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/data/agent.sh
sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/user
cc -Wall -Wextra -o /tmp/rfeye-spectral-parse src/rfeye_spectral_parse.c
sh scripts/test-parser-smoke.sh
sh scripts/test-hardware-fixture-probe.sh
```

Node triage:

```sh
/usr/sbin/rfeye-agent capture_status
/usr/sbin/rfeye-agent pipeline_status
/usr/sbin/rfeye-agent storage_status
/usr/sbin/rfeye-agent acquisition_debug
/usr/sbin/rfeye-agent heatmap_bundle
/usr/sbin/rfeye-agent raw_capture_test 12 128 phy0
/usr/sbin/rfeye-agent parser_probe
cat /sys/kernel/debug/ieee80211/phy0/ath10k/spectral_scan_ctl
```

Safe reset:

```sh
/usr/sbin/rfeye-agent stop
/usr/sbin/rfeye-agent reset
cat /sys/kernel/debug/ieee80211/phy0/ath10k/spectral_scan_ctl
```

## Definition of done for next patch

- Triage report completed
- Root layer identified or narrowed
- Any code fix is minimal and justified by evidence
- Parser source sync check remains in validation path
- No channel hopping
- No channel changes
- Captures remain under `/tmp/rfeye`
- Final `spectral_scan_ctl=disable`
