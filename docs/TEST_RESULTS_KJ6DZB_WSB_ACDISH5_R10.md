# RFeye r10 Test Results - KJ6DZB-WSB-ACdish5

Date: 2026-05-25
Node: `KJ6DZB-WSB-ACdish5` / `10.188.138.222:2222`
Web: `http://10.188.138.222:8080`
Route: MSE-88 / `192.168.3.88`

## Package

- Version: `aredn-rfeye 0.1.0-r10`
- Artifact: `artifacts/ipk/aredn-rfeye_0.1.0-r10_mips_24kc.ipk`
- SHA256: `6491834c7a10d572e24327fb4fa5e42656d19812dd3edbeaf4ac5b613f07f11b`

## Local validation

Passed:

```sh
sh -n package/aredn-rfeye/files/usr/sbin/rfeye-agent
sh -n package/aredn-rfeye/files/usr/sbin/rfeye-survey
sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/data/agent.sh
sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/data/download.sh
sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/user
cc -Wall -Wextra -o /tmp/rfeye-spectral-parse src/rfeye_spectral_parse.c
sh scripts/test-parser-smoke.sh
sh scripts/test-hardware-fixture-probe.sh
```

## Node environment

- AREDN: `4.26.1.0 r29087-d9c5716d1d`
- Kernel: `6.6.119`
- Target: `ath79/generic`, arch `mips_24kc`
- Radio: `phy0` / `wlan0`
- Channel/frequency/width: channel 141 / 5705 MHz / 20 MHz
- Mode: IBSS `AREDN-20-v3`

No channel hopping or channel changes were performed.

## Basic checks

Installed package:

```text
aredn-rfeye - 0.1.0-r10
```

`reset` returned `spectral_ctl_after=disable`.

`pipeline_test 128 phy0` passed all stages:

- `capture_raw_bytes=true`
- `parse_resync=true`
- `extract_bins=true`
- `normalize_bins=true`
- `write_latest_frame=true`
- `write_waveform=true`
- `append_waterfall=true`
- `update_ambient=true`
- `render_heatmap_bundle=true`

Pipeline test product counts:

- raw bytes: 262144
- waveform bins: 128
- waterfall rows: 1
- ambient rows: 1
- ring frames: 1

## Manual 5-minute run

Procedure:

```sh
/usr/sbin/rfeye-agent reset
/usr/sbin/rfeye-agent start 300 128 phy0
# sampled at about 60s, 180s, and after completion
/usr/sbin/rfeye-agent capture_status
/usr/sbin/rfeye-agent pipeline_status
/usr/sbin/rfeye-agent storage_status
/usr/sbin/rfeye-agent heatmap_bundle
/usr/sbin/rfeye-agent stop
```

### 60-second checkpoint

- frames captured: 16
- no-frame count: 0
- frame rate: about 16/min
- waterfall rows: 16
- ambient rows: 2
- ring frames: 16
- `/tmp/rfeye` bytes: about 622-680 KiB depending on capture timing
- free memory: about 56 MiB
- load average: about 5.8-6.1

### 180-second checkpoint

- frames captured: 49-50
- no-frame count: 0
- frame rate: about 15/min
- waterfall rows: 49-51
- ambient rows: 4-5
- ring frames: 49-51
- `/tmp/rfeye` bytes: about 708-741 KiB
- free memory: about 55 MiB
- load average: about 5.8-6.0

### Completion checkpoint

- run duration: 300 seconds requested; final status sampled after completion
- frames captured: 80
- no-frame count: 0
- frame rate: about 13.8/min in final `capture_status`; pipeline status during run reported about 16.1/min
- waveform bins: 128
- waterfall rows: 80
- ambient rows/current: 6 rows/current populated
- ring frames: 80
- `/tmp/rfeye` total: `772 KiB` (`state_dir_bytes` about 790-803 KiB)
- latest capture size: `32768` bytes
- `frame_ring.ndjson`: `65242` bytes
- `waterfall.ndjson`: `45079` bytes
- `ambient.ndjson`: `2730` bytes
- `heatmap_bundle.json`: `51248` bytes
- final `spectral_scan_ctl`: `disable`

The waterfall and ring products remained bounded and well under the configured caps (`waterfall_rows=300`, `ring_frames=512`). Capture data remained under `/tmp/rfeye`.

## soak_test command

After fixing the r10 `soak_test` summary quoting and rebuilding, final retest:

```sh
/usr/sbin/rfeye-agent soak_test 300 128 phy0
```

Returned valid JSON:

```json
{
  "ok": true,
  "duration_seconds": 300,
  "frames_captured": 100,
  "no_frame_count": 0,
  "frame_rate": 0.33,
  "latest_capture_bytes": 32768,
  "waterfall_rows": 100,
  "ambient_rows": 6,
  "state_dir_bytes": 823296,
  "final_spectral_ctl": "disable",
  "result": "PASS"
}
```

Final storage after `soak_test`:

- `/tmp/rfeye` bytes: `823296`
- latest.tlv: `32768`
- raw-test.tlv: `262144`
- frame_ring.ndjson: `79635`
- waterfall.ndjson: `54728`
- ambient.ndjson: `2730`
- heatmap_bundle.json: `60904`
- final `spectral_scan_ctl`: `disable`

## CGI and GUI checks

During the manual 5-minute run:

- `agent.sh?action=capture_status` returned valid long-run metrics.
- `agent.sh?action=storage_status` returned valid storage JSON.
- `agent.sh?action=heatmap_bundle` returned waveform/waterfall/ambient products with capture metrics.
- The GUI page was fetched and verified to include the 5-minute selector, elapsed/remaining field, storage field, and `pollBusy` guard to avoid overlapping fetches.

Manual visual browser verification was not performed in this run. Node-side GUI data and page behavior hooks were verified by CGI fetch.

## Memory/storage observations

- `/tmp/rfeye` stayed under 1 MiB in both 5-minute runs.
- `latest.tlv` stayed at 32 KiB snapshots during live capture.
- `frame_ring.ndjson`, `waterfall.ndjson`, and `heatmap_bundle.json` grew with frames but remained bounded by configured caps.
- Free memory stayed around 54-56 MiB in observed checkpoints.
- Load average was around 5-6 during active capture/rendering. This did not wedge capture but should be watched in longer 10-minute/30-minute soaks.

## Result

PASS for r10 5-minute stability target.

## Recommended next stability target

Proceed to a 10-minute soak next. Keep the same safety constraints and watch load average, GUI responsiveness, and `/tmp/rfeye` size. Do not proceed to 30-minute or 1-hour runs until 10-minute behavior is confirmed.
