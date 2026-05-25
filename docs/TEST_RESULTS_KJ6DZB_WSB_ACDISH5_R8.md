# RFeye r8 Test Results - KJ6DZB-WSB-ACdish5

Date: 2026-05-25
Node: `KJ6DZB-WSB-ACdish5` / `10.188.138.222:2222`
Web: `http://10.188.138.222:8080`
Route: MSE-88 / `192.168.3.88`

## Package

- Version: `aredn-rfeye 0.1.0-r8`
- Artifact: `artifacts/ipk/aredn-rfeye_0.1.0-r8_mips_24kc.ipk`
- SHA256: `43b6fb8f5728905c206ebc09df8cfb549c1b0ffce5d77eb1aad41e4a43baa0e6`

## Local validation

Passed:

```sh
sh -n package/aredn-rfeye/files/usr/sbin/rfeye-agent
sh -n package/aredn-rfeye/files/usr/sbin/rfeye-survey
sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/data/agent.sh
sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/user
cc -Wall -Wextra -o /tmp/rfeye-spectral-parse src/rfeye_spectral_parse.c
sh scripts/test-parser-smoke.sh
sh scripts/test-hardware-fixture-probe.sh
```

Parser smoke test passed; fixture probe selected `tlv3_be` and emitted 4 frames.

## Node validation

Installed successfully:

```text
aredn-rfeye - 0.1.0-r8
```

Radio info remained unchanged; no channel hopping or channel changes were performed:

```json
{"channel":141,"frequency_mhz":5705,"width_mhz":20,"mode":"IBSS"}
```

### raw_capture_test

Command:

```sh
/usr/sbin/rfeye-agent raw_capture_test 10 128 phy0
```

Result:

```json
{"ok":true,"bytes_read":262144,"frames_emitted":1,"trailing_truncated":false}
```

### pipeline_test

Command:

```sh
/usr/sbin/rfeye-agent pipeline_test 128 phy0
```

Result summary:

```json
{
  "ok": true,
  "raw_bytes": 262144,
  "parser_exit": 0,
  "parser_frame_present": true,
  "frames_emitted": 1,
  "stages": {
    "capture_raw_bytes": true,
    "parse_resync": true,
    "extract_bins": true,
    "normalize_bins": true,
    "write_latest_frame": true,
    "write_waveform": true,
    "append_waterfall": true,
    "update_ambient": true,
    "render_heatmap_bundle": true
  },
  "pipeline": {
    "normalized_bins_count": 128,
    "waveform_bins": 128,
    "waterfall_rows": 1,
    "ambient_rows": 1,
    "frames_captured": 1,
    "last_error": ""
  }
}
```

### pipeline_status

After `pipeline_test`:

```json
{
  "ok": true,
  "raw_bytes": 262144,
  "parser_exit": 0,
  "parser_frame_present": true,
  "parser_frame_bytes": 323,
  "bins_extracted": true,
  "normalized_bins_count": 128,
  "waveform_written": true,
  "waveform_bins": 128,
  "waterfall_rows": 1,
  "ambient_rows": 1,
  "frames_captured": 1,
  "last_error": ""
}
```

### heatmap_bundle counts

After `pipeline_test`, `heatmap_bundle` contained:

- `waveform.bins`: 128
- `waterfall.rows`: 1
- `ambient.rows`: 1 with `pending=true` and `current` populated
- `pipeline.normalized_bins_count`: 128
- `pipeline.last_error`: empty

### timed start loop

Commands:

```sh
/usr/sbin/rfeye-agent reset
/usr/sbin/rfeye-agent start 10 128 phy0
sleep 12
/usr/sbin/rfeye-agent capture_status
/usr/sbin/rfeye-agent pipeline_status
/usr/sbin/rfeye-agent heatmap_bundle
/usr/sbin/rfeye-agent waveform
/usr/sbin/rfeye-agent waterfall
/usr/sbin/rfeye-agent ambient
/usr/sbin/rfeye-agent stop
cat /sys/kernel/debug/ieee80211/phy0/ath10k/spectral_scan_ctl
```

Result summary:

- `capture_status.capture.frames_captured`: 1
- `capture_status.capture.bytes_written`: 16384
- `pipeline_status.raw_bytes`: 16384
- `pipeline_status.parser_frame_present`: true
- `pipeline_status.normalized_bins_count`: 128
- `pipeline_status.waveform_bins`: 128
- `pipeline_status.waterfall_rows`: 1
- `pipeline_status.ambient_rows`: 1
- `heatmap_bundle.waveform.bins`: non-empty
- `heatmap_bundle.waterfall.rows`: non-empty
- `ambient.current`: populated
- final `spectral_scan_ctl`: `disable`

Timed capture uses a smaller per-cycle snapshot under `/tmp/rfeye` so the first product update completes during the 10-second timed run; raw diagnostic capture remains 262144 bytes.

### CGI tests

Tested through MSE-88 with curl:

```sh
curl 'http://10.188.138.222:8080/cgi-bin/apps/rfeye/data/agent.sh?action=pipeline_status'
curl 'http://10.188.138.222:8080/cgi-bin/apps/rfeye/data/agent.sh?action=pipeline_test'
curl 'http://10.188.138.222:8080/cgi-bin/apps/rfeye/data/agent.sh?action=heatmap_bundle'
```

Results:

- `pipeline_status`: valid JSON, `parser_frame_present=true`, `waveform_bins=128`, `waterfall_rows=1`, `ambient_rows=1`.
- `pipeline_test`: valid JSON, all stages true, `waveform_bins=128`, `waterfall_rows=2`, `ambient_rows=2`.
- `heatmap_bundle`: valid JSON with non-empty waveform and waterfall data. The local `head -c` truncation caused curl to report an expected client-side `(23) Failed writing body`; the CGI returned data before truncation.

## Result

PASS.

r8 connects decoded resync parser frames into waveform, waterfall, ambient, and heatmap_bundle products. No classifier work, channel hopping, or channel changes were added. Captures and products remain under `/tmp/rfeye`, and `spectral_scan_ctl` ended at `disable`.

## Next blocker

None for the r8 plumbing objective. Future work, if requested separately, can address UI presentation or richer product interpretation, but those were intentionally out of scope for r8.
