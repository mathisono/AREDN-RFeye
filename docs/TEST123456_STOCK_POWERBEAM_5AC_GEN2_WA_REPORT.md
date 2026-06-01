# Tests 1–6 Report: Stock PowerBeam 5AC Gen2 (WA) — Baseline

> **Date:** 2026-05-31 22:44 PDT  
> **Tests:** R17 PR #2730 Test Plan — Tests 1, 2, 3, 4, 5, 6  
> **Target:** PowerBeam 5AC Gen2 (PBE-5AC-Gen2) at `10.108.120.18`  
> **Firmware:** Stock AirOS `WA.v8.7.11` (build 46972, 2022-06-14)  
> **Kernel:** Linux 2.6.32.68 (mips, AR934x)  
> **Access:** SSH via MSE-88 jump host (`mat@192.168.3.88`) → `ubnt@10.108.120.18`  
> **Uptime at test:** 23 days, 6:53  

---

## Board Identity (confirmed)

| Field | Value |
|-------|-------|
| Model | PowerBeam 5AC Gen2 |
| Short name | P5C |
| Board model | PBE-5AC-Gen2 |
| Sysid | **`0xe3d6`** |
| FCC ID | SWX-PBE5ACG2 |
| HW MAC | `24:5A:4C:0E:25:14` |
| Platform | **WA (AR934x / QCA956x)** |
| Device name | PB5ACG2-A-4C |
| Memory | 64 MB |
| Radio 1 (PCI) | ath10k-equiv, bus=pci, subsysid=0xe3d6, 2x2, 24 dBm max |
| Radio 2 (AHB/WMAC) | on-chip, bus=ahb, subsysid=0xe3d6, **1x1**, 19 dBm max |

---

## Test 1: Verify PR #2730 DTS Hooks Present

| Check | Result | Notes |
|-------|--------|-------|
| WMAC DTS node | ❌ ABSENT | Stock AirOS has no device-tree filesystem |
| `/proc/device-tree/` | ❌ ABSENT | Not mounted (kernel 2.6.32) |
| `lsmod \| grep ath9k` | ❌ Not loaded | Proprietary QCA driver stack |
| ath9k phy debugfs | ❌ ABSENT | No debugfs ath9k entries |
| AREDN `device_config.json` wlan1 | ❌ N/A | No AREDN config |

### Test 1 Verdict: **N/A — Expected (stock AirOS)**

AirOS Radio 2 (WMAC) configuration from `board.info`:
```
radio.2.bus=ahb              ← on-chip WMAC
radio.2.chains=1             ← 1x1 SISO (vs Rocket's 2x2!)
radio.2.ieee_mode_bg=1       ← 802.11b/g mode (2.4 GHz capable)
radio.2.web_exclude=1        ← hidden from UI
radio.2.txpower.max=19
radio.2.chanbw=5,10,20,40,80
```

**⚡ Key difference from Rocket 5AC Lite (XC):**
- WA WMAC is **1x1 SISO** and **802.11b/g** (not 802.11a)
- Rocket XC WMAC is **2x2 MIMO** and **802.11a**
- This reflects different QCA SoCs: WA=AR934x/QCA956x, XC=QCA955x

---

## Test 2: Pre-RFeye WMAC State (Baseline)

### Wireless Interfaces

| Interface | Standard | Mode | Freq | ESSID | Tx Power | Signal | Notes |
|-----------|----------|------|------|-------|----------|--------|-------|
| `wifi0` | — | raw | — | — | — | — | PCI radio, MAC `24:5A:4C:0E:25:14`, mem `b0000000` |
| `ath0` | 802.11ac | Master | 5.76 GHz | "W6BBpTpWA6KQB" | 24 dBm | -60 dBm | Production PTP link, WPA2, 400 Mbps, 40 MHz BW |
| `wifi1` | — | raw | — | — | — | — | **WMAC (AHB)**, MAC `26:5A:4C:0E:25:14`, mem `b8100000` |
| `airview1` | 802.11na | Monitor | 4.92 GHz | "spectral" | 13 dBm | NF -103 dBm | AirView spectral scanner on WMAC |

### ✅ WMAC MAC is REAL (not fake!)

The WA WMAC uses MAC `26:5A:4C:0E:25:14` — this is the board MAC (`24:5A:4C:0E:25:14`) with byte 0 incremented by 2. This is a **factory-assigned MAC derived from the board MAC**, NOT the fake `00:02:03:04:05:06` seen on the XC Rocket.

