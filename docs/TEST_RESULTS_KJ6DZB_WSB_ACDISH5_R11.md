# RFeye r11 Test Results — KJ6DZB-WSB-ACdish5

Date: 2026-05-25

## Package

- Version: `aredn-rfeye_0.1.0-r11_mips_24kc.ipk`
- SHA256: `38c8fff8b508dedb731665d23b57d340dbae5a0550b070cd1854acd725cf5aac`

## GUI/UX change results (code + local validation)

- GUI scroll behavior (desktop/mobile CSS): **Implemented**
- Compact controls/radio/diagnostics cards: **Implemented**
- Waveform panel scale/labels/center marker/no-data: **Implemented**
- Waterfall panel contrast/legend/row labeling/no-data: **Implemented**
- Ambient panel labeling/current-row context/no-data: **Implemented**
- Display scaling (auto/manual/reset): **Implemented**
- Raw JSON collapsed by default: **Implemented**
- Polling overlap guard: **Implemented**

## Pre-build validation

- `sh -n package/aredn-rfeye/files/usr/sbin/rfeye-agent` ✅
- `sh -n package/aredn-rfeye/files/usr/sbin/rfeye-survey` ✅
- `sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/data/agent.sh` ✅
- `sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/user` ✅
- `cc -Wall -Wextra -o /tmp/rfeye-spectral-parse src/rfeye_spectral_parse.c` ✅
- `sh scripts/test-parser-smoke.sh` ✅
- `sh scripts/test-hardware-fixture-probe.sh` ✅

## Node retest through MSE-88

Target:
- node: `10.188.138.222`
- ssh: `:2222`
- web: `http://10.188.138.222:8080`
- jump: `MSE-88 / 192.168.3.88`

Status in this run: **BLOCKED** (node access unavailable from this environment).

Access attempts:
- `ssh -p 2222 root@10.188.138.222` → timeout
- `ssh -J root@192.168.3.88 -p 2222 root@10.188.138.222` → jump-host auth failure

Pending node metrics:
- 5-minute GUI run result
- frames captured
- waterfall rows
- storage usage
- final `spectral_scan_ctl` state
- screenshots path

## Result

**PARTIAL**

- Package build and local validation passed.
- On-node r11 install/retest results remain pending.
