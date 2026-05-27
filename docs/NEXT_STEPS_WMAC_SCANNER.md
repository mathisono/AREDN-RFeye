# Next Steps: Upgrade RFeye for WMAC Spectral Scanner

> **Date:** 2026-05-26
> **Prerequisite:** WMAC DTS patch merged and firmware built with `phy1` exposed
> **Approach:** Incremental — each phase is independently useful and testable

---

## Phase 0: Verify Hardware (bench only)

Build AREDN firmware with patch 758, flash bench PBE-5AC-500, confirm:

```bash
# Does phy1 exist?
ls /sys/class/ieee80211/
# Expected: phy0 phy1

# Does ath9k spectral debugfs exist?
ls /sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan_ctl
# Expected: file exists

# Can we create a monitor VAP?
iw phy phy1 interface add mon1 type monitor
ip link set mon1 up

# Can we trigger spectral scan and get data?
iw dev mon1 set channel 149 HT20
echo background > /sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan_ctl
dd if=/sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan0 bs=4096 count=1 | hexdump -C | head
echo disable > /sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan_ctl
```

**Gate:** If this works, proceed. If not, debug the DTS/caldata/driver before
touching any RFeye code.

---

## Phase 1: Teach `rfeye-probe` About Dual Phys

**Current state:** `rfeye-probe` detects `phy0` only, checks ath10k debugfs.

**Change:** Probe both phys and report each radio's capability.

```
/usr/bin/rfeye-probe
```

Additions:
- Walk `/sys/class/ieee80211/phy*/` to discover all phys
- For each phy, detect driver: `ath10k` → production radio, `ath9k` → scanner radio
- Report scanner capability:

```json
{
  "phys": {
    "phy0": {
      "driver": "ath10k",
      "role": "mesh",
      "interface": "wlan0",
      "spectral": true,
      "spectral_path": "/sys/kernel/debug/ieee80211/phy0/ath10k"
    },
    "phy1": {
      "driver": "ath9k",
      "role": "scanner",
      "interface": null,
      "spectral": true,
      "spectral_path": "/sys/kernel/debug/ieee80211/phy1/ath9k"
    }
  },
  "scanner_available": true,
  "scanner_phy": "phy1"
}
```

**LOE:** ~30 lines of shell. No risk to production radio.

---

## Phase 2: Scanner Radio Manager (`rfeye-scanner`)

**New component:** `/usr/sbin/rfeye-scanner` — manages the WMAC scanner radio.

Responsibilities:
- Create/destroy monitor VAP on `phy1` (`mon1` or `scan0`)
- Set channel on the monitor VAP
- Start/stop ath9k spectral scan
- Read FFT data from `spectral_scan0`
- **Never touch phy0/wlan0**

### Interface lifecycle

```bash
# Start scanner
iw phy phy1 interface add scan0 type monitor
ip link set scan0 up

# Tune to channel
iw dev scan0 set channel $CH HT20

# Enable spectral
echo background > /sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan_ctl

# Read FFT frames
cat /sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan0

# Disable
echo disable > /sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan_ctl

# Cleanup
ip link set scan0 down
iw dev scan0 del
```

### Key design decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| TX power | Set to 0 or disable TX | Scanner is receive-only |
| Interface name | `scan0` | Distinct from `wlan0`, `mon0` |
| Channel list | Configurable, default all 5 GHz | Operator may limit range |
| Dwell time per channel | Configurable, default 50–100 ms | Balance speed vs sensitivity |
| Sweep mode | Sequential channel hop | Simple, proven (matches AirView approach) |

### Safety rules

- `rfeye-scanner` refuses to operate on `phy0`
- If `phy1` does not exist, fail cleanly with status message
- Scanner teardown on exit/signal/error (always clean up `scan0`)
- TX disabled on scanner VAP at creation

**LOE:** ~200 lines of shell. Isolated from production radio path.

---

## Phase 3: ath9k FFT Parser

**Current state:** `rfeye_spectral_parse.c` handles ath10k TLV
(`ATH_FFT_SAMPLE_ATH10K` with type 3 headers).

**Change:** Add ath9k FFT frame parsing.

The ath9k `spectral_scan0` debugfs produces `struct fft_sample_ath10k` or
`struct fft_sample_ath9k` frames depending on the chip. For the QCA9558 WMAC
(AR9003 family), the format is `ATH_FFT_SAMPLE_HT20` (type 1) or
`ATH_FFT_SAMPLE_HT20_40` (type 2).

### TLV header (shared between ath9k and ath10k)

```c
struct fft_sample_tlv {
    uint8_t  type;    // 1=HT20, 2=HT20_40, 3=ATH10K
    uint16_t length;  // big-endian
} __packed;
```

### ath9k HT20 sample (type 1, AR9003+)

```c
struct fft_sample_ath9k {
    struct fft_sample_tlv tlv;
    uint8_t  max_exp;
    uint16_t freq;           // center freq MHz, big-endian
    int8_t   rssi;
    int8_t   noise;
    uint16_t max_magnitude;  // big-endian
    uint8_t  max_index;
    uint8_t  bitmap_weight;
    uint64_t tsf;            // big-endian
    uint8_t  data[];         // 56 or 128 bins
} __packed;
```

### What to add to `rfeye_spectral_parse.c`

1. Recognize type 1 (`ATH_FFT_SAMPLE_HT20`) and type 2 (`ATH_FFT_SAMPLE_HT20_40`)
2. Parse `freq`, `rssi`, `noise`, `max_magnitude`, `max_index`, `data[]` bins
3. Output same JSON format as ath10k frames (frequency, noise, bins array)
4. Add `--driver ath9k` or auto-detect from TLV type field

The parser already handles TLV framing and resync — adding type 1/2 is
mostly a new struct definition and bin extraction path.

