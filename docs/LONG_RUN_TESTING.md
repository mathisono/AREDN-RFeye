# RFeye Long-Run Testing

RFeye long-run tests are intended to prove that live waveform/waterfall/ambient products remain bounded and useful without changing channel settings or writing continuous data to flash.

## Safety rules

- Do not enable channel hopping.
- Do not change channel settings.
- Capture files must remain under `/tmp/rfeye`.
- Do not write captures or exports to flash on the node.
- Stop immediately if memory, CPU/load, or `/tmp` usage grows unexpectedly.
- Always verify `spectral_scan_ctl` returns to `disable` after stop/reset.

## 5-minute r10 test

CLI procedure:

```sh
/usr/sbin/rfeye-agent reset
/usr/sbin/rfeye-agent start 300 128 phy0
sleep 60
/usr/sbin/rfeye-agent capture_status
/usr/sbin/rfeye-agent pipeline_status
/usr/sbin/rfeye-agent storage_status
/usr/sbin/rfeye-agent heatmap_bundle
sleep 120
/usr/sbin/rfeye-agent capture_status
/usr/sbin/rfeye-agent pipeline_status
/usr/sbin/rfeye-agent storage_status
/usr/sbin/rfeye-agent heatmap_bundle
sleep 140
/usr/sbin/rfeye-agent capture_status
/usr/sbin/rfeye-agent pipeline_status
/usr/sbin/rfeye-agent storage_status
/usr/sbin/rfeye-agent heatmap_bundle
/usr/sbin/rfeye-agent stop
cat /sys/kernel/debug/ieee80211/phy0/ath10k/spectral_scan_ctl
```

Wrapper procedure:

```sh
/usr/sbin/rfeye-agent soak_test 300 128 phy0
```

Expected summary fields include `duration_seconds`, `frames_captured`, `no_frame_count`, `frame_rate`, `latest_capture_bytes`, `waterfall_rows`, `ambient_rows`, `state_dir_bytes`, `final_spectral_ctl`, and `result`.

## GUI procedure

Open:

```text
http://localnode.local.mesh/cgi-bin/apps/rfeye/user
```

Select `5min`, start capture, and watch:

- GUI remains responsive.
- Waveform updates.
- Waterfall rows accumulate but remain capped by `waterfall_rows`.
- Ambient current row persists or ambient rows update.
- Frame count increases when spectral data is available.
- No-frame count does not clear the display.
- Elapsed/remaining time advance.
- Storage usage remains bounded.

## Future tests

After r10 5-minute stability passes, repeat the same checkpoints for:

- 10 minutes
- 30 minutes
- 1 hour
- 6 hours

Increase duration one step at a time. Do not skip to long soaks if 5-minute or 10-minute behavior is unstable.

## What to watch

- `frames_captured` and frame rate: should be nonzero when spectral samples are available.
- `no_frame_count`: occasional no-frame cycles are acceptable; continuous growth may indicate acquisition or parser issues.
- `waterfall_rows`: should cap at configured `waterfall_rows`, not grow forever.
- `ring_frames`: should cap at configured `ring_frames`.
- `/tmp/rfeye` total bytes: should remain bounded.
- `latest.tlv`: should stay near the configured snapshot size.
- `heatmap_bundle.json`: should remain small enough for GUI polling.
- `free_mem_kb` and `load_average`: should not trend badly over the run.

## Expected files under `/tmp/rfeye`

Common files:

- `latest.tlv`
- `latest_frame.json`
- `latest_parser_frame.json`
- `frame_ring.ndjson`
- `waterfall.ndjson`
- `ambient.ndjson`
- `ambient.work`
- `waveform.json`
- `heatmap_bundle.json`
- `pipeline-status.json`
- `session.json`
- `radio.json`
- debug files such as `debug-last-cycle.tlv`, `debug-last-parser.txt`, and `debug-last-stats.json`

## Result interpretation

- **PASS**: 5-minute run completes, GUI remains responsive, products stay bounded, storage is measured, and final `spectral_scan_ctl` is `disable`.
- **PARTIAL**: run completes but frame count is low/intermittent or GUI was not manually verified; diagnostics are clear and spectral state is safe.
- **FAIL**: capture wedges, storage grows unexpectedly, GUI freezes, or spectral state does not return to `disable`.
