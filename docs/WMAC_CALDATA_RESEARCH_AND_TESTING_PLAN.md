# WMAC Caldata Research & Testing Plan

> **Date:** 2026-05-27 (updated 2026-05-29)  
> **Status:** DRAFT — for review before push  
> **Context:** r16 WMAC spectral scanning works with shared PCI caldata,  
> but the shared PCI caldata is **not an appropriate WMAC calibration  
> source** — it may be structurally accepted by ath9k but was factory-  
> calibrated for a different RF chain. The working question is:  
>
> **What structurally valid AR9300/ath9k-compatible calibration source  
> should be used for the WMAC when XC boards have blank ART+0x1000?**  
>
> This document defines the reconnaissance and testing needed to answer  
> that question, and the safety rules that must hold until it is answered.

---

## 0. Safety Rules

> **These rules supersede any earlier framing that treats shared PCI caldata
> as an acceptable WMAC calibration source.**

1. **Do not feed blank ART+0x1000** to the WMAC. All-`0xFF` is not caldata.
2. **Do not feed random bytes.** Synthetic caldata without RF-path measurement is not caldata.
3. **Do not feed PCI radio ART+0x5000 as the production WMAC calibration source.**
   The 0x5000 data is structurally valid AR9300 EEPROM and ath9k will parse it,
   but it was factory-calibrated for a completely different RF chain (discrete
   QCA988x PCI radio with dedicated LNA/PA, vs. SoC-integrated AR9550 WMAC).
   Using it as the WMAC source is a known-bad fallback, acceptable only for
   deliberate bench testing of that specific fallback.
4. **Find a valid WMAC-calibrated source.** Preferred sources, in order:
   - WA-series (non-XC) board ART+0x1000 with factory WMAC caldata
   - Stock Ubiquiti DTS or firmware evidence of a WMAC caldata path
   - NanoBeam AC Gen2 XC caldata at ART+0x1000 (if populated)
   - Runtime-derived caldata with external RF reference
5. **Treat any substitute caldata as bench-only** until confirmed structurally
   valid and radio-appropriate for the target WMAC RF path.

---

## 1. Current State

### What's Working (r16)

The AREDN PR #2725 DTS patch enables the QCA9558 on-chip WMAC on XC boards
(PowerBeam 5AC 500, Rocket 5AC Lite, NanoBeam AC XC) by sharing the existing
`cal_art_5000` nvmem cell between the ath10k PCI radio and the ath9k WMAC.

**r16 soak test results:**
- 5-minute sweep, 24 channels including DFS
- ~130 frames/sec, ~1 full 24-channel sweep/sec
- 2.6 MB captured, zero mesh impact
- Spectral data is usable for relative visualization

> **⚠️ r16 caldata status:** r16 uses PCI caldata (ART+0x5000) for the WMAC.
> This produces structurally valid spectral data but is **not appropriate as
> a production WMAC calibration source.** See Section 0 safety rules. The
> r16 results should be treated as a bench-only proof-of-concept for the
> spectral scanning pipeline, not as evidence that the caldata question is
> resolved.

### The Caldata Situation

The ART partition layout on XC boards:

| Offset | Size | Content | Notes |
|--------|------|---------|-------|
| 0x0000 | 6 bytes | Ethernet MAC | Used by eth0 |
| 0x1000 | 0x440 | All `0xFF` | **Blank** — no WMAC caldata written at factory |
| 0x5000 | 0x844 | AR9300 compressed EEPROM | Factory-calibrated for the **ath10k PCI radio** |

The AR9300 EEPROM at 0x5000 contains:
- Header: `44 08 7f 34`
- Board ID: `CUS223-720-S0849`
- MAC: `f4:92:bf:bc:8a:de` (WMAC MAC, offset from PCI MAC)
- Per-frequency calibration piers (2G + 5G)
- Per-chain gain corrections
- Noise floor thresholds
- TX power target tables
- Regulatory CTL limits

**The key issue:** This caldata was factory-calibrated for the ath10k PCI
radio's RF chain — its LNA, PA, filters, antenna path, and board trace
characteristics. The on-chip WMAC has a **completely different RF front-end**:

