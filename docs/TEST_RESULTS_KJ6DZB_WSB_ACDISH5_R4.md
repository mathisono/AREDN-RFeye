# RFeye r4 Node Test Report — KJ6DZB-WSB-ACdish5

Date: 2026-05-25

## Node info

- Node: `KJ6DZB-WSB-ACdish5`
- Target: `10.188.138.222:2222`
- Access path used: SSH jump host `MSE-88` (`192.168.3.88`, user `mat`)
- Web: `http://10.188.138.222:8080`

## Package under test

- File: `artifacts/ipk/aredn-rfeye_0.1.0-r4_mips_24kc.ipk`
- SHA256: `2489b5f37a23ae084770eb9cf11559617fabf218af3948625b8e26734a0aa577`
- Source commit: `511c9ab`

## Install result

- Installed via jump host path:
  - `opkg install --force-reinstall /tmp/aredn-rfeye_0.1.0-r4_mips_24kc.ipk`
- Result: success (`aredn-rfeye (0.1.0-r4)` reinstalled/configured).

## Connectivity check from MSE-88

- `ping 10.188.138.222`: success (4/4).
- `traceroute 10.188.138.222`: reaches node in 2 hops.
- `ip route get 10.188.138.222`: route via `10.245.94.33`.
- `curl -I --max-time 5 http://10.188.138.222:8080/`: HTTP redirect response received.

## CLI results

Executed:

- `/usr/sbin/rfeye-agent status` ✅
- `/usr/sbin/rfeye-agent radio_info` ✅
  - trusted radio info remained correct: channel `141`, frequency `5705 MHz`, width `20 MHz`.
- `/usr/sbin/rfeye-agent reset` ✅ (`spectral_ctl_after":"disable"`)
- `/usr/sbin/rfeye-agent start 10 128 phy0` ✅
- `/usr/sbin/rfeye-agent capture_status` during and after run ✅
- `/usr/sbin/rfeye-agent heatmap_bundle` ✅ valid JSON
- `/usr/sbin/rfeye-agent waveform` ✅ valid JSON
- `/usr/sbin/rfeye-agent waterfall` ✅ valid JSON
- `/usr/sbin/rfeye-agent ambient` ✅ valid JSON
- `/usr/sbin/rfeye-agent stop` ✅
- `cat .../spectral_scan_ctl` final: `disable` ✅

Observed in this run:
- captures completed without crashing, but `frames_captured` stayed `0` and heatmap rows were empty.
- repeated short start/stop cycles (3x) worked.
- during repeated stop, shell warning appeared:
  - `/usr/sbin/rfeye-agent: line 556: err: parameter not set`

## CGI results

From MSE-88:

- `action=radio_info` ✅ valid JSON
- `action=ui_state` ✅ valid JSON
- `action=heatmap_bundle` ✅ valid JSON
- `action=waveform` ✅ valid JSON
- `action=waterfall` ✅ valid JSON
- `action=ambient` ✅ valid JSON

## GUI check

- `curl -I http://10.188.138.222:8080/cgi-bin/apps/rfeye/user` returned `HTTP/1.1 200 OK`.
- Browser visual verification of panel rendering was not performed in this CLI-only retest.

## Heatmap behavior

- Endpoint structure is correct and stable.
- In this node run, no FFT frames were captured, so waveform/waterfall/ambient bins/rows remained empty.
- UI JSON remained alive/valid (no fatal break on no-frame condition).

## Repeated start/stop

- Performed at least 3 cycles.
- State transitions were stable (`running` -> `complete`).
- `spectral_scan_ctl` ended in `disable` after stop/reset.

## Safety checks

- No channel hopping observed.
- No channel changes observed.
- Capture/session data remained under `/tmp/rfeye`.

## Overall

- **PARTIAL PASS**

### Main blocker

- Node still returns zero captured frames in these runs, so heatmaps are structurally correct but contain no accumulated frame data.
- Minor shell robustness bug in stop path (`err` unbound variable warning) should be fixed next.
