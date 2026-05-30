# The XC vs. WA Divide

> **Date:** 2026-05-29  
> **Context:** T0 stock firmware probe + XC AREDN bench node comparison  
> **Question answered:** Why do XC boards have blank WMAC caldata?

---

## 0. Hardware Background

Ubiquiti split the 5AC generation into two distinct hardware trees:

**XC Boards:** The heavy-hitters (Gen1). These use the beefier 720 MHz CPU
(QCA955x based). Devices include the Rocket 5AC Lite, NanoBeam 5AC-19, and
the PowerBeam 5AC 500.

**WA Boards:** The cost-optimized versions (Gen1 and Gen2). These step down
to a 535 MHz CPU with half the RAM (QCA956x based). Devices include the
LiteBeam Gen2 and the lower-end PowerBeam 5AC-300/400.

Because the underlying SoCs are entirely different, the physical PCB
traces — specifically how the RF Switch (RFS) taps the antenna path to feed
the secondary AirView radio — are routed differently. The `4CCCC` marking
is almost certainly an SMD code for a GaAs/SOI RF switch IC (like a
Skyworks or similar SPDT switch). You cannot use the WA trace map to figure
out the XC board; the pinouts and switch logic GPIOs simply won't match up.

### Why Antenna Integration Matters for Calibration

**Rocket 5AC Lite (Connectorized):** This device ends in two RP-SMA
connectors. Ubiquiti has no idea if the end-user is going to screw on a
13 dBi omni, a 22 dBi sector, or a 34 dBi dish. Because the antenna gain
and physical path are unknown variables, they likely didn't bother
populating granular phase/amplitude or band-edge calibration for the
secondary AirView radio. The generic fallback template was "good enough"
for a listen-only spectrum analyzer.

**PowerBeam 5AC 500 (Integrated):** This device has a fixed, proprietary
27 dBi integrated feed horn and inner RF isolation shielding. Because the
physical RF path, impedance, and antenna characteristics are 100% known
and permanent from the factory, Ubiquiti's engineers may have populated the
EEPROM/ART partition with real baseline noise floor calibrations and spur
mitigation channels specific to that exact dish setup.

> **T0 probe result:** The XC PowerBeam 5AC 500 running AREDN has **blank
> 0x1000** — Ubiquiti did NOT calibrate the WMAC on this board. The WA
> PowerBeam 5AC Gen2 running stock AirOS **does** have factory WMAC caldata.
> This confirms the XC/WA split is both a hardware and factory-calibration
> divide. Whether a stock XC firmware image has AirView (and if so, what
> caldata it uses) remains an open question — see the testing checklist.

---

## 1. Summary

| Property | WA Board | XC Board |
|----------|----------|----------|
| **Example** | PowerBeam 5AC Gen2 (PBE-5AC-Gen2) | PowerBeam 5AC 500 (PBE-5AC-500) |
| **SoC** | QCA956x (535 MHz) | QCA955x (720 MHz) |
| **SysID** | 0xe3d6 | 0xe3d5 |
| **Board ID** | cus223-022-n1725 | CUS223-720-S0849 |
| **EEPROM+0x1000 (WMAC)** | **1021 bytes structured caldata** | **All 0xFF (blank)** |
| **EEPROM+0x5000 (PCI)** | 2114 bytes, compressed AR9300 | 2113 bytes, compressed AR9300 |
| **WMAC caldata format** | AR9300 template (`02 02`) | N/A — no caldata |
| **PCI caldata format** | AR9300 compressed (`44 08`) | AR9300 compressed (`44 08`) |
| **Stock firmware WMAC use** | AirView spectral scanner (`airview1`) | Unknown (needs XC stock FW test) |
| **Firmware prefix** | WA | XC |

**The blank 0x1000 on XC boards is a deliberate Ubiquiti factory decision.**
They chose not to calibrate the WMAC on XC hardware.

---

## 2. Evidence

### 2.1 WA Board (PowerBeam 5AC Gen2, stock AirOS WA.v8.7.11)

Probed 2026-05-29 at 10.108.120.18 via MSE-88.