| Property | ath10k PCI Radio | WMAC On-Chip Radio |
|----------|-----------------|-------------------|
| Chip | QCA988x (discrete) | AR9550 (SoC-integrated) |
| Bus | PCIe | AHB |
| RF front-end | Dedicated LNA/PA chain | SoC-integrated RF |
| Antenna | 19 dBi planar dish | ~2 dBi PCB trace / UFL |
| Intended bands | 5 GHz only | 2.4 + 5 GHz |
| Factory calibrated? | Yes (ART 0x5000) | **No** (ART 0x1000 is blank) |

### Why It Works Anyway

The ath9k driver can parse the AR9300 EEPROM at 0x5000 because it's the
native AR9300 format. For receive-only spectral scanning:

- ✅ FFT hardware produces valid magnitude data regardless of caldata accuracy
- ✅ Channel/frequency tuning works (synthesizer settings are silicon-level)
- ✅ Relative signal comparisons within a single sweep are valid
- ⚠️ Absolute dBm values are **wrong** — gain corrections are for a different RF path
- ⚠️ Noise floor reference is calibrated for a different chain
- ⚠️ Per-frequency flatness may have systematic errors across the band
- ⚠️ Cross-channel gain variation may not match reality

RFeye already disclaims "approximate/relative dBm, not lab-calibrated" — so
this is acceptable for the current use case.

---

## 2. What Proper WMAC Caldata Would Improve

### 2.1 Noise Floor Accuracy

The ath9k spectral scan power formula (from kernel docs):

```
power(i) = nf + RSSI + 10*log(b(i)^2) - bin_sum
```

Where `nf` is the noise floor. The driver reads `noiseFloorThreshCh[]` from
the AR9300 EEPROM `modalHeader` to bound the noise floor calibration. With
mismatched caldata, the noise floor reference can be off by **5–15 dB**,
shifting all reported power levels by a fixed offset.

**Impact on RFeye:** Absolute dBm readings would shift. Relative comparisons
within a sweep remain valid since the offset is constant.

### 2.2 Per-Chain Gain Correction

The AR9300 EEPROM stores per-chain, per-frequency calibration data in
`calPierData2G[]` and `calPierData5G[]`. Each calibration pier contains:

- `refPower` — measured output power at a reference setting
- `voltMeas` — voltage regulator reading at calibration time
- `tempMeas` — temperature at calibration time

These allow the driver to compensate for per-chain gain variation and
temperature drift. With PCI caldata on the WMAC:

- **Gain slope across frequency** will follow the PCI radio's curve, not the WMAC's
- **Temperature compensation** will use the wrong baseline
- Bands where the two RF paths diverge most (e.g., band edges, DFS) will have the worst errors

**Impact on RFeye:** The spectral "shape" across a wideband sweep may have
systematic tilt or ripple that doesn't match the true RF environment.

### 2.3 Per-Frequency Flatness

Factory calibration measures response at specific frequency piers and the
driver interpolates between them. The ath10k PCI radio and WMAC have different:

- Filter bandpass shapes
- LNA gain vs. frequency curves
- Board trace losses at different frequencies

With mismatched caldata, some frequency ranges will read systematically
high or low relative to their true level.

**Impact on RFeye:** A strong signal at 5300 MHz and the same signal at
5700 MHz might show different apparent power levels even though they're
identical. This makes cross-channel comparisons less trustworthy.

### 2.4 Regulatory/TX (Not Applicable)

The CTL (Conformance Test Limit) tables and TX power targets in the EEPROM
are irrelevant — the WMAC is receive-only for spectral scanning. RFeye
never transmits on this radio. TX should remain disabled or zero-power.

---

## 3. How Could We Get Proper WMAC Caldata?

### Option A: Runtime Noise Floor Calibration (Easiest)

The ath9k driver performs runtime noise floor calibration (NF_CAL) on each
channel change. This partially compensates for the missing factory data:

- The driver measures the actual noise floor on the current channel
- This updates the `nf` value used in the spectral power formula
- Per-channel NF readings are stored and aged

**What this fixes:** Absolute noise floor offset per channel.  
**What this doesn't fix:** Per-chain gain slope, frequency flatness, temperature drift.

**Test:** Compare NF readings from `ath9k` debugfs with those from a
known-good reference (e.g., an SDR or calibrated spectrum analyzer).

```bash
# Read current noise floor on phy1
cat /sys/kernel/debug/ieee80211/phy1/ath9k/dump_nfcal
```

### Option B: Cross-Calibrate Against the ath10k Radio (Medium)

Use the ath10k production radio as a reference source:

