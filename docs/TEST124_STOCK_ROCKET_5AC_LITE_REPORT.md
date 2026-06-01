# Tests 1, 2, 4 Report: Stock Rocket 5AC Lite — Baseline

> **Date:** 2026-05-31 22:24 PDT  
> **Tests:** R17 PR #2730 Test Plan — Tests 1, 2, 4  
> **Target:** Rocket 5AC Lite (R5AC-Lite) at `192.168.3.238`  
> **Firmware:** Stock AirOS `XC.v8.7.22` (build 48486, 2026-02-27)  
> **Kernel:** Linux 2.6.32.68 (mips)  
> **Access:** SSH via `stock-rocket` skill (`ubnt@192.168.3.238`)  
> **Uptime at test:** ~22 minutes  

---

## Board Identity (confirmed)

| Field | Value |
|-------|-------|
| Model | Rocket 5AC Lite |
| Short name | R5C |
| Sysid | **`0xe1f5`** |
| FCC ID | SWX-RM5ACL |
| HW MAC | `04:18:D6:A4:71:C1` |
| Platform | XC (QCA955x) |
| Radio 1 (PCI) | ath10k, bus=pci, devdomain=5000, subsysid=0xe1f5 |
| Radio 2 (AHB/WMAC) | on-chip, bus=ahb, devdomain=5000, subsysid=0xe1f5 |

---

## Test 1: Verify PR #2730 DTS Hooks Present

| Check | Result | Notes |
|-------|--------|-------|
| WMAC DTS node (`/sys/firmware/devicetree/`) | ❌ **ABSENT** | Stock AirOS has no device-tree filesystem exports |
| `/proc/device-tree/` | ❌ **ABSENT** | Not mounted on AirOS (kernel 2.6.32) |
| `lsmod \| grep ath9k` | ❌ Not loaded | AirOS uses proprietary monolithic QCA drivers |
| `ath9k phy` debugfs | ❌ ABSENT | No debugfs ieee80211 ath9k entries |
| AREDN `device_config.json` wlan1 | ❌ N/A | No AREDN config on stock firmware |

### Test 1 Verdict: **N/A — Expected**

PR #2730 DTS hooks are an AREDN/OpenWrt feature. Stock AirOS uses kernel 2.6.32 without device-tree filesystem support. The WMAC is instead initialized by Ubiquiti's proprietary driver stack directly from ART partition hardware addresses.

**Key observation:** AirOS knows about Radio 2 (the WMAC) via `board.info`:
```
radio.2.bus=ahb
radio.2.devdomain=5000
radio.2.txpower.max=19
radio.2.chains=2
radio.2.web_exclude=1      ← hidden from the web UI
```

---

## Test 2: Pre-RFeye WMAC State (Baseline)

### Wireless Stack

| Tool | Available | Notes |
|------|-----------|-------|
| `iw` | ❌ | nl80211 not present — AirOS uses Wireless Extensions |
| `iwconfig` | ✅ | Primary wireless tool on AirOS |
| debugfs ieee80211 | ❌ | Not available |

### Wireless Interfaces

| Interface | Standard | Mode | Freq | ESSID | Tx Power | Notes |
|-----------|----------|------|------|-------|----------|-------|
| `wifi0` | — | raw | — | — | — | PCI radio (ath10k-equiv), MAC `04:18:D6:A4:71:C1`, mem `b0000000` |
| `ath0` | 802.11ac | Master | 5.745 GHz | "Lightsabersforyou" | 25 dBm | Production AP, WPA2, 173.3 Mbps |
| `wifi1` | — | raw | — | — | — | **WMAC (AHB)**, MAC `00:02:03:04:05:06`, mem `b8100000` |
| `airview1` | 802.11na | Monitor | 4.92 GHz | "spectral" | 13 dBm | **AirView spectral scanner on WMAC** |

### WMAC MAC Address: Fake/Default

The WMAC (`wifi1`/`airview1`) uses MAC `00:02:03:04:05:06` — this is **not a real MAC**. This confirms the WMAC has no factory calibration data and the driver assigned a placeholder MAC from the default template.

### Firmware Files

```
/lib/firmware/ — empty (no ath9k-eeprom-* files)
```

AirOS does not use the Linux firmware loading path. The proprietary driver reads EEPROM/ART directly.

### dmesg Key Lines

```
ar9300_eeprom_restore_internal[4673] No vaid CAL, calling default template
```

Confirms: no valid factory WMAC calibration at ART+0x1000; driver falls back to AR9300 default template.

### ART / EEPROM Partition Contents

**Flash layout** (`/proc/mtd`):

| Partition | Offset | Size | Name |
|-----------|--------|------|------|
| mtd0 | 0x000000 | 256K | u-boot |
| mtd1 | 0x040000 | 64K | u-boot-env |
| mtd2 | 0x050000 | 1M | kernel |
| mtd3 | 0x150000 | 14.4M | rootfs |
| mtd4 | 0xFB0000 | 256K | cfg |
| mtd5 | 0xFF0000 | 64K | **EEPROM** |

**ART+0x0000** (board header):
```
04 18 d6 a5 71 c1 06 18  d6 a5 71 c1 e1 f5 07 77
00 01 5c 2a ff ff 00 0d  ff ff ff ff ff ff ff ff
```
- Board MAC: `04:18:D6:A5:71:C1`
- Sysid: `0xe1f5` (Rocket 5AC Lite confirmed)
- Subvendor: `0x0777` (Ubiquiti)