**LOE:** ~100–150 lines of C added to existing parser.

---

## Phase 4: Channel Sweep Engine

**New capability in `rfeye-scanner`:** sweep across channels, collecting
per-channel FFT snapshots, producing a stitched wideband view.

### Sweep loop (pseudocode)

```
channel_list = [36, 40, 44, 48, 52, ..., 165]  # configurable
dwell_ms = 80
bins_per_channel = 56  # ath9k default

while scanning:
    for ch in channel_list:
        set_channel(scan0, ch)
        sleep(dwell_ms)
        frames = read_spectral(phy1)
        for frame in frames:
            parsed = parse_ath9k_fft(frame)
            store_bins(parsed.freq, parsed.bins)
    emit_wideband_snapshot(all_bins)
```

### Output: wideband JSON

Similar to AirView's `/tmp/airview/data` but using open formats:

```json
{
  "timestamp": 1748311200,
  "sweep_ms": 4200,
  "channels_scanned": 52,
  "freq_min_mhz": 5180,
  "freq_max_mhz": 5845,
  "bin_width_khz": 312.5,
  "bins": [
    {"freq_mhz": 5180.15625, "power_db": -95},
    {"freq_mhz": 5180.46875, "power_db": -93},
    ...
  ],
  "latest_power": [-95, -93, -91, ...],
  "noise_floor": [-102, -101, -103, ...]
}
```

### Data products

| Product | Update rate | Description |
|---------|------------|-------------|
| `latest_sweep.json` | Every sweep (~2–5 sec) | Current power per bin |
| `sweep_history.ndjson` | Append per sweep | Waterfall history |
| `max_hold.json` | Cumulative | Peak power per bin |
| `average.json` | Running average | Mean power per bin |

**LOE:** ~300 lines of shell/C. This is the core new feature.

---

## Phase 5: Dual-Mode Agent

**Current state:** `rfeye-agent` manages ath10k spectral on `phy0`.

**Change:** Support two operating modes:

### Mode A: Current-channel (existing, phy0)

- ath10k `spectral_scan_ctl` on production radio
- No channel changes, no link disruption
- Default mode, always available
- Existing UI, heatmap, waterfall all work as-is

### Mode B: Wideband scanner (new, phy1)

- ath9k spectral on dedicated WMAC
- Channel sweep across 5 GHz band
- Wideband power display
- Only available when `phy1` exists

### Agent changes

```
rfeye-agent status        → includes scanner_available, scanner_status
rfeye-agent scanner_start → starts rfeye-scanner on phy1
rfeye-agent scanner_stop  → stops scanner, cleans up scan0
rfeye-agent sweep_status  → latest sweep metadata
rfeye-agent sweep_data    → latest wideband JSON
```

The agent delegates to `rfeye-scanner` for all phy1 operations.
Current-channel mode on phy0 continues to work exactly as before.

**LOE:** ~150 lines added to agent. Mostly dispatch + status reporting.

---

## Phase 6: UI — Wideband Display

### New UI panels (when scanner is available)

1. **Wideband spectrum trace** — 5180–5845 MHz power vs frequency
   - Current sweep, max-hold, average overlays
   - Channel boundary markers (20/40/80 MHz)
   - Operating channel highlighted

2. **Wideband waterfall** — time vs frequency heatmap
   - Scrolling, last N sweeps
   - Color-mapped power levels

3. **Scanner controls**
   - Start/stop scanner
   - Channel range selector
   - Dwell time slider
   - Sweep count / continuous toggle

### UI detection

```javascript
// Probe scanner capability
fetch('/cgi-bin/apps/rfeye/data/agent.sh?action=status')
  .then(r => r.json())
  .then(d => {
    if (d.scanner_available) {
      showWidebandPanel();
    }
  });
```

On single-radio nodes (no `phy1`), the wideband panels simply don't appear.
The existing current-channel UI is unchanged.

**LOE:** ~400 lines of HTML/JS/CSS. Can reuse existing waterfall rendering
with wider frequency axis.

---

## Implementation Order

| Step | Phase | Risk | Dependency |
|------|-------|------|------------|
| 1 | Phase 0: Verify hardware | None | DTS patch merged, firmware built |
| 2 | Phase 1: Dual-phy probe | None | Phase 0 passes |
| 3 | Phase 3: ath9k parser | None | Can develop with test fixtures |
| 4 | Phase 2: Scanner manager | Low | Phase 0 passes |
| 5 | Phase 4: Sweep engine | Low | Phase 2 + 3 |
| 6 | Phase 5: Dual-mode agent | Low | Phase 4 |
| 7 | Phase 6: Wideband UI | None | Phase 5 |

Phases 1 and 3 can be developed in parallel and tested offline with fixtures
before the hardware is ready. Phase 2 requires the flashed bench device.

---

## What Does NOT Change

- **phy0/wlan0 is never touched by scanner code**
- **Existing current-channel mode works exactly as before**
- **Single-radio nodes see no difference** (scanner panels hidden)
- **No new kernel modules or firmware blobs needed**
- **No channel hopping on the production radio**
- **All safety rules from ARCHITECTURE.md remain in force**

---

## Comparison: Before and After

| Capability | Before (phy0 only) | After (phy0 + phy1) |
|------------|--------------------|--------------------|
| Current-channel FFT | ✅ 20/40/80 MHz on operating channel | ✅ Unchanged |
| Wideband spectrum | ❌ Not possible | ✅ 5180–5845 MHz via phy1 sweep |
| Wideband waterfall | ❌ Not possible | ✅ Time-frequency heatmap |
| AP disruption | None | None (scanner on separate radio) |
| Interference detection | Current channel only | Full 5 GHz band |
| Hardware requirement | Any ath10k node | QCA955x XC boards (phy1 required) |