1. On a quiet channel where ath10k is operating, read the ath10k spectral
   power for the current channel
2. Simultaneously read the ath9k WMAC spectral power for the same channel
3. Compute the gain offset between the two radios
4. Apply this offset as a per-channel correction factor in RFeye

**Advantage:** Uses the factory-calibrated ath10k as a reference without
any external equipment.

**Limitation:** Only works on the single channel where ath10k is operating.
Can't calibrate across the full band unless you temporarily retune ath10k
(which disrupts the mesh — not acceptable for production).

### Option C: Generate WMAC Caldata from a Reference (Hard)

Use an external RF source or calibrated spectrum analyzer to:

1. Inject known-level signals at multiple frequency piers
2. Record the WMAC's reported levels
3. Compute gain correction tables
4. Write a custom AR9300 EEPROM to ART+0x1000

**Advantage:** Proper factory-style calibration for the actual WMAC RF path.  
**Disadvantage:** Requires lab equipment, per-board procedure, and a method
to write to the ART partition (flash write, risky on production hardware).

### Option D: Borrow NanoBeam AC Gen2 XC Caldata (Exploratory)

The NanoBeam AC Gen2 XC (`NBE-5AC-Gen2`) is the same board family but
**does have factory WMAC caldata at ART+0x1000**. The upstream OpenWrt DTS
uses `mtd-cal-data = <&art 0x1000>`.

If the WMAC RF path is physically similar across XC boards, one Gen2's
caldata might serve as a "better-than-nothing" template for other XC boards.

**Risk:** Board-to-board variation in component tolerances means one board's
caldata won't perfectly match another. Still likely better than sharing PCI
caldata.

**Test:** If we can obtain a Gen2 XC ART dump, compare the 0x1000 caldata
structure with the 0x5000 data. Look at which fields differ (gain piers,
NF thresholds, modal header).

---

## 4. Testing Plan

### T0: Stock Firmware Calibration Reconnaissance

**Goal:** Before testing RFeye with substitute caldata, inspect a stock
Ubiquiti Rocket / XC image to determine whether the vendor firmware exposes
a WMAC calibration source through device tree, firmware files, or runtime
extraction.

**Commands:**

1. Record whether `/proc/device-tree` exists on stock firmware.
2. Capture `ls -R /proc/device-tree`.
3. Capture `ls -laR /lib/firmware`.
4. Search device tree for `wifi`, `wmac`, `ath`, `cal`, `eeprom`, `art`,
   `nvmem`, `pci`, and `qca` nodes.
5. Search `/lib/firmware` for ath9k / AR9300 / QCA calibration files.
6. Hexdump any matching DT files to inspect structure and content.

**What we're looking for in device tree:**
```
/proc/device-tree/.../wifi...
/proc/device-tree/.../wmac...
/proc/device-tree/.../ath9k...
/proc/device-tree/.../qca9558...
/proc/device-tree/.../mtd-cal-data
/proc/device-tree/.../nvmem-cells
/proc/device-tree/.../calibration
/proc/device-tree/.../eeprom
/proc/device-tree/.../art
```

**What we're looking for in firmware:**
```
ath9k, ar9300, qca9558, caldata, eeprom,
board.bin, board-2.bin, ubnt, radio, wmac
```

**Success condition:**
- Find evidence that stock firmware either:
  - uses valid ART+0x1000 data on WA hardware,
  - loads WMAC caldata from a firmware file,
  - derives WMAC caldata at runtime,
  - or has no WMAC calibration source exposed (confirming the blank-ART
    situation is vendor-side, not an AREDN omission).

**Safety condition:**
- Do not use blank ART+0x1000.
- Do not generate random calibration data.
- Do not use ART+0x5000 as the production WMAC calibration source.
- Treat any substitute caldata as bench-only until confirmed structurally
  valid and radio-appropriate.

**Status:** ✅ COMPLETE (2026-05-29). See `docs/T0_STOCK_PROBE_RESULTS.md`.

**Key findings:**
- Target was a **WA board** (PowerBeam 5AC Gen2, PBE-5AC-Gen2), not XC
- **EEPROM+0x1000 contains REAL WMAC caldata** — NOT blank! AR9300 template
  format (`02 02`), ~512 bytes, with MAC `26:5A:4C:0E:25:14` and structured
  gain/calibration tables
