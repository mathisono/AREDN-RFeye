# Wideband Spectral Research — Working Brief

> **Created:** 2026-05-26
> **Status:** Research / bench-only — not for production deployment

## Context

AREDN RFeye currently captures spectral data on the **current operating channel
only**, using ath10k debugfs spectral scan. This is correct for production use on
single-radio devices where the AP must not be disrupted.

This brief tracks research into wideband (multi-channel) spectral coverage and
what would be required to display an AirView-like 5100–5900 MHz spectrum view.

---

## Key Finding: AirView Uses a Dedicated Scanner Radio

Observation of a Ubiquiti Rocket 5AC Lite (2026-05-26) revealed that AirView
achieves its wideband display by **sweeping a dedicated second radio** through the
5 GHz band while the main AP radio stays on its operating channel.

See: [AIRVIEW_ARCHITECTURE_FINDINGS.md](AIRVIEW_ARCHITECTURE_FINDINGS.md)

### Implications

- A wideband 5100–5900 MHz display is **not** achievable from a single
  ath10k spectral capture. Each capture covers only the current channel width
  (20/40/80 MHz).
- Achieving AirView-like coverage requires either:
  1. A **dedicated scanner radio** (second radio, not serving the AP), or
  2. **Retune-and-stitch** on the production radio, which disrupts AP service
     for the duration of each off-channel dwell.
- Option 2 is unsuitable for production AREDN nodes.

---

## WMI Knobs: Still Useful, But Per-Channel

The ath10k WMI spectral scan parameters (`spectral_bins`, `spectral_count`,
`spectral_fft_size`, etc.) and research targets like `scan_rpt_mode=3` remain
useful for improving the quality and resolution of **each FFT slice** on the
current channel.

They do **not** provide cross-channel sweep or wideband instantaneous capture.

### `scan_rpt_mode=3` Status

- Should still be tested for its effect on per-channel FFT reporting (more
  samples per scan trigger, or different report format).
- It is **not expected** to provide 800 MHz instantaneous capture by itself.
- Testing should continue on bench hardware only.

---

## PBE-5AC-500 Hardware Check Required

Before assuming AirView-like scanning is possible on the PBE-5AC-500, verify
whether a second radio exists or is exposed.

> **Note:** No stock-firmware PBE-5AC-500 is available. The check should be
> performed on the AREDN-firmware unit. The commands below work on both
> stock (Ubiquiti vendor driver) and AREDN (ath10k/OpenWrt).

### Recommended commands (AREDN node via SSH):

```bash
# Radio / phy enumeration
iw dev
iw phy
ls -la /sys/class/net/

# Kernel log — radio probe, spectral, VAP creation
dmesg | grep -iE 'wifi|ath[^e]|qca|spectral|airview|vap|radio|pci.*(attach|probe)|ahb'

# Spectral debugfs (ath10k path on AREDN)
find /sys/kernel/debug -maxdepth 5 -type f | grep -i spectral

# Loaded modules
cat /proc/modules | grep -iE 'ath|spectral'

# PCI devices
lspci 2>/dev/null
cat /proc/bus/pci/devices 2>/dev/null

# Board info (if present — may not exist on AREDN)
cat /etc/board.info 2>/dev/null | grep -E 'phycount|radio.*bus'
```

### What to look for — compare against R5AC-Lite dual-radio fingerprint

See `AIRVIEW_ARCHITECTURE_FINDINGS.md` → "Dual-Radio Fingerprint" section.

| Check | Dual-radio (R5AC-Lite) | Single-radio (expected PBE-5AC-500) |
|-------|----------------------|------------------------------------|
| `/sys/class/net/wifi1` | Present (AHB) | Absent |
| `radio.2.bus=ahb` in board.info | Present | Absent |
| `ath-spectral-filter` in `/proc/interrupts` | Present (~1300/sec) | Absent |
| `airview1` interface | Present | Absent |
| Second PCI or AHB radio in dmesg | `wifi1: Atheros ???: mem=0xb8100000` | Not expected |
| `iw dev` phy count | Two phys (or vendor-hidden) | One phy |

### Result: Confirmed Single-Radio (2026-05-26)

Checked via SSH through MSE-88 to `KJ6DZB-WSB-ACdish5` (AREDN firmware).

| Check | R5AC-Lite (dual) | PBE-5AC-500 (result) |
|-------|-------------------|---------------------|
| SoC | QCA9560 | QCA9558 ver 1 rev 0 |
| `/sys/class/net/wifi1` | Present (AHB) | **Absent** |
| `/sys/class/ieee80211/` | (vendor, hidden) | `phy0` only |
| `iw dev` phy count | N/A (vendor driver) | **1 phy** (`phy0` → `wlan0`) |
| PCI devices | `077711ac` (ath_pci) | `077711ac` (ath10k_pci) — **one device** |
| `ath-spectral-filter` interrupt | ~1300/sec | **None** |
| `airview1` interface | Present | **Absent** |
| `radio.2.bus=ahb` | Present | **No board.info** |
| ath10k spectral debugfs | N/A (vendor) | ✅ `spectral_scan_ctl`, `spectral_scan0`, `spectral_bins`, `spectral_count` |
| ath9k module | Not loaded | Loaded but unused (no ath9k net interface) |

**Conclusion:** The PBE-5AC-500 is a **single-radio device** with one QCA988x
on PCIe (`phy0`/`wlan0`). There is no second scanner radio. The QCA9558 SoC does
contain an on-chip radio (hence `ath9k` module loaded), but it is **not exposed
as a network interface** and is not used for spectral scanning under AREDN.

AirView-like wideband scanning is **not feasible** on this hardware without
taking the production radio off-channel. RFeye on PBE-5AC-500 should remain
**current-channel only** in production mode.

The upstream ath10k spectral debugfs path (`spectral_scan_ctl`, `spectral_scan0`,
`spectral_bins=128`, `spectral_count`) is present and functional for
per-channel FFT capture — this is RFeye's correct data source.

---

## Research Modes (Bench Only)

For bench/lab hardware not serving production AREDN traffic, future research
may explore:

- **Retune-and-stitch:** Cycle through channels, capture FFT on each, stitch
  into wideband view. Requires AP to be down or in a non-serving test mode.
- **Monitor-mode VAP:** Create a monitor VAP on the production radio and
  retune it. Still disrupts the single shared radio.
- **External SDR:** Use an RTL-SDR or similar alongside the AREDN node for
  wideband capture without touching the production radio.

These are explicitly **not** production features and must not be enabled on
nodes carrying live mesh traffic.

---

## Summary

| Capability | Status |
|------------|--------|
| Current-channel FFT (production) | ✅ Working, correct default |
| WMI knob tuning (bench) | 🔬 Research continues |
| `scan_rpt_mode=3` (bench) | 🔬 To be tested per-channel |
| Wideband sweep-and-stitch | ❌ Requires dedicated radio or AP disruption |
| PBE-5AC-500 second radio | ❓ Needs hardware verification |
| AirView-equivalent on single-radio | ❌ Not feasible without link drop |
