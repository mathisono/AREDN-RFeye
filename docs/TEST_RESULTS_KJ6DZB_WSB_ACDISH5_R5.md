# RFeye r5 Node Test Report — KJ6DZB-WSB-ACdish5

Date: 2026-05-25

## Node info

- Node: `KJ6DZB-WSB-ACdish5`
- Target: `10.188.138.222:2222`
- Access path used: SSH jump host `MSE-88` (`192.168.3.88`, user `mat`)
- Web: `http://10.188.138.222:8080`

## Package under test

- File: `artifacts/ipk/aredn-rfeye_0.1.0-r5_mips_24kc.ipk`
- SHA256: `b1a76b6723754cfc9571eb21c2c3d53c0b6a0acbeab7fb8df1c7f1e53a098043`
- Source commit: `8fbc87f`

## Install result

- Installed via jump host path:
  - `opkg install --force-reinstall /tmp/aredn-rfeye_0.1.0-r5_mips_24kc.ipk`
- Result: success (`aredn-rfeye (0.1.0-r5)` reinstalled/configured).

## Connectivity check from MSE-88

- `ping 10.188.138.222`: success.
- `traceroute 10.188.138.222`: reaches node in 2 hops.
- `ip route get 10.188.138.222`: route present.
- `curl -I --max-time 5 http://10.188.138.222:8080/`: HTTP response received.

## CLI results

Executed:

- `/usr/sbin/rfeye-agent reset` ✅
- `/usr/sbin/rfeye-agent radio_info` ✅
  - trusted radio info remained correct: channel `141`, frequency `5705 MHz`, width `20 MHz`.
- `/usr/sbin/rfeye-agent raw_capture_test 10 128 phy0` ✅ valid JSON, but no parsed frames.
  - `bytes_read`: `262144`
  - `frames_emitted`: `0`
- `/usr/sbin/rfeye-agent acquisition_debug` ✅ valid JSON
- `/usr/sbin/rfeye-agent start 10 128 phy0` ✅
- `/usr/sbin/rfeye-agent capture_status` after run ✅
- `/usr/sbin/rfeye-agent heatmap_bundle` ✅ valid JSON
- `/usr/sbin/rfeye-agent stop` ✅
- `cat .../spectral_scan_ctl` final: `disable` ✅

Observed:
- Capture loop still produced zero parsed frames.
- `capture` JSON now clearly reports `error: "no parsed frames"`.
- `acquisition_debug` reports debug file paths and parser/ctl presence.

## CGI results

From MSE-88:

- `action=acquisition_debug` ✅ valid JSON
- `action=heatmap_bundle` ✅ valid JSON
- GUI HEAD request returned `HTTP/1.1 200 OK` ✅

## GUI observations

- HTML endpoint still returns successfully.
- Browser-level visual confirmation was not performed from CLI.
- Debug/heatmap JSON endpoints are stable and structurally correct.

## Heatmap behavior

- When no frames are parsed, `heatmap_bundle` now reports the zero-frame state clearly.
- Waveform/waterfall/ambient rows remain empty until frames are available.
- `acquisition_debug` exposes diagnostic file locations and parser availability.

## Repeated start/stop

- Start/stop remains repeatable.
- `spectral_scan_ctl` ends at `disable` after stop/reset.

## Overall

- **PARTIAL PASS**

## Main blocker

- Acquisition still produces zero parsed frames on this node, even in `raw_capture_test`, so the next step is deeper capture-path/driver timing investigation.