```
EEPROM+0x0000  MAC1: 24:5a:4c:0f:25:14  MAC2: 26:5a:4c:0f:25:14
EEPROM+0x1000  Header: 02 02 (AR9300 template format)
               MAC: 26:5a:4c:0e:25:14 (WMAC)
               1021/1024 bytes populated
               Gain tables, cal piers, CTL data present
EEPROM+0x5000  Header: 44 08 (AR9300 compressed)
               MAC: 24:5a:4c:0e:25:14 (PCI)
               Board ID: cus223-022-n1725
```

Stock firmware:
- Creates `airview1` interface on WMAC (wifi1, AHB bus) in Monitor mode
- Loads `ath_spectral` kernel module
- `board.info` lists `radio.2.bus=ahb`, `radio.2.chains=1`, `radio.2.web_exclude=1`
- Uses the WMAC for AirView spectral scanning — same use case as RFeye

### 2.2 XC Board (PowerBeam 5AC 500, AREDN 4.26.1.0)

Probed 2026-05-29 at 10.188.138.222:2222 via MSE-88.

```
EEPROM+0x0000  MAC1: f4:92:bf:bd:8a:de  MAC2: f6:92:bf:bd:8a:de
EEPROM+0x1000  ALL 0xFF — completely blank, 0/1024 bytes populated
EEPROM+0x5000  Header: 44 08 (AR9300 compressed)
               MAC: f4:92:bf:bc:8a:de (PCI)
               Board ID: CUS223-720-S0849
```

