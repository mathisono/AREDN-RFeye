# RFeye Development Roadmap

> **Last updated:** 2026-05-31

---

## Current: r16 (Shipped)

WMAC wideband spectral scanner working on XC boards with bench-only PCI
caldata fallback. Single-channel ath10k spectral on all AREDN ath10k nodes.

---

## Next: r17 — Caldata Self-Provisioning for AREDN PR #2730

### The AREDN Integration Model (PR #2730, merged)

[AREDN PR #2730](https://github.com/aredn/aredn/pull/2730) provides the
hooks for RFeye to operate the WMAC spectrum radio on supported Ubiquiti
AC boards. The design is a clean separation of concerns:

**What AREDN provides (PR #2730):**
- DTS entries that enable the WMAC (`&wmac { status = "okay"; }`) on:
  - Rocket 5AC Lite (`qca9558_ubnt_rocket-5ac-lite.dts`)
  - PowerBeam 5AC 500 (`qca9558_ubnt_powerbeam-5ac-500.dts`)
- `qca,no-eeprom` flag — tells ath9k to look for caldata on the
  **filesystem** instead of in the ART/EEPROM partition
- Hardware config entries for `wlan1` with `"disabled": true`
- The WMAC will **not initialize by default** because no caldata file
  is present — it will fail silently until an app provides one

**What RFeye provides (r17):**
- The correct caldata firmware file for the target board's WMAC
- Installation of the firmware file to the expected filesystem path
- Reload of the ath9k module (or require a reboot) to re-initialize
  the WMAC with the newly-available caldata
- All spectral scanning functionality via the initialized WMAC

### r17 Implementation Tasks

1. **Determine the ath9k firmware file path and format**
   - When `qca,no-eeprom` is set, ath9k looks for caldata at a specific
     path under `/lib/firmware/` (e.g., `ath9k-caldata-<phy>.bin` or
     the path specified in the DTS)
   - Verify the exact path the kernel expects on AREDN builds

2. **Source valid WMAC caldata**
   - XC boards (PBE-5AC-500, R5AC-Lite): ART+0x1000 is blank — no
     factory caldata exists. Options:
     - Use the ath9k `ar9300_default` built-in template (generic but
       correct radio type)
     - Use WA board caldata as a cross-board reference (available in
       `artifacts/eeprom/eeprom_wa_pbe5ac_gen2.bin` at offset 0x1000)
     - Generate a caldata file from runtime NF calibration
   - WA boards (if supported later): extract from ART+0x1000 directly

3. **RFeye agent: caldata provisioning flow**
   ```
   rfeye-agent probe
     → detect board type (XC/WA) and WMAC state
     → if WMAC not initialized and caldata file missing:
       → install appropriate caldata to /lib/firmware/...
       → reload ath9k module (rmmod + modprobe) or flag reboot needed
     → if WMAC initialized:
       → proceed with spectral scanning as r16
   ```

4. **Package the caldata**
   - Include caldata file(s) in the RFeye IPK
   - Or download on first run from a known source
   - Document provenance and limitations (bench-only vs. production)

5. **Safety: do not break the production radio**
   - ath9k module reload must not affect the ath10k PCI radio
   - If ath9k reload fails, RFeye should report the error and continue
     with single-channel ath10k mode only
   - Never write to flash — caldata goes to tmpfs or a persistent
     overlay path that doesn't risk bricking

### r17 Caldata Safety Rules (unchanged)

1. Do not feed blank ART+0x1000 (all 0xFF)
2. Do not feed random bytes
3. Do not use PCI caldata (ART+0x5000) as production WMAC source
4. Find a valid WMAC-calibrated source
5. Treat any substitute caldata as bench-only until confirmed valid

---

## Future: r18+ — Accuracy and Calibration

### Runtime Calibration Improvements
- Read ath9k `dump_nfcal` per channel, apply to spectral power formula
- Cross-radio offset table (ath10k vs ath9k on shared channel)
- Per-channel gain correction from empirical measurements

### Extended Hardware Support
- NanoBeam AC XC (`qca9558_ubnt_nanobeam-ac-xc.dts`) — needs DTS entry
  in AREDN, same caldata model as PR #2730
- WA boards — extract and use factory caldata from ART+0x1000

### Live Noise Floor Feed for RF Coverage Modeling

> *"If you just make a bad guess [at the noise floor], you are building
> a house on some very shaky foundations."*
> — CloudRF, "Dynamic noise floor modelling with a CRFS RFeye receiver"

The commercial CRFS RFeye platform demonstrates the exact capability
that AREDN-RFeye will bring to the mesh: **network-attached spectral
sensing that replaces static RF planning assumptions with live data.**

In a demonstration using a 2.4 GHz directional link over water, a
static "guessed" noise floor produced a coverage map showing a viable
link with massive range. When the same model was fed a **live noise
floor measurement** from a remote RFeye receiver, the actual noise was
-92 dBm instead of the assumed thermal baseline — and the coverage area
shrunk dramatically, revealing the link was completely unviable in
practice. Polling the same receiver seconds later showed the noise floor
fluctuating between -92 dBm and -108 dBm as Wi-Fi bursts raised and
lowered the background interference in real time.

**This is why AREDN-RFeye exists.** Every AREDN node with a WMAC
spectral radio becomes a network-attached RF sensor. Once the C daemon
is parsing the 8-bit TLV bin payloads from ath9k relayfs and computing
per-bin dBm via:

```
P_i = NF + RSSI + 20·log₁₀(b_i) − BinSum
```

…that live dBm noise data can feed into RF modeling engines to map
dynamic mesh link viability based on real-world interference, not
static assumptions.

#### Integration Targets

| Engine | Integration Path | Use Case |
|--------|-----------------|----------|
| [CloudRF](https://cloudrf.com) | REST API — POST measured NF per channel as JSON | Cloud-hosted coverage maps with live noise |
| [SPLAT!](https://www.qsl.net/kd2bd/splat.html) | Feed measured NF into `-d` (receive sensitivity) parameter | Offline terrain-aware link modeling |
| [Radio Mobile](https://www.ve2dbe.com/rmonline.html) | Import per-site noise floor measurements | Amateur radio propagation studies |
| AREDN mesh status | Expose via AREDN API / local web UI | Node-level spectral health dashboard |
| Custom heatmaps | JSON API → mapping overlay | Mesh-wide interference visualization |

#### Implementation Phases

**Phase 1 — Per-Node Noise Floor API (r18/r19)**
- Daemon computes rolling NF per channel from ath9k spectral sweeps
- Expose via JSON endpoint: `GET /api/spectral/noise_floor`
  ```json
  {
    "timestamp": 1748745600,
    "node": "KJ6DZB-WSB-ACdish5",
    "channels": {
      "5180": { "nf_dbm": -98, "peak_dbm": -72, "occupancy": 0.12 },
      "5300": { "nf_dbm": -103, "peak_dbm": -95, "occupancy": 0.01 },
      "5745": { "nf_dbm": -88, "peak_dbm": -45, "occupancy": 0.67 }
    }
  }
  ```
- This is the minimum useful output: **real measured NF** to replace
  the "-96 dBm guess" that breaks every coverage model

**Phase 2 — Mesh-Wide Spectral Aggregation (r20+)**
- Central collector polls all RFeye-equipped nodes
- Build a mesh-wide noise floor map: channel × node × time
- Detect interference sources by correlating across nodes
- Feed into AREDN channel planning / auto-channel selection

**Phase 3 — Live Coverage Modeling (r21+)**
- Integrate with CloudRF or SPLAT! to produce coverage maps
  using live per-site NF measurements instead of static assumptions
- Recalculate link viability on demand or on schedule
- Alert operators when measured noise degrades a link below threshold
- This is the full "network-attached RFeye" capability: accurate,
  remote, real-time RF assessment without sending a technician

#### What Changes vs. Static Modeling

| Approach | NF Source | Accuracy | Updates |
|----------|-----------|----------|--------|
| Traditional | Assumed -96 dBm (thermal) | ❌ Wrong in real environments | Never |
| Site survey | One-time measurement | ⚠️ Snapshot — stale within hours | Manual |
| **AREDN-RFeye** | **Live ath9k spectral** | **✅ Real-time per-channel** | **Continuous** |

The CloudRF/CRFS RFeye demo showed a link going from "looks great" to
"completely unviable" based solely on replacing a guessed NF with a
measured one. Every AREDN node with a WMAC spectral radio can provide
that same measurement for its local RF environment, turning the entire
mesh into a distributed spectrum observatory.

### Ubiquiti Spectral Pipeline Recovery (Future Project)
- Reverse engineer `ubntspecd` binary and `ubnthal` kernel module
- Recover the proprietary spectral data format and calibration pipeline
- Reimplement as open-source for AREDN
- See `docs/WA_AIRVIEW_RUNTIME_PROBE.md` Section 6

---

## Reference: AREDN Integration History

| PR | Status | Description |
|----|--------|-------------|
| [#2725](https://github.com/aredn/aredn/pull/2725) | Superseded | WMAC DTS enablement with shared PCI caldata at ART+0x5000 |
| [#2730](https://github.com/aredn/aredn/pull/2730) | **Merged** | WMAC DTS hooks with `qca,no-eeprom` — caldata from filesystem, app-provisioned |

PR #2730 supersedes PR #2725. The key change: AREDN provides the DTS hooks,
RFeye provides the caldata. The WMAC does not initialize until an app
(RFeye) installs the appropriate firmware file.

---

## Reference: Research Documents

| Document | Contents |
|----------|----------|
| [`The XC vs. WA Divide`](The%20XC%20vs.%20WA%20Divide.md) | Hardware comparison, EEPROM analysis, caldata presence |
| [`T0_STOCK_PROBE_RESULTS`](T0_STOCK_PROBE_RESULTS.md) | Stock WA firmware probe — AirView, factory caldata |
| [`WA_AIRVIEW_RUNTIME_PROBE`](WA_AIRVIEW_RUNTIME_PROBE.md) | ubntspecd runtime analysis, proprietary driver stack |
| [`WMAC_CALDATA_RESEARCH_AND_TESTING_PLAN`](WMAC_CALDATA_RESEARCH_AND_TESTING_PLAN.md) | Full caldata research, safety rules, testing phases, TLV format, power formula |
| [`ONCHIP_SCANNER_RADIO_RESEARCH`](ONCHIP_SCANNER_RADIO_RESEARCH.md) | QCA9558 WMAC hardware research |
| [`SPECTRAL_MATH_AUDIT_REPORT`](SPECTRAL_MATH_AUDIT_REPORT.md) | Power formula audit — verified against upstream kernel + FFT_eval |
| [`TEST6_STOCK_ROCKET_5AC_LITE_REPORT`](TEST6_STOCK_ROCKET_5AC_LITE_REPORT.md) | Stock XC baseline: WMAC active, default template, fake MAC |
| [`TEST123456_STOCK_POWERBEAM_5AC_GEN2_WA_REPORT`](TEST123456_STOCK_POWERBEAM_5AC_GEN2_WA_REPORT.md) | Stock WA baseline: real caldata, 1x1 WMAC, board.info |
| [`TEST124_STOCK_ROCKET_5AC_LITE_REPORT`](TEST124_STOCK_ROCKET_5AC_LITE_REPORT.md) | XC ART dumps: blank 0x1000, valid 0x5000, board identity |
| [`TEST8_11_12_DUAL_DEVICE_REPORT`](TEST8_11_12_DUAL_DEVICE_REPORT.md) | Dual-device caldata extraction + production safety verification |