- Stock firmware creates `airview1` monitor interface on WMAC for AirView
  spectral scanning — same use case as RFeye
- `ath_spectral` module loaded; WMAC hidden from web UI (`web_exclude=1`)
- No device tree on stock AirOS (kernel 2.6.32); caldata loaded directly
  from EEPROM MTD partition
- **WA boards have factory WMAC caldata; XC boards do not** — this is a
  vendor decision, not an AREDN omission
- WMAC caldata uses template format (`02 02`), PCI uses compressed (`44 08`)

---

### Phase 1: Quantify Current Accuracy (No Hardware Changes)

**Goal:** Establish baseline — how far off are the WMAC spectral readings
with shared PCI caldata?

| Test | Method | What It Tells Us |
|------|--------|-----------------|
| **T1: NF cal dump** | `dump_nfcal` on phy1 across all 24 channels | Whether runtime NF cal is running and producing reasonable values |
| **T2: Cross-radio comparison** | Compare ath10k and ath9k spectral on the mesh channel simultaneously | Gain offset between the two radios on one known channel |
| **T3: Ambient noise floor sweep** | Record min power per channel across full 5 GHz band, compare against expected thermal noise (~-95 dBm for 20 MHz BW) | Whether the absolute noise floor is in the right ballpark |
| **T4: Known signal injection** | Use a nearby node's beacon as a reference, compare reported power on ath10k vs ath9k | Real-world cross-radio offset |
| **T5: Band-edge flatness** | Compare spectral shape at 5150 MHz vs 5500 MHz vs 5850 MHz with the same environment | Whether frequency-dependent gain error is visible |

#### T1 Procedure
```bash
# On the node, after a sweep:
cat /sys/kernel/debug/ieee80211/phy1/ath9k/dump_nfcal
# Record values for each chain, each channel
```

#### T2 Procedure
```bash
# 1. Start ath10k spectral on current mesh channel
echo background > /sys/kernel/debug/ieee80211/phy0/ath10k/spectral_scan_ctl
# 2. Read ath10k spectral data
cat /sys/kernel/debug/ieee80211/phy0/ath10k/spectral_scan0 > /tmp/ath10k_ref.bin

# 3. Tune ath9k to the same channel
iw dev mon1 set channel <mesh_channel> HT20
echo background > /sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan_ctl
cat /sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan0 > /tmp/ath9k_ref.bin
echo disable > /sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan_ctl

# 4. Parse both, compare average power levels
# The delta is the cross-radio gain offset
```

#### T3 Procedure
```bash
# Capture spectral data across all channels during a quiet period (late night)
# Parse each channel's FFT bins, compute median power
# Expected: ~-90 to -100 dBm for thermal noise floor in 20 MHz
# If readings are consistently 10+ dB off, caldata is the likely cause
```

### Phase 2: Runtime Correction (Software Only)

**Goal:** Improve accuracy without touching flash or caldata.

| Test | Method | What It Tells Us |
|------|--------|-----------------|
| **T6: NF-corrected sweep** | Read `dump_nfcal` per channel, apply correction to spectral power formula | Whether runtime NF cal alone closes the gap |
| **T7: Cross-radio offset table** | Build a per-channel offset table from T2/T4, apply in RFeye | Whether a simple gain offset per channel improves accuracy |

#### T6 Implementation Sketch
```javascript
// In the GUI or agent, after each channel:
// 1. Read the runtime NF value from the driver
// 2. Use it instead of the default -96 dBm assumption
// 3. power(i) = nf_actual + RSSI + 10*log(b(i)^2) - bin_sum
```

### Phase 3: Hardware Caldata (If Needed)

**Goal:** Only pursue if Phase 1/2 show unacceptable errors.

| Test | Method | Depends On |
|------|--------|-----------|
| **T8: Gen2 XC caldata comparison** | Obtain NanoBeam Gen2 ART dump, compare 0x1000 vs 0x5000 structure | Access to a Gen2 board |
| **T9: ART 0x1000 write test** | Write Gen2-derived caldata to 0x1000 on bench PBE-5AC-500, point WMAC DTS at 0x1000 | Bench-only, not production |
| **T10: External reference cal** | Use SDR or calibrated SA to measure true power, build correction table | Lab equipment |

---

## 5. Success Criteria