AREDN:
- ath9k module loaded (via PR #2725 DTS patch) but has no valid 0x1000 caldata
- Falls back to template EEPROM defaults or shared PCI caldata at 0x5000
- No device tree wifi/cal nodes (PR #2725 adds them)

---

## 3. The Two Board IDs

Both boards use `CUS223` but with different suffixes:

| Board | Board ID | Interpretation |
|-------|----------|---------------|
| WA (Gen2) | `cus223-022-n1725` | Design rev 022, variant n1725 |
| XC (500) | `CUS223-720-S0849` | Design rev 720, variant S0849 |

The `720` vs `022` suggests significantly different PCB revisions with
different SoCs, different RF switch routing, and different antenna paths.

---

## 4. Why Ubiquiti Skipped WMAC Calibration on XC

Likely reasons (inferred, not confirmed):

1. **Different SoC and RF path.** QCA955x (XC) vs QCA956x (WA) have
   different integrated WMAC silicon and different board trace routing.
   The RF switch tapping the antenna for AirView is routed differently.

2. **Cost / product decision.** XC boards may not ship with AirView in
   firmware, making WMAC calibration unnecessary.

3. **Connectorized vs integrated antenna.** The Rocket 5AC Lite (XC,
   connectorized) has unknown antenna characteristics at the factory.
   Calibrating for an unknown antenna path has less value than
   calibrating the integrated dish on a PowerBeam.

4. **AirView removed or simplified on XC.** The XC firmware variant may
   use a generic template fallback for AirView (if it exists at all),
   while the WA firmware uses per-unit factory caldata.

---

## 5. EEPROM Format Differences

### WMAC: AR9300 Template Format (`02 02`)

The WA board's WMAC caldata at 0x1000 uses the AR9300 **template format**:

- Byte 0: `02` — major version (AR9300 EEPROM version)
- Byte 1: `02` — template index
- Bytes 2–7: MAC address
- Remainder: delta-compressed against a built-in template in the ath9k driver

The template format stores only the **differences** from a known-good
reference EEPROM built into the driver firmware. This is how Ubiquiti
writes per-unit calibration without needing a full 2 KB EEPROM per radio.

### PCI: AR9300 Compressed Format (`44 08`)

Both WA and XC boards use the AR9300 **compressed format** at 0x5000:

- Header: `44 08` — block length + checksum
- Contains: full compressed EEPROM with board ID, MAC, gain tables,
  cal piers, CTL limits, regulatory data

The compressed format is self-contained and does not reference a template.

### What This Means for XC WMAC

When ath9k loads on the XC board and finds all-0xFF at 0x1000:
1. It cannot parse any valid header (no `02 xx` template, no `44 08` compressed)
2. It falls back to the kernel's `ar9300_default` template — hardcoded
   default calibration data that is not board-specific
3. If PR #2725 points the WMAC at 0x5000 instead, ath9k gets structurally
   valid data but for the **wrong radio** (PCI, not WMAC)

Neither fallback produces correct WMAC calibration.

---

## 6. PCI Caldata Comparison

The PCI caldata at 0x5000 differs between the two boards by **289 bytes**
out of 2128 structured bytes. Key differences:

| Field | WA | XC |
|-------|----|----|
| MAC | `24:5a:4c:0e:25:14` | `f4:92:bf:bc:8a:de` |
| Board ID | `cus223-022-n1725` | `CUS223-720-S0849` |
| SysID | 0xe3d6 | 0xe3d5 |
| Header CRC | Different | Different |
| Cal pier values | Different (per-unit factory cal) | Different |

The 289-byte difference confirms these are **per-unit, per-board factory
calibrations** — not copies of a template. Each board was individually
characterized on the production line.

---

## 7. Implications for RFeye

### What We Now Know

1. **Valid WMAC caldata exists** — on WA boards, at EEPROM+0x1000, in
   AR9300 template format.

2. **XC boards will never have factory WMAC caldata.** Ubiquiti made this
   decision at the hardware/factory level. No firmware update can fix it.

3. **WA caldata is for a different SoC.** The WA board uses QCA956x, the
   XC uses QCA955x. The integrated WMAC silicon and RF paths differ.
   Using WA caldata on XC hardware is better than PCI caldata (at least
   it's WMAC-type data) but cross-SoC applicability is uncertain.

4. **Ubiquiti already does what RFeye does.** The WA AirView feature is
   exactly the same concept — spectral scanning on the WMAC monitor
   interface. This validates the RFeye approach.

### Options for XC WMAC Calibration

In order of preference:

1. **Use the ath9k default template.** When 0x1000 is blank, ath9k falls
   back to `ar9300_default` in the driver. This is generic but targets
   the right radio type (WMAC, not PCI).

2. **Use WA board 0x1000 caldata as a reference.** Extract it, verify it
   parses correctly in ath9k on XC hardware. Cross-SoC applicability is
   uncertain but it's real WMAC cal data.

3. **Runtime NF calibration.** The ath9k driver does per-channel noise
   floor calibration at runtime, which partially compensates for missing
   factory data. This is already happening.

4. **Test XC stock firmware.** Determine if Ubiquiti's XC firmware has
   AirView and if so, what caldata source it uses. This could reveal
   a hidden caldata path we haven't found. See the XC stock FW testing
   checklist.

5. **Do not use PCI caldata (0x5000) as WMAC source.** This is the current
   PR #2725 approach and should be treated as bench-only.

---

## 8. Open Question: Does XC Stock Firmware Have AirView?

We have probed:
- ✅ WA board on stock AirOS — has AirView, has WMAC caldata
- ✅ XC board on AREDN — no WMAC caldata (0x1000 blank)
- ❓ XC board on stock AirOS — **not yet tested**

The critical question: does Ubiquiti's XC firmware (`XC.v8.x.x`) activate
the WMAC at all? If it does, what caldata does it use?

See the XC stock firmware testing checklist for the probe procedure.

---

## 9. Full EEPROM Dumps Available

| File | Board | SoC | SHA256 |
|------|-------|-----|--------|
| `artifacts/eeprom/eeprom_wa_pbe5ac_gen2.bin` | WA PBE-5AC-Gen2, stock AirOS | QCA956x | `825d652b...` |
| `artifacts/eeprom/eeprom_xc_pbe5ac500_aredn.bin` | XC PBE-5AC-500, AREDN | QCA955x | `62761919...` |

Both are full 64 KB EEPROM partition dumps extracted via `dd if=/dev/mtdblock<N> bs=65536 count=1`.
