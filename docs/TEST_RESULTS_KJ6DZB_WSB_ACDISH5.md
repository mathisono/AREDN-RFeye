# AREDN RFeye Test Results: KJ6DZB-WSB-ACdish5

## Node

- Host/IP: `10.188.138.222`
- SSH port: `2222`
- AREDN/OpenWrt: `AREDN 4.26.1.0 r29087-d9c5716d1d`
- Kernel: `6.6.119` on `mips`
- Node hostname: `KJ6DZB-WSB-ACdish5`
- Radio: `phy0` / `wlan0` / IBSS `AREDN-20-v3`
- Channel: 141 / 5705 MHz / 20 MHz

## Result

PARTIAL PASS.

## Working

- Package installed.
- Installed files exist and are executable.
- ath10k spectral files are present.
- debugfs is mounted.
- CLI `rfeye-agent status` returns JSON.
- CLI `rfeye-agent snapshot` returns a JSON FFT frame.
- 10-second capture creates `/tmp/rfeye/latest.tlv` around 23 KB.
- Parser reads `latest.tlv` and emits JSON frames.
- GUI page loads.
- CGI status and survey endpoints return HTTP 200 JSON.
- Mesh stayed stable.
- `spectral_scan_ctl` returned to `disable` after testing.

## Issues found

1. `rfeye-survey survey` returned valid JSON but all counters were zero.
2. `rfeye-survey utilization` returned an unclear invalid survey delta error.
3. A 5-second capture did not create `latest.tlv` by the check time; a 10-second retry worked.
4. CGI snapshot returned `no frame` while direct CLI snapshot worked.
5. Parser emitted valid frames but also printed `truncated TLV payload`.

## Patch focus from this result

- Return valid JSON for zero/unchanged survey counters.
- Add raw survey dump JSON endpoint for field debugging.
- Add `rfeye-agent capture_status` and CGI `action=capture_status`.
- Write captures to `/tmp/rfeye/latest.tlv.tmp` and rename on completion.
- Improve snapshot retries and diagnostics.
- Treat a truncated trailing TLV as a warning if at least one frame was emitted.

## Safety notes

- No channel settings were changed.
- No automatic channel hopping was enabled.
- Capture data stayed under `/tmp/rfeye`.
