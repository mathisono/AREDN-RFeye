# OpenClaw Task Plan for AREDN RFeye

## Mission

Build AREDN RFeye as an AREDN-native spectrum visibility app for ath10k AC nodes using OpenWrt `PACKAGE_ATH_SPECTRAL`.

## Constraints

- Must fit inside an AREDN node image.
- Must use AREDN app integration paths under `/www/cgi-bin/apps` and `/www/apps`.
- Must avoid large dependencies.
- Must not write continuous RF captures to flash.
- Must degrade gracefully on unsupported hardware.
- Must keep the first implementation simple enough to run on small embedded targets.

## Phase 1: App shell and support probe

### Tasks

- Create OpenWrt package skeleton `package/aredn-rfeye`.
- Install `/usr/bin/rfeye-probe`.
- Install `/www/cgi-bin/apps/rfeye/user`.
- Install `/www/cgi-bin/apps/rfeye/admin`.
- Install `/www/cgi-bin/apps/rfeye/data`.
- Install `/www/apps/rfeye/icon.svg`.
- Add default `/etc/config/rfeye`.

### Acceptance criteria

- RFeye appears in AREDN Apps menu.
- RFeye user page loads AREDN CSS.
- Probe shows debugfs status, ath10k spectral file presence, and `iw` survey availability.
- Unsupported devices show a clear message.

## Phase 2: Data model

### Tasks

- Define JSON format for probe status.
- Define JSON format for survey counters.
- Define JSON frame format for FFT samples.
- Define capture metadata schema.

### Acceptance criteria

- `/cgi-bin/apps/rfeye/data` returns valid JSON.
- JSON is stable enough for UI polling.
- Errors include human-readable messages and machine-readable flags.

## Phase 3: Spectral TLV parser

### Tasks

- Read `spectral_scan0` binary data.
- Parse ath10k spectral TLV records.
- Normalize FFT bins, frequency, width, noise, RSSI, max magnitude, and TSF.
- Add fixture captures for tests.

### Acceptance criteria

- Parser survives truncated data.
- Parser can replay fixture captures offline.
- Parser never blocks the web UI indefinitely.

## Phase 4: Live UI

### Tasks

- Add current FFT canvas.
- Add waterfall canvas.
- Add max-hold and average modes.
- Add channel-width overlays.
- Add pause/reset controls.

### Acceptance criteria

- UI remains responsive on a node-class CPU.
- Browser receives downsampled frames, not raw unbounded data.
- No high-rate flash writes.

## Phase 5: Channel utilization

### Tasks

- Collect `iw dev <iface> survey dump` periodically.
- Compute deltas for active, busy, RX, TX, and noise counters.
- Show total utilization, self TX, RX, and external busy estimate.

### Acceptance criteria

- Utilization updates at a sane low rate.
- Missing counters are handled gracefully.
- UI labels values as estimates.

## Phase 6: Classifier v1

### Tasks

- Extract occupied bandwidth, peak bin, persistence, duty cycle, and hop behavior.
- Implement rule-based labels.
- Add explanation string for each classification.

### Acceptance criteria

- Classifier produces labels only when confidence is reasonable.
- Unknown signals remain labeled unknown.
- DFS/radar-like label is marked non-regulatory.

## First OpenClaw prompt

```text
You are working in the AREDN-RFeye repository. Build the Phase 1 scaffold only. Keep it small and OpenWrt/AREDN-friendly. Do not add heavy dependencies. Implement the package skeleton, rfeye-probe shell script, AREDN app endpoints, JSON status endpoint, default config, and icon. The probe must check debugfs, ath10k spectral files, and iw survey availability. The UI must load AREDN CSS and show clear status panels. Do not implement binary spectral TLV parsing yet.
```