This confirms the WA board has **real factory WMAC caldata** containing a proper MAC address.

### Production Link Status

```
wlanOpmode=ap-ptp-ac
wlanConnections=1
wlanUptime=2011879 sec (23.3 days)
signal=-60 dBm, noise=-84 dBm, cinr=23
chanbw=40, freq=5760, centerFreq=5770
distance=4650 m
wlanTxRate=300.0 Mbps
```

Active PTP link, running for 23+ days continuously.

### Firmware Files

```
/lib/firmware/ — empty (no ath9k-eeprom-* files)
```

### dmesg — No caldata error!

```
ol_transfer_bin_file 259: Download Firmware data len 216060. Mode: PTP
FIRMWARE:P 34 V 1 T 110
```

**Critical difference from XC Rocket:** There is **NO `ar9300_eeprom_restore_internal: No vaid CAL`** message. The WA board's WMAC loaded its caldata successfully from the ART partition without needing the default template fallback.

### Flash Layout

| Partition | Size | Name |
|-----------|------|------|
| mtd0 | 256K | u-boot |
| mtd1 | 64K | u-boot-env |
| mtd2 | 1M | kernel |
| mtd3 | 14.4M | rootfs |
| mtd4 | 256K | cfg |
| mtd5 | 64K | **EEPROM** |

### 🔑 ART / EEPROM Dumps

**ART+0x0000** (board header):
```
24 5a 4c 0f 25 14 26 5a  4c 0f 25 14 e3 d6 07 77
00 01 ec 0e 52 ff 00 0d  ff ff ff ff ff ff ff ff
```
- Board MAC: `24:5A:4C:0F:25:14` (factory programmed)
- Sysid: **`0xe3d6`** (PowerBeam 5AC Gen2, WA)
- Subvendor: `0x0777` (Ubiquiti)

**ART+0x1000** (WMAC caldata region):
```
02 02 26 5a 4c 0e 25 14  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 2a 00 00 1f 00
33 03 00 00 00 00 04 00  48 00 7d 02 03 00 08 ff
11 01 00 00 00 10 01 00  00 ee ee 0e 00 10 00 10
```
✅ **REAL FACTORY WMAC CALDATA!**
- Header: `02 02` (AR9300 EEPROM format)
- MAC: `26:5A:4C:0E:25:14` (matches wifi1 interface MAC)
- Contains valid calibration parameters (not blank, not default template)

**ART+0x5000** (PCI/ath10k caldata region):
```
44 08 be d7 02 0d 24 5a  4c 0e 25 14 2a 00 00 00
15 49 00 08 44 0c 08 00  00 00 15 00 00 00 33 00
00 00 00 00 00 00 98 00  00 4f 00 00 00 63 75 73
32 32 33 2d 30 32 32 2d  6e 31 37 32 35 00 00 00
```
- ✅ Valid ath10k caldata (header `0x4408`)
- MAC: `24:5A:4C:0E:25:14`
- Board ID: `cus223-022-n1725`

### Test 2 Verdict: **✅ PASS (Baseline Captured)**

---

## Test 3: Install RFeye r17

| Check | Result |
|-------|--------|
| `opkg info aredn-rfeye` | ❌ opkg not available (stock AirOS) |
| `rfeye-wmac-provision.sh` | ❌ NOT FOUND |
| `caldata reference bin` | ❌ NOT FOUND |

### Test 3 Verdict: **N/A — Stock AirOS, no package manager**

RFeye cannot be installed on stock AirOS; requires AREDN firmware.

---

## Test 4: WMAC Provision Status (Pre-Provisioning)

| Check | Result |
|-------|--------|
| `rfeye-agent wmac_status` | ❌ NOT INSTALLED |

### Equivalent Status (Manual Reconstruction)

```json
{
  "ok": true,
  "board": {
    "sysid": "0xe3d6",
    "model": "PowerBeam 5AC Gen2",
    "platform": "WA (AR934x)",
    "supported": true
  },
  "wmac": {
    "phy": "wifi1 (proprietary, not ath9k)",
    "initialized": true,
    "spectral_ready": false,
    "mac": "26:5A:4C:0E:25:14",
    "note": "WMAC active via AirOS proprietary driver with REAL factory caldata"
  },
  "caldata": {
    "installed": true,
    "source": "ART+0x1000 (factory)",
    "format": "AR9300 (header 02 02)",
    "art_0x1000": "VALID — real factory WMAC caldata with MAC 26:5A:4C:0E:25:14",
    "art_0x5000": "VALID — ath10k PCI caldata"
  },
  "ath9k_loaded": false,
  "airview_interface": "airview1 (802.11na monitor @ 4.92 GHz, NF -103 dBm)"
}
```

