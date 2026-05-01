# AREDN RFeye Architecture (Node + Linux Workbench)

## Core design goal

AREDN RFeye is a **two-part system**:

1. **AREDN node package** (sensor + safe control + basic UI)
2. **Linux RFeye Workbench** (deep analysis + replay + storage)

The node must stay small and conservative. Heavy analysis belongs on Linux.

---

## Part 1: AREDN node package

### Scope

- Detect ath10k spectral capability.
- Start/stop spectral safely on current channel.
- Collect survey counters (`active`, `busy`, `tx`, `rx`, `noise`).
- Provide low-rate snapshots for node UI.
- Provide short timed captures for export.

### Required footprint

Keep dependencies minimal:

- shell/ucode/C preferred
- no Python runtime requirement on node
- no heavy JS framework
- no DB

### Node components

- `/usr/bin/rfeye-probe`
- `/usr/sbin/rfeye-agent` (future; aka `spectrald`)
- `/etc/init.d/rfeye`
- `/etc/config/rfeye`
- `/www/cgi-bin/apps/rfeye/user`
- `/www/cgi-bin/apps/rfeye/admin`
- `/www/cgi-bin/apps/rfeye/data`
- `/www/apps/rfeye/icon.svg`

### Safety rules

- Protect mesh service first.
- Current-channel/background mode by default.
- No automatic channel hopping on production radio.
- Time-limit all captures.
- Throttle browser output.
- Keep high-rate capture in RAM only.
- Fail closed when spectral files are missing.

### Acceptance tests (node)

- `spectral_scan0` yields non-empty TLV data.
- `spectral_bins` settable to 128 or 256.
- start/stop scan does not destabilize Wi-Fi.
- survey counters produce usable deltas.

---

## Part 2: Linux RFeye Workbench

### Scope

- Full waterfall and FFT tooling.
- Replay of saved captures.
- Classifier tuning and experimentation.
- Long-term storage and reporting.

### Suggested stack

- Python or Go backend
- HTML/JS frontend
- WebSocket for live frames
- SQLite or flat files for sessions
- pytest (or equivalent) fixtures/tests

### Capture layout

```text
captures/
  2026-04-30-node-k6abc-5745mhz/
    metadata.json
    spectral.raw.tlv
    spectral.normalized.jsonl
    survey.jsonl
    notes.md
```

---

## Data paths

### Path A: low-rate JSON snapshots (node UI)

Used by AREDN app UI at ~1–4 FPS.

```json
{
  "phy": "phy0",
  "driver": "ath10k",
  "timestamp_tsf": 123456789,
  "freq1_mhz": 5745,
  "freq2_mhz": 0,
  "width_mhz": 80,
  "noise": -96,
  "rssi": 42,
  "max_index": 115,
  "max_magnitude": 900,
  "avgpwr_db": 38,
  "relpwr_db": 20,
  "bins": [12, 13, 15, 18]
}
```

### Path B: timed raw capture export (Linux)

For deep analysis and replay.

Examples:

- `/cgi-bin/apps/rfeye/capture?seconds=10&format=tlv`
- `/cgi-bin/apps/rfeye/capture?seconds=10&format=jsonl`

Future ubus:

- `ubus call spectrald capture_start '{"seconds":10,"format":"tlv"}'`

---

## Service API target (future)

- `spectrald.status`
- `spectrald.start`
- `spectrald.stop`
- `spectrald.snapshot`
- `spectrald.capture_start`
- `spectrald.capture_stop`
- `spectrald.survey`
- `spectrald.classifiers`

---

## UI split

### Node UI answers

- Is scan supported?
- Is RF energy present?
- Is channel busy/noisy?
- Is non-Wi-Fi interference likely?
- Should operator investigate/move channel?

### Linux UI answers

- Exact spectral shape and motion over time
- replay comparisons
- multi-site/channel analysis
- classifier tuning
- exportable reports/screenshots

---

## Classification split

### On-node (simple, explainable)

- Wi-Fi-like OFDM
- narrowband fixed carrier
- wideband high-duty interferer
- frequency hopper
- pulse/burst source
- possible DFS/radar-like candidate (hint only)
- unknown non-Wi-Fi

### Linux (advanced)

- long-window persistence
- hop tracking
- pattern comparison
- replay-driven tuning
- optional ML experiments

---

## Product principle

**Node = RF sensor + safety controller + short-term buffer + basic UI**

**Linux = full analyzer + replay + classifier lab + reporting**
