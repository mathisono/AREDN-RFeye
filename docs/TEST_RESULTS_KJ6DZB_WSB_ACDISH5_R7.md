# RFeye r7 Test Results — KJ6DZB-WSB-ACdish5

- Date: 2026-05-24 PDT
- Node: `10.188.138.222`
- Jump host: `192.168.3.88` (MSE-88)
- Package: `aredn-rfeye 0.1.0-r7`
- IPK SHA256: `6832ffceaf88e0e15d10b2739f6809cb2aaf10cc05973fa2bc04455707e9419d`

## Summary

- `raw_capture_test`: `bytes_read=262144`, `frames_emitted=1`
- Raw file is mostly nonzero: `229701` nonzero bytes vs `32443` zero bytes
- `parser_probe`: valid JSON with best framing `tlv3_be`
- `--resync --stats`: `frames_emitted=10`, `skipped_bytes=88`
- `heatmap_bundle`: waveform/waterfall still empty in timed run
- `spectral_scan_ctl` final state: `disable`
- Result: **PARTIAL PASS**

## Raw capture

- Raw file size: `262144`
- First 64 hex bytes:

`03030300040601030704020304040506020602050300010306040205040401030401020001060405010400020301030101020704000102020703010003030102`

## Probe diagnosis

`/usr/sbin/rfeye-agent parser_probe` and direct parser probe both reported:

- `probe_scan_window`: `65536`
- `best_guess`: `tlv3_be`
- `candidate_records`: `60329`
- `type3_candidate_records`: `50`
- `type_counts`: many values (not only type 3), consistent with mixed payload bytes and repeated framing

Type 3 TLVs exist: **Yes**.

## Resync decode

`/usr/lib/rfeye/rfeye-spectral-parse --resync --stats --input /tmp/rfeye/raw-test.tlv --phy phy0 --limit 10 --bins 64`

- `frames_emitted=10`
- `skipped_bytes=88`
- Frame JSON emitted successfully (no crash)
- Bins are nonzero with visible variance: **plausible**

## Timed agent capture/heatmap check

Commands run:

- `/usr/sbin/rfeye-agent start 10 128 phy0`
- `sleep 12`
- `/usr/sbin/rfeye-agent heatmap_bundle`
- `/usr/sbin/rfeye-agent stop`

Observed:

- `acquisition_debug.frames_emitted=1`
- `waveform.bins=[]`
- `waterfall.rows=[]`
- `capture.error="no parsed frames"`

So parser can decode from raw capture test/resync, but timed loop still does not populate plotted rows.

## Final status and next fix

- **Status:** PARTIAL PASS
- **Primary r7 goal met:** parser probe/resync now identifies usable framing and emits frames from real node capture.
- **Remaining blocker:** capture loop/session-to-heatmap path is still dropping parsed frame output even when parser stats show emitted frames.
- **Next recommended fix:** instrument and correct `capture_once`/session write path so parsed frame JSON from `capture_cycle_sequence` is persisted into `latest_parser_frame.json`, waveform bins, and waterfall rows during timed runs.
