# OpenClaw Working Brief

## Project

AREDN-RFeye

RFeye is a node-side AREDN/OpenWrt RF spectrum visibility prototype for ath10k-based 802.11ac radios. The project goal is an AirView-like node test view, with heavier analysis and reporting deferred to a future Linux Workbench.

## Start here

Do **not** add new RF features yet.

The next OpenClaw session should focus on the intermittent capture/feed stall. r12 already fixed the stale packaged-parser issue, so do not spend time re-litigating the r11 parser mismatch unless the node parser again lacks `--probe` / `--resync`.

Immediate next action:

1. Confirm parser source sync.
2. Confirm installed parser supports `--probe` and `--resync`.
3. Reproduce or catch the intermittent stall.
4. Save a triage report.
5. Only then decide whether to add a minimal watchdog/re-prime fix.

## Active milestone

r12 complete; intermittent capture/feed stall triage in progress.

## Guardrails

- No classifier work yet
- No channel hopping
- No channel changes
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