### Test 4 Verdict: **N/A — Baseline captured**

---

## Test 5: Run WMAC Provisioning

| Check | Result |
|-------|--------|
| `rfeye-agent wmac_provision` | ❌ NOT INSTALLED |

### Test 5 Verdict: **N/A — Stock AirOS**

---

## Test 6: Verify WMAC Initialized

| Check | Result | Notes |
|-------|--------|-------|
| `iw phy` | ❌ Not available | No nl80211 on AirOS |
| `spectral_scan_ctl` | ❌ Not present | No debugfs ath9k spectral |
| `iw dev` | ❌ Not available | No nl80211 |
| `iwconfig` | ✅ Two radios | `ath0` (production) + `airview1` (WMAC spectral) |
| `rfeye-agent wmac_status` | ❌ Not installed | Expected |

### Wireless State (Post = Same as Pre — no provisioning occurred)

- **`ath0`** — 802.11ac Master @ 5.76 GHz, 400 Mbps, signal -59 dBm (active PTP link)
- **`airview1`** — 802.11na Monitor @ 4.92 GHz, NF -103 dBm (AirView spectral)
- Both radios operating normally

### Test 6 Verdict: **N/A — Baseline captured**

---

## XC vs WA Comparison (Stock Firmware)

| Feature | Rocket 5AC Lite (XC) | PowerBeam 5AC Gen2 (WA) |
|---------|---------------------|------------------------|
| Sysid | `0xe1f5` | `0xe3d6` |
| SoC | QCA955x | AR934x / QCA956x |
| WMAC chains | 2x2 MIMO | **1x1 SISO** |
| WMAC mode | 802.11a (5 GHz) | **802.11b/g (2.4 GHz)** |
| ART+0x1000 | ❌ Blank (all `0xFF`) | ✅ **Real caldata** (`02 02` + MAC) |
| WMAC caldata source | Default template | **Factory ART** |
| WMAC MAC | `00:02:03:04:05:06` (fake) | `26:5A:4C:0E:25:14` (real) |
| dmesg caldata error | `No vaid CAL, calling default template` | **None — loaded OK** |
| airview1 NF | -105 dBm | -103 dBm |
| airview1 freq | 4.92 GHz | 4.92 GHz |
| Kernel | 2.6.32.68 | 2.6.32.68 |
| Memory | 128 MB | 64 MB |
| FW version | XC.v8.7.22 | WA.v8.7.11 |

---

## Key Findings & Implications

### 1. ✅ WA boards have REAL factory WMAC caldata

ART+0x1000 contains valid AR9300 EEPROM data starting with `02 02` and including the board's own WMAC MAC. This is the caldata that was factory-written for this specific board's RF chain. **This is the gold-standard caldata source for RFeye.**

### 2. ⚡ WA WMAC is 1x1 SISO, 802.11b/g — not 802.11a!

The `board.info` shows `radio.2.chains=1` and `radio.2.ieee_mode_bg=1`. This means:
- The WA WMAC is a **single-chain, 2.4 GHz-class radio**
- Despite `airview1` being tuned to 4.92 GHz, the hardware is configured as 802.11b/g
- The XC WMAC (QCA955x) is 2x2 MIMO 802.11a — these are **different radios**

**Impact on RFeye:** Using WA caldata on an XC board is a **cross-SoC, cross-mode** substitution (b/g → a, 1x1 → 2x2). The AR9300 template format may still be compatible, but RF calibration values (gain, NF offsets, power tables) may not be optimal.

### 3. 📡 Both platforms use AirView identically

Both XC and WA stock firmware create `airview1` as a monitor interface at 4.92 GHz — Ubiquiti uses the same AirView spectral approach regardless of the WMAC caldata source (factory vs default template).

### 4. 🔐 Active production link unaffected

The WA PowerBeam has been running a PTP link for 23+ days at 300 Mbps with WMAC active alongside. This is long-term proof of dual-radio coexistence safety.

### 5. This board has the caldata RFeye already ships

The ART+0x1000 data here matches what was extracted for `ath9k-caldata-wmac-wa-reference.bin` in a previous T0 probe. This confirms the reference caldata in the RFeye r17 package came from a board like this one.

---

*Report generated by builder_bob — MSE-88 jump host → stock PowerBeam 5AC Gen2 (WA), R17 test plan Tests 1–6*