**ART+0x1000** (WMAC caldata region):
```
ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff   ← ALL 0xFF
```
⚠️ **BLANK** — 64 bytes sampled are all `0xFF`. **No factory WMAC calibration data exists on this XC board.** This is consistent with T0 findings on other XC boards.

**ART+0x5000** (PCI/ath10k caldata region):
```
44 08 69 b8 02 03 04 18  d6 a4 71 c1 2a 00 00 00
55 49 00 08 44 0c 08 00  00 00 15 00 00 00 33 00
00 00 00 00 00 00 98 00  00 4f 00 00 00 43 55 53
32 32 33 2d 37 32 30 2d  53 30 38 34 39 00 00 00
```
- ✅ Real caldata present (header `0x4408`)
- Board MAC embedded: `04:18:D6:A4:71:C1`
- Board ID string: `CUS223-720-S0849`
- This is the **production ath10k radio's factory calibration** — correct and expected.

### Test 2 Verdict: **PASS (Baseline Captured)**

The stock Rocket 5AC Lite confirms:
- ✅ No ath9k firmware files on filesystem (AirOS loads from ART directly)
- ✅ ART+0x1000 is **blank (all 0xFF)** — no factory WMAC caldata
- ✅ ART+0x5000 has valid PCI/ath10k caldata
- ✅ WMAC is active via proprietary driver using AR9300 default template
- ✅ WMAC uses fake MAC `00:02:03:04:05:06` (from default template)

---

## Test 4: WMAC Provision Status (Pre-Provisioning)

| Check | Result | Notes |
|-------|--------|-------|
| `rfeye-agent wmac_status` | ❌ **Not installed** | Expected — stock AirOS, no RFeye |

### Equivalent Status (Manual Reconstruction)

Based on the data collected, the equivalent `wmac_status` output for this board would be:

```json
{
  "ok": true,
  "board": {
    "sysid": "0xe1f5",
    "model": "Rocket 5AC Lite",
    "supported": true
  },
  "wmac": {
    "phy": "wifi1 (proprietary, not ath9k)",
    "initialized": true,
    "spectral_ready": false,
    "note": "WMAC active via AirOS proprietary driver, not Linux ath9k"
  },
  "caldata": {
    "installed": false,
    "art_0x1000": "BLANK (all 0xFF)",
    "art_0x5000": "VALID (ath10k PCI caldata)",
    "driver_fallback": "AR9300 default template"
  },
  "ath9k_loaded": false,
  "airview_interface": "airview1 (802.11na monitor @ 4.92 GHz)"
}
```

### Test 4 Verdict: **N/A — Baseline Captured**

RFeye is not installed on stock firmware. The reconstructed status confirms the board is `supported` (sysid `0xe1f5`), the WMAC exists but uses a default template (no real caldata), and the ath9k Linux driver is not present.

---

## Summary of Findings

### ✅ Confirmed Facts

| Finding | Evidence |
|---------|----------|
| Board is Rocket 5AC Lite, sysid `0xe1f5` | `board.info`, ART+0x0C |
| XC platform (QCA955x) | Firmware version prefix `XC.` |
| ART+0x1000 is blank (no factory WMAC cal) | Hex dump: all `0xFF` |
| ART+0x5000 has valid ath10k caldata | Hex dump: header `0x4408`, real MAC, board ID |
| WMAC is active under AirOS | `wifi1` + `airview1` interfaces present |
| WMAC uses AR9300 default template | `dmesg: No vaid CAL, calling default template` |
| WMAC has fake MAC from template | `00:02:03:04:05:06` |
| Dual-radio coexistence works | `ath0` (production) + `airview1` (WMAC) both UP |

### 🔑 Implications for RFeye r17

1. **Blank ART+0x1000 is confirmed** on a real Rocket 5AC Lite — XC boards definitively have no factory WMAC caldata
2. **AirOS proves the WMAC works with default template** — spectral scanning without real caldata is vendor-validated
3. **RFeye's WA reference caldata should be an improvement** over the default template since it contains real RF calibration values
4. **The firmware path for AREDN will be different** — AirOS uses direct ART access at memory `b8100000`, while AREDN/OpenWrt uses `/lib/firmware/ath9k-eeprom-ahb-18100000.wmac.bin`
5. **Memory address `b8100000`** from `wifi1` ifconfig confirms the AHB WMAC register base — matches the OpenWrt DTS `18100000` (KSEG1 mapping)

### 📡 Radio 2 (WMAC) Details from `board.info`

```
radio.2.bus=ahb              ← on-chip (not PCI)
radio.2.txpower.max=19       ← max 19 dBm
radio.2.txpower.min=0
radio.2.chains=2             ← 2x2 MIMO
radio.2.ieee_mode_a=1        ← 802.11a capable
radio.2.web_exclude=1        ← hidden from management UI
radio.2.chanbw=5,10,20,40,80 ← supports multiple bandwidths
```

This confirms Ubiquiti intentionally uses the WMAC as an internal-only spectral radio, hidden from the user-facing management interface.

---

*Report generated by builder_bob — stock-rocket skill, R17 test plan Tests 1, 2, 4*
