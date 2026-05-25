# RFeye r9 Test Results - KJ6DZB-WSB-ACdish5

Date: 2026-05-25
Node: `KJ6DZB-WSB-ACdish5` / `10.188.138.222:2222`
Web: `http://10.188.138.222:8080`
Route: MSE-88 / `192.168.3.88`

## Package

- Version: `aredn-rfeye 0.1.0-r9`
- Artifact: `artifacts/ipk/aredn-rfeye_0.1.0-r9_mips_24kc.ipk`
- SHA256: `ec25e05fab06640e96514642391058986681ce533879e73ccad9e130a90f7638`

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

## Node validation

Installed successfully:

```text
aredn-rfeye - 0.1.0-r9
```

Radio stayed on the existing channel; no channel hopping or channel changes were performed:

```json
{"channel":141,"frequency_mhz":5705,"width_mhz":20,"mode":"IBSS"}
```

### raw_capture_test

```json
{"ok":true,"bytes_read":262144,"frames_emitted":1,"trailing_truncated":false}
```

### pipeline_test

```json
{
  "ok": true,
  "raw_bytes": 262144,
  "parser_frame_present": true,
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

### 10-second timed loop

Final r9 quick retest:

```json
{
  "capture_status": {
    "frames_captured": 8,
    "bytes_written": 32768,
    "no_frame_count": 0
  },
  "capture_metrics": {
    "frames_per_second": 0.25,
    "frames_per_minute": 15.0,
    "sample_interval_ms": 1000,
    "snapshot_bytes": 16384,
    "parser_mode": "resync"
  },
  "pipeline_status": {
    "raw_bytes": 16384,
    "parser_frame_present": true,
    "normalized_bins_count": 128,
    "waveform_bins": 128,
    "waterfall_rows": 7,
    "ambient_rows": 1,
    "frames_captured": 7,
    "last_error": ""
  }
}
```

The primary r9 target was met: a 10-second timed capture produced more than 3 valid frames when spectral data was available.

### Repeated start/stop

Three repeated start/stop cycles completed without wedging capture. Earlier full-run repeats produced 2 frames per 10-second run before the final multi-frame-per-capture optimization; final quick retest after that optimization produced 8 frames. All observed final `spectral_scan_ctl` checks returned `disable`.

### Product counts

- Waveform bin count: 128
- Waterfall rows: 7 in final timed-loop pipeline status
- Ambient: current row populated; ambient row count 1
- `heatmap_bundle.waveform.bins`: non-empty
- `heatmap_bundle.waterfall.rows`: non-empty and accumulates multiple rows

### Export/download tests

CLI:

- `export_tlv_info`: returned `/tmp/rfeye/latest.tlv`, `exists=true`, size `16384`, and SHA256 when `sha256sum` was available.
- `export_jsonl | head`: returned parser JSONL frames.

CGI:

```sh
curl 'http://10.188.138.222:8080/cgi-bin/apps/rfeye/data/agent.sh?action=capture_status'
curl 'http://10.188.138.222:8080/cgi-bin/apps/rfeye/data/agent.sh?action=pipeline_status'
curl 'http://10.188.138.222:8080/cgi-bin/apps/rfeye/data/agent.sh?action=heatmap_bundle'
curl -I 'http://10.188.138.222:8080/cgi-bin/apps/rfeye/data/download.sh?type=tlv'
curl -I 'http://10.188.138.222:8080/cgi-bin/apps/rfeye/data/download.sh?type=jsonl'
```

Results:

- `capture_status`: valid JSON with frame cadence metrics.
- `pipeline_status`: valid JSON with `parser_frame_present=true`, `waveform_bins=128`, `waterfall_rows=7`.
- `heatmap_bundle`: valid JSON with capture metrics and product data. Local `head -c` truncation caused expected curl `(23) Failed writing body` after data was received.
- `download.sh?type=tlv`: HTTP 200, `Content-Type: application/octet-stream`.
- `download.sh?type=jsonl`: HTTP 200, `Content-Type: application/x-ndjson`.
- Agent actions `download_tlv` and `download_jsonl` also returned HTTP 200 headers.

### GUI observations

The GUI was updated to display:

- Capture state
- Frames captured
- No-frame count
- Last frame age
- Frame rate
- Capture progress
- Sample interval / snapshot bytes
- Capture bytes
- Parser mode

The GUI preserves the last visible waveform/waterfall/ambient data while waiting for the next frame and shows a waiting/live status instead of clearing panels on a single no-frame condition. Browser manual verification was not performed in this run, but the updated CGI data needed by the GUI was validated.

### Final spectral state

Final `spectral_scan_ctl` after stop/reset: `disable`.

## Result

PASS.

r9 improved live usability and cadence. The final timed 10-second retest produced 8 captured frames and waterfall rows accumulated. Export/download diagnostics worked. No classifier work, channel hopping, channel changes, or flash-based long-term storage were added; capture data remains under `/tmp/rfeye`.

## Next blocker

Manual browser GUI visual verification can still be performed, but the node-side JSON/CGI data and status fields needed for the live display passed.