| Level | Criteria | Action |
|-------|----------|--------|
| **Good enough** | Ambient noise floor reads within ±6 dB of expected; wideband shape is visually consistent; relative comparisons are valid | Ship as-is, document limitations |
| **Improved** | Runtime NF correction brings readings within ±3 dB; cross-channel gain is flat within ±4 dB | Add NF correction to RFeye agent |
| **Lab-grade** | All readings within ±2 dB of reference SA; per-channel flatness within ±2 dB | Requires proper caldata (Phase 3) |

RFeye's design intent is "Good enough" — it's a field diagnostic tool, not
a lab instrument. "Improved" is worth pursuing if it's achievable in software.
"Lab-grade" is a stretch goal for dedicated bench hardware.

---

## 6. Caldata Assessment Summary (Corrected)

Correcting the earlier assessment that was written before r15-r16 proved the
WMAC spectral path works:

| Claim | Status | Notes |
|-------|--------|-------|
| PR #2725 enables QCA9558 WMAC on XC boards | ✅ Correct | |
| Patch uses `cal_art_5000` (not 0x1000) | ✅ Correct | 0x1000 is blank on Gen1 XC boards; 0x5000 has valid AR9300 EEPROM |
| "Cannot work" — no valid caldata | ⚠️ Partially wrong | The WMAC *functions* with PCI caldata (structurally valid AR9300 EEPROM), but PCI caldata is **not an appropriate WMAC calibration source** — it is calibrated for a different RF chain. See Section 0 safety rules |
| Template EEPROM fallback | ⚠️ Misleading | The 0x5000 data IS factory-written (unique MAC, board ID). It's not a template — it's caldata for a *different radio on the same board* |
| "RFeye should not depend on PR #2725" | ❌ Outdated | r16 depends on it and passes soak tests |
| Proper WMAC caldata is required | ✅ Correct | Not just "would help" — using PCI caldata is a known-bad fallback acceptable only for bench testing. A valid WMAC-calibrated source must be identified before any production use |
| Spectral scanning ≠ radio communication | ✅ Key insight | FFT hardware works without TX-quality caldata |

---

## 7. Recommended Next Steps

> **r17 goal (revised 2026-05-29):** r17 should not be "make 0x5000 better."
> r17 should be **"identify a valid WMAC caldata source and reject unsafe
> sources."**

1. **Run T0 (stock firmware reconnaissance)** on the bench Rocket
   - Highest priority — determines whether Ubiquiti ever ships WMAC caldata
     on XC boards, and if so, how it's exposed
   - Results gate all subsequent decisions

2. **Run Phase 1 tests (T1–T5)** on the bench PBE-5AC-500
   - Quantifies how far off the PCI-caldata fallback actually is
   - Results determine whether runtime correction is worth pursuing

3. **If T0 finds a valid WMAC caldata source:** Evaluate using it
   - WA-series 0x1000 data or Gen2 XC 0x1000 data
   - Compare structure against 0x5000 to confirm it's WMAC-calibrated
   - Test on bench hardware only

4. **If T0 finds no WMAC caldata anywhere:** Document this as a vendor gap
   - XC boards ship with no WMAC calibration — the blank 0x1000 is
     intentional because Ubiquiti never activates the WMAC on these boards
   - Evaluate Option A (runtime NF cal) and Option B (cross-radio offset)
     as software-only mitigations
   - Ship with explicit "uncalibrated WMAC" disclaimer

5. **If NF is off by >10 dB:** Implement T6 (runtime NF correction)
   - Software-only fix, no flash writes

6. **If cross-channel gain varies >8 dB:** Implement T7 (offset table)
   - Build empirical correction table from T2/T4 measurements

7. **Update README** with measured accuracy characteristics and caldata
   provenance after testing

---

## 8. References

- [ath9k spectral scan — Linux Wireless docs](https://wireless.docs.kernel.org/en/latest/en/users/drivers/ath9k/spectral_scan.html)
- [AR9300 EEPROM structure — iPXE reference](https://dox.ipxe.org/structar9300__eeprom.html)
- [ART partition caldata — CodeFetch collection](https://github.com/CodeFetch/art-collection)
- [OpenWrt NanoBeam AC Gen2 XC DTS patch](https://lists.openwrt.org/pipermail/openwrt-devel/2023-March/040697.html)
- AREDN PR #2725: WMAC DTS enablement for XC boards
- `docs/ONCHIP_SCANNER_RADIO_RESEARCH.md` — earlier research on the WMAC hardware
